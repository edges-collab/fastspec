#ifndef _SWGPIO_H_
#define _SWGPIO_H_

#include <stdio.h>
#//include <sys/io.h> // ioperm, etc.
#include "switch.h"

#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>


// ---------------------------------------------------------------------------
//
// GPIO Switch
//
// ---------------------------------------------------------------------------
class SWGpio : public Switch {

  private:

    // Member variables
    unsigned int*   m_pPinMap;
    unsigned int    m_uNumBits;
    unsigned int    m_uIOport;
    unsigned int    m_uSwitchState;
    std::string     m_strPath;


    // Member functions
    
    bool initPin(unsigned int uPin) {

      // Open gpio export file
      std::string str = m_strPath + "/export";      
      int fd = open(str.c_str(), O_WRONLY);
      if (fd == -1) {
        printf("SWGpio: Unable to open %s for pin %d \n", str.c_str(), uPin);
        return false;
      }
          
      // Write pin to gpio export file
      str = std::to_string(uPin);
      if (write(fd, str.c_str(), str.length()) != (long int) str.length()) {
        printf("SWGpio: Error writing pin %s to gpio export file\n", str.c_str());
        close(fd);
        return false;
      }
      
      // Close gpio export file
      close(fd);
    
      // Open the pin's gpio direction file
      str = m_strPath + "/gpio" + std::to_string(uPin) + "/direction";   
      fd = open(str.c_str(), O_WRONLY);       
      if (fd == -1) {
        printf("SWGpio: Unable to open %s\n", str.c_str());
        return false;
      }

      // Write the mode to the file
      if (write(fd, "out", 3) != 3) {
        printf("SWGpio: Error writing to %s\n", str.c_str());
        close(fd);
        return false;
      }
      
      // Close the pin's direction file
      close(fd);
      return true;
    }
        
        
    bool writeBit(unsigned int uBit, bool bOn) {
     
      std::string str = m_strPath + "/gpio" + std::to_string(m_pPinMap[uBit]) + "/value";
      
      // Open the pin's value file
      int fd = open(str.c_str(), O_WRONLY);           
      if (fd == -1) {
        printf("SWGpio: Unable to open %s\n", str.c_str());
        return false;
      }

      // Write the value to the pin's file
      str = bOn ? "1" : "0";
      if (write(fd, str.c_str(), str.length()) != (long int) str.length()) {
        printf("SWGpio: Error writing %s to bit %d\n", str.c_str(), uBit);
        close(fd);
        return false;
      }
      close(fd);
      
      return true;      
    }        
        

  public:

    // Constructor and destructor
    SWGpio() : m_pPinMap(0), m_uNumBits(0), m_uSwitchState(0) {}
    ~SWGpio() {}


    bool init (unsigned int uIOport) {
      m_uIOport = uIOport;
      return true;
    }
    
    
    bool setPinMap( const std::string strPath, 
               unsigned int* pPinMap,
               unsigned int uNumBits ) {
                
      // Store the gpio system path string (probably: "/sys/class/gpio")
      m_strPath = strPath; 
      
      // Check we have minimum number of gpio pins assigned
      if (uNumBits < 2) {
        printf("SWGpio: Init failed because fewer than 2 pins were assigned\n");
        return false;
      }
      
      // Store gpio pin map
      m_uNumBits = uNumBits;
      m_pPinMap = new unsigned int[uNumBits];
      
      unsigned int i = 0;
      for (i=0; i<uNumBits; i++) {
        m_pPinMap[i] = pPinMap[i];
      }
      
      // Initialize each gpio pin
      for (i=0; i<uNumBits; i++) {
        if (!initPin(pPinMap[i])) {
          printf("SWGpio: Init of gpio pin %d failed\n", pPinMap[i]);
          return false;
        }
      }
     
      return true;
    }


    unsigned int set(unsigned int uSwitchState) {

      switch(uSwitchState) {
  
        case 0:
          // Antenna (equiv integer = 0)
          writeBit(0, false);
          writeBit(1, false);
          break;
  
        case 1:
          // Load (equiv integer = 2)
          writeBit(0, true);
          writeBit(1, false);          
          break;    
  
        case 2:
          // Load + Cal (equiv integer = 3)
          writeBit(0, true);
          writeBit(1, true);
          break;     
  
        default:
          printf("SWGpio: Unhandled switch value, %d\n", uSwitchState);
          break;       
      }

      // Remember the switch state
      m_uSwitchState = uSwitchState;

      return uSwitchState;
    }


    unsigned int get() { return m_uSwitchState; }

};


#endif // _SWGPIO_H_
