
#include "spectrometer.h"
#include "timing.h"
#include "utility.h"
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
Spectrometer::Spectrometer(unsigned long uNumChannels, 
                           unsigned long uNumSamplesPerAccumulation, 
                           double dBandwidth,
                           double dSwitchDelayTime,
                           Digitizer* pDigitizer,
                           Channelizer* pChannelizer,
                           Switch* pSwitch,
                           Controller* pController)
{

  // User specified configuration
  m_uNumChannels = uNumChannels;
  m_uNumSamplesPerAccumulation = uNumSamplesPerAccumulation;
  m_dSwitchDelayTime = dSwitchDelayTime;
  m_dBandwidth = dBandwidth;

  // Derived configuration
  m_dChannelSize = m_dBandwidth / (double) m_uNumChannels; // MHz
  m_dAccumulationTime = (double) uNumSamplesPerAccumulation / m_dBandwidth / 2.0; // seconds
  m_dStartFreq = 0.0;
  m_dStopFreq = m_dBandwidth;
  m_dChannelFactor = 4; // because of Blackmann Harris (should really be closer to 3)

  // Assign the Spectrometer instance's process function to the Digitizer
  // callback on data transfer.
  m_pDigitizer = pDigitizer;
  m_pDigitizer->setCallback(this);

  // Remember the receiver switch controller
  m_pSwitch = pSwitch;

  // Remember the controller
  m_pController = pController;

  // Initialize the Accumulators
  m_accumAntenna.init( m_uNumChannels, m_dStartFreq, 
                   m_dStopFreq, m_dChannelFactor );
  m_accumAmbientLoad.init( m_uNumChannels, m_dStartFreq, 
                   m_dStopFreq, m_dChannelFactor );
  m_accumHotLoad.init( m_uNumChannels, m_dStartFreq, 
                   m_dStopFreq, m_dChannelFactor );
  
  m_pCurrentAccum = NULL;

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

} // constructor



// ----------------------------------------------------------------------------
// Destructor
// ----------------------------------------------------------------------------
Spectrometer::~Spectrometer()
{
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

} // destructor



// ----------------------------------------------------------------------------
// setOutput
// ----------------------------------------------------------------------------
void Spectrometer::setOutput(const string& sOutput, const string& sInstrument, bool bDirectory)
{
  printf("Spectrometer: Will write output to %s\n", sOutput.c_str());

  m_sOutput = sOutput;
  m_sInstrument = sInstrument;
  m_bDirectory = bDirectory;
}



// ----------------------------------------------------------------------------
// getFileName
// ----------------------------------------------------------------------------
string Spectrometer::getFileName()
{

  string sSuffix = ".acq";

  if (m_bDirectory)
  {
    TimeKeeper startTime = m_accumAntenna.getStartTime();
    return construct_filepath_name(m_sOutput, startTime.getFileString(2), m_sInstrument, sSuffix);

  } else {
    return m_sOutput + sSuffix;
  }
}


