#ifndef _SWLABJACK_H_
#define _SWLABJACK_H_

#include "switch.h"
#include "labjackusb.h"

using namespace std;

// Labjack setup for the CC1 board in EDGES
// See Raul's LoCo Memo:  EDGES Temperature Control Circuit
// for circuit diagram of CC1 (temperature_control_20141208.pdf)
// DB15 - dark gray: CC1-4, LJ-EIO7
// DB14 - white: CC1-2, LJ-EIO5
// DB13 - light gray: CC1-3, LJ-EIO3
// DB12 - blue: CC1-1, LJ-EIO1
// DB11 - black: CC1-GND, LJ-GND

// For VNA switch, the config is:
// CC1-1 + CC1-4 = 27V (open)
// CC1-2 + CC1-4 = 30V (short)
// CC1-3 + CC1-4 = 33V (match)
// CC1-4 only = 35V (antenna)


// For 3-pos switch, the config is:
// CC1-1 + CC1-4 = 17V (p0 - ant)
// CC1-2 + CC1-4 = 20V (p1 - amb)
// CC1-3 + CC1-4 = 24V (p2 - hot)
// xxxx CC1-4 only = ??

// See labjack U3 manual, section 5.2.2 and 5.2.3 for packet info

// ---------------------------------------------------------------------------
//
// SWTTY -- Switch control by sending ASCII characters to a TTY device
//
// ---------------------------------------------------------------------------
class SWLabJack : public Switch {

  private:

    unsigned int m_uSwitchState;
		HANDLE m_hDevice;

  public:

    // Constructor and destructor
    SWLabJack() {
      m_uSwitchState = 0;
			m_hDevice = NULL;

      printf("SWLabJack: Using LabJack U3 switch\n");
    }

    ~SWLabJack() {

			if (m_hDevice != NULL) {
				LJUSB_CloseDevice(m_hDevice);
				m_hDevice = NULL;
			}		
		}

		void addChecksum(uint8_t *pPacket, uint8_t iLength) {

				uint16_t iSum = 0;
				uint16_t iQuot = 0;

				// Calculate the checksum 16 value
				for( int i = 6; i < iLength; i++ ) {
				    iSum += (uint16_t) pPacket[i];
				}

				pPacket[4] = (uint8_t)(iSum & 0xFF);
				pPacket[5] = (uint8_t)((iSum / 256) & 0xFF);

				// Add the first byte checksum 8 value
				iSum = 0;
				for( int i = 1; i < 6; i++ ) {
				    iSum += (uint16_t) pPacket[i];
				}

				// Now sum quotient and remainder of 256 division, then repeat.
				iQuot = iSum / 256;
				iSum = (iSum - 256*iQuot) + iQuot;
				iQuot = iSum / 256;

				pPacket[0] = (uint8_t) ((iSum - 256*iQuot) + iQuot);
		}

    bool init() {

		  // Open the U3
		  m_hDevice = LJUSB_OpenDevice(1, 0, U3_PRODUCT_ID);
		  
		  if( m_hDevice == NULL ) {
		    printf("SWLabJack: Failed to connect to LabJack U3.\n");
				return false;
		  }

			printf("SWLabJack: Writing configuration to LabJackU3...\n");

			// Build the configuration command packet to make sure the EIO pins 
			// are set to digital.  We're assuming the CC1 board uses pins E1, 
			// E3, E5, and E7.  We're setting all EIO pins to digital (bits = 0).
			uint8_t sendBuff[12];
			sendBuff[1] = (uint8_t)(0xF8);  // Command byte
			sendBuff[2] = (uint8_t)(0x03);  // Number of data words
			sendBuff[3] = (uint8_t)(0x0B);  // Extended command number
			sendBuff[6] = 8;  						// Writemask (4 = FIO, 8 = EIO)
			sendBuff[7] = 0;  						// Reserved
			sendBuff[8] = 0;  						// TimerCounterConfig
			sendBuff[9] = 0;  						// DAC1 enable
			sendBuff[10] = 0;  						// FIO analog (1) vs digital (0)
			sendBuff[11] = 0;  					  // EIO analog (1) vs digital (0)

			// Packet bytes 0, 4, and 5 are filled in by checksum		  
			addChecksum(sendBuff, 12);

			// Send the configuration
			if (LJUSB_Write(m_hDevice, sendBuff, 12) < 12) {
				printf("SWLabJack: Failed to write configuration to LabJack U3.\n");
				return false;
			} else {
				printf("SWLabJack: Done writing configuration to LabJack U3.\n");
			}

			// Set to state 0
			set(0);

      return true;
    }

		bool writeFeedbackPacket(uint8_t* pData, uint8_t iLength) {

		  if( m_hDevice == NULL ) {
		    printf("SWLabJack: Failed to connect to LabJack U3.\n");
				return false;
		  }

			// Calculate the data content length value
			uint8_t iDataLengthByte = iLength + 1;
			if ( (iDataLengthByte % 2) != 0 ) {
        iDataLengthByte++;
			}
			iDataLengthByte /= 2;

			// Create the packet
			uint8_t iPacketLength = iLength + 7;
			uint8_t* pPacket = new uint8_t[iPacketLength];

			pPacket[1] = (uint8_t)(0xF8);  // Command byte
			pPacket[2] = iDataLengthByte;  // Number of data words
			pPacket[3] = (uint8_t)(0x00);  // Always 0x00
			pPacket[6] = 0;  						 // Echo (returned in response)		
		
			for (int i = 0; i < iLength; i++) {
				pPacket[7 + i] = pData[i];
			}

			addChecksum(pPacket, iPacketLength);
			
			// Write the packet to the LabJack
			if (LJUSB_Write(m_hDevice, pPacket, iPacketLength) < iPacketLength) {
				printf("SWLabJack: Failed to write feedback packet.\n");
				return false;
			}
			
			return true;
		}
			

		uint8_t getBitValues(unsigned int uSwitchState) {

	/*		// On = high logic
			if (uSwitchState == 0) {
				return (uint8_t) 130;  // E1 and E7
			} else if (uSwitchState == 1) {
				return (uint8_t) 136;  // E3 and E7
			} else {
				return (uint8_t) 160;  // E5 and E7
			}	
*/
			// On = low logic
			if (uSwitchState == 0) {
				return (uint8_t) 40;  // E1 and E7 on, E3 and E5 off
			} else if (uSwitchState == 1) {
				return (uint8_t) 34;  // E3 and E7 on, E1 and E5 off
			} else {
				return (uint8_t) 10;  // E5 and E7 on, E1 and E3 off
			}	


		}


    unsigned int set(unsigned int uSwitchState) {

      // Send the switch state to the LabJack
			uint8_t dataBuff[7];
			dataBuff[0] = 27;		// IOType = PortStateWrite (forces dir to write)
			dataBuff[1] = 0;		// Writemask FIO bits	
			dataBuff[2] = 170;  // Writemask EIO bits	 (Assumes we are using bits 1, 3, 5, 7 (mask = 2+8+32+129 = 170)
			dataBuff[3] = 0;		// Writemask CIO bits	
			dataBuff[4] = 0;  	// FIO bit values
			dataBuff[5] = getBitValues(uSwitchState);  	// EIO bit values
			dataBuff[6] = 0;  	// CIO bit values

			writeFeedbackPacket(dataBuff, 7);
      
			// Remember the switch state
      m_uSwitchState = uSwitchState;
      return uSwitchState;
    }

    unsigned int increment() {
      return set((m_uSwitchState + 1) % 3);
    }

    unsigned int get() { return m_uSwitchState; }

};

#endif // _SWLABJACK_H_
