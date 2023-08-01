#ifndef _SPECTROMETER_H_
#define _SPECTROMETER_H_

#include <string>
#include <functional>
#include "accumulator.h"
#include "digitizer.h"
#include "channelizer.h"
#include "dumper.h"
#include "controller.h"
#include "switch.h"
#include "timing.h"

#ifndef SAMPLE_DATA_TYPE
  #error Aborted in spectrometer.h because SAMPLE_DATA_TYPE was not defined.
#endif

// ---------------------------------------------------------------------------
//
// SPECTROMETER
//
// Uses PXBoard, Switch, and FFTPool objects to control and acquire data from
// the EDGES system.
//
// ---------------------------------------------------------------------------
class Spectrometer : public DigitizerReceiver, ChannelizerReceiver {

  private:

    // Member variables
    Digitizer*      m_pDigitizer;
    Channelizer*    m_pChannelizer;
    Dumper*         m_pDumper;
    Switch*         m_pSwitch;
    Controller*     m_pController;    
    Accumulator     m_accumAntenna;
    Accumulator     m_accumAmbientLoad;
    Accumulator     m_accumHotLoad;
    Accumulator*    m_pCurrentAccum;
    unsigned long   m_uNumFFT;
    unsigned long   m_uNumChannels;
    unsigned long   m_uNumSamplesPerAccumulation;
    double          m_dSwitchDelayTime;         // seconds
    double          m_dAccumulationTime;        // seconds
    double          m_dBandwidth;               // MHz
    double          m_dChannelSize;             // MHz
    double          m_dChannelFactor;    
    double          m_dStartFreq;               // MHz
    double          m_dStopFreq;                // MHz
    bool            m_bLocalStop;
    bool            m_bUseStopCycles;
    bool            m_bUseStopSeconds;
    bool            m_bUseStopTime;
    bool            m_bDumpingThisCycle;
    unsigned long   m_uStopCycles;
    double          m_dStopSeconds;             // Seconds
    TimeKeeper      m_tkStopTime;               // UTC


    // Private helper functions
    std::string getFileName();
    bool isStop(unsigned long, Timer&);
    bool isAbort();

  public:

    // Constructor and destructor
    Spectrometer( unsigned long, unsigned long, double, double, 
                  Digitizer*, Channelizer*, Dumper*, Switch*, Controller*);
    ~Spectrometer();

    // Execution functions
    void run();
    void sendStop();

    void setStopCycles(unsigned long);
    void setStopSeconds(double);
    void setStopTime(const std::string&);

    // Callbacks
    unsigned long onDigitizerData(SAMPLE_DATA_TYPE*, unsigned int, unsigned long, double, double);
    void onChannelizerData(ChannelizerData*);

};



#endif // _SPECTROMETER_H_
