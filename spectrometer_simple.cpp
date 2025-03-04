#include "spectrometer_simple.h"
#include "timing.h"
#include "utility.h"
#include "version.h"
#include <unistd.h> // usleep

#define SPEC_SLEEP_MICROSECONDS 5
#define SPEC_PLOT_SECONDS 10

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
                                       unsigned long uNumSpectraPerAccumulation,                                       
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
  m_uNumFFT = 2*m_uNumChannels;
  
  m_uNumSpectraPerAccumulation = uNumSpectraPerAccumulation;  
//  m_uNumSpectraPerAccumulation = m_uNumSamplesPerAccumulation / m_uNumFFT;

	printf("Spectrometer: Number of spectra per accumulation: %lu\n", m_uNumSpectraPerAccumulation);
						
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
    uCycleDrops = 0;
    dutyCycleTimer.tic();
    tk.setNow();

    printf("----------------------------------------------------------------------\n");
    printf("Spectrometer: Starting cycle %lu at %s\n", uCycle, tk.getDateTimeString(5).c_str());
    printf("Spectrometer: Total run time so far: %.02f seconds (%.3g days)\n", 
      totalRunTimer.toc(), totalRunTimer.toc()/3600.0/24);   
    //printf("Spectrometer: m_write.size: %ld, m_active.size: %ld, m_released.size: %ld\n", m_write.size(), m_active.size(), m_released.size());
    printf("----------------------------------------------------------------------\n");
                  
    Accumulator* pAccum = activateAccumulator();
    if (pAccum) {
         
      // Mark state of new cycle
	    tk.setNow();
    
      // Reset current accumulator and record start time
      pAccum->clear();
      pAccum->setStartTime(tk);
      
		  // Acquire data
		  m_pDigitizer->acquire();

      // Note the stop time
      pAccum->setStopTime();
      
      // Wait for all the samples to be channelized
      m_pChannelizer->waitForEmpty();
    
      // Normalize ADCmin and ADCmax:  we divide adcmin and adcmax by 2 here to  
      // be backwards compatible with pxspec.  This limits adcmin and adcmax to 
      // +/- 0.5 rather than +/-1.0
      pAccum->setADCmin(pAccum->getADCmin()/2);
      pAccum->setADCmax(pAccum->getADCmax()/2);   
         
      printf("Spectrometer: Duty cycle = %6.3f\n", 1.0 * m_uNumSamplesPerAccumulation / (2.0 * 1e6 * m_dBandwidth) / dutyCycleTimer.toc());
      if (uCycleDrops > 0)
      {	
        printf("Spectrometer: Drop fraction = " RED "%6.3f\n" RESET, 1.0 * uCycleDrops / (m_uNumSamplesPerAccumulation + uCycleDrops));
      } else {
        printf("Spectrometer: Drop fraction = %6.3f\n", 1.0 * uCycleDrops / (m_uNumSamplesPerAccumulation + uCycleDrops));
      }     
      printf("Spectrometer: acdmin = %6.3f,  adcmax = %6.3f\n", pAccum->getADCmin(), pAccum->getADCmax());
      printf("\n");
    
      // Write the accumulation (asynchronously)
      transferAccumulatorToWriteQueue();
      
  	} else {
  	  printf("Spectrometer: ERROR - Failed to activate accumulator. Aborting.\n");
  	  sendStop();
  	}
  }

  // Print closing info
  printf("Spectrometer: Stopping...\n");
  printf("Spectrometer: Total cycles: %lu\n", uCycle);
  printf("Spectrometer: Total run time: %.02f seconds (%.3g days)\n", totalRunTimer.toc(), totalRunTimer.toc()/3600.0/24);

  // Make sure we've finished processing and writing everything in queus before
  // we end.
  waitForDone();

  printf("Spectrometer: Done.\n");

} // run()

