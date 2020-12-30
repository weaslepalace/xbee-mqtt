
#include "Arduino.h"

#include "xbee/platform.h"
#include "xbee/socket.h"
#include "xbee/atcmd.h"
#include "MQTTPacket.h"

xbee_dispatch_table_entry_t const xbee_frame_handlers[] = {
	XBEE_FRAME_HANDLE_LOCAL_AT,
	XBEE_SOCK_FRAME_HANDLERS,
	XBEE_FRAME_TABLE_END
};


static size_t constexpr RX_BUFFER_LEN = 1500;
static uint8_t rx_buffer[RX_BUFFER_LEN];
static bool got_response = false;


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
	_fetch_head = i;
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
		PINGRESP
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


static size_t constexpr ACCESS_POINT_NAME_LEN = 40;
static char access_point_name[ACCESS_POINT_NAME_LEN];
static bool got_access_point_name = false;
static int readAPNCallback(xbee_cmd_response_t const *response)
{
	memcpy(access_point_name, response->value_bytes, response->value_length);
	return XBEE_ATCMD_DONE;
}

static int apiMode = -1;
static int readAPIModeCallback(xbee_cmd_response_t const *response)
{
	apiMode = response->value;
	return XBEE_ATCMD_DONE;
}

class GB4XBee {
	public:
	GB4XBee()
	{
		is_command_mode = false;
	}

	bool enterCommandMode()
	{
		delay(1200);
		Serial.write("+++", 3);
		delay(1200);
		is_command_mode = responseOK();
		return is_command_mode;
	}

	/**
	 * Force the Xbee into command mode by sending a break signal
	 *   (hold low / assert) for 6 seconds
 	 * Caveat: The arduino isn't really tooled for this sort of interface
 	 *   although it should be
	 */
	void enterCommandModeSlow()
	{
		//To execute a break, configure the UART port as a GPIO and set low
		// Serial.end() must be called first, otherwise pinMode(), and
		// digitalWrite() will have no effect
		Serial.end();
		pinMode(1, OUTPUT);
		digitalWrite(1, LOW);
		delay(6000);

		//When the UART pin was set as a GPIO, it was disconnected from the
		// uart peripheral. Our friends at Arduino were not expecting you to do
		// this, and provide no function nor method to reconnect the pin to the
		// peripheral. This lower lever function provided by pio.c in libsam
		// reconfigures the pin as a UART. This is normally called in init()
		// which is called by at the top of main().
		PIO_Configure(
			g_APinDescription[PINS_UART].pPort,
			g_APinDescription[PINS_UART].ulPinType,
			g_APinDescription[PINS_UART].ulPin,
			g_APinDescription[PINS_UART].ulPinConfiguration);
	
		
		Serial.begin(9600);
		Serial.setTimeout(10000);
	}

	bool factoryReset()
	{
		Serial.print("ATRE\r");
		return responseOK();
	}

	bool applyChanges()
	{
		Serial.print("ATAC\r");
		return responseOK();
	}

	bool writeChanges()
	{
		Serial.print("ATWR\r");
		return responseOK();
	}

	bool writeChanges(xbee_dev_t *xbee)
	{
		int16_t handle = xbee_cmd_create(xbee, "WR");
		if(handle < 0)
		{
			return false;
		}
		return (0 == xbee_cmd_send(handle));
	}

	bool readAPIMode(xbee_dev_t *xbee)
	{
		int16_t handle = xbee_cmd_create(xbee, "AP");
		if(handle < 0)
		{
			return false;
		}
		xbee_cmd_set_callback(handle, readAPIModeCallback, NULL);
		xbee_cmd_send(handle);
		return true;
	}

	bool readAccessPointName(char *name, size_t nameLen)
	{
		Serial.print("ATAN\r");
		Serial.readBytes(name, nameLen);
		return responseOK();
	}

	bool readAccessPointName(xbee_dev_t *xbee)
	{
		int16_t handle = xbee_cmd_create(xbee, "AN");
		if(handle < 0)
		{
			return false;
		}
		xbee_cmd_set_callback(handle, readAPNCallback, NULL);
		xbee_cmd_send(handle);
		return true;
	}

	bool setAccessPointName(char const *name, size_t const name_len)
	{
		size_t const cmd_len = name_len + 6;
		char cmd[cmd_len] = "ATAN";
		strncat(cmd, name, name_len);
		strncat(cmd, "\r", 1);
		Serial.print(cmd);
		return responseOK();
	}

