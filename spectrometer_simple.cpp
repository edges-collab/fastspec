#include "spectrometer_simple.h"
#include "timing.h"
#include "utility.h"
#include "version.h"
#include <unistd.h> // usleep

#define SWITCH_SLEEP_MICROSECONDS 500000

// Terminal font colors
#define RED   "\x1B[31m"
#define GRN   "\x1B[32m"
#define YEL   "\x1B[33m"
#define BLU   "\x1B[34m"
#define MAG   "\x1B[35m"
#define CYN   "\x1B[36m"
#define WHT   "\x1B[37m"
#define RESET "\x1B[0m"

using namespace std;

// ----------------------------------------------------------------------------
// Constructor
// ----------------------------------------------------------------------------
SpectrometerSimple::SpectrometerSimple(unsigned long uNumChannels, 
                                       unsigned long uNumSamplesPerAccumulation, 
                                       double dBandwidth,
                                       unsigned int uNumAccumulators,
                                       Digitizer* pDigitizer,
                                       Channelizer* pChannelizer,
                                       Controller* pController )
{

  // User specified configuration
  m_uNumChannels = uNumChannels;
  m_uNumSamplesPerAccumulation = uNumSamplesPerAccumulation;
  m_dBandwidth = dBandwidth;
  m_uNumAccumulators = uNumAccumulators;

  // Derived configuration
  m_dChannelSize = m_dBandwidth / (double) m_uNumChannels; // MHz
  m_dAccumulationTime = (double) uNumSamplesPerAccumulation / m_dBandwidth / 2.0; // seconds
  m_dStartFreq = 0.0;
  m_dStopFreq = m_dBandwidth;
  m_dChannelFactor = 2; // because of Blackmann Harris 
  m_uNumSpectraPerAccumulation = m_uNumSamplesPerAccumulation / (2*m_uNumChannels);

	printf("Spectrometer: Number of spectra per accumulation: %ul\n", m_uNumSpectraPerAccumulation);
			
			
  // Assign the Spectrometer instance's process function to the Digitizer
  // callback on data transfer.
  m_pDigitizer = pDigitizer;
  m_pDigitizer->setCallback(this);
  
  // Remember the controller
  m_pController = pController;

  // Setup the stop flags
  m_bLocalStop = false;
  m_bUseStopCycles = false;
  m_bUseStopSeconds = false;
  m_bUseStopTime = false;
  m_uStopCycles = 0;
  m_dStopSeconds = 0;

  // Initialize the FFT pool
  m_uNumFFT = 2*m_uNumChannels;
  m_pChannelizer = pChannelizer;
  m_pChannelizer->setCallback(this);
  
	// Allocate the accumulators
	for (unsigned int i=0; i<m_uNumAccumulators; i++) {
	
		Accumulator* pAccum = new Accumulator();
		if (pAccum)
		{
			pAccum->init(m_uNumChannels, m_dStartFreq, m_dStopFreq, m_dChannelFactor);
			m_released.push_front(pAccum);
			m_uNumMinFreeAccumulators++;
		} else {
			printf("Spectrometer: Failed to allocate accumulator %d\n", i);
		}
	}	
	
  
  // Initialize the mutex
  pthread_mutex_init(&m_mutex, NULL);

  // Spawn the disk writing thread
  printf("Spectrometer: Creating file thread...\n");
  m_bThreadIsReady = false;
  m_bThreadStop = false;
  if (pthread_create(&m_thread, NULL, threadLoop, this) != 0 ) {
  	printf("Spectrometer: Failed to create thread.");
  	sendStop();
  }

  // Wait for thread to report ready
  Timer tr;
  tr.tic();
  while (!m_bThreadIsReady) {
    usleep(10000);
  }
  printf("Spectrometer: Thread is ready after %.3g seconds\n", tr.toc());

} // constructor



// ----------------------------------------------------------------------------
// Destructor
// ----------------------------------------------------------------------------
SpectrometerSimple::~SpectrometerSimple()
{
	// Make sure everyone knows we're stopping
	sendStop();
	
	// Wait for all accumulations to finish writing
  waitForDone();
  m_bThreadStop = true;
  
  // Disconnect from digitizer
  if (m_pDigitizer) {
    m_pDigitizer->setCallback(NULL);
  }

  // Disconnect from channelizer
  if (m_pChannelizer) {

    // Wait for the channelizer to send us any data it is still processing
    m_pChannelizer->waitForEmpty();  

    // Disconnect
    m_pChannelizer->setCallback(NULL);
  }
   
  printf("Spectrometer: Maximum number of accumulators used: %d of %d\n", 
  m_uNumAccumulators - m_uNumMinFreeAccumulators, m_uNumAccumulators);
  
	// Free accumulators
	list<Accumulator*>::iterator it;

	for (it = m_released.begin(); it != m_released.end(); it++) {
		free(*it);
	}

	for (it = m_active.begin(); it != m_active.end(); it++) {
		free(*it);
	}

	for (it = m_write.begin(); it != m_write.end(); it++) {
		free(*it);
	}

	// Free thread
	pthread_mutex_destroy(&m_mutex);

	
} // destructor



