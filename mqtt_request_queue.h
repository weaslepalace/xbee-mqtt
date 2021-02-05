/**
 * mqtt_request_queue.h
 * Container for pending MQTT requests
 */


#include <cstddef>
#include <cstdint>
#include <cstring>

class Request; //Forward-declaration

template<typename T>
class LinkedNode {
	public:
	LinkedNode()
	{
		m_next = nullptr;
		m_prev = nullptr;
	}

	bool linkPrev(T *prev)
	{
		if(this == prev)
		{
			return false;
		}
		m_prev = prev;
		return true;
	}
	
	bool linkNext(T *next)
	{
		m_linked = true;
		if(this == next)
		{
			return false;
		}
		m_next = next;
		return true;
	}

	void unlink()
	{
		if(nullptr != m_next)
		{
			m_next->linkPrev(m_prev);
			m_next = nullptr;
		}
		if(nullptr != m_prev)
		{
			m_prev->linkNext(m_next);
			m_prev = nullptr;
		}
		m_linked = false;
	}

	T *prev()
	{
		return m_prev;
	}

	T *next()
	{
		return m_next;
	}
	
	bool isLinked()
	{
		return m_linked;
	}

	private:
	bool m_linked = false;
	T *m_next = nullptr;
	T *m_prev = nullptr;
};

class Request : public LinkedNode<Request> {
	public:
	static size_t constexpr TOPIC_MAX_SIZE  = 64;
	static size_t constexpr MESSAGE_MAX_SIZE = 300;

	Request()
	{
	}

	Request(
		char const top[], size_t toplen,
		uint8_t const mes[], size_t meslen,
		uint8_t q = 0, uint8_t r = 0, uint16_t id = 0)
	{
		topic_len = (toplen < TOPIC_MAX_SIZE) ? toplen : TOPIC_MAX_SIZE;
		message_len = (meslen < MESSAGE_MAX_SIZE) ? meslen : MESSAGE_MAX_SIZE;
		qos = q;
		retain = r;
		tries = 0;
		packet_id = id;
		memcpy(topic, top, topic_len);
		memcpy(message, mes, message_len);
	}

	char topic[TOPIC_MAX_SIZE] = "";
	size_t topic_len = 0;
	uint8_t message[MESSAGE_MAX_SIZE] = "";
	size_t message_len = 0;
	uint8_t qos;
	uint8_t retain;
	uint8_t duplicate = 0;
	uint16_t packet_id;
	int32_t start_time;
	uint8_t tries = 0;
	bool got_puback = false;
};


template <typename T>
class LinkedQueue {
	public:
	LinkedQueue()
	{
	}

	LinkedQueue(T *head_init, T *tail_init)
	{
		m_head = head_init;
		m_tail = tail_init;
	}


	bool enqueue(T *elem)
	{
		if(true == m_full)
		{
			return false;
		}
		if(nullptr == m_tail)
		{
			m_tail = elem;
		}
		elem->linkPrev(m_head);
		m_head->linkNext(elem);
		m_head = elem;
		
		return true;
	}


	T *peak()
	{
		if(true == isEmpty())
		{
			return nullptr;
		}
		return m_tail;
	}


	T *dequeue()
	{
		if(true == isEmpty())
		{
			return nullptr;
		}
		T *elem = m_tail;
		m_tail = m_tail->next();
		elem->unlink();
		m_full = false;
		return elem;
	}


	void remove(T *elem)
	{
		if(m_tail == elem)
		{
			dequeue();
			return;
		}
		if(m_head == elem)
		{
			m_head = m_head->prev();
		}
		m_full = false;
		elem->unlink();
	}


	bool isEmpty()
	{
		return nullptr == m_tail;
	}


	bool isFull()
	{
		return m_full;
	}


	size_t getLength()
	{
		size_t len = 0;
		for(T *elem = m_tail; nullptr != elem; elem = elem->next())
		{
			len++;
		}
		return len;
	}


	protected:
	T *m_head = nullptr;
	T *m_tail = nullptr;
	bool m_full = false;
};

template <size_t N_MEMB>
class RequestQueue : public LinkedQueue<Request> {
	public:
	RequestQueue() : LinkedQueue(&m_array[0], nullptr)
	{
	}

	bool insert(Request &req)
	{
		Request *slot = findEmptySlot();
		if(nullptr == slot)
		{
			m_full = true;
			return false;
		}
		memcpy(slot, &req, sizeof req);
		enqueue(slot);
		return true;
	}

	bool insert(Request *req)
	{
		Request *slot = findEmptySlot();
		if(nullptr == slot)
		{
			m_full = true;
			return false;
		}
		memcpy(slot, req, sizeof (Request));
		enqueue(slot);
		return true;
	}


	Request *findByPacketId(uint8_t packet_id)
	{
		if(true == isEmpty())
		{
			return nullptr;
		}
		Request *req = m_tail;
		for(; nullptr != req; req = req->next())
		{
			if(packet_id == req->packet_id)
			{
				break;
			}
		}
		return req;
	}
	
	private:
	Request *findEmptySlot()
	{
		Request *slot = nullptr;
		size_t i = 0;
		for( ; i < sizeof m_array; i++)
		{
			if(false == m_array[i].isLinked())
			{
				slot = &m_array[i];
				break;
			}
		}
		return slot;
	}

	Request m_array[N_MEMB];
};

