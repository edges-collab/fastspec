#include "spectrometer_simple.h"
#include "timing.h"
#include "utility.h"
#include "version.h"
#include <unistd.h> // usleep

#define SPEC_SLEEP_MICROSECONDS 5

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
                                       double dPlotIntervalSeconds,                               
                                       Digitizer* pDigitizer,
                                       Channelizer* pChannelizer,
                                       Controller* pController )
{

  // User specified configuration
  m_uNumChannels = uNumChannels;
  m_uNumSamplesPerAccumulation = uNumSamplesPerAccumulation;
  m_dBandwidth = dBandwidth;
  m_uNumAccumulators = uNumAccumulators;
  m_dPlotIntervalSeconds = dPlotIntervalSeconds;

  // Derived configuration
  m_dChannelSize = m_dBandwidth / (double) m_uNumChannels; // MHz
  m_dAccumulationTime = (double) uNumSamplesPerAccumulation / (m_dBandwidth * 1e6); // seconds
  m_dStartFreq = 0.0;
  m_dStopFreq = m_dBandwidth;
  m_dChannelFactor = 2; // because of Blackmann Harris 
  m_uNumFFT = 2*m_uNumChannels;
  m_uNumSamplesSoFar = 0;  
// m_uNumSpectraPerAccumulation = 1 + (uSamplesPerAccum - uNumTaps*uNumFFT) / uNumFFT;
  m_uNumSpectraPerAccumulation = m_uNumSamplesPerAccumulation / m_uNumFFT;

	printf("Spectrometer: Number of spectra per accumulation: %lu\n", m_uNumSpectraPerAccumulation);
						
  // Setup the stop flags
  m_bLocalStop = false;
  m_bUseStopCycles = false;
  m_bUseStopSeconds = false;
  m_bUseStopTime = false;
  m_uStopCycles = 0;
  m_dStopSeconds = 0;
						
  // Remember the controller
  m_pController = pController;
  
    // Assign this instance to the digitizer callback to receive samples.
  m_pDigitizer = pDigitizer;
  m_pDigitizer->setCallback(this);
  
  // Assign this instance to channelizer to receive channelized spectra.
  m_pChannelizer = pChannelizer;
  m_pChannelizer->setCallback(this);
  
	// Allocate the accumulator pool
	m_uNumMinFreeAccumulators = 0;
	for (unsigned int i=0; i<m_uNumAccumulators; i++) {
	
		Accumulator* pAccum = new Accumulator();
		if (pAccum)
		{
		  pAccum->setId(m_uNumMinFreeAccumulators++);
			pAccum->init(m_uNumChannels, m_dStartFreq, m_dStopFreq, m_dChannelFactor);
			m_empty.push_front(pAccum);
		} else {
			printf("Spectrometer: Failed to allocate accumulator %d\n", i);
		}
	}	
	printf("Spectrometer: Allocated %u accumulators (%.02f MB)\n", 
	  m_uNumMinFreeAccumulators, 
	  1.0*m_uNumMinFreeAccumulators*m_uNumChannels*sizeof(ACCUM_DATA_TYPE)/1e6);
	  
  // Initialize the mutex
  pthread_mutex_init(&m_mutex, NULL);

  // Spawn the disk writing thread
  printf("Spectrometer: Creating file thread...\n");
  m_bThreadIsReady = false;
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
	  
  // Disconnect from digitizer
  if (m_pDigitizer) {
    m_pDigitizer->setCallback(NULL);
  }

  // Disconnect from channelizer
  if (m_pChannelizer) {

    // Wait for the channelizer to send us any data it is still processing
    //m_pChannelizer->waitForEmpty();  

    // Disconnect
    m_pChannelizer->setCallback(NULL);
  }
   
  printf("Spectrometer: Maximum number of accumulators used: %d of %d\n", 
  m_uNumAccumulators - m_uNumMinFreeAccumulators, m_uNumAccumulators);
  
	// Free accumulators
	list<Accumulator*>::iterator it;

	for (it = m_empty.begin(); it != m_empty.end(); it++) {
		free(*it);
	}
	
	for (it = m_receive.begin(); it != m_receive.end(); it++) {
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

  if ((m_pDigitizer == NULL) || (m_pChannelizer == NULL)) {
    printf("Spectrometer: No digitizer or channelizer object at start of run.  End.\n");
    return;
  }
 
  // Reset the state
  m_bLocalStop = false;
  m_uNumSamplesSoFar = 0;  
  moveAllToEmpty();
  moveToReceive();
  
  TimeKeeper tk;
  tk.setNow();
  m_receive.front()->setStartTime(tk);
  
  // Take samples endlessly (stop signal is handled in threadloop function)
  m_pDigitizer->acquire();
  
  // Stop gracefully
  m_pChannelizer->waitForEmpty();

  printf("Spectrometer: Done.\n");

} // run()


// ----------------------------------------------------------------------------
// handleLivePlot() - Handle output for live plotting if needed
// ----------------------------------------------------------------------------
bool SpectrometerSimple::handleLivePlot(Accumulator* pAccum, Timer& plotTimer) {

  // Should we be plotting?
  if (!m_pController->plot()) {
  
    // If not, make sure the timer is stopped
    plotTimer.reset();
    return false;
  }

  // Plot if the controller just told us to start or if we've passed the
  // update interval
  if (!plotTimer.running() || (plotTimer.toc() > m_dPlotIntervalSeconds)) {
      
    // Restart the plot timer
    plotTimer.tic();
    
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
  Accumulator* pAccum = NULL;
  Timer totalRunTimer;
  Timer writeTimer;
  Timer plotScheduleTimer;
  Timer plotFileTimer;
  TimeKeeper tk;
  bool bPlot = false;
  unsigned long uCycle = 0;
  unsigned long uCycleDrops = 0;
  double duration = 0;
   
  // Report ready
  pSpec->threadIsReady();

  // Loop until a stop signal is received
  totalRunTimer.tic();
  plotScheduleTimer.tic();
  printf("\n");
  
  while (!pSpec->isStop(uCycle, totalRunTimer)) {

	  // If there are accumulations to write
		if (!pSpec->m_write.empty()) {
		
      // Normalize ADCmin and ADCmax:  we divide adcmin and adcmax by 2 here to  
      // be backwards compatible with pxspec.  This limits adcmin and adcmax to 
      // +/- 0.5 rather than +/-1.0
      pAccum = pSpec->m_write.front();
      pAccum->setADCmin(pAccum->getADCmin()/2);
      pAccum->setADCmax(pAccum->getADCmax()/2);  
      
      // Get some metrics
      duration = pAccum->getStopTime() - pAccum->getStartTime();      
      uCycleDrops = pAccum->getDrops(); 
		  uCycle++;     
		  tk.setNow(); 
      
      printf("----------------------------------------------------------------------\n");
      printf("Spectrometer: Writing cycle %lu at %s\n", 
        uCycle, tk.getDateTimeString(5).c_str());
      printf("Spectrometer: Total run time so far: %.02f seconds (%.3g days)\n", 
        totalRunTimer.toc(), totalRunTimer.toc()/3600.0/24);   
      printf("----------------------------------------------------------------------\n");  
      
   			// Write to disk (creates new file/path if needed)
			writeTimer.tic();
			pSpec->writeToFile(pAccum);
			writeTimer.toc();
			
			// Write live plot file needed
			plotFileTimer.tic();
			bPlot = pSpec->handleLivePlot(pAccum, plotScheduleTimer);
			plotFileTimer.toc();   
      
      printf("Spectrometer: Duty cycle = %6.3f over %.03f seconds\n", 
        pSpec->m_dAccumulationTime / duration, duration);

      //printf("Spectrometer: startTime %s and stop time %s\n", 
      //  pAccum->getStartTime().getDateTimeString(6).c_str(), 
      //  pAccum->getStopTime().getDateTimeString(6).c_str() );
              
      if (uCycleDrops > 0)
      {	
        printf("Spectrometer: Drop fraction = " RED "%6.3f\n" RESET, 
          1.0 * uCycleDrops / (pSpec->m_uNumSamplesPerAccumulation + uCycleDrops));
      } else {
        printf("Spectrometer: Drop fraction = %6.3f\n", 
          1.0 * uCycleDrops / (pSpec->m_uNumSamplesPerAccumulation + uCycleDrops));
      }     
      
      printf("Spectrometer: acdmin = %6.3f,  adcmax = %6.3f\n", 
        pAccum->getADCmin(), pAccum->getADCmax());
    		
    	if (bPlot) {
        printf("Spectrometer: Write time = %.04f seconds (plus %.04f for plot file)\n", 
          writeTimer.get(), plotFileTimer.get());
      } else {
        printf("Spectrometer: Write time = %0.4f seconds\n", writeTimer.get());      
      } 
            
      printf("\n");
        				
		  // Release the accumulator back to the empty pool
		  pSpec->moveToEmpty();
		
    } else {

      // Sleep before trying again
      usleep(SPEC_SLEEP_MICROSECONDS);
    }
  }
 
  // Print closing info
  printf("Spectrometer: Stopping...\n");
  printf("Spectrometer: Total cycles: %lu\n", uCycle);
  printf("Spectrometer: Total run time: %.02f seconds (%.3g days)\n", totalRunTimer.toc(), totalRunTimer.toc()/3600.0/24); 
 
  pSpec->m_pDigitizer->stop();
   
  // Exit the thread
  pthread_exit(NULL);

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
    
    double dStepFreq = (pAccum->getStopFreq() - 
      pAccum->getStartFreq()) / (double) pAccum->getDataLength();
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
  while ( (uIndex + m_uNumFFT) <= uBufferLength ) {
                     
    // Try to add to the channelizer buffer
    if (m_pChannelizer->push(&(pBuffer[uIndex]), m_uNumFFT, dScale, dOffset)) {
     
//printf("OnDigitizer... Added samples associated with accumulator %u\n", m_receive.back()->getId());
      
      m_uNumSamplesSoFar += m_uNumFFT;
      uAdded++;
      
    }  else {
      
      printf("Spectrometer::OnDigitizerData: Failed to push to channelizer!\n");
      
      // Record the dropped samples (drops are recorded to the freshest
      // accummulator in the receive queue).  
      m_receive.back()->addDrops(m_uNumFFT);
    } 
    
    // Time for a new accumulator?
    if (m_uNumSamplesSoFar == m_uNumSamplesPerAccumulation) {
                     
      // Save the stop time   
      if (!m_receive.empty()) {
        m_receive.back()->setStopTime();
      }
      
      // Add a new accumulator to receive queue for writing future samples
      Accumulator* pAccum = moveToReceive();
      if (pAccum) {
      
        // Set the start time in the accumlator
        pAccum->setStartTime();
        
      } else {        
      
        printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
        printf("Spectrometer::OnDigitizerData: Failed to get fresh accumulator!\n");   
        printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");    
      
      }
      
      // Reset the sample counter
      m_uNumSamplesSoFar = 0;   
        
    } // if uNumSmamplesSoFar
    
    // Increment the index to the next chunk of transferred data
    uIndex += m_uNumFFT;    
    
  } // while
  
  // Return the total number of transferred samples that were successfully
  // entered in the FFTPool buffer for processing.
  return uAdded * m_uNumFFT;

} // onDigitizerData()




// ----------------------------------------------------------------------------
// onChannelizerData() -- Do something with a spectrum returned from the 
//                        Channelizer
// ----------------------------------------------------------------------------
void SpectrometerSimple::onChannelizerData(ChannelizerData* pData) 
{  
  // Add data to the oldest accumuator in the receive queue until we've received
  // enough specra to fill the accumulator, then move the accumlator to the
  // write queue.  We don't need to worry about getting a new accumulator for 
  // receiving because that happens based on how many samples we've received 
  // from the digitizer in onDigitizerData().
   
	if (!m_receive.empty()) {
	
//	  printf("onChannelizer... Adding to accumulator %u\n", m_receive.front()->getId());
	  
	  if (m_receive.front()->add( pData->pData, 
                                pData->uNumChannels, 
                                pData->dADCmin, 
                                pData->dADCmax ) >= m_uNumSpectraPerAccumulation ) {
	    moveToWrite();
	  }
	}		  
} // onChannelizerData()




// ----------------------------------------------------------------------------
// moveToWrite
// -- Returns a pointer to the moved accumulator (null if no action)
// ----------------------------------------------------------------------------
Accumulator* SpectrometerSimple::moveToWrite() 
{
  Accumulator* pAccum = NULL;
  
  pthread_mutex_lock(&m_mutex);	
    
  if (!m_receive.empty()) 
  {
  	pAccum = m_receive.front();  
  	m_receive.pop_front();	
  	m_write.push_back(pAccum);
  }
  
  pthread_mutex_unlock(&m_mutex);	
    
  return pAccum;
}
	
	
// ----------------------------------------------------------------------------
// moveToReceive()
// -- Returns a pointer to the moved accumulator (null if no action)
// ----------------------------------------------------------------------------
Accumulator* SpectrometerSimple::moveToReceive() 
{
	Accumulator* pAccum = NULL;
  
  pthread_mutex_lock(&m_mutex);	
    
  if (!m_empty.empty()) 
  {
  	pAccum = m_empty.front();
  	m_empty.pop_front();
  	m_receive.push_back(pAccum);
  	
  	if (m_empty.size() < m_uNumMinFreeAccumulators) {
  	  m_uNumMinFreeAccumulators = m_empty.size();
  	}  	
  } 
  
  pthread_mutex_unlock(&m_mutex);	 
  
  return pAccum; 
}
	

// ----------------------------------------------------------------------------
// moveToEmpty()
// -- Returns a pointer to the moved accumulator (null if no action)
// ----------------------------------------------------------------------------
Accumulator* SpectrometerSimple::moveToEmpty() 
{
	Accumulator* pAccum = NULL;
  
  pthread_mutex_lock(&m_mutex);	
    
  if (!m_write.empty()) 
  {
  	pAccum = m_write.front();
  	pAccum->clear();
  	m_write.pop_front();
  	m_empty.push_back(pAccum);
  }
  
  pthread_mutex_unlock(&m_mutex);	  
  
  return pAccum;
}

// ----------------------------------------------------------------------------
// moveAllToEmpty()
// ----------------------------------------------------------------------------
void SpectrometerSimple::moveAllToEmpty()
{
  pthread_mutex_lock(&m_mutex);	
  
	list<Accumulator*>::iterator it;  
	for (it = m_receive.begin(); it != m_receive.end(); it++) {
		(*it)->clear();
		m_empty.push_back(*it);
	}
	m_receive.clear();
	
	for (it = m_write.begin(); it != m_write.end(); it++) {
		(*it)->clear();
		m_empty.push_back(*it);
	}
	m_write.clear();	
	
  pthread_mutex_unlock(&m_mutex);	
}  

	
