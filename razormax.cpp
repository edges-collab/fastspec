#include "razormax.h"

// ----------------------------------------------------------------------------
// The Gage SDK library headers have complicated dependencies which make it
// difficult to use with external software.  To avoid them, we'll implement a 
// few things there.
// ----------------------------------------------------------------------------

// Values needed to enable the eXpert data streaming mode (from CsExpert.h)
#define CS_BBOPTIONS_STREAM   0x2000
#define CS_MODE_USER1         0x40000000
#define CS_MODE_USER2         0x80000000

// Show error strings (from CsSdkMisc.h)
void DisplayErrorString(const int i32Status)
{
	char	szErrorString[256];
	CsGetErrorString(i32Status, szErrorString, 255);
	printf("\nRazormax driver error: %s\n", szErrorString);
}




// ----------------------------------------------------------------------------
// Constructor
// ----------------------------------------------------------------------------
RazorMax::RazorMax(double dAcquisitionRate, 
                   unsigned long uSamplesPerAccumulation, 
                   unsigned int uSamplesPerTransfer)
{

  // Pre-populate the user specifiable setup parameters
  m_uAcquisitionRate = 1000000*dAcquisitionRate;    // Hz
  m_uSamplesPerAccumulation = uSamplesPerAccumulation;
  m_uSamplesPerTransfer = uSamplesPerTransfer;
  m_uInputChannel = 1;        // 1 is always used for single channel mode
  m_u32BoardIndex = 1;        // 1 is always the first (only) board

  // To be assigned
  m_hBoard = 0;
  m_pBuffer1 = NULL;
  m_pBuffer2 = NULL;
  m_pReceiver = NULL;
  m_uBytesPerTransfer = 0;            
  m_uEffectiveSamplesPerTransfer = 0; 

  m_bStop = false;
}


// ----------------------------------------------------------------------------
// Destructor
// ----------------------------------------------------------------------------
RazorMax::~RazorMax()
{
  disconnect();
}



// ----------------------------------------------------------------------------
// stop
// ----------------------------------------------------------------------------
void RazorMax::stop()
{
  m_bStop = true;
}


// ----------------------------------------------------------------------------
// scale
// ----------------------------------------------------------------------------
double RazorMax::scale() 
{ 
  return m_dScale; 
}  // scale()


// ----------------------------------------------------------------------------
// offset
// ----------------------------------------------------------------------------
double RazorMax::offset() 
{
  return m_dOffset; 
} // offset()


// ----------------------------------------------------------------------------
// bytesPerSample
// ----------------------------------------------------------------------------
unsigned int RazorMax::bytesPerSample()
{ 
  return 2; 
} // bytesPerSample()


// ----------------------------------------------------------------------------
// type
// ----------------------------------------------------------------------------
Digitizer::DataType RazorMax::type() 
{ 
  return Digitizer::DataType::uint16; 
} // type()



// ----------------------------------------------------------------------------
// connect -- Connect to and initialize the device
// uBoardNumber = UNUSED

