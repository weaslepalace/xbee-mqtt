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


/**
 *	Initialize and start the MQTT and radio state machine
 *	Note: This is not a concurrent, and GB4MQTT::poll() must be called once per
 *		loop in order to advance and execute the state machien
 *	@return
 *		true on success 
 *		false on failure
 */
bool GB4MQTT::begin()
{
	if(false == radio.begin())
	{
		state = State::RADIO_INIT_FAILED;
		return false;
	}
	state = State::NOT_CONNECTED;
	return true;	
}


/**
 *	Enqueue a message to publish. The message will be sent via the state
 *		machine in subsequent calls to GB4MQTT::poll().
 *	Note (1st March 2021): Because of reasons, only one message can be enqueued
 *	@param topic - Publish topic string
 *	@param topic_len - Length of topic in bytes
 *	@param message - Message to publish
 *	@param message_len - Length of message in bytes
 *	@param qos - Publish Quality of Serice
 *	         0 - At most once
 *	         1 - At least once
 *	         2 - Only once
 *	@param disconnect - Indicates to the state machine whether or not the
 *	                    connection should be terminated after the publish
 *	                    request has been sent
 *	             true - Disconnect after publish
 *	            false - Stay connected a after publish
 *	@return GB4MQTT::Return::PUBLISH_QUEUED
 */
GB4MQTT::Return GB4MQTT::publish(
	char const topic[],
	size_t topic_len,
	uint8_t const message[],
	size_t message_len,
	uint8_t qos, 
	bool disconnect)
{
	strncpy(m_publish_request.topic, topic, topic_len);
	m_publish_request.topic_len = topic_len;
	memcpy(m_publish_request.message, message, message_len);
	m_publish_request.message_len = message_len;
	m_publish_request.qos = qos;
	m_publish_request.retain = 0;
	m_publish_request.duplicate = 0;
	m_publish_request.start_time = millis();
	m_publish_request.tries = 0;
	m_publish_request.packet_id = packet_id.get_next();
	m_publish_request.disconnect = disconnect;
	m_publish_request.got_puback = false;
	m_publish_request.ready_to_send = true;
	m_publish_request.active = true;
	
	return Return::PUBLISH_QUEUED;	
}


/**
 *	Execute the MQTT and radio state machines
 *	Note: Must be called once per main loop
 *	@return
 *		GB4MQTT::Return::IN_PROGRESS - Radio is establishing a socket
 *		                               connection to the server via the tower
 *		                               or access point
 *		GB4MQTT::Return::RADIO_READY - Radio has established a socket
 *		                               conenction, but a connection has not yet
 *		                               been made to the broker via the MQTT
 *		                               protocol
 *		GB4MQTT::Return::CONNECTED - A connection to the has been established
 *		                             and messages can now be published to the 
 *		                             broker
 */
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
		if((nullptr == address) || (false == m_publish_request.active))
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


/**
 *	Formulate and transmit an MQTT CONNECT request packet to the broker
 *	Must be done before messages can be published
 *	@return
 *		GB4MQTT::Return::CONNECT_PACKET_ERROR - There is a problem with the
 *		                                        packet. Most likely, the packet
 *		                                        is too long to fit into a buffer
 *		GB4MQTT::Return::CONNECT_SOCKET_ERROR - There is a problem with the
 *		                                        socket, and a new connection
 *		                                        should be established
 *		GB4MQTT::Return::IN_PROGRESS - The radio is busy sending another packet
 *		                               Wait for the transmission to end or
 *		                               timeout
 *		GB4MQTT::Return::DISCONNECTED - An attempt was made to send on a socket
 *		                                that isn't connected
 *		GB4MQTT::Return::CONNECT_SENT - The CONNECT request has bee sent to
 *		                                the broker
 */
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


/**
 *	Helper function for resetting data used by _fetch_callback()
 *	Call before the call to MQTTPacket_read()
 *	@param d - Pointer to the user data that _fetch_callback() will write into
 */
static uint8_t *_fetch_data = NULL;
static int _fetch_head = 0;
static void _set_fetch_data(uint8_t *d)
{
	_fetch_head = 0;
	_fetch_data = d;
}

/**
 *	Callback used by MQTTPacket_read() to write received MQTT packets into a 
 *	user data array.
 *	@param c - Pointer to user data array
 *	@param len - Length of user data arary in bytes
 *	@return
 *		-1 on failure
 *		Otherwise, length of data written
 */
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


