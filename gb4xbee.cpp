/**
 *	gb4xbee.cpp
 */

#include "gb4xbee.h"
#include "xbee/atcmd.h"

static uint32_t constexpr GB4XBEE_CAST_GUARD = 0x47425842;

xbee_dispatch_table_entry_t const xbee_frame_handlers[] = {
	XBEE_FRAME_HANDLE_LOCAL_AT,
	XBEE_SOCK_FRAME_HANDLERS,
	XBEE_FRAME_TABLE_END
};


GB4XBee::GB4XBee(
	uint32_t baud,
	char const apn[],
	bool use_tls,
	uint8_t use_tls_profile) :
	cast_guard(GB4XBEE_CAST_GUARD),
	ser({.baudrate = baud}),
	guard_time_start(0),
	response_start_time(0),
	got_access_point_name(0),
	need_set_access_point_name(0),
	err(0),
	tls_profile(use_tls_profile)
{
	transport_protocol =
		(true == use_tls) ?
		XBEE_SOCK_PROTOCOL_SSL : XBEE_SOCK_PROTOCOL_TCP; 
	strncpy(access_point_name, apn, GB4XBEE_ACCESS_POINT_NAME_SIZE - 1);
}


/**
 *	Initialize the XBee driver, and start the state machine
 *	@return
 *		true - Driver and state machine started
 *		false - Problem starting the XBee driver
 */
bool GB4XBee::begin()
{
	int status = xbee_dev_init(&xbee, &ser, NULL, NULL);
	if(0 != status)
	{
		err = status;
		return false;
	}
	
	m_state = State::START;	
	return true;
}


/**
 *	Allows an external module to manipulate the state machine. Forces the state
 *	to GB4XBee::State::SOCKET_COOLDOWN, and starts cooldown timer. The cooldown
 *	timer adds a delay to the creation of a new socket since socket creation 
 *	tends to fail if down immediately after closing the socket.
 *	@return The new state machine state
 */
GB4XBee::State GB4XBee::resetSocket()
{
	m_state = State::SOCKET_COOLDOWN_PERIOD;
	socket_cooldown_start_time = millis();
	return m_state;
}


/**
 *	Execute and advance the state machine to do the initial configuration and 
 *	startup for the device. This only needs to be done at the start of the
 *	program's execution. 
 *	Upon completion of the state machine, the device will be in API mode, and
 *	ready to create a socket.
 *	Call once per startup loop.
 *	@return
 *		GB4XBee::Return::STARTUP_IN_PROGRESS - The device is not yet configured
 *		GB4XBee::Return::STARTUP_COMPLETE - The device is in API mode, and
 *		                                    ready to create a socket. This
 *		                                    does not need to be called again
 *		                                    until the MCU is reset
 */
