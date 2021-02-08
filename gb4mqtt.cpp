/**
 *	gb4mqtt.cpp
 */

#include "gb4mqtt.h"

GB4MQTT::GB4MQTT(
	uint32_t radio_baud,
	char const *radio_apn,
	bool use_tls,
	uint16_t network_port,
	char *network_address,
	char *id,
	char *name,
	char *pwd) :
	radio(radio_baud, radio_apn, use_tls),
	port(network_port),
	address(network_address),
	client_id(id),
	client_name(name),
	client_password(pwd)
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


GB4MQTT::Return GB4MQTT::publish(
	char const topic[],
	size_t topic_len,
	uint8_t const message[],
	size_t message_len,
	uint8_t qos)
{
	if(false == is_ready())
	{
		return Return::NOT_READY;
	}

	if(true == pub_queue.isFull())
	{
		return Return::PUBLISH_QUEUE_FULL;
	}

	MQTTRequest req(
		topic, topic_len,
		message, message_len,
		qos, 0, packet_id.get_next());
	pub_queue.insert(req);
	digitalWrite(LED_BUILTIN, HIGH);
	return Return::PUBLISH_QUEUED;	
}


//GB4MQTT::Return GB4MQTT::subscribe(
//	uint8_t topic[],
//	size_t topic_len,
//	uint8_t max_qos)
//{
//
//}


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
		if(nullptr == address)
		{
			break;
		}
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
			case Return::LISTENING:
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

 
		case GB4MQTT::State::BEGIN_STANDBY:
		state = State::STANDBY;
		//Flow-through OK

		case State::STANDBY:
		dispatchIncomming();
//		handleInFlightRequests();
		if(false == handlePublishRequests())
		{
			state = State::BEGIN_CONNECT_RETRY_DELAY;
			break;
		}

		switch(pollKeepAliveTimer())
		{
			case Return::KEEPALIVE_TIMER_RUNNING:
			break;

			case Return::KEEPALIVE_TIMER_PING:
			sendPingRequest();
			break;

			case Return::KEEPALIVE_TIMER_RECONNECT:
			state = State::BEGIN_CONNECT_RETRY_DELAY;
			break;

			default:
			break;
		}
	
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
		break;

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

	if(State::NOT_CONNECTED == state)
	{
		return Return::RADIO_READY;
	}
	if((State::BEGIN_STANDBY != state) && (State::STANDBY != state))
	{
		return Return::IN_PROGRESS;
	}

	return Return::CONNECTED;
}