/**
 *	Check if there is pending data to read. If so, read it into a message buffer
 *	@param message - Message buffer to read data into
 *	@param message_len - Input - Max size of message buffer to prevent overrun
 *	                     Output - The length of the received message in bytes
 *	@param type - Output - The MQTT Control Packet type
 *	                       see paho.mqtt.embedded-c/MQTTPacket/src/MQTTPacket.h
 *	@return
 *		GB4MQTT::Return::WAITING_MESSAGE - No message was received
 *		GB4MQTT::Return::MESSAGE_RECEIVED - Yep
 *
 */
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


/**
 *	Check if a received message contains a CONNACK.
 *	If so, look at its contents to see if the connection was accepted
 *	@param message - Input - Message buffer that may contain a CONNACK packet
 *	@param message_len - Length of message in bytes
 *	@return 
 *		GB4MQTT::Return:CONNACK_ERROR - There wsa an error decoding the packet
 *		                                It's probably not a CONNACK
 *		GB4MQTT::Return::CONNACK_REJECTED - The CONNECT request was rejected,
 *		                                    most likely due to an auth error
 *		                                    Check the certificates, username,
 *		                                    password, and client ID
 *		GB4MQTT::Return::GOT_CONNACK - The broker has accepted the connection
 */
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


/**
 *	Check if a received message is a PUBACK control packet from the broker
 *	@param message - Input - Message buffer that may contain a PUBACK packet
 *	@param message_len - Length of message in bytes
 *	@return
 *		true - The message is a PUBACK
 *		false - The message is something other than a PUBACK	
 */
bool GB4MQTT::checkPuback(uint8_t message[], size_t message_len)
{
	uint16_t id;
	uint8_t dup;
	uint8_t type;
	if(0 == MQTTDeserialize_ack(&type, &dup, &id, message, message_len))
	{
		return false;
	}
	if(m_publish_request.packet_id != id)
	{
		return false;
	}
	m_publish_request.got_puback = true;
	return true;	
}


/**
 *	Check for pending data to read, read it, and do something with it.
 *	@return
 *		GB4MQTT::Return::LISTENING - No message received
 *		GB4MQTT::Return::DISPATCH_TYPE_ERROR - Message type doesn't match a
 *		                                       known type in enum msgType
 *		GB4MQTT::Return::DISPATCHED_PING - Read a PINGRESP message
 *		                                   reset the keepalive timer
 *		GB4MQTT::Return::CONNACK_REJECTED - The CONNECT request was rejected,
 *		                                    most likely due to an auth error
 *		                                    Check the certificates, username,
 *		                                    password, and client ID
 *		GB4MQTT::Return::GOT_CONNACK - The broker has accepted the connection
 *		GB4MQTT::Return::PUBACK_MALFORMED -
 *		GB4MQTT::Return::DISPATCHED_PUBACK - 
 *
 */
