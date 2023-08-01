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

    enum DataType {
      uint16 = 1,
      int16 = 2,
      uint32 = 3,
      int32 = 4,
      float16 = 5,
      float32 = 6,
      float64 = 7 
    };
    
    virtual bool                  acquire() = 0; 
    virtual bool                  connect(unsigned int) = 0;
    virtual void                  disconnect() = 0;
    virtual void                  setCallback(DigitizerReceiver*) = 0;
    virtual void                  stop() = 0;
    
    virtual double                scale() = 0;
    virtual double                offset() = 0;
    virtual Digitizer::DataType   type() = 0;
    virtual unsigned int          bytesPerSample() = 0;
    
};

#endif // _DIGITIZER_H_
