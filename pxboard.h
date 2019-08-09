#ifndef _PXBOARD_H_
#define _PXBOARD_H_

#include <functional>
#include <px14.h>
#include "digitizer.h"

//#define pxboard_callback_fptr std::function<unsigned int(px14_sample_t*, unsigned int, unsigned long)>

// ---------------------------------------------------------------------------
//
// PXBoard
//
// Wrapper class that exposes basic functionality for a PX14000 digitizer
// board.  The principal method to retrieve data is "acquire", which utilizes
// a callback function set using "setCallback".
//
// ---------------------------------------------------------------------------
class PXBoard : public Digitizer {

  private:

    // Member variables
    double                      m_dAcquisitionRate;
    unsigned int                m_uInputChannel;
    unsigned int                m_uVoltageRange1;
    unsigned int                m_uVoltageRange2;
    unsigned int                m_uSamplesPerTransfer;
    unsigned int                m_uBoardRevision;
    unsigned int                m_uSerialNumber;
    HPX14                       m_hBoard;
    px14_sample_t*              m_pBuffer1; 
    px14_sample_t*              m_pBuffer2;
    DigitizerReceiver*          m_pReceiver;
    bool                        m_bStop;

  public:

    // Constructor and destructor
    PXBoard();
    ~PXBoard();

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



#endif // _PXBOARD_H_
