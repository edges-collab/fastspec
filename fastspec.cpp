
#define DEFAULT_INI_FILE "./edges.ini" 

#ifdef SIMULATE
  #include "pxsim.h"
  #include "swsim.h"
  #define PX_DIGITIZER PXSim
  #define SWITCH SWSim
#else
  #include "pxboard.h"
  #define PX_DIGITIZER PXBoard
  #ifdef MEZIO
    #include "swneuosys.h"
    #define SWITCH SWNeuosys
  #else
    #include "swparallelport.h"
    #define SWITCH SWParallelPort
  #endif
#endif

#include "ini.h"
#include "pfb.h"
#include "spectrometer.h"
#include "switch.h"
#include "utility.h"
#include "controller.h"
#include <fstream>      // ifstream, ofstream, ios


Controller ctrl;

void on_signal(int iSignalNumber) {
  ctrl.onSignal(iSignalNumber);
}


// ----------------------------------------------------------------------------
// print_help - Print help information to terminal
// ----------------------------------------------------------------------------
void print_help()
{
  printf("\n");
  printf("| ------------------------------------------------------------------------\n");
  printf("| FASTSPEC - HELP\n");
  printf("| ------------------------------------------------------------------------\n\n"); 

  printf("FASTPEC is a spectrometer for the EDGES instrument.  It controls the\n");
  printf("receiver switch states and acquires samples from an analog-to-digital\n");
  printf("converter (ADC) and sends them through a polyphase filter bank (PFB) for\n");
  printf("channelization.  Channelization is multi-threaded. Accumulated spectra\n");
  printf("are stored to disk in an .acq file.\n\n");

  printf("FASTSPEC accepts many configuration settings, all of which are available\n");
  printf("through command line arguments and by using a .ini configuration file.\n");
  printf("The list of configuration settings printed above shows all of the\n");
  printf("options supported.  The first term (e.g. 'Spectrometer') provides the .ini\n");
  printf("section under which the option is stored.  The second term (e.g. '-p') is\n");
  printf("a shorthand command line flag for setting a parameter value.  The third\n");
  printf("term (e.g. 'show_plots') provides the paramater name in the .ini file.\n");
  printf("It can also be used on the command line as '--show_plots [value]'.\n");
  printf("Command line flags override any value provided in a .ini file.\n\n");

  printf("Some key parameters are:\n\n");

  printf("-f        Specify an output file.  If not specified, output files are\n");
  printf("          determined from the datadir parameter in the config file.\n");
  printf("\n");
  printf("-h        View this help message.\n");
  printf("\n");
  printf("-i        Specify the .ini configuration file.  If not specified, the\n");
  printf("          default configuration file is tried: \n\n");

  printf("               " DEFAULT_INI_FILE "\n\n");

  printf("FASTSPEC can also be called to control the behavior of an already running\n");
  printf("instance.  Three commands are supported:\n\n");

  printf("hide      Send HIDE PLOTS signal to a currently running FASTSPEC instance.\n");
  printf("show      Send SHOW PLOTS signal to a currently running FASTSPEC instance.\n");
  printf("stop      Send STOP signal to a currently running FASTSPEC instance.\n\n");
}


/*
// ----------------------------------------------------------------------------
// print_config - Print configuration to terminal
// ----------------------------------------------------------------------------
void print_config( const string& sConfigFile, const string& sSite, 
                   const string& sInstrument, const string& sOutput,
                   double dAcquisitionRate, double dBandwidth,
                   unsigned int uInputChannel, unsigned int uVoltageRange,
                   unsigned int uNumChannels, unsigned int uSamplesPerTransfer,
                   unsigned long uSamplesPerAccum, unsigned int uSwitchIOport,
                   double dSwitchDelay, unsigned int uNumFFT, 
                   unsigned int uNumThreads, unsigned int uNumBuffers, 
                   unsigned int uNumTaps, unsigned int uWindowFunctionId,
                   unsigned int uStopCycles, double dStopSeconds, 
                   const string& sStopTime, bool bStoreConfig )
{
  printf("\n");
  printf("| ------------------------------------------------------------------------\n");
  printf("| FASTSPEC\n");
  printf("| ------------------------------------------------------------------------\n");  
  printf("| \n");
  printf("| Configuration file: %s\n", sConfigFile.c_str());
  printf("| \n");    
  printf("| Installation - Site: %s\n", sSite.c_str());
  printf("| Installation - Instrument: %s\n", sInstrument.c_str());
  printf("| Installation - Output path: %s\n", sOutput.c_str());
  printf("| \n");      
  printf("| Spectrometer - Digitizer - Input channel: %d\n", uInputChannel);
  printf("| Spectrometer - Digitizer - Voltage range mode: %d\n", uVoltageRange);
  printf("| Spectrometer - Digitizer - Number of channels: %d\n", uNumChannels);
  printf("| Spectrometer - Digitizer - Acquisition rate: %g\n", dAcquisitionRate);
  printf("| Spectrometer - Digitizer - Samples per transfer: %d\n", uSamplesPerTransfer);
  printf("| Spectrometer - Digitizer - Samples per accumulation: %lu\n", uSamplesPerAccum);
  printf("| \n"); 
  printf("| Spectrometer - Switch - Receiver switch IO port: 0x%x\n", uSwitchIOport); 
  printf("| Spectrometer - Switch - Receiver switch delay (seconds): %g\n", dSwitchDelay); 
  printf("| \n");   
  printf("| Spectrometer - Channelizer - Number of FFT threads: %d\n", uNumThreads);
  printf("| Spectrometer - Channelizer - Number of FFT buffers: %d\n", uNumBuffers);
  printf("| Spectrometer - Channelizer - Number of polyphase filter bank taps: %d\n", uNumTaps);
  printf("| Spectrometer - Channelizer - Window function ID: %d\n", uWindowFunctionId);  
  printf("| Spectrometer - Channelizer - Stop after cycles: %d\n", uStopCycles);
  printf("| Spectrometer - Channelizer - Stop after seconds: %g\n", dStopSeconds);
  printf("| Spectrometer - Channelizer - Stop at UTC: %s\n", (sStopTime.empty() ? "---" : sStopTime.c_str()));
  printf("| Spectrometer - Channelizer - Store copy of configuration file: %d\n", bStoreConfig);
  printf("| \n");  
  printf("| Bandwidth: %6.2f\n", dBandwidth);        
  printf("| Samples per FFT: %d\n", uNumFFT);    
  printf("| \n");              
  printf("| ------------------------------------------------------------------------\n"); 
  printf("\n");
}

*/


