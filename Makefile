# Makefile for PciAcqPX14
# 	@g++ -Wall -g -std=c++0x $^ -o $(TARGET) -lsig_px14400 -lfftw3 -pthread


TARGET  := fastspec
INSTALL := /usr/local/bin
SRCS := $(filter-out gensamples.cpp, $(wildcard *.cpp))
LIBS := -lsig_px14400 -lfftw3f -pthread -lrt
CFLAGS := -Wall -O3 -mtune=native -std=c++0x -L/usr/lib
MEZIO := -lwdt_dio -DMEZIO
DOUBLE := -lfftw3 -DFFT_DOUBLE_PRECISION

SIMULATE_SRCS := $(filter-out pxboard.cpp razormax.cpp, $(SRCS))
SIMULATE_LIBS := $(filter-out -lsig_px14400, $(LIBS)) -DSIMULATE


RAZORMAX_LIBS := -lCsSsm -lCsAppSupport -lfftw3f -pthread -lrt -DRAZORMAX
RAZORMAX_SRCS := $(filter-out gensamples.cpp pxboard.cpp, $(wildcard *.cpp))
RAZORMAX_INCS := -I/home/jdbowman/gati-linux-driver/Sdk/CsAppSupport -I/home/jdbowman/gati-linux-driver/Sdk/C_Common -I/home/jdbowman/gati-linux-driver/Include/Public

.PHONY : clean all

razormax : $(RAZORMAX_SRCS)
	@echo "Building "$(TARGET)" ("$@")..."
	@g++ $^ -o $(TARGET)_$@ $(CFLAGS) $(RAZORMAX_LIBS) $(RAZORMAX_INCS) 
	chmod u+s $(TARGET)_$@
	#cp $(TARGET)_$@ $(INSTALL)/$(TARGET)_$@
	#chmod 755 $(INSTALL)/$(TARGET)_$@
	#chmod u+s $(INSTALL)/$(TARGET)_$@
	@echo "Done."

single : $(SRCS)
	@echo "Building "$(TARGET)" ("$@")..."
	@g++ $^ -o $(TARGET)_$@ $(CFLAGS) $(LIBS) 
	chmod u+s $(TARGET)_$@
	cp $(TARGET)_$@ $(INSTALL)/$(TARGET)_$@
	chmod 755 $(INSTALL)/$(TARGET)_$@
	chmod u+s $(INSTALL)/$(TARGET)_$@
	@echo "Done."

double : $(SRCS)
	@echo "Building "$(TARGET)" ("$@")..."
	@g++ $^ -o $(TARGET)_$@ $(CFLAGS) $(LIBS) $(DOUBLE)
	chmod u+s $(TARGET)_$@
	cp $(TARGET)_$@ $(INSTALL)/$(TARGET)_$@
	chmod 755 $(INSTALL)/$(TARGET)_$@
	chmod u+s $(INSTALL)/$(TARGET)_$@
	@echo "Done."

mezio : $(SRCS)
	@echo "Building "$(TARGET)" ("$@")..."
	@g++ $^ -o $(TARGET)_$@ $(CFLAGS) $(LIBS) $(MEZIO) 
	chmod u+s $(TARGET)_$@
	cp $(TARGET)_$@ $(INSTALL)/$(TARGET)_$@
	chmod 755 $(INSTALL)/$(TARGET)_$@
	chmod u+s $(INSTALL)/$(TARGET)_$@
	@echo "Done."

simulate : $(SIMULATE_SRCS)
	@echo "Building "$(TARGET)" ("$@")..."
	@g++ $^ -o $(TARGET)_$@ $(CFLAGS) $(SIMULATE_LIBS)
	chmod u+s $(TARGET)_$@
	cp $(TARGET)_$@ $(INSTALL)/$(TARGET)_$@
	chmod 755 $(INSTALL)/$(TARGET)_$@
	chmod u+s $(INSTALL)/$(TARGET)_$@
	@echo "Done."

gensamples : gensamples.cpp
	@echo "Building "$@"..."
	@g++ $^ -o $@ $(CFLAGS) $(SIMULATE_LIBS)
	cp $@ $(INSTALL)/$@
	chmod 755 $(INSTALL)/$@
	@echo "Done."


all : single double mezio simulate

clean :
	@echo "Cleaning "$(TARGET)"..."
	@rm -f *~ *.o $(TARGET) $(TARGET)_* 
	@rm -f $(INSTALL)/$(TARGET) $(INSTALL)/$(TARGET)_*
	@echo "Cleaning gensamples..."
	@rm -f gensamples