GB4XBee::Return GB4XBee::pollStartup()
{
	switch(m_state)
	{
		case State::START:
		startCommandModeGuard();
		m_state = State::AWAIT_COMMAND_MODE_GUARD_0; 
		break;

		case State::AWAIT_COMMAND_MODE_GUARD_0:
		if(false == pollCommandModeGuard())
		{
			break;
		}
		m_state = State::BEGIN_COMMAND_MODE;
		break;

		case State::BEGIN_COMMAND_MODE:
		sendEscapeSequence();
		startCommandModeGuard();
		m_state = State::AWAIT_COMMAND_MODE_GUARD_1;
		break;

		case State::AWAIT_COMMAND_MODE_GUARD_1:
		if(false == pollCommandModeGuard())
		{
			break;
		}
		m_state = State::AWAIT_COMMAND_MODE_RESPONSE;
		break;

		case State::AWAIT_COMMAND_MODE_RESPONSE:
		switch(pollResponseOK())
		{
			case Return::COMMAND_OK:
			m_state = State::BEGIN_API_MODE_COMMAND;
			break;
			case Return::COMMAND_TIMEOUT:
			//Error state
			break;
			case Return::COMMAND_NOT_OK:
			//Error state
			break;
			default:
			break;
		}
		break;

		case State::BEGIN_API_MODE_COMMAND:
		sendAPIMode();
		m_state = State::AWAIT_API_MODE_RESPONSE;
		break;

		case State::AWAIT_API_MODE_RESPONSE:
		switch(pollResponseOK())
		{
			case Return::COMMAND_OK:
			m_state = State::BEGIN_COMMAND_MODE_EXIT;
			break;
			case Return::COMMAND_TIMEOUT:
			//Error state
			break;
			case Return::COMMAND_NOT_OK:
			//Error state
			break;
			default:
			break;
		}
		break;

		case State::BEGIN_COMMAND_MODE_EXIT:
		sendCommandModeExit();
		m_state = State::AWAIT_COMMAND_MODE_EXIT_RESPONSE;
		break;

		case State::AWAIT_COMMAND_MODE_EXIT_RESPONSE:
		switch(pollResponseOK())
		{
			case Return::COMMAND_OK:
			m_state = State::BEGIN_INIT_XBEE_API;
			break;
			case Return::COMMAND_TIMEOUT:
			//Error state
			break;
			case Return::COMMAND_NOT_OK:
			//Error state
			break;
			default:
			break;
		}
		break;

		case State::BEGIN_INIT_XBEE_API:
		startInitAPI();
		m_state = State::AWAIT_INIT_XBEE_API_DONE;
		break;

		case State::AWAIT_INIT_XBEE_API_DONE:
		{
			Return status = pollInitStatus();
			if(Return::INIT_TRY_AGAIN == status)
			{
				restartInitAPI();
				break;
			}
			else if(Return::INIT_IN_PROGRESS == status)
			{
				break;
			}
		}
		m_state = State::BEGIN_READ_APN;
		break;

		case State::BEGIN_READ_APN:
		sendReadAPN();
		m_state = State::AWAIT_READ_APN_RESPONSE;
		break;

		case State::AWAIT_READ_APN_RESPONSE:
		switch(pollAPNStatus())
		{
			case Return::APN_IS_SET:
			m_state = State::BEGIN_CREATE_SOCKET;
			break;
			case Return::APN_NOT_SET:
			m_state = State::SET_APN;
			break;
			case Return::APN_READ_ERROR:
			//Error state
			break;
			default:
			break;
		}
		break;

		case State::SET_APN:
		sendSetAPN();
		sendWriteChanges();
		m_state = State::BEGIN_CREATE_SOCKET;
		break;
	}

	if(m_state != State::BEGIN_CREATE_SOCKET)
	{
		return Return::STARTUP_IN_PROGRESS;
	}

	return Return::STARTUP_COMPLETE;
}


/**
 *	Execute and advance to state machine main state machine
 *	The purpose of the state machine is to maintian connection to a
 *	socket to a preconfigured address. If a socket is unexpectedly closed, it
 *	will automatically try to reconnect.
 *	Call once per main loop.
 *	@return The current state of the state machine
 */
GB4XBee::State GB4XBee::poll()
{
	if(m_state < State::SOCKET_COOLDOWN_PERIOD)
	{
		pollStartup();
		return m_state;
	}

	int32_t xbee_status = xbee_dev_tick(&xbee);
	
	switch(m_state)
	{
		case State::SOCKET_COOLDOWN_PERIOD:
		if(false == pollSocketCooldown())
		{
			break;
		}
		m_state = State::BEGIN_CREATE_SOCKET;
		break;

		case State::BEGIN_CREATE_SOCKET:
		notify_count = g_notify.reset();
		connect_in_progress = false;
		if(false == sendSocketCreate())
		{
			m_state = resetSocket();
			break;	
		}
		m_state = State::AWAIT_SOCKET_ID;
		break;

		case State::AWAIT_SOCKET_ID: 	
		if(g_notify.count() == notify_count)
		{
			uint32_t elapsed = millis() - socket_create_start_time;
			if(elapsed > GB4XBEE_SOCKET_CREATE_TIMEOUT)
			{
				m_state = State::BEGIN_CREATE_SOCKET;
			}
			break;
		}
		notify_count = g_notify.count();
		if(
			(XBeeNotify::FrameType::SOCK_CREATE_RESP != g_notify.type) ||
			(XBeeNotify::SockMesg::SUCCESS != g_notify.sock_mesg))
		{
			//State error: reset the socket
			m_state = resetSocket(); 
			break;
		}
		m_state = State::SOCKET_READY;
		break;

		case State::SOCKET_READY:
		if(false == connect_in_progress)
		{
			break;
		}
		m_state = State::AWAIT_CONNECT_RESPONSE;
		break;

		case State::AWAIT_CONNECT_RESPONSE:
		if(g_notify.count() == notify_count)
		{
			uint32_t elapsed = millis() - connect_start_time;
			if(elapsed > GB4XBEE_CONNECT_TIMEOUT)
			{
				m_state = resetSocket();
			}
			break;
		}
		notify_count = g_notify.count();
		if(
			(XBeeNotify::FrameType::SOCK_CONNECT_RESP != g_notify.type) ||
			(XBeeNotify::SockMesg::SUCCESS != g_notify.sock_mesg))
		{
			m_state = resetSocket();
			break;
		}
		m_state = State::AWAIT_CONNECTION;
		break;

		case State::AWAIT_CONNECTION:
		if(g_notify.count() == notify_count)
		{
			uint32_t elapsed = millis() - connect_start_time;
			if(elapsed > GB4XBEE_CONNECT_TIMEOUT)
			{
				m_state = resetSocket();
			}
			break;
		}
		notify_count = g_notify.count();
		if(
			(XBeeNotify::FrameType::SOCK_STATE != g_notify.type) ||
			(XBeeNotify::StateMesg::CONNECTED != g_notify.state_mesg))
		{
			m_state = resetSocket();
			break;
		} 
		m_state = State::CONNECTED;
		break;

		case State::SENDING:
		if((millis() - send_start_time) > GB4XBEE_SEND_TIMEOUT)
		{
			m_state = State::CONNECTED;
		}
		//Fall-through OK

		case State::CONNECTED:
		if(g_notify.count() == notify_count)
		{
			break;
		}
		notify_count = g_notify.count();
		if(
			(XBeeNotify::FrameType::SOCK_STATE == g_notify.type) &&
			(XBeeNotify::StateMesg::CONNECTED != g_notify.state_mesg))
		{
			m_state = resetSocket();
			break;
		}
		if(
			(XBeeNotify::FrameType::TX_STATUS == g_notify.type) &&
			(XBeeNotify::TxMesg::SUCCESS != g_notify.tx_mesg))
		{
			m_state = resetSocket();
			break;
		}
		m_state = State::CONNECTED;
		break;
	
		default:
		break;
	}

	return m_state;
}


