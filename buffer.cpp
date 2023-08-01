#include <stdlib.h>
#include <stdio.h>
#include "buffer.h"


// ----------------------------------------------------------------------------
// Constructor
// ----------------------------------------------------------------------------
Buffer::Buffer() { 

  m_pos = m_full.begin();
  m_uHolds = 0;
  m_uMaxFullSize = 0;
	pthread_mutex_init(&m_mutex, NULL);

}


// ----------------------------------------------------------------------------
// Denstructor
// ----------------------------------------------------------------------------
Buffer::~Buffer() { 

  printf("Buffer: Maximum number of buffers used: %d of %d blocks (%.3g%%)\n", 
    m_uMaxFullSize, m_uNumItems, 100.0*m_uMaxFullSize/m_uNumItems);
  
	// Free buffer items
	list<Buffer::item>::iterator it;

	for (it = m_empty.begin(); it != m_empty.end(); it++) {
		free(it->pData);
	}

	for (it = m_full.begin(); it != m_full.end(); it++) {
		free(it->pData);
	}

	pthread_mutex_destroy(&m_mutex);
}


// ----------------------------------------------------------------------------
// allocate
// ----------------------------------------------------------------------------
// Create the buffers that will be used
void Buffer::allocate(unsigned int uNumItems, unsigned int uItemLength) { 
	
	m_uNumItems = uNumItems;
	m_uItemLength = uItemLength;

	Buffer::item item;

	// Allocate the buffer items
	for (unsigned int i=0; i<uNumItems; i++) {
		item.pData = (BUFFER_DATA_TYPE*) malloc(uItemLength*sizeof(BUFFER_DATA_TYPE));
		item.uHolds = 0;

		if (item.pData !=NULL) {
			m_empty.push_front(item);
		} else {
			printf("Buffer::Allocate -- Failed to allocate buffer item %d\n", i);
		}
	}	

  // Exercise the buffer to make sure delays from first time use don't occur
  // during operation
  SAMPLE_DATA_TYPE* pTemp = (SAMPLE_DATA_TYPE*) malloc(m_uItemLength*sizeof(SAMPLE_DATA_TYPE));
  if (pTemp) {
    push(pTemp, m_uItemLength, 1.0, 0.0); 
    free(pTemp); 
  }

  Buffer::iterator iter;
  request(iter, 1);
  release(iter);
  clear(); 
}


// ----------------------------------------------------------------------------
// request
// ----------------------------------------------------------------------------
// Get the iterator of the next available item.  Fails and returns false if 
// fewer than uNumAvailable items remain in the queue.  Adds a hold to the next
// available item, but not to any beyond that (even if uNumAvailable > 1).  
// Advances the buffer iterator head marker (m_pos) one step (even if 
// uNumAvailable > 1).
bool Buffer::request(Buffer::iterator& iter, unsigned int uNumAvailable) {

  bool bReturn = true;

	pthread_mutex_lock(&m_mutex);

  // Check that not at end of list
	if (m_pos == m_full.end()) {

    iter.it = m_full.end();
    iter.pBuffer = NULL;
    bReturn = false;

  } else {

    // Check that there are enough items ahead in the list
    list<Buffer::item>::iterator it = m_pos;
    unsigned int i = 1;
    while ((i++ < uNumAvailable) && bReturn) {
      if (++it == m_full.end()) {
        iter.it = m_full.end();
        iter.pBuffer = NULL;        
        bReturn = false;
      }
    }  

    // If enough items, assign the iter
    if (bReturn) {

      // Set the output to the next position
      iter.it = m_pos;
      iter.pBuffer = this;

    	// Add a hold to the item
    	(iter.it)->uHolds++;

      // Increment the buffer iterator head marker (m_pos)
      m_pos++;

      // Increment the current total holds counter
      m_uHolds++;
    } 
  }

	pthread_mutex_unlock(&m_mutex);

	return bReturn;
}



// ----------------------------------------------------------------------------
// available
// ----------------------------------------------------------------------------
// Returns false if there are fewer than uNumAvailable items available 
// including the current item in the iterator.
bool Buffer::available(Buffer::iterator& iter, unsigned int uNumAvailable) {

  bool bReturn = true;

  pthread_mutex_lock(&m_mutex);

  // Make sure is a valid iterator
  if ((iter.pBuffer != this) || (iter.it == m_full.end())) {
    bReturn = false;
  } else {

    // Check that there are enough items ahead in the list
    list<Buffer::item>::iterator it = iter.it;
    unsigned int i = 1;
    while ((i++ < uNumAvailable) && bReturn) {
      if (++it == m_full.end()) {
        bReturn = false;
      }
    }
  }

  pthread_mutex_unlock(&m_mutex);

  return bReturn;
}


// ----------------------------------------------------------------------------
// next
// ----------------------------------------------------------------------------
// Safely increment the iterator to next item, with appropriate holds.  Returns
// false if no more items in buffer.  Does not advance the buffer's iterator  
// head marker (m_pos).  Use Buffer::request to increment the iterator and 
// advance the buffer iterator head marker.
bool Buffer::next(Buffer::iterator& iter) {
	
  bool bReturn = true;

  pthread_mutex_lock(&m_mutex);

  // Make sure is a valid iterator
  if ((iter.pBuffer != this) || (iter.it == m_full.end())) {
    bReturn = false;
  } else {

  	// Test increment and check if end of buffer
    list<Buffer::item>::iterator it = iter.it;
	  if (++it == m_full.end()) {
      bReturn = false;
    } else {

      // Release hold on current item
      (iter.it)->uHolds--;

      // Increment and add hold on new item
      (++iter.it)->uHolds++;
	  }
  }

	pthread_mutex_unlock(&m_mutex);

  return bReturn;
}


