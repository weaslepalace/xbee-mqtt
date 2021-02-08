/**
 * static_queue.h
 */

#ifndef STATIC_QUEUE_H
#define STATIC_QUEUE_H

#include "linked_queue.h"

template <typename T, size_t N_MEMB>
class StaticQueue : public LinkedQueue<T> {
	public:
	StaticQueue() : LinkedQueue<T>(&m_array[0], nullptr)
	{
		m_next_available_slot = &m_array[0];
		m_slot_index = 0;
	}

	bool insert(T const &elem)
	{
		if(nullptr == m_next_available_slot)
		{
			m_next_available_slot = findEmptySlot();
			if(nullptr == m_next_available_slot)
			{
				return false;
			}
		}
		LinkedNode<T> *slot = m_next_available_slot;
		LinkedNode<T> node(elem);	
		memcpy(slot, &node, sizeof node);
		this->enqueueNode(slot);
		m_next_available_slot = findEmptySlot();
		if(nullptr == m_next_available_slot)
		{
			this->setFull(true);
		}
		return true;
	}

	bool insert(T const *elem)
	{
		if(nullptr == m_next_available_slot)
		{
			m_next_available_slot = findEmptySlot();
			if(nullptr == m_next_available_slot)
			{
				return false;
			}
		}
		LinkedNode<T> *slot = m_next_available_slot;
		LinkedNode<T> node(*elem);	
		memcpy(slot, &node, sizeof node);
		this->enqueueNode(slot);
		m_next_available_slot = findEmptySlot();
		if(nullptr == m_next_available_slot)
		{
			this->setFull(true);
		}
		return true;
	}

	T *getElem(size_t idx)
	{
		return &m_array[idx].value();
	}
	
	LinkedNode<T> *getNode(size_t idx)
	{
		return &m_array[idx];
	}

	void reset()
	{
		for(size_t i = 0; i <  N_MEMB; i++)
		{
			m_array[i].unlink();
		}
		this->init(&m_array[0], nullptr);
		m_next_available_slot = &m_array[0];
		m_slot_index = 0;
	}


	private:
	LinkedNode<T> *findEmptySlot()
	{
		LinkedNode<T> *slot = nullptr;
		for(size_t i = m_slot_index + 1; i != m_slot_index; i = ((i + 1) % N_MEMB))
		{
			if(false == m_array[i].isLinked())
			{
				slot = &m_array[i];
				m_slot_index = i;
				break;
			}
		}
		return slot;
	}

	LinkedNode<T> *m_next_available_slot;
	size_t m_slot_index = 0;
	LinkedNode<T> m_array[N_MEMB];
};

#endif //STATIC_QUEUE_H
