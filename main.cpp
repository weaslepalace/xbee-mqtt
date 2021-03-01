
#include "Arduino.h"
#include "gb4mqtt.h"
//#include "gb4xbee.h"
#include "MQTTPacket.h"
#include <cstdio>
#include <ctime>
#include <sys/time.h>

#define SENTINEL_DESTINATION

#ifdef SENTINEL_DESTINATION
char constexpr client_id[] = "tonitrus";
//char constexpr host[] = "nsps-sentinel-iot-hub.azure-devices.net";
char constexpr host[] = "40.78.22.17";
char constexpr username[] = "nsps-sentinel-iot-hub.azure-devices.net/tonitrus";
char constexpr password[] = "";
char constexpr topic[] = "devices/tonitrus/messages/events/";
#endif //SENTINEL_DESTINATION

#ifdef BRIDGE_DESTINATION
char constexpr client_id[] = "test";
char constexpr host[] = "69.62.134.151";
char constexpr username[] = "";
char constexpr password[] = "";
char constexpr topic[] = "hello";
#endif //BRIDGE_DESTINATION

int main()
{
	init();
	watchdogDisable();

	pinMode(LED_BUILTIN, OUTPUT);
	digitalWrite(LED_BUILTIN, LOW);

	GB4MQTT mqtt(
		9600,
		"em",
		true,
		8883,
		const_cast<char*>(host),
		const_cast<char*>(client_id),
		const_cast<char*>(username),
		const_cast<char*>(password));
	Serial.begin(mqtt.getRadioBaud());
	Serial.setTimeout(10000);

	mqtt.begin();
	
	char const compile_time[22] = __DATE__ " " __TIME__;
	char const mon[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
	struct tm btm = {0};
	char compile_month[4];
	sscanf(compile_time, "%s %d %d %d:%d:%d",
		compile_month,
		&btm.tm_mday,
		&btm.tm_year,
		&btm.tm_hour,
		&btm.tm_min,
		&btm.tm_sec);
	btm.tm_year -= 1900;
	char *monp = strstr(mon, compile_month);
	btm.tm_mon = (monp - mon) / 3;
	time_t epoch = mktime(&btm);
	
#ifdef BRIDGE_DESTINATION
	uint8_t cnt_s[3 * 64];
	for(int i = 0; i < 64; i++)
	{
		snprintf(reinterpret_cast<char*>(&cnt_s[i * 3]), 3, " 02X", i * 3); 
	}
	size_t cnt_len = sizeof cnt_s;	
#endif //BRIDGE_DESTINATION	

	int delay_start = millis();
#ifdef SENTINEL_DESTINATION
	float lat = 15.0;
	float lon = 30.0;
	static size_t constexpr report_size = 150;
	static size_t constexpr number_of_reports = 6;
	static size_t constexpr report_buffer_size = 
		report_size * number_of_reports;
	uint8_t cnt_s[report_buffer_size] = "[";
	size_t report_index = 1;

//	mqtt.publish(topic, sizeof topic, cnt_s + 1, report_size * 4, true);
#endif //SENTINEL_DESTINATION
	for(uint32_t cnt = 0, objnum = 0;;)
	{	
		if((millis() - delay_start) > 10000)
		{
			delay_start = millis();
#ifdef SENTINEL_DESTINATION
			
			char t_buf[32];
			time_t now = epoch + (delay_start / 1000);
			size_t t_buf_len = strftime(
				t_buf, 32,
				"%Y-%m-%dT%H:%M:%SZ", gmtime(&now));
			report_index += snprintf(
				reinterpret_cast<char*>(&cnt_s[report_index]), report_size,
			 	"{"
			 		"\"device_id\":\"%s\","
			 		"\"latitude\":%f,"
			 		"\"longitude\":%f,"
			 		"\"robotState\":\"Error\","
			 		"\"timestamp\":\"%.*s\","
					"\"cnt\":%d"
			 	"}%c",
				client_id,
				lat,
				lon,
				32, t_buf,
				objnum,
				cnt < 5 ? ',' : ']');
			lat += 0.1;
			lon -= 0.05;
#endif //SENTINEL_DESTINATION
			cnt++;
			objnum++;
		}

		if(6 == cnt)
		{
			cnt = 0;
#ifdef SENTINEL_DESTINATION
			size_t cnt_len = report_index; 
			report_index = 1;
#endif //SENTINEL_DESTINATION
			mqtt.publish(topic, sizeof topic, cnt_s, cnt_len, 1, true);
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
	for(;;);	
}