// ----------------------------------------------------------------------------
// run() - Main executable function for the EDGES spectrometer
// ----------------------------------------------------------------------------
void SpectrometerSimple::run()
{ 
  m_bLocalStop = false;
  Timer totalRunTimer;
  Timer dutyCycleTimer;
  Timer writeTimer;
  Timer plotTimer;
  TimeKeeper tk;
  double dDutyCycle_Overall;
  unsigned long uCycleDrops = 0;
  unsigned long uCycle = 0;

  if ((m_pDigitizer == NULL) || (m_pChannelizer == NULL)) {
    printf("Spectrometer: No digitizer or channelizer object at start of run.  End.\n");
    return;
  }

  // Loop until a stop signal is received
  totalRunTimer.tic();
  printf("\n");

  while (!isStop(uCycle, totalRunTimer)) {

    uCycle++;
    dutyCycleTimer.tic();
    tk.setNow();

    printf("----------------------------------------------------------------------\n");
    printf("Spectrometer: Starting cycle %lu at %s\n", uCycle, tk.getDateTimeString(5).c_str());
    printf("Spectrometer: Total run time so far: %.0f seconds (%.3g days)\n", 
      totalRunTimer.toc(), totalRunTimer.toc()/3600.0/24);   
    printf("----------------------------------------------------------------------\n");

    // Check for abort signal
    if (isAbort()) { 
      printf("Spectrometer: Aborting -- Waiting for channelizer to finish...");
      m_pChannelizer->waitForEmpty();
      printf("  Done.\n");
      return; 
    }
                   
    Accumulator* pAccum = activateAccumulator();
    if (pAccum) {
         
      // Mark state of new cycle
	    tk.setNow();
    	printf("Spectrometer: Starting accumulation %lu at %s\n", uCycle, tk.getDateTimeString(5).c_str());
    
      // Reset current accumulator and record start time
      pAccum->clear();
      pAccum->setStartTime(tk);
      
		  // Acquire data
		  m_pDigitizer->acquire();

      // Note the stop time
      pAccum->setStopTime();
  	}
  }

  // Print closing info
  printf("Spectrometer: Stopping...\n");
  printf("Spectrometer: Total cycles: %lu\n", uCycle);
  printf("Spectrometer: Total run time: %.0f seconds (%.3g days)\n", totalRunTimer.toc(), totalRunTimer.toc()/3600.0/24);

  // Make sure the channelizer has cleared before we end
  m_pChannelizer->waitForEmpty();

  printf("Spectrometer: Done.\n");

} // run()


// ----------------------------------------------------------------------------
// isAbort() 
// ----------------------------------------------------------------------------
bool SpectrometerSimple::isAbort() 
{
  if (m_bLocalStop) {
    return true;
  }

  if (m_pController) {
    return m_pController->stop();
  }

  return false;
} // isAbort()


// ----------------------------------------------------------------------------
// isStop() 
// ----------------------------------------------------------------------------
bool SpectrometerSimple::isStop(unsigned long uCycle, Timer& ti) 
{
  if (isAbort()) {
    return true;
  }

  if (m_bUseStopCycles && (uCycle >= m_uStopCycles)) {
    return true;
  }

  if (m_bUseStopSeconds && (ti.toc() >= m_dStopSeconds)) {
    return true;
  }

  TimeKeeper tk;
  tk.setNow();

  if (m_bUseStopTime && (tk >= m_tkStopTime)) {
    return true;
  }

  return false;
} // isStop()


// ----------------------------------------------------------------------------
// sendStop() 
// ----------------------------------------------------------------------------
void SpectrometerSimple::sendStop() 
{
  m_bLocalStop = true;
}


// ----------------------------------------------------------------------------
// setStopCycles()
// ----------------------------------------------------------------------------
void SpectrometerSimple::setStopCycles(unsigned long uCycles) 
{
  m_bUseStopCycles = true;
  m_uStopCycles = uCycles;

  printf("Spectrometer: Will stop after %lu cycles.\n", uCycles);
}


// ----------------------------------------------------------------------------
// setStopSeconds()
// ----------------------------------------------------------------------------
void SpectrometerSimple::setStopSeconds(double dSeconds) 
{
  m_bUseStopSeconds = true;
  m_dStopSeconds = dSeconds;

  printf("Spectrometer: Will stop after %.0f seconds (%.3g days).\n", dSeconds, dSeconds/24.0/3600.0);
}


