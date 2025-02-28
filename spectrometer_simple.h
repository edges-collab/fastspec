#ifndef _SPECTROMETER_SIMPLE_H_
#define _SPECTROMETER_SIMPLE_H_

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
// SPECTROMETER_SIMPLE
//
// Uses PXBoard, Switch, and FFTPool objects to control and acquire data from
// the EDGES system.
//
// ---------------------------------------------------------------------------
class SpectrometerSimple : public DigitizerReceiver, ChannelizerReceiver {

  private:

    // Member variables
    Digitizer*      m_pDigitizer;
    Channelizer*    m_pChannelizer;
    Controller*     m_pController;    
    
		pthread_mutex_t   	m_queueMutex;    
		pthread_mutex_t   	m_fileMutex;    
    list<Accumulator*>	m_released;
		list<Accumulator*>	m_active;
		
    Accumulator*    m_pCurrentAccum;
    unsigned long   m_uNumFFT;
    unsigned long   m_uNumChannels;
    unsigned long   m_uNumSamplesPerAccumulation;
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
    unsigned long   m_uStopCycles;
    double          m_dStopSeconds;             // Seconds
    TimeKeeper      m_tkStopTime;               // UTC


    // Private helper functions
    std::string getFileName();
    bool writeToAcqFile();
    bool handleLivePlot();
    bool isStop(unsigned long, Timer&);
    bool isAbort();
    void allocateAccumulators(unsigned int);
    Accummulator* activateAccumulator();
    void releaseAccumulator();
  

  public:

    // Constructor and destructor
    SpectrometerSimple( unsigned long, unsigned long, double, 
                  Digitizer*, Channelizer*, Controller*);
    ~SpectrometerSimple();

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



#endif // _SPECTROMETER_SIMPLE_H_
