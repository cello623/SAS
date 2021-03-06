/* =============================================================================================
   Program Description

   Version 0 of the GRASP program does not compress the images. Many of the
   functions used are made available in the Linux version of the AVT GigE SDK, which can be
   found online.

   This program was written for use in a Linux environment. Follow the prompts in the program
   to advance it. Pressing CTRL + 'c' will allow changing of parameters, pausing of the program,
   or termination of the program.

   Please pay attention to the FRAMESCOUNT parameter and recompile if it needs to be changed.

   Development versions of the level zero software are appended with _dvN.cpp 
   Where N is the version.
   ========================================================================================== */


/* =============================================================================================
   Includes
   ========================================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <iostream>
#include <sstream>
#include <string>
#include <cstdlib>
#include <sys/time.h>
#include <fstream>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <arpa/inet.h>

//PvApi libraries
#define PVDECL
#define _x86
#define _LINUX
#include <PvApi.h>
#include <ImageLib.h>
// _____________________________________________________________________________________________

/* =============================================================================================
   Declaration of namespaces and definitions
   ========================================================================================== */
using namespace std;
//#define	_STDCALL
#define	TRUE		0
#define	FRAMESCOUNT	1
// _____________________________________________________________________________________________

/* =============================================================================================
   Structure for camera data. The universal ID is unique to each camera and the handle allows
   the program to interact with the camera. The frame(s) are filled with images as they are
   taken.
   ========================================================================================== */
struct tCamera {

	unsigned long	UID;
	tPvHandle		Handle;
	tPvFrame		Frames[FRAMESCOUNT];
	char			TimeStamps[FRAMESCOUNT][23];
	volatile bool	NewFlags[FRAMESCOUNT];
	int				TicksPerSec;
	int				TicksUntilCapture;
	unsigned long	ExposureLength;
	int				BufferIndex;
	float			FrameHeight;
	float			FrameWidth;
	volatile bool	PauseCapture;
	volatile bool	WantToSave;
};
// _____________________________________________________________________________________________


/* =============================================================================================
	Global Variables
   ========================================================================================== */

const int MAXNUMOFCAMERAS = 4;	// Specifies the maximum number of cameras used. Can be changed.
unsigned long NUMOFCAMERAS;		// Actual number of cameras detected.
volatile bool TERMINATE = false;
volatile bool CAMERASFOUND = false;
int TICKSLCM;
int TICKCOUNT = 0;
bool PAUSEPROGRAM = false;
tCamera GCamera;
tCamera CAMERAS[MAXNUMOFCAMERAS];

//diagnostics
int snapcount=0;
int savecount=0;
int framecount=0;
int timeoutcount=0;
int noSignocount=0;
int Signocount=0;
tPvErr queueStatus;
int frameandqueueErrors=0;
int queueErrors=0;
int trigErrorCount =0;
int queueError=0;

//error handling flags
int queueErrorFrame=0;
bool frameandqueueFlag= false;
bool triggerFlag=false;
int queueClearFlag = 1;
bool requeueCallFlag=false;
bool timeoutFlag=false;
// __________________________________________________________________________________________end


/* =============================================================================================
   Function declarations.
   ========================================================================================== */
void PrintError(int errCode);
void Sleep(unsigned int time);
void SetConsoleCtrlHandler(void (*func)(int), int junk);
void CameraEventCB(void* Context, tPvInterface Interface, tPvLinkEvent Event,
							unsigned long UniqueId);
bool YesOrNo();
int LCM(int x, int y);
bool WaitForCamera();
bool CameraGrab();
bool CameraSetup(tCamera* Camera);
void TicksLCM();
bool CameraStart(tCamera* Camera);
void CameraStop(tCamera* Camera);
void CameraSnap(int x);
void CameraUnsetup(tCamera* Camera);
void CtrlCHandler(int Signo);
void DisplayParameters();
void ProcessImages();
void FindCentroid(tCamera* Camera, int BufferIndex);
void tester(int x, int y);
void FrameDoneCB(tPvFrame* pFrame);
void FrameStats(tCamera* Camera);
void RestartImCap(int j);
void RestartAcq(int j);
void timer(int x);
void queueErrorHandling(int i);
// __________________________________________________________________________________________end


/* =============================================================================================
   Main Program
   Based on the AVT GigE SDK examples named Stream and Snap.
   ========================================================================================== */
int main(int argc, char* argv[]) {

	// Initialize the API.
	if(!PvInitialize()) {

		SetConsoleCtrlHandler(CtrlCHandler, TRUE);

		PvLinkCallbackRegister(CameraEventCB, ePvLinkAdd, NULL);
		PvLinkCallbackRegister(CameraEventCB, ePvLinkRemove, NULL);

		// Wait for a camera to be plugged in.
		if(WaitForCamera()) {
			// Grab cameras.
			if(CameraGrab()) {
				// Set up cameras.
				bool setupSuccess = true;
				for(unsigned int i = 0; i < NUMOFCAMERAS; i++) {
					if(!CameraSetup(&CAMERAS[i]))
						setupSuccess = false;
				}
				if(setupSuccess) {
					// Start streaming from cameras.
					bool startSuccess = true;
					for(unsigned int j = 0; j < NUMOFCAMERAS; j++) {
						if(!CameraStart(&CAMERAS[j]))
							startSuccess = false;
					}
					if(!startSuccess) {
						printf("Failed to start streaming from cameras.\n");
	                } else {
						TicksLCM(); //must have ticks after the signal handler is setup
						while(!TERMINATE) {
							//Wait for interrputs which generate triggers to snap and process images
						}
					}

					// Unsetup the cameras.
					for(unsigned int k = 0; k < NUMOFCAMERAS; k++) {
						CameraUnsetup(&CAMERAS[k]);
					}
				} else
					printf("Failed to set up cameras.\n");
			} else
				printf("Failed to grab cameras.\n");
		} else
			printf("Failed to find cameras or user has ended polling for cameras.\n");

		PvLinkCallbackUnRegister(CameraEventCB, ePvLinkAdd);
		PvLinkCallbackUnRegister(CameraEventCB, ePvLinkRemove);

		// Uninitialize the API.
		PvUnInitialize();

		printf("\nEnd of program reached.\n\n");

	} else
		printf("\nFailed to initialize the API. Program terminating.\n\n");

    return 0;

}
// __________________________________________________________________________________________end


