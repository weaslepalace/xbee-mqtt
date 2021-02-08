/**
 * test.cpp
 * Unit test for StaticQueue class
 */

#include "static_queue.h"
#include <cstdint>
#include <iostream>
#include <cstdlib>
#include <cmath>

static size_t constexpr QUEUE_SIZE = 10;
static uint32_t constexpr TEST_VALUES[QUEUE_SIZE] = { 
	1000, 1001, 1002, 1003, 1004, 1005, 1006, 1007, 1008, 1009
};

class TestStaticQueue {

	public:
	TestStaticQueue() {}

	/**
	 * Insert a single object into the queue.
	 * Verify that it exists in the queue.
	 */
	bool singleObjectInsert()
	{
		m_queue.reset();
		m_name.assign("singleObjectInsert");
		m_queue.insert(TEST_VALUES[0]);
		uint32_t *res = m_queue.peak();
		return 
			(TEST_VALUES[0] == *res) &&
			(1 == m_queue.length()) &&
			(false == m_queue.isFull()) &&
			(false == m_queue.isEmpty()); 
	}

	/**
	 * Insert a single object into the m_queue, then remove it
	 * Verify that the object was removed
	 */
	bool singleObjectRemove()
	{
		m_queue.reset();
		m_name.assign("singleObjectRemove");
		m_queue.insert(TEST_VALUES[0]);
		uint32_t *res = m_queue.dequeue();
		return
			(TEST_VALUES[0] == *res) &&
			(0 == m_queue.length()) &&
			(true == m_queue.isEmpty()) &&
			(false == m_queue.isFull());
	}
	

	/**
	 * Dequeue and empty list
	 * Verify that the list remains empty and nothing crashes
	 */
	bool emptyListRemove()
	{
		m_queue.reset();
		m_name.assign("emptyListRemove");
		uint32_t *res = m_queue.dequeue();
		return 
			(nullptr == res) &&
			(0 == m_queue.length()) &&
			(true == m_queue.isEmpty()) &&
			(false == m_queue.isFull());
	}

	
	/**
	 * Completely fill the list
	 * Verify that the list is full, and all the values are valid
	 */
	bool fillList()
	{
		m_queue.reset();
		m_name.assign("fillList");
		for(size_t i = 0; i < QUEUE_SIZE; i++)
		{
			if(false == m_queue.insert(TEST_VALUES[i]))
			{
				return false;
			}
		}
		for(size_t i = 0; i < QUEUE_SIZE; i++)
		{
			if(TEST_VALUES[i] != *m_queue.getElem(i))
			{
				return false;
			}
		}

		return
			(QUEUE_SIZE == m_queue.length()) &&
			(false == m_queue.isEmpty()) &&
			(true == m_queue.isFull());
	}

	
	/**
	 * Completely fill the list, then insert one more object into the list
	 * Verify that the insert fails, and all the values remain valid
	 */
	bool fullListInsert()
	{
		fillList();
		m_name.assign("fullListInsert");
		if(false != m_queue.insert(1010))
		{
			return false;
		}
		for(size_t i = 0; i < QUEUE_SIZE; i++)
		{
			if(TEST_VALUES[i] != *m_queue.getElem(i))
			{
				return false;
			}
		}

		return
			(QUEUE_SIZE == m_queue.length()) &&
			(false == m_queue.isEmpty()) &&
			(true == m_queue.isFull());
	}

	/**
	 * Completely fill the list, then traverse the list using next() and prev() methods
	 * Verify that all objects are accessable and valid via traversal
	 */
	bool fullListTraversal()
	{
		fillList();
		m_name.assign("fullListTraversal");
		
		if(false == mTraversal(QUEUE_SIZE, TEST_VALUES))
		{
			return false;
		}

		return
			(QUEUE_SIZE == m_queue.length()) &&
			(false == m_queue.isEmpty()) &&
			(true == m_queue.isFull());
	}

	/**
 	 * Remove odd numbered objects from a full list
 	 * Verify that all objects are accessable and valid via traversal
 	 */
	bool oddRemovalTraversal()
	{
		fillList();
		m_name.assign("oddRemovalTraversal");
		for(size_t i = 1; i < QUEUE_SIZE; i += 2)
		{
			m_queue.remove(m_queue.getNode(i));
		}
		
		uint32_t evens[QUEUE_SIZE / 2];
		for(size_t i = 0; i < QUEUE_SIZE; i += 2)
		{
			evens[i / 2] = TEST_VALUES[i];
		}

		if(false == mTraversal(QUEUE_SIZE / 2, evens))
		{
			return false;
		}
		
		return
			((QUEUE_SIZE / 2) == m_queue.length()) &&
			(false == m_queue.isEmpty()) &&
			(false == m_queue.isFull());
	}

