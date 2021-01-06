


GB4MQTT::GB4MQTT()
{
	state = GB4MQTT::State::INIT;
	err = 0;
}


bool GB4MQTT::begin()
{
	if(false == radio.begin())
	{
		err = radio.get_error();
		state = GB4MQTT::State::RADIO_INIT_FAILED;
		return false;
	}
	state = GB4MQTT::State::STARTING_RADIO;
	return true;	
}

GB4MQTT::tick()
{
	switch(state)
	{

		case GB4MQTT::State::NOT_CONNECTED:
		if(false == xbee_cmd.sendConnect())
		{
			//Error state
			break;
		}
		state = GB4MQTT::State::CONNECTING_SOCKET;
		break;

		case GB4MQTT::State::CONNECTING_SOCKET:
		if(GB4XBee::CONNECTED == xbee_cmd.pollConnectStatus())
		{
			break;
		}
		state = GB4MQTT::State::CONNECT_MQTT;
		break;

		case GB4MQTT::State::CONNECT_MQTT:
		if(connect() < 0)
		{
			//Error state
			break;
		}
		state = GB4MQTT::State::AWAIT_CONNACK;
		break;

		case GB4MQTT::State::AWAIT_CONNACK:
		if(false == pollConnackStatus())
		{
			//there should be a timeout here
			break;
		}
		state = GB4MQTT::State::BEGIN_STANDBY;
		break;

		case GB4MTQQ::State::BEGIN_STANDBY:
		resetKeepAliveTimer();
		state = GB4MQTT::State::STANDBY;
		break;

		//PUBLISH messages can also come in during this state
		//  and all of the below states
		case GB4MQTT::State::STANDBY:
		if(true == pollKeepAliveTimer())
		{
			sendPing();
			state = GB4MQTT::State::AWAIT_PINGACK;
		}
		else if(false != pub_queue.is_empty())
		{
			if(0 != pub_queue.peakQoS())
			{
			}
		}
		else if(false != sub_queue.is_empty())
		{
			state = GB4MQTT::State:AWAIT_SUBACK;
		}
		break;

		case GB4MQTT::State::AWAIT_SUBACK:
		if(false == pollSubackStatus())
		{
			//There should be a timeout here
			break;
		}
		break;

		case GB4MQTT::State::PING:
		if(ping() < 0)
		{
			//Error state
			break;
		}
		state = GB4MQTT::State::AWAIT_PINGACK;
		break;

		case GB4MQTT::State::AWAIT_PINGACK:
		if(false == pollPingackStatus())
		{
			//Timeout?
			break;
		}
		break;
		
	}
}



GB4MQTTQueue::GB4MQTTQueue()
	head(0),
	tail(0),
	length(0),
	full(0)
{
}


bool GB4MQTTQueue::is_empty()
{
	return (false == full) && (head == tail);
}

bool GB4MQTTQueue::is_full()
{
	return full;
}

bool GB4MQTTQueue::get_length()
{
	size_t len = GB4MQTTQUEUE_ARRAY_SIZE;
	if(false == full)
	{
		if(head >= tail)
		{
			len = head - tail;
		}
		else
		{
			len = GB4MQTTQUEUE_ARRAY_SIZE + head - tail;
		}
	}

	return len;
}


void GB4MQTTQueue::enqueue(
	char *topic,
	size_t topic_len,
	uint16_t id,
	uint8_t qos,
	void *message,
	size_t message_len)
{
	array[head].topic = topic;
	array[head].topic_len = topic_len;
	array[head].id = id;
	array[head].qos = qos;
	array[head].message = message;
	array[head].message_len = message_len;
	if(true == full)
	{
		tail = (tail + 1) % GB4MQTTQUEUE_ARRAY_SIZE;
	}
	head = (head + 1) % GB4MQTTQUEUE_ARRAY_SIZE;
	
	full = (head == tail);		
}	



bool GB4MQTTQueue::peak(GB4MQTTQueue::Request *req)
{
	if(true == empty())
	{
		return false;
	}
	req = &array[tail];
	return true;
}


uint8_t GB4MQTTQueue::peakQoS()
{
	if(true == empty())
	{
		return 0;
	}
	return array[tail].qos;
}


bool GB4MQTTQueue::dequeue(GB4MQTTQueue::Request *req)
{
	if(true == empty())
	{
		return false;
	}
	req = &array[tail];
	full = false;
	tail = (tail + 1) % GB4MQTT_ARRAY_SIZE;
}