// ----------------------------------------------------------------------------
bool RazorMax::connect(unsigned int uBoardNumber)
{
  printf ("Connecting to and initializing RazorMax digitizer device...\n");

  int iStatus = CS_SUCCESS;

  // Initializes the CompuScope boards found in the system. If the
  // system is not found, a message with the error code will appear.
  // Otherwise iStatus will contain the number of systems found.
  iStatus = CsInitialize();
  if (CS_FAILED(iStatus))
  {
    printf("RazorMax::connect -- Initialize\n");
    DisplayErrorString(iStatus);
    disconnect();
    return false;
  }

  // Get the system. The first board that is found will be the one
  // that is used. m_hBoard will hold a unique system identifier
  // that is used when referencing the system.
  iStatus = CsGetSystem(&m_hBoard, 0, 0, 0, 0);
  if (CS_FAILED(iStatus))
  {
    printf("RazorMax::connect -- GetSystem\n");
    DisplayErrorString(iStatus);
    disconnect();
    return false;
  }

  // Display system information to command line
  if (!printBoardInfo())
  {
    disconnect();
    return false;
  }

  // *** Configure acquisition mode ***

  // In Streaming, some hardware related information are placed at the end of 
  // each segment. These samples contain also timestamp information for the 
  // segemnt. The number of extra bytes may be different depending on CompuScope 
  // card model.
  unsigned int   uSegmentTail_Bytes = 0;
  iStatus = CsGet(m_hBoard, 0, CS_SEGMENTTAIL_SIZE_BYTES, &uSegmentTail_Bytes);
  if (CS_FAILED(iStatus))
  {
    printf("RazorMax::connect -- Failed to get segment tail size\n");
    DisplayErrorString(iStatus);
    disconnect();
    return false;
  }
  printf("\nSegment tail in bytes: %d\n", uSegmentTail_Bytes);

  // Get the existing acquisition configuration structure for editing
  CSACQUISITIONCONFIG csAcquisitionCfg = {0};
  csAcquisitionCfg.u32Size = sizeof(CSACQUISITIONCONFIG);
  iStatus = CsGet(m_hBoard, CS_ACQUISITION, CS_CURRENT_CONFIGURATION, &csAcquisitionCfg);
  if (CS_FAILED(iStatus))
  {
    printf("RazorMax::connect -- Failed to get existing acquisition configuration\n");
    DisplayErrorString(iStatus);
    disconnect();
    return false;
  }
  
  // Use a single channel (RazorMax will always use channel 1 in single channel 
  // mode).  See ACQUISITION_MODES in CsDefines.h
  csAcquisitionCfg.u32Mode = CS_MODE_SINGLE;  

  // (must #include CsExpert.h)
  // Check if selected system supports Expert Stream and set the correct 
  // firmware image to be used.
  long i64ExtendedOptions = 0;
  iStatus = CsGet(m_hBoard, CS_PARAMS, CS_EXTENDED_BOARD_OPTIONS, &i64ExtendedOptions);
  if (CS_FAILED(iStatus))
  {
    printf("RazorMax::connect -- Failed to get extended options\n");
    DisplayErrorString(iStatus);
    disconnect();
    return false;
  }

  if (i64ExtendedOptions & CS_BBOPTIONS_STREAM)
  {
    printf("Selecting firmware for eXpert Data Stream from image 1\n");
    csAcquisitionCfg.u32Mode |= CS_MODE_USER1;
  }
  else if ((i64ExtendedOptions >> 32) & CS_BBOPTIONS_STREAM)
  {
    printf("Selecting firmware for eXpert Stream from image 2\n");
    csAcquisitionCfg.u32Mode |= CS_MODE_USER2;
  }
  else
  {
    printf("RazorMax::connect -- Board does not support eXpert Data Stream for continous data transfer.");
    disconnect();
    return false;
  }

  // Set the acquisition rate
  csAcquisitionCfg.i64SampleRate = m_uAcquisitionRate;

  // Set the trigger timeout to zero to force acquisitions to start immediately
  csAcquisitionCfg.i64TriggerTimeout = 0;

  // Set the basic transfer parameters (more in razormax::acquire)
  csAcquisitionCfg.i64Depth = m_uSamplesPerTransfer;
  csAcquisitionCfg.i64SegmentSize = m_uSamplesPerTransfer;

  // Set the number of segments per acquisition.  Note: Not sure why need to 
  // add 1 to the segment count, but otherwise it stops one transfer short.
  csAcquisitionCfg.u32SegmentCount = 1 + m_uSamplesPerAccumulation / csAcquisitionCfg.i64SegmentSize;
  
  // High part of 64-bit segment count (assuming we won't reach this amount)
  csAcquisitionCfg.i32SegmentCountHigh = 0;

  // Check that our total acquisition size is an integer number of segments
  if (m_uSamplesPerAccumulation % csAcquisitionCfg.i64SegmentSize)
  {
    printf("\nRazorMax::acquire -- Samples per transfer size is not a multiple of total number of samples per acquisition.");
    disconnect();
    return false;
  }

  // Push configuration to driver (not finished until committed to hardware below).
  iStatus = CsSet(m_hBoard, CS_ACQUISITION, &csAcquisitionCfg);
  if (CS_FAILED(iStatus))
  {
    printf("\nRazorMax::acquire -- Failed to write RazorMax acquisition configuration");
    DisplayErrorString(iStatus);
    disconnect();
    return false;
  }
  
  // Commit the values to the hardware.  Any invalid parameters will be caught 
  // here and an error returned.
  iStatus = CsDo(m_hBoard, ACTION_COMMIT);
  if (CS_FAILED(iStatus))
  {
    printf("RazorMax::connect -- Failed to commit configuration to board.");
    DisplayErrorString(iStatus);
    disconnect();
    return false;
  }

  // Display acquistion configuration
  if (!printAcquisitionConfig())
  {
    disconnect();
    return false;
  }

  // Display channel configuration (use default input channel = 1)
  if (!printChannelConfig(m_uInputChannel))
  {
    disconnect();
    return false;
  }
  
  // *** Allocate transfer buffers ***
  
  // Calculate the buffer size in bytes
  m_uBytesPerTransfer = m_uSamplesPerTransfer * csAcquisitionCfg.u32SampleSize 
    + uSegmentTail_Bytes;
    
  // Calculate the buffer size in equivalent number of samples
  m_uEffectiveSamplesPerTransfer = m_uBytesPerTransfer / csAcquisitionCfg.u32SampleSize;

  // Bytes per transfer must be a multiple of the DMA boundary, usually 16
  unsigned int u32DmaBoundaryBytes = 16;
  if ( m_uBytesPerTransfer % u32DmaBoundaryBytes )
  {
    printf("\nBytes per transfer is a not aligned to DMA boundary length.");
    printf("\nBytes per transfer = samples per transfer * 2 + segment tail bytes.  It is currently: %u", m_uBytesPerTransfer);
    printf("\nSegment tail bytes are: %u", uSegmentTail_Bytes);
    printf("\nTry make bytes per transfer a multiple of %u", u32DmaBoundaryBytes);
    disconnect();
    return false;
  }

  // Create Buffer #1
  iStatus = CsStmAllocateBuffer(m_hBoard, m_u32BoardIndex,  m_uBytesPerTransfer, &m_pBuffer1);
  if (CS_FAILED(iStatus))
  {
    printf("\nRazorMax::connect -- Failed to allocate memory for stream buffer 1 (board index: %u, bytes: %u).\n", 
      m_u32BoardIndex, m_uBytesPerTransfer);
    disconnect();
    return false;
  }

  // Create Buffer #2
  iStatus = CsStmAllocateBuffer(m_hBoard, m_u32BoardIndex,  m_uBytesPerTransfer, &m_pBuffer2);
  if (CS_FAILED(iStatus))
  {
    printf("\nRazorMax::connect -- Failed to allocate memory for stream buffer 2 (board index: %u, bytes: %u).\n", 
      m_u32BoardIndex, m_uBytesPerTransfer);
    disconnect();
    return false;
  }

  // Calculate the scale and offset used outside the digitizer class to convert 
  // from samples to voltages based on: voltage = scale*sample + offset.  
  //
  // Gage defines the voltage as:
  // voltage = (acq.i32SampleOffset - sample)/acq.i32SampleRes * (voltage_range/2)
  // 
  // Our scale is:  scale = -(voltage_range/2) / acq.i32SampleRes
  // Our offset is: offset = - acq.i32SampleOffset * scale
  // 
  // Using that voltage range is always 2.0 for RazorMax, we get our factors:
  m_dScale = -1.0 / csAcquisitionCfg.i32SampleRes;
  m_dOffset = -1.0 * m_dScale * csAcquisitionCfg.i32SampleOffset;
  
  return true;

} // connect()



