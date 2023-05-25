#ifndef _RAZORMAX_H_
#define _RAZORMAX_H_

#include <functional>
#include "digitizer.h"

#include <CsAppSupport.h>
#include "CsTchar.h"
#include "CsSdkMisc.h"
#include "CsSdkUtil.h"
#include "CsExpert.h"
#include <CsPrototypes.h>

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
    double                      m_dAcquisitionRate;
    unsigned int                m_uInputChannel;
    unsigned int                m_uVoltageRange1;
    unsigned int                m_uVoltageRange2;
    unsigned int                m_uSamplesPerTransfer;
    unsigned int                m_uBoardRevision;
    unsigned int                m_uSerialNumber;
    CSHANDLE                    m_hBoard;
    void*                       m_pBuffer1; 
    void*                       m_pBuffer2;
    DigitizerReceiver*          m_pReceiver;
    bool                        m_bStop;

  public:

    // Constructor and destructor
    RazorMax();
    ~RazorMax();

    // Setup functions    
    bool connect(unsigned int);
    void disconnect();

    bool setAcquisitionRate(double);
    bool setInputChannel(unsigned int);
    bool setVoltageRange(unsigned int, unsigned int);
    bool setTransferSamples(unsigned int);

    void setCallback(DigitizerReceiver*);

    // Main execution function
    bool acquire(unsigned long);
    void stop();

};



#endif // _RAZORMAX_H_