// ----------------------------------------------------------------------------
// Main
// ----------------------------------------------------------------------------
int main(int argc, char* argv[])
{   
    // Don't buffer stdout (so we see messages without lag)
    setbuf(stdout, NULL);

    printf("\n");
    printf("| ------------------------------------------------------------------------\n");
    printf("| FASTSPEC\n");
    printf("| ------------------------------------------------------------------------\n");
    printf("\n");  


    // -----------------------------------------------------------------------
    // Initialize the process controller (which is declared globally at top)
    // -----------------------------------------------------------------------
    ctrl.registerHandler(on_signal);
    ctrl.setArgs(argc, argv);

    // Start in the commanded mode 
    int iCtrlMode = CTRL_MODE_START;
    iCtrlMode = ctrl.getOptionBool("[ARGS]", "stop", "stop", false) ? CTRL_MODE_STOP : iCtrlMode;
    iCtrlMode = ctrl.getOptionBool("[ARGS]", "show", "show", false) ? CTRL_MODE_SHOW : iCtrlMode;
    iCtrlMode = ctrl.getOptionBool("[ARGS]", "hide", "hide", false) ? CTRL_MODE_HIDE : iCtrlMode;

    if (!ctrl.run(iCtrlMode)) {
      return 0;
    }


    // -----------------------------------------------------------------------
    // Parse the configuration options from command line and INI file
    // -----------------------------------------------------------------------

    string sConfigFile = ctrl.getOptionStr("[ARGS]", "inifile", "-i", string(DEFAULT_INI_FILE));

    if (!ctrl.setINI(sConfigFile)) {
      printf("Failed to parse .ini config file -- Using only command line\n");
      printf("arguments and default values.\n");
    }

    string sDataDir           = ctrl.getOptionStr("Installation", "datadir", "-d", "");
    string sSite              = ctrl.getOptionStr("Installation", "site", "-z", "");
    string sInstrument        = ctrl.getOptionStr("Installation", "instrument", "-j", "");
    string sOutput            = ctrl.getOptionStr("Spectrometer", "output_file", "-f", "");
    long uSwitchIOPort        = ctrl.getOptionInt("Spectrometer", "switch_io_port", "-o", 0x3010);
    double dSwitchDelay       = ctrl.getOptionReal("Spectrometer", "switch_delay", "-e", 0.5);
    long uInputChannel        = ctrl.getOptionInt("Spectrometer", "input_channel", "-l", 2);
    long uNumChannels         = ctrl.getOptionInt("Spectrometer", "num_channels", "-n", 65536);
    long uSamplesPerAccum     = ctrl.getOptionInt("Spectrometer", "samples_per_accumulation", "-a", 1024L*2L*1024L*1024L);
    long uSamplesPerTransfer  = ctrl.getOptionInt("Spectrometer", "samples_per_transfer", "-t", 2*1024*1024);
    long uVoltageRange        = ctrl.getOptionInt("Spectrometer", "voltage_range", "-v", 0);
    double dAcquisitionRate   = ctrl.getOptionInt("Spectrometer", "acquisition_rate", "-r", 400);
    long uNumThreads          = ctrl.getOptionInt("Spectrometer", "num_fft_threads", "-m", 4);
    long uNumBuffers          = ctrl.getOptionInt("Spectrometer", "num_fft_buffers", "-b", 400);
    long uNumTaps             = ctrl.getOptionInt("Spectrometer", "num_taps", "-q", 5);
    long uWindowFunctionId    = ctrl.getOptionInt("Spectrometer", "window_function_id", "-w", 1);
    bool bStoreConfig         = ctrl.getOptionBool("Spectrometer", "store_config", "-k", false);
    long uStopCycles          = ctrl.getOptionInt("Spectrometer", "stop_cycles", "-c", 0); 
    double dStopSeconds       = ctrl.getOptionReal("Spectrometer", "stop_seconds", "-s", 0);
    string sStopTime          = ctrl.getOptionStr("Spectrometer", "stop_time", "-u", "");
    bool bPlot                = ctrl.getOptionBool("Spectrometer", "show_plots", "-p", false);    

    // Handle help flag if set
    if (ctrl.getOptionBool("[ARGS]", "help", "-h", false)) {
      print_help();
      return 0;
    }

    // Set controls
    ctrl.setPlot(bPlot);

    // Calculate a few derived configuration parameters
    double dBandwidth = dAcquisitionRate / 2.0;
    unsigned int uNumFFT = uNumChannels * 2;
    printf("Bandwidth: %6.2f\n", dBandwidth);        
    printf("Samples per FFT: %d\n", uNumFFT);    
    printf("\n");

    bool bDirectory = sOutput.empty();
    if (bDirectory) { 
      sOutput = sDataDir + "/" + sSite + "/" + sInstrument; 
    }

    // -----------------------------------------------------------------------
    // Check the configuration
    // -----------------------------------------------------------------------  
    if (uSamplesPerTransfer % uNumFFT != 0) {
      printf("Warning: The number of samples per transfer is not a multiple "
             "of the number of FFT samples.  Will not be able to achieve "
             "100%% duty cycle.\n\n");
    }

    if (uSamplesPerAccum % uNumFFT != 0) {
      printf("Warning: The number of samples per accumulation is not a "
             "multiple of the number of FFT samples.\n\n");
    }


    // -----------------------------------------------------------------------
    // Store a copy of the ini file with the data we're about to write
    // -----------------------------------------------------------------------
    if (bStoreConfig) {

      ifstream src(sConfigFile, ios::binary);
      string sSuffix = ".ini";
      string sStoreFile;     

      if (bDirectory) {

        // Use the same naming scheme as the spectrometer output, but with more date detail
        TimeKeeper tk;
        tk.setNow();
        string sDateString = tk.getFileString(5);
        sStoreFile = construct_filepath_name(sOutput, sDateString, sInstrument, sSuffix);

      } else {
        
        // Use the name provided
        sStoreFile = sOutput + sSuffix;
      }
      
      printf("Storing copy of configuration in: %s\n", sStoreFile.c_str());
      if (make_path(get_path(sStoreFile), 0755)) {
        ofstream dst(sStoreFile, ios::binary);
        dst << src.rdbuf();     
        printf("Done.\n");
      } else {
        printf("Failed.\n");
      }
    }

    // -----------------------------------------------------------------------
    // Initialize the receiver switch
    // -----------------------------------------------------------------------   
    SWITCH sw; 
    if (!sw.init(uSwitchIOPort)) {
      printf("Failed to control parallel port.  Abort.\n");
      return 1;
    }
    sw.set(0);

    // -----------------------------------------------------------------------
    // Initialize the digitizer board
    // -----------------------------------------------------------------------       
    PX_DIGITIZER dig;
    dig.setInputChannel(uInputChannel);
    dig.setVoltageRange(1, uVoltageRange);
    dig.setVoltageRange(2, uVoltageRange);
    dig.setAcquisitionRate(dAcquisitionRate);
    dig.setTransferSamples(uSamplesPerTransfer); // should be a multiple of number of FFT samples

    // Connect to the digitizer board
    if (!dig.connect(1)) { 
      printf("Failed to connect to PX board. Abort. \n");
      return 1;
    }

    // -----------------------------------------------------------------------
    // Initialize the asynchronous channelizer
    // -----------------------------------------------------------------------
    PFB chan ( uNumThreads,
               uNumBuffers,
               uNumChannels, 
               uNumTaps );

    // Select the window function
    chan.setWindowFunction(uWindowFunctionId);

    // -----------------------------------------------------------------------
    // Initialize the Spectrometer
    // -----------------------------------------------------------------------
    Spectrometer spec( uNumChannels, 
                       uSamplesPerAccum, 
                       dBandwidth, 
                       dSwitchDelay,
                       (Digitizer*) &dig,
                       (Channelizer*) &chan,
                       (Switch*) &sw, 
                       &ctrl );

    spec.setOutput(sOutput, sInstrument, bDirectory);

    if (uStopCycles > 0) { spec.setStopCycles(uStopCycles); }
    if (dStopSeconds > 0) { spec.setStopSeconds(dStopSeconds); }
    if (!sStopTime.empty()) { spec.setStopTime(sStopTime); }


    // -----------------------------------------------------------------------
    // Take data until the controller tell us it is time to stop
    // (usually when a SIGINT is received)
    // -----------------------------------------------------------------------    
    spec.run();

    return 0;
}