// -----------------------------------------------------------
// disconnect() -- Disconnect from and free resouces used for
//                  PX14400 device
// -----------------------------------------------------------
void RazorMax::disconnect() 
{
  printf("\nFreeing RazorMax resources..\n");

  if (m_hBoard) {

    // Abort the current acquisition
    CsDo(m_hBoard, ACTION_ABORT);

    if (m_pBuffer1) {
      CsStmFreeBuffer(m_hBoard, 0, m_pBuffer1);
      m_pBuffer1 = NULL;
    }

    if (m_pBuffer2) {
      CsStmFreeBuffer(m_hBoard, 0, m_pBuffer2);
      m_pBuffer2 = NULL;
    }

    CsFreeSystem(m_hBoard);
    m_hBoard = 0;
  }

} // disconnect()


/*
// ----------------------------------------------------------------------------
// setTransferSamples
// ----------------------------------------------------------------------------
bool RazorMax::setTransferSamples(unsigned int uSamplesPerTransfer)
{
  m_uSamplesPerTransfer = uSamplesPerTransfer;
  return true;

} // setTransferSamples()



// ----------------------------------------------------------------------------
// setAcquisitionRate - Acquisition rate in S/s (Hz)
// ----------------------------------------------------------------------------
bool RazorMax::setAcquisitionRate(unsigned int dAcquisitionRate)
{
  m_uAcquisitionRate = 1000000*dAcquisitionRate;
  return true;

} // setAcquisitionRate()
*/

