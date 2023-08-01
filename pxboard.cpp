#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <sched.h>
#include <math.h>
#include <sys/io.h>
#include <fcntl.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>
#include <ctype.h>

#include "pxboard.h"



// ----------------------------------------------------------------------------
// Constructor
// ----------------------------------------------------------------------------
PXBoard::PXBoard( double dAcquisitionRate,
                  unsigned long uSamplesPerAccumulation,
                  unsigned int uSamplesPerTransfer,
                  unsigned int uInputChannel,
                  unsigned int uVoltageRange )
{

  // Pre-populate the user specifiable setup parameters
  m_dAcquisitionRate = dAcquisitionRate;        // MS/s
  m_uSamplesPerAccumulation = uSamplesPerAccumulation;
  m_uSamplesPerTransfer = uSamplesPerTransfer;  // 2^20 known to work well
  m_uInputChannel = uInputChannel;              // 0 = dual, 1= input1, 2=input2
  m_uVoltageRange1 = uVoltageRange;
  m_uVoltageRange2 = uVoltageRange;

  // To be retrieved from board
  m_uSerialNumber = -1;
  m_uBoardRevision = -1;

  // To be assigned 
  m_hBoard = NULL;
  m_pBuffer1 = NULL;
  m_pBuffer2 = NULL;
  m_pReceiver = NULL;
  m_dScale = 0;
  m_dOffset = 0;

  m_bStop = false;
}


// ----------------------------------------------------------------------------
// Destructor
// ----------------------------------------------------------------------------
PXBoard::~PXBoard()
{
  disconnect();
}



// ----------------------------------------------------------------------------
// stop
// ----------------------------------------------------------------------------
void PXBoard::stop()
{
  m_bStop = true;
}



// ----------------------------------------------------------------------------
// connect -- Connect to and initialize the PX14400 device
// uBoardNumber = 1 will use first board found, otherwise specify serial number

// ----------------------------------------------------------------------------
bool PXBoard::connect(unsigned int uBoardNumber) 
{
  int res;

  printf ("Connecting to and initializing PX14400 device...\n");

  if ((res=ConnectToDevicePX14(&m_hBoard, uBoardNumber)) != SIG_SUCCESS) {
    DumpLibErrorPX14(res, "Failed to connect to PX14400 device: ");
    return false;
  }

  // Get some info about the board to prove we are connected
  GetBoardRevisionPX14(m_hBoard, &m_uBoardRevision, NULL);
  GetSerialNumberPX14(m_hBoard, &m_uSerialNumber);
  printf("Connected to board %d, revision %d\n", m_uSerialNumber, m_uBoardRevision);

  // Set all hardware settings into a known state
  if ((res=SetPowerupDefaultsPX14(m_hBoard)) != SIG_SUCCESS) {
    DumpLibErrorPX14(res, "Failed to set powerup defaults: ");
    return false;
  }

  // Apply board settings
  if (!setVoltageRange()) { return false; }
  if (!setInputChannel()) { return false; }
  if (!setAcquisitionRate()) { return false; }
  if (!setSamplesPerTransfer()) { return false; }

  // Calculate the scale and offset used outside the digitizer class to convert 
  // from samples to voltages based on: voltage = scale*sample + offset.  
  //
  // PX14400 defines the voltage as:
  // voltage = ((sample - 32768) / 32768
  // 
  // Our scale is:  scale = 1/32768
  // Our offset is: offset = -1
  // 
  // TBD: need to make sure this based on selected voltage range
  m_dScale = 1.0 / 32768;
  m_dOffset = -1.0;
  
  return true;

} // connect()



// -----------------------------------------------------------
// disconnect() -- Disconnect from and free resouces used for 
//                  PX14400 device
// -----------------------------------------------------------
void PXBoard::disconnect() 
{
  if (m_hBoard) {

    EndBufferedPciAcquisitionPX14(m_hBoard);

    if (m_pBuffer1) {
      FreeDmaBufferPX14(m_hBoard, m_pBuffer1);
      m_pBuffer1 = NULL;
    }

    if (m_pBuffer2) {
      FreeDmaBufferPX14(m_hBoard, m_pBuffer2);
      m_pBuffer2 = NULL;
    }

    DisconnectFromDevicePX14(m_hBoard);

    m_hBoard = NULL;
    m_uSerialNumber = -1;
    m_uBoardRevision = -1;
  }
} // disconnect()



