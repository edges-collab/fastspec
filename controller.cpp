#include <cstdio>       // remove
#include <fstream>      // ifstream, ofstream
#include <signal.h>     // sigaction
#include <sys/types.h>  
#include <limits>       // numeric_limits<int>::max();
#include "utility.h"    // font colors
#include "controller.h"



// ----------------------------------------------------------------------------
// Constructor
// ----------------------------------------------------------------------------
Controller::Controller() {
  
  m_argc = 0;
  m_argv = NULL;
  m_pIni = NULL;
  m_iMode = CTRL_MODE_NOTSET;
  m_bStopSignal = false;
  m_bPlot = false;
  m_pSpawn = NULL;
}


// ----------------------------------------------------------------------------
// Destructor
// ----------------------------------------------------------------------------
Controller::~Controller() {

  // Stop any plotting
  setPlot(false);

  // Close out as needed for the execution modes
  switch (m_iMode) {

    case CTRL_MODE_START:
      printf("Controller: Removing PID file\n");
      deletePID();
      break;

    case CTRL_MODE_STOP:
      break; 
  }

  if (m_pIni) {
    delete m_pIni;
  }
}


// ----------------------------------------------------------------------------
// deletePID - Delete existing PID file
// ----------------------------------------------------------------------------
void Controller::deletePID() {
  std::remove(PID_FILE);
}


// ----------------------------------------------------------------------------
// deletePlot - Delete existing PID file
// ----------------------------------------------------------------------------
void Controller::deletePlot() {
  std::remove(PLOT_FILE);
}




// ----------------------------------------------------------------------------
// getOptionBool - (long) Returns a parameter value based on command line 
//             arguments, INI configuration file, or a default value supplied
//             in the function call (in that order).         
// ----------------------------------------------------------------------------
long Controller::getOptionBool( const std::string& strSection, 
                                const std::string& strLong, 
                                const std::string& strShort, 
                                bool def) {
  bool val = false;
  std::string strS("");
  std::string strL("--");

  strS += strShort;
  strL += strLong;

  // Check INI file
  if (m_pIni != NULL) {
    val = m_pIni->GetBoolean(strSection, strLong, def);
  }

  // Check command line arguments
  for (int i=0; i<m_argc; i++) {
    std::string sArg = m_argv[i];
    if ((sArg.compare(strS) == 0) || (sArg.compare(strL) == 0)) {
      val = true;
      if (m_argc > (i+1)) {
        const char* begin = std::string(m_argv[i+1]).c_str();
        char* end;
        long n = strtol(begin, &end, 0); // parses decimal and hex
        val = end > begin ? n : true;
      }
      break;
    }
  }

  // Print the option parameter name and value
  printf("Using: (%s) %s %s: %d\n", strSection.c_str(), strS.c_str(), strL.c_str(), val);
  return val;
}


// ----------------------------------------------------------------------------
// getOptionInt - (long) Returns a parameter value based on command line 
//             arguments, INI configuration file, or a default value supplied
//             in the function call (in that order).         
// ----------------------------------------------------------------------------
long Controller::getOptionInt( const std::string& strSection, 
                               const std::string& strLong, 
                               const std::string& strShort, 
                               long def) {
  long val = def;
  std::string strS("");
  std::string strL("--");

  strS += strShort;
  strL += strLong;

  // Check INI file
  if (m_pIni != NULL) {
    val = m_pIni->GetInteger(strSection, strLong, def);
  }

  // Check command line arguments
  for (int i=0; i<m_argc; i++) {
    std::string sArg = m_argv[i];
    if ((sArg.compare(strS) == 0) || (sArg.compare(strL) == 0)) {
      if (m_argc > (i+1)) {
        const char* begin = std::string(m_argv[i+1]).c_str();
        char* end;
        long n = strtol(begin, &end, 0); // parses decimal and hex
        val = end > begin ? n : def;
        break;
      }
    }
  }

  // Print the option parameter name and value
  if (val > std::numeric_limits<unsigned int>::max()) {
    printf("Using: (%s) %s %s: %ld\n", strSection.c_str(), strS.c_str(), strL.c_str(), val);
  } else {
    printf("Using: (%s) %s %s: %ld (0x%x)\n", strSection.c_str(), strS.c_str(), strL.c_str(), val, (unsigned int) val);
  }
  return val;
}


// ----------------------------------------------------------------------------
// getOptionReal - (double) Returns a parameter value based on command line 
//             arguments, INI configuration file, or a default value supplied
//             in the function call (in that order).         
// ----------------------------------------------------------------------------
double Controller::getOptionReal( const std::string& strSection, 
                                  const std::string& strLong, 
                                  const std::string& strShort, 
                                  double def) {

  double val = def;
  std::string strS("");
  std::string strL("--");

  strS += strShort;
  strL += strLong;

  // Check INI file
  if (m_pIni != NULL) {
    val = m_pIni->GetReal(strSection, strLong, def);
  }

  // Check command line arguments
  for (int i=0; i<m_argc; i++) {
    std::string sArg = m_argv[i];
    if ((sArg.compare(strS) == 0) || (sArg.compare(strL) == 0)) {
      if (m_argc > (i+1)) {
        val = std::stod(m_argv[i+1]);
        break;
      }
    }
  }

  // Print the option parameter name and value
  printf("Using: (%s) %s %s: %g\n", strSection.c_str(), strS.c_str(), strL.c_str(), val);
  return val;
}