// ----------------------------------------------------------------------------
// setStopTime()
// ----------------------------------------------------------------------------
void SpectrometerSimple::setStopTime(const string& strDateTime) 
{
  m_bUseStopTime = true;
  m_tkStopTime.set(strDateTime);

  printf("Spectrometer: Will stop at %s.\n", m_tkStopTime.getDateTimeString(5).c_str());
}


// ----------------------------------------------------------------------------
// threadIsready
// ----------------------------------------------------------------------------
void SpectrometerSimple::threadIsReady()
{
	m_bThreadIsReady = true;
}



// ----------------------------------------------------------------------------
// threadLoop -- Primary execution loop of each thread
// ----------------------------------------------------------------------------
void* SpectrometerSimple::threadLoop(void* pContext)
{
  SpectrometerSimple* pSpec = (SpectrometerSimple*) pContext;

  // Report ready
  pSpec->threadIsReady();

  while (!pSpec->m_bThreadStop) {

		Accumulator* pAccum = NULL;
	
		//if there are accumulations to write
		pAccum = pSpect->m_write.pop_front();
		if (pAccum) {
		
			// Write to disk
		
    } else {

      // Sleep before trying again
      usleep(THREAD_SLEEP_MICROSECONDS);
    }
  }

  // Exit the thread
  pthread_exit(NULL);

}



// ----------------------------------------------------------------------------
// WaitForDone
// ----------------------------------------------------------------------------
void SpectrometerSimple::waitForDone() 
{
  // Wait
  while (m_write.size() > 0) || (m_active.size() > 0)) 
  {
    usleep(THREAD_SLEEP_MICROSECONDS);
  }
}




// ----------------------------------------------------------------------------
// WriteToFile
// ----------------------------------------------------------------------------
bool SpectrometerSimple::writeToFile() {

  std::string sFilePath = m_pController->getAcqFilePath(m_accumAntenna.getStartTime());

  // Write the .acq entry
  printf("Spectrometer: Writing accumulations to file: %s\n", sFilePath.c_str());

  // If needed, start a new file and write the header
  if (!is_file(sFilePath)) {

    // Write the header
    std::ofstream fs;
    fs.open(sFilePath);
    if (!fs.is_open()) {
      printf("Spectrometer: Failed to write header to new .acq file.\n");
      return false;
    }
    fs << "; FASTSPEC v" << VERSION_MAJOR << "." 
                         << VERSION_MINOR << "." 
                         << VERSION_PATCH << std::endl;
    fs << m_pController->getConfigStr();
    fs.close();
  }

  // Write the data
  return append_accumulation( sFilePath, 
                              m_accum1 );

}



// ----------------------------------------------------------------------------
// onDigitizerData() -- Do something with transferred data.  This function ignores
//                 any transferred data at the end of the transfer beyond the
//                 last integer multiple of m_uNumFFT.  If an Channelizer buffer 
//                 is not available for a given chunk of the transfer, it drops 
//                 that chunk of data.  Returns total number of samples 
//                 successfully processed into a Channelizer buffer.
// ----------------------------------------------------------------------------
unsigned long SpectrometerSimple::onDigitizerData( SAMPLE_DATA_TYPE* pBuffer, 
                                             unsigned int uBufferLength,
                                             unsigned long uTransferredSoFar,
                                             double dScale,
                                             double dOffset ) 
{
  unsigned int uIndex = 0;
  unsigned int uAdded = 0;
    
  // Loop over the transferred data and enter it into the channelizer buffer
  while ( ((uIndex + m_uNumFFT) <= uBufferLength) 
         && (uTransferredSoFar < m_uNumSamplesPerAccumulation)) {
    
    // Try to add to the channelizer buffer
    if (m_pChannelizer->push(&(pBuffer[uIndex]), m_uNumFFT, dScale, dOffset)) { 
      uTransferredSoFar += m_uNumFFT;
      uAdded++;
    }  
        
    // Increment the index to the next chunk of transferred data
    uIndex += m_uNumFFT;
  }

  // Keep a permanent record of how many samples were dropped
  m_active.back()->addDrops(uBufferLength - uAdded * m_uNumFFT);

  // Return the total number of transferred samples that were successfully
  // entered in the FFTPool buffer for processing.
  return uAdded * m_uNumFFT;

} // onDigitizerData()




