/********************************************************************
 FileName:		MultiTouch_MultiModes.c
 Dependencies:	See INCLUDES section
 Processor:     PIC18, PIC24, dsPIC, and PIC32 USB Microcontrollers
 Hardware:      This demo is natively intended to be used on Microchip USB demo
                boards supported by the MCHPFSUSB stack.  See release notes for
                support matrix.  This demo can be modified for use on other 
                hardware platforms.
 Complier:      Microchip C18 (for PIC18), XC16 (for PIC24/dsPIC), XC32 (for PIC32)
 Company:       Microchip Technology, Inc.

 Software License Agreement:

 The software supplied herewith by Microchip Technology Incorporated
 (the "Company") for its PIC(R) Microcontroller is intended and
 supplied to you, the Company's customer, for use solely and
 exclusively on Microchip PIC Microcontroller products. The
 software is owned by the Company and/or its supplier, and is
 protected under applicable copyright laws. All rights are reserved.
 Any use in violation of the foregoing restrictions may subject the
 user to criminal sanctions under applicable laws, as well as to
 civil liability for the breach of the terms and conditions of this
 license.

 THIS SOFTWARE IS PROVIDED IN AN "AS IS" CONDITION. NO WARRANTIES,
 WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT NOT LIMITED
 TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 PARTICULAR PURPOSE APPLY TO THIS SOFTWARE. THE COMPANY SHALL NOT,
 IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL OR
 CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.

/********************************************************************
This demo is configured by default to support up to two simultaneous contacts,
by using "parallel reporting" (all data for each contact is contained in each
HID report packet sent to the host).  The demo can be modified to support additional
contacts if the report descriptor in usb_config.h is modified, and the appropriate
contact data is sent to the host for each HID report.
********************************************************************/

#ifndef USBMULTITOUCH_MULTI_MODES_C
#define USBMULTITOUCH_MULTI_MODES_C

/** INCLUDES *******************************************************/
#include "./USB/usb.h"
#include "HardwareProfile.h"
#include "./USB/usb_function_hid.h"

/** CONFIGURATION **************************************************/

//	PIC18F14K50
#pragma config CPUDIV = NOCLKDIV
#pragma config USBDIV = OFF
#pragma config FOSC   = HS
#pragma config PLLEN  = ON
#pragma config FCMEN  = OFF
#pragma config IESO   = OFF
#pragma config PWRTEN = OFF
#pragma config BOREN  = ON
#pragma config BORV   = 30
#pragma config WDTEN  = OFF
#pragma config WDTPS  = 32768
#pragma config MCLRE  = OFF
#pragma config HFOFST = OFF
#pragma config STVREN = ON
#pragma config LVP    = OFF
#pragma config XINST  = OFF
#pragma config BBSIZ  = OFF
#pragma config CP0    = OFF
#pragma config CP1    = OFF
#pragma config CPB    = OFF
#pragma config WRT0   = OFF
#pragma config WRT1   = OFF
#pragma config WRTB   = OFF
#pragma config WRTC   = OFF
#pragma config EBTR0  = OFF
#pragma config EBTR1  = OFF
#pragma config EBTRB  = OFF        

/** VARIABLES ******************************************************/
#pragma udata HID_VARS=0x260

#define IN_DATA_BUFFER_ADDRESS_TAG
#define OUT_DATA_BUFFER_ADDRESS_TAG

unsigned char hid_report_in[HID_INT_IN_EP_SIZE] IN_DATA_BUFFER_ADDRESS_TAG;
unsigned char hid_report_out[HID_INT_OUT_EP_SIZE] OUT_DATA_BUFFER_ADDRESS_TAG;

#if defined(__18CXX)
    #pragma udata
#endif

BYTE UIESave;
BYTE ReportsSent;
WORD SOFCount;			//Can get updated in interrupt context if using USB interrupts instead of polling
WORD SOFCountIntSafe;	//Shadow holding variable for the SOFCounter.  This number will not get updated in the interrupt context.
WORD SOFCountIntSafeOld;
USB_HANDLE lastTransmission = 0;

BOOL HIDApplicationModeChanging;
BYTE DeviceIdentifier;
BYTE DeviceMode;
//DeviceMode variable values.  See also the usb_config.h "DEFAULT_DEVICE_MODE" definition.
#define MULTI_TOUCH_DIGITIZER_MODE	0x02

#if !defined(pBDTEntryIn)
	extern volatile BDT_ENTRY *pBDTEntryIn[USB_MAX_EP_NUMBER+1];
#endif


