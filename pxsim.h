#ifndef _PXSIM_H_
#define _PXSIM_H_

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



// ---------------------------------------------------------------------------
//
// PXSim
//
// Simulates the PXBoard digitizer class without the dependencies and without
// connecting to an actual digitizer board.  Provides samples that are 
// uniform random (**not gaussian**).  Mostly just for rough approximation.
// Generating better random numbers was too slow.
//
// Use:
// #define SAMPLE_DATA_TYPE unsigned short
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
    double                      m_dVoltageOffset;
    unsigned long               m_uSamplesPerAccumulation; 
    unsigned int                m_uSamplesPerTransfer;
    SAMPLE_DATA_TYPE*           m_pBuffer; 
    DigitizerReceiver*          m_pReceiver;
    bool                        m_bStop;
    double                      m_dScale;
    double                      m_dOffset;

  public:

    // Constructor and destructor
    PXSim( double dAcquisitionRate, 
           unsigned long uSamplesPerAccumulation, 
           unsigned int uSamplesPerTransfer ) {
      m_bStop = false;
      m_dCWFreq1 = 0;
      m_dCWAmp1 = 0;
      m_dCWFreq2 = 0;
      m_dCWAmp2 = 0;   
      m_dNoiseAmp = 0;   
      m_dVoltageOffset = 0;
      m_dScale =  1.0/32768;
      m_dOffset = -1.0;
      m_dAcquisitionRate = dAcquisitionRate;     // MS/s
      m_uSamplesPerAccumulation = uSamplesPerAccumulation;
      m_uSamplesPerTransfer = uSamplesPerTransfer;  
      printf("Using SIMULATED digitizer\n");
      
      m_pBuffer = (SAMPLE_DATA_TYPE*) malloc(m_uSamplesPerTransfer * sizeof(SAMPLE_DATA_TYPE));
      m_pReceiver = NULL;
    }

    ~PXSim() { 
      if (m_pBuffer) {
        free(m_pBuffer);
        m_pBuffer = NULL;
      }
    }
  

    bool acquire() {

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
      unsigned long uRandom = 0;
      unsigned short* pPointer = 0; // used to divide 64 bit uRandom into 4 x 16 bit components
      float cw[4];     
      float dCW1 = 2.0 * M_PI * m_dCWFreq1 / m_dAcquisitionRate;
      float dCW2 = 2.0 * M_PI * m_dCWFreq2 / m_dAcquisitionRate;
            
      while ((uNumSamples < m_uSamplesPerAccumulation) && !m_bStop) {

        // For each transfer, populate the buffer
        for (unsigned int i=0; i<m_uSamplesPerTransfer; i+=4) {

          // Calculate four voltage samples of two continuous waves
          cw[0]  = m_dCWAmp1 * sin(dCW1 * uSampleIndex);
          cw[0] += m_dCWAmp2 * sin(dCW2 * uSampleIndex++);
          cw[1]  = m_dCWAmp1 * sin(dCW1 * uSampleIndex);
          cw[1] += m_dCWAmp2 * sin(dCW2 * uSampleIndex++);
          cw[2]  = m_dCWAmp1 * sin(dCW1 * uSampleIndex);
          cw[2] += m_dCWAmp2 * sin(dCW2 * uSampleIndex++);
          cw[3]  = m_dCWAmp1 * sin(dCW1 * uSampleIndex);
          cw[3] += m_dCWAmp2 * sin(dCW2 * uSampleIndex++);
          
          // Add the noise and offset contributions to each voltage sample
          uRandom = xorshf96(); 
          pPointer = (unsigned short*) &uRandom;
          cw[0] += m_dNoiseAmp * (pPointer[0] / 32768.0 - 1) + m_dVoltageOffset;
          cw[1] += m_dNoiseAmp * (pPointer[1] / 32768.0 - 1) + m_dVoltageOffset;
          cw[2] += m_dNoiseAmp * (pPointer[2] / 32768.0 - 1) + m_dVoltageOffset;
          cw[3] += m_dNoiseAmp * (pPointer[3] / 32768.0 - 1) + m_dVoltageOffset;
                    
          /// Convert from voltages to PX digitizer data units
          cw[0] = (cw[0] - m_dOffset) / m_dScale;
          cw[1] = (cw[1] - m_dOffset) / m_dScale;
          cw[2] = (cw[2] - m_dOffset) / m_dScale;
          cw[3] = (cw[3] - m_dOffset) / m_dScale;
          
          // Store in buffer.  This implicitly casts to SAMPLE_DATA_TYPE
          m_pBuffer[i] = cw[0];
          m_pBuffer[i+1] = cw[1];
          m_pBuffer[i+2] = cw[2];
          m_pBuffer[i+3] = cw[3];
          
        }

        // Wait until enough time has passed that the samples would have been 
        // acquired if we were actually taking the data
        if (timer.toc() < dTransferTime) {
            usleep( (dTransferTime - timer.get()) * 1e6 );
        }

        printf("Actual transfer time: %8.6f, Desired transfer time: %8.6f, Diff: %8.6f\n", 
          timer.get(), dTransferTime, (dTransferTime-timer.get())*1.0e6);

        // Reset the timer
        timer.tic();

        // Call the callback function to process the chunk of data
        uNumSamples += m_pReceiver->onDigitizerData(m_pBuffer, m_uSamplesPerTransfer, uNumSamples, m_dScale, m_dOffset);

      } // transfer loop

      return true;
    }

    bool connect(unsigned int) {      
      
      if (m_pBuffer == NULL) { return false;}
      
      return true;
    }

    void disconnect() { }

    void setSignal(double dFreq1, double dAmp1, double dFreq2, double dAmp2, double dNoiseAmp, double dVoltageOffset) { 
      m_dCWFreq1 = dFreq1; 
      m_dCWAmp1 = dAmp1;
      m_dCWFreq2 = dFreq2;
      m_dCWAmp2 = dAmp2;
      m_dNoiseAmp = dNoiseAmp;
      m_dVoltageOffset = dVoltageOffset;

      printf("PXSim: Digitized voltage simulation parameters:\n");
      printf("PXSim: Continuous wave 1: amplitude %g at %g MHz\n", m_dCWAmp1, m_dCWFreq1);
      printf("PXSim: Continuous wave 2: amplitude %g at %g MHz\n", m_dCWAmp2, m_dCWFreq2);
      printf("PXSim: Uniform (not Gaussian) noise: amplitude %g\n", m_dNoiseAmp);
      printf("PXSim: Constant offset %g\n", m_dVoltageOffset);
    }

    void setCallback(DigitizerReceiver* pReceiver) { m_pReceiver = pReceiver; }

    void stop() { m_bStop = true; }
    
    double scale() { return m_dScale; }
    
    double offset() { return m_dOffset; }
    
    unsigned int bytesPerSample() { return 2; }
    
    Digitizer::DataType type() { return Digitizer::DataType::uint16; }

};



#endif // _PXSIM_H_