/* =============================================================================================
   An error message printer for translations of numerical error codes to their
   corresponding errors, as noted by the Prosilica PvAPI Manual. Good for debugging.
   ========================================================================================== */
void PrintError(int errCode) {

	printf("Error encountered. Error code: %d\n", errCode);
	switch(errCode) {
		case 2: {
			printf("ePvErrCameraFault: Unexpected camera fault.\n");
			break;
		}
		case 3: {
			printf("ePvErrInternalFault: Unexpected fault in PvAPI or driver.\n");
			break;
		}
		case 4: {
			printf("ePvErrBadHandle: Camera handle is bad.\n");
			break;
		}
		case 5: {
			printf("ePvErrBadParameter: Function parameter is bad.\n");
			break;
		}
		case 6: {
			printf("ePvErrBadSequence: Incorrect sequence of API calls. For example,\n");
			printf("queuing a frame before starting image capture.\n");
			break;
		}
		case 7: {
			printf("ePvErrNotFound: Returned by PvCameraOpen when the requested camera\n");
			printf("is not found.\n");
			break;
		}
		case 8: {
			printf("ePvErrAccessDenied: Returned by PvCameraOpen when the camera cannot be\n");
			printf("opened in the requested mode, because it is already in use by another\n");
			printf("application.\n");
			break;
		}
		case 9: {
			printf("ePvErrUnplugged: Returned when the camera has been unexpectedly\n");
			printf("unplugged.\n");
			break;
		}
		case 10: {
			printf("ePvErrInvalidSetup: Returned when the user attempts to capture images,\n");
			printf("but the camera setup is incorrect.\n");
			break;
		}
		case 11: {
			printf("ePvErrResources: Required system or network resources are unavailable.\n");
			break;
		}
		case 12: {
			printf("ePvErrQueueFull: The frame queue is full.\n");
			break;
		}
		case 13: {
			printf("ePvErrBufferTooSmall: The frame buffer is too small to store the image.\n");
			break;
		}
		case 14: {
			printf("ePvErrCancelled: Frame is cancelled. This is returned when frames are\n");
			printf("aborted using PvCaptureQueueClear.\n");
			break;
		}
		case 15: {
			printf("ePvErrDataLost: The data for this frame was lost. The contents of the\n");
			printf("image buffer are invalid.\n");
			break;
		}
		case 16: {
			printf("ePvErrDataMissing: Some of the data in this frame was lost.\n");
			break;
		}
		case 17: {
			printf("ePvErrTimeout: Timeout expired. This is returned only by functions with\n");
			printf("a specified timeout.\n");
			break;
		}
		case 18: {
			printf("ePvErrOutOfRange: The attribute value is out of range.\n");
			break;
		}
		case 19: {
			printf("ePvErrWrongType: This function cannot access the attribute, because the\n");
			printf("attribute type is different.\n");
			break;
		}
		case 20: {
			printf("ePvErrForbidden: The attribute cannot be written at this time.\n");
			break;
		}
		case 21: {
			printf("ePvErrUnavailable: The attribute is not available at this time.\n");
			break;
		}
		case 22: {
			printf("ePvErrFirewall: A firewall is blocking the streaming port.\n");
			break;
		}
		default: {
			printf("Unrecognizable error. Check for unintended calls to this function.\n");
			break;
		}
	}

}
// __________________________________________________________________________________________end



/* =============================================================================================
   To facilitate waiting for camera discovery
   ========================================================================================== */
void Sleep(unsigned int time) {

	struct timespec t,r;

	t.tv_sec = time / 1000;
	t.tv_nsec = (time % 1000) * 1000000;

	while(nanosleep(&t, &r) == -1) //if the nanosleep is interrupted by a sig handler then 
		t = r;					   // it returns -1	

}
// __________________________________________________________________________________________end



/* =============================================================================================
   Handler for CTRL key
   ========================================================================================== */
void SetConsoleCtrlHandler(void (*func)(int), int junk) {

	signal(SIGINT, func);

}
// __________________________________________________________________________________________end



/* =============================================================================================
   Callback called when a camera is plugged or unplugged.
   From AVT GigE SDK example named Stream.
   ========================================================================================== */
void CameraEventCB(void* Context, tPvInterface Interface, tPvLinkEvent Event,
							unsigned long UniqueId) {

	switch(Event) {
		case ePvLinkAdd: {
			printf("\nCamera %lu is now plugged in.\n", UniqueId);
			break;
		}
		case ePvLinkRemove: {
			printf("\nCamera %lu has been unplugged.\n", UniqueId);
			break;
		}
		default:
			break;
	}
}
// __________________________________________________________________________________________end



