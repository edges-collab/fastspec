#ifndef _CHANNELIZER_H_
#define _CHANNELIZER_H_

#ifndef SAMPLE_DATA_TYPE
  #error Aborted in channelizer.h because SAMPLE_DATA_TYPE was not defined.
#endif

#ifndef BUFFER_DATA_TYPE
  #error Aborted in channelizer.h because BUFFER_DATA_TYPE was not defined.
#endif

struct ChannelizerData {
  BUFFER_DATA_TYPE* pData;
  unsigned int uNumChannels;
  double dADCmin;
  double dADCmax;
};



// ---------------------------------------------------------------------------
//
// ChannelizerReceiver
//
// Virtual Interface for class that can receive channelizer data
//
// ---------------------------------------------------------------------------
class ChannelizerReceiver {

  public:  

    virtual void  onChannelizerData(ChannelizerData*) = 0;
};




// ---------------------------------------------------------------------------
//
// Channelizer
//
// Virtual Interface for asynchronous channelizer classes
//
// ---------------------------------------------------------------------------
class Channelizer {

  public:

    virtual bool    push(SAMPLE_DATA_TYPE*, unsigned int, double, double) = 0;
    virtual void    setCallback(ChannelizerReceiver*) = 0;
    virtual void		waitForEmpty() = 0;

};

#endif // _CHANNELIZER_H_
