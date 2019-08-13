#ifndef _CONTROLLER_H_
#define _CONTROLLER_H_

#define PID_FILE "/tmp/fastspec.pid"

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

      // Get the PID of an existance instance if it exists
      pid_t oldpid = readPID();

      // Execute
      switch (iMode) {

        // Start execution
        case CTRL_MODE_START:

          // Write our pid to file if there isn't a valid pid already there
          if ((oldpid == 0) || (kill(oldpid, 0) != 0)) {
            printf("Controller: Writing this PID (%d) to " PID_FILE "\n", getpid());
            writePID();
            return true;
          } else {
            return false;
          }
          break;

        // Stop execution of an existing instance if a valid pid exists
        case CTRL_MODE_STOP:

          if (oldpid != 0) {
            if (kill(oldpid, 0) == 0) {
              printf("Controller: Sending stop signal to previous PID (%d)\n", oldpid);
              sendStopSignal(oldpid);
              return false;
            } 
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
    void writePID() {

      std::ofstream pidFile;

      pidFile.open(PID_FILE);
      if (pidFile.is_open()) {
        pidFile << getpid();
        pidFile.close();
      }
    }


    // Read existing PID file
    pid_t readPID() {

      pid_t pid = 0;
      std::ifstream pidFile;

      pidFile.open(PID_FILE);
      if (!pidFile.is_open()) {
        return 0;
      } 

      pidFile >> pid;
      pidFile.close();
      return pid;
    }


    // Delete existing PID file
    void deletePID() {
      std::remove(PID_FILE);
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


