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
  m_dAcquisitionRate = 0;     // MS/s
  m_uVoltageRange1 = 0;
  m_uVoltageRange2 = 0;
  m_uInputChannel = 0;        // 0 = dual, 1= input1, 2=input2
  m_uSamplesPerTransfer = 0;     // 2^20 samples known to work well

  // To be retrieved from board
  m_uSerialNumber = -1;
  m_uBoardRevision = -1;

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
// uBoardNumber = 1 will use first board found, otherwise specify serial number

// ----------------------------------------------------------------------------
bool RazorMax::connect(unsigned int uBoardNumber) 
{
  printf ("Connecting to and initializing RazorMax digitizer device...\n");

  int32           i32Status = CS_SUCCESS;
  //pthread_t*      thread;
  //pthread_attr_t  attr;
  //BOOL*			      threadFinished;
  //int             ret = 0;
  //CsDataPackMode	CurrentPackConfig;
  
  CSTRIGGERCONFIG     g_CsTriggerCfg = {0};
  CSCHANNELCONFIG     g_CsChannelCfg = {0};
  CSACQUISITIONCONFIG g_CsAcquisitionCfg = {0};
  CSSYSTEMINFO        g_CsSysInfo = {0};

	// Initializes the CompuScope boards found in the system. If the
	// system is not found a message with the error code will appear.
	// Otherwise i32Status will contain the number of systems found.
	i32Status = CsInitialize();

	if (CS_FAILED(i32Status))
	{
		DisplayErrorString(i32Status);
		return (false);
	}

	// Get the system. The first system that is found will be the one
	// that is used. m_hBoard will hold a unique system identifier 
	// that is used when referencing the system.
	i32Status = CsGetSystem(&m_hBoard, 0, 0, 0, 0);
	if (CS_FAILED(i32Status))
	{
		DisplayErrorString(i32Status);
		return (false);
	}

	// Get system information. The u32Size field must be filled in
	// prior to calling CsGetSystemInfo
	g_CsSysInfo.u32Size = sizeof(CSSYSTEMINFO);
	i32Status = CsGetSystemInfo(m_hBoard, &g_CsSysInfo);
	if (CS_FAILED(i32Status))
	{
		DisplayErrorString(i32Status);
		CsFreeSystem(m_hBoard);
		return (false);
	}
	
	// Display the system information
	printf("\nNumber of boards found: %u", g_CsSysInfo.u32BoardCount);
	printf("\nBoard name: %s", g_CsSysInfo.strBoardName);
	printf("\nBoard total memory size: %d", g_CsSysInfo.i64MaxMemory);
	printf("\nBoard vertical resolution (in bits): %u", g_CsSysInfo.u32SampleBits);
	printf("\nBoard sample resolution (used for voltage conversion): %d", g_CsSysInfo.i32SampleResolution);	
	printf("\nBoard sample size (in bytes): %u", g_CsSysInfo.u32SampleSize);
	printf("\nBoard sample offset (ADC output at middle of range): %d", g_CsSysInfo.i32SampleOffset);
	printf("\nBoard type (see BOARD_TYPES in CsDefines.h): %u", g_CsSysInfo.u32BoardType);
	printf("\nBoard addon options: %u", g_CsSysInfo.u32AddonOptions);
	printf("\nBoard options: %u", g_CsSysInfo.u32BaseBoardOptions);
	printf("\nBoard number of trigger engines: %u", g_CsSysInfo.u32TriggerMachineCount);
	printf("\nBoard number of channels: %u\n", g_CsSysInfo.u32ChannelCount);	

	// Get board information.
	ARRAY_BOARDINFO arrBoardInfo = {0};
	arrBoardInfo.u32BoardCount = 1;
	arrBoardInfo.aBoardInfo[0].u32Size = sizeof(CSBOARDINFO);
	i32Status = CsGet(m_hBoard, CS_BOARD_INFO, CS_CURRENT_CONFIGURATION, &arrBoardInfo);
	if (CS_FAILED(i32Status))
	{
		DisplayErrorString(i32Status);
		CsFreeSystem(m_hBoard);
		return (false);
	}
	
	// Display the board information
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


  // Configure the board

  // Fill in the channel configuration structure
  g_CsChannelCfg.u32Size = sizeof(CSCHANNELCONFIG);
  g_CsChannelCfg.u32ChannelIndex = CS_CHAN_1;	      // 1 corresponds to the first channel
  g_CsChannelCfg.u32Term = CS_COUPLING_DC;	        // Channel coupling and termination: see CHAN_DEFS in CsDefines.h 
  g_CsChannelCfg.u32Impedance = CS_REAL_IMP_50_OHM; // Channel impedance, in Ohms (50 or 1000000)
  g_CsChannelCfg.u32Filter = CS_FILTER_OFF;	        // Filter (default = 0 (No filter))
  g_CsChannelCfg.i32DcOffset = 0;		                // Channel DC offset, in mV ??? CS_GAIN_1_V	
  g_CsChannelCfg.i32Calib = 0;	                    // Channel onboard calibration (default = 0)

  // Fill in the acquisition configuration structure
  g_CsAcquisitionCfg.u32Size = sizeof(CSACQUISITIONCONFIG);
  g_CsAcquisitionCfg.i64SampleRate = 400000000; // Sample rate value in Hz
	g_CsAcquisitionCfg.u32ExtClk = 0;			        // External clocking.  A non-zero value means "active" and zero means "inactive"
	g_CsAcquisitionCfg.u32ExtClkSampleSkip = 0;   // Sample clock skip factor in external clock mode.  The sampling rate will 
	g_CsAcquisitionCfg.u32Mode = CS_MODE_SINGLE;	// Acquisition mode of the system: see ACQUISITION_MODES in CsDefines.h
	g_CsAcquisitionCfg.u32SampleBits = 0;         // Actual vertical resolution, in bits, of the CompuScope system.
	g_CsAcquisitionCfg.i32SampleRes = 0;		      // Actual sample resolution for the CompuScope system
	g_CsAcquisitionCfg.u32SampleSize = 0;		      // Actual sample size, in Bytes, for the CompuScope system
	g_CsAcquisitionCfg.u32SegmentCount = 0;	      // Number of segments per acquisition.
	g_CsAcquisitionCfg.i64Depth = 0;			        // Number of samples to capture after the trigger event is logged and trigger delay 
	g_CsAcquisitionCfg.i64SegmentSize = 0;		    // Maximum possible number of points that may be stored for one segment 
	g_CsAcquisitionCfg.i64TriggerTimeout = 0;     // Amount of time to wait (in 100 nanoseconds units) after start of segment 
  g_CsAcquisitionCfg.u32TrigEnginesEn = 0;	    // Enables the external signal used to enable or disable the trigger engines
	g_CsAcquisitionCfg.i64TriggerDelay = 0;       // Number of samples to skip after the trigger event before starting
	g_CsAcquisitionCfg.i64TriggerHoldoff = 0;     // Number of samples to acquire before enabling the trigger circuitry. The 
	g_CsAcquisitionCfg.i32SampleOffset = 0;       // Actual sample offset for the CompuScope system
	g_CsAcquisitionCfg.u32TimeStampConfig = TIMESTAMP_DEFAULT;	// Time stamp mode: see TIMESTAMPS_MODES in CsDefines.h
	g_CsAcquisitionCfg.i32SegmentCountHigh = 0;   // High patrt of 64-bit segment count. Number of segments per acquisition.

  // Fill in the trigger configuration structure (see TRIGGER_DEFS in CsDefines.h)
	g_CsTriggerCfg.u32Size = sizeof(CSTRIGGERCONFIG);
	g_CsTriggerCfg.u32TriggerIndex =  1;	                  // Trigger engine index. 1 corresponds to the first trigger engine.
	g_CsTriggerCfg.u32Condition =  CS_TRIG_COND_POS_SLOPE;  // See \link TRIGGER_DEFS Trigger condition \endlink constants
	g_CsTriggerCfg.i32Level =  100;			                    // Trigger level as a percentage of the trigger source input range (±100%)
	g_CsTriggerCfg.i32Source = CS_TRIG_SOURCE_DISABLE;      // Trigger source
	g_CsTriggerCfg.u32ExtCoupling = CS_COUPLING_DC;		      // External trigger coupling: AC or DC
	g_CsTriggerCfg.u32ExtTriggerRange = 1000;               // External trigger full scale input range, in mV
	g_CsTriggerCfg.u32ExtImpedance = 50; 	                  // External trigger impedance, in Ohms
	g_CsTriggerCfg.i32Value1 = 0;			                      // Reserved for future use
	g_CsTriggerCfg.i32Value2 = 0;			                      // Reserved for future use
	g_CsTriggerCfg.u32Filter = 0;			                      // Reserved for future use
	g_CsTriggerCfg.u32Relation = CS_RELATION_OR;            // Logical relation applied to the trigger engine outputs (default OR)


  // Fill in streaming configuration
  //uint32 u32BufferSizeBytes = STREAM_BUFFERSIZE;
	//uint32 u32TransferTimeout = TRANSFER_TIMEOUT;


  // Get current acquisition values
  i32Status = CsGet(m_hBoard, CS_ACQUISITION, CS_CURRENT_CONFIGURATION, &g_CsAcquisitionCfg);
	if (CS_FAILED(i32Status))
	{
		DisplayErrorString(i32Status);
		CsFreeSystem(m_hBoard);
		return (false);
	}
	
	// Display current acquisition configuration settings
	printf("\nAcquisition i64SampleRate: %d", g_CsAcquisitionCfg.i64SampleRate);
	printf("\nAcquisition u32ExtClk: %u", g_CsAcquisitionCfg.u32ExtClk);	
	printf("\nAcquisition u32ExtClkSampleSkip: %u", g_CsAcquisitionCfg.u32ExtClkSampleSkip);	
	printf("\nAcquisition u32Mode: %u", g_CsAcquisitionCfg.u32Mode);	
	printf("\nAcquisition u32SampleBits: %u", g_CsAcquisitionCfg.u32SampleBits);	
	printf("\nAcquisition i32SampleRes: %d", g_CsAcquisitionCfg.i32SampleRes);	
	printf("\nAcquisition u32SampleSize: %u", g_CsAcquisitionCfg.u32SampleSize);	
	printf("\nAcquisition u32SegmentCount: %u", g_CsAcquisitionCfg.u32SegmentCount);	
	printf("\nAcquisition i64Depth: %d", g_CsAcquisitionCfg.i64Depth);	
	printf("\nAcquisition i64SegmentSize: %d", g_CsAcquisitionCfg.i64SegmentSize);	
	printf("\nAcquisition i64TriggerTimeout: %d", g_CsAcquisitionCfg.i64TriggerTimeout);	
	printf("\nAcquisition u32TrigEnginesEn: %u", g_CsAcquisitionCfg.u32TrigEnginesEn);	
	printf("\nAcquisition i64TriggerDelay: %d", g_CsAcquisitionCfg.i64TriggerDelay);	
	printf("\nAcquisition i64TriggerHoldoff: %d", g_CsAcquisitionCfg.i64TriggerHoldoff);	
	printf("\nAcquisition i32SampleOffset: %d", g_CsAcquisitionCfg.i32SampleOffset);	
	printf("\nAcquisition u32TimeStampConfig: %u", g_CsAcquisitionCfg.u32TimeStampConfig);	
	printf("\nAcquisition i32SegmentCountHigh: %d", g_CsAcquisitionCfg.i32SegmentCountHigh);	
	
	
	
	
	// Check if selected system supports Expert Stream
	// And set the correct image to be used.
	int64	i64ExtendedOptions = 0;
	CsGet(m_hBoard, CS_PARAMS, CS_EXTENDED_BOARD_OPTIONS, &i64ExtendedOptions);
	
	printf("\nExtended board options: %d\n", i64ExtendedOptions);
	
	if (i64ExtendedOptions & CS_BBOPTIONS_STREAM)
	{
		printf("\nSelecting Expert Stream from image 1.");
		g_CsAcquisitionCfg.u32Mode |= CS_MODE_USER1;
	}
	else if ((i64ExtendedOptions >> 32) & CS_BBOPTIONS_STREAM)
	{
		printf("\nSelecting Expert Stream from image 2.");
		g_CsAcquisitionCfg.u32Mode |= CS_MODE_USER2;
	}
	else
	{
		printf("\nCurrent system does not support Expert Stream.");
		printf("\nApplication terminated.");
		CsFreeSystem(m_hBoard);
		return false;
	}

	/*
	Sets the Acquisition values down the driver, without any validation, 
	for the Commit step which will validate system configuration.
	*/

	i32Status = CsSet(m_hBoard, CS_ACQUISITION, &g_CsAcquisitionCfg);
	if (CS_FAILED(i32Status))
	{
		DisplayErrorString(i32Status);
		return CS_MISC_ERROR;
	}
	
	
  // Free the CompuScope system so it's available for another application
	DisplayErrorString(i32Status);
	CsFreeSystem(m_hBoard);
	return(false);

/*
	// Load application specific information from the ini file
	i32Status = LoadStmConfiguration(szIniFile, &g_StreamConfig);
	if (CS_FAILED(i32Status))
	{
		DisplayErrorString(i32Status);
		CsFreeSystem(m_hBoard);
		return (false);
	}
	if (CS_USING_DEFAULTS == i32Status)
	{
	  printf("\nNo ini entry for Stm configuration. Using defaults.");
	}

	// Streaming Configuration.
	// Validate if the board supports hardware streaming. If it is not supported,
	// we'll exit gracefully.
	i32Status = InitializeStream(m_hBoard);
	if (CS_FAILED(i32Status))
	{
		// Error string was displayed in InitializeStream
		CsFreeSystem(m_hBoard);
		return (false);
	}

	if (CreateIPCObjects()==-1)
	{
		_ftprintf (stderr, _T("\nUnable to create IPC object for synchronization.\n"));
		CsFreeSystem(m_hBoard);
		return (false);
	}


	// Detect if the CompuScope card supports Streamming with Data Packing 
	i32Status =	CsGet( m_hBoard, 0, CS_DATAPACKING_MODE, &CurrentPackConfig );
	if ( CS_FAILED(i32Status) )
	{
		if ( CS_INVALID_PARAMS_ID == i32Status )
		{
			// The parameter CS_DATAPACKING_MODE is not supported
			if ( 0 != g_StreamConfig.DataPackCfg )
			{
				_ftprintf (stderr, _T("\nThe current CompuScope system does not support Data Streamming in Packed mode.\n"));
				CsFreeSystem(m_hBoard);
				return (false);
			}
		}
		else
		{
			// Other errors
			DisplayErrorString(i32Status);
			CsFreeSystem(m_hBoard);
			return (false);
		}
	}
	else
	{
		// The CompuScope system support Streamming in Packed data format.
		// Set the Data Packing mode for streaming
		// This should be done before ACTION_COMMIT
		i32Status =	CsSet( m_hBoard, CS_DATAPACKING_MODE, &g_StreamConfig.DataPackCfg );
		if (CS_FAILED(i32Status))
		{
			DisplayErrorString(i32Status);
			CsFreeSystem(m_hBoard);
			return (false);
		}
	}

	// Commit the values to the driver.  This is where the values get sent to the
	// hardware.  Any invalid parameters will be caught here and an error returned.
	i32Status = CsDo(m_hBoard, ACTION_COMMIT);
	if (CS_FAILED(i32Status))
	{
		DisplayErrorString(i32Status);
		CsFreeSystem(m_hBoard);
		return (false);
	}

	// After ACTION_COMMIT, the sample size may change.
	// Get user's acquisition data to use for various parameters for transfer
	g_CsAcqCfg.u32Size = sizeof(CSACQUISITIONCONFIG);
	i32Status = CsGet(m_hBoard, CS_ACQUISITION, CS_CURRENT_CONFIGURATION, &g_CsAcqCfg);
	if (CS_FAILED(i32Status))
	{
		DisplayErrorString(i32Status);
		CsFreeSystem(m_hBoard);
		return (false);
	}

	// Get the total amount of data we expect to receive.
	// We can get this value from driver or calculate the following formula
	// llTotalSamplesConfig = (g_CsAcqCfg.i64SegmentSize + SegmentTail_Size) * (g_CsAcqCfg.u32Mode&CS_MASKED_MODE) * g_CsAcqCfg.u32SegmentCount;
	i32Status = CsGet( m_hBoard, 0, CS_STREAM_TOTALDATA_SIZE_BYTES, &llTotalSamplesConfig );
	if (CS_FAILED(i32Status))
	{
		DisplayErrorString(i32Status);
		CsFreeSystem(m_hBoard);
		return (false);
	}

	// Convert to number of samples
	if ( -1 != llTotalSamplesConfig )
	{
		llTotalSamplesConfig /= g_CsAcqCfg.u32SampleSize;
	}

	//  Create threads for stream
	thread = malloc(g_CsSysInfo.u32BoardCount * sizeof(pthread_t));
	threadFinished = malloc(g_CsSysInfo.u32BoardCount * sizeof(BOOL));

	for (n = 1, i = 0; n <= g_CsSysInfo.u32BoardCount; n++, i++ )
	{
			// open new file
			g_hFile[n-1] = fopen(szSaveFileName,"w+");
			if ( 0 == g_hFile[n-1] )
			{
				printf"\n Unable to open file <%s> <errno = %d msg: %s> \n", szSaveFileName, errno, strerror(errno));
				CsFreeSystem(m_hBoard);
				return (-1);
			}
		}
		fflush(stdout);
		pthread_attr_init(&attr);
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
		ret = pthread_create(&thread[i], &attr, CardStreamThread, (void*)&n);
		threadFinished[i] = FALSE;

		if (ret != 0)
		{
			printf("Unable to create thread for card %d.\n");
			g_bStreamError = TRUE;
			CsFreeSystem(m_hBoard);
			free(thread);
			free(threadFinished);
			DestroyIPCObjects();
			return (false);
		}
		else
		{
			// Wait for the g_hThreadReadyForStream to make sure that the thread was successfully created and are ready for stream
			struct timespec wait_timeout ={0};
			pthread_mutex_lock(&g_hThreadReadyForStream.mutex);
			ComputeTimeout(5000, &wait_timeout);
			pthread_cond_timedwait(&g_hThreadReadyForStream.cond, &g_hThreadReadyForStream.mutex, &wait_timeout);
			pthread_mutex_unlock(&g_hThreadReadyForStream.mutex);

			// make sure the CardStreamThread start normally
			if (g_ThreadStartStatus < 0)
			{
       		g_bStreamError = TRUE;
				  CsFreeSystem(m_hBoard);
				  free(thread);
				  free(threadFinished);
				  DestroyIPCObjects();
				  return (false);
			}
		}
	} // End for

*/


  // Apply board settings
  /*if (!setVoltageRange(1, m_uVoltageRange1)) { return false; }
  if (!setVoltageRange(2, m_uVoltageRange2)) { return false; }
  if (!setInputChannel(m_uInputChannel)) { return false; }
  if (!setAcquisitionRate(m_dAcquisitionRate)) { return false; }
  if (!setTransferSamples(m_uSamplesPerTransfer)) { return false; }
*/

  return true;

} // connect()