// ----------------------------------------------------------------------------
// getOptionStr - (string) Returns a parameter value based on command line 
//             arguments, INI configuration file, or a default value supplied
//             in the function call (in that order).         
// ----------------------------------------------------------------------------
std::string Controller::getOptionStr( const std::string& strSection, 
                                      const std::string& strLong, 
                                      const std::string& strShort, 
                                      const std::string& def) {
  std::string val(def);
  std::string strS("");
  std::string strL("--");

  strS += strShort;
  strL += strLong;

  // Check INI file
  if (m_pIni != NULL) {
    val = m_pIni->Get(strSection, strLong, def);
  }

  // Check command line arguments
  for (int i=0; i<m_argc; i++) {
    std::string sArg = m_argv[i];
    if ((sArg.compare(strS) == 0) || (sArg.compare(strL) == 0)) {
      if (m_argc > (i+1)) {
        val = std::string(m_argv[i+1]);
        break;
      }
    }
  }

  // Print the option parameter name and value
  printf("Using: (%s) %s %s: %s\n", strSection.c_str(), strS.c_str(), strL.c_str(), val.c_str());
  return val;
}


// ----------------------------------------------------------------------------
// getProcStart - Get the start time associated with a PID
// ----------------------------------------------------------------------------
unsigned long long Controller::getProcStart(pid_t pid) {
  
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


// ----------------------------------------------------------------------------
// mode - Return mode of operation
// ----------------------------------------------------------------------------
int Controller::mode() const { 
  return m_iMode; 
}


// ----------------------------------------------------------------------------
// onSignal - Called by the global wrapper when a unix signal is received
// ----------------------------------------------------------------------------
void Controller::onSignal(int iSignalNumber) {

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

    case SIGUSR1:
      printf(BLU "----------------------------------------------------------------------\n" RESET);
      printf(BLU "*** SHOW PLOT signal received ***\n" RESET);
      printf(BLU "----------------------------------------------------------------------\n" RESET);         
      setPlot(true);
      break;

    case SIGUSR2:
      printf(BLU "----------------------------------------------------------------------\n" RESET);
      printf(BLU "*** HIDE PLOT signal received ***\n" RESET);
      printf(BLU "----------------------------------------------------------------------\n" RESET);         
      setPlot(false);
      break;
  }
}


// ----------------------------------------------------------------------------
// plot - return true if should show plots, false if not
// ----------------------------------------------------------------------------
bool Controller::plot() const { 
  return m_bPlot; 
}


// ----------------------------------------------------------------------------
// plotPath - return the path to the temporary plot data file
// ----------------------------------------------------------------------------
std::string Controller::plotPath() { 
  return std::string(PLOT_FILE); 
}


// ----------------------------------------------------------------------------
// readControlFile - Read existing control file and get its contained pid and 
//                   starttime.  Returns false if can't open the file
// ----------------------------------------------------------------------------
bool Controller::readControlFile(pid_t& pid, unsigned long long& tm) {

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


// ----------------------------------------------------------------------------
// registerHandler - Register a passed wrapper function pointer as the handler  
//                   for unix signals.  The wrapper function should call the 
//                   onSignal member of an instance of this class.
// ----------------------------------------------------------------------------
void Controller::registerHandler(void (*handler)(int)) {

  // Register our SIGINT handler
  struct sigaction sAction;
  sAction.sa_handler = handler;
  sAction.sa_flags = SA_RESTART;
  sigemptyset(&sAction.sa_mask);
  sigaction(SIGINT,&sAction, NULL);
  sigaction(SIGUSR1,&sAction, NULL);
  sigaction(SIGUSR2,&sAction, NULL);
}  


// ----------------------------------------------------------------------------
// run - Set the run mode and check for an existing application instance.  
//       Returns false if this application should stop.
// ----------------------------------------------------------------------------
bool Controller::run(int iMode) {

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

    // Tell existing instance to start plotting
    case CTRL_MODE_SHOW:

      if (bExistingProc) {
        printf("Controller: Sending show plot signal to previous PID (%d)\n", oldpid);
        sendShowPlotSignal(oldpid);
        return false;
      }
      
      printf("Controller: No valid existing instance found\n");
      return false;
      break;

    // Tell existing instance to stop plotting
    case CTRL_MODE_HIDE:

      if (bExistingProc) {
        printf("Controller: Sending hide plot signal to previous PID (%d)\n", oldpid);
        sendHidePlotSignal(oldpid);
        return false;
      }
      
      printf("Controller: No valid existing instance found\n");
      return false;
      break;
  }

  return false;
}