// ----------------------------------------------------------------------------
// acquire() - Transfer data from the board
//
//         Set uNumSamplesToTransfer = 0 for infinte loop.  Otherwise,
//         transfers are are acquired and sent to the callback.  The callback
//         should return the number of samples handled successfully.  This
//         function will continue to deliver sample to the callback until the
//         sum of all callbacks responses reaches uNumSamplestoTransfer. Te loop
//         can be aborted by calling stop().
//
// ----------------------------------------------------------------------------
bool RazorMax::acquire()
{
  void *pCurrentBuffer = NULL;
  void *pPreviousBuffer = NULL;
  unsigned long uNumSamples = 0;
  unsigned long n = 0;
  int iStatus = CS_SUCCESS;

  unsigned int u32TransferTimeout = 5000;
  unsigned int u32ErrorFlag = 0;
  unsigned int u32ActualLength = 0;
  unsigned int u8EndOfData = 0;

  // Reset the stop flag
  m_bStop = false;

  if (m_hBoard == 0) {
    printf("\nRazorMax:acquire -- No board handle.  Was RazorMax::connect() called?");
    printf("\nAborting acquisition.");
    return false;
  }

  if (m_pBuffer1 == NULL || m_pBuffer2 == NULL) {
    printf("\nRazorMax::Acquire -- No transfer buffers.  Was RazorMax::connect() called?");
    printf("\nAborting acquisition.");
    return false;
  }

  // Start the data acquisition
  iStatus = CsDo(m_hBoard, ACTION_START);
  if (CS_FAILED(iStatus))
  {
    printf("\nAborting acquisition.");
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
    iStatus = CsStmTransferToBuffer(m_hBoard, m_u32BoardIndex, pCurrentBuffer, 
      m_uEffectiveSamplesPerTransfer);
    if (CS_FAILED(iStatus))
    {
      if ( CS_STM_COMPLETED == iStatus ) {
        printf("*** Stream supposedly completed.  Aborting acquisition ***\n");
        CsDo(m_hBoard, ACTION_ABORT);
        return false;
      } else {
        DisplayErrorString(iStatus);
        printf("*** Transfer failed.  Aborting acquisition ***\n");
        CsDo(m_hBoard, ACTION_ABORT);
        return false;
      }
    }

    // Call the callback function to process previous chunk of data while we're
    // transfering the current.
    if (pPreviousBuffer) {
      if (m_pReceiver) {                      
        uNumSamples += m_pReceiver->onDigitizerData((SAMPLE_DATA_TYPE*) pPreviousBuffer, 
          m_uSamplesPerTransfer, uNumSamples, m_dScale, m_dOffset);
      } else {
        uNumSamples += m_uSamplesPerTransfer;
      }
    }

    // Wait for the asynchronous DMA transfer to complete so we can loop
    // back around to start a new one. Calling thread will sleep until
    // the transfer completes.
    iStatus = CsStmGetTransferStatus(m_hBoard, m_u32BoardIndex, 
      u32TransferTimeout, &u32ErrorFlag, &u32ActualLength, &u8EndOfData );
    if (CS_FAILED(iStatus))
    {
      DisplayErrorString(iStatus);

      if ( CS_STM_TRANSFER_TIMEOUT == iStatus )
      {
        printf("\n *** Time Out Error ***\n");
      }

      CsDo(m_hBoard, ACTION_ABORT);
      return false;
    }

    /*
    // (must #include CsExpert.h)
    if ( STM_TRANSFER_ERROR_FIFOFULL & u32ErrorFlag )
    {
      printf("\n *** FIFO Full Error ***\n");
      CsDo(m_hBoard, ACTION_ABORT);
      return false;
    }
    */

    // Switch buffers
    pPreviousBuffer = pCurrentBuffer;
    n++;

  } // transfer loop

  // Stop acquisition
  CsDo(m_hBoard, ACTION_ABORT);

  return true;

} // acquire()



