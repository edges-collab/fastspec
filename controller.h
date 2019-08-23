#ifndef _CONTROLLER_H_
#define _CONTROLLER_H_

#define PID_FILE        "/tmp/fastspec.pid"
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

    // Return mode of operation
    int mode() const;

    // Called by the global wrapper when a unix signal is received
    void onSignal(int); 

    // Handle the writing of data to disk after each spectrometer switch cycle
    void onSpectrometerData(Accumulator&, Accumulator&, Accumulator&);

    // True if should show plots, false if should hide plots
    bool plot() const;

    // Set the run mode and check for an existing application
    // instance.  Returns false if this application should 
    // stop.
    bool run(int);

    // Give the controller the commandline arguments for later parsing
    void setArgs(int argc, char**);

    // Give the controller the INI file for parsing
    bool setINI(const std::string&);

    // Set the output file or directory for writing spectrometer data
    void setOutput( const std::string&, const std::string&, 
                    const std::string&, const std::string& );

    // Tell the controller that there should be plotting and how to bin data
    void setPlot(bool, unsigned int);

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

    // Delete existing PID file
    void deletePlot();

    // Returns the filepath that should be used to write spectrometer data
    std::string getAcqPath();

    // Get the start time associated with a PID
    unsigned long long getProcStart(pid_t pid);

    // Read existing control file and get its contained pid and starttime.
    // Returns false if can't open the file
    bool readControlFile(pid_t& pid, unsigned long long& tm); 

    // Sends the stop signal
    int sendStopSignal(pid_t pid);

    // Sends a hard kill signal
    int sendKillSignal(pid_t pid);

    // Sends a signal to show plots
    int sendShowPlotSignal(pid_t pid);

    // Sends a signal to stop showing plots
    int sendHidePlotSignal(pid_t pid);

    // Shutdown the external plotter
    void stopPlotter();

    // (Over)write PID to file
    bool writeControlFile();

    // ------------------------
    // Private member variables
    // ------------------------
    int             m_argc;
    char**          m_argv;
    INIReader*      m_pIni;
    bool            m_bPlot;
    bool            m_bStopSignal;
    int             m_iMode;
    unsigned int    m_uPlotBin;
    spawn*          m_pSpawn;
    std::string     m_strConfig;
    std::string     m_sDataDir;
    std::string     m_sSite;
    std::string     m_sInstrument;
    std::string     m_sOutput;
    bool            m_bDirectory; 
    TimeKeeper      m_tkPrevStartTime;

};



#endif // _CONTROLLER_H_