/** PRIVATE PROTOTYPES *********************************************/
static void InitializeSystem(void);
void UserInit(void);
void YourHighPriorityISRCode();
void YourLowPriorityISRCode();
void USBCBSendResume(void);
void TouchEnable(void);
void TouchInterrupt(void);

#define LED1On() LATBbits.LATB7 = 1
#define LED1Off() LATBbits.LATB7 = 0
#define LED2On() LATCbits.LATC6 = 1
#define LED2Off() LATCbits.LATC6 = 0
#define LED3On() LATCbits.LATC3 = 1
#define LED3Off() LATCbits.LATC3 = 0

/** VECTOR REMAPPING ***********************************************/
#if defined(__18CXX)
	//On PIC18 devices, addresses 0x00, 0x08, and 0x18 are used for
	//the reset, high priority interrupt, and low priority interrupt
	//vectors. However, if using the HID Bootloader, the bootloader
	//firmware will occupy the 0x00-0xFFF region.  Therefore,
	//the bootloader code remaps these vectors to new locations:
	//0x1000, 0x1008, 0x1018 respectively.  This remapping is only necessary 
	//if you wish to program the hex file generated from this project with
	//the USB bootloader. 
	#define REMAPPED_RESET_VECTOR_ADDRESS		0x1000
	#define REMAPPED_HIGH_INTERRUPT_VECTOR_ADDRESS	0x1008
	#define REMAPPED_LOW_INTERRUPT_VECTOR_ADDRESS	0x1018
	#define APP_VERSION_ADDRESS                     0x1016 //Fixed location, so the App FW image version can be read by the bootloader.
	#define APP_SIGNATURE_ADDRESS                   0x1006 //Signature location that must be kept at blaknk value (0xFFFF) in this project (has special purpose for bootloader).

    //--------------------------------------------------------------------------
    //Application firmware image version values, as reported to the bootloader
    //firmware.  These are useful so the bootloader can potentially know if the
    //user is trying to program an older firmware image onto a device that
    //has already been programmed with a with a newer firmware image.
    //Format is APP_FIRMWARE_VERSION_MAJOR.APP_FIRMWARE_VERSION_MINOR.
    //The valid minor version is from 00 to 99.  Example:
    //if APP_FIRMWARE_VERSION_MAJOR == 1, APP_FIRMWARE_VERSION_MINOR == 1,
    //then the version is "1.01"
    #define APP_FIRMWARE_VERSION_MAJOR  0   //valid values 0-255
    #define APP_FIRMWARE_VERSION_MINOR  2   //valid values 0-99
    //--------------------------------------------------------------------------
	
	#pragma romdata AppVersionAndSignatureSection = APP_VERSION_ADDRESS
	ROM unsigned char AppVersion[2] = {APP_FIRMWARE_VERSION_MINOR, APP_FIRMWARE_VERSION_MAJOR};
	#pragma romdata AppSignatureSection = APP_SIGNATURE_ADDRESS
	ROM unsigned short int SignaturePlaceholder = 0xFFFF;
	extern void _startup (void);        // See c018i.c in your C18 compiler dir
	#pragma code REMAPPED_RESET_VECTOR = REMAPPED_RESET_VECTOR_ADDRESS
	void _reset (void)
	{
	    _asm goto _startup _endasm
	}


	//--------------------------------------------------------------------
	//NOTE: See PIC18InterruptVectors.asm for important code and details
	//relating to the interrupt vectors and interrupt vector remapping.
	//Special considerations apply if clock switching to the 31kHz INTRC 
	//during USB suspend.
	//--------------------------------------------------------------------	
#pragma code
BOOL send_state = 0;

	//These are your actual interrupt handling routines.
	#pragma interrupt YourHighPriorityISRCode
	void YourHighPriorityISRCode()
	{
            // USB device interrupt
            LED1On();
            #if defined(USB_INTERRUPT)
                USBDeviceTasks();
            #endif
            //Check which interrupt flag caused the interrupt.
            //Service the interrupt
            //Clear the interrupt flag
            //Etc.
            LED1Off();
	
	}	//This return will be a "retfie fast", since this is in a #pragma interrupt section 
	#pragma interruptlow YourLowPriorityISRCode
	void YourLowPriorityISRCode()
	{
            // Touch panel interrupt
            TouchInterrupt();
	}	//This return will be a "retfie", since this is in a #pragma interruptlow section 


/** DECLARATIONS ***************************************************/
#if defined(__18CXX)
    #pragma code
#endif


#include "touch_comm.h"
#include "i2c.h"

