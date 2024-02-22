# Makefile for FASTSPEC

# ------------------------------------------------------------------------------
# SETUP FROM COMMAND LINE ARGUMENTS
# ------------------------------------------------------------------------------

# Setup the digitizer configuration
ifeq ($(digitizer), pxboard)
	DIG_SRCS := pxboard.cpp
	DIG_HDRS := pxboard.h
	DIG_LIBS := -lsig_px14400
	DIG_DEFS := -DDIG_PXBOARD -DSAMPLE_DATA_TYPE="unsigned short"
	DIG_INCS := 
else ifeq ($(digitizer), pxsim)
	DIG_SRCS :=
	DIG_HDRS := pxsim.h
	DIG_LIBS := 
	DIG_DEFS := -DDIG_PXSIM -DSAMPLE_DATA_TYPE="unsigned short"
	DIG_INCS := 
else ifeq ($(digitizer), razormax)
	DIG_SRCS := razormax.cpp
	DIG_HDRS := razormax.h
	DIG_LIBS := -lCsSsm -lCsAppSupport 
	DIG_DEFS := -DDIG_RAZORMAX -DSAMPLE_DATA_TYPE="short"
	DIG_INCS := -I/usr/local/include/gage
# The Gage driver SDK includes are in three folders.  FASTSPEC only needs the
# ones in Include/Public and assumes these have been copied to:
# /usr/local/include/gage/
#
#  -I../gati-linux-driver/Sdk/CsAppSupport \
#  -I../gati-linux-driver/Sdk/C_Common \
#	 -I../gati-linux-driver/Include/Public
	 
else
	# Cause an abort before building
	ERROR_DIG := true
endif

# Setup the switch configuration
ifeq ($(switch), sim)
	SW_HDRS := swsim.h
	SW_LIBS :=
	SW_DEFS := -DSW_SIM
else ifeq ($(switch), parallelport)
	SW_HDRS := swparallelport.h
	SW_LIBS :=
	SW_DEFS := -DSW_PARALLELPORT
else ifeq ($(switch), tty)
	SW_HDRS := swtty.h
	SW_LIBS :=
	SW_DEFS := -DSW_TTY
else ifeq ($(switch), mezio)
	SW_HDRS := swneuosys.h
	SW_LIBS := -lwdt_dio
	SW_DEFS := -DSW_MEZIO
else
	# Cause an abort before building
	ERROR_SWITCH := true
endif

# Setup the math precision configuration
FFT_LIBS := -lfftw3f		 
FFT_DEFS := -DFFT_SINGLE_PRECISION -DBUFFER_DATA_TYPE="float"

ifeq ($(precision), single)
  # Do nothing, use default values above
else ifeq ($(precision), double)
	FFT_LIBS := -lfftw3	 
	FFT_DEFS := -DFFT_DOUBLE_PRECISION -DBUFFER_DATA_TYPE="double"
else
	# Proceed with default (single precision)
	override precision := single
	USE_DEFAULT_PRECISION := true
endif


# ------------------------------------------------------------------------------
# SETUP CORE CONFIGURATION
# ------------------------------------------------------------------------------

# Outname executable name
TARGET_BASE := fastspec
TARGET := $(TARGET_BASE)_$(digitizer)_$(switch)_$(precision)

# Directory to install	
INSTALL := /usr/local/bin

# Files and libraries shared by all configurations
CORE_SRCS := buffer.cpp bytebuffer.cpp controller.cpp dumper.cpp fastspec.cpp \
	ini.cpp pfb.cpp spectrometer.cpp utility.cpp 
CORE_HDRS := accumulator.h buffer.h bytebuffer.h channelizer.h controller.h  \
	digitizer.h dumper.h ini.h pfb.h spectrometer.h switch.h spawn.h timing.h  \
	utility.h version.h wdt_dio.h 
CORE_LIBS := -pthread -lrt
CORE_CFLAGS := -Wall -O3 -mtune=native -std=c++0x -L/usr/lib

# Tell make these targets don't generate a file with the same name
.PHONY : clean install


# ------------------------------------------------------------------------------
# TARGET RULES
#------------------------------------------------------------------------------

# TARGET -- $(TARGET):  Builds the executable using the options specified on 
#                       make command line
$(TARGET): $(CORE_SRCS) $(DIG_SRCS) $(SW_SRCS) $(CORE_HDRS) $(DIG_HDRS) $(SW_HDRS)

ifdef ERROR_DIG
	# Abort with error message
	$(error No digitizer type specified on make command line. Use make argument: \
	  digitizer=[pxboard, pxsim, razormax])
endif

ifdef ERROR_SWITCH
	# Abort with error message
	$(error No switch method specified on make command line. Use make argument: \
	  switch=[mezio, parallelport, sim])
endif

ifdef USE_DEFAULT_PRECISION
	@echo ""
	@echo "WARNING: Floating point math precision not specified or not recognized on" 
	@echo "         make command line. Proceeding with single precision. Use make"  
	@echo "         argument: precision=[single, double]"
endif 

	@echo ""
	@echo "Using digitizer: $(digitizer)"
	@echo "Using switch: $(switch)"
	@echo "Using precision: $(precision)"
	@echo ""
	@echo "Building $@..."
	@echo ""
	
	@g++ $(CORE_SRCS) $(DIG_SRCS) $(SW_SRCS) -o $(TARGET) $(CORE_LIBS) \
	  $(CORE_CFLAGS) $(DIG_LIBS) $(DIG_INCS) $(DIG_DEFS) $(SW_LIBS) $(SW_DEFS) \
	  $(FFT_LIBS) $(FFT_DEFS) 
	@chmod u+s $(TARGET)
	
	@echo "Done.\n"


# TARGET - install:  Copy executables to the INSTALL diectory
install: $(TARGET)
	@echo "\nInstalling $(TARGET)..."
	sudo cp $(TARGET) $(INSTALL)/$(TARGET)
	sudo chmod 755 $(INSTALL)/$(TARGET)
	sudo chmod u+s $(INSTALL)/$(TARGET)
	@echo "Done.\n"


# TARGET -- clean:  Naively removes all likely files created by build
clean:
	@echo "\nRemoving build and install files for all $(TARGET_BASE) versions..."
	rm -f *~ *.o $(TARGET_BASE) $(TARGET_BASE)_* 
	sudo rm -f $(INSTALL)/$(TARGET_BASE) $(INSTALL)/$(TARGET_BASE)_*
	rm -f gensamples
	@echo "Done.\n"
	

# TARGET -- gensamples:  Builds this standalone helper
gensamples: gensamples.cpp
	@echo "\nBuilding $@..."
	@g++ $^ -o $@ $(CORE_FLAGS) $(CORE_LIBS)
	@echo "Done.\n"




