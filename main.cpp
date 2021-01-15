
#include "Arduino.h"
#include "gb4mqtt.h"
//#include "gb4xbee.h"
#include "MQTTPacket.h"
#include <cstdio>


static uint8_t rx_buffer[128];
static uint8_t *_fetch_data = NULL;
static int _fetch_head = 0;
static int fetch_callback(uint8_t *c, int length)
{
	if(NULL == _fetch_data)
	{
		return -1;
	}
	int i = 0; 
	for(; i < length; i++)
	{
		c[i] = _fetch_data[i + _fetch_head];
	} 
	_fetch_head += i;
	return length;
}

static void fetch_rewind()
{
	_fetch_data = NULL;
	_fetch_head = 0;
}

static void set_fetch_data(uint8_t *r)
{
	_fetch_data = r;
}


class GB4MQTTClient {
	public:
	GB4MQTTClient()
	{
		state = Await::CONACK;
		is_connected = false;
	}

	enum class Await {
		CONACK,
		PUBSUB,
		SUBACK,
		PUBACK,
		PUBREQ,
		PUBCOMP,
		UNSUBACK,
		PINGRESP,
		PUBLISH
	} state;

	bool is_connected;

	private:
};


class GB4MQTTTopic {
	public:
	enum class QoS {
		AT_MOST_ONCE,
		AT_LEAST_ONCE,
		EXACTLY_ONCE
	} qos;
};



static bool got_mqtt_connack = false;
static bool got_mqtt_suback = false;
static bool got_mqtt_beep_topic = false;

//static void receive_callback(
//	xbee_sock_t sock,
//	uint8_t status,
//	void const *payload,
//	size_t payload_length)
//{
//	if(payload_length >= RX_BUFFER_LEN)
//	{
//		payload_length = RX_BUFFER_LEN;
//	}
////	memcpy(rx_buffer, payload, payload_length);
////	rx_buffer[payload_length] = '\0';
//	got_response = true;
//
//	static GB4MQTTClient client;
//	set_fetch_data((uint8_t *)payload);
//	enum msgTypes type = (enum msgTypes) MQTTPacket_read(
//		rx_buffer,
//		payload_length,
//		fetch_callback);
//	switch(client.state)
//	{
//		case GB4MQTTClient::Await::CONACK:
//		if(CONNACK == type)
//		{
//			enum connack_return_codes code;
//			uint8_t session_bit;
//			if(0 == MQTTDeserialize_connack(
//				&session_bit,
//				(uint8_t*)&code,
//				rx_buffer,
//				payload_length))
//			{
//				break;
//			}
//			if(MQTT_CONNECTION_ACCEPTED != code)
//			{
//				break;
//			}
//			client.state = GB4MQTTClient::Await::SUBACK;
//			client.is_connected = true;
//
//			got_mqtt_connack = true;
//		}
//		break;
//
//		case GB4MQTTClient::Await::SUBACK:
//		if(SUBACK == type)
//		{
//			uint16_t packet_id;
//			int qos_count;
//			int qos[1];
//			if(1 != MQTTDeserialize_suback(
//				&packet_id,
//				1,
//				&qos_count,
//				qos,
//				rx_buffer,
//				payload_length))
//			{
//				break;
//			}
//			got_mqtt_suback = true;
//
//			client.state = GB4MQTTClient::Await::PUBLISH;
//		}
//		break;
//		
//		case GB4MQTTClient::Await::PUBLISH:
//		if(PUBLISH == type)
//		{
//			uint8_t duplicate_flag;
//			uint8_t retain_flag;
//			int qos;
//			uint16_t packet_id;
//			MQTTString topic;
//			uint8_t *topic_data;
//			int topic_data_len;	
//			if(1 != MQTTDeserialize_publish(
//				&duplicate_flag,
//				&qos,
//				&retain_flag,
//				&packet_id,
//				&topic,
//				&topic_data,
//				&topic_data_len,
//				rx_buffer,
//				payload_length))
//			{
//				break;
//			}
//			if(0 != memcmp("beep", topic.lenstring.data, 4))
//			{
//				break;
//			}
//			if('1' != topic_data[0])
//			{
//				break;
//			}
//			got_mqtt_beep_topic = true;
//		}
//		break;
//
//		default:
//		break;
//	}
//	fetch_rewind();
//}

volatile uint32_t timeoutCount = 0;