/********************************************************************
 * Function:        void main(void)
 *
 * PreCondition:    None
 *
 * Input:           None
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        Main program entry point.
 *
 * Note:            None
 *******************************************************************/
void main(void)
{
    InitializeSystem();

    USBDeviceAttach();

    TouchEnable();

    while(1)
    {
        if (send_state) {
            send_state = 0;
            touch_send();
        }
	// Code happens in interrupts
    }//end while
}//end main

void InitLEDs(void) {
    LATCbits.LATC3 = 0; TRISCbits.RC3 = 0;
    LATCbits.LATC6 = 0; TRISCbits.RC6 = 0;
    LATBbits.LATB7 = 0; TRISBbits.RB7 = 0;
}

/********************************************************************
 * Function:        static void InitializeSystem(void)
 *
 * PreCondition:    None
 *
 * Input:           None
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        InitializeSystem is a centralize initialization
 *                  routine. All required USB initialization routines
 *                  are called from here.
 *
 *                  User application initialization routine should
 *                  also be called from here.                  
 *
 * Note:            None
 *******************************************************************/
static void InitializeSystem(void)
{
    ADCON1 |= 0x0F;                 // Default all pins to digital
    InitLEDs();

    UserInit();

    USBDeviceInit();	//usb_device.c.  Initializes USB module SFRs and firmware
    					//variables to known states.

}//end InitializeSystem



/******************************************************************************
 * Function:        void UserInit(void)
 *
 * PreCondition:    None
 *
 * Input:           None
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        This routine should take care of all of the demo code
 *                  initialization that is required.
 *
 * Note:            
 *
 *****************************************************************************/
void UserInit(void)
{
    //initialize the variable holding the handle for the last
    // transmission
    lastTransmission = 0;

    HIDApplicationModeChanging = FALSE;

    //Initialize device mode and digitizer emulation variables.
    //--------------------------------------------------------
    DeviceIdentifier = 0x01;
    DeviceMode = MULTI_TOUCH_DIGITIZER_MODE;	//Set the device mode (ex: mouse, single-touch digitizer, multi-touch digitizer) to a default value.  Set the default in the usb_config.h file.

}//end UserInit

void TouchEnable(void) {
    // Enable interrupt
    RCONbits.IPEN = 1;      // Enable interrupt priority feature
    INTCONbits.GIE = 1;     // Global interrupt enable
    INTCONbits.PEIE = 1;    // Peripheral interrupt ennable

    TRISCbits.RC1 = 1;      // Set interrupt 1 pin as input
    ANSELbits.ANS5 = 0;     // Disable A/D converter on RC1
    INTCON2bits.INTEDG1 = 0;// Interrupt 1 trigger on falling edge
    INTCON3bits.INT1IP = 0;  // Interrupt 1 low priority
    INTCON3bits.INT1IF = 0;  // Clear int1 flag
    INTCON3bits.INT1IE = 1;  // Enable int1

    i2c_Init();
}

void TouchInterrupt(void) {
    unsigned int tp = 0;

    if (!INTCON3bits.INT1IF) return;
    INTCON3bits.INT1IE = 0; // Disable interrupt
    INTCON3bits.INT1IF = 0; // Reset flag
    tp = touch_points();

    if (tp > 0) {
        LED2On();
    }
    if (tp > 1) {
        LED3On();
    }

    touch_read();
    send_state = 1;

    INTCON3bits.INT1IE = 1; // Enable interrupt
}

// ******************************************************************************************************
// ************** USB Callback Functions ****************************************************************
// ******************************************************************************************************
// The USB firmware stack will call the callback functions USBCBxxx() in response to certain USB related
// events.  For example, if the host PC is powering down, it will stop sending out Start of Frame (SOF)
// packets to your device.  In response to this, all USB devices are supposed to decrease their power
// consumption from the USB Vbus to <2.5mA* each.  The USB module detects this condition (which according
// to the USB specifications is 3+ms of no bus activity/SOF packets) and then calls the USBCBSuspend()
// function.  You should modify these callback functions to take appropriate actions for each of these
// conditions.  For example, in the USBCBSuspend(), you may wish to add code that will decrease power
// consumption from Vbus to <2.5mA (such as by clock switching, turning off LEDs, putting the
// microcontroller to sleep, etc.).  Then, in the USBCBWakeFromSuspend() function, you may then wish to
// add code that undoes the power saving things done in the USBCBSuspend() function.

// The USBCBSendResume() function is special, in that the USB stack will not automatically call this
// function.  This function is meant to be called from the application firmware instead.  See the
// additional comments near the function.

