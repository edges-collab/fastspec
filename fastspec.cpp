
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
  printf("| EDGES Spectrometer\n");
  printf("| ------------------------------------------------------------------------\n");  
  printf("\n");
  printf("-f        Specify an output file.  If not specified, output files are\n");
  printf("          determined from the datadir parameter in the config file.\n");
  printf("\n");
  printf("-h        View this help message.\n");
  printf("\n");
  printf("-i        Specify the .ini configuration file.  If not specified, the\n");
  printf("          default configuration file is tried: \n\n");
  printf("               " DEFAULT_INI_FILE "\n");
  printf("\n");
  printf("stop      Send stop signal to any running spectrometer processes.\n");
  printf("\n");
}



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




// ----------------------------------------------------------------------------
// Main
// ----------------------------------------------------------------------------
int main(int argc, char* argv[])
{   
    string sConfigFile(DEFAULT_INI_FILE);
    string sOutput;
    bool bDirectory = true;
    unsigned long uStopCycles = 0;
    double dStopSeconds = 0;
    string sStopTime;
    int iCtrlMode = CTRL_MODE_START;

    setbuf(stdout, NULL);


    // -----------------------------------------------------------------------
    // Parse the command line 
    // -----------------------------------------------------------------------
    for(int i=0; i<argc; i++)
    {
      string sArg = argv[i];

      if (sArg.compare("-f") == 0) { 
        // Specify an output filepath instead of the usual directory structure
        if (argc > i) {
          sOutput = argv[i+1]; 
          bDirectory = false;
        }
      } else if (sArg.compare("-h") == 0) { 
        // Show help text
        print_help();
        return 0;
      } else if (sArg.compare("-i") == 0) { 
        // Specify a config file at a different path than the default path
        if (argc > i) {
          sConfigFile = argv[i+1];
        } 
      } else if (sArg.compare("-c") == 0) { 
        // Stop after cycles
        if (argc > i) {
          uStopCycles = strtoul(argv[i+1], NULL, 10);
        }
      } else if (sArg.compare("-s") == 0) { 
        // Stop after seconds since start
        if (argc > i) {
          dStopSeconds = strtof(argv[i+1], NULL);
        }
      } else if (sArg.compare("-t") == 0) {
        // Stop at specific time
        if (argc > i) {
          sStopTime = argv[i+1];
        }
      } else if (sArg.compare("stop") == 0) {
        iCtrlMode = CTRL_MODE_STOP;
      }
    }

    // -----------------------------------------------------------------------
    // Initialize the process controller (which is declared globally at top)
    // -----------------------------------------------------------------------
    ctrl.registerHandler(on_signal);
    if (!ctrl.run(iCtrlMode)) {
      return 0;
    }

    // -----------------------------------------------------------------------
    // Parse the Spectrometer configuration .ini file
    // -----------------------------------------------------------------------
    INIReader reader(sConfigFile);

    if (reader.ParseError() < 0) {
      printf("Failed to parse .ini config file.  Abort.\n");
      return 1;
    }

    string sDataDir = reader.Get("Installation", "datadir", "");
    string sSite = reader.Get("Installation", "site", "");
    string sInstrument = reader.Get("Installation", "instrument", "");
    unsigned int uSwitchIOPort = reader.GetInteger("Spectrometer", "switch_io_port", 0x3010);
    double dSwitchDelay = reader.GetReal("Spectrometer", "switch_delay", 0.5);
    unsigned int uInputChannel = reader.GetInteger("Spectrometer", "input_channel", 2);
    unsigned int uNumChannels = reader.GetInteger("Spectrometer", "num_channels", 65536);
    unsigned long uSamplesPerAccum = reader.GetInteger("Spectrometer", "samples_per_accumulation", 1024L*2L*1024L*1024L);
    unsigned int uSamplesPerTransfer = reader.GetInteger("Spectrometer", "samples_per_transfer", 2*1024*1024);
    unsigned int uVoltageRange = reader.GetInteger("Spectrometer", "voltage_range", 0);
    double dAcquisitionRate = reader.GetReal("Spectrometer", "acquisition_rate", 400);
    unsigned int uNumThreads = reader.GetInteger("Spectrometer", "num_fft_threads", 4);
    unsigned int uNumBuffers = reader.GetInteger("Spectrometer", "num_fft_buffers", 400);
    unsigned int uNumTaps = reader.GetInteger("Spectrometer", "num_taps", 1);
    unsigned int uWindowFunctionId = reader.GetInteger("Spectrometer", "window_function_id", 1);
    bool bStoreConfig = reader.GetBoolean("Spectrometer", "store_config", false);

    if (uStopCycles == 0) { 
      uStopCycles = reader.GetInteger("Spectrometer", "stop_cycles", 0); 
    }
    if (dStopSeconds == 0) {
      dStopSeconds = reader.GetReal("Spectrometer", "stop_seconds", 0);
    }
    if (sStopTime.empty()) {
      string sStopTime = reader.Get("Spectrometer", "stop_time", "");
    }

    // Calculate a few derived configuration parameters
    double dBandwidth = dAcquisitionRate / 2.0;
    unsigned int uNumFFT = uNumChannels * 2;
    if (bDirectory) { 
      sOutput = sDataDir + "/" + sSite + "/" + sInstrument; 
    }


    // -----------------------------------------------------------------------
    // Print the configuration to terminal
    // ----------------------------------------------------------------------- 
    //printf("INI File -- relevant settings\n\n");
    //reader.Print(string("installation"));
    //printf("\n");
    //reader.Print(string("spectrometer"));
    //printf("\n");

    print_config( sConfigFile, sSite, sInstrument, sOutput, dAcquisitionRate, 
                  dBandwidth, uInputChannel, uVoltageRange, uNumChannels, 
                  uSamplesPerTransfer, uSamplesPerAccum, uSwitchIOPort, 
                  dSwitchDelay, uNumFFT, uNumThreads, uNumBuffers, uNumTaps, 
                  uWindowFunctionId, uStopCycles, dStopSeconds, sStopTime, 
                  bStoreConfig );

    
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
    chan.setWindowFunction(1);

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