// ----------------------------------------------------------------------------
// handleLivePlot() - Handle output for live plotting if needed
// ----------------------------------------------------------------------------
bool SpectrometerSimple::handleLivePlot(Accumulator* pAccum) {

  // Should we be plotting?
  if (!m_pController->plot()) {
  
    // If not, make sure the timer is stopped
    m_plotTimer.reset();
    return false;
  }

  // Plot if the controller just told us to start or if we've passed the
  // update interval
  if (!m_plotTimer.running() || (m_plotTimer.toc() > SPEC_PLOT_SECONDS)) {
      
    // Restart the plot timer
    m_plotTimer.tic();
    
    // Write the temporary file for plotting
    if (!write_plot_file( m_pController->getPlotFilePath(), 
                          *pAccum, *pAccum, *pAccum,
                          m_pController->getPlotBinLevel())) {
      return false;
    }
    
    // Tell the plotter to refresh
    m_pController->updatePlotter();
  }
  
  return true;
}

// ----------------------------------------------------------------------------
// isStop() 
// ----------------------------------------------------------------------------
bool SpectrometerSimple::isStop(unsigned long uCycle, Timer& ti) 
{
  if (m_bLocalStop) {
    return true;
  }

  if (m_pController && m_pController->stop()) {
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
	
		// If there are accumulations to write
		pAccum = pSpec->m_write.front();
		if (pAccum) {
		
			// Write to disk (creates new file/path if needed)
			pSpec->writeToFile(pAccum);
			
			// Write live plot file needed
			pSpec->handleLivePlot(pAccum);
				
		  // Release the accumulator back into waiting queue
		  pSpec->releaseAccumulator();
		
    } else {

      // Sleep before trying again
      usleep(SPEC_SLEEP_MICROSECONDS);
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
  while ((m_write.size() > 0) || (m_active.size() > 0)) 
  {
    printf("m_write.size: %ld, m_active.size: %ld, m_released.size: %ld\n", m_write.size(), m_active.size(), m_released.size());
    usleep(1000000);
  }
}




// ----------------------------------------------------------------------------
// WriteToFile
// ----------------------------------------------------------------------------
bool SpectrometerSimple::writeToFile(Accumulator* pAccum) {

  std::string sFilePath = m_pController->getAcqFilePath(pAccum->getStartTime());
  sFilePath.replace(sFilePath.length()-3, 3, "ssp");
  
  // Write the data
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
    fs << "; SIMPLESPEC v" << VERSION_MAJOR << "." 
                           << VERSION_MINOR << "." 
                           << VERSION_PATCH << std::endl;
    fs << m_pController->getConfigStr();
    fs << ";+++BEGIN_DATA_HEADER" << std::endl;
    fs << ";--data_type_size: " << sizeof(ACCUM_DATA_TYPE) << std::endl;
    fs << ";--num_channels: " << pAccum->getDataLength() << std::endl;  
    fs << ";--start_frequency: " << pAccum->getStartFreq() << " MHz" << std::endl;
    fs << ";--stop_frequency: " << pAccum->getStopFreq() << " MHz" << std::endl;
    
    double dStepFreq = (pAccum->getStopFreq() - pAccum->getStartFreq()) / (double) pAccum->getDataLength();
    fs << ";--step_frequency: " << dStepFreq << " MHz" << std::endl;    
    fs << ";+++END_DATA_HEADER" << std::endl;
    
    fs.close();    
  }

  // Write the data
  return append_accumulation_simple( sFilePath,  pAccum );
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
    
  // printf("Spectrometer: OnDigitizerData\n");
  
  // Loop over the transferred data and enter it into the channelizer buffer
  while ( ((uIndex + m_uNumFFT) <= uBufferLength) 
         && (uTransferredSoFar < m_uNumSamplesPerAccumulation)) {
    
    // Try to add to the channelizer buffer
    if (m_pChannelizer->push(&(pBuffer[uIndex]), m_uNumFFT, dScale, dOffset)) {
     
      uTransferredSoFar += m_uNumFFT;
      uAdded++;
      
    }  else {
      printf("Spectrometer::OnDigitizerData: Failed to push to channelizer!\n");
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

	if (m_uNumMinFreeAccumulators > m_released.size())
		m_uNumMinFreeAccumulators = m_released.size();

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
	

	
