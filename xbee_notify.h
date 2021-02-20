/**
 * xbee_notify.h
 */

#ifndef XBEE_NOTIFY_H
#define XBEE_NOTIFY_H

#include "xbee/socket.h"
#include "xbee/platform.h"

class XBeeNotify {

	public:

	enum class TxMesg {
    	SUCCESS              =  0x00,
    	MAC_ACK_FAIL         =  0x01,
    	CCA_FAIL             =  0x02,
    	STACK_NOT_READY      =  0x03,
    	PHYSICAL_ERROR       =  0x04,
    	NO_BUFFERS           =  0x18,
    	NET_ACK_FAIL         =  0x21,
    	NOT_JOINED           =  0x22,
    	INVALID_EP           =  0x2C,
    	INTERNAL_ERROR       =  0x31,
    	RESOURCE_ERROR       =  0x32,
    	NO_SECURE_SESSION    =  0x34,
    	ENCRYPTION_FAILURE   =  0x35,
    	PAYLOAD_TOO_BIG      =  0x74,
    	UNREQ_INDIRECT_MSG   =  0x75,
    	SOCKET_CREATION_FAIL =  0x76,
    	CONNECTION_DNE       =  0x77,
    	INVALID_UDP_PORT     =  0x78,
    	INVALID_TCP_PORT     =  0x79,
    	INVALID_HOST_ADDR    =  0x7A,
    	INVALID_DATA_MODE    =  0x7B,
    	INVALID_INTERFACE    =  0x7C,
    	INTERFACE_BLOCKED    =  0x7D,
    	CONNECTION_REFUSED   =  0x80,
    	CONNECTION_LOST      =  0x81,
    	NO_SERVER            =  0x82,
    	SOCKET_CLOSED        =  0x83,
    	UNKNOWN_SERVER       =  0x84,
    	UNKNOWN_ERROR        =  0x85,
    	INVALID_TLS_CONFIG   =  0x86,
    	KEY_NOT_AUTHORIZED   =  0xBB
	};

	enum class SockMesg {
		SUCCESS         = 0x00,
		INVALID_PARAM   = 0x01,
		FAILED_TO_READ  = 0x02,
		IN_PROGRESS     = 0x03,
		CONNECTED       = 0x04,
		UNKNOWN         = 0x05,
		BAD_SOCKET      = 0x20,
		OFFLINE         = 0x22,
		INTERNAL_ERR    = 0x31,
		RESOURCE_ERR    = 0x32,
		BAD_PROTOCOL    = 0x7B,
	};		

	enum class StateMesg {
		CONNECTED          = 0x00,
		DNS_FAILED         = 0x01,
		CONNECTION_REFUSED = 0x02,
		TRANSPORT_CLOSED   = 0x03,
		TIMED_OUT          = 0x04,
		INTERNAL_ERR       = 0x05,
		HOST_UNREACHABLE   = 0x06,
		CONNECTION_LOST    = 0x07,
		UNKNOWN_ERR        = 0x08,
		UNKNOWN_SERVER     = 0x09,
		RESOURCE_ERR       = 0x0A,
		LISTENER_CLOSED    = 0x0B,
	};

	enum class FrameType {
		NONE              = 0x00,
		TX_STATUS         = 0x89,
		SOCK_OPTION_RESP  = 0xC1,
		SOCK_CREATE_RESP  = 0xC0,
		SOCK_CONNECT_RESP = 0xC2,
		SOCK_CLOSE_RESP   = 0xC3,
		SOCK_LISTEN_RESP  = 0xC6,
		SOCK_RECEIVE      = 0xCD,
		SOCK_RECEIVE_FROM = 0xCE,
		SOCK_STATE        = 0xCF,
	};
	
	XBeeNotify(); 

	xbee_sock_t socket;
	FrameType type;
	TxMesg tx_mesg;
	SockMesg sock_mesg;
	StateMesg state_mesg;

	void countUp()
	{
		m_count++;
	}
	
	uint32_t count()
	{
		return m_count;
	}

	uint32_t reset()
	{
		m_count = 0;
		return m_count;	
	}

	static void callback(
		xbee_sock_t sockid,
		uint8_t frame_type,
		uint8_t message);

	private:
	uint32_t m_count = 0;
};



class XBeeReceive {
	public:
	static size_t constexpr MESSAGE_PAYLOAD_SIZE = 300; 	

	XBeeReceive();
	size_t read(uint8_t *buffer, size_t len);
	void write(uint8_t const *buffer, size_t len);
 
	xbee_sock_t socket;

	bool pending()
	{
		return m_pending;
	}
	
	uint32_t droppedCount()
	{
		return m_dropped_count;
	}

	void droppedCountUp()
	{
		m_dropped_count++;
	}

	uint32_t count()
	{
		return m_count;
	}

	static void callback(
		xbee_sock_t sock,
		uint8_t status,
		void const *payload,
		size_t payload_length);
	
	private:
	uint32_t m_count = 0;
	uint32_t m_dropped_count = 0;
	bool m_pending;
	size_t m_payload_len = 0;
	uint8_t m_payload[MESSAGE_PAYLOAD_SIZE];
};

extern XBeeNotify g_notify;
extern XBeeReceive g_receive;

#endif //XBEE_NOTIFY_H