// ----------------------------------------------------------------------------
// run() - Main executable function for the EDGES spectrometer
// ----------------------------------------------------------------------------
void Spectrometer::run()
{ 
  m_bLocalStop = false;
  Timer totalRunTimer;
  Timer dutyCycleTimer;
  Timer writeTimer;
  TimeKeeper tk;
  double dDutyCycle_Overall;
  unsigned long uCycleDrops = 0;
  unsigned long uCycle = 0;

  if ((m_pDigitizer == NULL) || (m_pChannelizer == NULL) || (m_pSwitch == NULL)) {
    printf("Spectrometer: No digitizer, channelizer, or switch object at start of run.  End.\n");
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


    // Cycle between switch states
    for (unsigned int i=0; i<3; i++) {

      // Check for abort signal
      if (isAbort()) { 
        printf("Spectrometer: Aborting -- Waiting for channelizer to finish...");
        m_pChannelizer->waitForEmpty();
        printf("  Done.\n");
        return; 
      }

      tk.setNow();
      printf("Spectrometer: Starting switch state %d at %s\n", i, tk.getDateTimeString(5).c_str());

      // Change receiver switch state and pause briefly for it to take effect
      m_pSwitch->set(i);
      usleep((unsigned int) (m_dSwitchDelayTime * 1e6)); 

      // Wait for any previous channelizer processes still running to finish
      m_pChannelizer->waitForEmpty();

      // Reset the accumulator and record the start time
      switch(i) {
        case 0:
          m_pCurrentAccum = &m_accumAntenna;
          break;
        case 1:
          m_pCurrentAccum = &m_accumAmbientLoad;
          break;
        case 2:
          m_pCurrentAccum = &m_accumHotLoad;
          break;
      }

      m_pCurrentAccum->clear();
      m_pCurrentAccum->setStartTime();

      // Acquire data
      m_pDigitizer->acquire(m_uNumSamplesPerAccumulation);

      // Note the stop time
      m_pCurrentAccum->setStopTime();
    }

    // Wait for any remaining channelizer processes to finish
    m_pChannelizer->waitForEmpty();

    // Normalize ADCmin and ADCmax:  we divide adcmin and adcmax by 2 here to  
    // be backwards compatible with pxspec.  This limits adcmin and adcmax to 
    // +/- 0.5 rather than +/-1.0
    m_accumAntenna.setADCmin(m_accumAntenna.getADCmin()/2);
    m_accumAntenna.setADCmax(m_accumAntenna.getADCmax()/2);
    m_accumAmbientLoad.setADCmin(m_accumAmbientLoad.getADCmin()/2);
    m_accumAmbientLoad.setADCmax(m_accumAmbientLoad.getADCmax()/2);
    m_accumHotLoad.setADCmin(m_accumHotLoad.getADCmin()/2);
    m_accumHotLoad.setADCmax(m_accumHotLoad.getADCmax()/2);
    
    // Write to ACQ
    writeTimer.tic();;
    printf("\nSpectrometer: Writing to file: %s\n", getFileName().c_str());
    write_switch_cycle(getFileName(), m_accumAntenna, m_accumAmbientLoad, m_accumHotLoad);      
    //writeTimer.toc();

    // Write a file for plotting
    if (m_pController->plot()) {
      printf("Spectrometer: Writing to plot file: /tmp/fastspec.dat\n");
      write_for_plot(m_pController->plotPath(), m_accumAntenna, m_accumAmbientLoad, m_accumHotLoad, 4);
      m_pController->updatePlotter();
    }
    writeTimer.toc();

    // Calculate overall duty cycle
    dutyCycleTimer.toc();
    dDutyCycle_Overall = 3.0 * m_uNumSamplesPerAccumulation / (2.0 * 1e6 * m_dBandwidth) / dutyCycleTimer.get();
    uCycleDrops = m_accumAntenna.getDrops() + m_accumAmbientLoad.getDrops() + m_accumHotLoad.getDrops();

    printf("\n");
    printf("Spectrometer: Cycle time             = %6.3f seconds\n", dutyCycleTimer.get());
    printf("Spectrometer: Accum time (ideal)     = %6.3f seconds\n", 3.0 * m_uNumSamplesPerAccumulation / (2.0 * 1e6 * m_dBandwidth));
    printf("Spectrometer: Switch time (ideal)    = %6.3f seconds\n", 3*m_dSwitchDelayTime);
    printf("Spectrometer: Write time             = %6.3f seconds\n", writeTimer.get());
    printf("Spectrometer: Duty cycle             = %6.3f\n", dDutyCycle_Overall);
    if (uCycleDrops>0) {
      printf("Spectrometer: Drop fraction          = " RED "%6.3f\n" RESET, 1.0 * uCycleDrops / (m_uNumSamplesPerAccumulation + uCycleDrops));
    } else {
      printf("Spectrometer: Drop fraction          = %6.3f\n", 1.0 * uCycleDrops / (m_uNumSamplesPerAccumulation + uCycleDrops));
    }
    printf("Spectrometer: p0 (antenna) -- acdmin = %6.3f,  adcmax = %6.3f\n", m_accumAntenna.getADCmin(), m_accumAntenna.getADCmax());
    printf("Spectrometer: p1 (ambient) -- acdmin = %6.3f,  adcmax = %6.3f\n", m_accumAmbientLoad.getADCmin(), m_accumAmbientLoad.getADCmax());
    printf("Spectrometer: p2 (hot)     -- acdmin = %6.3f,  adcmax = %6.3f\n", m_accumHotLoad.getADCmin(), m_accumHotLoad.getADCmax());
    printf("\n");

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
bool Spectrometer::isAbort() 
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
bool Spectrometer::isStop(unsigned long uCycle, Timer& ti) 
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
void Spectrometer::sendStop() 
{
  m_bLocalStop = true;
}


// ----------------------------------------------------------------------------
// setStopCycles()
// ----------------------------------------------------------------------------
void Spectrometer::setStopCycles(unsigned long uCycles) 
{
  m_bUseStopCycles = true;
  m_uStopCycles = uCycles;

  printf("Spectrometer: Will stop after %lu cycles.\n", uCycles);
}


// ----------------------------------------------------------------------------
// setStopSeconds()
// ----------------------------------------------------------------------------
void Spectrometer::setStopSeconds(double dSeconds) 
{
  m_bUseStopSeconds = true;
  m_dStopSeconds = dSeconds;

  printf("Spectrometer: Will stop after %.0f seconds (%.3g days).\n", dSeconds, dSeconds/24.0/3600.0);
}


// ----------------------------------------------------------------------------
// setStopTime()
// ----------------------------------------------------------------------------
void Spectrometer::setStopTime(const string& strDateTime) 
{
  m_bUseStopTime = true;
  m_tkStopTime.set(strDateTime);

  printf("Spectrometer: Will stop at %s.\n", m_tkStopTime.getDateTimeString(5).c_str());
}


// ----------------------------------------------------------------------------
// onDigitizerData() -- Do something with transferred data.  This function ignores
//                 any transferred data at the end of the transfer beyond the
//                 last integer multiple of m_uNumFFT.  If an Channelizer buffer 
//                 is not available for a given chunk of the transfer, it drops 
//                 that chunk of data.  Returns total number of samples 
//                 successfully processed into a Channelizer buffer.
// ----------------------------------------------------------------------------
unsigned long Spectrometer::onDigitizerData( unsigned short* pBuffer, 
                                             unsigned int uBufferLength,
                                             unsigned long uTransferredSoFar ) 
{
  unsigned int uIndex = 0;
  unsigned int uAdded = 0;
  unsigned int uDropped = 0;

  // Loop over the transferred data and enter it into the channelizer buffer
  while ( ((uIndex + m_uNumFFT) <= uBufferLength) 
         && (uTransferredSoFar < m_uNumSamplesPerAccumulation)) {
    

    // Try to add to the channelizer buffer
    if (m_pChannelizer->push(&(pBuffer[uIndex]), m_uNumFFT)) { 

      uTransferredSoFar += m_uNumFFT;
      uAdded++;

    } else {

      // No buffers were available so we'll drop the chunk and move on
      uDropped++;
    }

    // Increment the index to the next chunk of transferred data
    uIndex += m_uNumFFT;
  }

  // Keep a permanent record of how many samples were dropped
  m_pCurrentAccum->addDrops(uDropped * m_uNumFFT);

  //printf("onTransfer: added blocks here: %u, dropped blocks here: %u, total samples: %lu\n", uAdded, uDropped, uTransferredSoFar);

  // Return the total number of transferred samples that were successfully
  // entered in the FFTPool buffer for processing.
  return uAdded * m_uNumFFT;

} // onDigitizerData()




// ----------------------------------------------------------------------------
// onChannelizerData() -- Do something with a spectrum returned from the Channelizer
// ----------------------------------------------------------------------------
void Spectrometer::onChannelizerData(ChannelizerData* pData) 
{  
  m_pCurrentAccum->add(pData->pData, pData->uNumChannels, pData->dADCmin, pData->dADCmax);
} // onChannelizerData()
