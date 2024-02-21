#ifndef _SWTTY_H_
#define _SWTTY_H_

#include <stdio.h>
#include <string>
#include <sstream>
#include <vector>
#include "switch.h"

using namespace std;

// ---------------------------------------------------------------------------
//
// SWTTY -- Switch control by sending ASCII characters to a TTY device
//
// ---------------------------------------------------------------------------
class SWTTY : public Switch {

  private:

    unsigned int m_uSwitchState;
    string m_sPath;
    string m_s0;
    string m_s1;
    string m_s2;

  public:

    // Constructor and destructor
    SWTTY(string& sPath, string& s0, string& s1, string& s2 ) {
      m_sPath = sPath;
      m_s0 = fixControlCharacters(s0); 
      m_s1 = fixControlCharacters(s1);  
      m_s2 = fixControlCharacters(s2); 
      m_uSwitchState = 0;
      printf("Using TTY switch\n");
    }

    ~SWTTY() {}

    // Replace all occurences of the substring, sFrom, withing in sInOut with 
    // the new substring, sTo.
    void replaceSubString(string& sInOut, const string& sFrom, const string& sTo) {
      size_t index = 0;
      while( (index = sInOut.find(sFrom, index)) != string::npos ) {
          sInOut.replace(index, sFrom.length(), sTo);
          index += sTo.length();
      }
    }
    
    string fixControlCharacters(string str) {
      replaceSubString(str, "\\n", "\n");
      replaceSubString(str, "\\r", "\r");
      replaceSubString(str, "\\t", "\t");
      return str;
    }

    bool init(const string& sInit) {

      FILE* file = fopen(m_sPath.c_str(), "w");
      if (!file) {
        return false;
      }
      
      string sCommand = fixControlCharacters(sInit);
      
     // printf("%s", sCommand.c_str());
      fputs(sCommand.c_str(), file);
      fclose(file);
      return true;
    }

    unsigned int set(unsigned int uSwitchState) {

      string sCommand;
      if (uSwitchState == 0) sCommand = m_s0;  // antenna
      if (uSwitchState == 1) sCommand = m_s1;  // load
      if (uSwitchState == 2) sCommand = m_s2;  // load + cal

      // Send the switch state to the TTY device
      FILE* file = fopen(m_sPath.c_str(), "w");
      if (file) { 
        //printf("%s\n", sCommand.c_str());
        fputs(sCommand.c_str(), file);
        fclose(file);
      }

      // Remember the switch state
      m_uSwitchState = uSwitchState;
      return uSwitchState;
    }

    unsigned int increment() {
      return set((m_uSwitchState + 1) % 3);
    }

    unsigned int get() { return m_uSwitchState; }

};

#endif // _SWTTY_H_
