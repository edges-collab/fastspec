#ifndef _CONTROLLER_H_
#define _CONTROLLER_H_

#define PID_FILE        "/tmp/fastspec.pid"
#define MSG_FILE        "/tmp/fastspec.msg"
#define PLOT_FILE       "/tmp/fastspec_plot.txt"
#define PLOTTER_EXE     "/usr/bin/gnuplot"
#define PROC_DIR        "/proc"

#define CTRL_MODE_NOTSET        0
#define CTRL_MODE_START         1
#define CTRL_MODE_STOP          2
#define CTRL_MODE_SHOW          3
#define CTRL_MODE_HIDE          4
#define CTRL_MODE_KILL          5
#define CTRL_MODE_HELP          6
#define CTRL_MODE_ABORT         7
#define CTRL_MODE_DUMP_START    8
#define CTRL_MODE_DUMP_STOP     9

#include <string>
#include <unistd.h>     // pid
#include "accumulator.h"
#include "ini.h"
#include "spawn.h" 




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
    Controller();

    // Destructor
    ~Controller();

    // ----------------
    // Public functions
    // ----------------

    // Returns a string that lists all of the configuration settings that
    // have been queried using the getOption*() calls.  
    std::string getConfigStr() const;

    // Get an option parameter value based on the commandline args and INI file
    long getOptionBool(const std::string&, const std::string&, const std::string&, bool);
    
    long getOptionInt(const std::string&, const std::string&, const std::string&, long);
    
    double getOptionReal(const std::string&, const std::string&, const std::string&, double);
    
    std::string getOptionStr(const std::string&, const std::string&, const std::string&, const std::string&);

   // Return the filepath that should be used to start/append to ACQ file
    std::string getAcqFilePath(const TimeKeeper& tk);
    
    // Return the filepath that should be used to start dump file
    std::string getDumpFilePath(const TimeKeeper& tk);
    
    // Return the filepath that should be used for live plot file
    std::string getPlotFilePath();   
    
    // Binning level for reducing live plot data length
    unsigned int getPlotBinLevel() const;

    // Return mode of operation
    int mode() const;

    // Called by the global wrapper when a unix signal is received
    void onSignal(int); 

    // Handle the writing of data to disk after each spectrometer switch cycle
    // void onSpectrometerData(Accumulator&, Accumulator&, Accumulator&);
   
    // True if should show plots, false if should hide plots
    bool plot() const;
    
    // True if should dump raw digitizer data, false if should not
    bool dump() const;

    // Set the run mode and check for an existing application
    // instance.  Returns false if this application should 
    // stop.
    bool run(int);

    // Give the controller the commandline arguments for later parsing
    void setArgs(int argc, char**);

    // Give the controller the INI file for parsing
    bool setINI(const std::string&);

    // Set the parameters used to determine output filepath base for
    // writing spectrometer data (acq or dmp)
    void setOutputConfig( const std::string&, const std::string&, 
                          const std::string&, const std::string& );
                    
    
      // Tell the controller if there should be plotting and how to bin data
    void setPlot(bool, unsigned int);

    // Tell the controller if there should be raw data dumping
    void setDump(bool);
    
    // True if a stop signal has bene received
    bool stop() const;    

    // Should be called when a new plot file is ready for the plotter
    // to load and plot
    void updatePlotter(); 

    // Register a passed wrapper function pointer as the handler for 
    // unix signals.  The wrapper function should call the onSignal
    // member of an instance of this class.
    static void registerHandler(void (*handler)(int));


  private: 

    // -----------------
    // Private functions
    // -----------------

    // Delete existing PID file
    void deletePID();

    // Delete existing plot file
    void deletePlot();

    // Delete existing message file
    void deleteMsg();
    
    // Get the start time associated with a PID
    unsigned long long getProcStart(pid_t pid);

    // Read existing control file and get its contained pid and starttime.
    // Returns false if can't open the file
    bool readPID(pid_t& pid, unsigned long long& tm); 
    
    // Read existing message file.
    // Returns false if can't open the file    
    bool readMsg(int& msg);

    // Sends the appropriate message and notification signal
    int sendMsg(pid_t pi, int msg);
    
    // Shutdown the external plotter
    void stopPlotter();
    
    // Update the output filepath base if necessary based on the specified
    // timestamp and return the filepath base
    std::string updateOutputBase(const TimeKeeper&);

    // (Over)write PID to file
    bool writePID();
       

    // ------------------------
    // Private member variables
    // ------------------------
    int             m_argc;
    char**          m_argv;
    INIReader*      m_pIni;
    bool            m_bPlot;
    bool            m_bDump;
    bool            m_bStopSignal;
    int             m_iMode;
    unsigned int    m_uPlotBin;
    spawn*          m_pSpawn;
    std::string     m_strConfig;
    std::string     m_sDataDir;
    std::string     m_sSite;
    std::string     m_sInstrument;
    std::string     m_sUserSpecifiedAcqFilePath;
    std::string     m_sCurrentFilePathBase;
    TimeKeeper      m_tkCurrentFilePathTime;
    bool            m_bAutoName; 
    

};



#endif // _CONTROLLER_H_


