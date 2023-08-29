#ifndef _SWNEUOSYS_H_
#define _SWNEUOSYS_H_

#include <stdio.h>
#include <sys/io.h> // ioperm, etc.
#include <unistd.h> // usleep
#include "switch.h"
#include "wdt_dio.h"


// ---------------------------------------------------------------------------
//
// SWNeousys -- For controlling latched switches on a Neuosys ruggedized
//              computer with MezIO card.
//
// ---------------------------------------------------------------------------

class SWNeuosys : public Switch {

  private:

    // Member variables
    unsigned int    m_uIOport;
    unsigned int    m_uSwitchState;

  public:

    // Constructor and destructor
    SWNeuosys() : m_uIOport(0), m_uSwitchState(0) {}
    ~SWNeuosys() {}

    bool init(unsigned int uIOport) {

      // Remember the port
      m_uIOport = uIOport;
      printf("spectral_mode 0=ant 1=amb 2=hot 3=open_cable 4=shorted_cable %d\n",uIOport);

      // Set the port access bits for this thread
      if ( ! InitDIO() ) {
        printf("InitDIO <-- ERROR\n");
        return false;
      }

      return true;
    }


    unsigned int set(unsigned int uSwitchState) {

      int iWriteValue=0;
      int iWreleaselatch;
      if (uSwitchState == 0 && m_uIOport == 0) iWriteValue = (1<<8) + 1;  // antenna
      if (uSwitchState == 0 && m_uIOport == 1) iWriteValue = (1<<8) + (1<<7);  // amb
      if (uSwitchState == 0 && m_uIOport == 2) iWriteValue = (1<<8) + (1<<4);  // hot
      if (uSwitchState == 0 && m_uIOport == 3) iWriteValue = (1<<8) + (1<<6) + (1<<11);  // open
      if (uSwitchState == 0 && m_uIOport == 4) iWriteValue = (1<<8) + (1<<6) + (1<<10);  // short
      if (uSwitchState == 1) iWriteValue = (1<<8) + (1<<1);  // load
      if (uSwitchState == 2) iWriteValue = (1<<8) + (1<<1) + (1<<12);  // load + cal

      // Send the switch state to the parallel port
      DOWritePort(iWriteValue);   
      iWreleaselatch = iWriteValue&(~((1<<8) + (1<<11) + (1<<10))); 
      usleep(1e5);
      DOWritePort(iWreleaselatch); // release current on latched switches
      
      // Remember the switch state
      m_uSwitchState = uSwitchState;

      return uSwitchState;
    }

    unsigned int get() { return m_uSwitchState; }

};

#endif // _SWNEUOSYS_H_