	/**
	 * Remove even numbered object from a full list
	 * Verify that all objects are accessable via traversal
	 */
	bool evenRemovalTraversal()
	{
		fillList();
		m_name.assign("evenRemovalTraversal");
		for(size_t i = 0; i < QUEUE_SIZE; i += 2)
		{
			m_queue.remove(m_queue.getNode(i));
		}
		
		uint32_t odds[QUEUE_SIZE / 2];
		for(size_t i = 1; i < QUEUE_SIZE; i += 2)
		{
			odds[i / 2] = TEST_VALUES[i];
		}

		if(false == mTraversal(QUEUE_SIZE / 2, odds))
		{
			return false;
		}
		
		return
			((QUEUE_SIZE / 2) == m_queue.length()) &&
			(false == m_queue.isEmpty()) &&
			(false == m_queue.isFull());
	}

	/**
	 * Remove the head and tail from a full list
	 * Verify that all objects are accessable via traversal
	 */
	bool endsRemovalTraversal()
	{
		fillList();
		m_name.assign("endsRemovalTraversal");
		m_queue.remove(m_queue.peakNode());
		m_queue.remove(m_queue.getNode(QUEUE_SIZE - 1));
	
		if(false == mTraversal(QUEUE_SIZE - 2, &TEST_VALUES[1]))
		{
			return false;
		}

		return
			((QUEUE_SIZE - 2) == m_queue.length()) &&
			(false == m_queue.isEmpty()) &&
			(false == m_queue.isFull());
	}


	/**
 	 *	Remove even object from a full list, and insert more objects
	 * Verify that all objects are accessable via traversal
 	 */
	bool insertAfterRemoveTraversal()
	{
		fillList();
		evenRemovalTraversal();
		m_name.assign("insertAfterRemoveTraversal");
	
		uint32_t new_values[] = {1010, 1011, 1012, 1013, 1014};
		for(size_t i = 0; i < sizeof new_values; i++)
		{
			m_queue.insert(new_values[i]);
		}	
		
		uint32_t check_values[(QUEUE_SIZE / 2) + 6];
		for(size_t i = 0; i < QUEUE_SIZE / 2; i++)
		{
			check_values[i] = TEST_VALUES[1 + (i * 2)];
		}
		memcpy(&check_values[QUEUE_SIZE / 2], new_values, 5 * sizeof new_values[0]);

		if(false == mTraversal(5 + (QUEUE_SIZE / 2), check_values))
		{
			return false;
		}

		return 
			(QUEUE_SIZE == m_queue.length()) &&
			(false == m_queue.isEmpty()) &&
			(true == m_queue.isFull());
	}

	/**
	 * Print the result of the most recent test
	 */
	std::string printResult()
	{
		std::string result = m_name;
		result += "\nResult:\n";
		result += "\tm_array = { ";
		for(size_t i = 0; i < QUEUE_SIZE; i++)
		{
			char buf[10];
			sprintf(buf, "%d, ", *m_queue.getElem(i)); 
			result += buf;
		}
		result += "}\n";
		result += "\tisFull = ";
		result += (true == m_queue.isFull()) ? "true\n" : "false\n";
		result += "\tisEmpty = ";
		result += (true == m_queue.isEmpty()) ? "true\n" : "false\n";
		result += "\tlength = ";
		char buf[10];
		sprintf(buf, "%lu\n", m_queue.length());
		result += buf;
		return result;
	}

	private:

	bool mTraversal(size_t expected_cnt, uint32_t const values[])
	{
		size_t cnt = 0;
		LinkedNode<uint32_t> *end;
		for(
			LinkedNode<uint32_t> *node = m_queue.peakNode();
			nullptr != node;
			node = node->next(), cnt++)
		{
			if(values[cnt] != node->value())
			{
				return false;
			}
			end = node;
		}
		if((expected_cnt != cnt) || (nullptr == end) || (nullptr != end->next()))
		{
			return false;
		}
		cnt--;
		for(
			LinkedNode<uint32_t> *node = end;
			nullptr != node;
			node = node->prev(), cnt--)
		{
			if(values[cnt] != node->value())
			{
				return false;
			}
		}

		return (static_cast<size_t>(-1) == cnt);
	}

	StaticQueue<uint32_t, QUEUE_SIZE> m_queue;
	std::string m_name;
};


class TestType {
	public:
	TestType() {}
	TestType(uint32_t id)
	{
		init(id);
	};

	void init(uint32_t id)
	{
		m_id = id;
		m_sin = std::sin(m_id * M_PI / 180);
		m_cos = std::cos(m_id * M_PI / 180);
	}