/* =============================================================================================
   Wait for the user to choose between "yes" or "no" when prompted.
   ========================================================================================== */
bool YesOrNo() {

	char choice;

	do {
		choice = getc(stdin);
	} while(choice != 'y' && choice != 'n');

	return choice == 'y';

}
// __________________________________________________________________________________________end



/* =============================================================================================
   Finds the least common multiple of two integers.
   ========================================================================================== */
int LCM(int x, int y) {

	if(x == 1)
		return y;
	else if(y == 1)
		return x;
	else {
		for(int i = 1;  ; i++) {
			if(i % x == 0 && i % y == 0)
				return i;	
		}
	}
}
// __________________________________________________________________________________________end



/* =============================================================================================
   Wait for a camera to be plugged in.
   From AVT GigE SDK example named Stream; also used in Snap.
   ========================================================================================== */
bool WaitForCamera() {

	int i = 0;

	//Wait for API to discover cameras on the network
	printf("\nWaiting for a camera to be plugged in...\n");
	while(!PvCameraCount() && !TERMINATE) {
		Sleep(250); 
		i++;
		if(i > 10000)
			return false;
	}

	if(TERMINATE)
		return false;

	CAMERASFOUND = true;
	return true;

}
// __________________________________________________________________________________________end



/* =============================================================================================
   Grab cameras and establish default parameters.
   ========================================================================================== */
bool CameraGrab() {

	tPvUint32 count;
	tPvCameraInfo list[MAXNUMOFCAMERAS];

	count = PvCameraList(list, MAXNUMOFCAMERAS, &NUMOFCAMERAS);
	if(count >= 1) {

		for(unsigned int i = 0; i < count; i++) {
			printf("\nGrabbing camera %s with ID %lu\n", list[i].SerialString,
				   list[i].UniqueId);
        	memset(&CAMERAS[i], 0, sizeof(tCamera));
			CAMERAS[i].UID = list[i].UniqueId;
			CAMERAS[i].BufferIndex = 0;
			CAMERAS[i].PauseCapture = false;
			CAMERAS[i].WantToSave = false;
			for(int j = 0; j < FRAMESCOUNT; j++) {
				CAMERAS[i].NewFlags[j] = false; // =true for a populated frame that hasn't been processed
			}
		}
		return true;

	} else
		return false;

}
// __________________________________________________________________________________________end



/* =============================================================================================
   Opens a camera and fills in parameters.
   ========================================================================================== */
bool CameraSetup(tCamera* Camera) {

	//Open cameras discovered by the API
	if(!PvCameraOpen(Camera->UID, ePvAccessMaster, &(Camera->Handle))) {

		printf("Now setting up camera with ID %lu\n", Camera->UID);
		printf("\nHow quickly should pictures be processed?\n");
		printf("Pictures per second: ");
		scanf("%d", &Camera->TicksPerSec);
		
		printf("\nHow long should the exposure value be in microseconds?\n");
		printf("Upper limit based on pictures per second: %d\n", 1000000 / Camera->TicksPerSec);
		printf("Exposure time: ");
		scanf("%lu", &Camera->ExposureLength);

		printf("\nWould you like to save the pictures to the hard drive? (y/n): ");
		if(YesOrNo())
			Camera->WantToSave = true;
		else
			Camera->WantToSave = false;
		printf("\n");

		//define the pixel format. see camera and driver attributes p.15
		PvAttrEnumSet(Camera->Handle, "PixelFormat", "Mono8");

		//define ROI
		/*if(PvAttrUint32Set(Camera->Handle, "RegionX", 0)||
			PvAttrUint32Set(Camera->Handle, "RegionY", 0)||
			PvAttrUint32Set(Camera->Handle, "Height", 400)||
			PvAttrUint32Set(Camera->Handle, "Width", 400)){
			cout<<"The ROI isn't defined correctly\n"<<"\n";
		}*/

		return true;

	} else
		return false;

	return true;
}
// __________________________________________________________________________________________end



/* =============================================================================================
   Finds the least common multiple of all the ticks/second values of the cameras, then sets up
   a periodic ticker accordingly.
   ========================================================================================== */
void TicksLCM() {

	bool tempBool = PAUSEPROGRAM;

	PAUSEPROGRAM = true;

	if(NUMOFCAMERAS == 1)
		TICKSLCM = CAMERAS[0].TicksPerSec;
	else if (NUMOFCAMERAS == 2)
		TICKSLCM = LCM(CAMERAS[0].TicksPerSec, CAMERAS[1].TicksPerSec);
	else {
		TICKSLCM = LCM(CAMERAS[0].TicksPerSec, CAMERAS[1].TicksPerSec);
		for(unsigned int i = 2; i < NUMOFCAMERAS; i++) {
			TICKSLCM = LCM(TICKSLCM, CAMERAS[i].TicksPerSec);
		}
	}

	for(unsigned int j = 0; j < NUMOFCAMERAS; j++) {
		CAMERAS[j].TicksUntilCapture = TICKSLCM / CAMERAS[j].TicksPerSec;
	}

	//print TICKSLCM
	cout<<"This is the TICKSLCM: "<<TICKSLCM<<"\n";

	timer(0); 	//create and arm the timer which generates an interrupts at a cadence of TICKSLCM
	
	PAUSEPROGRAM = tempBool;

}
// __________________________________________________________________________________________end