// -----------------------------------------------------------
// disconnect() -- Disconnect from and free resouces used for 
//                  PX14400 device
// -----------------------------------------------------------
void RazorMax::disconnect() 
{

  int32 i32Status = CS_SUCCESS;
  
  if (m_hBoard) {

    // Abort the current acquisition
	  CsDo(m_hBoard, ACTION_ABORT);

	  // Free the CompuScope system and any resources it's been using
	  i32Status = CsFreeSystem(m_hBoard);

	  //if ( !g_bStreamError && !g_bCtrlAbort && !g_bTransferError )
	  //{
		//  printf("\nStream has finished %u loops.\n", g_CsAcqCfg.u32SegmentCount);
	  //}
	  	  
	  printf("\nFreeing resources..\n");
	  //free(thread);
	  //free(threadFinished);
	  //DestroyIPCObjects();
	  
    m_hBoard = 0;
    m_uSerialNumber = -1;
    m_uBoardRevision = -1;
  }
} // disconnect()



// ----------------------------------------------------------------------------
// setTransferSamples 
// ----------------------------------------------------------------------------
bool RazorMax::setTransferSamples(unsigned int uSamplesPerTransfer) 
{
  return true;

} // setTransferSamples()



// ----------------------------------------------------------------------------
// setAcquisitionRate - Acquisition rate in MS/s
// ----------------------------------------------------------------------------
bool RazorMax::setAcquisitionRate(double dAcquisitionRate) 
{
  return true;

} // setAcquisitionRate()