// ----------------------------------------------------------------------------
// setCallback() -- Specify the callback function to use when
//                  a transfer is received
// ----------------------------------------------------------------------------
void RazorMax::setCallback(DigitizerReceiver* pReceiver)
{
  m_pReceiver = pReceiver;
} // setCallback()




// ----------------------------------------------------------------------------
// printAcquisitionConfig() -- Queries the board for the current acquisition
//   configuration.  Prints a summary to the command line.
// ----------------------------------------------------------------------------
bool RazorMax::printAcquisitionConfig()
{
  // Get current acquisition values
  CSACQUISITIONCONFIG csAcquisitionCfg = {0};
  csAcquisitionCfg.u32Size = sizeof(CSACQUISITIONCONFIG);
  int iStatus = CsGet(m_hBoard, CS_ACQUISITION, CS_CURRENT_CONFIGURATION, &csAcquisitionCfg);
  if (CS_FAILED(iStatus))
  {
    printf("\nFailed to get RazorMax acquisition configuration\n");
    DisplayErrorString(iStatus);
    return false;
  }

  // Display current acquisition configuration settings
  printf("\nRazorMax Acquisition Configuration -- see CsStruct.h");
  printf("\n----------------------------------------------------");
  printf("\nAcquisition i64SampleRate: %ld", csAcquisitionCfg.i64SampleRate);
  printf("\nAcquisition u32ExtClk: %u", csAcquisitionCfg.u32ExtClk);
  printf("\nAcquisition u32ExtClkSampleSkip: %u", csAcquisitionCfg.u32ExtClkSampleSkip);
  printf("\nAcquisition u32Mode: %u", csAcquisitionCfg.u32Mode);
  printf("\nAcquisition u32SampleBits: %u", csAcquisitionCfg.u32SampleBits);
  printf("\nAcquisition i32SampleRes: %d", csAcquisitionCfg.i32SampleRes);
  printf("\nAcquisition u32SampleSize: %u", csAcquisitionCfg.u32SampleSize);
  printf("\nAcquisition u32SegmentCount: %u", csAcquisitionCfg.u32SegmentCount);
  printf("\nAcquisition i64Depth: %ld", csAcquisitionCfg.i64Depth);
  printf("\nAcquisition i64SegmentSize: %ld", csAcquisitionCfg.i64SegmentSize);
  printf("\nAcquisition i64TriggerTimeout: %ld", csAcquisitionCfg.i64TriggerTimeout);
  printf("\nAcquisition u32TrigEnginesEn: %u", csAcquisitionCfg.u32TrigEnginesEn);
  printf("\nAcquisition i64TriggerDelay: %ld", csAcquisitionCfg.i64TriggerDelay);
  printf("\nAcquisition i64TriggerHoldoff: %ld", csAcquisitionCfg.i64TriggerHoldoff);
  printf("\nAcquisition i32SampleOffset: %d", csAcquisitionCfg.i32SampleOffset);
  printf("\nAcquisition u32TimeStampConfig: %u", csAcquisitionCfg.u32TimeStampConfig);
  printf("\nAcquisition i32SegmentCountHigh: %d\n", csAcquisitionCfg.i32SegmentCountHigh);

  return true;
}



