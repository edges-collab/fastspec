#ifndef _SWPARLLELPORT_H_
#define _SWPARLLELPORT_H_

#include <stdio.h>
#include <sys/io.h> // ioperm, etc.
#include "switch.h"

using namespace std;

// ---------------------------------------------------------------------------
//
// ParallelPortSwitch
//
// ---------------------------------------------------------------------------
class SWParallelPort : public Switch {

  private:

    // Member variables
    unsigned int    m_uIOport;
    unsigned int    m_uSwitchState;

  public:

    // Constructor and destructor
    SWParallelPort() : m_uIOport(0), m_uSwitchState(0) {}
    ~SWParallelPort() {}

    bool init(unsigned int uIOport) {
      // Define the base address of the Parallel port PCIe Card
      // Use command: lspci -v to find the first I/O port hex address
      // Use command: sudo modprobe parport_pc io=0x#### irq=## to update the kernal
      // module

      // Remember the port
      m_uIOport = uIOport;

      // Set the port access bits for this thread
      if (ioperm(m_uIOport, 3, 1) != 0) {
        printf("You must run this program with root privileges to access the parallel port.\n");
        return false;
      }

      return true;
    }


    unsigned int set(unsigned int uSwitchState) {

      int iWriteValue;
      if (uSwitchState == 0) iWriteValue = 0;  // antenna
      if (uSwitchState == 1) iWriteValue = 2;  // load
      if (uSwitchState == 2) iWriteValue = 3;  // load + cal

      // Send the switch state to the parallel port
      outb(iWriteValue, m_uIOport);  

      // Remember the switch state
      m_uSwitchState = uSwitchState;

      return uSwitchState;
    }


    unsigned int increment() {
      return set((m_uSwitchState + 1) % 3);
    }


    unsigned int get() { return m_uSwitchState; }

};


#endif // _SWPARLLELPORT_H_