// Note *: The "usb_20.pdf" specs indicate 500uA or 2.5mA, depending upon device classification. However,
// the USB-IF has officially issued an ECN (engineering change notice) changing this to 2.5mA for all 
// devices.  Make sure to re-download the latest specifications to get all of the newest ECNs.

/******************************************************************************
 * Function:        void USBCBSuspend(void)
 *
 * PreCondition:    None
 *
 * Input:           None
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        Call back that is invoked when a USB suspend is detected
 *
 * Note:            None
 *****************************************************************************/
void USBCBSuspend(void)
{
	//Example power saving code.  Insert appropriate code here for the desired
	//application behavior.  If the microcontroller will be put to sleep, a
	//process similar to that shown below may be used:
	
	
	//ConfigureIOPinsForLowPower();
	//SaveStateOfAllInterruptEnableBits();
	//DisableAllInterruptEnableBits();
	//EnableOnlyTheInterruptsWhichWillBeUsedToWakeTheMicro();	//should enable at least USBActivityIF as a wake source
	//Sleep();
	//RestoreStateOfAllPreviouslySavedInterruptEnableBits();	//Preferrably, this should be done in the USBCBWakeFromSuspend() function instead.
	//RestoreIOPinsToNormal();									//Preferrably, this should be done in the USBCBWakeFromSuspend() function instead.

	//Alternatively, the microcontorller may use clock switching to reduce current consumption.
	#if defined(__18CXX)
	//Save state of enabled interrupt enable bits.
	UIESave = UIE;
	
	//Disable all interrupt enable bits (except UIR, ACTVIF, and UIR, URSTIF for receiving the USB resume signalling)
	UIE = 0x05;		//ACTVIF and URSTIF still enabled.  All others disabled.
	
	//Configure device for low power consumption
	#if defined(OSCTUNE)
		OSCTUNEbits.INTSRC = 0;		//31kHz from INTRC, less accurate but lower power
	#endif
	
	#if !defined(PIC18F97J94_FAMILY)
	    OSCCON = 0x03;		//Sleep on sleep, 31kHz selected as microcontroller clock source
	#else
	    OSCCON = 0x06;      //Sleep on sleep, 500kHz from FRC selected
	#endif
	
	//Should configure all I/O pins for lowest power consumption.
	//Typically this is done by driving unused I/O pins as outputs and driving them high or low.
	//In this example, this is not done however, in case the user is expecting the I/O pins
	//to remain tri-state and has hooked something up to them.
	//Leaving the I/O pins floating will waste power and should not be done in a
	//real application.
	#endif

	//IMPORTANT NOTE: If the microcontroller goes to sleep in the above code, do not
	//clear the USBActivityIF (ACTVIF) bit here (after waking up/resuming from sleep).  This bit is 
	//cleared inside the usb_device.c file.  Clearing USBActivityIF here will cause 
	//things to not work as intended.	




}


/******************************************************************************
 * Function:        void USBCBWakeFromSuspend(void)
 *
 * PreCondition:    None
 *
 * Input:           None
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        The host may put USB peripheral devices in low power
 *					suspend mode (by "sending" 3+ms of idle).  Once in suspend
 *					mode, the host may wake the device back up by sending non-
 *					idle state signalling.
 *					
 *					This call back is invoked when a wakeup from USB suspend 
 *					is detected.
 *
 * Note:            None
 *****************************************************************************/
void USBCBWakeFromSuspend(void)
{
	// If clock switching or other power savings measures were taken when
	// executing the USBCBSuspend() function, now would be a good time to
	// switch back to normal full power run mode conditions.  The host allows
	// 10+ milliseconds of wakeup time, after which the device must be 
	// fully back to normal, and capable of receiving and processing USB
	// packets.  In order to do this, the USB module must receive proper
	// clocking (IE: 48MHz clock must be available to SIE for full speed USB
	// operation).  
	// Make sure the selected oscillator settings are consistant with USB operation 
	// before returning from this function.

	//Switch clock back to main clock source necessary for USB operation
	//Previous clock source was something low frequency as set in the 
	//USBCBSuspend() function.
	#if defined(__18CXX)
        OSCCON = 0x60;		//Primary clock source selected.
        //Adding a software start up delay will ensure
        //that the primary oscillator and PLL are running before executing any other
        //code.  If the PLL isn't being used, (ex: primary osc = 48MHz externally applied EC)
        //then this code adds a small unnecessary delay, but it is harmless to execute anyway.
        {
            unsigned int pll_startup_counter = 800;	//Long delay at 31kHz, but ~0.8ms at 48MHz
            while(pll_startup_counter--);		//Clock will switch over while executing this delay loop
        }
        //Now restore the interrupt enable bits that we previously disabled in the USBCBSuspend() function,
        //when we first entered USB suspend state.
        UIE = UIESave;
	#endif

}