/*============================================================================================
 Handles timers for the camera SNAP
 timer(0); create timer
 timer(1); disable timer
==============================================================================================*/
void timer(int x){

		/*Create and handle the CameraSnap timer: 
		1. Create the handler structure
		2. Create the event notification structure
		3. Create the timer 
		4. Arm the timer */
	
	
		//create handler
		struct sigaction act;  
		struct sigaction oldact;
		act.sa_handler = CameraSnap;
		sigaction(10, &act, &oldact);	

		//set the sigevent notification structure
		struct sigevent timersigevent; //create signal event
		memset(&timersigevent, 0, sizeof timersigevent); //initialize the struct with zeros
		timersigevent.sigev_notify = SIGEV_SIGNAL; //send a signal upon expiration of timer
		timersigevent.sigev_signo = 10; //set to SIGUSR1 number 10

		//create the timer
		timer_t timer1; //timer identifier	
		if(timer_create(CLOCK_REALTIME, &timersigevent, &timer1) == 0){
			printf("timer created correctly\n");
		}else{
			printf("timer not created \n");
		} 


	if(x == 0){
		//Set timer values
		struct itimerspec cadence;
		memset(&cadence, 0, sizeof cadence);
		if (TICKSLCM != 1){
			cadence.it_value.tv_sec= 0; 		//value is time from set until first tick
			cadence.it_value.tv_nsec =  1000000000/ TICKSLCM; 
 			cadence.it_interval.tv_sec=0; 		//interval resets the timer to this value
			cadence.it_interval.tv_nsec= 1000000000/ TICKSLCM;
		} else{
			cadence.it_value.tv_sec= 1; 
			cadence.it_value.tv_nsec =  0; 
	 		cadence.it_interval.tv_sec= 1; 
			cadence.it_interval.tv_nsec= 0;
		}

		//arm the timer
		if(timer_settime(timer1, 0, &cadence, NULL) == 0){
			printf("timer armed correctly\n");
		}else{
			printf("timer not armed.\n");
		}
	}else if(x == 1){
		//disable the timer	
		struct itimerspec pause;
		memset(&pause, 0, sizeof pause);
		if(timer_settime(timer1, 0, &pause, NULL) == 0)
			printf("SNAP timer disabled.\n");
	}

	cout<<"\n";
}
//_________________________________________________________________________


/* =============================================================================================
   Finishes setting up a camera and starts streaming.
   ========================================================================================== */
bool CameraStart(tCamera* Camera) {

	unsigned long FrameSize = 0;

	printf("Starting camera with ID %lu\n", Camera->UID);

	// Auto adjust the packet size to the maximum supported by the network; usually 8228,
	// but 6000 for the current hardware.
	if(PvCaptureAdjustPacketSize(Camera->Handle, 6000)== ePvErrSuccess )
			cout<< "Packet Size sucessfully determined.\n\n";
	else
			cout<<"Possible Packet Size issue.\n\n";

	// Determine how big the frame buffers should be and set the exposure value.
	if(!PvAttrUint32Get(Camera->Handle, "TotalBytesPerFrame", &FrameSize)
	   && !PvAttrUint32Set(Camera->Handle, "ExposureValue", Camera->ExposureLength)) {

		Camera->FrameHeight = fabs(sqrt(.75 * FrameSize));
		Camera->FrameWidth = FrameSize / Camera->FrameHeight;
		printf("\nFrame buffer size: %lu; %f by %f\n", FrameSize, Camera->FrameWidth,
			   Camera->FrameHeight);

		bool failed = false;

		// Allocate the buffer for multiple frames.
		for(int i = 0; i < FRAMESCOUNT; i++) {
			Camera->Frames[i].ImageBuffer = new char[FrameSize];
			if(Camera->Frames[i].ImageBuffer)
				Camera->Frames[i].ImageBufferSize = FrameSize;
			else
				failed = true;
		}

		if(!failed) {
			// Prepare the camera for capturing pictures.
			if(!PvCaptureStart(Camera->Handle)) {
				// Set the trigger mode to software for controlled acquisition.
				if(!PvAttrEnumSet(Camera->Handle, "AcquisitionMode", "Continuous")
				   && !PvAttrEnumSet(Camera->Handle, "FrameStartTriggerMode", "Software")) {
					// Begin acquisition.
					if(PvCommandRun(Camera->Handle, "AcquisitionStart")) {
						// If that fails, reset the camera to non-capture mode.
						PvCaptureEnd(Camera->Handle);
						return false;
					} else {
						printf("Camera with ID %lu is now acquiring images.\n", Camera->UID);
						Camera->PauseCapture = false;
						return true;
					}
				} else
					return false;
			} else
				return false;
		} else
			return false;
	} else
		return false;
}
// __________________________________________________________________________________________end



/* =============================================================================================
   Stop streaming from a camera.
   ========================================================================================== */
void CameraStop(tCamera* Camera) {

	printf("\nStopping the stream for camera with ID %lu.\n", Camera->UID);
	PvCommandRun(Camera->Handle, "AcquisitionStop");
	PvCaptureEnd(Camera->Handle);

}
// _________________________________________________________________________________________end



/*=============================================================================================
  Called by the timer interrupt 
  Triggers image acquisition and timestamps the image.
  ========================================================================================== */
