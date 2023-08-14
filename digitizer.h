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
      uint8 = 1,
      int8 = 2,
      uint16 = 3,
      int16 = 4,
      uint32 = 5,
      int32 = 6,
      uint64 = 7,
      int64 = 8,
      float16 = 9,
      float32 = 10,
      float64 = 11 
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