// ----------------------------------------------------------------------------
// printChannelConfig() -- Queries the board for the current channel
//   configuration.  Prints a summary to the command line.
// ----------------------------------------------------------------------------
bool RazorMax::printChannelConfig(unsigned int uInputChannel)
{
  CSCHANNELCONFIG csChannelCfg = {0};
  csChannelCfg.u32Size = sizeof(CSCHANNELCONFIG);
  csChannelCfg.u32ChannelIndex = uInputChannel;
  int iStatus = CsGet(m_hBoard, CS_CHANNEL, CS_CURRENT_CONFIGURATION, &csChannelCfg);
  if (CS_FAILED(iStatus))
  {
    printf("\nFailed to get RazorMax channel configuration\n");
    DisplayErrorString(iStatus);
    return false;
  }

  printf("\nRazorMax Channel Configuration -- see CsStruct.h");
  printf("\n------------------------------------------------");
  printf("\nChannel u32ChannelIndex: %u", csChannelCfg.u32ChannelIndex); // 1 is first channel
  printf("\nChannel u32Term: %u", csChannelCfg.u32Term);            // Termination (DC or AC)
  printf("\nChannel u32InputRange: %u", csChannelCfg.u32InputRange);// mV (peak to peak)
  printf("\nChannel u32Impedance: %u", csChannelCfg.u32Impedance);  // Impedance (50 or 1000000 OHM)
  printf("\nChannel u32Filter: %u", csChannelCfg.u32Filter);        // 0 = no filter
  printf("\nChannel i32DcOffset: %d", csChannelCfg.i32DcOffset);
  printf("\nChannel i32Calib: %d\n", csChannelCfg.i32Calib);

  return true;
}



// ----------------------------------------------------------------------------
// printTriggerConfig() -- Queries the board for the current trigger
//   configuration.  Prints a summary to the command line.
// ----------------------------------------------------------------------------
bool RazorMax::printTriggerConfig(unsigned int uTriggerIndex)
{
  CSTRIGGERCONFIG csTriggerCfg = {0};
  csTriggerCfg.u32Size = sizeof(CSTRIGGERCONFIG);
  csTriggerCfg.u32TriggerIndex = uTriggerIndex;
  int iStatus = CsGet(m_hBoard, CS_TRIGGER, CS_CURRENT_CONFIGURATION, &csTriggerCfg);
  if (CS_FAILED(iStatus))
  {
    printf("\nFailed to get RazorMax trigger configuration\n");
    DisplayErrorString(iStatus);
    return false;
  }

  printf("\nRazorMax Trigger Configuration -- see CsStruct.h");
  printf("\n------------------------------------------------");
  printf("\nTrigger u32TriggerIndex: %u", csTriggerCfg.u32TriggerIndex);
  printf("\nTrigger u32Condition: %u", csTriggerCfg.u32Condition);
  printf("\nTrigger i32Level: %d", csTriggerCfg.i32Level);
  printf("\nTrigger i32Source: %d", csTriggerCfg.i32Source);
  printf("\nTrigger u32ExtCoupling: %u", csTriggerCfg.u32ExtCoupling);
  printf("\nTrigger u32ExtTriggerRange: %u", csTriggerCfg.u32ExtTriggerRange);
  printf("\nTrigger u32ExtImpedance: %u", csTriggerCfg.u32ExtImpedance);
  printf("\nTrigger i32Value1: %d", csTriggerCfg.i32Value1);
  printf("\nTrigger i32Value2: %d", csTriggerCfg.i32Value2);
  printf("\nTrigger u32Filter: %u", csTriggerCfg.u32Filter);
  printf("\nTrigger u32Relation: %u\n", csTriggerCfg.u32Relation);

  return true;
}