/********************************************************************
 * Function:        void USBCB_SOF_Handler(void)
 *
 * PreCondition:    None
 *
 * Input:           None
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        The USB host sends out a SOF packet to full-speed
 *                  devices every 1 ms. This interrupt may be useful
 *                  for isochronous pipes. End designers should
 *                  implement callback routine as necessary.
 *
 * Note:            None
 *******************************************************************/
void USBCB_SOF_Handler(void)
{
    // No need to clear UIRbits.SOFIF to 0 here.
    // Callback caller is already doing that.

	//Using SOF packets (which arrive at 1ms intervals) for time
	//keeping purposes in this demo.  This enables this demo to send constant
	//velocity contact point X/Y coordinate movement.
    SOFCount++;	
}

/*******************************************************************
 * Function:        void USBCBErrorHandler(void)
 *
 * PreCondition:    None
 *
 * Input:           None
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        The purpose of this callback is mainly for
 *                  debugging during development. Check UEIR to see
 *                  which error causes the interrupt.
 *
 * Note:            None
 *******************************************************************/
void USBCBErrorHandler(void)
{
    // No need to clear UEIR to 0 here.
    // Callback caller is already doing that.

	// Typically, user firmware does not need to do anything special
	// if a USB error occurs.  For example, if the host sends an OUT
	// packet to your device, but the packet gets corrupted (ex:
	// because of a bad connection, or the user unplugs the
	// USB cable during the transmission) this will typically set
	// one or more USB error interrupt flags.  Nothing specific
	// needs to be done however, since the SIE will automatically
	// send a "NAK" packet to the host.  In response to this, the
	// host will normally retry to send the packet again, and no
	// data loss occurs.  The system will typically recover
	// automatically, without the need for application firmware
	// intervention.
	
	// Nevertheless, this callback function is provided, such as
	// for debugging purposes.
}


/*******************************************************************
 * Function:        void USBCBCheckOtherReq(void)
 *
 * PreCondition:    None
 *
 * Input:           None
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        When SETUP packets arrive from the host, some
 * 					firmware must process the request and respond
 *					appropriately to fulfill the request.  Some of
 *					the SETUP packets will be for standard
 *					USB "chapter 9" (as in, fulfilling chapter 9 of
 *					the official USB specifications) requests, while
 *					others may be specific to the USB device class
 *					that is being implemented.  For example, a HID
 *					class device needs to be able to respond to
 *					"GET REPORT" type of requests.  This
 *					is not a standard USB chapter 9 request, and 
 *					therefore not handled by usb_device.c.  Instead
 *					this request should be handled by class specific 
 *					firmware, such as that contained in usb_function_hid.c.
 *
 * Note:            None
 *******************************************************************/
void USBCBCheckOtherReq(void)
{
    USBCheckHIDRequest();
}//end


/*******************************************************************
 * Function:        void USBCBStdSetDscHandler(void)
 *
 * PreCondition:    None
 *
 * Input:           None
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        The USBCBStdSetDscHandler() callback function is
 *					called when a SETUP, bRequest: SET_DESCRIPTOR request
 *					arrives.  Typically SET_DESCRIPTOR requests are
 *					not used in most applications, and it is
 *					optional to support this type of request.
 *
 * Note:            None
 *******************************************************************/
void USBCBStdSetDscHandler(void)
{
    // Must claim session ownership if supporting this request
}//end


/*******************************************************************
 * Function:        void USBCBInitEP(void)
 *
 * PreCondition:    None
 *
 * Input:           None
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        This function is called when the device becomes
 *                  initialized, which occurs after the host sends a
 * 					SET_CONFIGURATION (wValue not = 0) request.  This 
 *					callback function should initialize the endpoints 
 *					for the device's usage according to the current 
 *					configuration.
 *
 * Note:            None
 *******************************************************************/
void USBCBInitEP(void)
{
    //enable the HID endpoint
    USBEnableEndpoint(HID_EP,USB_IN_ENABLED|USB_HANDSHAKE_ENABLED|USB_DISALLOW_SETUP);
}