void GB4XBee::startCommandModeGuard()
{
	guard_time_start = millis();
}


bool GB4XBee::pollCommandModeGuard()
{
	return (millis() - guard_time_start) > GB4XBEE_COMMAND_MODE_GUARD_TIME; 
}


void GB4XBee::sendEscapeSequence()
{
	Serial.write("+++", 3);
	response_start_time = millis();
}


void GB4XBee::sendAPIMode()
{
	Serial.write("ATAP1\r", 6);
	response_start_time = millis();
}


void GB4XBee::sendCommandModeExit()
{
	Serial.write("ATCN\r", 5);
	response_start_time = millis();
}


GB4XBee::Return GB4XBee::pollResponseOK()
{
	size_t n = Serial.available();
	if(n < 3)
	{
		if((millis() - response_start_time) > GB4XBEE_DEFAULT_COMMAND_TIMEOUT)
		{
			return Return::COMMAND_TIMEOUT;
		}
		return Return::COMMAND_IN_PROGRESS;
	}

	char ok_buffer[7];
	n = Serial.readBytes(ok_buffer, n);
	if(0 != memcmp("OK", ok_buffer, 2))
	{
		return Return::COMMAND_NOT_OK;
	}

	return Return::COMMAND_OK;
}


void GB4XBee::startInitAPI()
{
	xbee_cmd_init_device(&xbee);
}


void GB4XBee::restartInitAPI()
{
	xbee_cmd_query_device(&xbee, 0);
}


GB4XBee::Return GB4XBee::pollInitStatus()
{
	xbee_dev_tick(&xbee);
	int status = xbee_cmd_query_status(&xbee);
	if(-EBUSY == status)
	{
		return Return::INIT_IN_PROGRESS;
	}
	if(0 != status)
	{
		return Return::INIT_TRY_AGAIN;
	}

	return Return::INIT_DONE;
}


/**
 *	Callback for the AT command to read the access point name (AN)
 *	Verifies that the APN in the device matches the desired APN which as
 *	configured with a argument to the class constructor.
 *	@param response - Object created by the XBee driver containing the response
 *	                  to the AT command. This object also has context parameter
 *	                  embedded is a void pointer to the GB4XBee object that
 *	                  initiated the AT command.
 *	                  Note: The void pointer is cast to a GB4XBee pointer. In
 *	                        order to prevent dereference of invalid address
 *	                        space, a cast guard is used to validate that the
 *	                        pointer points to the correct object.
 *	@return
 *		XBEE_ATCMD_DONE - Indicates to the XBee driver calling this callback
 *		                  that this is the only callback it needs to call
 */
