/**
 * linked_queue.h
 */

#ifndef LINKED_NODE_H
#define LINKED_NODE_H

#include "linked_node.h"
#include <cstddef>

template <typename T>
class LinkedQueue {
	public:
	LinkedQueue();


	bool enqueue(T *elem)
	{
		if(true == m_full)
		{
			return false;
		}
		elem->linkPrev(m_head);
		m_head->linkNext(elem);
		m_head = m_head->next();
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


	private:
	T *m_head = nullptr;
	T *m_tail = nullptr;
	bool m_full = false;
};

#endif //LINKED_NODE_H
