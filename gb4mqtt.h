/**
 * gb4mqtt.h
 */

#ifndef GB4MQTT_H
#define GB4MQTT_H

#include "gb4xbee.h"
#include "static_queue.h"
#include "MQTTPacket.h"

static int32_t constexpr GB4MQTT_CONNACK_TIMEOUT = 10000;
static size_t constexpr GB4MQTT_MAX_PACKET_SIZE = 128;

static uint16_t constexpr GB4MQTT_NETWORK_TIMEOUT_INTERVAL_SECONDS = 120;
//static uint16_t constexpr GB4MQTT_NETWORK_TIMEOUT_INTERVAL_SECONDS = 20;
static float constexpr GB4MQTT_NETWORK_KEEPALIVE_MODIFIER = 2.0;
static int32_t constexpr GB4MQTT_NETWORK_TIMEOUT_INTERVAL =
	GB4MQTT_NETWORK_TIMEOUT_INTERVAL_SECONDS * 1000;
static int32_t constexpr GB4MQTT_KEEPALIVE_INTERVAL = 
	GB4MQTT_NETWORK_TIMEOUT_INTERVAL / GB4MQTT_NETWORK_KEEPALIVE_MODIFIER;

static char constexpr GB4MQTT_DEFAULT_CLIENT_ID[] = "UNNAMED_GB4";
static char constexpr GB4MQTT_CONNECT_PACKET_SIZE = 80;
static int32_t constexpr GB4MQTT_PUBLISH_TIMEOUT = 10000;
static uint8_t constexpr GB4MQTT_PUBLISH_MAX_TRIES = 4;
static size_t constexpr GB4MQTT_MAX_QUEUE_DEPTH = 2;


class MQTTRequest {
	public:
	static size_t constexpr TOPIC_MAX_SIZE = 64;
	static size_t constexpr MESSAGE_MAX_SIZE = 900;

	MQTTRequest()
	{
		topic_len = 0;
		message_len = 0;
		qos = 0;
		retain = 0;
		packet_id = 0;
		start_time = 0;
		tries = 0;
		duplicate = false;
		got_puback = false;
		ready_to_send = false;
		disconnect = false;
		active = false;
	}

	MQTTRequest(
		char const top[], size_t toplen,
		uint8_t const mes[], size_t meslen,
		uint8_t q = 0, uint8_t r = 0, uint16_t id = 0,
		bool disconn = false)
	{
		strncpy(topic, top, toplen);
		topic_len = toplen;
		memcpy(message, mes, meslen);
		message_len = meslen;
		qos = q;
		retain = r;
		packet_id = id;
		start_time = millis();
		tries = 0;
		duplicate = 0;
		got_puback = false;
		ready_to_send = true;
		disconnect = disconn;
		active = false;
	}

	char topic[TOPIC_MAX_SIZE];
	size_t topic_len;
	uint8_t message[MESSAGE_MAX_SIZE];
	size_t message_len;
	uint8_t qos;
	uint8_t retain;
	uint8_t duplicate = 0;
	uint16_t packet_id;
	int32_t start_time;
	uint8_t tries = 0;
	bool got_puback = false;
	bool ready_to_send = false;
	bool disconnect = false;
	bool active = false;
};


class GB4MQTT {
	public:
	GB4MQTT(
		uint32_t radio_baud,
		char const *radio_apn,
		bool use_tls = false,
		uint16_t network_port = 0,
		char *network_address = const_cast<char*>(""),
		char *id = const_cast<char*>(GB4MQTT_DEFAULT_CLIENT_ID),
		char *name = const_cast<char*>(""),
		char *pwd = const_cast<char*>(""));

	enum class Return {
		NOT_READY = -16,
		PUBACK_MALFORMED = -15,
		PUBLISH_TIMEOUT = -14,
		PUBLISH_QUEUE_FULL = -13,
		PUBLISH_QUEUE_EMPTY = -12,
		PUBLISH_SOCKET_ERROR = -11,
		PUBLISH_PACKET_ERROR = -10,
		BUFFER_FULL = -9,
		DISPATCH_TYPE_ERROR = -8,
		PING_PACKET_ERROR = -7,
		PING_SOCKET_ERROR = -6,
		CONNECT_PACKET_ERROR = -5,
		CONNECT_SOCKET_ERROR = -4,
		CONNACK_TIMEOUT = -3,
		CONNACK_REJECTED = -2,
		CONNACK_ERROR = -1,	
		LISTENING = 0,
		RADIO_READY,
		CONNECT_SENT,	
		GOT_CONNACK,
		WAITING_MESSAGE,
		MESSAGE_RECEIVED,
		CONNECTED,
		KEEPALIVE_TIMER_RECONNECT,
		KEEPALIVE_TIMER_PING,
		KEEPALIVE_TIMER_RUNNING,
		PING_SENT,
		DISPATCHED_PING,
		PUBLISH_QUEUED,
		PUBLISH_SENT,
		PUBACK_IN_PROGRESS,
		GOT_PUBACK,
		DISPATCHED_PUBACK,
		IN_PROGRESS,
	};

	bool begin();
	Return publish(
		char const topic[],
		size_t topic_len,
		uint8_t const message[],
		size_t message_len,
		uint8_t qos = 0,
		bool disconenct = false);
	Return poll();

	void end();

	uint32_t getRadioBaud()
	{
		return radio.getBaud();
	}

	uint64_t getRadioSerialNumber()
	{
		return radio.getSerialNumber();
	}
	
	bool isReady()
	{
		return (state == State::STANDBY);
	}

	void setClientID(char *id)
	{
		client_id = id;
	}

	void setPassword(char *pwd)
	{
		client_password = pwd;
	}

	void setClientName(char *name)
	{
		client_name = name;
	}

	void setAddress(char *addr)
	{
		address = addr;
	}

	void setPort(uint16_t p)
	{
		port = p;
	}
	
	void startConnect()
	{
		allow_connect = true;
	}

	private:
	Return sendConnectRequest();
	Return sendPingRequest();
	Return pollIncomming(
		uint8_t message[],
		size_t *message_len,
		enum msgTypes *type);
	Return dispatchIncomming();	
	Return pollConnackStatus();
	Return checkConnack(uint8_t message[], size_t message_len);
	bool checkPuback(uint8_t message[], size_t message_len);
	void resetKeepAliveTimer();
	Return pollKeepAliveTimer();
	Return sendPublishRequest(MQTTRequest &req);
	bool handlePublishRequests();
	void handleInFlightRequests();

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
	int32_t keepalive_period_start_time;
	int32_t ping_period_start_time;
	char *client_id;
	char *client_name;
	char *client_password;
	uint16_t port;
	char *address;
	bool allow_connect; 
	
	class PacketId {
		public:
		PacketId()
		{
			packet_id = 1;
		}

		uint8_t get_next()
		{
			uint8_t pid = packet_id;
			packet_id++;
			return pid;
		}
		
		private:
		uint16_t packet_id = 1;
	};
	PacketId packet_id;	
	
	MQTTRequest m_publish_request;
//	StaticQueue<MQTTRequest, GB4MQTT_MAX_QUEUE_DEPTH> pub_queue;
//	StaticQueue<MQTTRequest, GB4MQTT_MAX_QUEUE_DEPTH> in_flight;
//	StaticQueue<MQTTRequest, GB4MQTT_MAX_QUEUE_DEPTH> sub_queue;
};


#endif //GB4MQTT_H
