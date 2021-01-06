#ifndef GB4XBEE_H
#define GB4XBEE_H

#include "xbee/platform.h"
#include "xbee/socket.h"
#include "Arduino.h"

static int32_t constexpr GB4XBEE_COMMAND_MODE_GUARD_TIME = 1200;
static size_t constexpr GB4XBEE_ACCESS_POINT_NAME_SIZE = 32;
static size_t constexpr GB4XBEE_RECEIVED_MESSAGE_MAX_SIZE = 1500;
static uint32_t constexpr GB4XBEE_DEFAULT_BAUD = 9600;
static uint32_t constexpr GB4XBEE_DEFAULT_COMMAND_TIMEOUT = 10000;

class GB4XBee {
	public:

	enum class Return {
		INIT_TRY_AGAIN = -5,
		APN_READ_ERROR = -4, 
		SOCKET_ERROR = -3,
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
		CONNECT_IN_PROGRESS,
		CONNECTED,
		DISCONNECTED,
		WAITING_MESSAGE,
		MESSAGE_RECEIVED,
		MESSAGE_SENT,
		STARTUP_IN_PROGRESS,
		STARTUP_COMPLETE
	};

	GB4XBee(uint32_t baud, char const apn[]);
	
	bool begin();
	Return pollStartup();
	bool connect(uint16_t port, char const *address);
	Return pollConnectStatus();
	Return pollReceivedMessage(uint8_t message[], size_t *message_len);
	Return sendMessage(uint8_t message[], size_t message_len);

	bool verifyAccessPointName(uint8_t const *value, size_t const len);
	uint32_t getBaud();
	char const *getAPN();

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
	Return pollSocketStatus();

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
		BEGIN_CREATE_SOCKET,
		AWAIT_SOCKET_ID,
		READY,	
	};

	State state;
	int32_t err;
	int32_t guard_time_start;
	int32_t response_start_time;
	char access_point_name[GB4XBEE_ACCESS_POINT_NAME_SIZE];
	size_t access_point_name_len;
	bool got_access_point_name;
	bool need_set_access_point_name;
	xbee_dev_t xbee;
	xbee_serial_t ser;	
	xbee_sock_t sock;
};

#endif //GB4XBEE_H