	uint32_t id()
	{
		return m_id;
	}
	
	double sin()
	{
		return m_sin;
	}

	double cos()
	{
		return m_cos;
	}

	private:
	uint32_t m_id;
	double m_sin;
	double m_cos;
};


template <typename T, size_t N_MEMB>
class TestComplexStaticQueue : public TestStaticQueue {
	public:
	TestComplexStaticQueue ()
	{
		for(size_t i = 0; i < N_MEMB; i++)
		{
			test_values[i].init(1000 + i);
			m_queue.insert(test_values[i]);
		}
	}

	
	bool insertAfterRemovalTraversal()
	{
		m_name.assign("complex_insertAfterRemovalTraversal");

		//Remove node at even indicies
		for(size_t i = 0; i < (N_MEMB / 2); i++)
		{
			m_queue.remove(m_queue.getNode(2 * i));
		}

		T new_values[N_MEMB / 2];
		for(size_t i = 0; i < (N_MEMB / 2); i++)
		{
			new_values[i].init(1000 + N_MEMB + i);
			m_queue.insert(new_values[i]);
		}
		
		T check_values[N_MEMB];
		for(size_t i = 0; i < (N_MEMB / 2); i++)
		{
			check_values[i].init(test_values[1 + (2 * i)].id());
		}
		memcpy(&check_values[N_MEMB / 2], new_values, (N_MEMB / 2) * sizeof new_values[0]);

		size_t cnt = 0;
		LinkedNode<T> *end;
		for(
			LinkedNode<T> *node = m_queue.peakNode();
			nullptr != node;
			node = node->next(), cnt++)
		{
			if(check_values[cnt].id() != node->value().id())
			{
				std::cout << cnt << std::endl;
				return false;
			}
			end = node;
		}

		if((N_MEMB != cnt) || (nullptr == end) || (nullptr != end->next()))
		{
			return false;
		}
		cnt--;

		for(
			LinkedNode<T> *node = end;
			nullptr != node;
			node = node->prev(), cnt--)
		{
			if(check_values[cnt].id() != node->value().id())
			{
				return false;
			}
		}

		return
			(static_cast<size_t>(-1) == cnt) &&
			(N_MEMB == m_queue.length()) &&
			(false == m_queue.isEmpty()) &&
			(true == m_queue.isFull());
	}
	
	/**
	 * Print the result of the most recent test
	 */
	std::string printResult()
	{
		std::string result = m_name;
		result += "\nResult:\n";
		result += "\tm_array = { ";
		for(size_t i = 0; i < N_MEMB; i++)
		{
			char buf[10];
			sprintf(buf, "%d, ", m_queue.getElem(i)->id()); 
			result += buf;
		}
		result += "}\n";
		result += "\tisFull = ";
		result += (true == m_queue.isFull()) ? "true\n" : "false\n";
		result += "\tisEmpty = ";
		result += (true == m_queue.isEmpty()) ? "true\n" : "false\n";
		result += "\tlength = ";
		char buf[10];
		sprintf(buf, "%lu\n", m_queue.length());
		result += buf;
		return result;
	}

	private:
	T test_values[N_MEMB];
	StaticQueue<T, N_MEMB> m_queue;
	std::string m_name;
};


int main()
{
	TestStaticQueue test;

	if(false == test.singleObjectInsert())
	{
		std::cout << test.printResult();
		return -1;
	}
	
	if(false == test.singleObjectRemove())
	{
		std::cout << test.printResult();
		return -1;
	}

	if(false == test.emptyListRemove())
	{
		std::cout << test.printResult();
		return -1;
	}	

	if(false == test.fillList())
	{
		std::cout << test.printResult();
		return -1;
	}
	
	if(false == test.fullListInsert())
	{
		std::cout << test.printResult();
		return -1;
	}
	
	if(false == test.fullListTraversal())
	{
		std::cout << test.printResult();
		return -1;
	}

	if(false == test.evenRemovalTraversal())
	{
		std::cout << test.printResult();
		return -1;
	}

	if(false == test.oddRemovalTraversal())
	{
		std::cout << test.printResult();
		return -1;
	}

	if(false == test.endsRemovalTraversal())
	{
		std::cout << test.printResult();
		return -1;
	}
		
	if(false == test.insertAfterRemoveTraversal())
	{
		std::cout << test.printResult();
		return -1;
	}

	TestComplexStaticQueue<TestType, 76564> complex_test;
	if(false == complex_test.insertAfterRemovalTraversal())
	{
		std::cout << complex_test.printResult();
		return -1;
	}
}
