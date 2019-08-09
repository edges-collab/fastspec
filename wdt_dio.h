//*****************************************************************************
//  Copyright (c) 2016, Neousys Technology Inc. All rights reserved.
//
//  Library:     WDT_DIO
//  Revision:    2.2.0
//  Date:        July 31th, 2015
//  Description: Neousys series platforms support additional Dynamic Linking
//               Library (DLL), which have Watch-Dog Timer (WDT), Digital
//               Input Output (DIO), etc.
//  Remark 1:    The 32-bit library is based on WinIO library, which is the
//               copyright of Yariv Kaplan 1998-2010.
//  Remark 2:    The 64-bit library is based on WinRing0 library, which is the
//               copyright of OpenLibSys.
//  Remark 3:    These libraries do not support WOW64 directly.
//*****************************************************************************

#ifndef  Neousys_WDT_DIO_Library
#define  Neousys_WDT_DIO_Library

#pragma  once

#ifndef  BYTE
#define  BYTE   unsigned char
#endif

#ifndef  WORD
#define  WORD   unsigned short
#endif

#ifndef  BOOL
#define  BOOL   int
#endif

#if defined(__GNUC__) && __GNUC__ >= 4
#ifndef  DWORD
#define  DWORD  unsigned int
#endif
#define  __cdecl
#define  __stdcall
#pragma  GCC visibility  push(default)
#else
#ifndef  DWORD
#define  DWORD  unsigned long
#endif
#endif

#pragma  pack(push, 1)

