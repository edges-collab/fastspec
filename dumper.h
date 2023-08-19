#ifndef _DUMPER_H_
#define _DUMPER_H_

#include <pthread.h>
#include "bytebuffer.h"
#include "timing.h"

#define DUMPER_THREAD_SLEEP_MICROSECONDS 5

class Dumper {

  private:

    // Member variables
    FILE*                         m_pFile;
    pthread_t                     m_thread;
    ByteBuffer                    m_buffer;
    unsigned long                 m_uBytesPerAccumulation;
    unsigned int                  m_uBytesPerTransfer;
    unsigned long                 m_uBytesWritten;
    unsigned int                  m_uNumReady;
    unsigned int                  m_uDataType;
    double                        m_dSampleRate;
    double                        m_dScale;
    double                        m_dOffset;
    bool                          m_bStop;
    Timer                         m_timer;
    
    // Private helper functions
    void            threadIsReady();

  public:
      
    // Constructor and destructor
    Dumper( unsigned long, 
            unsigned int, 
            unsigned int, 
            unsigned int,  
            double,
            double,
            double );
            
    ~Dumper();

    // Interface functions
    bool            push(void*, unsigned int);
    void            closeFile();
    bool            openFile( const std::string&, 
                              const TimeKeeper&, 
                              const unsigned int );
    void            waitForEmpty();
    double          getTimerInterval();
    

    // Other functions
    static void*    threadLoop(void*);

};



#endif // _DUMPER_H_
