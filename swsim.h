#ifndef _SWSIM_H_
#define _SWSIM_H_

#include <stdio.h>
#include "switch.h"

// ---------------------------------------------------------------------------
//
// SWSim -- Simulated switch that does not control any physical hardware
//
// ---------------------------------------------------------------------------
class SWSim : public Switch {

  private:

    unsigned int m_uSwitchState;

  public:

    // Constructor and destructor
    SWSim() { 
      m_uSwitchState = 0; 
      printf("Using SIMULATED switch\n"); 
    }
    
    ~SWSim() {}

    unsigned int set(unsigned int uSwitchState) {
      m_uSwitchState = uSwitchState;
      return uSwitchState;
    }

    unsigned int increment() {
      return set((m_uSwitchState + 1) % 3);
    }

    unsigned int get() { return m_uSwitchState; }

};

#endif // _SWSIM_H_
