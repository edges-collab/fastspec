#ifndef _CONTROLLER_H_
#define _CONTROLLER_H_

#define PID_FILE "/tmp/fastspec.pid"
#define PROC_DIR "/proc"

#define CTRL_MODE_NOTSET        0
#define CTRL_MODE_START         1
#define CTRL_MODE_STOP          2


#include <cstdio>       // remove
#include <fstream>      // ifstream, ofstream
#include <signal.h>     // sigaction
#include <sys/types.h>  
#include <unistd.h>     // getpid
#include "utility.h"    // font colors



// ---------------------------------------------------------------------------
//
// Controller
//
// Handles process ID and IPC signal sending/receiving
//
// ---------------------------------------------------------------------------
class Controller {

  public:

    // Constructor
    Controller() {
      
      m_iMode = CTRL_MODE_NOTSET;
      m_bStopSignal = false;
    }


    // Destructor
    ~Controller() {

      switch (m_iMode) {

        case CTRL_MODE_START:
          printf("Controller: Removing PID file\n");
          deletePID();
          break;

        case CTRL_MODE_STOP:
          break; 
      }
    }


    // Public functions

    // True if a stop signal has bene received
    bool stop() const { return m_bStopSignal; }

    // Return mode of operation
    int mode() const { return m_iMode; }

    // Set the run mode and check for an existing application
    // instance.  Returns false if this application should 
    // stop.
    bool run(int iMode) {

      m_iMode = iMode;

      // Check for an existing instance
      pid_t oldpid;
      unsigned long long oldtime;

      bool bExistingFile = readControlFile(oldpid, oldtime);
      bool bExistingProc = bExistingFile                              // A control file exists
                            && (oldpid != 0)                          // A reasonable old PID was found in the file
                            && (kill(oldpid, 0) == 0)                 // The old PID is currently running and accessible
                            && (oldtime == getProcStart(oldpid));     // Retrieving the starttime for the old PID now matches what is recorded in the file


      // Execute
      switch (iMode) {

        // Start execution
        case CTRL_MODE_START:

          // Abort if valid existing process, otherwise record our process info
          if (bExistingProc) {
            return false;
          } else {
            printf("Controller: Writing this PID (%d) to " PID_FILE "\n", getpid());
            writeControlFile();
            return true;
          }
          break;

        // Stop execution of an existing instance if a valid pid exists
        case CTRL_MODE_STOP:

          if (bExistingProc) {
            printf("Controller: Sending stop signal to previous PID (%d)\n", oldpid);
            sendStopSignal(oldpid);
            return false;
          }
          
          printf("Controller: No valid existing instance found\n");
          return false;
          break;
      }

      return false;
    }

    // Called by the global wrapper when a unix signal is received
    void onSignal(int iSignalNumber) {

      switch (iSignalNumber) {
        case SIGINT:

          printf("\n");
          printf(BLU "----------------------------------------------------------------------\n" RESET);
          printf(BLU "*** STOP signal received ***\n" RESET);
          printf(BLU "----------------------------------------------------------------------\n" RESET);          
          printf(BLU "Program will exit gracefully as soon as possible.  This can be invoked\n" RESET);
          printf(BLU "in three ways:\n\n" RESET);
          printf(BLU "1. Pressing CTRL-C in the execution terminal\n" RESET);
          printf(BLU "2. Running 'fastspec stop' from any terminal\n" RESET);
          printf(BLU "3. Running 'kill -s SIGINT <PID>' from any terminal, where <PID> is\n" RESET);
          printf(BLU "   the appropriate process ID number\n" RESET);
          printf(BLU "----------------------------------------------------------------------\n\n" RESET);

          // Set the stop flag
          m_bStopSignal = true;
          break;
      }
    }


    // Register a passed wrapper function pointer as the handler for 
    // unix signals.  The wrapper function should call the onSignal
    // member of an instance of this class.
    static void registerHandler(void (*handler)(int)) {

      // Register our SIGINT handler
      struct sigaction sAction;
      sAction.sa_handler = handler;
      sAction.sa_flags = SA_RESTART;
      sigemptyset(&sAction.sa_mask);
      sigaction(SIGINT,&sAction, NULL);
    }  


  private: 

    // (Over)write PID to file
    bool writeControlFile() {

      std::ofstream fs;
      fs.open(PID_FILE);
      if (fs.is_open()) {
        fs << getpid() << " ";
        fs << getProcStart(getpid());
        fs.close();
        return true;
      }

      return false;
    }


    // Read existing control file and get its contained pid and starttime.
    // Returns false if can't open the file
    bool readControlFile(pid_t& pid, unsigned long long& tm) {

      pid = 0;
      tm = 0;

      std::ifstream fs;
      fs.open(PID_FILE);
      if (!fs.is_open()) {
        return false;
      } 

      fs >> pid;
      fs >> tm;

      fs.close();
      
      return true;
    }


    // Delete existing PID file
    void deletePID() {
      std::remove(PID_FILE);
    }


    // Get the start time associated with a PID
    unsigned long long getProcStart(pid_t pid) {
      
      std::ifstream ifs;
      std::string str(PROC_DIR "/");

      ifs.open(str + std::to_string(pid) + "/stat");
      if (!ifs.is_open()) {
        return 0;
      } 

      // Read through the file until we get to the 
      // 22nd entry, which is the starttime
      for (int i=0; i<22; i++) {
        ifs >> str;
      }
      ifs.close();

      return stoull(str);
    }



    // Sends the stop signal
    int sendStopSignal(pid_t pid) {
      return kill(pid, SIGINT);
    }

    // Send a hard kill signal
    int sendKillSignal(pid_t pid) {
      return kill(pid, SIGKILL);
    }


    // Member variables
    bool        m_bStopSignal;
    int         m_iMode;
    pid_t       m_previousPID;

};



#endif // _CONTROLLER_H_