void CameraSnap(int x) {

	//variables for thread blocking
	sigset_t set;
	struct timespec timeout;
	
	//fill out structs for set of signals used to unblock the thread after snapping
	sigemptyset(&set); 				//initializes the set to exclude all signals
	if(sigaddset(&set, 10) == -1)  	//add chosen Signo to set, the same interrupt which calls this function
		printf("Signo NOT added to Sig. \n");
	if(!PAUSEPROGRAM) {

		//diagnostic to see if we snap on time
		tester(0, 0);

		//advance ticks
		TICKCOUNT++;

		for(unsigned int i = 0; i < NUMOFCAMERAS; i++) {
			if(CAMERAS[i].Handle != NULL && TICKCOUNT % CAMERAS[i].TicksUntilCapture == 0
			   && !CAMERAS[i].PauseCapture) {

				//if triggerFlag= true, didn't return from previous call to trigger
				if(triggerFlag){
					cout<<"\nRestarting Image Capture: trigger fault.\n";
					trigErrorCount++;
					RestartImCap(i);
				}
				
				//set timeout for threadblocking
				timeout.tv_sec = 0;
				timeout.tv_nsec = CAMERAS[i].ExposureLength+200000000;

				//timestamp image
				ostringstream os;
				ostringstream imageIndex;
			  	time_t rawtime;
				timeval highrestime;
			  	struct tm * timeinfo;
				char filename[17];
				time(&rawtime);
				timeinfo = localtime(&rawtime);
				strftime (filename, 17, "%Y%m%d_%H%M%S_", timeinfo);
				gettimeofday(&highrestime, NULL); 
				imageIndex << highrestime.tv_usec;
				os << filename << imageIndex.str();
				strcpy(CAMERAS[i].TimeStamps[CAMERAS[i].BufferIndex], os.str().c_str());
			 
				//Check if program returned from queue flag last time
				if(requeueCallFlag){
					cout<<"Restarting image capture: failure to return from requeue.\n\n";
					RestartImCap(i);
					requeueCallFlag = false;
				}
				requeueCallFlag=true;

				//requeue a frame buffer
				queueStatus=PvCaptureQueueFrame(CAMERAS[i].Handle,
									&(CAMERAS[i].Frames[CAMERAS[i].BufferIndex]),
									FrameDoneCB); //Place an image buffer onto the queue frame
				requeueCallFlag=false;

				//Snap picture if requeue success
				if(queueStatus == ePvErrSuccess){
					CAMERAS[i].NewFlags[CAMERAS[i].BufferIndex] = true;
					frameandqueueFlag = false;

					//trigger command and set flags
					triggerFlag= true;
					timeoutFlag= true;
					cout<<"Frame queued successfully. Trigger SNAP. \n";
					PvCommandRun(CAMERAS[i].Handle, "FrameStartTriggerSoftware"); //Software trigger to SNAP				
					triggerFlag= false;

					//block thread until interrupted or timed out. If timed out, kill the image
					//since there are two threads, the interrupt isn't always caught on this thread
					//A flag is used to ensure that timeouts are real and the image definitely hasn't returned 
					if (sigtimedwait(&set, NULL, &timeout) == -1){
							if(errno == EAGAIN){
								if(timeoutFlag){
									timeoutFlag= false;
									printf("Timeout. \n");
									timeoutcount++;
									if(PvCaptureQueueClear(CAMERAS[i].Handle)==ePvErrSuccess)
										printf("Frame buffer queue cleared. \n");
								}
							} else if(errno == EINTR){
								//printf("Interrupted by signal other than Signo. \n");
								noSignocount++;
							}else if(errno == EINVAL){
								//printf("Timeout is invalid. \n");
							}
					}else{
							//printf("Wait interrupted by Signo. \n");
							Signocount++;
					}
				}else if(queueStatus == ePvErrUnplugged){
					cout<<"Queue frame returns Camera is unplugged. No Snap. \n";
					frameandqueueFlag= false;
					
				}else if(queueStatus != ePvErrSuccess){
					cout<<"Failed to requeue frame. No Snap. \n";
					queueErrorHandling(i);
				}else{
					cout<<"Queue return status is unknown!!!!!.\n";
					frameandqueueFlag=false;
				}
			
				//Change active buffer for a camera
				CAMERAS[i].BufferIndex++;
				if (CAMERAS[i].BufferIndex >= FRAMESCOUNT)
					CAMERAS[i].BufferIndex = 0;
				os.seekp(0);			// Rewind os so we overwrite value next time
				imageIndex.seekp(0);	// Rewind for the same reason as above
			}else{
				//Diagnostics
				printf("skipped the SNAP if statement because: "); 
				if(CAMERAS[i].Handle == NULL)
					printf("CAMERAS[i].Handle == NULL \n");
				if(TICKCOUNT % CAMERAS[i].TicksUntilCapture != 0)
					printf("Remainder of TICKCOUNT & CAMERAS[i].TicksUntilCapture != 0 \n");
				if(CAMERAS[i].PauseCapture)
					printf("CAMERAS[i].PauseCapture == true \n");
				if(CAMERAS[i].NewFlags[CAMERAS[i].BufferIndex])
					printf("CAMERAS[i].NewFlags[CAMERAS[i].BufferIndex] == true \n");
			}
		
		}

		//reset TICKCOUNT 
		if(TICKCOUNT >= TICKSLCM)
			TICKCOUNT = 0;
	}

}
// __________________________________________________________________________________________end



