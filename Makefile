# Makefile for PciAcqPX14
# 	@g++ -Wall -g -std=c++0x $^ -o $(TARGET) -lsig_px14400 -lfftw3 -pthread


TARGET  := fastspec
OBJS = *.o
LIBS = -lsig_px14400 -lfftw3f -pthread -lrt
CFLAGS = -Wall -O3 -mtune=native -std=c++0x -L/usr/lib
MEZIO = -lwdt_dio -DMEZIO
DOUBLE = -lfftw3 -DFFT_DOUBLE_PRECISION
SIMULATE = -DSIMULATE

.PHONY : clean

single : *.cpp
	@echo "Building "$(TARGET)" ("$@")..."	
	@g++ $^ -o $(TARGET) $(CFLAGS) $(LIBS) 
	chmod u+s $(TARGET)

double : *.cpp
	@echo "Building "$(TARGET)" ("$@")..."
	@g++ $^ -o $(TARGET)_double $(CFLAGS) $(LIBS) $(DOUBLE)
	chmod u+s $(TARGET)_double

mezio : *.cpp
	@echo "Building "$(TARGET)" ("$@")..."
	@g++ $^ -o $(TARGET)_mezio $(CFLAGS) $(LIBS) $(MEZIO) 
	chmod u+s $(TARGET)_mezio

simulate : *.cpp
	@echo "Building "$(TARGET)" ("$@")..."
	@g++ $^ -o $(TARGET)_simulate $(CFLAGS) $(LIBS) $(SIMULATE)
	chmod u+s $(TARGET)_simulate	

all : single double simulate mezio

clean :
	@echo "Cleaning "$(TARGET)"..."
	@rm -f *~ *.o $(TARGET) $(TARGET)_*

