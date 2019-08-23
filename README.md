# fastspec
Multithreaded spectrometer code implemented in C++

OVERVIEW

FASTPEC is a spectrometer for the EDGES instrument.  It controls the receiver switch states and acquires samples from an analog-to-digital converter (ADC) and sends them through a polyphase filter bank (PFB) for channelization.  Channelization is multi-threaded. Accumulated spectra are stored to disk in an .acq file.

INSTALLATION

Install FASTSPEC by entering the fastspec directory and using 'make' to compile the executable.  FASTSPEC can be compiled in several ways:

make single - uses single floating point precision in the FFT and controls the receiver switch through a parallel port
              
make double - uses double floating point precision in the FFT and controls the receiver switch through a parallel port
               
make mezio - uses single point precision in the FFT and controls the receiver through a MezIO interface.

make simulate - generates a mock digitizer data stream and processes it identically to the "single" configuration above. A
                simulated receiver switch is used so no physical hardware is controlled.
          
Use 'sudo make [target]' to install the resulting executable in /usr/local/bin. It will also ensure the s-bit is set so for the  versions that need access to the parallel port.

RUNNING

Run FASTSPEC by calling the appropriate executable, e.g.:

>> fastspec_single

RUNTIME CONFIGURATION

FASTSPEC accepts many configuration settings, all of which are available through command line arguments and by using a .ini configuration file.  The list of configuration settings printed above shows all of the options supported.  The first term (e.g. 'Spectrometer') provides the .ini section under which the option is stored.  The second term (e.g. '-p') is a shorthand command line flag for setting a parameter value.  The third term (e.g. 'show_plots') provides the paramater name in the .ini file.  It can also be used on the command line as '--show_plots [value]'.  Command line flags override any values provided in a .ini file.

Some key parameters are:

-f        Specify an output file.  If not specified, output files are determined from the datadir parameter in the config file.
          
-h        View this help message.

-i        Specify the .ini configuration file.  If not specified, the default configuration file is tried (usually ./fastspec.ini)

PROCESS CONTROL

FASTSPEC can also be called to control the behavior of an already running instance.  Four commands are supported:

hide      Send HIDE PLOTS signal to an already running FASTSPEC instance.
kill      Send 'kill -9' signal to an already running FASTSPEC instance. 
show      Send SHOW PLOTS signal to an already running FASTSPEC instance.
stop      Send STOP signal to an already running FASTSPEC instance.

PLOTTING

FASTSPEC uses gnuplot for plotting ('-p', '--show_plots').  Gnuplot can be installed using the system package manager, e.g. sudo apt-get install gnuplot.  In addition to the built-in gnuplot commands, such as '+' and '-' to scale the plot, right clicking corners of a box to zoom, exporting to image files, etc., the FASTSPEC plot can be toggled between a plot showing the raw switch spectra and a plot showing the'corrected' (uncalibrated) antenna temperature by pressing 'x' in the plot window.