GB4MQTT::Return GB4MQTT::dispatchIncomming()
{
	uint8_t message[GB4MQTT_MAX_PACKET_SIZE];
	size_t message_len = sizeof message;
	enum msgTypes type = static_cast<enum msgTypes>(0);
	if(Return::WAITING_MESSAGE == pollIncomming(message, &message_len, &type))
	{
		return Return::LISTENING;
	}

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


/**
 *	Check for a response to a CONNECT control packet and maintain a timeout
 *	@return
 *		GB4MQTT::Return::LISTENING - No message received
 *		GB4MQTT::Return::CONNACK_TIMEOUT - No response has been received in the
 *		                                   number of milliseconds given by
 *		                                   GB4MQTT_CONNACK_TIMEOUT
 *		GB4MQTT::Return::DISPATCH_TYPE_ERROR - Message type doesn't match a
 *		                                       known type in enum msgType
 *		GB4MQTT::Return::GOT_CONNACK - The broker has accepted the connection
 */
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


/**
 *	Send a PINGREQ control packet to maintain keepalive status,
 *	and check if the broker is listening
 *	A PINGREQ should be sent if there has been nothing sent or recieved for
 *	the number of millisecond given by GB4MQTT_KEEPALIVE_INTERVAL
 *	@return
 *		GB4MQTT::Return::PING_PACKET_ERROR - There was a problem with the
 *		                                     packet format, or it is too large
 *		                                     to fit into a buffer
 *		GB4MQTT::Return::IN_PROGRESS - The radio is busy sending another packet
 *		                               Wait for the transmission to end or
 *		                               timeout
 *		GB4MQTT::Return::SOCKET_ERROR - There was a problem with the socket and
 *		                                it must be disconnected
 *		GB4MQTT::Return::PING_SENT - The PINGREQ has been successfully queued
 *		                             for transmission
 */
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


/**
 *	Reset the keepalive timer to prevent a timeout
 *	Should be called whenever a message sent to the broker is acknowledged
 */
void GB4MQTT::resetKeepAliveTimer()
{
	keepalive_period_start_time = millis();
	ping_period_start_time = millis();
}


/**
 *	Update the keepalive timer. Called once per call to GB4MQTT::poll() if
 *	state is GB4MQTT::State::CONNECTED
 *	@return 
 *		GB4MQTT::Return::KEEPALIVE_TIMER_RECONNECT - A timeout has ocurred and
 *		                                             a new connection should be
 *		                                             attempted
 *		GB4MQTT::Return::KEEPALIVE_TIMER_PING - A PINGREQ should be sent in
 *		                                        order to prevent a timeout
 *		GB4MQTT::Retrun::KEEPALIVE_TIMER_RUNNING - No timeout, everything's fine
 */
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


/**
 *	Formulate and transmit an MQTT Publish control packet to the broker
 *	This will be called by GB4MQTT::poll() after a publish message has been
 *	enqueued with a call to GB4MQTT::publish
 *	@param req - A publish request object generated by a call to
 *	             GB4MQTT::publish()
 *	@return 
 *		GB4MQTT::Return::IN_PROGRESS - The radio is busy sending another packet
 *		                               Wait for the transmission to end or
 *		                               timeout
 *		GB4MQTT::Return::PUBLISH_SOCKET_ERROR - There was a problem with the
 *		                                        socket and it must be
 *		                                        disconnected
 *		GB4MQTT::Return::PUBLISH_PACKET_ERROR - There was a problem formatting
 *		                                        the control packet
 *		GB4MQTT::Return::PUBLISH_SENT - The PUBLISH packet has been
 *		                                successfully queued for
 *		                                transmission
 */
GB4MQTT::Return GB4MQTT::sendPublishRequest(MQTTRequest &req)
{
	static size_t constexpr PUBLISH_HEADER_SIZE = 6;
	static size_t constexpr PACKET_MAX_SIZE = 
		MQTTRequest::TOPIC_MAX_SIZE + 
		MQTTRequest::MESSAGE_MAX_SIZE +
		PUBLISH_HEADER_SIZE;

	uint8_t packet[PACKET_MAX_SIZE];
	MQTTString topic = {.cstring = req.topic};
	int32_t packet_len = MQTTSerialize_publish(
		packet, PACKET_MAX_SIZE,
		req.duplicate,
		req.qos,
		req.retain,
		req.packet_id,
		topic,
		req.message, req.message_len);
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


/**
 *	Handle the transmission a PUBLISH control packets, and keep track of its
 *	state while waiting for a response if Quality of Service is greater than 0.
 *	Retransmit failed packets.
 *	Disconnect when finised if the disconnect is true.
 *	@return
 *		true - Everything's fine
 *		false - There was a problem requiring the socket to be reset
 */
bool GB4MQTT::handlePublishRequests()
{
	if(false == m_publish_request.active)
	{
		return true;
	}	

	if(true == m_publish_request.ready_to_send)
	{
		m_publish_request.ready_to_send = false;
		Return send_ok = sendPublishRequest(m_publish_request);
		switch(send_ok)
		{
			case Return::PUBLISH_SENT:
			if(0 == m_publish_request.qos)
			{
				m_publish_request.active = false;
			}
			else if(
				(0 != m_publish_request.qos) &&
				(0 == m_publish_request.duplicate))
			{
				if(false == m_publish_request.disconnect)
				{
					m_publish_request.duplicate = 1;
				}
				m_publish_request.start_time = millis();	
			}
			break;
	
			case Return::PUBLISH_PACKET_ERROR:
			m_publish_request.active = false;
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
		if(true == m_publish_request.got_puback)
		{
			m_publish_request.active = false;
			if(true == m_publish_request.disconnect)
			{
				//Disconnect after transmission
				uint8_t disconn[2] = {0xE0, 0x00};
				radio.sendMessage(disconn, 2);
				return false; //Forces socket to reset
			}
			return true;
		}
	
		if((millis() - m_publish_request.start_time) < GB4MQTT_PUBLISH_TIMEOUT)
		{
			return true;
		}

		if(true == m_publish_request.disconnect)
		{
			//Disconnect after transmission
			uint8_t disconn[2] = {0xE0, 0x00};
			radio.sendMessage(disconn, 2);
			m_publish_request.active = false;
			return false; //Forces socket to reset
		}

		if(m_publish_request.tries > GB4MQTT_PUBLISH_MAX_TRIES)
		{
			m_publish_request.active = false;
			return false;;
		}
		m_publish_request.start_time = millis();
		m_publish_request.tries++;
		m_publish_request.ready_to_send = true;
	}
	
	return true;
}