// ----------------------------------------------------------------------------
// printBoardInfo() -- Queries the CompuScope driver for information about the
//   system and board.  Prints a summary to the command line.s
// ----------------------------------------------------------------------------
bool RazorMax::printBoardInfo()
{
  int iStatus = 0;

  if (m_hBoard == 0)
  {
    printf("\nNo connection to RazorMax system\n");
    return false;
  }

  // Get system information
  CSSYSTEMINFO csSysInfo = {0};
  csSysInfo.u32Size = sizeof(CSSYSTEMINFO);
  iStatus = CsGetSystemInfo(m_hBoard, &csSysInfo);
  if (CS_FAILED(iStatus))
  {
    printf("\nFailed to get RazorMax system info\n");
    DisplayErrorString(iStatus);
    return false;
  }

  // Display the system information
  printf("\nRazorMax System Info -- see CsStruct.h");
  printf("\n--------------------------------------");
  printf("\nNumber of boards found: %u", csSysInfo.u32BoardCount);
  printf("\nBoard name: %s", csSysInfo.strBoardName);
  printf("\nBoard total memory size: %ld", csSysInfo.i64MaxMemory);
  printf("\nBoard vertical resolution (in bits): %u", csSysInfo.u32SampleBits);
  printf("\nBoard sample resolution (used for voltage conversion): %d", csSysInfo.i32SampleResolution);
  printf("\nBoard sample size (in bytes): %u", csSysInfo.u32SampleSize);
  printf("\nBoard sample offset (ADC output at middle of range): %d", csSysInfo.i32SampleOffset);
  printf("\nBoard type (see BOARD_TYPES in CsDefines.h): %u", csSysInfo.u32BoardType);
  printf("\nBoard addon options: %u", csSysInfo.u32AddonOptions);
  printf("\nBoard options: %u", csSysInfo.u32BaseBoardOptions);
  printf("\nBoard number of trigger engines: %u", csSysInfo.u32TriggerMachineCount);
  printf("\nBoard number of channels: %u\n", csSysInfo.u32ChannelCount);

  // Get additional board information
  ARRAY_BOARDINFO arrBoardInfo = {0};
  arrBoardInfo.u32BoardCount = 1;
  arrBoardInfo.aBoardInfo[0].u32Size = sizeof(CSBOARDINFO);
  iStatus = CsGet(m_hBoard, CS_BOARD_INFO, CS_CURRENT_CONFIGURATION, &arrBoardInfo);
  if (CS_FAILED(iStatus))
  {
    printf("\nFailed to get RazorMax board info\n");
    DisplayErrorString(iStatus);
    return false;
  }

  // Display the board information
  printf("\nRazorMax Board Info -- see CsStruct.h");
  printf("\n-------------------------------------");
  printf("\nBoard index in the system: %u", arrBoardInfo.aBoardInfo[0].u32BoardIndex);
  printf("\nBoard type: %u", arrBoardInfo.aBoardInfo[0].u32BoardType);
  printf("\nBoard serial number: %s", arrBoardInfo.aBoardInfo[0].strSerialNumber);
  printf("\nBoard base version: %u", arrBoardInfo.aBoardInfo[0].u32BaseBoardVersion);
  printf("\nBoard base firmware version: %u", arrBoardInfo.aBoardInfo[0].u32BaseBoardFirmwareVersion);
  printf("\nBoard base firmware options: %u", arrBoardInfo.aBoardInfo[0].u32BaseBoardFwOptions);
  printf("\nBoard base pcb options: %u", arrBoardInfo.aBoardInfo[0].u32BaseBoardHwOptions);
  printf("\nBoard addon version: %u", arrBoardInfo.aBoardInfo[0].u32AddonBoardVersion);
  printf("\nBoard addon firmware version: %u", arrBoardInfo.aBoardInfo[0].u32AddonBoardFirmwareVersion);
  printf("\nBoard addon firmware options: %u", arrBoardInfo.aBoardInfo[0].u32AddonFwOptions);
  printf("\nBoard addon pcb options: %u\n", arrBoardInfo.aBoardInfo[0].u32AddonHwOptions);


  // Display system capabilities
  printf("\nRazorMax Capability Details -- see CsDefines.h");
  printf("\n----------------------------------------------");

  // Check impedance options for the input channel
  unsigned int uBufferSize = 0;
  iStatus = CsGetSystemCaps(m_hBoard, CAPS_IMPEDANCES | m_uInputChannel,  NULL,  &uBufferSize);
  if (CS_FAILED(iStatus))
  {
    printf("\nFailed to get RazorMax channel impedance info\n");
    DisplayErrorString(iStatus);
    return false;
  }

  if (uBufferSize > 0) {

    // Allocate the tables
    unsigned int u32ImpedanceTables = uBufferSize / sizeof(CSIMPEDANCETABLE);
    CSIMPEDANCETABLE* pImpedanceTables = new CSIMPEDANCETABLE[u32ImpedanceTables];

    // Get the tables and print the results
    if (pImpedanceTables != NULL) {
      iStatus = CsGetSystemCaps(m_hBoard, CAPS_IMPEDANCES | m_uInputChannel,  
        pImpedanceTables,  &uBufferSize);
      for (unsigned int i=0; i<u32ImpedanceTables; i++) {
        printf("\nChannel %d -- Impedance: %d (%s)", m_uInputChannel, 
          pImpedanceTables[i].u32Imdepdance, pImpedanceTables[i].strText);
      }

      // Cleanup
      delete(pImpedanceTables);
    }
  }

  // Check input voltage range options for the specified channel
  uBufferSize = 0;
  iStatus = CsGetSystemCaps(m_hBoard, CAPS_INPUT_RANGES | m_uInputChannel,  NULL,  &uBufferSize);
  if (CS_FAILED(iStatus))
  {
    printf("\nFailed to get RazorMax input voltage range info\n");
    DisplayErrorString(iStatus);
    return false;
  }

  if (uBufferSize > 0) {

    // Allocate the tables
    unsigned int u32RangeTables = uBufferSize / sizeof(CSRANGETABLE);
    CSRANGETABLE* pRangeTables = new CSRANGETABLE[u32RangeTables];

    // Get the tables and print the results
    if (pRangeTables != NULL) {
      iStatus = CsGetSystemCaps(m_hBoard, CAPS_INPUT_RANGES | m_uInputChannel,  
        pRangeTables,  &uBufferSize);
      for (unsigned int i=0; i<u32RangeTables; i++) {
        printf("\nChannel %d -- Input Range: %d (%s)", m_uInputChannel, 
          pRangeTables[i].u32InputRange, pRangeTables[i].strText);
      }

      // Cleanup
      delete(pRangeTables);
    }
  }
 
  // Check sample rate options
  uBufferSize = 0;
  iStatus = CsGetSystemCaps(m_hBoard, CAPS_SAMPLE_RATES,  NULL,  &uBufferSize);
  if (CS_FAILED(iStatus))
  {
    printf("\nFailed to get RazorMax sample rate info\n");
    DisplayErrorString(iStatus);
    return false;
  }

  if (uBufferSize > 0) {

    // Allocate the tables
    unsigned int u32RateTables = uBufferSize / sizeof(CSSAMPLERATETABLE);
    CSSAMPLERATETABLE* pRateTables = new CSSAMPLERATETABLE[u32RateTables];

    // Get the tables and print the results
    if (pRateTables != NULL) {
      iStatus = CsGetSystemCaps(m_hBoard, CAPS_SAMPLE_RATES,  pRateTables,  &uBufferSize);

      if (CS_FAILED(iStatus)) {
        printf("\nFailed to get RazorMax sample rate info -- uBufferSize: %u, table size: %ld, u32RateTables: %u\n", uBufferSize, sizeof(CSSAMPLERATETABLE), u32RateTables);
      } else {
        for (unsigned int i=0; i<u32RateTables; i++) {
          printf("\nSample Rate: %ld (%s)", pRateTables[i].i64SampleRate, pRateTables[i].strText);
        }
      }

      // Cleanup
      delete(pRateTables);
    }
  }

  printf("\n");

  return true;

}


