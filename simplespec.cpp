
#define DEFAULT_INI_FILE "./simplespec.ini"

// Handle the digitizer configuration compiler flags
#if defined DIG_RAZORMAX
  #include "razormax.h"
#elif defined DIG_PXBOARD
  #include "pxboard.h"
#elif defined DIG_PXSIM
  #include "pxsim.h"
#elif
  #error Aborted in fastspec.cpp because digitizer flag not defined
#endif


#include "pfb.h"
#include "dumper.h"
#include "spectrometer_simple.h"
#include "utility.h"
#include "controller.h"
#include "version.h"
#include <fstream>      // ifstream, ofstream, ios


// ----------------------------------------------------------------------------
// The controller must be a global object so that the signal handling can
// be routed through it.
// ----------------------------------------------------------------------------
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
  printf("| SIMPLESPEC v%d.%d.%d - HELP\n", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
  printf("| ------------------------------------------------------------------------\n\n");

  printf("SIMPLESPEC is a generic spectrometer.  It acquires samples from an analog-to-digital\n");
  printf("converter (ADC) and sends them through a polyphase filter bank (PFB) for\n");
  printf("channelization.  Channelization is multi-threaded. Accumulated spectra\n");
  printf("are stored to disk in a binary file.\n\n");

  printf("SIMPLESPEC accepts many configuration settings, all of which are available\n");
  printf("through command line arguments and by using a .ini configuration file.\n");
  printf("The list of configuration settings printed above shows all of the\n");
  printf("options supported.  The first term (e.g. 'Spectrometer') provides the .ini\n");
  printf("section under which the option is stored.  The second term (e.g. '-p') is\n");
  printf("a shorthand command line flag for setting a parameter value.  The third\n");
  printf("term (e.g. 'show_plots') provides the paramater name in the .ini file.\n");
  printf("It can also be used on the command line as '--show_plots [value]'.\n");
  printf("Command line flags override any values provided in a .ini file.\n\n");

  printf("Some key parameters are:\n\n");

  printf("-f        Specify an output file.  If not specified, output files are\n");
  printf("          determined from the datadir parameter in the config file.\n");
  printf("\n");
  printf("-h        View this help message.\n");
  printf("\n");
  printf("-i        Specify the .ini configuration file.  If not specified, the\n");
  printf("          default configuration file is tried: \n\n");

  printf("               " DEFAULT_INI_FILE "\n\n");

  printf("SIMPLESPEC can also be called to control the behavior of an already running\n");
  printf("instance.  Six commands are supported:\n\n");

  
  printf("dump      Start dumping raw antenna samples to .dmp files.\n");
  printf("nodump    Stop dumping raw antenna samples to .dmp files.\n"); 
  printf("show      Show the live plotting window.\n");
  printf("hide      Hide the live plotting window.\n");
  printf("kill      Send the hard abort 'kill -9' signal to the already running instance.\n"); 
  printf("stop      Send a soft stop signal to the already running instance.\n\n");

  printf("SIMPLESPEC uses gnuplot for live plotting ('-p', '--show_plots').  Gnuplot can be\n");
  printf("installed using the system package manager, e.g. sudo apt-get install gnuplot.\n");
  printf("In addition to the built-in gnuplot commands, such as '+' and '-' to scale\n");
  printf("the plot, right clicking corners of a box to zoom, exporting to image files,\n");
  printf("etc., the SIMPLESPEC plot can be toggled between a plot showing the raw switch\n");
  printf("spectra and a plot showing the'corrected' (uncalibrated) antenna temperature\n"); 
  printf("by pressing 'x' in the plot window.\n\n");

}