	bool setAccessPointName(xbee_dev_t *xbee, char const *name)
	{
		int16_t handle = xbee_cmd_create(xbee, "AN");
		if(handle < 0)
		{
			return false;
		}
		if(xbee_cmd_set_param_str(handle, name) < 0)
		{
			return false;
		}
		return (0 == xbee_cmd_send(handle));
	}

	bool exitCommandMode()
	{
		Serial.write("ATCN\r", 5);
		return responseOK();
//		is_command_mode = false;
//		return true;
	}

	bool enableAPIMode()
	{
		Serial.write("ATAP1\r", 6);
		return responseOK();
	}

	private:
	static constexpr size_t OK_LEN = 2;
	static constexpr size_t OK_RESP_LEN = 3;
	static constexpr size_t OK_BUFFER_LEN = 4;

	bool is_command_mode;
	bool responseOK()
	{
		char ok_buffer[OK_BUFFER_LEN];
		size_t n = Serial.readBytes(ok_buffer, OK_RESP_LEN);
		if(n < OK_RESP_LEN)
		{
			return false;
		}
		return (0 == memcmp("OK", ok_buffer, OK_LEN));
	}

};


static bool got_connection = false;
static bool got_socket_id = false;
static bool got_close_resp = false;
static bool got_socket_create_error = false;

static void notify_callback(
	xbee_sock_t sock,
	uint8_t frame_type,
	uint8_t message)
{
	switch(frame_type)
	{
		case XBEE_FRAME_TX_STATUS:
		break;

		case XBEE_FRAME_SOCK_STATE:
		break;

		case XBEE_FRAME_SOCK_CREATE_RESP:
		if(XBEE_SOCK_STATUS_SUCCESS == message)
		{
			got_socket_id = true;
			got_socket_create_error = false;
		}
		else
		{
			got_socket_id = false;
			got_socket_create_error = true;
		}
		break;		

		case XBEE_FRAME_SOCK_CONNECT_RESP:
		if(XBEE_SOCK_STATUS_SUCCESS == message)
		{
			got_connection = true;
		}
		break;

		case XBEE_FRAME_SOCK_CLOSE_RESP:
		if(XBEE_SOCK_STATUS_SUCCESS == message)
		{
			got_close_resp = true;
		}
		break;
		
		case XBEE_FRAME_SOCK_LISTEN_RESP:
		default:
		break;
	}
}

static bool got_mqtt_connack = false;

static void receive_callback(
	xbee_sock_t sock,
	uint8_t status,
	void const *payload,
	size_t payload_length)
{
	if(payload_length >= RX_BUFFER_LEN)
	{
		payload_length = RX_BUFFER_LEN;
	}
//	memcpy(rx_buffer, payload, payload_length);
//	rx_buffer[payload_length] = '\0';
	got_response = true;

	static GB4MQTTClient client;
	set_fetch_data((uint8_t *)payload);
	enum msgTypes type = (enum msgTypes) MQTTPacket_read(
		rx_buffer,
		payload_length,
		fetch_callback);
	switch(client.state)
	{
		case GB4MQTTClient::Await::CONACK:
		if(CONNACK == type)
		{
			enum connack_return_codes code;
			uint8_t session_bit;
			if(0 == MQTTDeserialize_connack(
				&session_bit,
				(uint8_t*)&code,
				rx_buffer,
				payload_length))
			{
				break;
			}
			if(MQTT_CONNECTION_ACCEPTED != code)
			{
				break;
			}
			client.state = GB4MQTTClient::Await::PUBSUB;
			client.is_connected = true;

			got_mqtt_connack = true;
		}
		break;
		
		default:
		break;
	}
}