// ----------------------------------------------------------------------------
// onChannelizerData() -- Do something with a spectrum returned from the Channelizer
// ----------------------------------------------------------------------------
void SpectrometerSimple::onChannelizerData(ChannelizerData* pData) 
{  
	m_active.front()->add(pData->pData, pData->uNumChannels, pData->dADCmin, pData->dADCmax);
  if (m_active.front()->getNumAccums() == m_uSpectraPerAccumulation)
  {
  	transferAccumulationToWriteQueue();
  	
  	/*
		
    // Normalize ADCmin and ADCmax:  we divide adcmin and adcmax by 2 here to  
    // be backwards compatible with pxspec.  This limits adcmin and adcmax to 
    // +/- 0.5 rather than +/-1.0
    m_accum1.setADCmin(m_accum1.getADCmin()/2);
    m_accum1.setADCmax(m_accum1.getADCmax()/2);
    m_accum2.setADCmin(m_accum2.getADCmin()/2);
    m_accum2.setADCmax(m_accum2.getADCmax()/2);    
    
    // Write to file
    writeTimer.tic();
    //printf("\nSpectrometer: Writing to file...\n");
    writeToFile();
    writeTimer.toc();
    
    // Calculate overall duty cycle
    dutyCycleTimer.toc();
    dDutyCycle_Overall = 3.0 * m_uNumSamplesPerAccumulation / (2.0 * 1e6 * m_dBandwidth) / dutyCycleTimer.get();
    uCycleDrops = m_accumAntenna.getDrops() + m_accumAmbientLoad.getDrops() + m_accumHotLoad.getDrops();

    printf("\n");
    printf("Spectrometer: Cycle time             = %6.3f seconds\n", dutyCycleTimer.get());
    printf("Spectrometer: Accum time (ideal)     = %6.3f seconds\n", 3.0 * m_uNumSamplesPerAccumulation / (2.0 * 1e6 * m_dBandwidth));
    printf("Spectrometer: File write time         = %6.3f seconds\n", writeTimer.get());
    if (m_pController->plot()) {
      printf("Spectrometer: Plot write time        = %6.3f seconds\n", plotTimer.get());
    }
   
    printf("Spectrometer: Duty cycle             = %6.3f\n", dDutyCycle_Overall);
    if (uCycleDrops>0) {
      printf("Spectrometer: Drop fraction          = " RED "%6.3f\n" RESET, 1.0 * uCycleDrops / (m_uNumSamplesPerAccumulation + uCycleDrops));
    } else {
      printf("Spectrometer: Drop fraction          = %6.3f\n", 1.0 * uCycleDrops / (m_uNumSamplesPerAccumulation + uCycleDrops));
    }
    printf("Spectrometer: p0 (antenna) -- acdmin = %6.3f,  adcmax = %6.3f\n", m_accum1.getADCmin(), m_accum1.getADCmax());
    printf("Spectrometer: p1 (ambient) -- acdmin = %6.3f,  adcmax = %6.3f\n", m_accum2.getADCmin(), m_accum2.getADCmax());
    printf("\n");

*/
  	
  	
  	
  	
  	
  	
  }
  
} // onChannelizerData()




// ----------------------------------------------------------------------------
// activateAccumulator()
// ----------------------------------------------------------------------------
Accumulator* SpectrometerSimple::activateAccumulator() 
{
  Accumulator* pAccum = NULL;
      
  pthread_mutex_lock(&m_mutex);

  if (!m_released.empty()) 
  {	
  	pAccum = m_released.front();
    m_active.push_back(pAccum);
  	m_released.pop_front();
  }

	if (m_uMinFreeAccumulators > m_released.size())
		m_uMinFreeAccumulators = m_released.size();

  pthread_mutex_unlock(&m_mutex);

	return pAccum;
}


// ----------------------------------------------------------------------------
// transferAccumulatorToWriteQueue()
// -- Returns a pointer to the next active accumulator (if available, otherwise NULL)
// ----------------------------------------------------------------------------
Accumulator* SpectrometerSimple::transferAccumulatorToWriteQueue() 
{
  Accumulator* pAccum = NULL;
  
  pthread_mutex_lock(&m_mutex);
  
  if (!m_active.empty()) 
  {
  	pAccum = m_active.front();  	
  	m_write.push_back(pAccum);
  	m_active.pop_front();
  
  	// Pointer to next accumulator in active queue
  	if (!m_active.empty())
  	{
	  	pAccum = m_active.front();
	 	}
  }
  
  pthread_mutex_unlock(&m_mutex);
  
  return pAccum;
}
	
	
	
// ----------------------------------------------------------------------------
// releaseAccumulator()
// ----------------------------------------------------------------------------
void SpectrometerSimple::releaseAccumulator() 
{
	Accumulator* pAccum = NULL;

  pthread_mutex_lock(&m_mutex);
  
  if (!m_write.empty()) 
  {
  	pAccum = m_write.front();
  	pAccum->clear();
  	m_released.push_back(pAccum);
  	m_write.pop_front();
  }
  
  pthread_mutex_unlock(&m_mutex);
}
	

	