/********************************************************************
 * Function:        void USBCBSendResume(void)
 *
 * PreCondition:    None
 *
 * Input:           None
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        The USB specifications allow some types of USB
 * 					peripheral devices to wake up a host PC (such
 *					as if it is in a low power suspend to RAM state).
 *					This can be a very useful feature in some
 *					USB applications, such as an Infrared remote
 *					control	receiver.  If a user presses the "power"
 *					button on a remote control, it is nice that the
 *					IR receiver can detect this signalling, and then
 *					send a USB "command" to the PC to wake up.
 *					
 *					The USBCBSendResume() "callback" function is used
 *					to send this special USB signalling which wakes 
 *					up the PC.  This function may be called by
 *					application firmware to wake up the PC.  This
 *					function will only be able to wake up the host if
 *                  all of the below are true:
 *					
 *					1.  The USB driver used on the host PC supports
 *						the remote wakeup capability.
 *					2.  The USB configuration descriptor indicates
 *						the device is remote wakeup capable in the
 *						bmAttributes field.
 *					3.  The USB host PC is currently sleeping,
 *						and has previously sent your device a SET 
 *						FEATURE setup packet which "armed" the
 *						remote wakeup capability.   
 *
 *                  If the host has not armed the device to perform remote wakeup,
 *                  then this function will return without actually performing a
 *                  remote wakeup sequence.  This is the required behavior, 
 *                  as a USB device that has not been armed to perform remote 
 *                  wakeup must not drive remote wakeup signalling onto the bus;
 *                  doing so will cause USB compliance testing failure.
 *                  
 *					This callback should send a RESUME signal that
 *                  has the period of 1-15ms.
 *
 * Note:            This function does nothing and returns quickly, if the USB
 *                  bus and host are not in a suspended condition, or are 
 *                  otherwise not in a remote wakeup ready state.  Therefore, it
 *                  is safe to optionally call this function regularly, ex: 
 *                  anytime application stimulus occurs, as the function will
 *                  have no effect, until the bus really is in a state ready
 *                  to accept remote wakeup. 
 *
 *                  When this function executes, it may perform clock switching,
 *                  depending upon the application specific code in 
 *                  USBCBWakeFromSuspend().  This is needed, since the USB
 *                  bus will no longer be suspended by the time this function
 *                  returns.  Therefore, the USB module will need to be ready
 *                  to receive traffic from the host.
 *
 *                  The modifiable section in this routine may be changed
 *                  to meet the application needs. Current implementation
 *                  temporary blocks other functions from executing for a
 *                  period of ~3-15 ms depending on the core frequency.
 *
 *                  According to USB 2.0 specification section 7.1.7.7,
 *                  "The remote wakeup device must hold the resume signaling
 *                  for at least 1 ms but for no more than 15 ms."
 *                  The idea here is to use a delay counter loop, using a
 *                  common value that would work over a wide range of core
 *                  frequencies.
 *                  That value selected is 1800. See table below:
 *                  ==========================================================
 *                  Core Freq(MHz)      MIP         RESUME Signal Period (ms)
 *                  ==========================================================
 *                      48              12          1.05
 *                       4              1           12.6
 *                  ==========================================================
 *                  * These timing could be incorrect when using code
 *                    optimization or extended instruction mode,
 *                    or when having other interrupts enabled.
 *                    Make sure to verify using the MPLAB SIM's Stopwatch
 *                    and verify the actual signal on an oscilloscope.
 *******************************************************************/
void USBCBSendResume(void)
{
    static WORD delay_count;
    
    //First verify that the host has armed us to perform remote wakeup.
    //It does this by sending a SET_FEATURE request to enable remote wakeup,
    //usually just before the host goes to standby mode (note: it will only
    //send this SET_FEATURE request if the configuration descriptor declares
    //the device as remote wakeup capable, AND, if the feature is enabled
    //on the host (ex: on Windows based hosts, in the device manager 
    //properties page for the USB device, power management tab, the 
    //"Allow this device to bring the computer out of standby." checkbox 
    //should be checked).
    if(USBGetRemoteWakeupStatus() == TRUE) 
    {
        //Verify that the USB bus is in fact suspended, before we send
        //remote wakeup signalling.
        if(USBIsBusSuspended() == TRUE)
        {
            USBMaskInterrupts();
            
            //Clock switch to settings consistent with normal USB operation.
            USBCBWakeFromSuspend();
            USBSuspendControl = 0; 
            USBBusIsSuspended = FALSE;  //So we don't execute this code again, 
                                        //until a new suspend condition is detected.

            //Section 7.1.7.7 of the USB 2.0 specifications indicates a USB
            //device must continuously see 5ms+ of idle on the bus, before it sends
            //remote wakeup signalling.  One way to be certain that this parameter
            //gets met, is to add a 2ms+ blocking delay here (2ms plus at 
            //least 3ms from bus idle to USBIsBusSuspended() == TRUE, yeilds
            //5ms+ total delay since start of idle).
            delay_count = 3600U;        
            do
            {
                delay_count--;
            }while(delay_count);
            
            //Now drive the resume K-state signalling onto the USB bus.
            USBResumeControl = 1;       // Start RESUME signaling
            delay_count = 1800U;        // Set RESUME line for 1-13 ms
            do
            {
                delay_count--;
            }while(delay_count);
            USBResumeControl = 0;       //Finished driving resume signalling

            USBUnmaskInterrupts();
        }
    }
}


