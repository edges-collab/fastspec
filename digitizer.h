#ifndef _DIGITIZER_H_
#define _DIGITIZER_H_

// ---------------------------------------------------------------------------
//
// DigitizerReceiver
//
// Virtual Interface for class that can receive digitizer data.  
//
// SAMPLE_DATA_TYPE (e.g. short, unsigned short, etc.) must defined as a 
// compiler directive.
//
// ---------------------------------------------------------------------------
class DigitizerReceiver {

  public:  
    
    virtual unsigned long   onDigitizerData( SAMPLE_DATA_TYPE*, 
                                             unsigned int, 
                                             unsigned long,
                                             double,
                                             double ) = 0;
};




// ---------------------------------------------------------------------------
//
// Digitizer
//
// Virtual Interface for digitizer classes
//
// ---------------------------------------------------------------------------
class Digitizer {

  public:

    virtual bool    acquire() = 0; 
    virtual bool    connect(unsigned int) = 0;
    virtual void    disconnect() = 0;
    virtual void    setCallback(DigitizerReceiver*) = 0;
    virtual void    stop() = 0;
    
};

#endif // _DIGITIZER_H_
