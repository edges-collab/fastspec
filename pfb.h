#ifndef _PFB_H_
#define _PFB_H_

#include <functional>
#include <fftw3.h>
#include <pthread.h>
#include "channelizer.h"
#include "buffer.h"

#if defined FFT_DOUBLE_PRECISION
  #define FFT_REAL_TYPE           double
  #define FFT_COMPLEX_TYPE        fftw_complex
  #define FFT_PLAN_TYPE           fftw_plan
  #define FFT_EXECUTE             fftw_execute
  #define FFT_PLAN                fftw_plan_dft_r2c_1d
  #define FFT_DESTROY_PLAN        fftw_destroy_plan
  #define FFT_MALLOC              fftw_malloc
  #define FFT_FREE                fftw_free
#elif defined FFT_SINGLE_PRECISION
  #define FFT_REAL_TYPE           float
  #define FFT_COMPLEX_TYPE        fftwf_complex
  #define FFT_PLAN_TYPE           fftwf_plan
  #define FFT_EXECUTE             fftwf_execute
  #define FFT_PLAN                fftwf_plan_dft_r2c_1d
  #define FFT_DESTROY_PLAN        fftwf_destroy_plan
  #define FFT_MALLOC              fftwf_malloc
  #define FFT_FREE                fftwf_free
#elif
  #error Aborted in pfb.h because FFT precision was not explicitly defined.
#endif


#define THREAD_SLEEP_MICROSECONDS 5

using namespace std;

class PFB : public Channelizer {

  private:

    // Member variables
    ChannelizerReceiver*          m_pReceiver;
    pthread_t*                    m_pThreads;
    pthread_mutex_t               m_mutexPlan;
    pthread_mutex_t               m_mutexCallback;
    CHANNELIZER_DATA_TYPE*        m_pWindow;
    Buffer                        m_buffer;
    unsigned int                  m_uNumTaps;
    unsigned int                  m_uNumThreads;
    unsigned int                  m_uNumChannels;
    unsigned int                  m_uNumFFT;
    unsigned int                  m_uNumSamples;
    unsigned int                  m_uNumBuffers;
    unsigned int                  m_uNumReady;
    bool                          m_bStop;
    
    // Private helper functions
    void            process(Buffer::iterator&, 
                            FFT_REAL_TYPE*, 
                            FFT_COMPLEX_TYPE*, 
                            FFT_PLAN_TYPE );

    void            threadIsReady();

  public:

    // Constructor and destructor
    PFB( unsigned int, unsigned int, unsigned int, unsigned int, 
         unsigned int );
    ~PFB();

    // Interface functions
    bool            push(unsigned short*, unsigned int);
    void            setCallback(ChannelizerReceiver*);
    void            waitForEmpty();

    // Other functions
    bool            setWindowFunction(unsigned int);
    static void*    threadLoop(void*);

};



#endif // _PFB_H_
