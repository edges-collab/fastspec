#ifndef _BYTEBUFFER_H_
#define _BYTEBUFFER_H_


#include <pthread.h>
#include <list>

using namespace std;

// A circular buffer based on an iterator architecture and implemented
// using Standard Template Library <list> class.  Each item in the
// buffer is a pointer to a block of data.  Multiple access is 
// supported, with the tail of the buffer advancing only when all
// open iterators have moved beyond an item.
class ByteBuffer {

	public:

	// Nested class for the items used in the internal <list>.   
	// These items have a member variable to track the number of 
	// iterators pointing to each item.
	class item {

		public:

			// Constructor
			item(void* p = NULL, unsigned int u = 0) 
				: pData(p), uHolds(u) {}

			// Copy Constructor
			item(const ByteBuffer::item& item2) 
				: pData(item2.pData), uHolds(item2.uHolds) {}		

			// Destructor
			~item() {}

			// Member variables
			void*         		  pData;
			unsigned int 				uHolds;
	};

	// Nested class for the external iterator exposed by the buffer.
	// The iterator is operated on by passing it in calls to the main 
	// buffer.
	class iterator {

		public:

			// Constructor 
			iterator() : pBuffer(NULL) {}

			// Copy constructor
			//iterator(const Buffer::iterator& iterator2) 
			//	: it(iterator2.it), pBuffer(iterator2.pBuffer) {}

			// Destructor
			~iterator() {}

		private: 

			// Member variables
			list<ByteBuffer::item>::iterator 		it;
			ByteBuffer*											    pBuffer;

		friend ByteBuffer;
	};

		// Constructor
	  ByteBuffer();

	  // Destructor
	  ~ByteBuffer();


	  // Allocate the buffer items that will be used.  This defines the size of 
	  // the buffer and must be called before the buffer can be used.
	  void allocate(unsigned int, unsigned int);

	  // Get the iterator of the next available item as long as there at least
	  // i[unsigned int] items available ahead of the current position.
		bool request(ByteBuffer::iterator&, unsigned int);

		// Returns false if there are fewer than uNumAvailable items available in 
		// the list starting at iterator's current position
		bool available(ByteBuffer::iterator&, unsigned int);

	  // Safely increment the iterator to the next item.  Does not advance the
	  // position recorder or mark a hold on the item.  Returns false if no item 
	  // available.
	  bool next(ByteBuffer::iterator&);

	  // Release the iterator (each iterator acquired with 'request' must be released)
	  void release(ByteBuffer::iterator&);

	  // Returns pointer to the buffer block in the current item in the iterator
	  void* data(ByteBuffer::iterator&);

	  // Push data into the buffer.  Returns false if buffer is full.
	  bool push(void*, unsigned int);  

	  // Returns number of holds on items currently in buffer
	  unsigned int holds();

	  // Returns number of items currently in buffer
	  unsigned int size();
	  
	  // Returns true if buffer is empty
	  bool empty();

	  // Empties the buffer
	  void clear();
	
	private:

	  // Advances the tail of the circular buffer if possible
	  void cleanup();

	  // Member variables
		list<ByteBuffer::item>							m_empty;
		list<ByteBuffer::item>							m_full;
		list<ByteBuffer::item>::iterator		m_pos;
		unsigned int 										    m_uItemLength;
		unsigned int										    m_uNumItems;
		unsigned int 										    m_uHolds;
		unsigned int 										    m_uMaxFullSize;
		pthread_mutex_t        					    m_mutex;

};




#endif // _BYTERBUFFER_H_
