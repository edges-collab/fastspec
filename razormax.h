#ifndef _RAZORMAX_H_
#define _RAZORMAX_H_

#include <functional>
#include "digitizer.h"

#include <CsTypes.h>

// ---------------------------------------------------------------------------
//
// RazorMax
//
// Wrapper class that exposes basic functionality for a PX14000 digitizer
// board.  The principal method to retrieve data is "acquire", which utilizes
// a callback function set using "setCallback".
//
// Use:
// #define SAMPLE_DATA_TYPE short
//
// ---------------------------------------------------------------------------
class RazorMax : public Digitizer {

  private:

    // Member variables
    unsigned int                m_uAcquisitionRate;
    unsigned long               m_uSamplesPerAccumulation;
    unsigned int                m_uSamplesPerTransfer;
    unsigned int                m_uBytesPerTransfer;
    unsigned int                m_uEffectiveSamplesPerTransfer;
    unsigned int                m_uInputChannel;
    unsigned int                m_u32BoardIndex; 
    CSHANDLE                    m_hBoard;
    void*                       m_pBuffer1; 
    void*                       m_pBuffer2;
    DigitizerReceiver*          m_pReceiver;
    bool                        m_bStop;
    double                      m_dScale;
    double                      m_dOffset;

    // Diagnostic information functions
    bool printAcquisitionConfig();
    bool printChannelConfig(unsigned int);
    bool printTriggerConfig(unsigned int);
    bool printBoardInfo();

  public:

    // Constructor and destructor
    RazorMax(double, unsigned long, unsigned int);
    ~RazorMax();

    // Setup functions    
    bool connect(unsigned int);
    void disconnect();

    //bool setAcquisitionRate(unsigned int);
    //bool setInputChannel(unsigned int);
    //bool setVoltageRange(unsigned int);
    //bool setTransferSamples(unsigned int);

    void setCallback(DigitizerReceiver*);

    // Main execution function
    bool acquire();
    void stop();

};



#endif // _RAZORMAX_H_
