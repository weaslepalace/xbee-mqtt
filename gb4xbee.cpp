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


GB4XBee::GB4XBee(uint32_t baud, char const apn[]) :
	cast_guard(GB4XBEE_CAST_GUARD),
	ser({.baudrate = baud}),
	guard_time_start(0),
	response_start_time(0),
	got_access_point_name(0),
	need_set_access_point_name(0),
	err(0)
{
	access_point_name_len = strlen(apn);
	if(access_point_name_len >= GB4XBEE_ACCESS_POINT_NAME_SIZE)
	{
		access_point_name_len = GB4XBEE_ACCESS_POINT_NAME_SIZE - 1;
	}
	memcpy(access_point_name, apn, access_point_name_len);
}


uint32_t GB4XBee::getBaud()
{
	return ser.baudrate;
}


char const *GB4XBee::getAPN()
{
	return access_point_name;
}


bool GB4XBee::begin()
{
	int status = xbee_dev_init(&xbee, &ser, NULL, NULL);
	if(0 != status)
	{
		err = status;
		return false;
	}
	
	state = State::START;	
	return true;
}


void GB4XBee::resetSocket()
{
	state = State::BEGIN_CREATE_SOCKET;
}


GB4XBee::Return GB4XBee::pollStartup()
{
	switch(state)
	{
		case State::START:
		startCommandModeGuard();
		state = State::AWAIT_COMMAND_MODE_GUARD_0; 
		break;

		case State::AWAIT_COMMAND_MODE_GUARD_0:
		if(false == pollCommandModeGuard())
		{
			break;
		}
		state = State::BEGIN_COMMAND_MODE;
		break;

		case State::BEGIN_COMMAND_MODE:
		sendEscapeSequence();
		startCommandModeGuard();
		state = State::AWAIT_COMMAND_MODE_GUARD_1;
		break;

		case State::AWAIT_COMMAND_MODE_GUARD_1:
		if(false == pollCommandModeGuard())
		{
			break;
		}
		state = State::AWAIT_COMMAND_MODE_RESPONSE;
		break;

		case State::AWAIT_COMMAND_MODE_RESPONSE:
		switch(pollResponseOK())
		{
			case Return::COMMAND_OK:
			state = State::BEGIN_API_MODE_COMMAND;
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
		state = State::AWAIT_API_MODE_RESPONSE;
		break;

		case State::AWAIT_API_MODE_RESPONSE:
		switch(pollResponseOK())
		{
			case Return::COMMAND_OK:
			state = State::BEGIN_COMMAND_MODE_EXIT;
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
		state = State::AWAIT_COMMAND_MODE_EXIT_RESPONSE;
		break;

		case State::AWAIT_COMMAND_MODE_EXIT_RESPONSE:
		switch(pollResponseOK())
		{
			case Return::COMMAND_OK:
			state = State::BEGIN_INIT_XBEE_API;
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
		state = State::AWAIT_INIT_XBEE_API_DONE;
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
		state = State::BEGIN_READ_APN;
		break;

		case State::BEGIN_READ_APN:
		sendReadAPN();
		state = State::AWAIT_READ_APN_RESPONSE;
		break;

		case State::AWAIT_READ_APN_RESPONSE:
		switch(pollAPNStatus())
		{
			case Return::APN_IS_SET:
			state = State::BEGIN_CREATE_SOCKET;
			break;
			case Return::APN_NOT_SET:
			state = State::SET_APN;
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
		state = State::BEGIN_CREATE_SOCKET;
		break;

		case State::BEGIN_CREATE_SOCKET:
		sendSocketCreate();
		state = State::AWAIT_SOCKET_ID;
		break;

		case State::AWAIT_SOCKET_ID: 	
		switch(pollSocketStatus())
		{
			case Return::GOT_SOCKET_ID:
			state = State::READY;
			break;

			case Return::SOCKET_TIMEOUT:
			case Return::SOCKET_ERROR:
			state = State::BEGIN_CREATE_SOCKET;
			break;

			default:
			break;
		}
		break;

		default:
		break;
	}

	if(state != State::READY)
	{
		return Return::STARTUP_IN_PROGRESS;
	}

	return Return::STARTUP_COMPLETE;
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


static bool notify_connection_lost = false;
static bool notify_connection_started = false;
static bool notify_connection_refused = false;
static bool notify_connection_try_again = false;
static bool notify_got_connection = false;
static bool notify_got_socket_id = false;
static bool notify_socket_error = false;
static bool notify_socket_closed = false;
static void notify_callback(
	xbee_sock_t sock,
	uint8_t frame_type,
	uint8_t message)
{
	switch(frame_type)
	{
		case XBEE_FRAME_TX_STATUS:
		switch(message)
		{
			case XBEE_TX_DELIVERY_SUCCESS:
			break;
			
			case XBEE_TX_DELIVERY_RESOURCE_ERROR:
			notify_connection_try_again = true;
			break;

			case XBEE_TX_DELIVERY_CONNECTION_REFUSED:
			notify_connection_refused = true;
			break;

			default:
			notify_socket_error = true;
			break;
		}
		break;

		case XBEE_FRAME_SOCK_STATE:
		switch(message)
		{
			case XBEE_SOCK_STATE_CONNECTED:
			notify_got_connection = true;
			break;
			
			case XBEE_SOCK_STATE_CONNECTION_REFUSED:
			notify_connection_refused = true;
			break;

			case XBEE_SOCK_STATE_CONNECTION_LOST:
			notify_connection_lost = true;
			break;	
			
			default:
			notify_socket_error = true;
			break;
		}
		break;

		case XBEE_FRAME_SOCK_CREATE_RESP:
		if(XBEE_SOCK_STATUS_SUCCESS == message)
		{
			notify_got_socket_id = true;
			notify_socket_error = false;
		}
		else
		{
			notify_got_socket_id = false;
			notify_socket_error = true;
		}
		break;		

		case XBEE_FRAME_SOCK_CONNECT_RESP:
		if(XBEE_SOCK_STATUS_SUCCESS == message)
		{
			notify_connection_started = true;
		}
		break;

		case XBEE_FRAME_SOCK_CLOSE_RESP:
		if(XBEE_SOCK_STATUS_SUCCESS == message)
		{
			notify_socket_closed = true;
		}
		break;
		
		case XBEE_FRAME_SOCK_LISTEN_RESP:
		default:
		break;
	}
}


bool GB4XBee::sendSocketCreate()
{
	notify_socket_error = false;
	notify_got_socket_id = false;
	notify_socket_closed = false;
	xbee_sock_reset(&xbee);
	sock = xbee_sock_create(&xbee, XBEE_SOCK_PROTOCOL_TCP, notify_callback);
	if(sock < 0)
	{
		err = sock;
		return false;
	}
	socket_create_start_time = millis();
	return true;
}


GB4XBee::Return GB4XBee::pollSocketStatus()
{
	xbee_dev_tick(&xbee);
	if(true == notify_socket_error)
	{
		return Return::SOCKET_ERROR;
	}
	if((millis() - socket_create_start_time) > GB4XBEE_SOCKET_CREATE_TIMEOUT)
	{
		return Return::SOCKET_TIMEOUT;
	}
	if(false == notify_got_socket_id)
	{
		return Return::SOCKET_IN_PROGRESS;
	}
	return Return::GOT_SOCKET_ID;
}



static bool notify_received_message = false;
static uint32_t received_messages_dropped = 0;
static size_t  received_message_len = 0;
static uint8_t received_message_payload[GB4XBEE_RECEIVED_MESSAGE_MAX_SIZE]; 
static void receive_callback(
	xbee_sock_t sock,
	uint8_t status,
	void const *payload,
	size_t payload_length)
{
	if(true == notify_received_message)
	{
		received_messages_dropped++;
		return;
	}

	received_message_len = 
		(payload_length < GB4XBEE_RECEIVED_MESSAGE_MAX_SIZE) ? 
		payload_length : GB4XBEE_RECEIVED_MESSAGE_MAX_SIZE;
	memcpy(received_message_payload, payload, received_message_len);
	notify_received_message = true;
}


bool GB4XBee::connect(uint16_t port, char const *address)
{
	notify_connection_try_again = false;
	notify_connection_refused = false;
	notify_got_connection = false;
	int status = xbee_sock_connect(sock, port, 0, address, receive_callback);
	if(0 != status)
	{
		err = status;
		return false;
	}
	connect_start_time = millis();
	return true;
} 


GB4XBee::Return GB4XBee::pollConnectStatus()
{
	xbee_dev_tick(&xbee);
	if(true == notify_connection_try_again)
	{
		return Return::CONNECT_TRY_AGAIN;
	}
	else if(true == notify_connection_refused)
	{
		return Return::CONNECT_ERROR;
	}
	else if((millis() - connect_start_time) > GB4XBEE_CONNECT_TIMEOUT)
	{
		return Return::CONNECT_TIMEOUT;
	}
	else if(false == notify_got_connection)
	{
		return Return::CONNECT_IN_PROGRESS;
	}

	return  Return::CONNECTED;
}


GB4XBee::Return GB4XBee::pollReceivedMessage(uint8_t message[], size_t *message_len)
{
	xbee_dev_tick(&xbee);
	if(false == notify_received_message)
	{
		return Return::WAITING_MESSAGE;
	}
	
	*message_len =
		(*message_len < received_message_len) ?
		*message_len : received_message_len;
		
	memcpy(message, received_message_payload, *message_len);

	return Return::MESSAGE_RECEIVED;
}


GB4XBee::Return GB4XBee::sendMessage(uint8_t message[], size_t message_len)
{
	int status = xbee_sock_send(sock, 0, message, message_len);
	if(-ENOENT == status)
	{
		return Return::DISCONNECTED;
	}
	else if(0 != status)
	{
		err = status;
		return Return::SOCKET_ERROR;
	}
	return Return::MESSAGE_SENT; 
}