int main()
{
	init();
	watchdogDisable();

	pinMode(LED_BUILTIN, OUTPUT);
	digitalWrite(LED_BUILTIN, LOW);

//	GB4MQTT mqtt(9600, "em", 1883, "69.62.134.151");
//	GB4MQTT mqtt(9600, "em", 1883, "13.93.230.129");
	GB4MQTT mqtt(
		9600,
//		"em",
		"hologram",
		8883,
//		"nsps-sentinel-iot-hub.azure-devices.net",
		"40.78.22.17",
		true,
		"tonitrus",
		"nsps-sentinel-iot-hub.azure-devices.net/tonitrus",
		"");
	Serial.begin(mqtt.getRadioBaud());
	Serial.setTimeout(10000);

	mqtt.begin();
//	while(GB4MQTT::Return::CONNECTED != mqtt.poll());
	int delay_start = millis();
	for(uint32_t cnt = 0;;)
	{	
		if((true == mqtt.is_ready()) && ((millis() - delay_start) > 2500))
		{
			delay_start = millis();
			uint8_t cnt_s[18];
			size_t cnt_len = snprintf(
				reinterpret_cast<char*>(cnt_s), 18,
				"{cnt=%d}",
				cnt);
			cnt++;
			static char constexpr t[] = "devices/tonitrus/messages/events/";
			mqtt.publish(t, sizeof t, cnt_s, cnt_len);
		}
		mqtt.poll();
	}

//	GB4XBee radio(9600, "em");
//	Serial.begin(radio.getBaud());
//	Serial.setTimeout(10000);
//
//	radio.begin();
//	while(GB4XBee::Return::STARTUP_COMPLETE != radio.pollStartup());	
//
//	GB4XBee::Return status;
//	do
//	{
////		if(false == radio.connect(1883, "69.62.134.151"))
//		if(false == radio.connect(1883, "13.93.230.129"))
//		{
//			radio.resetSocket();
//			while(GB4XBee::Return::STARTUP_COMPLETE != radio.pollStartup());
//			continue;
//		}		
//		do
//		{
//			status = radio.pollConnectStatus();
//		}
//		while(GB4XBee::Return::CONNECT_IN_PROGRESS == status);
//		switch(status)
//		{
//			case GB4XBee::Return::CONNECT_TIMEOUT:
//			timeoutCount++;
//			//Fall-through OK
//			case GB4XBee::Return::CONNECT_TRY_AGAIN:
//			case GB4XBee::Return::CONNECT_ERROR:
//			delay(1000);
//			break;
//			
//			default:
//			break;
//		}
//	}
//	while(GB4XBee::Return::CONNECTED != status);
//  
//  digitalWrite(LED_BUILTIN, HIGH);
//
//	uint8_t connect_packet[100];
//	MQTTPacket_connectData conn = MQTTPacket_connectData_initializer; 
//	conn.clientID.cstring = (char*)"NovaSource-GB4";
//	conn.keepAliveInterval = 60;
//	conn.cleansession = 1;
//	int connect_packet_len = MQTTSerialize_connect(connect_packet, 100, &conn);
//	if(0 == connect_packet_len)
//	{
//		Serial.println("Failed to create MQTT connect packet");
//		for(;;);
//	}
//	radio.sendMessage(connect_packet, connect_packet_len);
//	
//	uint8_t payload[64];
//	size_t payload_len = 64;
//	while(GB4XBee::Return::MESSAGE_RECEIVED != radio.pollReceivedMessage(payload, &payload_len));
//	GB4MQTTClient client;
//	set_fetch_data(payload);
//	enum msgTypes type = (enum msgTypes) MQTTPacket_read(rx_buffer, payload_len, fetch_callback);
//	if(CONNACK == type)
//	{
//		enum connack_return_codes code;
//		uint8_t session_bit;
//		if(0 != MQTTDeserialize_connack(
//			&session_bit,
//			(uint8_t*)&code,
//			rx_buffer,
//			payload_len))
//		{
//			if(MQTT_CONNECTION_ACCEPTED == code)
//			{
//				client.state = GB4MQTTClient::Await::SUBACK;
//				client.is_connected = true;
//
//				got_mqtt_connack = true;
//				digitalWrite(LED_BUILTIN, HIGH);
//			}
//		}
//	}
	digitalWrite(LED_BUILTIN, HIGH);
	for(;;);	
}
