/**
 * gb4mqtt.h
 */

#ifndef GB4MQTT_H
#define GB4MQTT_H

#include "gb4xbee.h"
#include "MQTTPacket.h"

static int32_t constexpr GB4MQTT_CONNACK_TIMEOUT = 10000;
static size_t constexpr GB4MQTT_MAX_PACKET_SIZE = 128;
static uint16_t constexpr GB4MQTT_DEFAULT_KEEPALIVE_INTERVAL = 60;
static char constexpr GB4MQTT_DEFAULT_CLIENT_ID[] = "UNNAMED_GB4";

class GB4MQTT {
	public:
	GB4MQTT(
		uint32_t radio_baud,
		char const *radio_apn,
		uint16_t network_port,
		char const network_address[]);

	enum class Return {
		CONNECT_PACKET_ERROR = -5,
		CONNECT_SOCKET_ERROR = -4,
		CONNACK_TIMEOUT = -3,
		CONNACK_REJECTED = -2,
		CONNACK_ERROR = -1,	
		CONNECT_SENT = 0,	
		WAITING_CONNACK,
		GOT_CONNACK,
		WAITING_MESSAGE,
		MESSAGE_RECEIVED,
		CONNECTED,
		IN_PROGRESS,
	};

	bool begin();
	Return poll();

	void end();

	uint32_t getRadioBaud()
	{
		return radio.getBaud();
	}
	
	private:
	Return sendConnectRequest();
	Return pollIncomming(
		uint8_t message[],
		size_t *message_len,
		enum msgTypes *type);	
	Return pollConnackStatus();
	enum class State {
		RADIO_INIT_FAILED = -2,
		INIT = 0,
		STARTING_RADIO,		
		RESET_SOCKET,
		RESTARTING_SOCKET,
		NOT_CONNECTED,
		CONNECTING_SOCKET,
		BEGIN_CONNECT_RETRY_DELAY,
		CONNECT_RETRY_DELAY,
		CONNECT_MQTT,
		AWAIT_CONNACK,
		BEGIN_STANDBY,
		STANDBY,
		AWAIT_SUBACK,
		PING,
		AWAIT_PINGACK
	};
	GB4XBee radio;
	State state;
	int err;
	int32_t connect_start_time;
	uint16_t keepalive_interval;
	char const *client_id;
	uint16_t const port;
	char const *address;
//	GB4MQTTQueue pub_queue;
//	GB4MQTTQueue sub_queue;
};


//static size_t constexpr GB4MQTTQUEUE_ARRAY_SIZE 64;
//class GB4MQTTQueue {
//	
//	public:
//	GB4MQTTQueue();
//	class Request {
//		char *topic;
//		size_t topic_len;
//		uint16_t id;
//		uint8_t qos;
//		void *message;
//		size_t message_len;
//	};
//
//	bool enqueue(
//		char *topic,
//		size_t topic_len, 
//		uint16_t id,
//		uint8_t qos,
//		void *message,
//		size_t message_len);
//	Request peak();
//	uint8_t peakQoS();
//	Request dequeue();
//	bool is_empty();
//	bool is_full()
//	bool get_length();
//
//	private:
//	Request array[GB4MQTTQUEUE_ARRAY_SIZE]; 
//	size_t head;
//	size_t tail;
//	size_t length;
//	bool full;
//};

#endif //GB4MQTT_H
