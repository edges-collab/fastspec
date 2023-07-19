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

#include "razormax.h"



#include <sys/stat.h>

#include <CsAppSupport.h>
#include "CsTchar.h"
#include "CsSdkMisc.h"
#include "CsSdkUtil.h"
#include "CsExpert.h"
#include <CsPrototypes.h>





// ----------------------------------------------------------------------------
// Constructor
// ----------------------------------------------------------------------------
RazorMax::RazorMax()
{

  // Pre-populate the user specifiable setup parameters
  m_uAcquisitionRate = 0;     // Hz
  m_uVoltageRange = 0;
  m_uInputChannel = 1;        // 1 = input1
  m_uSamplesPerTransfer = 0;
  m_uBytesPerTransfer = 0;    // Determined in connect()
  m_uSegmentTail_Bytes = 0;		// Determined in connect() by quering board   
  m_u32BoardIndex = 1;
  
  // To be assigned 
  m_hBoard = 0;
  m_pBuffer1 = NULL;
  m_pBuffer2 = NULL;
  m_pReceiver = NULL;

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
// connect -- Connect to and initialize the device
// uBoardNumber = UNUSED

// ----------------------------------------------------------------------------
bool RazorMax::connect(unsigned int uBoardNumber) 
{
  printf ("Connecting to and initializing RazorMax digitizer device...\n");

  int32 iStatus = CS_SUCCESS;

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

	// In Streaming, some hardware related information are placed at the end of each segment. 
	// These samples contain also timestamp information for the segemnt. The number of extra 
	// bytes may be different depending on CompuScope card model.
	iStatus = CsGet(m_hBoard, 0, CS_SEGMENTTAIL_SIZE_BYTES, &m_uSegmentTail_Bytes);
	if (CS_FAILED(iStatus))
	{
		DisplayErrorString(iStatus);
		disconnect();
		return false;
	}
	printf("\nSegment tail in bytes: %d\n", m_uSegmentTail_Bytes);

  // Allocate buffers using samples per transfer, sample size, and DMA boundary
  m_uSamplesPerTransfer = m_uSamplesPerTransfer; // 524288;  
  m_uBytesPerTransfer = m_uSamplesPerTransfer * 2 + m_uSegmentTail_Bytes;  // 2 bytes per samplecsAcquisitionCfg.u32SampleSize; 
	
	// Bytes per transfer must be a multiple of the DMA boundary, usually 16
  unsigned int u32DmaBoundaryBytes = 16;
	if ( m_uBytesPerTransfer % u32DmaBoundaryBytes )
	{
    printf("\nBytes per transfer is a not aligned to DMA boundary length.");
    printf("\nBytes per transfer = samples per transfer * 2 + segment tail bytes.  It is currently: %d", m_uBytesPerTransfer);;
    printf("\nSegment tail bytes are: %d", m_uSegmentTail_Bytes);
    printf("\nTry make bytes per transfer a multiple of %d", u32DmaBoundaryBytes);
    disconnect();
    return false;
  }

  // Create Buffer #1
	iStatus = CsStmAllocateBuffer(m_hBoard, m_u32BoardIndex,  m_uBytesPerTransfer, &m_pBuffer1);
	if (CS_FAILED(iStatus))
	{
		printf("\nUnable to allocate memory for stream buffer 1.\n");
		disconnect();
		return false;
	}

  // Create Buffer #2
	iStatus = CsStmAllocateBuffer(m_hBoard, m_u32BoardIndex,  m_uBytesPerTransfer, &m_pBuffer2);
	if (CS_FAILED(iStatus))
	{
		printf("\nUnable to allocate memory for stream buffer 2.\n");
		disconnect();
		return false;
	}

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



// ----------------------------------------------------------------------------
// setInputChannel
// ----------------------------------------------------------------------------
bool RazorMax::setInputChannel(unsigned int uChannel) 
{
	m_uInputChannel = uChannel;
  return true;

} // setInputChannel()


// ----------------------------------------------------------------------------
// setVoltageRange - Voltage range in mV
// ----------------------------------------------------------------------------
bool RazorMax::setVoltageRange(unsigned int uRange) 
{
	m_uVoltageRange = uRange;
  return true;

} // setVoltageRange()



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
bool RazorMax::acquire(unsigned long uNumSamplesToTransfer)
{

 	void *pCurrentBuffer = NULL;
  void *pPreviousBuffer = NULL;
  unsigned long uNumSamples = 0;
  unsigned long n = 0;
  int32 iStatus = CS_SUCCESS;

  unsigned int u32TransferTimeout = 5000;
  unsigned int u32ErrorFlag = 0;
  unsigned int u32ActualLength = 0;
  unsigned int u8EndOfData = 0;

  // Reset the stop flag
  m_bStop = false;

  if (m_hBoard == NULL) {
    printf("No board handle at start of run.  Was RazorMax::connect() called?");
	  printf("\nAborting acquisition.");    
    return false;
  }

  if (m_pBuffer1 == NULL || m_pBuffer2 == NULL) {
    printf("No transfer buffers at start of run.  Was RazorMax::connect() called?");
 	  printf("\nAborting acquisition.");
    return false;
  }
  
  
	// -----------------------------------------------
	// Configure acquisition mode
	// -----------------------------------------------

  // Get the existing acquisition configuration structure for editing
  CSACQUISITIONCONFIG csAcquisitionCfg = {0};
	csAcquisitionCfg.u32Size = sizeof(CSACQUISITIONCONFIG);
  iStatus = CsGet(m_hBoard, CS_ACQUISITION, CS_CURRENT_CONFIGURATION, &csAcquisitionCfg);
	if (CS_FAILED(iStatus))
	{
		printf("\nFailed to get RazorMax acquisition configuration\n");
		DisplayErrorString(iStatus);
    printf("\nAborting acquisition.");
		return false;
	}
	 
	// Check if selected system supports Expert Stream and set the correct firmware image to be used.
	int64	i64ExtendedOptions = 0;
	iStatus = CsGet(m_hBoard, CS_PARAMS, CS_EXTENDED_BOARD_OPTIONS, &i64ExtendedOptions);
	if (CS_FAILED(iStatus))
	{
		DisplayErrorString(iStatus);
		printf("\nAborting acquisition.");
		return false;
	}
	//printf("\nExtended board options: %d\n", i64ExtendedOptions);
	
	// Fill in our configuration
	csAcquisitionCfg.u32Mode = CS_MODE_SINGLE;	// Acquisition mode of the system: see ACQUISITION_MODES in CsDefines.h
	
	if (i64ExtendedOptions & CS_BBOPTIONS_STREAM)
	{
		//printf("\nSelecting firmware for eXpert Data Stream from image 1");
		csAcquisitionCfg.u32Mode |= CS_MODE_USER1;
	}
	else if ((i64ExtendedOptions >> 32) & CS_BBOPTIONS_STREAM)
	{
		//printf("\nSelecting firmware for eXpert Stream from image 2");
		csAcquisitionCfg.u32Mode |= CS_MODE_USER2;
	}
	else
	{
		printf("\nBoard does not support eXpert Data Stream for continous data transfer.");
		printf("\nAborting acquisition.");
		return false;
	}
	  
  // Set the acquisition rate
  csAcquisitionCfg.i64SampleRate = m_uAcquisitionRate;

  // Set the trigger timeout to zero to force acquisitions to start immediately
	csAcquisitionCfg.i64TriggerTimeout = 0;	                           
		
  // Set the transfer parameters
	csAcquisitionCfg.i64Depth = m_uSamplesPerTransfer;	//csAcquisitionCfg.i64Depth = 8160;	 
	csAcquisitionCfg.i64SegmentSize = m_uSamplesPerTransfer;
	csAcquisitionCfg.u32SegmentCount = 1 + uNumSamplesToTransfer / csAcquisitionCfg.i64SegmentSize; //  Number of segments per acquisition.  		  // JDB: The +1 is a hack here because it seems the card counts the segment tail bytes as part of the sample transfer, so it stops early!!  Adding any extra segment, gives more data, which is used to mkae up for the difference!																			
	csAcquisitionCfg.i32SegmentCountHigh = 0;   	// High part of 64-bit segment count. 
	
	//printf("\nNumSamplesToTransfer: %ld, SegmentSize: %d, SegmentCount %6.3f\n", uNumSamplesToTransfer, 
	//  csAcquisitionCfg.i64SegmentSize, 1.0 * uNumSamplesToTransfer / csAcquisitionCfg.i64SegmentSize);
	  
	if (uNumSamplesToTransfer % csAcquisitionCfg.i64SegmentSize) 
	{
	  printf("\nSamples per transfer size is not a multiple of total number of samples per acquisition.");
	  printf("\nAborting acquisition.");
	  return false;
	}
	  
	// Push to the driver (not finished until committed to hardware below).  
	iStatus = CsSet(m_hBoard, CS_ACQUISITION, &csAcquisitionCfg);
	if (CS_FAILED(iStatus))
	{
	  printf("\nFailed to write RazorMax acquisition configuration");
		DisplayErrorString(iStatus);
		disconnect();
		return false;
	}

	// Commit the values to the hardware.  Any invalid parameters will be caught here and an error returned.
	iStatus = CsDo(m_hBoard, ACTION_COMMIT);
	if (CS_FAILED(iStatus))
	{
		DisplayErrorString(iStatus);
	  printf("\nAborting acquisition.");
		return false;
	}

  //printAcquisitionConfig();
   
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
  while ((uNumSamples < uNumSamplesToTransfer) && !m_bStop) {

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
		iStatus = CsStmTransferToBuffer(m_hBoard, m_u32BoardIndex, pCurrentBuffer, m_uBytesPerTransfer/2);
		if (CS_FAILED(iStatus))
		{
		  if ( CS_STM_COMPLETED == iStatus ) {
				printf("Stream supposedly completed.  Aborting acquisition ***\n");
				CsDo(m_hBoard, ACTION_ABORT);	
			  return false;
			}	else {
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
     
        /*short *pIn = (short*) pPreviousBuffer;
        //printf("\nADC (total length = %u): %d, %d, %d, %d", u32ActualLength, pShort[10], pShort[100], pShort[1000], pShort[10000]);
        short dMax = pIn[0];
        short dMin = pIn[0];
        for (int i=0; i<m_uSamplesPerTransfer; i++) {
          if (pIn[i] < dMin) { 
            dMin = pIn[i]; 
          } else if (pIn[i] > dMax) { 
            dMax = pIn[i]; 
          }
        }
        printf("\nTransfer min: %6d, max: %6d", dMin, dMax);
        */
        uNumSamples += m_pReceiver->onDigitizerData((unsigned short*) pPreviousBuffer, m_uSamplesPerTransfer, uNumSamples);
      } else {
        uNumSamples += m_uSamplesPerTransfer;
      }
    }

    // Wait for the asynchronous DMA transfer to complete so we can loop
    // back around to start a new one. Calling thread will sleep until
    // the transfer completes.
		iStatus = CsStmGetTransferStatus(m_hBoard, m_u32BoardIndex, u32TransferTimeout, &u32ErrorFlag, &u32ActualLength, &u8EndOfData );
        
    //printf("u32TransferTimeout: %u, u32ErrorFlag: %u, u32ActualLength: %u, u8EndOfData: %u\n", u32TransferTimeout, u32ErrorFlag, u32ActualLength, u8EndOfData );		
		//printf("Transfer returned %d samples out of expected %d\n", u32ActualLength, m_uSamplesPerTransfer);
		if (CS_FAILED(iStatus))
		{
			DisplayErrorString(iStatus);

			if ( CS_STM_TRANSFER_TIMEOUT == iStatus )
			{
				printf("\n *** Time Out Error ***\n");
				// Timeout on CsStmGetTransferStatus().
				// Data transfer has not yet completed. We can repeat calling CsStmGetTransferStatus()  until the function
				// returns the status CS_SUCCESS.
				// In this sample program, we just stop and return the error.
			}
			
  	  CsDo(m_hBoard, ACTION_ABORT);			
			return false;
		}
		
		if ( STM_TRANSFER_ERROR_FIFOFULL & u32ErrorFlag )
		{
		  printf("\n *** FIFO Full Error ***\n");
  	  CsDo(m_hBoard, ACTION_ABORT);		  
		  return false;
		}
  
    // Switch buffers
    pPreviousBuffer = pCurrentBuffer;
    n++;

    //printf("Iteration: %d, Stop: %d, uNumSamples: %ld, uNumSamplesToTransfer: %ld\n", n, m_bStop, uNumSamples, uNumSamplesToTransfer);
    
  } // transfer loop

  // ** NOT ACTIVELY USED **
  // Process last acquisition of loop
  //if (pPreviousBuffer) {
  //  onTransfer(pPreviousBuffer, m_uSamplesPerTransfer);
  //}


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
// 	 configuration.  Prints a summary to the command line.
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
	printf("\nRazorMax Acquisition Configuration");
	printf("\n----------------------------------");
	printf("\nAcquisition i64SampleRate: %d", csAcquisitionCfg.i64SampleRate);
	printf("\nAcquisition u32ExtClk: %u", csAcquisitionCfg.u32ExtClk);	
	printf("\nAcquisition u32ExtClkSampleSkip: %u", csAcquisitionCfg.u32ExtClkSampleSkip);	
	printf("\nAcquisition u32Mode: %u", csAcquisitionCfg.u32Mode);	
	printf("\nAcquisition u32SampleBits: %u", csAcquisitionCfg.u32SampleBits);	
	printf("\nAcquisition i32SampleRes: %d", csAcquisitionCfg.i32SampleRes);	
	printf("\nAcquisition u32SampleSize: %u", csAcquisitionCfg.u32SampleSize);	
	printf("\nAcquisition u32SegmentCount: %u", csAcquisitionCfg.u32SegmentCount);	
	printf("\nAcquisition i64Depth: %d", csAcquisitionCfg.i64Depth);	
	printf("\nAcquisition i64SegmentSize: %d", csAcquisitionCfg.i64SegmentSize);	
	printf("\nAcquisition i64TriggerTimeout: %d", csAcquisitionCfg.i64TriggerTimeout);	
	printf("\nAcquisition u32TrigEnginesEn: %u", csAcquisitionCfg.u32TrigEnginesEn);	
	printf("\nAcquisition i64TriggerDelay: %d", csAcquisitionCfg.i64TriggerDelay);	
	printf("\nAcquisition i64TriggerHoldoff: %d", csAcquisitionCfg.i64TriggerHoldoff);	
	printf("\nAcquisition i32SampleOffset: %d", csAcquisitionCfg.i32SampleOffset);	
	printf("\nAcquisition u32TimeStampConfig: %u", csAcquisitionCfg.u32TimeStampConfig);	
	printf("\nAcquisition i32SegmentCountHigh: %d\n", csAcquisitionCfg.i32SegmentCountHigh);	

	return true;
}



// ----------------------------------------------------------------------------
// printChannelConfig() -- Queries the board for the current channel
// 	 configuration.  Prints a summary to the command line.
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
	
	printf("\nRazorMax Channel Configuration");
	printf("\n------------------------------");		
	printf("\nChannel u32ChannelIndex: %u", csChannelCfg.u32ChannelIndex);
	printf("\nChannel u32Term: %u", csChannelCfg.u32Term);            // Termination (DC or AC) 
	printf("\nChannel u32Impedance: %u", csChannelCfg.u32Impedance);  // Impedance (50 or 1000000 OHM)
	printf("\nChannel u32Filter: %u", csChannelCfg.u32Filter);        // 0 = no filter
	printf("\nChannel i32DcOffset: %d", csChannelCfg.i32DcOffset);
	printf("\nChannel i32Calib: %d\n", csChannelCfg.i32Calib);
	
	return true;
}

  

// ----------------------------------------------------------------------------
// printTriggerConfig() -- Queries the board for the current trigger
// 	 configuration.  Prints a summary to the command line.
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
		
	printf("\nRazorMax Trigger Configuration");
	printf("\n------------------------------");		
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
// 	 system and board.  Prints a summary to the command line.s
// ----------------------------------------------------------------------------
bool RazorMax::printBoardInfo() 
{
	int iStatus = 0;

	if (m_hBoard == NULL) 
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
	printf("\nRazorMax System Info");
	printf("\n--------------------");
	printf("\nNumber of boards found: %u", csSysInfo.u32BoardCount);
	printf("\nBoard name: %s", csSysInfo.strBoardName);
	printf("\nBoard total memory size: %d", csSysInfo.i64MaxMemory);
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
	printf("\nRazorMax Board Info");
	printf("\n-------------------");
	printf("\nBoard index in the system: %u", arrBoardInfo.aBoardInfo[0].u32BoardIndex);
	printf("\nBoard type: %u", arrBoardInfo.aBoardInfo[0].u32BoardType);
	printf("\nBoard serial number: %s", arrBoardInfo.aBoardInfo[0].strSerialNumber);
  printf("\nBoard version: %u", arrBoardInfo.aBoardInfo[0].u32BaseBoardVersion);
	printf("\nBoard firmware version: %u", arrBoardInfo.aBoardInfo[0].u32BaseBoardFirmwareVersion);
	printf("\nBoard firmware options: %u", arrBoardInfo.aBoardInfo[0].u32BaseBoardFwOptions);
	printf("\nBoard pcb options: %u", arrBoardInfo.aBoardInfo[0].u32BaseBoardHwOptions);
	printf("\nBoard addon version: %u", arrBoardInfo.aBoardInfo[0].u32AddonBoardVersion);
	printf("\nBoard addon firmware version: %u", arrBoardInfo.aBoardInfo[0].u32AddonBoardFirmwareVersion);
	printf("\nBoard addon firmware options: %u", arrBoardInfo.aBoardInfo[0].u32AddonFwOptions);
	printf("\nBoard addon pcb optinos: %u\n", arrBoardInfo.aBoardInfo[0].u32AddonHwOptions);


  // Display system capabilities
  printf("\nRazorMax Capability Details");
	printf("\n-----------------------------");

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
			iStatus = CsGetSystemCaps(m_hBoard, CAPS_IMPEDANCES | m_uInputChannel,  pImpedanceTables,  &uBufferSize);
			for (int i=0; i<u32ImpedanceTables; i++) {
				printf("\nChannel %d -- Impedance: %d (%s)", m_uInputChannel, pImpedanceTables[i].u32Imdepdance, pImpedanceTables[i].strText);
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
			iStatus = CsGetSystemCaps(m_hBoard, CAPS_INPUT_RANGES | m_uInputChannel,  pRangeTables,  &uBufferSize);
			for (int i=0; i<u32RangeTables; i++) {
				printf("\nChannel %d -- Input Range: %d (%s)", m_uInputChannel, pRangeTables[i].u32InputRange, pRangeTables[i].strText);
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
			  printf("\nFailed to get RazorMax sample rate info -- uBufferSize: %d, table size: %d, u32RateTables: %d\n", uBufferSize, sizeof(CSSAMPLERATETABLE), u32RateTables);
			} else {
			  for (int i=0; i<u32RateTables; i++) {
				  printf("\nSample Rate: %d (%s)", pRateTables[i].i64SampleRate, pRateTables[i].strText);
			  }
			}

			// Cleanup
			delete(pRateTables);
		}
	}

	printf("\n");

	return true;

}

