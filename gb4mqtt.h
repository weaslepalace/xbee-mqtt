/**
 * gb4mqtt.h
 */

#ifndef GB4MQTT_H
#define GB4MQTT_H

#include "xbee/platform.h"

static size_t constexpr GB4MQTTQUEUE_ARRAY_SIZE 64;
class GB4MQTTQueue {
	
	public:
	GB4MQTTQueue();
	class Request {
		char *topic;
		size_t topic_len;
		uint16_t id;
		uint8_t qos;
		void *message;
		size_t message_len;
	};

	bool enqueue(
		char *topic,
		size_t topic_len, 
		uint16_t id,
		uint8_t qos,
		void *message,
		size_t message_len);
	Request peak();
	uint8_t peakQoS();
	Request dequeue();
	bool is_empty();
	bool is_full()
	bool get_length();

	private:
	Request array[GB4MQTTQUEUE_ARRAY_SIZE]; 
	size_t head;
	size_t tail;
	size_t length;
	bool full;
};


class GB4MQTT {
	public:
	enum class State {
		XBEE_INIT_FAILED = -2,
		INIT = 0,
		STARTING_XBEE = 1,		


		NOT_CONNECTED,
		CONNECTING_SOCKET,
		CONNECT_MQTT,
		AWAIT_CONNACK,
		BEGIN_STANDBY,
		STANDBY,
		AWAIT_SUBACK,
		PING,
		AWAIT_PINGACK
				
		
	};

	GB4MQTT();
	void ~GB4MQTT();

	void tick();

	bool begin();
	void end();
	
	private:
	xbee_dev_t xbee;
	xbee_ser_t ser;
	State state;
	int why;

	GB4MQTTQueue pub_queue;
	GB4MQTTQueue sub_queue;
};

#endif //GB4MQTT_H
