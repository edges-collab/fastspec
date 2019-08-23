# fastspec
Multithreaded spectrometer code implemented in C++

OVERVIEW

FASTPEC is a spectrometer for the EDGES instrument.  It controls the receiver switch states and acquires samples from an analog-to-digital converter (ADC) and sends them through a polyphase filter bank (PFB) for channelization.  Channelization is multi-threaded. Accumulated spectra are stored to disk in an .acq file.

INSTALLATION

Prior to building, ensure the following dependencies are installed on the system:

* fftw3f-dev (for single precision versions)

* fftw3-dev (for double-precision versions) 

* sig_px14400 (not needed for simulator version)

* wdt_dio (for Mezio version)

Compile FASTSPEC by entering the fastspec directory and using 'make' to compile the executable.  FASTSPEC can be compiled in several ways:

* make single - uses single floating point precision in the FFT and controls the receiver switch through a parallel port
              
* make double - uses double floating point precision in the FFT and controls the receiver switch through a parallel port
               
* make mezio - uses single point precision in the FFT and controls the receiver through a MezIO interface.

* make simulate - generates a mock digitizer data stream and processes it identically to the "single" configuration above. A simulated receiver switch is used so no physical hardware is controlled.
          
Use 'sudo make [target]' when building allow installing the resulting executable in /usr/local/bin. It will also ensure the s-bit is set for versions that need access to the parallel port.

RUNNING

Run FASTSPEC by calling the appropriate executable, e.g.:

>> fastspec_single

RUNTIME CONFIGURATION

FASTSPEC accepts many configuration settings, all of which are available through command line arguments and by using a .ini configuration file.  See 'example.ini' for a sample configuration file.  Below is a list of supported configuration settings and sample values:

* ([ARGS ONLY]) start --start: 0
* ([ARGS ONLY]) stop --stop: 0
* ([ARGS ONLY]) show --show: 0
* ([ARGS ONLY]) hide --hide: 0
* ([ARGS ONLY]) kill --kill: 0
* ([ARGS ONLY]) -h --help: 1
* ([ARGS ONLY]) -i --inifile: ./fastspec.ini

(Installation) -d --datadir: /home/user/data
(Installation) -z --site: mro
(Installation) -j --instrument: low1 
(Spectrometer) -f --output_file: 
(Spectrometer) -o --switch_io_port: 0x3010
(Spectrometer) -e --switch_delay: 0.5
(Spectrometer) -l --input_channel: 1 
(Spectrometer) -v --voltage_range: 0 
(Spectrometer) -a --samples_per_accumulation: 2147483648 
(Spectrometer) -t --samples_per_transfer: 2097152
(Spectrometer) -r --acquisition_rate: 400 
(Spectrometer) -n --num_channels: 65536 
(Spectrometer) -q --num_taps: 5 
(Spectrometer) -w --window_function_id: 3 
(Spectrometer) -m --num_fft_threads: 4 
(Spectrometer) -b --num_fft_buffers: 400 
(Spectrometer) -c --stop_cycles:  
(Spectrometer) -s --stop_seconds: 
(Spectrometer) -u --stop_time: YYYY/MM/DDThh:mm:ss [UTC]
(Spectrometer) -p --show_plots: 0
(Spectrometer) -B --plot_bin: 1 
(Spectrometer) -F1 --sim_cw_freq1: 75
(Spectrometer) -A1 --sim_cw_amp1: 0.03
(Spectrometer) -F2 --sim_cw_freq2: 40
(Spectrometer) -A2 --sim_cw_amp2: 0.02
(Spectrometer) -AN --sim_noise_amp: 0.001

The first term (e.g. 'Spectrometer') provides the .ini section under which the option is stored when entered into a configuration file.  The second term (e.g. '-p') is a shorthand command line flag for setting a parameter value.  The third term (e.g. 'show_plots') provides the paramater name in the .ini file.  It can also be used on the command line as '--show_plots [value]'.  Command line flags override any values provided in a .ini file.

Some key parameters are:

-f: Specify an output file.  Overwrites any existing file at the specified path.  If no output file is specified on the command line or in a .ini file, FASTSPEC automatically determines output file paths from the datadir, site, and instrument parameters along with the time in UTC when spectra from each switch cycle is written.
          
-h: View this help message.

-i: Specify the .ini configuration file.  If not specified, the default configuration file is tried (usually ./fastspec.ini)

PROCESS CONTROL

FASTSPEC can also be called to control the behavior of an already running instance.  Four commands are supported:

hide: Send HIDE PLOTS signal to an already running FASTSPEC instance.
kill: Send 'kill -9' signal to an already running FASTSPEC instance. 
show: Send SHOW PLOTS signal to an already running FASTSPEC instance.
stop: Send STOP signal to an already running FASTSPEC instance.

PLOTTING

FASTSPEC uses gnuplot for plotting ('-p', '--show_plots').  Gnuplot can be installed using the system package manager, e.g.:

'sudo apt-get install gnuplot'  

In addition to the built-in gnuplot commands, such as '+' and '-' to scale the plot, right clicking corners of a box to zoom, exporting to image files, etc. (see http://www.gnuplot.info/ for more information), the FASTSPEC plot can be toggled between a plot showing the three raw switch position spectra and a plot showing the'corrected' (uncalibrated) antenna temperature by pressing 'x' in the plot window.

