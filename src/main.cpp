///The Main source files have no header files
//Everything is contained in here
#include <stdio.h>
using namespace std;

//Main loop
void loop();
//System Check
int system_check();

//Timer
#include <Timer/timer.h>
TIMER watch;

//The Hardware environment is always running
#include <hardware/hardware.h>
hardware hw;

//Controller is always running
#include "controller.h"
controller control;

//If you're simulating the vehicle you have to turn on the 
//modeling environment
#if defined (SIL) || (SIMONLY) || (HIL)
#if defined (DESKTOP)
#define MODELING
#include <modeling/modeling.h>
modeling model;
#endif
#endif

//If running in SIL or HIL mode you need to run in realtime
//But I think only on DESKTOP. Ok nvm. You need to run in realtime
//in auto mode as well which basically means only do this in SIMONLY
#ifndef SIMONLY
#define REALTIME
#endif

//Need some logic here to determine any hardware in the loop
//modes
#ifdef HIL
//Running in HIL mode
#ifdef DESKTOP
//Running in desktop mode HIL so CONTROLLOOP is off
#define CONTROLLOOP 0
#endif

#ifdef RPI
//Running HIL on RPI so CONTROLLOOP is on but MAIN LOOP IS OFF
#define CONTROLLOOP 1
#endif

#else //HIL
//Hardware in the loop is off so run everything
#define CONTROLLOOP 1
#endif

//Main Loop Functions
void renderloop(char* root_folder_name,int argc,char** argv);
void loop();

int main(int argc,char* argv[]) {

  //Initialize Random
  srand(time(NULL));

  //Print name of software
  printf("FASTKit Software version 42.0 \n");

  ////////////////////??CHECK FOR SUDO IF RUNNING IN AUTO MODE/////////
  #ifdef AUTO
  if (getuid()) {
    fprintf(stderr, "Not root. Please launch like this: sudo %s\n", argv[0]);
    exit(1);
  }
  #endif
  ///////////////////////////////////////////////////////////////////


  ////The main routine needs to grab the root file name//////
  char root_folder_name[256];
  if (argc > 1) {
    sprintf(root_folder_name,"%s","vehicles/");
    strcat(root_folder_name,argv[1]);
  } else {
    printf("Using Default Root Folder Name\n");
    sprintf(root_folder_name,"%s","vehicles/portalcube/");
  }
  //strcat(root_folder_name,"/");
  //printf("Root Folder Name = %s \n",root_folder_name);
  //////////////////////////////////////////////////////////

  //The Hardware block needs the root filename to run it's
  //initialization routine. Also send the number of signals to 
  //connect to PWM servos/ESCs
  hw.init(root_folder_name,control.NUMSIGNALS);

  //Initialize Controller
  control.init(hw.in_configuration_matrix);

  //Initialize Model if it's on
  #ifdef MODELING
  model.init(root_folder_name,hw.in_simulation_matrix,hw.in_configuration_matrix,argc,argv);
  #endif

  ////KICK OFF MAIN LOOP THREAD IF OPENGL IS ON
  #ifdef OPENGL_H 
  printf("Kicking off Main Loop as a thread \n");
  //When you have opengl running you need to kick this off as a thread
  boost::thread run(loop); 
  //Now there is a problem here. When you kick off the rendering environment and the Mainloop
  //above the code actually has 3 threads now. The MainLoop, the rendering environment and this main.cpp
  //In otherwords you need to create an infinite loop here
  while(1){cross_sleep(1);}; 
  #else
  //If you aren't rendering you just need to kick off the mainloop without a thread
  loop();
  #endif
}

//Main Loop
void loop() {
  //Enter into while loop while hardware and the model are ok
  int system_ok = system_check();
  double lastPRINTtime = 0;
  printf("Main Loop Begin \n");

  //Initialize the Timer if we're running in Software mode
  double initTime = 0;
  #ifdef MODELING
  initTime = -model.TIMESTEP;
  hw.send(-model.TIMESTEP,model.model_matrix,model.keyboardVars);
  #endif
  watch.init(initTime);
  //Run the hardware loop once just to initialize everything
  hw.loop(watch.currentTime,watch.elapsedTime,control.control_matrix);

  while (system_check()) {

    ///////////TIMING UPDATE/////////////////
    #ifdef REALTIME
    //Use real time clock
    //printf("Using Real time clock \n");
    watch.updateTime();
    #else
    //The system clock is updated by integrating the timestep by 1 timestep
    watch.incrementTime(model.TIMESTEP);
    #endif
    /////////////////////////////////////////

    //////////HARDWARE LOOP//////////////////
    //This runs as fast as possible
    //Need a routine that sends the model matrix to the hardware routine
    #ifdef MODELING
    hw.send(watch.currentTime,model.model_matrix,model.keyboardVars);
    //printf("hw.send %lf %lf %lf \n",hw.sense.orientation.roll,hw.sense.orientation.pitch,hw.sense.orientation.yaw);
    #endif

    //If We're running in HIL we need to run the HIL comms function    
    #ifdef HIL
    hw.hil();
    #endif
    
    //We only run the hardware / control loop if we're in SIMONLY, SIL, AUTO or HIL RPI
    //The control and hardware loops runs on the RPI 
    //The CONTROLLOOP variable is set in the preamble of this cpp file
    if (CONTROLLOOP) {
      hw.loop(watch.currentTime,watch.elapsedTime,control.control_matrix);
      //printf("hw.loop %lf %lf %lf \n",hw.sense.orientation.roll,hw.sense.orientation.pitch,hw.sense.orientation.yaw);
      /////////////////////////////////////////
      
      //////////CONTROL LOOP///////////////////
      //Runs as quickly as possible since the sensor states change 
      //as quickly as possible
      //Here's where things get weird though. If we're running in HIL mode
      //and we're on the desktop we don't run control.loop()
      control.loop(watch.currentTime,hw.rc.in.rx_array,hw.sense.sense_matrix);
      /////////////////////////////////////////
    } 

    ///////////MODELING LOOP/////////////////
    #ifdef MODELING
    model.loop(watch.currentTime,hw.rc.out.pwm_array);
    #endif
    /////////////////////////////////////////

    //PRINT TO STDOUT
    if (lastPRINTtime <= watch.currentTime) {
      lastPRINTtime+=hw.PRINTRATE;
      //Time
      printf("%lf %lf ",watch.currentTime,watch.elapsedTime);
      //First 4 RX signals
      hw.rc.in.printRCstate(-4);
      //Roll Pitch Yaw
      printf(" %lf %lf %lf ",hw.sense.orientation.roll,hw.sense.orientation.pitch,hw.sense.orientation.yaw);
      //PQR
      printf(" %lf %lf %lf ",hw.sense.orientation.roll_rate,hw.sense.orientation.pitch_rate,hw.sense.orientation.yaw_rate);
      //PWM Array
      hw.rc.out.print();
      //Newline
      printf("\n");
    }
  }
  printf("Main Loop End \n");
}

int system_check() {
  //Check to see if hardware systems are ok
  if (hw.ok) {
    //Hardware is ok but check modeling system if it is on
    #ifdef MODELING
    if (!model.ok) {
      //Modeling is bad so return 0
      return 0;
    }
    #endif
    //Hardware is good and if modeling is on it's ok too so return 1
    return 1;
  } else {
    //Hardware is bad so return 0
    return 0;
  }
}

