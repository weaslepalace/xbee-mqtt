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


/**
 *	Callback executed when a socket changes state. It is called by
 *	xbee_dev_tick() which is called by GB4XBee::poll(). When a socket's state
 *	changes flags are updated to direct the flow of the state machine.
 *	@param sockid - The ID of the socket whos state has changed
 *	@param frame_type - The API frame used to notify of the state change.
 *	                    Different frame types allow the message byte to be
 *	                    overloaded
 *	@param message - Single byte message (more more like a status code)
 *	                 porviding infomation on how the state changed
 *	                 0 represents a positive outcome (ie. connection success)
 *	                 Non-zero represents a negitive outcome (ie. socket error)
 */
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


/**
 *	Read received data that was buffered in the a call to
 *	XBeeReceive::callback()
 *	@param buffer - Output - Container to read buffered data into
 *	@param len - Total size of buffer to prevent overrun
 *	@return Lentgh of buffered data in bytes
 */
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


/**
 *	Helper function for XBeeReceive::callback()
 *	Copies received data into a buffer, and sets m_pendign flags to notify the
 *	state machine
 *	@param buffer - Input - Buffer containing the received data
 *	@param len - Length of buffer in bytes
 */
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


/**
 *	Callback function called when data has been received from a connected socket.
 *	Copies the data into a buffer and sets a flag to tell the state machine of
 *	the pending data.
 *  Called from within xbee_dev_tick() which is called by GB4XBee::poll()
 *	Note: This data is not queued, and will be overwritten if not read before 
 *	      subsequent calls to the callback, thus m_pending must be checked for
 *	      every call to xbee_dev_tick() if incomming data is expected
 *	@param sock - The socket the data was received on
 *	@param status - Seems to always be zero. I have no idea what this is.
 *	                Must be used internally by the xbee driver
 *	@param payload - Input - Buffer containing the received data
 *	@param payload_length - The length of buffer in bytes
 */
void XBeeReceive::callback(
	xbee_sock_t sock,
	uint8_t status,
	void const *payload,
	size_t payload_length)
{
	g_receive.socket = sock;
	g_receive.write(static_cast<uint8_t const *>(payload), payload_length);
}
	  
