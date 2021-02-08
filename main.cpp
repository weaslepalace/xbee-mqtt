
#include "Arduino.h"
#include "gb4mqtt.h"
//#include "gb4xbee.h"
#include "MQTTPacket.h"
#include <cstdio>


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
		"em",
//		"hologram",
		true,
		8883,
//		const_cast<char*>("nsps-sentinel-iot-hub.azure-devices.net"),
		const_cast<char*>("40.78.22.17"),
		const_cast<char*>("tonitrus"),
		const_cast<char*>("nsps-sentinel-iot-hub.azure-devices.net/tonitrus"),
		const_cast<char*>(""));
	Serial.begin(mqtt.getRadioBaud());
	Serial.setTimeout(10000);

	mqtt.begin();
//	while(GB4MQTT::Return::CONNECTED != mqtt.poll());
	int delay_start = millis();
	for(uint32_t cnt = 0;;)
	{	
//		if((true == mqtt.is_ready()) && ((millis() - delay_start) > 2500))
//		{
//			delay_start = millis();
//			uint8_t cnt_s[300];
//			size_t cnt_len = snprintf(
//				reinterpret_cast<char*>(cnt_s), 300,
//				"["
//					"{"
//						"\"device_id\":\"tonitrus\","
//						"\"latitude\":15.0437602,"
//						"\"longitude\":30.5005255,"
//						"\"robotState\":\"Error\","
//						"\"timestamp\":\"2021-02-03T00:23:22.595Z\""
//					"}"
//				"]");
//			cnt++;
//			static char constexpr t[] = "devices/tonitrus/messages/events/";
//			mqtt.publish(t, sizeof t, cnt_s, cnt_len, 1);
//		}
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
	for(;;);	
}
