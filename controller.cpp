#include <cstdio>       // remove
#include <fstream>      // ifstream, ofstream
#include <signal.h>     // sigaction
#include <sys/types.h>  
#include <sstream>
#include <limits>       // numeric_limits<int>::max();
#include "utility.h"    // font colors
#include "controller.h"
#include "version.h"



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
  m_bDump = false;
  m_pSpawn = NULL;
  m_uPlotBin = 1;
  m_uDumpCycles = 1;
  m_bDirectory = true;
}


// ----------------------------------------------------------------------------
// Destructor
// ----------------------------------------------------------------------------
Controller::~Controller() {

  // Stop any plotting
  setPlot(false, m_uPlotBin);

  // Close out as needed for the execution modes
  switch (m_iMode) {

    case CTRL_MODE_START:
      printf("Controller: Removing PID file\n");
      deletePID();
      deleteMsg();
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
// deleteMsg - Deletes any existing message file
// ----------------------------------------------------------------------------
void Controller::deleteMsg() {
  std::remove(MSG_FILE);
}


// ----------------------------------------------------------------------------
// getConfigStr - Returns a string listing all of the configuration options
//                that have been queried using getOption*() and the value
//                that was returned in the query.  Formatted as:
//
//                "; <option long name>: <value>\n"      
// ----------------------------------------------------------------------------
std::string Controller::getConfigStr() const {
  return m_strConfig;
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

  // Add to the config string
  m_strConfig += ";" + strL + ": " + std::to_string(val) + "\n";

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
    m_strConfig += ";" + strL + ": " + std::to_string(val) + "\n";

  } else {
    printf("Using: (%s) %s %s: %ld (0x%x)\n", strSection.c_str(), strS.c_str(), strL.c_str(), val, (unsigned int) val);
    std::stringstream ss;
    ss << std::hex << (unsigned int) val;
    m_strConfig += ";" + strL + ": " + std::to_string(val) + " (0x" + ss.str() + ")\n";
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

  // Add to the config string
  m_strConfig += ";" + strL + ": " + std::to_string(val) + "\n";

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

  // Add to the config string
  m_strConfig += ";" + strL + ": " + val + "\n";

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
      break; // SIGINT

    case SIGUSR1:
    
      int iMsg = 0;
      if (readMsg(iMsg)) {

        switch (iMsg) {
        
          case CTRL_MODE_SHOW:
            printf(BLU "----------------------------------------------------------------------\n" RESET);
            printf(BLU "*** SHOW PLOT signal received ***\n" RESET);
            printf(BLU "----------------------------------------------------------------------\n" RESET);         
            setPlot(true, m_uPlotBin);
            break;

          case CTRL_MODE_HIDE:
            printf(BLU "----------------------------------------------------------------------\n" RESET);
            printf(BLU "*** HIDE PLOT signal received ***\n" RESET);
            printf(BLU "----------------------------------------------------------------------\n" RESET);         
            setPlot(false, m_uPlotBin);
            break;

          case CTRL_MODE_DUMP_START:
            printf(BLU "----------------------------------------------------------------------\n" RESET);
            printf(BLU "*** BEGIN RAW DATA DUMP signal received ***\n" RESET);
            printf(BLU "----------------------------------------------------------------------\n" RESET);         
            setDump(true);
            break;
            
          case CTRL_MODE_DUMP_STOP:
            printf(BLU "----------------------------------------------------------------------\n" RESET);
            printf(BLU "*** END RAW DATA DUMP signal received ***\n" RESET);
            printf(BLU "----------------------------------------------------------------------\n" RESET);         
            setDump(false);
            break;   
        }       
      } else {
        printf("\n\n*** Received new message signal, but failed to read message***\n\n");
      }
      
      break; // SIGUSR1
  }
}

std::string Controller::getDumpFilePath(const TimeKeeper& tk, bool bMakePath) {

  std::string sOutput;

  // We only expect to be in this function when we have just started a 
  // a new antenna position accumulation.  We expect the dump start timestamp  
  // and the new antenna accumulator start timestamp to be identical.
  
  // Use the autonaming scheme
  if (m_bDirectory) {
  
    // If we already have an active ACQ filepath, then only change the base
    // if the day has changed (in anticipation of the ACQ filepath changing
    // on its next write, too).
    if ( m_sOutput.empty() || (m_tkPrevStartTime.doy() != tk.doy()) ) {

      sOutput = construct_filepath_name( std::string(m_sDataDir + "/" + m_sSite + "/" + m_sInstrument),
                                         tk.getFileString(5), m_sInstrument, 
                                         "__" + tk.getFileString(5) + ".dmp" );
                                         
    // If we don't have a existing ACQ filepath, create a new one (in
    // anticipation of the same ACQ filepath being created on its next write)
    } else {
      sOutput = construct_filepath_name( std::string(m_sDataDir + "/" + m_sSite + "/" + m_sInstrument),
                                         m_tkPrevStartTime.getFileString(5), m_sInstrument,  
                                         "__" + tk.getFileString(5) + ".dmp" );
    }
    
  // Use the user specified filepath as a base and add our suffix
  } else {
    sOutput = get_filepath_without_extension(m_sOutput) + "__" + tk.getFileString(5) + ".dmp";
  }

  // Make the directory structure needed for the filepath
  if (bMakePath) {
    if (!make_path(get_path(m_sOutput), 0775)) {
      printf("Controller: Failed to make path to new file.\n");
    }
  }
  
  return sOutput;
    
}



void Controller::onSpectrometerData( Accumulator& acc0, Accumulator& acc1, 
                                     Accumulator& acc2 ) {

  bool bStartFile = false;

  // Create a new file if using directory method and it is first write 
  // or a new day
  if (m_bDirectory && (m_sOutput.empty() || (m_tkPrevStartTime.doy() != acc0.getStartTime().doy()) )) {

    // Get the new path
    m_sOutput = construct_filepath_name( std::string(m_sDataDir + "/" + m_sSite + "/" + m_sInstrument),
                                          acc0.getStartTime().getFileString(5), m_sInstrument, ".acq" );
    bStartFile = true;

  // Overwrite anything in the user specified output file if this is 
  // our first write and we're not using directory method
  } else if (!m_bDirectory && (m_tkPrevStartTime == TimeKeeper())) {
    bStartFile = true;
  }


  // Write the .acq entry
  printf("Controller: Writing to file: %s\n", m_sOutput.c_str());

  // If needed, start a new file and write the header
  if (bStartFile) {

    // Note the start time of the current data
    m_tkPrevStartTime = acc0.getStartTime();
  
    // Make the directory structure
    if (!make_path(get_path(m_sOutput), 0775)) {
      printf("Controller: Failed to make path to new .acq file.\n");
      return;
    }
    
    // Write the header
    std::ofstream fs;
    fs.open(m_sOutput);
    if (!fs.is_open()) {
      printf("Controller: Failed to write header to new .acq file.\n");
      return;
    }
    fs << "; FASTSPEC " << VERSION << std::endl;
    fs << m_strConfig;
    fs.close();
  }

  // Write the data
  append_switch_cycle(m_sOutput, acc0, acc1, acc2);

  // Handle plotting if enabled
  if (m_bPlot) {

    // Write the temporary file for plotting
    write_plot_file(PLOT_FILE, acc0, acc1, acc2, m_uPlotBin);

    // Tell the plotter to refresh
    updatePlotter();
  }
}


// ----------------------------------------------------------------------------
// plot - return true if should show plots, false if not
// ----------------------------------------------------------------------------
bool Controller::plot() const { 
  return m_bPlot; 
}


// ----------------------------------------------------------------------------
// dump - return true if should dump raw dadta, false if not
// ----------------------------------------------------------------------------
bool Controller::dump() const { 
  return m_bDump; 
}


// ----------------------------------------------------------------------------
// readPID - Read existing PID file and get its contained pid and 
//                   starttime.  Returns false if can't open the file
// ----------------------------------------------------------------------------
bool Controller::readPID(pid_t& pid, unsigned long long& tm) {

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
// readMsg - Read existing message file. Returns false if can't open the file
// ----------------------------------------------------------------------------
bool Controller::readMsg(int& iMsg) {

  iMsg = 0;

  std::ifstream fs;
  fs.open(MSG_FILE);
  if (!fs.is_open()) {
    return false;
  } 

  fs >> iMsg;

  fs.close();
  
  deleteMsg();
  
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

  bool bExistingFile = readPID(oldpid, oldtime);
  bool bExistingProc = bExistingFile                              // A control file exists
                        && (oldpid != 0)                          // A reasonable old PID was found in the file
                        && (kill(oldpid, 0) == 0)                 // The old PID is currently running and accessible
                        && (oldtime == getProcStart(oldpid));     // Retrieving the starttime for the old PID now matches what is recorded in the file


  // Allow the app to show help without starting a new control file
  if (iMode==CTRL_MODE_HELP) {
    return true;
  } 
   
  // Start normal execution
  if (iMode == CTRL_MODE_START) {

    // Abort if valid existing process, otherwise record our process info
    if (bExistingProc) {
      
      printf("Controller: An instance of FASTSPEC is already running.  Use 'fastspec stop'\n");
      printf("Controller: to gracefully temrinate the existing instance.  Aborting...\n\n");

      // Change the mode to ABORT so the deconstructor doesn't delete
      // the PID file that belongs to the other process.
      m_iMode = CTRL_MODE_ABORT;
      return false;

    }
    
    printf("Controller: Writing this PID (%d) to " PID_FILE "\n", getpid());
    writePID();
    return true;   
  } 
  
  if (!bExistingProc) {
    printf("Controller: No valid existing instance found\n");
    return false;
  }
      
  // Execute one of the signal modes
  switch (iMode) {

    // Stop execution (gracefully) of an existing instance if a valid pid exists
    case CTRL_MODE_STOP:

      if (bExistingProc) {
        printf("Controller: Sending stop signal to previous PID (%d)\n", oldpid);
        kill(oldpid, SIGINT);
        return false;
      }
      
    // Forcibly kill execution of an existing instance if a valid pid exists
    case CTRL_MODE_KILL:

      if (bExistingProc) {
        printf("Controller: Sending kill -9 signal to previous PID (%d)\n", oldpid);
        kill(oldpid, SIGKILL);
        return false;
      }
          
    // Tell existing instance to start plotting
    case CTRL_MODE_SHOW:

      if (bExistingProc) {
        printf("Controller: Sending show plot signal to previous PID (%d)\n", oldpid);
        sendMsg(oldpid, iMode);
        return false;
      }
      
    // Tell existing instance to stop plotting
    case CTRL_MODE_HIDE:

      if (bExistingProc) {
        printf("Controller: Sending hide plot signal to previous PID (%d)\n", oldpid);
        sendMsg(oldpid, iMode);
        return false;
      }
      
    // Tell existing instance to start dumping raw data
    case CTRL_MODE_DUMP_START:

      if (bExistingProc) {
        printf("Controller: Sending start raw data dump signal to previous PID (%d)\n", oldpid);
        sendMsg(oldpid, iMode);
        return false;
      }
      
    // Tell existing instance to start dumping raw data
    case CTRL_MODE_DUMP_STOP:

      if (bExistingProc) {
        printf("Controller: Sending stop raw data dump signal to previous PID (%d)\n", oldpid);
        sendMsg(oldpid, iMode);
        return false;
      }
  }

  return false;
}


// ----------------------------------------------------------------------------
// sendMsg - Writes message to file and sends signal to the specified PID
// ----------------------------------------------------------------------------
int Controller::sendMsg(pid_t pid, int iMsg) {
  
  std::ofstream fs;
  fs.open(MSG_FILE);
  if (fs.is_open()) {
    fs << iMsg;
    fs.close();
  } else {
    return -1;
  }
  
  return kill(pid, SIGUSR1);
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
// setOutput
// ----------------------------------------------------------------------------
void Controller::setOutput( const std::string& sDataDir, 
                            const std::string& sSite, 
                            const std::string& sInstrument, 
                            const std::string& sOutput) {

  m_sDataDir = sDataDir;
  m_sSite = sSite;
  m_sInstrument = sInstrument;
  m_sOutput = sOutput;
  m_bDirectory = sOutput.empty();

  if (m_bDirectory) {
    printf("Controller: Will write output to %s\n", std::string(sDataDir + "/" + sSite + "/" + sInstrument).c_str());
  } else {
    printf("Controller: Will write output to %s\n", sOutput.c_str());
  }
}



// ----------------------------------------------------------------------------
// setPlot - Tell the controller that there should be plotting (or not).
//           uPlotbin channels are binned (averaged) together when saving the 
//           temporary plot data.  uPlotbin is ignored when bPlot is false;
//           
// ----------------------------------------------------------------------------
void Controller::setPlot(bool bPlot, unsigned int uPlotBin) { 

  m_uPlotBin = uPlotBin;

  // Nothing to do if already in the desired state
  if (bPlot == m_bPlot) {
    return;
  }

  m_bPlot = bPlot; 

  if (!m_bPlot) {
    // If stopping plotting, do some cleanup
    stopPlotter();
  }
}


// ----------------------------------------------------------------------------
// setDump - Tell the controller that there should be dumping (or not).
//           
// ----------------------------------------------------------------------------
void Controller::setDump(bool bDump) { 

  m_bDump = bDump; 

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
      m_pSpawn->stdin << "raw = 'set ylabel \"Power [dB] (arb)\"; unset yrange; plot \"" << PLOT_FILE << "\" using 1:2 title \"p0 (antenna)\" with lines, \"" << PLOT_FILE << "\" using 1:3 title \"p1 (load)\" with lines, \"" << PLOT_FILE << "\" using 1:4 title \"p2 (load+cal)\" with lines'" << std::endl;
      m_pSpawn->stdin << "ta = 'set ylabel \"Temperature [K]\"; set yrange [100:10000]; plot \"" << PLOT_FILE << "\" using 1:5 title \"T_{ant} (uncalib)\" with lines'" << std::endl;
      m_pSpawn->stdin << "@raw" << std::endl;
      m_pSpawn->stdin << "type=0" << std::endl;
      m_pSpawn->stdin << "bind all 'x' 'type=!type; if (type) { @ta } else { @raw }'" << std::endl;
      m_pSpawn->stdin << "set term qt title 'FASTSPEC Plot'" << std::endl;
    }
  }

  // If the plotter process was previously spawned and still has a valid PID
  if (m_pSpawn && (m_pSpawn->child_pid > 0) && (kill(m_pSpawn->child_pid, 0)==0)) {

    // Send the commands to update the plot
    m_pSpawn->stdin << "replot" << std::endl;
  }
}


// ----------------------------------------------------------------------------
// writePID - (Over)write PID and the process start time to file so
//                  - so another instance of the executable can verify that 
//                  - that one is/isn't already running.    
// ----------------------------------------------------------------------------
bool Controller::writePID() {

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




