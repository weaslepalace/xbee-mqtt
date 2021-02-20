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


uint32_t GB4XBee::getBaud()
{
	return ser.baudrate;
}


char const *GB4XBee::getAPN()
{
	return access_point_name;
}


uint64_t GB4XBee::getSerialNumber()
{
	if(m_state < State::BEGIN_API_MODE_COMMAND)
	{
		return 0;
	}

	return 
		(static_cast<uint64_t>(xbee.wpan_dev.address.ieee.l[0]) << 32) | 
		(xbee.wpan_dev.address.ieee.l[1]);
}


GB4XBee::State GB4XBee::state()
{
	return m_state;
}

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


GB4XBee::State GB4XBee::resetSocket()
{
	m_state = State::SOCKET_COOLDOWN_PERIOD;
	socket_cooldown_start_time = millis();
	return m_state;
}


void GB4XBee::startConnectRetryDelay()
{
	connect_retry_delay_start_time = millis();
}


bool GB4XBee::pollConnectRetryDelay()
{
	return
		(millis() - connect_retry_delay_start_time) > 
		GB4XBEE_CONNECT_RETRY_DELAY_INTERVAL;
}


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


bool GB4XBee::pollSocketCooldown()
{
	return
		(millis() - socket_cooldown_start_time) >
		GB4XBEE_SOCKET_COOLDOWN_INTERVAL;
}


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


GB4XBee::Return GB4XBee::getReceivedMessage(uint8_t message[], size_t *message_len)
{
	if(false == g_receive.pending())
	{
		return Return::WAITING_MESSAGE;
	}

	*message_len = g_receive.read(message, *message_len);

	return Return::MESSAGE_RECEIVED;
}


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