// ----------------------------------------------------------------------------
// sendStopSignal - Sends the stop signal to the specified PID
// ----------------------------------------------------------------------------
int Controller::sendStopSignal(pid_t pid) {
  return kill(pid, SIGINT);
}


// ----------------------------------------------------------------------------
// sendKillSignal - Sends the hard kill signal to the specified PID
// ----------------------------------------------------------------------------
int Controller::sendKillSignal(pid_t pid) {
  return kill(pid, SIGKILL);
}


// ----------------------------------------------------------------------------
// sendShowPlotSignal - Sends the show plots signal to the specified PID
// ----------------------------------------------------------------------------
int Controller::sendShowPlotSignal(pid_t pid) {
  return kill(pid, SIGUSR1);
}


// ----------------------------------------------------------------------------
// sendHidePlotSignal - Sends the hide plots signal to the specified PID
// ----------------------------------------------------------------------------
int Controller::sendHidePlotSignal(pid_t pid) {
  return kill(pid, SIGUSR2);
}


// ----------------------------------------------------------------------------
// setArgs - Store the commandline args for use with getOption
// ----------------------------------------------------------------------------
void Controller::setArgs(int argc, char** argv) { 
  m_argc = argc;
  m_argv = argv;
}


// ----------------------------------------------------------------------------
// setINI - Parse an INI file and store parameters for use with getOption
// ----------------------------------------------------------------------------
bool Controller::setINI(const std::string& str) { 

  if (m_pIni != NULL) {
    delete m_pIni;
    m_pIni = NULL;
  }

  m_pIni = new INIReader(str);
  if ((m_pIni == NULL) || (m_pIni->ParseError() != 0)) {
    return false;
  }

  return true;
}


// ----------------------------------------------------------------------------
// setPlot - Tell the controller that there should be plotting (or not)
// ----------------------------------------------------------------------------
void Controller::setPlot(bool bPlot) { 

  // Nothing to do if already in the desired state
  if (bPlot == m_bPlot) {
    return;
  }

  m_bPlot = bPlot; 
  if (!m_bPlot) {
    // If stopping plotting, do some cleanup
    stopPlotter();
    //deletePlot();
  }
}


// ----------------------------------------------------------------------------
// stop - Returns true if a stop signal has bene received
// ----------------------------------------------------------------------------
bool Controller::stop() const { 
  return m_bStopSignal; 
}


// ----------------------------------------------------------------------------
// stopPlotter - Shutdown the external plotter
// ----------------------------------------------------------------------------
void Controller::stopPlotter() {

  if (m_pSpawn) {
    m_pSpawn->stdin << "quit" << std::endl;
    //sendStopSignal(m_pSpawn->child_pid);
    delete(m_pSpawn);
    m_pSpawn = NULL;
  }
}


// ----------------------------------------------------------------------------
// updatePlotter - Tells the plotter to reload the data file and replot
// ----------------------------------------------------------------------------
void Controller::updatePlotter() {

  // Start the plotter if it hasn't been started before
  if (m_pSpawn == NULL) {

    printf("Controller: Starting plotter...\n");        
    const char* const argv[] = {PLOTTER_EXE, (const char*)0};
    m_pSpawn = new spawn(argv, true);

    // If the plotter process has been successfully spawned with a valid PID
    if (m_pSpawn && (m_pSpawn->child_pid > 0) && (kill(m_pSpawn->child_pid, 0)==0)) {

      // Send the commands to initialize the plotting
      m_pSpawn->stdin << "set macros" << std::endl;
      m_pSpawn->stdin << "set xlabel 'Frequency [MHz]'" << std::endl;
      m_pSpawn->stdin << "raw = 'set ylabel \"Power [dB] (arb)\"; unset yrange; plot \"/tmp/fastspec.dat\" using 1:2 title \"p0 (antenna)\" with lines, \"/tmp/fastspec.dat\" using 1:3 title \"p1 (load)\" with lines, \"/tmp/fastspec.dat\" using 1:4 title \"p2 (load+cal)\" with lines'" << std::endl;
      m_pSpawn->stdin << "ta = 'set ylabel \"Temperature [K]\"; set yrange [100:10000]; plot \"/tmp/fastspec.dat\" using 1:5 title \"T_{ant} (uncalib)\" with lines'" << std::endl;
      m_pSpawn->stdin << "@raw" << std::endl;
      m_pSpawn->stdin << "type=0" << std::endl;
      m_pSpawn->stdin << "bind all 'x' 'type=!type; if (type) { @ta } else { @raw }'" << std::endl;
    }
  }

  // If the plotter process was previously spawned and still has a valid PID
  if (m_pSpawn && (m_pSpawn->child_pid > 0) && (kill(m_pSpawn->child_pid, 0)==0)) {

    // Send the commands to update the plot
    m_pSpawn->stdin << "replot" << std::endl;
  }
}


// ----------------------------------------------------------------------------
// writeControlFile - (Over)write PID to file
// ----------------------------------------------------------------------------
bool Controller::writeControlFile() {

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