/*******************************************************************
 * Function:        BOOL USER_USB_CALLBACK_EVENT_HANDLER(
 *                        int event, void *pdata, WORD size)
 *
 * PreCondition:    None
 *
 * Input:           int event - the type of event
 *                  void *pdata - pointer to the event data
 *                  WORD size - size of the event data
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        This function is called from the USB stack to
 *                  notify a user application that a USB event
 *                  occured.  This callback is in interrupt context
 *                  when the USB_INTERRUPT option is selected.
 *
 * Note:            None
 *******************************************************************/
BOOL USER_USB_CALLBACK_EVENT_HANDLER(int event, void *pdata, WORD size)
{
    switch( event )
    {
        case EVENT_TRANSFER:
            //Add application specific callback task or callback function here if desired.
            break;
        case EVENT_SOF:
            USBCB_SOF_Handler();
            break;
        case EVENT_SUSPEND:
            USBCBSuspend();
            break;
        case EVENT_RESUME:
            USBCBWakeFromSuspend();
            break;
        case EVENT_CONFIGURED: 
            USBCBInitEP();
            break;
        case EVENT_SET_DESCRIPTOR:
            USBCBStdSetDscHandler();
            break;
        case EVENT_EP0_REQUEST:
            USBCBCheckOtherReq();
            break;
        case EVENT_BUS_ERROR:
            USBCBErrorHandler();
            break;
        case EVENT_TRANSFER_TERMINATED:
            //Add application specific callback task or callback function here if desired.
            //The EVENT_TRANSFER_TERMINATED event occurs when the host performs a CLEAR
            //FEATURE (endpoint halt) request on an application endpoint which was 
            //previously armed (UOWN was = 1).  Here would be a good place to:
            //1.  Determine which endpoint the transaction that just got terminated was 
            //      on, by checking the handle value in the *pdata.
            //2.  Re-arm the endpoint if desired (typically would be the case for OUT 
            //      endpoints).
            break;
        default:
            break;
    }      
    return TRUE; 
}

// *****************************************************************************
// ************** USB Class Specific Callback Function(s) **********************
// *****************************************************************************

/********************************************************************
 * Function:        void MultiGetTouchReportHandler(void)
 *
 * PreCondition:    None
 *
 * Input:           None
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        MultiTouchGetReportHandler() is used to respond to
 *					the HID device class specific GET_REPORT control
 *					transfer request (starts with SETUP packet on EP0 OUT).  
 *                  This get report handler callback function is placed
 *					here instead of in usb_function_hid.c, since this code
 *					is "custom" and needs to be specific to this particular 
 *					application example.  For this HID digitizer device,
 *					we need to be able to correctly respond to 
 *					GET_REPORT(feature)	requests.  The proper response to this
 *					type of request depends on the HID report descriptor (in 
 *					usb_descriptors.c).
 * Note:            
 *******************************************************************/
void MultiTouchGetReportHandler(void)
{
    static BYTE FeatureReport[2];
    BYTE bytesToSend;

	//Check if request was for a feature report with report ID = 0x02.
	//wValue MSB = 0x03 is Feature report (see HID1_11.pdf specifications),
	//LSB = 0x02 is Report ID "0x02".  Based on the report descriptor, a feature
	//report for this example project has Report ID = 0x02.
	if(SetupPkt.wValue == 0x0302)
	{
    	//Prepare a packet to send to the host
		FeatureReport[1] = 0x02;
		FeatureReport[0] = 0x02;

		//Determine the number of bytes to send to the host
		if(SetupPkt.wLength < 2u)
		{
			bytesToSend = SetupPkt.wLength;
		}
		else
		{
			//Size of the feature report.  Byte 0 is Report ID = 0x02, Byte 1 is
			//the maximum contacts value, which is "2" for this demo
			bytesToSend = 2;
		}

		//Now send the packet to the host, via the control transfer on EP0
		USBEP0SendRAMPtr((BYTE*)&FeatureReport[0], bytesToSend, USB_EP0_RAM);
	}	
}