/*==============================================================================================
	Error Handling for queue frame
==============================================================================================*/
void queueErrorHandling(int i){

	//if the requeue results in error 3 times, restart image capture
	//infinite requeue errors were seen in testing. Restarting image capture fixes the problem.
	if(snapcount == queueErrorFrame + 1){ 
		queueError++;
		cout<<"Successive queue Failure: "<<queueError+1<<"\n\n";
	}else{
		queueError = 0;	
	}
	if(queueError >= 2){
		cout<<"Restarting Image Stream: successive requeue errors.\n";
		queueErrors++;
		RestartImCap(i);
		queueError = 0;
	}
	queueErrorFrame = snapcount;


	//if a frame returns error 16 followed by a frame requeue failure, restart Image Capture
	//This pattern of frame and queue errors was seen to preceed system crashes in testing.
	if(frameandqueueFlag){
		cout<<"Restarting Image Stream: frame and queue errors\n";
		RestartImCap(i);
		frameandqueueFlag=false;
		frameandqueueErrors++;
	}
}
//______________________________________________________________________________________________


/* =============================================================================================
   Unsetup the camera.
   From AVT GigE SDK example named Stream.
   ========================================================================================== */
void CameraUnsetup(tCamera* Camera) {

	printf("Preparing to unsetup camera with ID %lu\n", Camera->UID);
	printf("\nClearing the queue.\n");
	// Dequeue all the frames still queued (causes a block until dequeue finishes).
	PvCaptureQueueClear(Camera->Handle);
	// Close the camera.
	printf("Closing the camera.\n"); 
	PvCameraClose(Camera->Handle);

	// Delete the allocated buffer(s).
	for(int i = 0; i < FRAMESCOUNT; i++)
		delete [] (char*)Camera->Frames[i].ImageBuffer;

	Camera->Handle = NULL;

}
// __________________________________________________________________________________________end



/* =============================================================================================
   CTRL-C handler for changing parameters, checking stats and restarting streams
   ========================================================================================== */
void CtrlCHandler(int Signo) {

	if(CAMERASFOUND) {

		PAUSEPROGRAM = true;
		DisplayParameters();

		// Print performance statistics
		cout<<"\nTimeout Count: "<<timeoutcount<<"\n";
		cout<<"Software Trigger error: "<<trigErrorCount<<"\n";
		cout<<"Successive Queue failures: "<<queueErrors<<"\n";
		cout<<"Frame and Queue errors: "<<frameandqueueErrors<<"\n\n";

		// Print Camera Statistics
		for(unsigned int k = 0; k < NUMOFCAMERAS; k++){
			FrameStats(&CAMERAS[k]);
		}

		// Options
		printf("Would you like to end the program? (y/n): ");
		if(YesOrNo()) {

			for(unsigned int i = 0; i < NUMOFCAMERAS; i++) {
				CAMERAS[i].PauseCapture = true;
				CAMERAS[i].WantToSave = false;
				CameraStop(&CAMERAS[i]);
			}
			TERMINATE = true;

		} else {

			for(unsigned int j = 0; j < NUMOFCAMERAS; j++) {

				printf("Dealing with settings for camera with ID %lu\n", CAMERAS[j].UID);
				printf("----------\n");

				printf("Would you like to change the pause option? (y/n): ");
				if(YesOrNo()) {
					if(CAMERAS[j].PauseCapture)
						CAMERAS[j].PauseCapture = false;
					else
						CAMERAS[j].PauseCapture = true;
				}

				printf("Would you like to change the save option? (y/n): ");
				if(YesOrNo()) {
					if(CAMERAS[j].WantToSave)
						CAMERAS[j].WantToSave = false;
					else
						CAMERAS[j].WantToSave = true;
				}

				printf("Would you like to change some parameters? (y/n): ");
				if(YesOrNo()) {
					CAMERAS[j].PauseCapture = true;
					CameraStop(&CAMERAS[j]);
					CameraUnsetup(&CAMERAS[j]);
					printf("Restarting camera with ID %lu\n", CAMERAS[j].UID);
					CameraSetup(&CAMERAS[j]);
					CameraStart(&CAMERAS[j]);
				}

				printf("Would you like to restart the acquisition for this camera? (y/n): ");
				if(YesOrNo()){
					RestartAcq(j);
				}

				printf("Would you like to restart the image capture stream? (y/n): ");
				if(YesOrNo()){
					RestartImCap(j);
				}
			}
		}

		PAUSEPROGRAM = false;

	} else {
		printf("Would you like to stop looking for cameras and end the program? (y/n): ");
		if(YesOrNo()) {
			TERMINATE = true;
		}
	}
}
// __________________________________________________________________________________________end



/*===========================================================================================
	Restart Acquisition
=============================================================================================*/
void RestartAcq(int j){

	//pause program
	PAUSEPROGRAM= true;

	if(PvCommandRun(CAMERAS[j].Handle, "AcquisitionStop")==ePvErrSuccess)
		cout<<"Acquisition Stopped.\n";
	if(PvCommandRun(CAMERAS[j].Handle, "AcquisitionStart")==ePvErrSuccess)
		cout<<"Acquisition Started. \n\n";

	//restart program
	PAUSEPROGRAM=false;
}
//____________________________________________________________________________________________