int main()
{
	init();
	watchdogDisable();

	xbee_serial_t ser = {
		.baudrate = 9600
	};
	Serial.begin(ser.baudrate);
	Serial.setTimeout(10000);


	GB4XBee xbee_init;

	int ts[4];
	ts[0] = millis();
	char ok_buffer[3];
	size_t n = 0;
	if(false == xbee_init.enterCommandMode())
	{
		Serial.println("+++ !OK");
		for(;;); //ATAP1
	}
	ts[1] = millis();
	if(false == xbee_init.enableAPIMode())
	{
		Serial.println("ATAP1 !OK");
		for(;;); //ATAP1
	}
	ts[2] = millis();
	if(false == xbee_init.exitCommandMode())
	{
		Serial.println("ATCN !OK");
		for(;;); //ATCN
	}
	ts[3] = millis();
	Serial.println();
	for(int i = 0; i < 4; i++)
	{
		Serial.println(ts[i]);
	}

	xbee_dev_t xbee;

	int status = xbee_dev_init(&xbee, &ser, NULL, NULL);
	if(0 != status)
	{
		Serial.print("Failed to init XBee ");
		Serial.println(status);
		for(;;);
	}	

	xbee_cmd_init_device(&xbee);
	bool xbee_init_done = false;
	while(false == xbee_init_done)
	{
		do
		{
			xbee_dev_tick(&xbee);
			status = xbee_cmd_query_status(&xbee);
		}
		while(-EBUSY == status);
		if(0 != status)
		{
			Serial.print("Failed to initialize AT command layer ");
			Serial.println(status);
	
			xbee_init.readAPIMode(&xbee);
			while(-1 == apiMode);
			if(1 != apiMode)
			{
//				while(false == xbee_init.enterCommandMode())
//				{
//					delay(1000);
//				}
//				xbee_init.enableAPIMode();
//				xbee_init.applyChanges();
//				xbee_init.writeChanges();
//				xbee_init.exitCommandMode();
			}

			xbee_cmd_query_device(&xbee, 0);
		}
		
		xbee_init_done = true;
	}

	xbee_init.readAccessPointName(&xbee);
	while(false == got_access_point_name);
	if(0 != strcmp("em", access_point_name))
	{
		xbee_init.setAccessPointName(&xbee, "em");
		xbee_init.writeChanges(&xbee);
	}

	xbee_sock_t sock;
	do
	{
		xbee_sock_reset(&xbee);
		sock = xbee_sock_create(
			&xbee,
			XBEE_SOCK_PROTOCOL_TCP,
			&notify_callback);
		if(sock < 0)
		{
			Serial.print("Failed to create socket ");
			Serial.println(sock);
			for(;;);
		}
		while(
			(false == got_socket_id) &&
			(false == got_socket_create_error))
		{
			xbee_dev_tick(&xbee);
		}
	}
	while(true == got_socket_create_error);

	status = xbee_sock_connect(
		sock,
		1883,
		0,
		"69.62.134.151",
		receive_callback);
	if(0 != status)
	{
		xbee_sock_close(sock);
		Serial.print("Failed to connect ");
		Serial.println(status);
		for(;;);
	}
	while(false == got_connection)
	{
		xbee_dev_tick(&xbee);
	}
	
	uint8_t connect_packet[100];
	MQTTPacket_connectData conn = MQTTPacket_connectData_initializer; 
	conn.clientID.cstring = (char*)"NovaSource-GB4";
	conn.keepAliveInterval = 60;
	conn.cleansession = 1;
	int connect_packet_len = MQTTSerialize_connect(connect_packet, 100, &conn);
	if(0 == connect_packet_len)
	{
		Serial.println("Failed to create MQTT connect packet");
		for(;;);
	}
	status = xbee_sock_send(sock, 0, connect_packet, connect_packet_len);
	if(0 != status)
	{
		Serial.print("Failed to send ");
		Serial.println(status);
		for(;;);
	}
	while(false == got_mqtt_connack)
	{
		xbee_dev_tick(&xbee);
	}

	for(;;)
	{
		uint8_t publish_packet[100];
		MQTTString topic = {.cstring = (char *)"count"};
		static uint8_t pub_count[1] = {'0'};
		int publish_packet_len = MQTTSerialize_publish(
			publish_packet, 100,
			0, 0, 0, 0,
			topic,
			pub_count,
			sizeof pub_count);
		pub_count[0]++;
		if(pub_count[0] > '9')
		{
			pub_count[0] = '0';
		}

		status = xbee_sock_send(sock, 0, publish_packet, publish_packet_len);
		if(0 != status)
		{
			Serial.print("Failed to send ");
			Serial.println(status);
			for(;;);
		}
		for(uint32_t tick0 = millis(); millis() - tick0 < 5000;)
		{
			xbee_dev_tick(&xbee);
		}
	}

	for(;;);	
}
