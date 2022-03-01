#ifndef CONTROLLER_H
#define CONTROLLER_H

//This is another class that is craft dependent as as such
//must adhere to specific standards

#include <MATLAB/MATLAB.h>
#include <RCIO/RCIO.h> //this is for STICK values

class controller {
private:
  double elapsedTime = 0,lastTime=0; //These are used to keep track of time elapsed.
  int CONTROLLER_FLAG = -99;
  void set_defaults();
public:
  int NUMSIGNALS=8; //motor 1,2,3,4,5,6,7,8
  MATLAB control_matrix; //This is a vector of TAERA1A2 in PWM signals
  void loop(double currentTime,int rx_array[],MATLAB sense_matrix);
  void init(MATLAB in_configuration_matrix);
  void print();
  controller(); //constructor
};

#endif
