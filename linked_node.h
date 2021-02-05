/**
 * linked_node.h
 * Building blocks of a linked list
 */

#ifndef LINKED_NODE_H
#define LINKED_NODE_H

template<typename T>
class LinkedNode {
	public:
	LinkedNode();

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
		return (nullptr != m_next) && (nullptr != m_prev);
	}

	private:
	T *m_next = nullptr;
	T *m_prev = nullptr;
};

#endif //LINKED_NODE_H

