# Makefile for FASTSPEC

# ------------------------------------------------------------------------------
# Setup the digitizer configuration
# ------------------------------------------------------------------------------
ifeq ($(digitizer), pxboard)
	DIG_SRCS := pxboard.cpp
	DIG_LIBS := -lsig_px14400
	DIG_DEFS := -DDIG_PXBOARD
else ifeq ($(digitizer), pxsim)
	DIG_SRCS := 
	DIG_LIBS := 
	DIG_DEFS := -DDIG_PXSIM
else ifeq ($(digitizer), razormax)
	DIG_SRCS := razormax.cpp
	DIG_LIBS := -lCsSsm -lCsAppSupport 
	DIG_DEFS := -DDIG_RAZORMAX	
	DIG_INCS := -I/home/jdbowman/gati-linux-driver/Sdk/CsAppSupport -I/home/jdbowman/gati-linux-driver/Sdk/C_Common -I/home/jdbowman/gati-linux-driver/Include/Public
else
	# Cause an abort before building
	ERROR_DIG := true
endif


# ------------------------------------------------------------------------------
# Setup the switch configuration
# ------------------------------------------------------------------------------
ifeq ($(switch), sim)
	SW_LIBS :=
	SW_DEFS := -DSW_SIM
else ifeq ($(switch), parallel)
	SW_LIBS :=
	SW_DEFS := -DSW_PARALLEL
else ifeq ($(switch), parallel)
	SW_LIBS := -lwdt_dio
	SW_DEFS := -DSW_MEZIO
else
	# Abort with error message
	ERROR_SWITCH := true
endif


# ------------------------------------------------------------------------------
# Setup the math precision configuration
# ------------------------------------------------------------------------------
FFT_LIBS := -lfftw3f		 
FFT_DEFS := -DFFT_SINGLE_PRECISION

ifeq ($(precision), single)
  # Do nothing, use default values above
else ifeq ($(precision), double)
	FFT_LIBS := -lfftw3f		 
	FFT_DEFS := -DFFT_DOUBLE_PRECISION
else
	# Proceed with default (single precision)
	override precision := single
	DEFAULT_PRECISION := true
endif

# ------------------------------------------------------------------------------
# Setup the core configuration
# ------------------------------------------------------------------------------

# Outname executable name
TARGET_BASE := fastspec
TARGET := $(TARGET_BASE)_$(digitizer)_$(switch)_$(precision)

# Director to install	
INSTALL := /usr/local/bin

# Files and libraries shared by all configurations
CORE_SRCS := $(filter-out gensamples.cpp pxboard.cpp razormax.cpp, $(wildcard *.cpp))
CORE_LIBS := -pthread -lrt
CORE_CFLAGS := -Wall -O3 -mtune=native -std=c++0x -L/usr/lib

# Tell make these targets don't generate a file with the same name
.PHONY : clean 


# ------------------------------------------------------------------------------
# default target - builds the executable using the options specified on the 
# make command line
# ------------------------------------------------------------------------------
default: $(CORE_SRCS) $(DIG_SRCS) $(SW_SRCS)

ifdef ERROR_DIG
	# Abort with error message
	$(error No digitizer type specified on make command line. Use make argument: digitizer=[pxboard, pxsim, razormax])
endif

ifdef ERROR_SWITCH
	# Abort with error message
	$(error No switch method specified on make command line. Use make argument: switch=[mezio, parallel, sim])
endif

ifdef DEFAULT_PRECISION
	@echo ""
	@echo "WARNING: No floating point math precision specified on make command line."
	@echo "         Proceeding with single precision."  
	@echo "         Use make argument: precision=[single, double]"

endif 

	@echo ""
	@echo "Using digitizer: $(digitizer)"
	@echo "Using switch: $(switch)"
	@echo "Using precision: $(precision)"
	@echo ""
	@echo "Output executable will be: $(TARGET)"
	@echo ""
	
	@g++ $^ -o $(TARGET) $(CORE_LIBS) $(CORE_CFLAGS) $(DIG_LIBS) $(DIG_INCS) $(DIG_DEFS) $(SW_LIBS) $(SW_DEFS) $(FFT_LIBS) $(FFT_DEFS) 
	@chmod u+s $(TARGET)
	#cp $(TARGET)_$@ $(INSTALL)/$(TARGET)_$@
	#chmod 755 $(INSTALL)/$(TARGET)_$@
	#chmod u+s $(INSTALL)/$(TARGET)_$@
	@echo "Done."


# ------------------------------------------------------------------------------
# clear target - naively removes all likely files created by build
# ------------------------------------------------------------------------------
clean:
	@echo "Cleaning $(TARGET_BASE)..."
	@rm -f *~ *.o $(TARGET_BASE) $(TARGET_BASE)_* 
	@rm -f $(INSTALL)/$(TARGET_BASE) $(INSTALL)/$(TARGET_BASE)_*
	@rm -f gensamples