/*============================================================================================
	Restart the image capture stream
==============================================================================================*/
void RestartImCap(int j){

	//pause program
	PAUSEPROGRAM = true;

	if(PvCommandRun(CAMERAS[j].Handle, "AcquisitionStop")==ePvErrSuccess){
		cout<<"Acquisition Stopped.\n";
	}else{
		cout<<"Couldn't stop Acquisition.\n";
	}
	if(PvCaptureQueueClear(CAMERAS[j].Handle)==ePvErrSuccess)
		printf("Frame buffer queue cleared. \n");
	if(PvCaptureEnd(CAMERAS[j].Handle)==ePvErrSuccess)
		printf("Image capture stream terminated. \n");
	if(PvCaptureStart(CAMERAS[j].Handle)==ePvErrSuccess)
		printf("Image capture stream restarted. \n");
	if(PvCommandRun(CAMERAS[j].Handle, "AcquisitionStart")==ePvErrSuccess)
		cout<<"Acquisition Started. \n\n";

	//restart Program
	PAUSEPROGRAM=false;
}
//______________________________________________________________________________________________



/* =============================================================================================
   Displays the parameters chosen for the camera.
   ========================================================================================== */
void DisplayParameters() {

	printf("\n");
	for(unsigned int i = 0; i < NUMOFCAMERAS; i++) {
		printf("Displaying settings for camera with ID %lu\n", CAMERAS[i].UID);
		printf("----------\n");
		printf("Images per second: %d\n", CAMERAS[i].TicksPerSec);
		printf("Exposure time in microseconds: %lu\n", CAMERAS[i].ExposureLength);
		if(CAMERAS[i].PauseCapture == true)
			printf("Camera capture paused? true\n");
		else
			printf("Camera capture paused? false\n");
		if(CAMERAS[i].WantToSave == true)
			printf("Saving images taken by this camera? true\n");
		else
			printf("Saving images taken by this camera? false\n");
		printf("\n");
	}
}
// __________________________________________________________________________________________end



/* =============================================================================================
   Processes the frame and saves it if desired.
   Loosely based on the AVT GigE SDK function FrameDoneCB in example named Stream.
   ========================================================================================== */
void ProcessImages() {

	//image returned before timeout
	timeoutFlag=false;

	//define variables
	char* image;

	for(unsigned int i = 0; i < NUMOFCAMERAS; i++) {
		for(unsigned int j = 0; j < FRAMESCOUNT; j++) {
			
			if(CAMERAS[i].NewFlags[j] && CAMERAS[i].Frames[j].Status == ePvErrSuccess
			   && CAMERAS[i].Frames[j].BitDepth != 0) {

				//For v0, we don't process images
				/*switch(i) {

					case 0: {
						FindCentroid(&CAMERAS[i], CAMERAS[i].BufferIndex);
						break;
					}
					case 1: {
						FindCentroid(&CAMERAS[i], CAMERAS[i].BufferIndex);						
						break;
					}
					case 2: {
						FindCentroid(&CAMERAS[i], CAMERAS[i].BufferIndex);
						break;
					}
					case 3: {
						FindCentroid(&CAMERAS[i], CAMERAS[i].BufferIndex);
						break;
					}
					default: {
						FindCentroid(&CAMERAS[i], CAMERAS[i].BufferIndex);
						break;
					}
				}*/
				
				if(CAMERAS[i].WantToSave) {
					 
					ostringstream filename;

					//Write to a binary file with .bin extension
					filename << "images/" << CAMERAS[i].UID << "_" << CAMERAS[i].TimeStamps[j]
							 <<"_"<< savecount << ".bin";
					ofstream imageFile;
					imageFile.open(filename.str().c_str(), ios::out | ios::binary);
					image=(char*)CAMERAS[i].Frames[j].ImageBuffer;
					if (imageFile.is_open()){
						//tester(1,0); //print time before write
						imageFile.write((char*)image, CAMERAS[i].Frames[j].ImageBufferSize);
						//tester(2,0); //print time after write
						imageFile.close();
						cout<< "Save count: "<<savecount<<"\n";
						cout<<"-------------------------------------------------------- \n";
					}

					 //This writes the image to a tiff from frame buffer. 
					/*filename << "images/" << CAMERAS[i].UID << "_" << CAMERAS[i].TimeStamps[j]
							 <<"_"<< savecount << ".tiff";	// Images saved here
					//tester(1,0);
					tiffstatus=ImageWriteTiff(filename.str().c_str(), &(CAMERAS[i].Frames[j]));
					//tester(2,0);
					//tiffstatus=ImageWriteTiff(filename.str().c_str(), &image);
					if(tiffstatus != 1){
						printf("Failed to save frame grabbed at %s.\n", filename.str().c_str());
					}
					//cout<< "Save count: "<<savecount<<"\n";
					tester(3,tiffstatus); */
					
					filename.seekp(0);	
					savecount++;				
				}

			}else{
				printf("ProcessImages skipped b/c: ");
				if(!CAMERAS[i].NewFlags[j])
					printf("CAMERAS[i].NewFlags[j]= false \n");
				if(CAMERAS[i].Frames[j].Status != ePvErrSuccess){
					printf("CAMERAS[i].Frames[j].Status != ePvErrSuccess \n");
					frameandqueueFlag = true;
				}
				if(CAMERAS[i].Frames[j].BitDepth == 0)
					printf("CAMERAS[i].Frames[j].BitDepth == 0 \n");
			}

			//set NewFlags = false for every frame that runs the callback
			CAMERAS[i].NewFlags[j] = false;
		}
	} 
}
// __________________________________________________________________________________________end



/* =============================================================================================
   Finds the centroid of an image and marks it with a horizontal and vertical line.
   ========================================================================================== */
