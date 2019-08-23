#ifndef _PXSIM_H_
#define _PXSIM_H_

#include <cstdlib>
#include <ctime>
#include <functional>
#include <math.h>
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
    double                      m_dCWFreq1;             // MHz
    double                      m_dCWAmp1;              // 0 to 1 
    double                      m_dCWFreq2;             // MHz
    double                      m_dCWAmp2;              // 0 to 1 
    double                      m_dNoiseAmp;            // 0 to 1   
    unsigned int                m_uSamplesPerTransfer;
    unsigned short*             m_pBuffer; 
    DigitizerReceiver*          m_pReceiver;
    bool                        m_bStop;

  public:

    // Constructor and destructor
    PXSim() {
      m_bStop = false;
      m_dCWFreq1 = 0;
      m_dCWAmp1 = 0;
      m_dCWFreq2 = 0;
      m_dCWAmp2 = 0;   
      m_dNoiseAmp = 0;   
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
        printf("PXSim: No transfer buffers at start of run. Was setTransferSamples() called?\n");
        return false;
      }

      if (m_dAcquisitionRate == 0) {
        printf("PXSim: No transfer buffers at start of run.  Was setAcquisitionRate() called?\n");
        return false;
      }

      if ((m_dCWAmp1 == 0) && (m_dCWAmp2 == 0) && (m_dNoiseAmp == 0)) {
        printf("PXSim: WARNING! No signal being generated. Was setSignal() called? Proceeding with null signal.\n");       
      }

      // Start the timer
      timer.tic();

      // Loop over number of transfers requested
      unsigned long long uSampleIndex = 0;  
      unsigned long uRandom;
      unsigned short* pPointer;
      double cw[4];
      double dCW1 = 2.0 * M_PI * m_dCWFreq1 / m_dAcquisitionRate;
      double dCW2 = 2.0 * M_PI * m_dCWFreq2 / m_dAcquisitionRate;

      while ((uNumSamples < uNumSamplesToTransfer) && !m_bStop) {

        // For each transfer, populate the buffer
        for (unsigned int i=0; i<m_uSamplesPerTransfer; i+=4) {

          /*cw[0]=0;cw[1]=0;cw[2]=0;cw[3]=0;
          for (double j=90.0+(0.001*m_counter); j<91; j+=0.1) {
            cw[0] += m_dCWAmp1 * sin(dCW1*uSampleIndex*j/m_dCWFreq1);
            cw[1] += m_dCWAmp1 * sin(dCW1*(uSampleIndex+1)*j/m_dCWFreq1);
            cw[2] += m_dCWAmp1 * sin(dCW1*(uSampleIndex+2)*j/m_dCWFreq1);
            cw[3] += m_dCWAmp1 * sin(dCW1*(uSampleIndex+3)*j/m_dCWFreq1);             
          }
          */

          // Calculate four samples of two continuous waves
          cw[0] = m_dCWAmp1 * sin(dCW1*uSampleIndex)     + m_dCWAmp2 * sin(dCW2*uSampleIndex);
          cw[1] = m_dCWAmp1 * sin(dCW1*(uSampleIndex+1)) + m_dCWAmp2 * sin(dCW2*(uSampleIndex+1));
          cw[2] = m_dCWAmp1 * sin(dCW1*(uSampleIndex+2)) + m_dCWAmp2 * sin(dCW2*(uSampleIndex+2));
          cw[3] = m_dCWAmp1 * sin(dCW1*(uSampleIndex+3)) + m_dCWAmp2 * sin(dCW2*(uSampleIndex+3)); 
          uSampleIndex +=4; 

          // Calculate four samples of gaussian noise
          uRandom = xorshf96(); 
          pPointer = (unsigned short*) &uRandom;
          m_pBuffer[i]   = 2.0 * m_dNoiseAmp * (pPointer[0] & 0x7FFF);
          m_pBuffer[i+1] = 2.0 * m_dNoiseAmp * (pPointer[1] & 0x7FFF);
          m_pBuffer[i+2] = 2.0 * m_dNoiseAmp * (pPointer[2] & 0x7FFF);
          m_pBuffer[i+3] = 2.0 * m_dNoiseAmp * (pPointer[3] & 0x7FFF);
          
          // Add the contributions together
          m_pBuffer[i]   += (1.0 - m_dNoiseAmp) * 32768.5 + 32768.0 * (cw[0]);
          m_pBuffer[i+1] += (1.0 - m_dNoiseAmp) * 32768.5 + 32768.0 * (cw[1]);
          m_pBuffer[i+2] += (1.0 - m_dNoiseAmp) * 32768.5 + 32768.0 * (cw[2]);
          m_pBuffer[i+3] += (1.0 - m_dNoiseAmp) * 32768.5 + 32768.0 * (cw[3]);
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

    void setSignal(double dFreq1, double dAmp1, double dFreq2, double dAmp2, double dNoiseAmp) { 
      m_dCWFreq1 = dFreq1; 
      m_dCWAmp1 = dAmp1;
      m_dCWFreq2 = dFreq2;
      m_dCWAmp2 = dAmp2;
      m_dNoiseAmp = dNoiseAmp;

      printf("PXSim: Continuous wave 1: amplitude %g at %g MHz\n", m_dCWAmp1, m_dCWFreq1);
      printf("PXSim: Continuous wave 2: amplitude %g at %g MHz\n", m_dCWAmp2, m_dCWFreq2);
      printf("PXSim: Uniform (not Gaussian) noise: amplitude %g\n", m_dNoiseAmp);
    }

    void setCallback(DigitizerReceiver* pReceiver) { m_pReceiver = pReceiver; }

    void stop() { m_bStop = true; }

};



#endif // _PXSIM_H_
