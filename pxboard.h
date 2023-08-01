#ifndef _PXBOARD_H_
#define _PXBOARD_H_

#include <functional>
#include <px14.h>
#include "digitizer.h"


// ---------------------------------------------------------------------------
//
// PXBoard
//
// Wrapper class that exposes basic functionality for a PX14000 digitizer
// board.  The principal method to retrieve data is "acquire", which utilizes
// a callback function set using "setCallback".
//
// Use:
// #define SAMPLE_DATA_TYPE unsigned short
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
    unsigned long               m_uSamplesPerAccumulation;
    unsigned int                m_uBoardRevision;
    unsigned int                m_uSerialNumber;
    HPX14                       m_hBoard;
    px14_sample_t*              m_pBuffer1; 
    px14_sample_t*              m_pBuffer2;
    DigitizerReceiver*          m_pReceiver;
    bool                        m_bStop;
    double                      m_dScale;
    double                      m_dOffset;

    bool setAcquisitionRate();
    bool setInputChannel();
    bool setVoltageRange();
    bool setSamplesPerTransfer();
    
  public:

    // Constructor and destructor
    PXBoard(double, unsigned long, unsigned int, unsigned int, unsigned int);
    ~PXBoard();

    // Setup functions    
    bool connect(unsigned int);
    void disconnect();

    // Interface
    void setCallback(DigitizerReceiver*);
    bool acquire();
    void stop();

    // Description functions
    double scale();
    double offset();
    unsigned int bytesPerSample();
    Digitizer::DataType type();
    
};



#endif // _PXBOARD_H_