// ----------------------------------------------------------------------------
// setSamplesPerTransfer 
// ----------------------------------------------------------------------------
bool PXBoard::setSamplesPerTransfer() 
{
  // Allocate a DMA buffer that will receive PCI acquisition data. By
  // allocating a DMA buffer, we can use the "fast" PX14400 library
  // transfer routines for highest performance.  We allocate two DMA 
  // buffers and alternate transfers between them.  We'll use 
  // asynchronous data transfers so we can transfer to one buffer 
  // while processing the other.

  int res;


  // Apply the settings (if connected to board)
  if (m_hBoard) {

    // First free old buffers (if they exist)
    if (m_pBuffer1) {
      FreeDmaBufferPX14(m_hBoard, m_pBuffer1);
      m_pBuffer1 = NULL;
    }

    if (m_pBuffer2) {
      FreeDmaBufferPX14(m_hBoard, m_pBuffer2);
      m_pBuffer2 = NULL;
    }

    // Then allocate new buffers
    res = AllocateDmaBufferPX14(m_hBoard, m_uSamplesPerTransfer, &m_pBuffer1);
    if (SIG_SUCCESS != res) {
      DumpLibErrorPX14(res, "Failed to allocate DMA buffer #1: ", m_hBoard);
      return false;
    }

    res = AllocateDmaBufferPX14(m_hBoard, m_uSamplesPerTransfer, &m_pBuffer2);
    if (SIG_SUCCESS != res) {
      DumpLibErrorPX14(res, "Failed to allocate DMA buffer #2: ", m_hBoard);
      return false;
    }
  }

  return true;

} // setSamplesPerTransfer()



// ----------------------------------------------------------------------------
// setAcquisitionRate - Acquisition rate in MS/s
// ----------------------------------------------------------------------------
bool PXBoard::setAcquisitionRate() 
{
  int res;

  // Apply the setting (if connected to board)
  if (m_hBoard) {

    if ((res=SetInternalAdcClockRatePX14(m_hBoard, m_dAcquisitionRate)) != SIG_SUCCESS) {
      DumpLibErrorPX14(res, "Failed to set acquisition rate: ", m_hBoard);
      return false;
    }
  }

  return true;

} // setAcquisitionRate()



// ----------------------------------------------------------------------------
// setInputChannel
// ----------------------------------------------------------------------------
bool PXBoard::setInputChannel() 
{
  int res;
  unsigned int uTriggerSource;
  unsigned int uActiveChannel;

  // Make sure the specified value is in the allowed range
  // 0 = dual (both channels)
  // 1 = Input 1
  // 2 = Input 2
  if (m_uInputChannel < 0 || m_uInputChannel > 2) {
    return false;
  }

  // Apply the setting (if connected to board)
  if (m_hBoard) {

    // Use the proper PX14 flags and assume internal triggering
    if (m_uInputChannel == 0) {
      uActiveChannel = PX14CHANNEL_DUAL;
      uTriggerSource = PX14TRIGSRC_INT_CH1;
    } else if (m_uInputChannel == 1) {
      uActiveChannel = PX14CHANNEL_ONE;
      uTriggerSource = PX14TRIGSRC_INT_CH1;
    } else {
      uActiveChannel = PX14CHANNEL_TWO;
      uTriggerSource = PX14TRIGSRC_INT_CH2;
    } 

    if ((res=SetActiveChannelsPX14(m_hBoard, uActiveChannel)) != SIG_SUCCESS) {
      DumpLibErrorPX14(res, "Failed to set active channel: ", m_hBoard);
      return false;
    }

    if ((res=SetTriggerSourcePX14(m_hBoard, uTriggerSource)) != SIG_SUCCESS) {
      DumpLibErrorPX14(res, "Failed to set triggering: ", m_hBoard);
      return false;
    }
  }  

  return true;

} // setInputChannel()


// ----------------------------------------------------------------------------
// setVoltageRange
// ----------------------------------------------------------------------------
bool PXBoard::setVoltageRange() 
{
  int res;

  // Apply the setting (if connected to board)
  if (m_hBoard) {
    if ((res=SetInputVoltRangeCh1PX14(m_hBoard, m_uVoltageRange1)) != SIG_SUCCESS) { 
      DumpLibErrorPX14(res, "Failed to set voltage range: ", m_hBoard);
      return false;
    }

    if ((res=SetInputVoltRangeCh2PX14(m_hBoard, m_uVoltageRange2)) != SIG_SUCCESS) { 
      DumpLibErrorPX14(res, "Failed to set voltage range: ", m_hBoard);
      return false;
    }
  } else {
    return false;
  }
 
  return true;

} // setVoltageRange()


// ----------------------------------------------------------------------------
// scale
// ----------------------------------------------------------------------------
double PXBoard::scale() 
{ 
  return m_dScale; 
}  // scale()


// ----------------------------------------------------------------------------
// offset
// ----------------------------------------------------------------------------
double PXBoard::offset() 
{
  return m_dOffset; 
} // offset()


// ----------------------------------------------------------------------------
// bytesPerSample
// ----------------------------------------------------------------------------
unsigned int PXBoard::bytesPerSample()
{ 
  return 2; 
} // bytesPerSample()


// ----------------------------------------------------------------------------
// type
// ----------------------------------------------------------------------------
Digitizer::DataType PXBoard::type() 
{ 
  return Digitizer::DataType::uint16; 
} // type()





