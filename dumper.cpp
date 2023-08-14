#include <unistd.h>
#include "dumper.h"
#include "timing.h"
#include "utility.h"




// ----------------------------------------------------------------------------
// Constructor
// ----------------------------------------------------------------------------
Dumper::Dumper( unsigned long uBytesPerAccumulation, 
                unsigned int uBytesPerTransfer, unsigned int uNumBuffers, 
                unsigned int uDataType, double dSampleRate,
                double dScale, double dOffset )
{
  m_uBytesPerAccumulation = uBytesPerAccumulation;
  m_uBytesPerTransfer = uBytesPerTransfer;
  m_uDataType = uDataType;
  m_dSampleRate = dSampleRate;
  m_dScale = dScale;
  m_dOffset = dOffset;

  m_pFile = NULL;
  m_uBytesWritten = 0;
  m_bStop = false;

  // Create buffers
  printf("\nDumper: Creating %d buffers (%g MB)...\n", uNumBuffers, 
    ((float) uNumBuffers)*m_uBytesPerTransfer/1024/1024);
  m_buffer.allocate(uNumBuffers, m_uBytesPerTransfer);

  // Spawn the thread
  printf("Dumper: Creating 1 thread...\n");
  m_uNumReady = 0;
  unsigned int uFailed = 0;
  if (pthread_create(&m_thread, NULL, threadLoop, this) != 0 ) {
    printf("Dumper: Failed to create thread.");
    uFailed++;
  } 

  // Wait for thread to report ready
  m_timer.tic();
  while (m_uNumReady < (1-uFailed)) {
    usleep(10000);
  }
  printf("Dumper: Thread ready after %.3g seconds\n", m_timer.toc() );
  m_timer.reset();
  
}



// ----------------------------------------------------------------------------
// Destructor
// ----------------------------------------------------------------------------
Dumper::~Dumper()
{
  // Join all of our threads back to us
  m_bStop = true;
  
  for (unsigned int i=0; i<m_uNumReady; i++) {
    pthread_join(m_thread, NULL);
  }
  
  // Close file if still open
  closeFile();

}



// ----------------------------------------------------------------------------
// getTimerInterval
// ----------------------------------------------------------------------------
double Dumper::getTimerInterval()
{
  return m_timer.get();
}


// ----------------------------------------------------------------------------
// openFile
// ----------------------------------------------------------------------------
bool Dumper::openFile(const std::string& sFilePath, const TimeKeeper& tk)
{
  // Clear anything left in the buffer if we didn't finish properly last time
  m_buffer.clear();
  
  // Close anything left open if we didn't finish properly last time
  closeFile();
  
  // Reset our data write counter
  m_uBytesWritten = 0;
  
  // Reset the timer
  m_timer.tic();
  
  // open file
  printf("Dumper::openFile -- Creating: %s\n", sFilePath.c_str());
  
  if ((m_pFile = fopen(sFilePath.c_str(), "w")) == NULL) {
    printf ("Error writing to data dump file.  Cannot write to: %s\n", 
      sFilePath.c_str());
    return false;
  }
  
  unsigned int uHdrBytes = 0;
  
  // Start file header with time stamp (4 bytes each)
  int i = tk.year();
  uHdrBytes += fwrite(&i, sizeof(i), 1, m_pFile) * sizeof(i);
  
  i = tk.doy();
  uHdrBytes += fwrite(&i, sizeof(i), 1, m_pFile) * sizeof(i);
  
  i = tk.hh();
  uHdrBytes += fwrite(&i, sizeof(i), 1, m_pFile) * sizeof(i);
  
  i = tk.mm();
  uHdrBytes += fwrite(&i, sizeof(i), 1, m_pFile) * sizeof(i);
  
  i = tk.ss();
  uHdrBytes += fwrite(&i, sizeof(i), 1, m_pFile) * sizeof(i);
    
  // Add sample type information (4 bytes)
  uHdrBytes += fwrite(&m_uDataType, sizeof(m_uDataType), 1, m_pFile) 
                * sizeof(m_uDataType);
    
  // Add sample normalization info (8 bytes each)
  uHdrBytes += fwrite(&m_dScale, sizeof(m_dScale), 1, m_pFile) 
                * sizeof(m_dScale);
                
  uHdrBytes += fwrite(&m_dOffset, sizeof(m_dOffset), 1, m_pFile) 
                * sizeof(m_dOffset); 
  
  // Add sample rate (8 bytes)
  uHdrBytes += fwrite(&m_dSampleRate, sizeof(m_dSampleRate), 1, m_pFile) 
                * sizeof(m_dSampleRate);
    
  // Add total number of bytes to follow (8 bytes)
  uHdrBytes += fwrite(&m_uBytesPerAccumulation, sizeof(m_uBytesPerAccumulation), 
                      1, m_pFile) * sizeof(m_uBytesPerAccumulation);
                      
  // Expect header to be 56 bytes
  if (uHdrBytes != 56) {
    printf ("Error writing to header to dump file.\n");
    return false;
  }
    
  return true;
}



