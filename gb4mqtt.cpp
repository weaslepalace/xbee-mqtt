/**
 *	gb4mqtt.cpp
 */

#include "gb4mqtt.h"

GB4MQTT::GB4MQTT(
	uint32_t radio_baud,
	char const *radio_apn,
	uint16_t network_port,
	char const *network_address) :
	radio(radio_baud, radio_apn),
	port(network_port),
	address(network_address),
	keepalive_interval(GB4MQTT_DEFAULT_KEEPALIVE_INTERVAL),
	client_id(GB4MQTT_DEFAULT_CLIENT_ID)
{
	connect_start_time = 0;
	state = State::INIT;
	err = 0;
}


bool GB4MQTT::begin()
{
	if(false == radio.begin())
	{
//		err = radio.get_error();
		state = State::RADIO_INIT_FAILED;
		return false;
	}
	state = State::STARTING_RADIO;
	return true;	
}


GB4MQTT::Return GB4MQTT::poll()
{
	switch(state)
	{
		case State::RESET_SOCKET:
		radio.resetSocket();
		state = State::STARTING_RADIO;
		break;		

		case State::STARTING_RADIO:
		if(GB4XBee::Return::STARTUP_COMPLETE != radio.pollStartup())
		{
			break;
		}
		state = State::NOT_CONNECTED;	
		break;
	

		case State::NOT_CONNECTED:
		if(false == radio.connect(port, address))
		{
			state = State::RESET_SOCKET;
			break;
		}
		state = State::CONNECTING_SOCKET;
		break;

		case State::CONNECTING_SOCKET:
		switch(radio.pollConnectStatus())
		{
			case GB4XBee::Return::CONNECT_IN_PROGRESS:
			break;

			case GB4XBee::Return::CONNECTED:
			state = State::CONNECT_MQTT;
			break;

			case GB4XBee::Return::CONNECT_TRY_AGAIN:
			case GB4XBee::Return::CONNECT_TIMEOUT:
			default:
			state = State::BEGIN_CONNECT_RETRY_DELAY;
			break;
		}
		break;

		case State::BEGIN_CONNECT_RETRY_DELAY:
		radio.startConnectRetryDelay();
		state = State::CONNECT_RETRY_DELAY;	
		break;
	
		case State::CONNECT_RETRY_DELAY:
		if(false == radio.pollConnectRetryDelay())
		{
			break;
		}
		state = State::NOT_CONNECTED;
		break;
		
		case State::CONNECT_MQTT:
		switch(sendConnectRequest())
		{
			case Return::CONNECT_SENT: 
			state = State::AWAIT_CONNACK;
			break;
			
			case Return::CONNECT_SOCKET_ERROR:
			state = State::RESET_SOCKET;
			break;

			case Return::CONNECT_PACKET_ERROR:
			default:
			//Error state
			break;
		}
		break;

		case State::AWAIT_CONNACK:
		switch(pollConnackStatus())
		{
			case Return::WAITING_CONNACK:
			break;

			case Return::GOT_CONNACK:
			state = State::BEGIN_STANDBY;
			break;

			//According to the specification, we should close the connection if the
			//	CONNACK doesn't come in within a reasonable amount of time.
			case Return::CONNACK_TIMEOUT:
			case Return::CONNACK_REJECTED:
			case Return::CONNACK_ERROR:
			default:
			//Close and reconnect? or reset sockets
			state = State::RESET_SOCKET;
			break;
		}
		break;


 
//		case GB4MTQQ::State::BEGIN_STANDBY:
//		resetKeepAliveTimer();
//		state = State::STANDBY;
//		break;
//
//		//PUBLISH messages can also come in during this state
//		//  and all of the below states
//		case State::STANDBY:
//		if(true == pollKeepAliveTimer())
//		{
//			sendPing();
//			state = State::AWAIT_PINGACK;
//		}
//		else if(false != pub_queue.is_empty())
//		{
//			if(0 != pub_queue.peakQoS())
//			{
//			}
//		}
//		else if(false != sub_queue.is_empty())
//		{
//			state = State:AWAIT_SUBACK;
//		}
//		break;
//
//		case State::AWAIT_SUBACK:
//		if(false == pollSubackStatus())
//		{
//			//There should be a timeout here
//			break;
//		}
//		break;
//
//		case State::PING:
//		if(ping() < 0)
//		{
//			//Error state
//			break;
//		}
//		state = State::AWAIT_PINGACK;
//		break;
//
//		case State::AWAIT_PINGACK:
//		if(false == pollPingackStatus())
//		{
//			//Timeout?
//			break;
//		}
//		break;
	}

	//Half-assed dev test; should be removed
	if(State::BEGIN_STANDBY != state)
	{
		return Return::IN_PROGRESS;
	}

	return Return::CONNECTED;
}




