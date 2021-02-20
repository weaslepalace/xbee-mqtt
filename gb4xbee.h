#ifndef GB4XBEE_H
#define GB4XBEE_H

#include "xbee_notify.h"
#include "xbee/platform.h"
#include "xbee/socket.h"
#include "Arduino.h"

static uint32_t constexpr GB4XBEE_COMMAND_MODE_GUARD_TIME = 1200;
static size_t constexpr GB4XBEE_ACCESS_POINT_NAME_SIZE = 32;
static size_t constexpr GB4XBEE_RECEIVED_MESSAGE_MAX_SIZE = 1500;
static uint32_t constexpr GB4XBEE_DEFAULT_BAUD = 9600;
static uint32_t constexpr GB4XBEE_DEFAULT_COMMAND_TIMEOUT = 10000;
static uint32_t constexpr GB4XBEE_CONNECT_TIMEOUT = 20000;
static uint32_t constexpr GB4XBEE_SOCKET_CREATE_TIMEOUT = 10000;
static uint32_t constexpr GB4XBEE_SOCKET_COOLDOWN_INTERVAL = 2000;
static uint32_t constexpr GB4XBEE_CONNECT_RETRY_DELAY_INTERVAL = 1000;
static uint32_t constexpr GB4XBEE_TLS_PROFILE_TIMEOUT = 10000;
static uint32_t constexpr GB4XBEE_SEND_TIMEOUT = 1000;

class GB4XBee {
	public:
	enum class Return {
		BUFFER_FULL = -13,
		TLS_PROFILE_ERROR = -12,
		TLS_PROFILE_TIMEOUT = -11,
		PACKET_ERROR = -10,
		INIT_TRY_AGAIN = -9,
		CONNECT_TIMEOUT = -8,
		CONNECT_TRY_AGAIN = -7,
		CONNECT_ERROR = -6,
		APN_READ_ERROR = -5, 
		SOCKET_ERROR = -4,
		SOCKET_TIMEOUT = -3,
		COMMAND_TIMEOUT = -2,
		COMMAND_NOT_OK = -1,
		COMMAND_OK = 0,
		COMMAND_IN_PROGRESS,
		INIT_DONE,
		INIT_IN_PROGRESS,
		APN_READ_IN_PROGRESS,
		APN_NOT_SET,
		APN_IS_SET,
		SOCKET_IN_PROGRESS,
		GOT_SOCKET_ID,
		TLS_PROFILE_IN_PROGRESS,
		TLS_PROFILE_OK,		
		CONNECT_IN_PROGRESS,
		CONNECTED,
		DISCONNECTED,
		WAITING_MESSAGE,
		MESSAGE_RECEIVED,
		MESSAGE_SENT,
		STARTUP_IN_PROGRESS,
		STARTUP_COMPLETE,
		IN_PROGRESS
	};

	enum class State {
		START,
		AWAIT_COMMAND_MODE_GUARD_0,
		BEGIN_COMMAND_MODE,
		AWAIT_COMMAND_MODE_GUARD_1,
		AWAIT_COMMAND_MODE_RESPONSE,
		BEGIN_API_MODE_COMMAND,
		AWAIT_API_MODE_RESPONSE,
		BEGIN_COMMAND_MODE_EXIT,
		AWAIT_COMMAND_MODE_EXIT_RESPONSE,
		BEGIN_INIT_XBEE_API,
		AWAIT_INIT_XBEE_API_DONE,
		BEGIN_READ_APN,
		AWAIT_READ_APN_RESPONSE,
		SET_APN,
		SOCKET_COOLDOWN_PERIOD,
		BEGIN_CREATE_SOCKET,
		AWAIT_SOCKET_ID,
		BEGIN_SET_TLS_PROFILE,
		AWAIT_TLS_PROFILE_RESPONSE,
		SOCKET_READY,
		AWAIT_CONNECT_RESPONSE,
		AWAIT_CONNECTION,
		CONNECTED,
		SENDING,
		MAX_STATE	
	};

	GB4XBee(
		uint32_t baud,
		char const apn[],
		bool use_tls = false,
		uint8_t use_tls_profile = 0);
	
	bool begin();
	State resetSocket();
	Return pollStartup();
	State poll();
	bool connect(uint16_t port, char const *address);
	Return pollConnectStatus();
	void startConnectRetryDelay();
	bool pollConnectRetryDelay();
	Return getReceivedMessage(uint8_t message[], size_t *message_len);
	Return sendMessage(uint8_t message[], size_t message_len);

	bool verifyAccessPointName(uint8_t const *value, size_t const len);
	uint32_t getBaud();
	char const *getAPN();
	uint64_t getSerialNumber();
	State state();
	uint32_t const cast_guard;

	private:
	void startCommandModeGuard();
	bool pollCommandModeGuard();
	void sendEscapeSequence();
	void sendAPIMode();
	void sendCommandModeExit();
	Return pollResponseOK();
	void startInitAPI();
	void restartInitAPI();
	Return pollInitStatus();
	bool sendReadAPN();
	Return pollAPNStatus();
	bool sendSetAPN();
	bool sendWriteChanges();
	bool sendSocketCreate();
	bool pollSocketCooldown();
	Return pollSocketStatus();
	bool sendSocketOption();
	Return pollSocketOptionResponse();


	State m_state;
	int32_t err;
	int32_t guard_time_start;
	int32_t response_start_time;
	int32_t connect_start_time;
	int32_t connect_retry_delay_start_time;
	int32_t socket_create_start_time;
	int32_t socket_cooldown_start_time;
	int32_t option_start_time;
	int32_t send_start_time;
	char access_point_name[GB4XBEE_ACCESS_POINT_NAME_SIZE];
	size_t access_point_name_len;
	bool got_access_point_name;
	bool need_set_access_point_name;
	xbee_dev_t xbee;
	xbee_serial_t ser;	
	xbee_sock_t sock;
	uint8_t transport_protocol;
	uint8_t tls_profile;
	uint32_t notify_count = 0;
	bool connect_in_progress = false;
};

#endif //GB4XBEE_H
