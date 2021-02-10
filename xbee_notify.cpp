/**
 * xbee_notify.cpp
 */

#include "xbee_notify.h"

XBeeNotify g_notify;
XBeeReceive g_receive;

XBeeNotify::XBeeNotify()
{
	m_count = 0;
	type = FrameType::NONE;
	tx_mesg = TxMesg::SUCCESS;
	sock_mesg = SockMesg::SUCCESS;
	state_mesg = StateMesg::CONNECTED;
}


void XBeeNotify::callback(
		xbee_sock_t sockid,
		uint8_t frame_type,
		uint8_t message)
{
	g_notify.socket = sockid;
	g_notify.type = static_cast<FrameType>(frame_type);
	switch(g_notify.type)
	{
		case FrameType::TX_STATUS:
		g_notify.tx_mesg = static_cast<TxMesg>(message);
		break;

		case FrameType::SOCK_CREATE_RESP:
		case FrameType::SOCK_CONNECT_RESP:
		g_notify.sock_mesg = static_cast<SockMesg>(message);
		break;

		case FrameType::SOCK_STATE:
		g_notify.state_mesg = static_cast<StateMesg>(message);
		break;

		case FrameType::SOCK_CLOSE_RESP:
		case FrameType::SOCK_LISTEN_RESP:
		case FrameType::SOCK_RECEIVE:
		case FrameType::SOCK_RECEIVE_FROM:
		case FrameType::SOCK_OPTION_RESP:
		default:
		return;
	}
	g_notify.countUp();
}


XBeeReceive::XBeeReceive()
{
	m_dropped_count = 0;
	m_count = 0;
	m_pending = false;
	memset(m_payload, 0, MESSAGE_PAYLOAD_SIZE);
}


size_t XBeeReceive::read(uint8_t *buffer, size_t len)
{
	if(len > m_payload_len)
	{
		len = m_payload_len;
	}
	memcpy(buffer, m_payload, len);
	m_pending = false;
	return len;
}

void XBeeReceive::write(uint8_t const *buffer, size_t len)
{
	if(true == m_pending)
	{
		m_dropped_count++;		
	}

	m_payload_len = (len > MESSAGE_PAYLOAD_SIZE) ? MESSAGE_PAYLOAD_SIZE : len;
	memcpy(m_payload, buffer, m_payload_len);

	m_count++;
	m_pending = true;	
}

void XBeeReceive::callback(
	xbee_sock_t sock,
	uint8_t status,
	void const *payload,
	size_t payload_length)
{
	g_receive.socket = sock;
	g_receive.write(static_cast<uint8_t const *>(payload), payload_length);
}
	  
