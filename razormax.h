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
// ---------------------------------------------------------------------------
class RazorMax : public Digitizer {

  private:

    // Member variables
    unsigned int                m_uAcquisitionRate;
    unsigned int                m_uInputChannel;
    unsigned int                m_uVoltageRange;
    unsigned int                m_uSamplesPerTransfer;
    unsigned int                m_uBytesPerTransfer;
    unsigned int                m_uSegmentTail_Bytes;
    unsigned int                m_u32BoardIndex; 
    //unsigned int                m_u32BufferSizeBytes;
    //unsigned int                m_u32TransferSizeSamples; // The actual number of samples after padding the buffer to DMA boundary
    CSHANDLE                    m_hBoard;
    void*                       m_pBuffer1; 
    void*                       m_pBuffer2;
    DigitizerReceiver*          m_pReceiver;
    bool                        m_bStop;

    // Diagnostic information functions
    bool printAcquisitionConfig();
    bool printChannelConfig(unsigned int);
    bool printTriggerConfig(unsigned int);
    bool printBoardInfo();

  public:

    // Constructor and destructor
    RazorMax();
    ~RazorMax();

    // Setup functions    
    bool connect(unsigned int);
    void disconnect();

    bool setAcquisitionRate(unsigned int);
    bool setInputChannel(unsigned int);
    bool setVoltageRange(unsigned int);
    bool setTransferSamples(unsigned int);

    void setCallback(DigitizerReceiver*);

    // Main execution function
    bool acquire(unsigned long);
    void stop();

};



#endif // _RAZORMAX_H_
