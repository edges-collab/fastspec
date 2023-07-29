
#include <algorithm>
#include <stdlib.h>
#include <unistd.h>
#include "pfb.h"
#include "utility.h"




// ----------------------------------------------------------------------------
// Constructor
// ----------------------------------------------------------------------------
PFB::PFB( unsigned int uNumThreads, unsigned int uNumBuffers, 
          unsigned int uNumChannels, unsigned int uNumTaps, 
          unsigned int uWindow )
{
  m_uNumTaps = uNumTaps;
  m_uNumThreads = uNumThreads;
  m_uNumBuffers = uNumBuffers;
  m_uNumChannels = uNumChannels;
  m_uNumFFT = 2*m_uNumChannels;
  m_uNumSamples = m_uNumTaps*m_uNumFFT;
  m_pReceiver = NULL;
  m_bStop = false;

  // Allocate space for window function
  m_pWindow = NULL;
  setWindowFunction(uWindow);

  // Create buffers
  printf("PFB: Creating %d buffers (%g MB)...\n", m_uNumBuffers, 
    ((float) m_uNumBuffers)*m_uNumFFT*sizeof(BUFFER_DATA_TYPE)/1024/1024);
  m_buffer.allocate(m_uNumBuffers, m_uNumFFT);

  // Initialize the mutexes
  pthread_mutex_init(&m_mutexCallback, NULL);
  pthread_mutex_init(&m_mutexPlan, NULL);

  // Allocate space for thread handles
  printf("PFB: Creating %d threads...\n", m_uNumThreads);
  m_pThreads = (pthread_t*) malloc(m_uNumThreads * sizeof(pthread_t));

  // Spawn the threads
  m_uNumReady = 0;
  unsigned int uFailed = 0;
  for (unsigned int i=0; i < m_uNumThreads; i++) {
    if (pthread_create(&(m_pThreads[i]), NULL, threadLoop, this) != 0 ) {
      printf("PFB: Failed to create thread %d of %d.", i, m_uNumThreads);
      uFailed++;
    } 
  }

  // Wait for all threads to report ready
  Timer tr;
  tr.tic();
  while (m_uNumReady < (m_uNumThreads-uFailed)) {
    usleep(10000);
  }
  printf("PFB: All threads ready after %.3g seconds\n", tr.toc());
}



// ----------------------------------------------------------------------------
// Destructor
// ----------------------------------------------------------------------------
PFB::~PFB()
{
  // Join all of our threads back to us
  m_bStop = true;
  
  for (unsigned int i=0; i<m_uNumThreads; i++) {
    pthread_join(m_pThreads[i], NULL);
  }

  // Free the thread pointers
  free(m_pThreads);

  // Destroy the mutexes
  pthread_mutex_destroy(&m_mutexCallback);
  pthread_mutex_destroy(&m_mutexPlan);

  // Free with the window array
  if (m_pWindow != NULL) {
    free(m_pWindow);
  }
}



// ----------------------------------------------------------------------------
// setCallback
// ----------------------------------------------------------------------------
void PFB::setCallback(ChannelizerReceiver* pReceiver)
{
  m_pReceiver = pReceiver;
}



// ----------------------------------------------------------------------------
// setWindowFunction - 
//     
// ----------------------------------------------------------------------------
bool PFB::setWindowFunction(unsigned int uType)
{

  // Create the window array if it doesn't already exist
  if (m_pWindow == NULL) {
    m_pWindow = (BUFFER_DATA_TYPE*) malloc(m_uNumSamples*sizeof(BUFFER_DATA_TYPE));
    if (m_pWindow == NULL) {
      printf("PFB:: Failed to allocate memory for window function.");
      return false;
    }
  }

  // Apply window function (if any)
  switch (uType) {

    case 1: 
      printf ("PFB: No window function (no sinc)\n");
      for (unsigned int i=0; i<m_uNumSamples; i++) {
        m_pWindow[i] = 1;
      }
      break;

    case 2:
      printf ("PFB: Using Blackman Harris (only, no sinc) window function\n");
      get_blackman_harris(m_pWindow, m_uNumSamples);
      break;

    case 3:
      printf ("PFB: Using sinc with Blackman Harris window function\n");
      get_blackman_harris(m_pWindow, m_uNumSamples);
      get_sinc(m_pWindow, m_uNumSamples, m_uNumChannels, true);
      break;

    default: 
      printf ("PFB: Using sinc only, no additional window function\n");
      get_sinc(m_pWindow, m_uNumSamples, m_uNumChannels, false);
      break;
  }
    
  return true;
}



// ----------------------------------------------------------------------------
// waitForEmpty - Blocks until stop signal is received or no more items can
//                start  processing (which is when there are only m_uNumTaps-1
//                item in the full list).  Make sure holds on all items
//                have cleared, indicating no items are still in processing.
// ----------------------------------------------------------------------------
void PFB::waitForEmpty()
{
  // Wait
  while (!m_bStop && ( (m_buffer.size() >= m_uNumTaps) || (m_buffer.holds() > 0))) {
    usleep(THREAD_SLEEP_MICROSECONDS);
  }

  // Can't process any more so clear the stragglers from the buffer
  m_buffer.clear();
}



// ----------------------------------------------------------------------------
// push -- Copies data into a buffer for processing.  If no buffers are 
//         available, it will return false.
// ----------------------------------------------------------------------------
bool PFB::push(SAMPLE_DATA_TYPE* pIn, unsigned int uLength, double dScale, 
               double dOffset)
{
  return m_buffer.push(pIn, uLength, dScale, dOffset);

} // push()