// ----------------------------------------------------------------------------
// Main
// ----------------------------------------------------------------------------
int main(int argc, char* argv[])
{
    // Don't buffer stdout (so we see messages without lag)
    setbuf(stdout, NULL);

    printf("\n");
    printf("| ------------------------------------------------------------------------\n");
    printf("| SIMPLESPEC v%d.%d.%d\n", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
    printf("| ------------------------------------------------------------------------\n");
    printf("\n");


    // -----------------------------------------------------------------------
    // Initialize the process controller (which is declared globally at top)
    // -----------------------------------------------------------------------
    ctrl.registerHandler(on_signal);
    ctrl.setArgs(argc, argv);

    // Start in the commanded mode
    int iCtrlMode = CTRL_MODE_START;
    iCtrlMode = ctrl.getOptionBool("[ARGS]", "start", "start", true) ? CTRL_MODE_START : iCtrlMode;
    iCtrlMode = ctrl.getOptionBool("[ARGS]", "stop", "stop", false) ? CTRL_MODE_STOP : iCtrlMode;
    iCtrlMode = ctrl.getOptionBool("[ARGS]", "show", "show", false) ? CTRL_MODE_SHOW : iCtrlMode;
    iCtrlMode = ctrl.getOptionBool("[ARGS]", "hide", "hide", false) ? CTRL_MODE_HIDE : iCtrlMode;
    iCtrlMode = ctrl.getOptionBool("[ARGS]", "kill", "kill", false) ? CTRL_MODE_KILL : iCtrlMode;
    iCtrlMode = ctrl.getOptionBool("[ARGS]", "dump", "dump", false) ? CTRL_MODE_DUMP_START : iCtrlMode;
    iCtrlMode = ctrl.getOptionBool("[ARGS]", "nodump", "nodump", false) ? CTRL_MODE_DUMP_STOP : iCtrlMode;
    iCtrlMode = ctrl.getOptionBool("[ARGS]", "help", "-h", false) ? CTRL_MODE_HELP : iCtrlMode;

    // Set the controller's mode based on the above options.  If the
    // controller returns false, then the execution should terminate.
    if (!ctrl.run(iCtrlMode)) {
      return 0;
    }

    // -----------------------------------------------------------------------
    // Parse the configuration options from command line and INI file
    // -----------------------------------------------------------------------

    string sConfigFile = ctrl.getOptionStr("[ARGS]", "inifile", "-i", string(DEFAULT_INI_FILE));

    if (!ctrl.setINI(sConfigFile) && (iCtrlMode != CTRL_MODE_HELP)) {
      printf("Failed to parse .ini config file -- Using only command line\n");
      printf("arguments and default values.\n");
    }

    string sDataDir           = ctrl.getOptionStr("Installation", "datadir", "-d", "");
    string sSite              = ctrl.getOptionStr("Installation", "site", "-z", "");
    string sInstrument        = ctrl.getOptionStr("Installation", "instrument", "-j", "");
    string sUserOutput        = ctrl.getOptionStr("Spectrometer", "output_file", "-f", "");
    
  
    // Digitizer configuration
    long uSamplesPerAccum     = ctrl.getOptionInt("Spectrometer", "samples_per_accumulation", "-a", 1024L*2L*1024L*1024L);
    long uSamplesPerTransfer  = ctrl.getOptionInt("Spectrometer", "samples_per_transfer", "-t", 2*1024*1024);
    long uNumAccumulators		  = ctrl.getOptionInt("Spectrometer", "num_accumulators", "-o", 32);
    double dAcquisitionRate   = ctrl.getOptionInt("Spectrometer", "acquisition_rate", "-r", 400);
    
    #if defined DIG_PXBOARD    
      long uInputChannel        = ctrl.getOptionInt("Spectrometer", "input_channel", "-l", 2);
      long uVoltageRange        = ctrl.getOptionInt("Spectrometer", "voltage_range", "-v", 0);
    #elif defined DIG_PXSIM
      double dCWFreq1         = ctrl.getOptionReal("Spectrometer", "sim_cw_freq1", "-F1", 75);
      double dCWAmp1          = ctrl.getOptionReal("Spectrometer", "sim_cw_amp1", "-A1", 0.03);
      double dCWFreq2         = ctrl.getOptionReal("Spectrometer", "sim_cw_freq2", "-F2", 110);
      double dCWAmp2          = ctrl.getOptionReal("Spectrometer", "sim_cw_amp2", "-A2", 0.02);  
      double dNoiseAmp        = ctrl.getOptionReal("Spectrometer", "sim_noise_amp", "-AN", 0.5);
      double dOffset          = ctrl.getOptionReal("Spectrometer", "sim_offset", "-AO", 0.0);
    #endif
    
    // Channelizer configuration
    long uNumChannels         = ctrl.getOptionInt("Spectrometer", "num_channels", "-n", 65536);
    long uNumTaps             = ctrl.getOptionInt("Spectrometer", "num_taps", "-q", 5);
    long uWindowFunctionId    = ctrl.getOptionInt("Spectrometer", "window_function_id", "-w", 1);
    long uNumThreads          = ctrl.getOptionInt("Spectrometer", "num_fft_threads", "-m", 4);
    long uNumBuffers          = ctrl.getOptionInt("Spectrometer", "num_fft_buffers", "-b", 400);
        
    // Run configuration
    long uStopCycles          = ctrl.getOptionInt("Spectrometer", "stop_cycles", "-c", 0); 
    double dStopSeconds       = ctrl.getOptionReal("Spectrometer", "stop_seconds", "-s", 0);
    string sStopTime          = ctrl.getOptionStr("Spectrometer", "stop_time", "-u", "");
    bool bPlot                = ctrl.getOptionBool("Spectrometer", "show_plots", "-p", false);    
    long uPlotBin             = ctrl.getOptionInt("Spectrometer", "plot_bin", "-B", 1);    
    long uPlotInterval        = ctrl.getOptionInt("Spectrometer", "plot_interval_seconds", "-e", 10);  
   // bool bDump                = ctrl.getOptionBool("Spectrometer", "dump_raw_data", "-y", false); 


    // Handle help flag if set (do this after parsing configuration so that the
    // configuration options are visible as part of the help message).
    if (iCtrlMode == CTRL_MODE_HELP) {
      print_help();
      return 0;
    }

    // Set some controls (the controller parses the configuration, but doesn't
    // used any of the info until explictly told)
    ctrl.setPlot(bPlot, (unsigned int) uPlotBin); 
    ctrl.setOutputConfig(sDataDir, sSite, sInstrument, sUserOutput);

    // Calculate a few derived configuration parameters
    double dBandwidth = dAcquisitionRate / 2.0;
    unsigned int uNumFFT = uNumChannels * 2;
    printf("Bandwidth: %6.2f\n", dBandwidth);        
    printf("Samples per FFT: %d\n", uNumFFT);  

    // -----------------------------------------------------------------------
    // Check the configuration
    // -----------------------------------------------------------------------  
    if (uSamplesPerTransfer % uNumFFT != 0) {
      printf("WARNING: The number of samples per transfer is not a multiple "
             "of the number of FFT samples.  This wil likely lead to poor "
             "sidelobe performance in the channelizer (due to discontinuous "
             "time stream).  Also, will not be able to achieve highest "
             "possible duty cycle.\n\n");
    }

    if (uSamplesPerAccum % uNumFFT != 0) {
      printf("WARNING: The number of samples per accumulation is not a "
             "multiple of the number of FFT samples.  Will not be able to "
             "achieve highest possible duty cycle.\n\n");
    }

   
    // -----------------------------------------------------------------------
    // Initialize the digitizer board
    // -----------------------------------------------------------------------

    #if defined DIG_RAZORMAX
      RazorMax dig( dAcquisitionRate, 
                    0, // for continuous sampling
                    uSamplesPerTransfer ); 
    #elif defined DIG_PXBOARD
      PXBoard dig( dAcquisitionRate, 
                   0, // for continuous sampling
                   uSamplesPerTransfer, 
                   uInputChannel, 
                   uVoltageRange );
    #elif defined DIG_PXSIM
      PXSim dig( dAcquisitionRate, 
                 0, // for continuous sampling
                 uSamplesPerTransfer );
      dig.setSignal(dCWFreq1, dCWAmp1, dCWFreq2, dCWAmp2, dNoiseAmp, dOffset);
    #endif

    // Connect to the digitizer board
    if (!dig.connect(1)) { 
      printf("Failed to connect to digitizer board. Abort. \n");
      return 1;
    }

    // -----------------------------------------------------------------------
    // Initialize the asynchronous channelizer
    // -----------------------------------------------------------------------
    PFB chan ( uNumThreads,
               uNumBuffers,
               uNumChannels, 
               uNumTaps, 
               uWindowFunctionId,
               true );  // return in order

    // -----------------------------------------------------------------------
    // Initialize the Spectrometer
    // -----------------------------------------------------------------------
    SpectrometerSimple spec( uNumChannels, 
                             uSamplesPerAccum, 
                             dBandwidth,
                             uNumAccumulators,
                             (double) uPlotInterval,
                             (Digitizer*) &dig,
                             (Channelizer*) &chan,
                             &ctrl );

    if (uStopCycles > 0) { spec.setStopCycles(uStopCycles); }
    if (dStopSeconds > 0) { spec.setStopSeconds(dStopSeconds); }
    if (!sStopTime.empty()) { spec.setStopTime(sStopTime); }

    // -----------------------------------------------------------------------
    // Take data until the controller tells us it is time to stop
    // (usually when a SIGINT is received)
    // -----------------------------------------------------------------------    
    spec.run();

    return 0;
}
