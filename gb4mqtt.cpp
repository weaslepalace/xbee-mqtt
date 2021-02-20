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
	allow_connect = false;
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
	state = State::NOT_CONNECTED;
	return true;	
}


GB4MQTT::Return GB4MQTT::publish(
	char const topic[],
	size_t topic_len,
	uint8_t const message[],
	size_t message_len,
	uint8_t qos, 
	bool disconnect)
{
//	if(false == isReady())
//	{
//		return Return::NOT_READY;
//	}

	if(true == pub_queue.isFull())
	{
		return Return::PUBLISH_QUEUE_FULL;
	}

	MQTTRequest req(
		topic, topic_len,
		message, message_len,
		qos, 0, packet_id.get_next(),
		disconnect);
	pub_queue.insert(req);
	return Return::PUBLISH_QUEUED;	
}


GB4MQTT::Return GB4MQTT::poll()
{
	GB4XBee::State radio_state = radio.poll();
	if(radio_state < GB4XBee::State::SOCKET_READY)
	{
		state = State::NOT_CONNECTED;
		return Return::IN_PROGRESS;
	}

	switch(state)
	{
		case State::NOT_CONNECTED:
		if((nullptr == address) || (true == pub_queue.isEmpty()))
		{
			break;
		}
		if(false == radio.connect(port, address))
		{
			radio.resetSocket();
			break;
		}
		state = State::CONNECTING_SOCKET;
		break;

		case State::CONNECTING_SOCKET:
		if(
			(GB4XBee::State::AWAIT_CONNECTION == radio_state) ||
			(GB4XBee::State::AWAIT_CONNECT_RESPONSE == radio_state))
		{
			break;
		}
		if(GB4XBee::State::CONNECTED != radio_state)
		{
			radio.resetSocket();
			state = State::NOT_CONNECTED;
			break;
		}
		state = State::CONNECT_MQTT;
		break;

		case State::CONNECT_MQTT:
		switch(sendConnectRequest())
		{
			case Return::CONNECT_SENT: 
			state = State::AWAIT_CONNACK;
			break;
			
			case Return::CONNECT_SOCKET_ERROR:
			radio.resetSocket();
			state = State::NOT_CONNECTED;
			break;

			case Return::CONNECT_PACKET_ERROR:
			default:
			//Error state
			break;
		}
		break;

		case State::AWAIT_CONNACK:
		{
			volatile Return r = pollConnackStatus();
			switch(r)
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
				radio.resetSocket();
				state = State::NOT_CONNECTED;
				break;
			}
		}
		break;
 
		case GB4MQTT::State::BEGIN_STANDBY:
		state = State::STANDBY;
		//Flow-through OK

		case State::STANDBY:
		dispatchIncomming();
		if(false == handlePublishRequests())
		{
			radio.resetSocket();
			state = State::NOT_CONNECTED;
			break;
		}

//		Return keepalive_status = pollKeepAliveTimer();
//		switch(keepalive_status)
//		{
//			case Return::KEEPALIVE_TIMER_RUNNING:
//			break;
//
//			case Return::KEEPALIVE_TIMER_PING:
//			sendPingRequest();
//			break;
//
//			case Return::KEEPALIVE_TIMER_RECONNECT:
//			radio.resetSocket();
//			state = State::NOT_CONNECTED;
//			break;
//
//			default:
//			break;
//		}
		break;
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
	GB4XBee::Return r = radio.sendMessage(connect_packet, connect_packet_len);
	switch(r)
	{
		case GB4XBee::Return::MESSAGE_SENT:
		break;

		case GB4XBee::Return::IN_PROGRESS:
		return Return::IN_PROGRESS;	
	
		case GB4XBee::Return::PACKET_ERROR:
		return Return::CONNECT_PACKET_ERROR;

		case GB4XBee::Return::DISCONNECTED:
		case GB4XBee::Return::SOCKET_ERROR:
		default:
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
		radio.getReceivedMessage(
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
	if(nullptr == node)
	{
		return nullptr;
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
	enum msgTypes type = static_cast<enum msgTypes>(0);
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
		status = Return::CONNACK_TIMEOUT;
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

	GB4XBee::Return r = radio.sendMessage(ping_request, PINGREQ_SIZE);
	switch(r)
	{
		case GB4XBee::Return::MESSAGE_SENT:
		break;

		case GB4XBee::Return::IN_PROGRESS:
		return Return::IN_PROGRESS;	

		case GB4XBee::Return::PACKET_ERROR:
		return Return::PING_PACKET_ERROR;
	
		case GB4XBee::Return::DISCONNECTED:
		case GB4XBee::Return::SOCKET_ERROR:
		default:
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
	GB4XBee::Return r = radio.sendMessage(packet, packet_len);
	switch(r)
	{
		case GB4XBee::Return::MESSAGE_SENT:
		status = Return::PUBLISH_SENT;
		break;

		case GB4XBee::Return::IN_PROGRESS:
		status = Return::IN_PROGRESS;
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
						if(false == req->disconnect)
						{
							req->duplicate = 1;
						}
						req->start_time = millis();	
					}
					req->ready_to_send = false;
				}
				break;
		
				case Return::PUBLISH_PACKET_ERROR:
				pub_queue.remove(node);
				break;
			
				case Return::IN_PROGRESS:
				break;
	
				case Return::PUBLISH_SOCKET_ERROR:
				default:
				return false;
			}
		}
		else
		{
			if(true == req->got_puback)
			{
				pub_queue.remove(node);
				if(true == req->disconnect)
				{
					//Disconnect after transmission
					uint8_t disconn[2] = {0xE0, 0x00};
					radio.sendMessage(disconn, 2);
					pub_queue.remove(node);	
					return false; //Forces socket to reset
				}
				continue;
			}
	
			if((millis() - req->start_time) < GB4MQTT_PUBLISH_TIMEOUT)
			{
				continue;
			}

			if(true == req->disconnect)
			{
				//Disconnect after transmission
				uint8_t disconn[2] = {0xE0, 0x00};
				radio.sendMessage(disconn, 2);
				pub_queue.remove(node);	
				return false; //Forces socket to reset
			}

			if(req->tries > GB4MQTT_PUBLISH_MAX_TRIES)
			{
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