// ----------------------------------------------------------------------------
// closeFile
// ----------------------------------------------------------------------------
void Dumper::closeFile()
{
  // If this is the first time we've tried to close the file, record the
  // interval.  
  m_timer.toc_if_first();
  
  // Close it
  if (m_pFile != NULL) {
    fclose(m_pFile);
    m_pFile = NULL;
  }
}



// ----------------------------------------------------------------------------
// waitForEmpty - Blocks until stop signal is received or no more items can
//                start  processing (which is when there are only m_uNumTaps-1
//                item in the full list).  Make sure holds on all items
//                have cleared, indicating no items are still in processing.
// ----------------------------------------------------------------------------
void Dumper::waitForEmpty()
{
  // Wait
  while (!m_bStop && ( (m_buffer.size() > 0) || (m_buffer.holds() > 0))) {

    printf("Dump: buffer size = %u, holds = %u\n", m_buffer.size(), m_buffer.holds());
    usleep(DUMPER_THREAD_SLEEP_MICROSECONDS);
  }

  // Can't process any more so clear the stragglers from the buffer
  m_buffer.clear();
  
  // Close the file if we haven't alrady
  closeFile();
}



// ----------------------------------------------------------------------------
// push -- Copies data into a buffer for processing.  If no buffers are 
//         available, it will return false.
// ----------------------------------------------------------------------------
bool Dumper::push(void* pIn, unsigned int uByteLength)
{
  if (m_pFile && (m_uBytesWritten < m_uBytesPerAccumulation)) {
    return m_buffer.push(pIn, uByteLength);
  }
  
  return false;
} // push()



// ----------------------------------------------------------------------------
// threadIsReady -- Allow the new thread to report it is ready
// ----------------------------------------------------------------------------
void Dumper::threadIsReady() {
  m_uNumReady++;
}



// ----------------------------------------------------------------------------
// threadLoop -- Primary execution loop of each thread
// ----------------------------------------------------------------------------
void* Dumper::threadLoop(void* pContext)
{
  Dumper* pDumper = (Dumper*) pContext;
  
  // Create an iterator for the buffer
  ByteBuffer::iterator iter;

  // Report ready
  pDumper->threadIsReady();

  while (!pDumper->m_bStop) {

    // If we have an open file and something is in the buffer, write 
    // the data from the buffer item to the file
    if (pDumper->m_pFile && pDumper->m_buffer.request(iter, 1)) {

      pDumper->m_uBytesWritten += fwrite( pDumper->m_buffer.data(iter), 1, 
                                          pDumper->m_uBytesPerTransfer, 
                                          pDumper->m_pFile );
    
      // If we've written all we expected for a given file, close the file
      if (pDumper->m_uBytesWritten >= pDumper->m_uBytesPerAccumulation) {
        pDumper->closeFile();    
      }
      
      // Release the iterator to be able to do it again
      pDumper->m_buffer.release(iter);

    } else {

      // Sleep before trying again
      usleep(DUMPER_THREAD_SLEEP_MICROSECONDS);
    }
  }
  
  // Exit the thread
  pthread_exit(NULL);

}

   




