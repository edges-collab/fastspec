#ifndef _BUFFER_H_
#define _BUFFER_H_


#include <pthread.h>
#include <list>

using namespace std;

#ifndef SAMPLE_DATA_TYPE
  #error Aborted in buffer.h because SAMPLE_DATA_TYPE was not defined.
#endif

#ifndef BUFFER_DATA_TYPE
  #error Aborted in buffer.h because BUFFER_DATA_TYPE was not defined.
#endif

// A circular buffer based on an iterator architecture and implemented
// using Standard Template Library <list> class.  Each item in the
// buffer is a pointer to a block of data.  Multiple access is 
// supported, with the tail of the buffer advancing only when all
// open iterators have moved beyond an item.
class Buffer {

	public:

  // Enumerator for pushing data of different types into buffer (avoiding templates)
  typedef enum pushType {
    uint16 = 0,
    int16 = 1
  } pushType;
  
	// Nested class for the items used in the internal <list>.   
	// These items have a member variable to track the number of 
	// iterators pointing to each item.
	class item {

		public:

			// Constructor
			item(BUFFER_DATA_TYPE* p = NULL, unsigned int u = 0) 
				: pData(p), uHolds(u) {}

			// Copy Constructor
			item(const Buffer::item& item2) 
				: pData(item2.pData), uHolds(item2.uHolds) {}		

			// Destructor
			~item() {}

			// Member variables
			BUFFER_DATA_TYPE*		  pData;
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
			list<Buffer::item>::iterator 		it;
			Buffer*													pBuffer;

		friend Buffer;
	};

		// Constructor
	  Buffer();

	  // Destructor
	  ~Buffer();


	  // Allocate the buffer items that will be used.  This defines the size of 
	  // the buffer and must be called before the before can be used.
	  void allocate(unsigned int, unsigned int);

	  // Get the iterator of the next available item
		bool request(Buffer::iterator&, unsigned int);

		// Returns false if there are fewer than uNumAvailable items available in 
		// the list starting at iterator's current position
		bool available(Buffer::iterator&, unsigned int);

	  // Safely increment the iterator to the next item. 
	  // Returns false if no item available.
	  bool next(Buffer::iterator&);

	  // Release the iterator (each iterator acquired with 'get' must be released)
	  void release(Buffer::iterator&);

	  // Returns pointer to the buffer block in the current item in the iterator
	  BUFFER_DATA_TYPE* data(Buffer::iterator&);

	  // Push data into the buffer.  Returns false if buffer is full.
	  bool push(BUFFER_DATA_TYPE*, unsigned int);

	  // Push data into the buffer.  Returns false if buffer is full.
	  bool push(SAMPLE_DATA_TYPE*, unsigned int, double, double);
	  

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
		list<Buffer::item>							m_empty;
		list<Buffer::item>							m_full;
		list<Buffer::item>::iterator		m_pos;
		unsigned int 										m_uItemLength;
		unsigned int										m_uNumItems;
		unsigned int 										m_uHolds;
		unsigned int 										m_uMaxFullSize;
		pthread_mutex_t        					m_mutex;

};




#endif // _BUFFER_H_