// ----------------------------------------------------------------------------
// release
// ----------------------------------------------------------------------------
// Release the iterator and move any finished full buffers back to empty queue
void Buffer::release(Buffer::iterator& iter) {
	
  pthread_mutex_lock(&m_mutex);

  // Make sure is a valid iterator
  if ((iter.pBuffer == this) && (iter.it != m_full.end())) {

  	// Release a hold on current item
  	(*iter.it).uHolds--;

  	// Set the iterator so it can't be used any more
    iter.it = m_full.end();
  	iter.pBuffer = NULL;		

    // Decrement the current holds counter
    m_uHolds--;
	}

	pthread_mutex_unlock(&m_mutex);

	// Move any finished full buffers back to empty queue
	cleanup();
}


// ----------------------------------------------------------------------------
// block
// ----------------------------------------------------------------------------
// Get pointer to data block in iterator's current item
BUFFER_DATA_TYPE* Buffer::data(Buffer::iterator& iter) {
  
  // Make sure is a valid iterator
  if ((iter.pBuffer != this) || (iter.it == m_full.end())) {
    return NULL;  
  } else {
    return (iter.it)->pData; 
  }
}



// ----------------------------------------------------------------------------
// cleanup
// ----------------------------------------------------------------------------
// Return finished buffers to the empty queue
void Buffer::cleanup() {

	list<Buffer::item>::iterator it;
	Buffer::item item;

	pthread_mutex_lock(&m_mutex);

	while ( ((it = m_full.begin()) != m_full.end()) && (it != m_pos) && ((*it).uHolds==0) ) {
		item = m_full.front();
		m_full.pop_front();
		m_empty.push_front(item);
	} 

	pthread_mutex_unlock(&m_mutex);
}



// ----------------------------------------------------------------------------
// push
// ----------------------------------------------------------------------------
// Push a copy of the input data into the buffer
bool Buffer::push(SAMPLE_DATA_TYPE* pIn, unsigned int uLength, double dScale, 
                  double dOffset) {

  Buffer::item item;
  bool bReturn = false;

  // Make sure input data has same length as buffer item block
  if (uLength == m_uItemLength) {
    
    pthread_mutex_lock(&m_mutex);

    // Try to get an empty buffer item for use
    if (!m_empty.empty()) {
  		
    	item = m_empty.front();
    	m_empty.pop_front();

      // No need to stay locked while we copy the data (only true if assume 
      // a single thread is inserting data or we don't care about data order
      // in the buffer)
    	pthread_mutex_unlock(&m_mutex);

      // Do the casting of the scale and offset ahead of time so it doesn't
      // happen for every entry in the loop
      BUFFER_DATA_TYPE scale = (BUFFER_DATA_TYPE) dScale;
      BUFFER_DATA_TYPE offset = (BUFFER_DATA_TYPE) dOffset;
      
      // Copy the incoming data into our buffer
      for (unsigned int i=0; i<uLength; i++) {
        item.pData[i] = ((BUFFER_DATA_TYPE) pIn[i]) * scale + offset;
      }
      
      // Make sure holds are cleared
      item.uHolds = 0;

      // Need to relock to finish list management (see unlock above copy)
    	pthread_mutex_lock(&m_mutex);

      // Put the item at the end of the buffer
      m_full.push_back(item);

      // If the list was empty, update m_pos to the new beginning
      if (m_pos == m_full.end()) { 
      	m_pos = m_full.begin();
      }

      // Keep track of the maximum size of the full list
      m_uMaxFullSize = (m_uMaxFullSize < m_full.size()) ? m_full.size() : m_uMaxFullSize;

      bReturn = true;
    }

    pthread_mutex_unlock(&m_mutex);
  }

	return bReturn;
}


// ----------------------------------------------------------------------------
// size
// ----------------------------------------------------------------------------
// Return number of holds on items in the buffer
unsigned int Buffer::holds() {

  return m_uHolds;
}


// ----------------------------------------------------------------------------
// size
// ----------------------------------------------------------------------------
// Return number of items in the full list (includes all items)
unsigned int Buffer::size() {

	pthread_mutex_lock(&m_mutex);
	unsigned int uSize = m_full.size();
	pthread_mutex_unlock(&m_mutex);

	return uSize;
}

// ----------------------------------------------------------------------------
// empty
// ----------------------------------------------------------------------------
// Return true if there are no items in the full list
bool Buffer::empty() {

  pthread_mutex_lock(&m_mutex);
  bool bEmpty = (m_full.size() == 0);
  pthread_mutex_unlock(&m_mutex);

  return bEmpty;
}

// ----------------------------------------------------------------------------
// clear
// ----------------------------------------------------------------------------
// Empties the buffer by moving any items in the full list to the empty list.
// Any items in use and with holds become invalid.
void Buffer::clear() {

  list<Buffer::item>::iterator it;
  Buffer::item item;

  pthread_mutex_lock(&m_mutex);

  while (m_full.begin() != m_full.end()) {
    item = m_full.front();
    item.uHolds = 0;
    m_full.pop_front();
    m_empty.push_front(item);
  }  

  m_pos = m_full.begin();

  pthread_mutex_unlock(&m_mutex);
}


