#ifndef _PXSIM_H_
#define _PXSIM_H_

#include <cstdlib>
#include <ctime>
#include <functional>
#include <stdio.h> // printf
#include <unistd.h> // usleep
#include "digitizer.h"
#include "timing.h"


// Fast random number generation
static unsigned long xr=123456789;
static unsigned long yr=362436069;
static unsigned long zr=521288629;

unsigned long xorshf96() { //period 2^96-1
  unsigned long tr;
  xr ^= xr << 16;
  xr ^= xr >> 5;
  xr ^= xr << 1;
  tr = xr;
  xr = yr;
  yr = zr;
  zr = tr ^ xr ^ yr;
  return zr;
}

/*
double rand_normal(float mean, float stddev)
{   
    //Box muller method
    static float n2 = 0.0;
    static int n2_cached = 0;
    if (!n2_cached)
    {
        float x, y, r;
        do
        {
            x = 2.0*rand()/RAND_MAX - 1;
            y = 2.0*rand()/RAND_MAX - 1;

            r = x*x + y*y;
        }
        while (r == 0.0 || r > 1.0);
        
        float d = sqrt(-2.0*log(r)/r);
        float n1 = x*d;
        n2 = y*d;
        float result = n1*stddev + mean;
        n2_cached = 1;
        return result;
        
    }
    else
    {
        n2_cached = 0;
        return n2*stddev + mean;
    }
}
*/

// ---------------------------------------------------------------------------
//
// PXSim
//
// Simulates the PXBoard digitizer class without the dependencies and without
// connecting to an actual digitizer board.  Provides samples that are 
// uniform random (**not gaussian**).  Mostly just for rough approximation.
// Generating better random numbers was too slow.
//
// ---------------------------------------------------------------------------
class PXSim : public Digitizer {

  private:

    // Member variables
    double                      m_dAcquisitionRate;     // MHz
    unsigned int                m_uSamplesPerTransfer;
    unsigned short*             m_pBuffer; 
    DigitizerReceiver*          m_pReceiver;
    bool                        m_bStop;

  public:

    // Constructor and destructor
    PXSim() {
      m_bStop = false;
      m_dAcquisitionRate = 0;     
      m_uSamplesPerTransfer = 0;  
      m_pBuffer = NULL;
      m_pReceiver = NULL;
      printf("Using SIMULATED digitizer\n");
    }

    ~PXSim() { 
      if (m_pBuffer) {
        free(m_pBuffer);
        m_pBuffer = NULL;
      }
    }
  

    bool acquire(unsigned long uNumSamplesToTransfer) {

      unsigned long uNumSamples = 0;
      Timer timer;
      double dTransferTime = m_uSamplesPerTransfer / m_dAcquisitionRate / 1.0e6;
      printf("Transfer time: %10.10f\n", dTransferTime);

      // Reset the stop flag
      m_bStop = false;

      if (m_pBuffer == NULL) {
        printf("No transfer buffers at start of run. Was PXSim::setTransferSamples() called?");
        return false;
      }

      if (m_dAcquisitionRate == 0) {
        printf("No transfer buffers at start of run.  Was PXSim::setAcquisitionRate() called?");
        return false;
      }

      // Init random number seed
      // std::srand(std::time(0));

      // Populate the buffer with new random numbers
      // for (unsigned int i=0; i<m_uSamplesPerTransfer; i++) {
      //  m_pBuffer[i] = std::rand() & 0xFFFF;
      //}

      // Start the timer
      timer.tic();

      // Main recording loop - The main idea here is that we're alternating
      // between two halves of our buffer. While we're transferring
      // fresh acquisition data to one half, we process the other half.

      // Loop over number of transfers requested
      while ((uNumSamples < uNumSamplesToTransfer) && !m_bStop) {

        // Populate the buffer with new random numbers
        for (unsigned int i=0; i<m_uSamplesPerTransfer; i+=4) {
            unsigned long uRandom = xorshf96();
            unsigned short* pPointer = (unsigned short*) &uRandom;
            m_pBuffer[i] = (pPointer[0] & 0x7FFF) + 16384;
            m_pBuffer[i+1] = (pPointer[1] & 0x7FFF) + 16384;
            m_pBuffer[i+2] = (pPointer[2] & 0x7FFF) + 16384;
            m_pBuffer[i+3] = (pPointer[3] & 0x7FFF) + 16384;

            //if(i==0){
            //    printf("RAN: %d, %d, %d, %d\n", pPointer[0], pPointer[1], pPointer[2], pPointer[3]);
            //    printf("ADC: %d, %d, %d, %d\n", m_pBuffer[i], m_pBuffer[i+1], m_pBuffer[i+2], m_pBuffer[i+3]);
            //}
        }

        // Wait until enough time has passed that the samples would have been 
        // acquired if we were actually taking the data
        if (timer.toc() < dTransferTime) {
            usleep( (dTransferTime - timer.get()) * 1e6 );
        }

        //printf("Toc: %8.6f, Transfer time: %8.6f, Diff: %8.6f\n", timer.get(), dTransferTime, (dTransferTime-timer.get())*1.0e6);


        // Reset the timer
        timer.tic();

        // Call the callback function to process the chunk of data
        uNumSamples += m_pReceiver->onDigitizerData(m_pBuffer, m_uSamplesPerTransfer, uNumSamples);

      } // transfer loop

      return true;
    }

    bool connect(unsigned int) { return true; }

    void disconnect() { }

    bool setAcquisitionRate(double dRate ) { m_dAcquisitionRate = dRate; return true; }

    bool setInputChannel(unsigned int) { return true; }

    bool setVoltageRange(unsigned int, unsigned int) { return true; }

    bool setTransferSamples(unsigned int uSamples) { 
      m_uSamplesPerTransfer = uSamples;    
      if (m_pBuffer) free(m_pBuffer);
      m_pBuffer = (unsigned short*) malloc(m_uSamplesPerTransfer * sizeof(unsigned short));
      return true;
    }

    void setCallback(DigitizerReceiver* pReceiver) { m_pReceiver = pReceiver; }

    void stop() { m_bStop = true; }

};



#endif // _PXSIM_H_