// ----------------------------------------------------------------------------
// acquire() - Transfer data from the board 
//
//         Set m_uSamplesPerAccumulation = 0 for infinte loop.  Otherwise, 
//         transfers are are acquired and sent to the callback.  The callback 
//         should return the number of samples handled successfully.  This  
//         function will continue to deliver samples to the callback until the 
//         sum of all callbacks responses reach m_uSamplesPerAccumulation. 
//         The loop can be aborted by calling stop().
//
// ----------------------------------------------------------------------------
bool PXBoard::acquire()
{
  px14_sample_t *pCurrentBuffer = NULL;
  px14_sample_t *pPreviousBuffer = NULL;
  unsigned long uNumSamples = 0;
  unsigned long n = 0;
  int res;

  // Reset the stop flag
  m_bStop = false;

  if (m_hBoard == NULL) {
    printf("No board handle at start of run.  Was PXBoard::connect() called?");
    return false;
  }

  if (m_pBuffer1 == NULL || m_pBuffer2 == NULL) {
    printf("No transfer buffers at start of run.  Was PXBoard::connect() called?");
    return false;
  }

  // Arm recording - Acquisition will begin when the PX14400 receives
  // a trigger event and then continue until stopped.
  if ((res=BeginBufferedPciAcquisitionPX14(m_hBoard)) != SIG_SUCCESS) {
    DumpLibErrorPX14(res, "Failed to arm recording: ", m_hBoard);
    return false;
  }

  // Force start of acquisition without waiting for the board to trigger
  // from detected voltage levels.  This starts acquisition even when
  // no input is connected to the board.
  if ((res=IssueSoftwareTriggerPX14(m_hBoard)) != SIG_SUCCESS) {
    DumpLibErrorPX14(res, "Failed to issue software trigger: ", m_hBoard);
    return false;
  }

  // Main recording loop - The main idea here is that we're alternating
  // between two halves of our DMA buffer. While we're transferring
  // fresh acquisition data to one half, we process the other half.

  // Loop over number of transfers requested
  while ((uNumSamples < m_uSamplesPerAccumulation) && !m_bStop) {

    // Determine where new data transfer data will go. We alternate
    // between our two DMA buffers
    if (n & 1) {
       pCurrentBuffer = m_pBuffer2;
    } else {
       pCurrentBuffer = m_pBuffer1;
    }

    // Start asynchronous DMA transfer of new data; this function starts
    // the transfer and returns without waiting for it to finish. This
    // gives us a chance to process the last batch of data in parallel
    // with this transfer.
    if ((res=GetPciAcquisitionDataFastPX14(m_hBoard, m_uSamplesPerTransfer, 
          pCurrentBuffer, PX14_TRUE)) != SIG_SUCCESS) {
      DumpLibErrorPX14 (res, "\nPXBoard::run() - Failed to obtain acquisition data: ", m_hBoard);
      EndBufferedPciAcquisitionPX14(m_hBoard);
      return false;
    }

    // Call the callback function to process previous chunk of data while we're
    // transfering the current.
    if (pPreviousBuffer) {
      if (m_pReceiver) {
        uNumSamples += m_pReceiver->onDigitizerData( (SAMPLE_DATA_TYPE*) pPreviousBuffer, 
          m_uSamplesPerTransfer, uNumSamples, m_dScale, m_dOffset);
      } else {
        uNumSamples += m_uSamplesPerTransfer;
      }
    }

    // Wait for the asynchronous DMA transfer to complete so we can loop
    // back around to start a new one. Calling thread will sleep until
    // the transfer completes.
    res = WaitForTransferCompletePX14(m_hBoard);

    // Check for error condition - board had FIFO overflow
    if (GetFifoFullFlagPX14(m_hBoard)) {
      printf("FIFO overflow\n");
      EndBufferedPciAcquisitionPX14(m_hBoard);
      return false;
    }
    
    // Check for error condition - all others
    if (SIG_SUCCESS != res) {
      if (SIG_CANCELLED == res)
        printf ("\nAcquisition cancelled");
      else {
        DumpLibErrorPX14(res, "\nAn error occurred waiting for transfer to complete: ", m_hBoard);
      }
      EndBufferedPciAcquisitionPX14(m_hBoard);
      return false;
    }

    // Switch buffers
    pPreviousBuffer = pCurrentBuffer;
    n++;

  } // transfer loop


  // Stop acquisition 
  EndBufferedPciAcquisitionPX14(m_hBoard);

  return true;

} // acquire()



// ----------------------------------------------------------------------------
// setCallback() -- Specify the callback function to use when
//                  a transfer is received
// ----------------------------------------------------------------------------
void PXBoard::setCallback(DigitizerReceiver* pReceiver) 
{
  m_pReceiver = pReceiver;
} // setCallback()