static int readAPNCallback(xbee_cmd_response_t const *response)
{
	GB4XBee *ctx = static_cast<GB4XBee *>(response->context);
	if(GB4XBEE_CAST_GUARD == ctx->cast_guard)
	{
		ctx->verifyAccessPointName(
			response->value_bytes,
			response->value_length);
	}
	return XBEE_ATCMD_DONE;
}


/**
 *	Helper function to verify that the APN in the device matches the desired APN
 *	which was configured with a argument to the class constructor.
 *	Sets a private flag need_set_access_point_name if they do not match
 *	@param value - The APN currently configured in the device
 *	@param len - The length of value in bytes
 *	@return
 *		false - value does not match the object's access_point_name
 *		true - value matches the object's access_point_name
 */
bool GB4XBee::verifyAccessPointName(uint8_t const *value, size_t const len)
{
	got_access_point_name = true;
	if(0 != memcmp(access_point_name, value, len))
	{
		need_set_access_point_name = true;
		return false;
	}
	return true;
}


/**
 *	Send the AT command to read the access point name off the XBee device (AN)
 *	This is an asynchronous call; the driver will readAPNCallback() when the
 *	response arrives. The callback checks if the APN on the device matches the 
 *	expected APN, and set's the need_set_access_point_name if they do not.
 *	@return 
 *		false - There was a problem creating the command
 *		true - The command was created and sent to the device
 */
bool GB4XBee::sendReadAPN()
{
	int16_t handle = xbee_cmd_create(&xbee, "AN");
	if(handle < 0)
	{
		err = handle;
		return false; 
	}
	xbee_cmd_set_callback(handle, readAPNCallback, this);
	xbee_cmd_send(handle);
	return true;
}


/**
 *	Check if readAPNCallback has yet been called, and if a new APN needs to be
 *	written to the device
 *	@return
 *		GB4XBee::Return::APN_READ_IN_PROGRESS - Waiting for a response from the
 *		                                        XBee device
 *		GB4XBee::Return::APN_NOT_SET - Response has arrived and a new APN need
 *		                               to be written to the device
 *		GB4XBee::Return::APN_IS_SET - Response has arrived and no further
 *		                              action is required
 */
GB4XBee::Return GB4XBee::pollAPNStatus()
{
	xbee_dev_tick(&xbee);
	if(false == got_access_point_name)
	{
		return Return::APN_READ_IN_PROGRESS; 
	}
	else if(true == need_set_access_point_name)
	{
		return Return::APN_NOT_SET;
	}
	return Return::APN_IS_SET;
}


/**
 *	Send the AT command to save parameters to non-volatile memory (WR)
 *	This command has no response and doesn't require a callback
 *	@return 
 *		false - There was a problem generating or sending the command
 *		true - The command was send
 */
bool GB4XBee::sendWriteChanges()
{
	int16_t handle = xbee_cmd_create(&xbee, "WR");
	if(handle < 0)
	{
		err = handle;
		return false;
	}
	int status = xbee_cmd_send(handle);
	if(status != 0)
	{
		err = status;
		return false;
	}
	return true;
}


/**
 *	Send the AT command to write a new access point name (AN) to the device
 *	This command has no response and doesn't require a callback
 *	@return 
 *		false - There was a problem generating or sending the command
 *		true - The command was send
 */
bool GB4XBee::sendSetAPN()
{
	int16_t handle = xbee_cmd_create(&xbee, "AN");
	if(handle < 0)
	{
		err = handle;
		return false;
	}
	int status = xbee_cmd_set_param_str(handle, access_point_name);
	if(status < 0)
	{
		err = status;
		return false;
	}
	status = xbee_cmd_send(handle);
	if(0 != status)
	{
		err = status;
		return false;
	}
	return true;
}


/**
 *	Send the API frame to create a new socket. Start a timeout.
 *	The socket's status will be updated in the notify callback in
 *	xbee_notify.cpp.
 *	The notify callback will be executed with a call to GB4XBee::poll() after
 *	the response arrives. 
 */
bool GB4XBee::sendSocketCreate()
{
	xbee_sock_reset(&xbee);
	sock = xbee_sock_create(&xbee, transport_protocol, XBeeNotify::callback);
	if(sock < 0)
	{
		err = sock;
		return false;
	}
	socket_create_start_time = millis();
	return true;
}


/**
 *	Check if enough time has elapsed since closing a socket before creating a
 *	new socket
 *	@return
 *		false - Continue waiting before creating a new socket
 *		true - A new socket may now be created
 */
