# fastspec
Multithreaded spectrometer code implemented in C++

## Overview

FASTPEC is a spectrometer for the EDGES instrument.  It controls the receiver switch states and acquires samples from an analog-to-digital converter (ADC) and sends them through a polyphase filter bank (PFB) for channelization.  Channelization is multi-threaded. Accumulated spectra are stored to disk in an `.acq` file.

## Installation

### Dependencies
Prior to building, ensure the following dependencies are installed on the system:

* `fftw3f-dev` - Only needed for Ubuntu 18.04 LTS or lower.  Provides single precision FFT.
* `fftw3-dev` - Provides double-precision FFT.  For Ubuntu 20.04 LTS and later it also provides single precision FFT.
* `sig_px14400` - Only needed for digitizer=pxboard.  Must be compiled from driver source code.  Drivers can be downloaded from Vitrek (https://vitrek.com/signatec/support/downloads-2/) or the EDGES Google Drive.  The latest released driver supports Ubuntu 16.04 LTS through Ubuntu 19.10.  See also our repositoriy at https://github.com/edges-collab/px14400_patch for updates to the drivers for more recent Linux kernels.
* `CsE16bcd` - Only needed for digitizer=raxormax.  Must compile from driver source code.  The current supported version (as of July 2023) includes Ubuntu 20.04 LTS.  See additional installation notes below.
* `wdt_dio` - Only needed for switch=mezio.  Provides interface to the MezIO system.
* `gnuplot-qt` - (Optional) Provides GUI for displaying live spectra plots.
* `exodriver` - Only needed for switch=labjack.  Provides interface to LabJack.  Download and build using git clone https://github.com/labjack/exodriver.git
* `libusb-1.0-0-dev` - Only needed for switch=labjack.  

### Supported Digitizer Boards
* `PX14400` - use make flag: digitizer=pxboard
* `RazorMax` - use make flag: digitizer=razormax

### Build Configuration and Process
This FASTSPEC repository supports building two applications: FASTSPEC and SIMPLESPEC.  After installing the dependencies, build either application by entering the `fastspec/` directory and using `make` to compile the executable.  FASTSPEC is configured using `make` flags following the form:
```
$ make application=<flag> digitizer=<flag> switch=<flag> precision=<flag>
```
Supported application flags are:
* `fastspec` - Builds a binary executable for the FASTSPEC spectrometer customized to support EDGES instruments.  This application cycles between the EDGES three-position switch states using the method specified in the `switch=<flag>` argument of the make command line.  All make command line flags are supported.  Output is written to the historical EDGES file format (.acq), which is uuencoded ASCII.
* `simplespec` - Builds a binary exectuable for the SIMPLESPEC spectrometer that is a generic streaming spectrometer that dumps accumulated spectra to a binary file (.ssp).  All make command line flags are supported except `switch`, which is ignored if present.

Supported digitizer flags are:

* `pxboard` - Requires the `sig_px14400` driver and px14.h header file to be installed (see dependencies above).
* `pxsim` - No required drivers.  Gnerates a mock digitizer data stream.  No physical digitizer is used.  Helpful for testing other aspects of the code. (very slow)
* `razormax` - Requires the Gage Linux SDK/dirver and associated header files to be installed (see EDGES Google Shared driver for latest driver source code)

Supported switch flags are:

* `labjack` - Requires `exodriver` and `libusb-1.0-0-dev` to be isntalled.  Assumed a single LabJack U3 is connected and its digital IO ports E1, E3, E5, and EIO7 are used to control an EDGES Control Circuit (CC1) board. 
* `mezio` - Requires `wdt_dio` to be installed and uses the MezIO system.
* `parallelport` - Uses the parallel port specified in the runtime configuration.
* `sim` - Dummy switch that has no effect on any hardware.  Helpful for testing other aspects of the code.
* `tty` - Uses the TTY device and command strings specified in the runtime configuration.  

Fastspec supports two build-time floating point precision options for the FFT used in the polyphase filter bank: single (32 bit) and double (64 bit).  These can be set with precision flags:

* `single` - Default option if no precision flag is set
* `double` - Double precision FFT significantly slows execution (requires more threads).

The `make` process will ask for `sudo` privileges at the end of the build to set the s-bit on the final executable (not sure if this needed for all build/execution cases, but it is included for all).

The name of the output exectuable is based on the flags provided to `make` and follows the form:  fastspec_[digitizer]_[switch]__[precision]

To install the output excutable to /usr/local/bin after building, use the `install` target on the `make` command line, e.g.:
```$ make install digitizer=razormax switch=parallelport```

To clean the build directory and remove all fastspec executables from /usr/local/bin, use: `make clean`

## Usage

Run FASTSPEC by calling the appropriate executable, e.g.:

```
$ fastspec_pxboard_mezio_single
```

### Runtime Configuration

FASTSPEC accepts many configuration settings, all of which are available through command line arguments and by using a `.ini` configuration file.  See `example.ini` for a sample configuration file.  Below is a list of supported configuration settings and sample values.

Each heading (e.g. 'Spectrometer') provides the `.ini` section under which the option is stored when entered into a configuration file.  The first term (e.g. '-p') is a shorthand command line flag for setting a parameter value.  The second term (e.g. `show_plots`) provides the paramater name in the .ini file.  It can also be used on the command line as '--show_plots [value]'.  Command line flags override any values provided in a `.ini` file.


#### `[ARGS_ONLY]`
* `start --start`: 0
* `stop --stop`: 0
* `show --show`: 0
* `hide --hide`: 0
* `kill --kill`: 0
* `dump --dump`: 0
* `nodump --nodump`: 0
* `-h --help`: 1
* `-i --inifile`: `./fastspec.ini`

#### Installation
* `-d --datadir`: `/home/user/data`
* `-z --site`: mro
* `-j --instrument`: low1 

#### Spectrometer
* `-f --output_file`: 
* `-o --switch_io_port`: 0x3010 [only used for parallelport switch]
* `-e --switch_delay`: 0.5
* `-l --input_channel`: 1 [only used pxboard digtizer]
* `-v --voltage_range`: 0 [only used pxboard digtizer]
* `-a --samples_per_accumulation`: 2147483648 
* `-t --samples_per_transfer`: 2097152
* `-r --acquisition_rate`: 400 
* `-n --num_channels`: 65536 
* `-q --num_taps`: 5 
* `-w --window_function_id`: 3 
* `-m --num_fft_threads`: 4 
* `-b --num_fft_buffers`: 400 
* `-c --stop_cycles`:  
* `-s --stop_seconds`: 
* `-u --stop_time`: YYYY/MM/DDThh:mm:ss [UTC]
* `-p --show_plots`: 0
* `-B --plot_bin`: 1
* `-M --num_dump_buffers`: 1000
* `-y --dump_raw_data`: 0 
* `-F1 --sim_cw_freq1`: 75
* `-A1 --sim_cw_amp1`: 0.03
* `-F2 --sim_cw_freq2`: 40
* `-A2 --sim_cw_amp2`: 0.02
* `-AN --sim_noise_amp`: 0.001
* `-AO --sim_offset`: 0.0

Some key parameters are:

* `-f`: Specify an output file.  Overwrites any existing file at the specified path.  If no output file is specified on the command line or in a .ini file, FASTSPEC automatically determines output file paths from the datadir, site, and instrument parameters along with the time in UTC when spectra from each switch cycle are written.
* `-h`: View this help message.
* `-i`: Specify the `.ini` configuration file.  If not specified, the default configuration file is tried (usually ./fastspec.ini)

### Process Control

FASTSPEC can also be called to control the behavior of an already running instance.  Four commands are supported:

* `hide`: Send HIDE PLOTS signal to an already running FASTSPEC instance.
* `kill`: Send 'kill -9' signal to an already running FASTSPEC instance. 
* `show`: Send SHOW PLOTS signal to an already running FASTSPEC instance.
* `stop`: Send STOP signal to an already running FASTSPEC instance.

### Plotting

FASTSPEC uses gnuplot for plotting (`-p`, `--show_plots`).  Gnuplot can be installed using the system package manager, e.g.:

```
$ sudo apt install gnuplot
```
In addition to the built-in gnuplot commands, such as '+' and '-' to scale the plot, right clicking corners of a box to zoom, exporting to image files, etc. (see http://www.gnuplot.info/ for more information), the FASTSPEC plot can be toggled between a plot showing the three raw switch position spectra and a plot showing the'corrected' (uncalibrated) antenna temperature by pressing 'x' in the plot window.

### RazorMax (Gage SDK and Driver) Installation Notes and Tips

Note: The RazorMax board needs to have the eXpert Data Streaming firmware option loaded (extra purchase when buying the board).  To install the streaming firmware, you will probably need to insert the board in a Windows machine and run the CompuScope Manager utility.  Follow the instructions in the manual.

To install the driver for the RazorMax digitizer on Ubuntu 20.04 LTS, follow these steps:

1. Download CS_Linux_5.04.40.zip from the EDGES Google Shared Drive folder: "EDGES/Project Engineering/2. Digitizers/Gage_RazorMax" and unpack.
2. cd into gati-linux-driver folder
3. Make sure .sh files are executable
4. For systems using Secure Boot (e.g. dual-boot computers with Windows), you will need to sign the kernel module with a certificate recognized by the trusted platform module.  
- Check if /var/lib/shim-signed/mok contains the files: MOK.priv and MOK.der.  If so you likely have a certificate that is already registered that you can use to sign the kernel module.  If you do not have the MOK files or if the driver build ultimately reports an error that the kernel module can be loaded, then you will need to make a new certificate/key and enroll it.  Do this by entering:
  ```
  $ sudo update-secureboot-policy new-key
  ```
  You will be prompted to enter a temporary password that you will need to use after rebooting the system to enroll the certificate.  After creating the key, reboot the computer.  The BIOS should halt the reboot and ask if you want to enroll the new key.  Follow the instructions to do so and then resume the boot.
- If you have existing MOK files, but the build ultimately reports an error that the kernel module can be loaded, you may have to add the existing certificate/key to the Ubuntu manager:
  ```
  $ sudo mokutil --import /var/lib/shim-signed/mok/MOK.der
  ```
- After getting your certificate/key registered, open the Gage driver file: ./Boards/KernelSpace/CsHexagon/Linux/Makefile and add the following line to the top of the "install" target section:
  ```
  @sudo kmodsign sha512 /var/lib/shim-signed/mok/MOK.priv /var/lib/shim-signed/mok/MOK.der $(TARGET)
  ```
- Note: If you want to try this manually for debugging, the $(TARGET) = CsE16bcd.ko
5. Run the gage install script:
  ```
  $ ./gagesc.sh install -c hexagon -ad
  ```
- Note: During installation, modprobe will report failure if the Razormax card isn't installed in the computer.  It will only load the module if the card is present.
6. After build succeeds, copy the public include files to /usr/local/include/gage so fastspec can easily find them
  ```
  $ sudo mkdir /usr/local/include/gage
  $ sudo cp ./Include/Public/*.h /usr/local/include/gage/
  $ sudo chmod a-x /usr/local/include/gage/*.h
  ```
7. You can also build the CsTestQt application (although it has limited utility). First you will likely need to `$ sudo apt install qt5-default`. Then:
  ```
  $ cd CsTestQt
  $ chmod a+x install_cstestqt.sh
  $ ./install_cstestqt.sh
  ```
8. You can also use the CompuScope SDK applications, particularly ./Sdk/Advanced/GageStream2Disk to test that your RazorMax board is communicating properly.
9. You can also use the /usr/local/bin/gagehp.sh script to check if the driver is loaded: `$ gagehp.sh -i`.