#ifdef  __cplusplus
extern "C" {
#endif

// DICOS, setup
//
typedef struct COS_INT_SETUP_T
{
    WORD    portMask;       // [in ] interrupt mask for corresponding DI channel(s), 0: disable 1: enable COS interrupt
    WORD    edgeMode;       // [in ] specify the condition of generating COS interrupt for corresponding DI channel(s), 0: level change 1: rising/falling edge
    WORD    edgeType;       // [in ] specify the rising/falling edge for corresponding DI channel(s), 0: rising edge 1: falling edge
}
COS_INT_SETUP, *COS_INT_SETUP_P;

// DICOS, callback argument
//
typedef struct COS_INT_CALLBACK_ARG_T
{
    WORD    portData;       // [out] port input data
    WORD    intrFlag;       // [out] interrupt flag
    BYTE    intrSeq;        // [out] interrupt sequence number (0~255)
}
COS_INT_CALLBACK_ARG, *COS_INT_CALLBACK_ARG_P;

typedef void (__stdcall* COS_INT_CALLBACK)(COS_INT_CALLBACK_ARG* arg);

// Deterministic Trigger I/O (DTIO), setup
// note: if trigSrcDI is configured for multiple pulseTgtDO(s), the last rising/falling edge setting is effecitve only
//
typedef struct DTIO_SETUP_T
{
    BYTE    trigMode;       // [in ] 0: never triggered (disabled mode), 1: always triggered, 2: rising edge, 3: falling edge
    BYTE    trigSrcDI;      // [in ] single DI channel used as trigger source input (ex: 0~7 for Nuvo-3000)
    BYTE    pulseTgtDO;     // [in ] single DO channel used as pulse target output (ex: 0~7 for Nuvo-3000)
    BYTE    pulseExtra;     // [in ] some extra-parameter combination for DTIO function (ex: DTIO_INIT_HIGN)
    DWORD   pulseDelay;     // [in ] output pulse delay tick count, range: 2~2147483647
    DWORD   pulseWidth;     // [in ] output pulse width tick count, range: 1~2147483647
}
DTIO_SETUP, *DTIO_SETUP_P;

// DTIO_SETUP.pulseExtra
//
#define DTIO_INIT_HIGN      (0x01)  // active-low

// Deterministic Trigger Fan Out (DTFO), setup
//
typedef struct DTFO_SETUP_T
{
    BYTE    trigMode;       // [in ] 0: never triggered (disabled mode), 1: reserved, 2: rising edge, 3: falling edge
    BYTE    trigSrcDI;      // [in ] single DI channel used as trigger source input (ex: 0~7 for Nuvo-3000)
    BYTE    pulseTgtDO;     // [in ] single DO channel used as pulse target output (ex: 0~7 for Nuvo-3000)
    BYTE    pulseExtra;     // [in ] some extra-parameter combination for DTFO function (ex: DTFO_INIT_HIGN)
    DWORD   pulseTag;       // [in ] reserved field
    DWORD   pulseWidth;     // [in ] output pulse width tick count, range: 1~2147483647
}
DTFO_SETUP, *DTFO_SETUP_P;

// DTFO_SETUP.pulseExtra
//
#define DTFO_INIT_HIGN      (0x01)  // active-low

// LED, mode
//
#define LED_DISABLE         (0x00000000)
#define LED_MODE_CC         (0x00000001)
#define LED_MODE_CV         (0x00000002)

// IGN, setting
//
typedef struct IGN_SETTING_T
{
    BYTE    mode;               // [out] 0: atx, 1: at, 2~7: ign mode
    BYTE    batteryType;        // [out] 0: 12v, 1: 24v
    BYTE    isSmartOff;         // [out] 0: disabled, 1: enabled
    BYTE    isPostCheck;        // [out] 0: disabled, 1: enabled, bios ok
    DWORD   onDelay;            // [out] in seconds
    DWORD   offDelay;           // [out] in seconds
    DWORD   hardOffTimeout;     // [out] in seconds
    double  lowVolThreshold;    // [out] low voltage threshold
}
IGN_SETTING, *IGN_SETTING_P;

// IGN_SETTING.mode
//
#define IGN_MODE_ATX        (0x00)
#define IGN_MODE_AT         (0x01)
#define IGN_MODE_IGN        (0x02)

// IGN_SETTING.batteryType
//
#define IGN_BATT_12V        (0x00)
#define IGN_BATT_24V        (0x01)

// CAN, setup
//
typedef struct CAN_SETUP_T
{
    DWORD   bitRate;        // [in ] bit rate, default: 500k
    DWORD   recvConfig;     // [in ] holds various settings for received message.
    DWORD   recvId;         // [in ] message identifier used for 11 or 29 bit identifiers
    DWORD   recvMask;       // [in ] message identifier mask used when identifier filtering is enabled.
}
CAN_SETUP, *CAN_SETUP_P;

// CAN, message object
//
typedef struct CAN_MSG_T
{
    DWORD   id;             // [i/o] message identifier used for 11 or 29 bit identifiers
    WORD    flags;          // [i/o] holds various status flags and settings.
    BYTE    extra;          // [res] reserved fields
    BYTE    len;            // [i/o] valid:1~8, number of bytes of data in the message object.
    BYTE    data[8];        // [i/o] message object's data.
}
CAN_MSG, *CAN_MSG_P;

// CAN, bit timing
//
typedef struct CAN_BITCLK_T
{
    WORD  syncPropPhase1Seg;    // [in ] valid:2~16, sum of the Synchronization, Propagation, and Phase Buffer 1 segments, measured in time quanta.
    WORD  phase2Seg;            // [in ] valid:1~8, the Phase Buffer 2 segment in time quanta.
    WORD  jumpWidth;            // [in ] valid:1~4, Resynchronization Jump Width in time quanta
    WORD  quantumPrescaler;     // [in ] valid:1~1023, CAN_CLK divider used to determine time quanta
}
CAN_BITCLK, *CAN_BITCLK_P;

// CAN_MSG.flags
//
#define CAN_MSG_EXTENDED_ID     (0x0004)
#define CAN_MSG_REMOTE_FRAME    (0x0040)
#define CAN_MSG_DATA_NEW        (0x0080)
#define CAN_MSG_DATA_LOST       (0x0100)

// CAN_SETUP.recvConfig
//
#define CAN_MSG_USE_ID_FILTER   (0x00000008)
#define CAN_MSG_USE_DIR_FILTER  (0x00000010 | CAN_MSG_USE_ID_FILTER)  // only accept the direction specified in the message type
#define CAN_MSG_USE_EXT_FILTER  (0x00000020 | CAN_MSG_USE_ID_FILTER)  // filters on only extended identifiers

// CAN, status
//
#define CAN_STATUS_BUS_OFF      (0x00000080)
#define CAN_STATUS_EWARN        (0x00000040)  // can controller error level has reached warning level.
#define CAN_STATUS_EPASS        (0x00000020)  // can controller error level has reached error passive level.
#define CAN_STATUS_LEC_STUFF    (0x00000001)  // a bit stuffing error has occurred.
#define CAN_STATUS_LEC_FORM     (0x00000002)  // a formatting error has occurred.
#define CAN_STATUS_LEC_ACK      (0x00000003)  // an acknowledge error has occurred.
#define CAN_STATUS_LEC_BIT1     (0x00000004)  // the bus remained a bit level of 1 for longer than is allowed.
#define CAN_STATUS_LEC_BIT0     (0x00000005)  // the bus remained a bit level of 0 for longer than is allowed.
#define CAN_STATUS_LEC_CRC      (0x00000006)  // a crc error has occurred.
#define CAN_STATUS_LEC_MASK     (0x00000007)  // this is the mask for the can last error code (lec).

// PWM, generator setup
//
typedef struct PWM_GEN_SETUP_T
{
    DWORD   genMode;       // [in ] logical OR of PWM_GEN_MODE_DOWN, PWM_GEN_MODE_UP_DOWN, ...
    DWORD   intrTriggers;  // [in ] interrupts and triggers to be enabled
    WORD    deadBandRise;  // [in ] valid:0~4096, width of delay from the rising edge, 0:disable
    WORD    deadBandFall;  // [in ] valid:0~4096, width of delay from the falling edge, 0:disable
}
PWM_GEN_SETUP, *PWM_GEN_SETUP_P;

// PWM_GEN_SETUP.genMode
//
#define PWM_GEN_MODE_DOWN               (0x00000000)  // down count mode
#define PWM_GEN_MODE_UP_DOWN            (0x00000002)  // up/down count mode
#define PWM_GEN_MODE_SYNC               (0x00000038)  // synchronous updates
#define PWM_GEN_MODE_NO_SYNC            (0x00000000)  // immediate updates
//
#define PWM_GEN_MODE_FAULT_LATCHED      (0x00040000)  // fault is latched
#define PWM_GEN_MODE_FAULT_UNLATCHED    (0x00000000)  // fault is not latched
#define PWM_GEN_MODE_FAULT_MINPER       (0x00020000)  // enable min fault period
#define PWM_GEN_MODE_FAULT_NO_MINPER    (0x00000000)  // disable min fault period
#define PWM_GEN_MODE_FAULT_EXT          (0x00010000)  // enable extended fault support
#define PWM_GEN_MODE_FAULT_LEGACY       (0x00000000)  // disable extended fault support
#define PWM_GEN_MODE_DB_NO_SYNC         (0x00000000)  // deadband updates occur immediately
#define PWM_GEN_MODE_DB_SYNC_LOCAL      (0x0000A800)  // deadband updates locally synchronized
#define PWM_GEN_MODE_DB_SYNC_GLOBAL     (0x0000FC00)  // deadband updates globally synchronized
#define PWM_GEN_MODE_GEN_NO_SYNC        (0x00000000)  // generator mode updates occur immediately
#define PWM_GEN_MODE_GEN_SYNC_LOCAL     (0x00000280)  // generator mode updates locally synchronized
#define PWM_GEN_MODE_GEN_SYNC_GLOBAL    (0x000003C0)  // generator mode updates globally synchronized

// PWM_GEN_SETUP.intrTriggers
//
#define PWM_INTR_CNT_ZERO   (0x00000001)  // intr if count = 0
#define PWM_INTR_CNT_LOAD   (0x00000002)  // intr if count = load
#define PWM_INTR_CNT_AU     (0x00000004)  // intr if count = cmpa u
#define PWM_INTR_CNT_AD     (0x00000008)  // intr if count = cmpa d
#define PWM_INTR_CNT_BU     (0x00000010)  // intr if count = cmpa u
#define PWM_INTR_CNT_BD     (0x00000020)  // intr if count = cmpa d
//
#define PWM_TRIG_CNT_ZERO   (0x00000100)  // trig if count = 0
#define PWM_TRIG_CNT_LOAD   (0x00000200)  // trig if count = load
#define PWM_TRIG_CNT_AU     (0x00000400)  // trig if count = cmpa u
#define PWM_TRIG_CNT_AD     (0x00000800)  // trig if count = cmpa d
#define PWM_TRIG_CNT_BU     (0x00001000)  // trig if count = cmpa u
#define PWM_TRIG_CNT_BD     (0x00002000)  // trig if count = cmpa d

// divClock
//
#define PWM_CLK_DIV_1       (0x00000000)  // system clock
#define PWM_CLK_DIV_2       (0x00000001)  // system clock /2
#define PWM_CLK_DIV_4       (0x00000002)  // system clock /4
#define PWM_CLK_DIV_8       (0x00000003)  // system clock /8
#define PWM_CLK_DIV_16      (0x00000004)  // system clock /16
#define PWM_CLK_DIV_32      (0x00000005)  // system clock /32
#define PWM_CLK_DIV_64      (0x00000006)  // system clock /64

// pinBits
//
#define PWM_PIN_0   (0x00000001)  // bit-wise id for pin 0
#define PWM_PIN_1   (0x00000002)  // bit-wise id for pin 1
#define PWM_PIN_2   (0x00000004)  // bit-wise id for pin 2
#define PWM_PIN_3   (0x00000008)  // bit-wise id for pin 3
#define PWM_PIN_4   (0x00000010)  // bit-wise id for pin 4
#define PWM_PIN_5   (0x00000020)  // bit-wise id for pin 5
#define PWM_PIN_6   (0x00000040)  // bit-wise id for pin 6
#define PWM_PIN_7   (0x00000080)  // bit-wise id for pin 7

// genBits, generator blocks
//
#define PWM_GEN_0   (PWM_PIN_0 | PWM_PIN_1)  // bit-wise id for gen 0
#define PWM_GEN_1   (PWM_PIN_2 | PWM_PIN_3)  // bit-wise id for gen 1
#define PWM_GEN_2   (PWM_PIN_4 | PWM_PIN_5)  // bit-wise id for gen 2
#define PWM_GEN_3   (PWM_PIN_6 | PWM_PIN_7)  // bit-wise id for gen 3

// QEI, setup
//
typedef struct QEI_SETUP_T
{
    DWORD   config;     // [in ] configuration for the quadrature encoder
    DWORD   maxPos;     // [in ] maximum position value
    DWORD   velPeriod;  // [in ] number of clock ticks over which to measure the velocity, 0:disable
    DWORD   velPreDiv;  // [in ] predivider applied to the input quadrature signal before it is counted
}
QEI_SETUP, *QEI_SETUP_P;

// QEI_SETUP.config
//
#define QEI_CONFIG_CAPTURE_A    (0x00000000)  // count on ChA edges only
#define QEI_CONFIG_CAPTURE_A_B  (0x00000008)  // count on ChA and ChB edges
#define QEI_CONFIG_NO_RESET     (0x00000000)  // do not reset on index pulse
#define QEI_CONFIG_RESET_IDX    (0x00000010)  // reset position on index pulse
#define QEI_CONFIG_QUADRATURE   (0x00000000)  // ChA and ChB are quadrature
#define QEI_CONFIG_CLOCK_DIR    (0x00000004)  // ChA and ChB are clock and dir
#define QEI_CONFIG_NO_SWAP      (0x00000000)  // do not swap ChA and ChB
#define QEI_CONFIG_SWAP         (0x00000002)  // swap ChA and ChB

// QEI_SETUP.velPreDiv
//
#define QEI_VEL_DIV_1           (0x00000000)  // predivide by 1
#define QEI_VEL_DIV_2           (0x00000040)  // predivide by 2
#define QEI_VEL_DIV_4           (0x00000080)  // predivide by 4
#define QEI_VEL_DIV_8           (0x000000C0)  // predivide by 8
#define QEI_VEL_DIV_16          (0x00000100)  // predivide by 16
#define QEI_VEL_DIV_32          (0x00000140)  // predivide by 32
#define QEI_VEL_DIV_64          (0x00000180)  // predivide by 64
#define QEI_VEL_DIV_128         (0x000001C0)  // predivide by 128

// Deterministic Trigger Version 2 (DT2), setup
//
typedef struct DT2_SETUP_T
{
    BYTE    index;          // [in ] channel index
    BYTE    modeFlags;      // [in ] some extra-parameter combination for DT2 mode type (ex: DT2_INIT_HIGN)
    BYTE    modeType;       // [in ] operation mode type, ex: PTT, PTP, PPT, ...
    BYTE    trigMode;       // [in ] 0: never (disabled), 1: rising edge (above), 2: falling edge (below), 3:level, 4: always
    DWORD   trigData;       // [in ] trigger data, ex: adc data, qei position, ...
    DWORD   trigSrc;        // [in ] trigger source
    DWORD   trigTgt;        // [in ] trigger target
    DWORD   pulseDelay;     // [in ] output pulse(offset) delay tick count, range: 2~2147483647
    DWORD   pulseWidth;     // [in ] output pulse(offset) width tick count, range: 1~2147483647
    DWORD   pulseCount;     // [in ] output pulse(offset) count, range: 1~2147483647
    DWORD   pulseData;      // [in ] output pulse data, ex: led current, ...
}
DT2_SETUP, *DT2_SETUP_P;

// DT2_SETUP.modeFlags
//
#define DT2_INIT_HIGN       (0x01)  // high-initial & low-active
#define DT2_TRIG_BUFF       (0x02)  // buffer triggered (array index: 0~31)
#define DT2_TRIG_EX_0       (0x04)  // external input (default deactivated)
#define DT2_TRIG_EX_1       (0x08)  // external input (default activated)

// DT2_SETUP.modeType
//
#define DT2_MODE_PTT        (0x00)  // pulse output with time delay and time length
#define DT2_MODE_PTP        (0x01)  // pulse output with time delay and offset length
#define DT2_MODE_PPT        (0x02)  // pulse output with offset delay and time length
#define DT2_MODE_PPP        (0x03)  // pulse output with offset delay and offset length
#define DT2_MODE_LST        (0x04)  // led sustain with time delay
#define DT2_MODE_LPT        (0x05)  // led pulse with time delay
#define DT2_MODE_LTT        (0x06)  // led toggle with time delay
#define DT2_MODE_CTX        (0x07)  // comparison output with time length
#define DT2_MODE_CPX        (0x08)  // comparison output with offset length
#define DT2_MODE_MTP        (0x09)  // pwm output with time delay
#define DT2_MODE_MPP        (0x0a)  // pwm output with offset delay
#define DT2_MODE_STX        (0x0b)  // sustain output with time delay
#define DT2_MODE_SPX        (0x0c)  // sustain output with offset delay
#define DT2_MODE_TTX        (0x0d)  // toggle output with time delay
#define DT2_MODE_TPX        (0x0e)  // toggle output with offset delay

// DT2_SETUP.trigMode
//
#define DT2_TRIG_NEVER      (0x00)
#define DT2_TRIG_RISING     (0x01)  // above
#define DT2_TRIG_FALLING    (0x02)  // below
#define DT2_TRIG_LEVEL      (0x03)
#define DT2_TRIG_ALWAYS     (0x04)

// DT2_SETUP.trigSrc
//
#define DT2_SRC_DI_0        (0x00010001)
#define DT2_SRC_DI_1        (0x00010002)
#define DT2_SRC_DI_2        (0x00010004)
#define DT2_SRC_DI_3        (0x00010008)
#define DT2_SRC_DI_4        (0x00010010)  // reserved
#define DT2_SRC_DI_5        (0x00010020)  // reserved
#define DT2_SRC_DI_6        (0x00010040)  // reserved
#define DT2_SRC_DI_7        (0x00010080)  // reserved
//
#define DT2_SRC_DO_0        (0x00020001)
#define DT2_SRC_DO_1        (0x00020002)
#define DT2_SRC_DO_2        (0x00020004)
#define DT2_SRC_DO_3        (0x00020008)
#define DT2_SRC_DO_4        (0x00020010)
#define DT2_SRC_DO_5        (0x00020020)
#define DT2_SRC_DO_6        (0x00020040)
#define DT2_SRC_DO_7        (0x00020080)
//
#define DT2_SRC_ADC_0       (0x00040001)
#define DT2_SRC_ADC_1       (0x00040002)
#define DT2_SRC_ADC_2       (0x00040004)  // reserved
#define DT2_SRC_ADC_3       (0x00040008)  // reserved
//
#define DT2_SRC_QEI_0       (0x00080001)
#define DT2_SRC_QEI_1       (0x00080002)  // reserved

// DT2_SETUP.trigTgt
//
#define DT2_TGT_DO_0        (0x00020001)
#define DT2_TGT_DO_1        (0x00020002)
#define DT2_TGT_DO_2        (0x00020004)
#define DT2_TGT_DO_3        (0x00020008)
#define DT2_TGT_DO_4        (0x00020010)
#define DT2_TGT_DO_5        (0x00020020)
#define DT2_TGT_DO_6        (0x00020040)
#define DT2_TGT_DO_7        (0x00020080)
//
#define DT2_TGT_PWM_0       (0x00100001)
#define DT2_TGT_PWM_1       (0x00100002)
#define DT2_TGT_PWM_2       (0x00100004)
#define DT2_TGT_PWM_3       (0x00100008)
#define DT2_TGT_PWM_4       (0x00100010)
#define DT2_TGT_PWM_5       (0x00100020)
#define DT2_TGT_PWM_6       (0x00100040)  // reserved
#define DT2_TGT_PWM_7       (0x00100080)  // reserved
//
#define DT2_TGT_LED_0       (0x00200001)
#define DT2_TGT_LED_1       (0x00200002)  // reserved

#pragma pack(pop)


// Register WinIO driver for non-administrator operation
//
BOOL __cdecl RegisterWinIOService(void);  // obselete function

// Isolated Digital I/O
//
BOOL __cdecl InitDIO(void);
BOOL __cdecl DIReadLine(BYTE ch);
WORD __cdecl DIReadPort(void);
void __cdecl DOWriteLine(BYTE ch, BOOL value);
void __cdecl DOWritePort(WORD value);
void __cdecl DOWriteLineChecked(BYTE ch, BOOL value);
void __cdecl DOWritePortChecked(WORD value);

// Watch-dog Timer (WDT)
//
BOOL __cdecl InitWDT(void);
BOOL __cdecl StopWDT(void);
BOOL __cdecl StartWDT(void);
BOOL __cdecl SetWDT(WORD tick, BYTE unit);
BOOL __cdecl ResetWDT(void);

// PoE ports enable/disable
//
BYTE __cdecl GetStatusPoEPort(BYTE port);  // 0: disabled 1: enabled
BOOL __cdecl DisablePoEPort(BYTE port);
BOOL __cdecl EnablePoEPort(BYTE port);

// DI Change-of-State notification (DICOS)
//
BOOL __cdecl SetupDICOS(COS_INT_SETUP *lpSetup, DWORD cbSetup);
BOOL __cdecl StartDICOS(void);  // note: without DTIO, DTFO, and DT2
BOOL __cdecl StopDICOS(void);
BOOL __cdecl RegisterCallbackDICOS(COS_INT_CALLBACK callback);

// Deterministic Trigger I/O (DTIO)
//
BOOL __cdecl SetupDTIO(DTIO_SETUP *lpSetup, DWORD cbSetup);
BOOL __cdecl StartDTIO(void);  // note: without DICOS, DTFO, and DT2
BOOL __cdecl StopDTIO(void);
BOOL __cdecl SetUnitDTIO(WORD unit, int delta);  // unit: micro-second(25~2500 us), delta: fine-tuning(+/- 0.04us)
WORD __cdecl GetUnitDTIO(void);  // current DTIO timing unit in micro-second

// Deterministic Trigger Fan Out (DTFO)
//
BOOL __cdecl SetupDTFO(DTFO_SETUP *lpSetup, DWORD cbSetup);
BOOL __cdecl StartDTFO(void);  // note: without DICOS, DTIO, and DT2
BOOL __cdecl StopDTFO(void);
BOOL __cdecl SetUnitDTFO(WORD unit, int delta);  // unit: micro-second(25~2500 us), delta: fine-tuning(+/- 0.04us)
WORD __cdecl GetUnitDTFO(void);  // current DTFO timing unit in micro-second

// Light-Emitting Diode (LED)
//
BOOL __cdecl LED_SetCurrentDriving(DWORD mode, DWORD data);  // mode: 0)disabled, 1)cc, data:current(mA), 2)cv, data:duty cycle(0.01%)

// Micro-Controller Unit (MCU)
//
BOOL __cdecl MCU_GetVersion(DWORD *lpDateCode);  // 0xYYYYMMDD e.g. 0x20150325

// Ignition Control (IGN)
//
BOOL __cdecl IGN_GetState(DWORD *lpState);  // 0: off, 1: on
BOOL __cdecl IGN_GetBatteryVoltage(double *lpVoltage);
BOOL __cdecl IGN_GetSetting(IGN_SETTING *lpSetting, DWORD cbSetting);

// Controller Area Network (CAN)
//
BOOL __cdecl CAN_RegisterReceived(DWORD idx, void (__stdcall *pfnHandler)(CAN_MSG *lpMsg, DWORD cbMsg));
BOOL __cdecl CAN_RegisterStatus(DWORD idx, void (__stdcall *pfnHandler)(DWORD status));
BOOL __cdecl CAN_Setup(DWORD idx, CAN_SETUP *lpSetup, DWORD cbSetup);
BOOL __cdecl CAN_Start(DWORD idx);
BOOL __cdecl CAN_Stop(DWORD idx);
BOOL __cdecl CAN_Send(DWORD idx, CAN_MSG *lpMsg, DWORD cbMsg);
//BOOL __cdecl CAN_BitTimingGet(DWORD idx, CAN_BITCLK *lpClkParms, DWORD cbClkParms);
//BOOL __cdecl CAN_BitTimingSet(DWORD idx, CAN_BITCLK *lpClkParms, DWORD cbClkParms);

// Pulse Width Modulator (PWM)
//
BOOL __cdecl PWM_ClockSet(DWORD idx, DWORD divClock);
BOOL __cdecl PWM_RegisterStatus(void (__stdcall *pfnHandler)(DWORD genBits, DWORD status));
BOOL __cdecl PWM_GenSetup(DWORD genBits, PWM_GEN_SETUP *lpSetup, DWORD cbSetup);
BOOL __cdecl PWM_GenPeriod(DWORD genBits, DWORD period);
BOOL __cdecl PWM_PulseWidth(DWORD pinBits, DWORD width);
BOOL __cdecl PWM_PulseInvert(DWORD pinBits);
BOOL __cdecl PWM_Start(DWORD pinBits);
BOOL __cdecl PWM_Stop(DWORD pinBits);
BOOL __cdecl PWM_SyncTimeBase(DWORD genBits);
BOOL __cdecl PWM_SyncUpdate(DWORD genBits);

// Analog to Digital Converter (ADC)
//
BOOL __cdecl ADC_Stop(DWORD idx);
BOOL __cdecl ADC_Start(DWORD idx);
BOOL __cdecl ADC_GetData(DWORD idx, double *lpData);

// Quadrature Encoder (QEI)
//
BOOL __cdecl QEI_Stop(DWORD idx);
BOOL __cdecl QEI_Start(DWORD idx);
BOOL __cdecl QEI_Setup(DWORD idx, QEI_SETUP *lpSetup, DWORD cbSetup);
BOOL __cdecl QEI_GetDirection(DWORD idx, DWORD *lpDirection);
BOOL __cdecl QEI_GetVelocity(DWORD idx, DWORD *lpVelocity);
BOOL __cdecl QEI_GetPosition(DWORD idx, DWORD *lpPosition);
BOOL __cdecl QEI_SetPosition(DWORD idx, DWORD position);

// PoE ports on the PCI card (PCI)
//
BYTE __cdecl PCI_GetStatusPoEPort(DWORD boardId, DWORD port);  // 0: disabled, 1: enabled
BOOL __cdecl PCI_DisablePoEPort(DWORD boardId, DWORD port);
BOOL __cdecl PCI_EnablePoEPort(DWORD boardId, DWORD port);

// Deterministic Trigger Version 2 (DT2)
//
BOOL __cdecl DT2_Setup(DT2_SETUP *lpSetup, DWORD cbSetup);
BOOL __cdecl DT2_Start(void);  // note: without DICOS, DTIO, and DTFO
BOOL __cdecl DT2_Stop(void);
BOOL __cdecl DT2_SetUnit(DWORD unit, int delta);  // unit: micro-second(25~2500 us), delta: fine-tuning(+/- 0.0125us)
WORD __cdecl DT2_GetUnit(void);  // unit in micro-second
BOOL __cdecl DT2_GetExIndex(WORD* lpIndex);
BOOL __cdecl DT2_SetExResult(WORD index, WORD value);
BOOL __cdecl DT2_SetExOthers(WORD value);  // software triggered


#if defined(__GNUC__) && __GNUC__ >= 4
#pragma  GCC visibility  pop
#endif

#ifdef  __cplusplus
}
#endif

#endif
