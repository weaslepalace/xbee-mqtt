/**
 *	linked_node.h
 */

#ifndef LINKED_NODE_H
#define LINKED_NODE_H

#include <cstddef>
#include <cstdint>
#include <cstring>

template <typename T>
class LinkedNode {
	public:
	LinkedNode()
	{
		m_next = nullptr;
		m_prev = nullptr;
	}
	LinkedNode(T const &value)
	{
		m_value = value;
		m_next = nullptr;
		m_prev = nullptr;
	}

	/**
	 * Create a bi-directional link between the calling object and the next
	 * object in the list
	 */
	bool link(LinkedNode<T> *next)
	{
		m_linked = true;
		if(this == next)
		{
			return false;
		}
		next->linkPrev(this);
		m_next = next;
		return true;
	}

	void unlink()
	{
		if(nullptr != m_next)
		{
			m_next->linkPrev(m_prev);
		}
		if(nullptr != m_prev)
		{
			m_prev->linkNext(m_next);
		}
		m_prev = nullptr;
		m_next = nullptr;
		m_linked = false;
	}

	LinkedNode<T> *prev()
	{
		return m_prev;
	}

	LinkedNode<T> *next()
	{
		return m_next;
	}
	
	bool isLinked()
	{
		return m_linked;
	}

	T &value()
	{
		return m_value;
	}

	private:
	bool linkPrev(LinkedNode<T> *prev)
	{
		m_linked = true;
		if(this == prev)
		{
			return false;
		}
		m_prev = prev;
		return true;
	}

	bool linkNext(LinkedNode<T> *next)
	{
		m_linked = true;
		if(this == next)
		{
			return false;
		}
		m_next = next;
		return true;
	}
	
	bool m_linked = false;
	LinkedNode<T> *m_next = nullptr;
	LinkedNode<T> *m_prev = nullptr;
	T m_value;
};

#endif //LINKED_NODE_H