GB4MQTT::Return GB4MQTT::sendConnectRequest()
{
	uint8_t connect_packet[GB4MQTT_CONNECT_PACKET_SIZE];
	MQTTPacket_connectData conn = MQTTPacket_connectData_initializer;
	conn.clientID.cstring = const_cast<char*>(client_id);
	conn.username.cstring = const_cast<char*>(client_name);
	conn.password.cstring = const_cast<char*>(client_password);
	conn.keepAliveInterval = GB4MQTT_NETWORK_TIMEOUT_INTERVAL_SECONDS;
	conn.cleansession = 1;
	int connect_packet_len = MQTTSerialize_connect(
		connect_packet,
		GB4MQTT_CONNECT_PACKET_SIZE,
		&conn);
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




//GB4MQTT::Return GB4MQTT::pollConnackStatus()
//{
//	uint8_t message[GB4MQTT_MAX_PACKET_SIZE];
//	size_t message_len = sizeof message;
//	enum msgTypes type;
//	if(Return::WAITING_MESSAGE == pollIncomming(message, &message_len, &type))
//	{
//		if((millis() - connect_start_time) > GB4MQTT_CONNACK_TIMEOUT)
//		{
//			return Return::CONNACK_TIMEOUT;
//		}
//		return Return::WAITING_CONNACK;
//	}
GB4MQTT::Return GB4MQTT::checkConnack(uint8_t message[], size_t message_len)
{
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


static MQTTRequest *findRequestByPacketId(
	StaticQueue<MQTTRequest, GB4MQTT_MAX_QUEUE_DEPTH> &queue,
	uint16_t id)
{
	LinkedNode<MQTTRequest> *node = queue.peakNode();
	for( ; nullptr != node; node = node->next())
	{
		if(node->value().packet_id == id)
		{
			break;
		}
	}
	return &node->value();
}


bool GB4MQTT::checkPuback(uint8_t message[], size_t message_len)
{
	uint16_t id;
	uint8_t dup;
	uint8_t type;
	if(0 == MQTTDeserialize_ack(&type, &dup, &id, message, message_len))
	{
		return false;
	}
	MQTTRequest *req = findRequestByPacketId(pub_queue, id);
	if(nullptr == req)
	{
		return false;
	}
	req->got_puback = true;
	return true;	
}


GB4MQTT::Return GB4MQTT::dispatchIncomming()
{
	uint8_t message[GB4MQTT_MAX_PACKET_SIZE];
	size_t message_len = sizeof message;
	enum msgTypes type;
	if(Return::WAITING_MESSAGE == pollIncomming(message, &message_len, &type))
	{
		return Return::LISTENING;
	}

	//Times to call resetKeepAliveTimer:
	//	PINGRESP, CONNACK,
	//	PUBACK, PUBREC, PUBCOMP,
	//	SUBACK, UNSUBACK
	GB4MQTT::Return status;
	switch(type)
	{
		case CONNACK:
		status = checkConnack(message, message_len);
		resetKeepAliveTimer();
		break;
	
		case PINGRESP:
		resetKeepAliveTimer();		
		status = Return::DISPATCHED_PING;
		break;

		case PUBLISH:
		//1. Deserialize and get topic info and message
		//2. Check topic length and name for matches in the array of registered topics
		//3. Call the callback associated with the registered topic with the message 
		break;

		case PUBACK:
		if(false == checkPuback(message, message_len))
		{
			status = Return::PUBACK_MALFORMED;
		}
		status = Return::DISPATCHED_PUBACK;
		resetKeepAliveTimer();
		break;

		case PUBREC:
		case PUBCOMP:
		case SUBACK:
		case UNSUBACK:
		resetKeepAliveTimer();

		default:
		status = Return::DISPATCH_TYPE_ERROR;
		break;
	}

	return status;
}


GB4MQTT::Return GB4MQTT::pollConnackStatus()
{
	Return status = dispatchIncomming();
	if(Return::GOT_CONNACK == status)
	{
		return Return::GOT_CONNACK;
	}
	if((millis() - connect_start_time) > GB4MQTT_CONNACK_TIMEOUT)
	{
		return Return::CONNACK_TIMEOUT;
	}
	return status;

}


GB4MQTT::Return GB4MQTT::sendPingRequest()
{
	static size_t constexpr PINGREQ_SIZE = 2;
	uint8_t ping_request[PINGREQ_SIZE];
	
	if(PINGREQ_SIZE != MQTTSerialize_pingreq(ping_request, PINGREQ_SIZE))
	{
		return Return::PING_PACKET_ERROR;
	}
	if(GB4XBee::Return::MESSAGE_SENT != radio.sendMessage(
		ping_request,
		PINGREQ_SIZE))
	{
		return Return::PING_SOCKET_ERROR;
	}
	ping_period_start_time = millis();
	return Return::PING_SENT;
}


void GB4MQTT::resetKeepAliveTimer()
{
	keepalive_period_start_time = millis();
	ping_period_start_time = millis();
}


GB4MQTT::Return GB4MQTT::pollKeepAliveTimer()
{
	int32_t network_interval = millis() - keepalive_period_start_time;
	if(network_interval > GB4MQTT_NETWORK_TIMEOUT_INTERVAL)
	{
		return Return::KEEPALIVE_TIMER_RECONNECT;
	}
	int32_t ping_interval = millis() - ping_period_start_time;
	if(ping_interval > GB4MQTT_KEEPALIVE_INTERVAL)
	{
		return Return::KEEPALIVE_TIMER_PING;
	}
	return Return::KEEPALIVE_TIMER_RUNNING;
}


GB4MQTT::Return GB4MQTT::sendPublishRequest(MQTTRequest *req)
{
	static size_t constexpr PUBLISH_HEADER_SIZE = 6;
	static size_t constexpr PACKET_MAX_SIZE = 
		MQTTRequest::TOPIC_MAX_SIZE + 
		MQTTRequest::MESSAGE_MAX_SIZE +
		PUBLISH_HEADER_SIZE;

	uint8_t packet[PACKET_MAX_SIZE];
	MQTTString topic = {.cstring = req->topic};
	int32_t packet_len = MQTTSerialize_publish(
		packet, PACKET_MAX_SIZE,
		req->duplicate,
		req->qos,
		req->retain,
		req->packet_id,
		topic,
		req->message, req->message_len);
	if(packet_len <= 0)
	{
		return Return::PUBLISH_PACKET_ERROR;
	}

	Return status;
	switch(radio.sendMessage(packet, packet_len))
	{
		case GB4XBee::Return::MESSAGE_SENT:
		status = Return::PUBLISH_SENT;
		break;

		case GB4XBee::Return::DISCONNECTED:
		case GB4XBee::Return::BUFFER_FULL:
		case GB4XBee::Return::PACKET_ERROR:
		case GB4XBee::Return::SOCKET_ERROR:
		default:
		status = Return::PUBLISH_SOCKET_ERROR;
		break;
	}
	return status;
}


//void GB4MQTT::handleInFlightRequests()
//{
//	for(
//		LinkedNode<MQTTRequest> *node = in_flight.peakNode();
//		nullptr != node;
//		node = node->next())
//	{
//		MQTTRequest *req = &node->value();
//		if(true == req->got_puback)
//		{
//			digitalWrite(LED_BUILTIN, LOW);
//			in_flight.remove(node);
//			continue;
//		}
//
//		if((millis() - req->start_time) < GB4MQTT_PUBLISH_TIMEOUT)
//		{
//			continue;
//		}
//
//		if(req->tries > GB4MQTT_PUBLISH_MAX_TRIES)
//		{
//			digitalWrite(LED_BUILTIN, LOW);
//			in_flight.remove(node);
//			continue;
//		}
//		req->start_time = millis();
//		req->tries++;
//		if(true == pub_queue.insert(req))
//		{
//			in_flight.remove(node);
//		}
//	}
//}



bool GB4MQTT::handlePublishRequests()
{	
	for(
		LinkedNode<MQTTRequest> *node = pub_queue.peakNode();
		nullptr != node;
		node = node->next())
	{
		MQTTRequest *req = &node->value();
		if(true == req->ready_to_send)
		{
			Return send_ok = sendPublishRequest(req);
			switch(send_ok)
			{
				case Return::PUBLISH_SENT:
				if(0 == req->qos)
				{
					pub_queue.remove(node);
				}
				else
				{
					if(0 == req->duplicate)
					{
						req->duplicate = 1;
						req->start_time = millis();	
					}
					req->ready_to_send = false;
				}
				break;
		
				case Return::PUBLISH_PACKET_ERROR:
				pub_queue.remove(node);
				break;
				
				case Return::PUBLISH_SOCKET_ERROR:
				default:
				digitalWrite(LED_BUILTIN, LOW);
				return false;
			}
		}
		else
		{
			if(true == req->got_puback)
			{
				digitalWrite(LED_BUILTIN, LOW);
				pub_queue.remove(node);
				continue;
			}
	
			if((millis() - req->start_time) < GB4MQTT_PUBLISH_TIMEOUT)
			{
				continue;
			}
	
			if(req->tries > GB4MQTT_PUBLISH_MAX_TRIES)
			{
				digitalWrite(LED_BUILTIN, LOW);
				pub_queue.remove(node);
				continue;
			}
			req->start_time = millis();
			req->tries++;
			req->ready_to_send = true;
		}
	}
	return true;
}