/******************************************************************************
 * Function:     	void touch_send()
 *
 * Side Effects: 	The ownership of the USB buffers will change according
 *               	to the required operation
 *
 *	//Byte[0] = Report ID == use literal value "0x01" when sending contact reports.
 *
 * 	//First contact point info in bytes 1-6.
 *	//Byte[1] = Bits7-2: pad bits (unused), Bit1:In Range, Bit0:Tip Switch
 *	//Byte[2] = Contact identifier number (assigned by firmware, see report descriptor)
 *	//Byte[3] = X-coordinate LSB
 *	//Byte[4] = X-coordinate MSB
 *	//Byte[5] = Y-coordinate LSB
 *	//Byte[6] = Y-coordinate MSB
 *
 *	//Second contact point info in bytes 7-12
 *	//Byte[7] = Bits7-2: pad bits (unused), Bit1:In Range, Bit0:Tip Switch
 *	//Byte[8] = Contact identifier number (assigned by firmware, see report descriptor)
 *	//Byte[9] = X-coordinate LSB
 *	//Byte[10]= X-coordinate MSB
 *	//Byte[11]= Y-coordinate LSB
 *	//Byte[12]= Y-coordinate MSB
 *
 *	//Contacts valid
 *	//Byte[13]= 8-bit number indicating how many of the above contact points are valid.
 *				If only the first contact is valid, report "1" here.  If both are
 *				valid, report "2". */
void touch_send(void) {

	//Make sure the endpoint buffer (in this case hid_report_in[] is not busy
	//before we modify the contents of the buffer for the next transmission.
    if(USBHandleBusy(lastTransmission)) return;

    // Report ID for multi-touch contact information reports (based on report descriptor)
    hid_report_in[0] = 0x01;	//Report ID in byte[0]

    hid_report_in[13] = t_data.data.TD_STATUS & 0b00000111; // Number of valid contacts

    // Touch point 1
    if (hid_report_in[13] >= 1) {
        hid_report_in[1] = 3; // Contact 1, In-range and tip switch
    } else {
        hid_report_in[1] = 0;
    }
    //First contact info in bytes 1-6
    hid_report_in[2] = (t_data.data.TOUCH1_YH >> 4) & 0x00001111;//Contact ID
    hid_report_in[3] = t_data.data.TOUCH1_XL;                   //X-coord LSB
    hid_report_in[4] = t_data.data.TOUCH1_XH & 0x00001111;	//X-coord MSB
    hid_report_in[5] = t_data.data.TOUCH1_YL;			//Y-coord LSB
    hid_report_in[6] = t_data.data.TOUCH1_YH & 0x00001111;		//Y-coord MSB

    // Touch point 2
    if (hid_report_in[13] >= 2) {
        hid_report_in[7] = 3; // Contact 2, In-range and tip switch
    } else {
        hid_report_in[7] = 0;
    }
    hid_report_in[8] = (t_data.data.TOUCH2_YH >> 4) & 0x00001111;//Contact ID
    hid_report_in[9] = t_data.data.TOUCH2_XL;                   //X-coord LSB
    hid_report_in[10] = t_data.data.TOUCH2_XH & 0x00001111;	//X-coord MSB
    hid_report_in[11] = t_data.data.TOUCH2_YL;			//Y-coord LSB
    hid_report_in[12] = t_data.data.TOUCH2_YH & 0x00001111;	//Y-coord MSB

    /*
    // Touch point 3
    if (hid_report_in[19] >= 3) {
        hid_report_in[13] = 3; // Contact 2, In-range and tip switch

        hid_report_in[14] = (t_data.data.TOUCH3_YH >> 4) & 0x00001111;//Contact ID
        hid_report_in[15] = t_data.data.TOUCH3_XL;                   //X-coord LSB
        hid_report_in[16] = t_data.data.TOUCH3_XH & 0x00001111;	//X-coord MSB
        hid_report_in[17] = t_data.data.TOUCH3_YL;			//Y-coord LSB
        hid_report_in[18] = t_data.data.TOUCH3_YH & 0x00001111;	//Y-coord MSB
    } else {
        hid_report_in[13] = 0;
        hid_report_in[14] = 0;
        hid_report_in[15] = 0;
        hid_report_in[16] = 0;
        hid_report_in[17] = 0;
        hid_report_in[18] = 0;
    }*/

    lastTransmission = HIDTxPacket(HID_EP, (BYTE*)hid_report_in, 14);
}

/** EOF MultiTouch_MultiModes.c *************************************************/
#endif
