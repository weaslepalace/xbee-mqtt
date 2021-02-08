/**
 * linked_queue.h
 */

#ifndef LINKED_QUEUE_H
#define LINKED_QUEUE_H

#include "linked_node.h"

template <typename T>
class LinkedQueue {
	public:
	LinkedQueue()
	{
	}

	LinkedQueue(LinkedNode<T> *head_init, LinkedNode<T> *tail_init)
	{
		init(head_init, tail_init);
	}

	void init(LinkedNode<T> *head_init, LinkedNode<T> *tail_init)
	{
		m_head = head_init;
		m_tail = tail_init;
		m_full = false;
	}

	bool enqueueNode(LinkedNode<T> *elem)
	{
		if(true == m_full)
		{
			return false;
		}
		if(nullptr == m_tail)
		{
			m_tail = elem;
			
		}
		m_head->link(elem);
		m_head = elem;
		
		return true;
	}


	LinkedNode<T> *peakNode()
	{
		if(true == isEmpty())
		{
			return nullptr;
		}
		return m_tail;
	}


	T *peak()
	{
		LinkedNode<T> *elem = peakNode();
		if(nullptr == elem)
		{
			return nullptr;
		}
		return &elem->value();
	}


	LinkedNode<T> *dequeueNode()
	{
		if(true == isEmpty())
		{
			return nullptr;
		}
		LinkedNode<T> *elem = m_tail;
		m_tail = m_tail->next();
		elem->unlink();
		m_full = false;
		return elem;
	}


	T *dequeue()
	{
		LinkedNode<T> *elem = dequeueNode();
		if(nullptr == elem)
		{
			return nullptr;
		}
		return &elem->value();
	}


	void remove(LinkedNode<T> *elem)
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
	
	void setFull(bool full)
	{
		m_full = full;
	}


	size_t length()
	{
		size_t len = 0;
		for(LinkedNode<T> *elem = m_tail; nullptr != elem; elem = elem->next())
		{
			len++;
		}
		return len;
	}


	private:
	LinkedNode<T> *m_head = nullptr;
	LinkedNode<T> *m_tail = nullptr;
	bool m_full = false;
};

#endif //LINKED_QUEUE_H