GB4MQTT::Return GB4MQTT::sendConnectRequest()
{
	uint8_t connect_packet[64];
	MQTTPacket_connectData conn = MQTTPacket_connectData_initializer;
	conn.clientID.cstring = const_cast<char*>(client_id);
	conn.keepAliveInterval = keepalive_interval;
	conn.cleansession = 1;
	int connect_packet_len = MQTTSerialize_connect(connect_packet, 64, &conn);
	if(0 == connect_packet_len)
	{
		return Return::CONNECT_PACKET_ERROR;
	}
	if(GB4XBee::Return::MESSAGE_SENT !=
		radio.sendMessage(
			connect_packet,
			connect_packet_len))
	{
		return Return::CONNECT_SOCKET_ERROR;
	}
	connect_start_time = millis();
	return Return::CONNECT_SENT;
}


static uint8_t *_fetch_data = NULL;
static int _fetch_head = 0;
static void _set_fetch_data(uint8_t *d)
{
	_fetch_head = 0;
	_fetch_data = d;
}

static int _fetch_callback(uint8_t *c, int len)
{
	if(NULL == _fetch_data)
	{
		return -1;
	}
	int i = 0; 
	for(; i < len; i++)
	{
		c[i] = _fetch_data[i + _fetch_head];
	} 
	_fetch_head += i;
	return len;
}


GB4MQTT::Return GB4MQTT::pollIncomming(
	uint8_t message[],
	size_t *message_len,
	enum msgTypes *type)
{
	uint8_t payload[GB4MQTT_MAX_PACKET_SIZE];
	size_t payload_len = sizeof payload;
	
	if(GB4XBee::Return::WAITING_MESSAGE ==
		radio.pollReceivedMessage(
			payload,
			&payload_len))
	{
		return Return::WAITING_MESSAGE;
	}

	if(payload_len > *message_len)
	{
		payload_len = *message_len;
	}
	
	_set_fetch_data(payload);
	*type = (enum msgTypes) MQTTPacket_read(
		message,
		payload_len,
		_fetch_callback);
	*message_len = payload_len;
	return Return::MESSAGE_RECEIVED;
}


GB4MQTT::Return GB4MQTT::pollConnackStatus()
{
	uint8_t message[GB4MQTT_MAX_PACKET_SIZE];
	size_t message_len = sizeof message;
	enum msgTypes type;
	if(Return::WAITING_MESSAGE == pollIncomming(message, &message_len, &type))
	{
		if((millis() - connect_start_time) > GB4MQTT_CONNACK_TIMEOUT)
		{
			return Return::CONNACK_TIMEOUT;
		}
		return Return::WAITING_CONNACK;
	}

	enum connack_return_codes code;
	uint8_t session;
	if(0 == MQTTDeserialize_connack(
		&session,
		(uint8_t*)&code,
		message,
		message_len))
	{
		return Return::CONNACK_ERROR;
	}
	
	if(MQTT_CONNECTION_ACCEPTED != code)
	{
		return Return::CONNACK_REJECTED;
	}
	
	return Return::GOT_CONNACK;
}


//GB4MQTTQueue::GB4MQTTQueue()
//	head(0),
//	tail(0),
//	length(0),
//	full(0)
//{
//}
//
//
//bool GB4MQTTQueue::is_empty()
//{
//	return (false == full) && (head == tail);
//}
//
//bool GB4MQTTQueue::is_full()
//{
//	return full;
//}
//
//bool GB4MQTTQueue::get_length()
//{
//	size_t len = GB4MQTTQUEUE_ARRAY_SIZE;
//	if(false == full)
//	{
//		if(head >= tail)
//		{
//			len = head - tail;
//		}
//		else
//		{
//			len = GB4MQTTQUEUE_ARRAY_SIZE + head - tail;
//		}
//	}
//
//	return len;
//}
//
//
//void GB4MQTTQueue::enqueue(
//	char *topic,
//	size_t topic_len,
//	uint16_t id,
//	uint8_t qos,
//	void *message,
//	size_t message_len)
//{
//	array[head].topic = topic;
//	array[head].topic_len = topic_len;
//	array[head].id = id;
//	array[head].qos = qos;
//	array[head].message = message;
//	array[head].message_len = message_len;
//	if(true == full)
//	{
//		tail = (tail + 1) % GB4MQTTQUEUE_ARRAY_SIZE;
//	}
//	head = (head + 1) % GB4MQTTQUEUE_ARRAY_SIZE;
//	
//	full = (head == tail);		
//}	
//
//
//
//bool GB4MQTTQueue::peak(GB4MQTTQueue::Request *req)
//{
//	if(true == empty())
//	{
//		return false;
//	}
//	req = &array[tail];
//	return true;
//}
//
//
//uint8_t GB4MQTTQueue::peakQoS()
//{
//	if(true == empty())
//	{
//		return 0;
//	}
//	return array[tail].qos;
//}
//
//
//bool GB4MQTTQueue::dequeue(GB4MQTTQueue::Request *req)
//{
//	if(true == empty())
//	{
//		return false;
//	}
//	req = &array[tail];
//	full = false;
//	tail = (tail + 1) % GB4MQTT_ARRAY_SIZE;
//}