// ----------------------------------------------------------------------------
// threadIsReady -- Allow a new thread to report is ready
// ----------------------------------------------------------------------------
void PFB::threadIsReady() {
  pthread_mutex_lock(&m_mutexCallback);
  m_uNumReady++;
  pthread_mutex_unlock(&m_mutexCallback);
}



// ----------------------------------------------------------------------------
// threadLoop -- Primary execution loop of each thread
// ----------------------------------------------------------------------------
void* PFB::threadLoop(void* pContext)
{
  PFB* pPool = (PFB*) pContext;

  // Allocate the FFT and final spectrum buffers
  FFT_REAL_TYPE* pLocal1 = (FFT_REAL_TYPE*) FFT_MALLOC(pPool->m_uNumFFT * sizeof(FFT_REAL_TYPE));
  FFT_COMPLEX_TYPE* pLocal2 = (FFT_COMPLEX_TYPE*) FFT_MALLOC((pPool->m_uNumChannels+1) * sizeof(FFT_COMPLEX_TYPE));

  // Create the FFT plan
  pthread_mutex_lock(&(pPool->m_mutexPlan));
  FFT_PLAN_TYPE pPlan = FFT_PLAN(pPool->m_uNumFFT, pLocal1, pLocal2, FFTW_MEASURE);
  pthread_mutex_unlock(&(pPool->m_mutexPlan));

  // Do a trial FFT execution 
  FFT_EXECUTE(pPlan);

  // Create an iterator for the buffer
  Buffer::iterator iter;

  // Report ready
  pPool->threadIsReady();

  while (!pPool->m_bStop) {

    // Try to get a full set of buffer items that need processing
    if (pPool->m_buffer.request(iter, pPool->m_uNumTaps)) {

      // Process the data in the buffer
      pPool->process(iter, pLocal1, pLocal2, pPlan);

      // Release the iterator to be able to do it again
      pPool->m_buffer.release(iter);

    } else {

      // Sleep before trying again
      usleep(THREAD_SLEEP_MICROSECONDS);
    }
  }

  // Destroy the FFT plan
  FFT_DESTROY_PLAN(pPlan);

  // Release the local buffers
  FFT_FREE(pLocal1);
  FFT_FREE(pLocal2);

  // Exit the thread
  pthread_exit(NULL);

}



// ----------------------------------------------------------------------------
// process -- Handle a buffer of data
// ----------------------------------------------------------------------------
void PFB::process( Buffer::iterator& iter, FFT_REAL_TYPE* pLocal1, 
                       FFT_COMPLEX_TYPE* pLocal2, FFT_PLAN_TYPE pPlan)
{

  unsigned int i;
  unsigned int t;
  BUFFER_DATA_TYPE* pIn = NULL;
  BUFFER_DATA_TYPE* pWin = m_pWindow;
  BUFFER_DATA_TYPE dMax = 0;
  BUFFER_DATA_TYPE dMin = 0;

  // Loop over taps of data
  for (t=0; t<m_uNumTaps; t++) {

    // Get the current buffer block and window block
    pIn = m_buffer.data(iter);
    pWin = &(m_pWindow[t*m_uNumFFT]);

    // Populate the pre-FFT array
    // Treat the first tap specially
    if (t==0) {

      // Overwrite anything already in array
      for (i=0; i<m_uNumFFT; i++) {
        pLocal1[i] = pIn[i] * pWin[i];
      }

      // Find the ADC max and min *** NOTE***
      // By only searching the first tap of data, when the entire buffer is 
      // processed across all threads, we won't have searched the last 
      // (m_uNumTaps-1) blocks of data.  Saving this for future work.
      dMax = pIn[0];
      dMin = pIn[0];
      for (i=0; i<m_uNumFFT; i++) {
        if (pIn[i] < dMin) { 
          dMin = pIn[i]; 
        } else if (pIn[i] > dMax) { 
          dMax = pIn[i]; 
        }
      }

    // All other taps
    } else {

      // Add in place to the array
      for (i=0; i<m_uNumFFT; i++) {
        pLocal1[i] += pIn[i] * pWin[i];
      }
    }

    // Advance the iterator to next block of data for next tap
    if (t<(m_uNumTaps-1)) {
      m_buffer.next(iter);
    }
  }
  
  // Perform the FFT
  FFT_EXECUTE(pPlan);

  // Square and calculate the spectrum 
  // This ignores the nyquist (highest) frequency keeping with preivous 
  // EDGES codes
  for (i = 0; i < m_uNumChannels; i++) { 
    pLocal1[i] = pLocal2[i][0]*pLocal2[i][0] + pLocal2[i][1]*pLocal2[i][1];
  }

  // Pack the resulting spectrum for sending to callback
  ChannelizerData sData;
  sData.pData = pLocal1;
  sData.uNumChannels = m_uNumChannels;
  sData.dADCmin = dMin;
  sData.dADCmax = dMax;

  // Send the resulting spectrum to the callback function for handling
  
  if (m_pReceiver == NULL) {
    printf("ERROR: PFB process has no callback function assigned!\n");
  } else {
    pthread_mutex_lock(&m_mutexCallback);
    m_pReceiver->onChannelizerData(&sData);
    pthread_mutex_unlock(&m_mutexCallback);
  }

} // process()
