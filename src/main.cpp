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
//Check for realtime clock
#ifndef SIMONLY
#define REALTIME
#endif

//The Hardware environment is always running
#include <hardware/src/hardware.h>
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


int main(int argc,char* argv[]) {
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
  model.init(root_folder_name,hw.in_simulation_matrix,hw.in_configuration_matrix);
  #endif

  //Begin main loop but run as a separate function in anticipation of threading
  loop();
}

//Main Loop
void loop() {
  //Enter into while loop while hardware and the model are ok
  int system_ok = system_check();
  double lastPRINTtime = 0;
  printf("Main Loop Begin \n");
  
  while (system_check()) {

    ///////////TIMING UPDATE/////////////////
    watch.updateTime();
    /////////////////////////////////////////

    //////////HARDWARE LOOP//////////////////
    //This runs as fast as possible
    //Need a routine that sends the model matrix to the hardware routine
    #ifdef MODELING
    hw.send(model.model_matrix);
    #endif
    hw.loop(watch.currentTime,watch.elapsedTime,control.control_matrix);
    /////////////////////////////////////////

    //////////CONTROL LOOP///////////////////
    //Runs as quickly as possible since the sensor states change 
    //as quickly as possible
    control.loop(watch.currentTime,hw.rc.in.rx_array,hw.sense.sense_matrix);
    /////////////////////////////////////////

    ///////////MODELING LOOP/////////////////
    #ifdef MODELING
    model.loop(watch.currentTime,hw.rc.out.pwm_array);
    #endif
    /////////////////////////////////////////

    //PRINT TO STDOUT
    if (lastPRINTtime < watch.currentTime) {
      lastPRINTtime+=hw.PRINTRATE;
      //Time
      printf("%lf ",watch.currentTime);
      //First 4 RX signals
      hw.rc.in.printRCstate(-4);
      //Roll Pitch Yaw
      printf("%lf %lf %lf ",hw.sense.orientation.roll,hw.sense.orientation.pitch,hw.sense.orientation.yaw);
      //PQR
      printf("%lf %lf %lf ",hw.sense.orientation.roll_rate,hw.sense.orientation.pitch_rate,hw.sense.orientation.yaw_rate);
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