bool GB4XBee::pollSocketCooldown()
{
	return
		(millis() - socket_cooldown_start_time) >
		GB4XBEE_SOCKET_COOLDOWN_INTERVAL;
}


/**
 *	Sent the API frame to connect the most recently created socket to the given
 *	port and network address.
 *	This currently doesn't support multiple sockets; only one socket may be
 *	open at a time.
 *	@param port - The network port to connect to
 *	@param address - The network address, either a fully qualified domain name,
 *	                 or a '.' seperated IP address
 *	@return
 *		false - There was a problem connecting to the socket. This socket shall
 *		        be closed and a new one created after a cooldown period
 *		true - The API frame was sent successfully. The socket's state will be
 *		       updated in a callback function in xbee_notify.cpp. The callback
 *		       will be executed with a call the GB4XBee::poll() after the
 *		       response arrives. The response contains infomation about whether
 *		       or not the connection was successful.
 */
bool GB4XBee::connect(uint16_t port, char const *address)
{
	int status = xbee_sock_connect(
		sock,
		port,
		0,
		address,
		XBeeReceive::callback);
	if(0 != status)
	{
		err = status;
		return false;
	}
	connect_start_time = millis();
	connect_in_progress = true;
	return true;
} 


/**
 * 	Read pending data from the UART buffer after receiveng Socket Receive API
 * 	frame from the XBee.
 *	Call this function after XBeeReceive::callback() in xbee_notify.cpp has
 *	been called. The callback is executed with call to GB4XBee::poll()
 *	@param message - Output - UART buffer contents containing the received data
 *	                          will be copied into this buffer
 *	@param message_len - Input - The total size of the message array before an
 *	                             overrun occurs
 *	                     Output - The length of the received message
 *	@return
 *		GB4XBee::Return::WAITING_MESSAGE - There is no pending message in the
 *		                                   UART buffer yet
 *		GB4XBee::Return::MESSAGE_RECEIVED - Received and copied the message 
 */
GB4XBee::Return GB4XBee::getReceivedMessage(uint8_t message[], size_t *message_len)
{
	if(false == g_receive.pending())
	{
		return Return::WAITING_MESSAGE;
	}

	*message_len = g_receive.read(message, *message_len);

	return Return::MESSAGE_RECEIVED;
}


/**
 *	Encode a message into a socket send API frame and send to the XBee device
 *	This will transmit the encoded message to endpoint of the connected socket
 *	@param message - Input - Message to send
 *	@param message_len - Length of message in bytes
 *	@return
 *		GB4XBee::Return::IN_PROGRESS - A messsage is already being sent, wait 
 *		                               for it to finish before sending this
 *		                               message 
 *		GB4XBee::Return::DISCONNECTED - The socket has been disconnected. It
 *		                                should be closed and a new one created 
 *		GB4XBee::Return::BUFFER_FULL - The UART buffer is full, wait for it to
 *		                               drain before attempting to send
 *		GB4XBee::Return::PACKET_ERROR - The UART buffer is not large enough to
 *		                                fit this API frame. The message should
 *		                                be broken up into multiple packets.
 *		GB4XBee::Return::SOCKET_ERROR - There a problem sending on the socket.
 *		                                The socket should be close and a new
 *		                                one created
 *		GB4XBee::Return::MESSAGE_SENT - The message was successfully sent. The
 *		                                state of the socket will be updated
 *		                                in XBeeNotify::callback() in
 *		                                xbee_notify.cpp. The callback is
 *		                                executed with a call to GB4XBee::poll()
 */
GB4XBee::Return GB4XBee::sendMessage(uint8_t message[], size_t message_len)
{
	if(State::SENDING == m_state)
	{
		return Return::IN_PROGRESS;
	}

	if(State::CONNECTED != m_state)
	{
		return Return::DISCONNECTED;
	}

	Return status;
	int send_ok = xbee_sock_send(sock, 0, message, message_len);
	switch(send_ok)
	{
		case 0:
		status = Return::MESSAGE_SENT;
		send_start_time = millis();
		m_state = State::SENDING;
		break;

		case -ENOENT:
		status = Return::DISCONNECTED;
		break;

		case -EBUSY:
		status = Return::BUFFER_FULL;
		break;

		case -EMSGSIZE:
		status = Return::PACKET_ERROR;
		break;

		default:
		status = Return::SOCKET_ERROR;
		break;	
	}
	return status; 
}