// ----------------------------------------------------------------------------
// setInputChannel
// ----------------------------------------------------------------------------
bool RazorMax::setInputChannel(unsigned int uChannel) 
{
  return true;

} // setInputChannel()


// ----------------------------------------------------------------------------
// setVoltageRange
// ----------------------------------------------------------------------------
bool RazorMax::setVoltageRange(unsigned int uChannel, unsigned int uRange) 
{
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
 /*
  void*		pBuffer1		= NULL;
	void*		pBuffer2		= NULL;
	void*		pCurrentBuffer	= NULL;
	BOOL		bDone					= FALSE;
	BOOL		bStreamCompletedSuccess = FALSE;

	uInt8		u8EndOfData = 0;

	uInt16		nCardIndex = *((uInt16 *) CardIndex);

	uInt32		u32LoopCount			= 0;	
	uInt32		u32TransferSize_Samples	= 0;	
	uInt32		u32ProcessedSegments	= 0;
	uInt32		u32SegmentCount			= 0;
	uInt32		u32NbrFiles				= 0;
	uInt32		u32ErrorFlag			= 0;
	uInt32		u32ActualLength			= 0;
	uInt32		u32TailLeftOver			= 0;
	int32		i32Status;
	int64		i64SegmentSizeBytes 	= 0;	
	int64		i64DataInSegment_Samples= 0;
	uInt32		u32SegmentTail_Bytes	= 0;
	uInt32		u32SegmentFileCount		= 1;

	STREAM_INFO	streamInfo = {0};

	
	// In Streaming, all channels are transferred within the same buffer. 
	// Calculate the size of data in a segment
	// (Segment size * number of channels)
	i64DataInSegment_Samples = (g_CsAcqCfg.i64SegmentSize*(g_CsAcqCfg.u32Mode & CS_MASKED_MODE));

  // In Streaming, some hardware related information are placed at the end of each segment. 
  // These samples contain also timestamp information for the segment. The number of extra 
  // bytes may be different depending on CompuScope card model.
	
	i32Status = CsGet(g_hSystem, 0, CS_SEGMENTTAIL_SIZE_BYTES, &u32SegmentTail_Bytes);
	if (CS_FAILED(i32Status))
	{
		DisplayErrorString(i32Status);
		g_bStreamError = TRUE;
		return NULL;
	}

	// We are gonna need the size of the segment with his tail for the analysis function.

	// To simplify the algorithm in the analysis function, the buffer size used for transfer 
	// should be multiple or a fraction of the size of segment
	// Resize the buffer size.
	i64SegmentSizeBytes = i64DataInSegment_Samples*g_CsAcqCfg.u32SampleSize;

	u32TransferSize_Samples = g_StreamConfig.u32BufferSizeBytes/g_CsAcqCfg.u32SampleSize;
	_ftprintf (stderr, _T("\n(Actual buffer size used for data streaming = %u Bytes)\n"), g_StreamConfig.u32BufferSizeBytes );

	// We adjust the size of the memory allocated for the timeStamp buffer if we can fit more 
	// segment + tail in the buffer than the SegmentCount
	if(g_i64SegmentPerBuffer > MAX_SEGMENTS_COUNT)
	{
		streamInfo.pi64TimeStamp = (int64 *)calloc(g_i64SegmentPerBuffer + 1, sizeof(int64));
	}
	else
	{
		streamInfo.pi64TimeStamp = (int64 *)calloc(MIN(g_CsAcqCfg.u32SegmentCount,MAX_SEGMENTS_COUNT), sizeof(int64));
	}
	
	if (NULL == streamInfo.pi64TimeStamp)
	{
		_ftprintf (stderr, _T("\nUnable to allocate memory to hold analysis results.\n"));
		g_bStreamError = TRUE;
		return NULL;
	}

	// We need to allocate a buffer for transferring the data. The buffer must be allocated
  // by a call to CsStmAllocateBuffer.  This routine will allocate a buffer suitable for 
  // streaming.  In this program we're allocating 2 streaming buffers so we can transfer 
  // to one while doing analysis on the other.
	i32Status = CsStmAllocateBuffer(g_hSystem, nCardIndex, g_StreamConfig.u32BufferSizeBytes, &pBuffer1);
	if (CS_FAILED(i32Status))
	{
		_ftprintf (stderr, _T("\nUnable to allocate memory for stream buffer 1.\n"));
		free(streamInfo.pi64TimeStamp);
		g_bStreamError = TRUE;
		g_ThreadStartStatus = -1;
		return NULL;
	}

	i32Status = CsStmAllocateBuffer(g_hSystem, nCardIndex, g_StreamConfig.u32BufferSizeBytes, &pBuffer2);
	if (CS_FAILED(i32Status))
	{
		printf("\nUnable to allocate memory for stream buffer 2.\n");
		CsStmFreeBuffer(g_hSystem, nCardIndex, pBuffer1);
		free(streamInfo.pi64TimeStamp);
		g_bStreamError = TRUE;
		g_ThreadStartStatus = -1;		
		return NULL;
	}

  // *** Need to add a start acquisition call here *** ??? //

	// Steam acqusition has started.
  //
	// Loop until either we've done the number of segments we want, or
	// the ESC key was pressed to abort. While we loop we transfer into
	// pCurrentBuffer and do our analysis on pWorkBuffer
	

	if (NULL == streamInfo.pi64TimeStamp )
	{
		_ftprintf (stderr, _T("\nUnable to allocate memory to hold analysis results.\n"));
		g_bStreamError = TRUE;
		return NULL;
	}

	// Variable declaration for streaming
	streamInfo.u32BufferSize	= g_StreamConfig.u32BufferSizeBytes;
	streamInfo.i64SegmentSize	= i64SegmentSizeBytes;
	streamInfo.u32TailSize		= u32SegmentTail_Bytes;

	streamInfo.i64ByteToEndSegment	= i64SegmentSizeBytes;
	streamInfo.u32ByteToEndTail	= u32SegmentTail_Bytes;
	streamInfo.u32LeftOverSize	= u32TailLeftOver;

	streamInfo.i64LastTs		= 0;
	streamInfo.u32Segment		= 1;

	streamInfo.u32SegmentCountDown  = g_CsAcqCfg.u32SegmentCount;

	streamInfo.bSplitTail		= FALSE;	// Default value

	while( ! ( bDone || bStreamCompletedSuccess) )
	{
		
		// Check if user has aborted or an error has occured
		if (g_bCtrlAbort || g_bStreamError)
		{
			break;
		}
		
		// Determine where new data transfer data will go. We alternate
		// between our 2 DMA buffers
		if (u32LoopCount & 1)
		{
			pCurrentBuffer = pBuffer2;
		}
		else
		{
			pCurrentBuffer = pBuffer1;
		}

		i32Status = CsStmTransferToBuffer(g_hSystem, nCardIndex, pCurrentBuffer, u32TransferSize_Samples);
		if (CS_FAILED(i32Status))
		{
			g_bStreamError = TRUE;
			DisplayErrorString(i32Status);
			break;
		}

		if ( g_StreamConfig.bDoAnalysis )
		{
			if ( NULL != streamInfo.pWorkBuffer )
			{
				
				// Do analysis on the previous buffer

				u32ProcessedSegments = AnalysisFunc(&streamInfo);
				
				// Save data in the file
				if ( u32ProcessedSegments > 0 )
				{
					SaveResults(&streamInfo, u32ProcessedSegments, &u32SegmentCount, &u32NbrFiles, &pFile, nCardIndex, &u32SegmentFileCount);
				}
				g_u32SegmentCounted[nCardIndex-1] += u32ProcessedSegments;
				
				// Note: g_i64TsFrequency holds the frequency of the timestamp clock. This is needed to convert the timestamp
				// counter values into actual time values. If you need it, you should make not of it or save it to a file.
			}
		}
		
		// Wait for the DMA transfer on the current buffer to complete so we can loop 
		// back around to start a new one. Calling thread will sleep until
		// the transfer completes
			
		i32Status = CsStmGetTransferStatus( g_hSystem, nCardIndex, g_StreamConfig.u32TransferTimeout, &u32ErrorFlag, &u32ActualLength, &u8EndOfData );
		if ( CS_SUCCEEDED(i32Status) )
		{
			
			//Calculate the total of data for this card
			g_llTotalSamples[nCardIndex-1] += u32TransferSize_Samples;
			bStreamCompletedSuccess = (0 != u8EndOfData);
		}
		else
		{
			g_bStreamError = TRUE;
			bDone = TRUE;
			if (CS_STM_TRANSFER_TIMEOUT == i32Status)
			{
				// Timeout on CsStmGetTransferStatus().
				// Data transfer has not yet completed. We can repeat calling CsStmGetTransferStatus() until 
				// we get the status success (ie data transfer is completed)
				// In this sample program, we consider the timeout as an error
				DisplayErrorString(i32Status);
			}
			else
			{
				DisplayErrorString(i32Status);				
			}
			break;
		}

		streamInfo.pWorkBuffer = pCurrentBuffer;

		u32LoopCount++;

	}
	if (g_StreamConfig.bDoAnalysis)
	{
		// Do analysis on the last buffer
		u32ProcessedSegments = AnalysisFunc(&streamInfo);
		SaveResults(&streamInfo, u32ProcessedSegments,  &u32SegmentCount, &u32NbrFiles, &pFile, nCardIndex, &u32SegmentFileCount);
		g_u32SegmentCounted[nCardIndex-1] += u32ProcessedSegments;
		fclose(pFile);
	}

	free(streamInfo.pi64TimeStamp);
	CsStmFreeBuffer(g_hSystem, 0, pBuffer1);
	CsStmFreeBuffer(g_hSystem, 0, pBuffer2);
 
 */
 
 
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

