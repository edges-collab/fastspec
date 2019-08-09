#ifndef _CHANNELIZER_H_
#define _CHANNELIZER_H_

#ifdef FFT_DOUBLE_PRECISION
	#define CHANNELIZER_DATA_TYPE					double
#else
  #define CHANNELIZER_DATA_TYPE         float
#endif

struct ChannelizerData {
  CHANNELIZER_DATA_TYPE* pData;
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

//#define channelizer_callback_fptr   std::function<void(const ChannelizerData*)>



// ---------------------------------------------------------------------------
//
// Channelizer
//
// Virtual Interface for asynchronous channelizer classes
//
// ---------------------------------------------------------------------------
class Channelizer {

  public:

    virtual bool    push(unsigned short*, unsigned int) = 0;
    virtual void    setCallback(ChannelizerReceiver*) = 0;
    virtual void		waitForEmpty() = 0;

};

#endif // _CHANNELIZER_H_