void FindCentroid(tCamera* Camera, int BufferIndex) {

	/*char* image;
	int averagePixelValue = 0;
	bool binaryImage[(int)(Camera->FrameWidth * Camera->FrameHeight)];
	int centroidX = 0;
	int centroidY = 0;
	int binaryCount = 0;
	float width = Camera->FrameWidth;
	float height= Camera->FrameHeight;

	image = (char*)Camera->Frames[BufferIndex].ImageBuffer;


	for(int i = 0; i < (int)(Camera->FrameWidth * Camera->FrameHeight); i++) {
		averagePixelValue += image[i];
	}

	//TEST
	cout<<"avg pixel value "<<averagePixelValue<<"\n";
	averagePixelValue = averagePixelValue / (int)(Camera->FrameWidth * Camera->FrameHeight);
	
	//TEST
	cout<<"avg pixel value "<< averagePixelValue<<"\n";
	
	for(int j = 0; j < (int)(Camera->FrameWidth * Camera->FrameHeight); j++) {
		if(image[j] >= averagePixelValue)
			binaryImage[j] = true;
		else
			binaryImage[j] = false;
	}

	for(int m = 0; m < (int)Camera->FrameWidth; m++) {
		for(int n = 0; n < (int)Camera->FrameHeight; n++) {
			if(binaryImage[m * n]) {
				centroidX += m;
				centroidY += n;
				binaryCount++;
			}
		}
	}

	//Test
	cout <<"centroid X, Y \n"<< centroidX <<', '<< centroidY<< '\n';
	centroidX = centroidX / binaryCount;
	centroidY = centroidY / binaryCount;

	for(int p = 0; p < (int)Camera->FrameHeight; p++) {
		image[centroidY + (p * (int)Camera->FrameWidth)] = 0;
	}

	for(int q = 0; q < (int)Camera->FrameWidth; q++) {
		image[q + (centroidX * (int)Camera->FrameWidth)] = 0;
	} */

}
// __________________________________________________________________________________________end


/*===========================================================================================
  Call back is run when a frame completes or is killed
  ===========================================================================================*/
void FrameDoneCB(tPvFrame* pFrame)
{
	//tester(4,0); 

	ProcessImages();
}
//__________________________________________________________________________________________end



/*==============================================================================================
	Test fuction for diagnostics
	========================================================================================= */
void tester(int x, int y){

	//get time
	timeval highrestime;
	char filename[17];
	time_t rawtime;
	struct tm * timeinfo;
	gettimeofday(&highrestime, NULL); 
	time(&rawtime);
	timeinfo = localtime(&rawtime);
	strftime (filename, 17, "%Y%m%d_%H%M%S_", timeinfo);

	//tests
	switch (x){
		case 0 : {
			cout<<"SNAP "<< snapcount <<" : "<<filename << highrestime.tv_usec<<"\n";
			snapcount++;
			break;
		}
		case 1 : {
			cout<<"START Saving : "<<filename << highrestime.tv_usec<<"\n";
			break;
		}
		case 2 : {
			cout<<"End Saving : "<<filename << highrestime.tv_usec<<"\n";
			break;
		}
		case 3 : {
			cout<<"Save Status : ";
			if( y == 1 ){
				cout<< "success.\n";
			}else{
				cout<< "failure.\n";
			}
			cout<<"-------------------------------------------------------- \n";
			break;
		}
		case 4 : {
			cout<<"** Frame "<< framecount<< " Done : "<<filename << highrestime.tv_usec<<"\n";
			framecount ++;
			break;
		}
		default: {
			break;
		}
	}
}
//_____________________________________________________________________________________________



/*=============================================================================================
	This returns statistics on each camera's frames
  ============================================================================================*/
void FrameStats(tCamera* Camera){

	unsigned long Ncomp=0;		//Num of frames acquired
	unsigned long Ndrop=0;		//Num of frames unsuccessfully acquired
	unsigned long Nerr=0;		//Num of erraneous packets
	unsigned long Nmiss=0;		//Num packets sent by camera not received by host
	unsigned long Nrec=0;		//Num packets sent by camera and received by host	
	unsigned long Nreq=0;		//Num of missing packets requested by camera for resend
	unsigned long Nres=0;		//Num of missing packets resent by camera and receieved by host

	PvAttrUint32Get(Camera->Handle, "StatFramesCompleted", &Ncomp);
	PvAttrUint32Get(Camera->Handle, "StatFramesDropped", &Ndrop);
	PvAttrUint32Get(Camera->Handle, "StatPacketsErroneous", &Nerr);
	PvAttrUint32Get(Camera->Handle, "StatPacketsMissed", &Nmiss);
	PvAttrUint32Get(Camera->Handle, "StatPacketsReceived", &Nrec);
	PvAttrUint32Get(Camera->Handle, "StatPacketsRequested", &Nreq);
	PvAttrUint32Get(Camera->Handle, "StatPacketsResent", &Nres);

	cout<<"\nStatistics for camera: "<<Camera->UID<<"\n";
	cout<<"Frames Completed: "<< Ncomp <<"\n";
	cout<<"Frames Dropped: "<<Ndrop<<"\n";
	cout<<"Num of erroneous packets received: "<<Nerr<<"\n";
	cout<<"Num of packets sent by camera and NOT received by host : "<<Nmiss<<"\n";
	cout<<"Num of packets sent by camera and received by host: "<<Nrec<<"\n";
	cout<<"Num of missing packets requested to camera for resend: "<<Nreq<<"\n";
	cout<<"Num of missing packets resent by camera and received by host: "<<Nres<<"\n\n";

}
//______________________________________________________________________________________________



