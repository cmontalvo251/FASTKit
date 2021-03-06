//This is a portal cube template. You must adhere to these standards if you write your
//own

#include "controller.h"

controller::controller() {
};

void controller::init(MATLAB in_configuration_matrix) {
  control_matrix.zeros(NUMSIGNALS,1,"Control Signals"); //The standards must be TAERA1A2A3A4
  set_defaults();
  printf("Controller Received Configuration Matrix \n");
  //in_configuration_matrix.disp();
  CONTROLLER_FLAG = in_configuration_matrix.get(11,1);
  printf("Controller Setup \n");
}

void controller::set_defaults() {
  control_matrix.set(1,1,OUTMIN);
  control_matrix.set(2,1,OUTMID);
  control_matrix.set(3,1,OUTMID);
  control_matrix.set(4,1,OUTMID);
}

void controller::print() {
  for (int i = 1;i<=NUMSIGNALS;i++) {
    printf("%d ",int(control_matrix.get(i,1)));
  }
}

void controller::loop(double currentTime,int rx_array[],MATLAB sense_matrix) {
  //The sensor matrix is a 29x1. See sensors.cpp for list of sensors
  //At a minimum you need to just feed through the rxcomms into the ctlcomms
  //Which means you can't have more control signals than receiver signals

  //Default Control Signals
  set_defaults();

  //I want to keep track of timeElapsed so that I can run integrators
  //and compute derivates
  elapsedTime = currentTime - lastTime;
  lastTime = currentTime;

  //First extract the relavent commands from the receiver.
  throttle = rx_array[0];
  aileron = rx_array[1];
  elevator = rx_array[2];
  rudder = rx_array[3];
  autopilot = rx_array[4];
  int icontrol = 0;

  //Check for user controlled
  if (CONTROLLER_FLAG == -1) {
    if (autopilot > STICK_MID) {
      icontrol = 1;
    } else {
      icontrol = 0;
    }
  } else {
    icontrol = CONTROLLER_FLAG;
  }

  //Aircraft Control cases
  // 0 = fully manual
  // 1 = roll (rudder mixing) and pitch control
  // 2 = roll (rudder mixing), pitch, and velocity control
  // 3 = roll (rudder), velocity and altitude control
  // 4 = velocity, altitude and heading control
  // 5 = velocity, altitude and waypoint control
  //Initialize commands
  roll_command = -99;
  pitch_command = -99;
  velocity_command = 20; //Hardcode to 20?
  altitude_command = 100; //Hardcode to 100?

  switch (icontrol) {
    case 5:
      //velocity, altitude and waypoint control
      printf("Waypoint + ");
    case 4:
      //velocity, altitude and heading control
      printf("Heading + ");
    case 3:
      //roll (rudder mixing), velocity and altitude control
      //printf("Altitude + ");
      AltitudeLoop(sense_matrix);
    case 2:
      //roll (rudder mixing), pitch and velocity control
      //printf("Velocity + ");
      VelocityLoop(sense_matrix);
    case 1:
      //roll (rudder mixing) and pitch control
      //Run the Innerloop
      //with inner loop guidance 
      //printf("INNER LOOP CONTROL -- ");
      //Check for inner loop only guidance
      if (roll_command == -99) {
        roll_command = (aileron-STICK_MID)*50.0/((STICK_MAX-STICK_MIN)/2.0);
      }
      if (pitch_command == -99) {
        pitch_command = -(elevator-STICK_MID)*15.0/((STICK_MAX-STICK_MIN)/2.0);
      }
      InnerLoop(sense_matrix);
    case 0:
      //printf("Passing signals \n");
      //Pass the receiver signals to the control_matrix and then break
      control_matrix.set(1,1,throttle);
      control_matrix.set(2,1,aileron);
      control_matrix.set(3,1,elevator);
      control_matrix.set(4,1,rudder);
      //control_matrix.disp();
      break;
  }
}

void controller::AltitudeLoop(MATLAB sense_matrix) {
  //Probably a good idea to use pressure altitude but might need to use 
  //GPS altitude if the barometer isn't good or perhaps even a KF approach
  //Who knows. Just simulating this now.
  double altitude = sense_matrix.get(28,1);  
  //Initialize altitude_dot to zero
  double altitude_dot = 0;
  //If altitude_prev has been set compute a first order derivative
  if (altitude_prev != -999) {
    altitude_dot = (altitude - altitude_prev) / elapsedTime;
  }
  //Then set the previous value
  altitude_prev = altitude;

  //Compute Pitch Command in Degrees
  double kp = 0.2;
  double kd = 0.1;
  pitch_command = kp*(altitude_command - altitude) + kd*(0-altitude_dot);
  pitch_command = CONSTRAIN(pitch_command,-45,45);

  //printf("T, ALT, ALT DOT = %lf %lf %lf \n",lastTime,altitude,altitude_dot);  
}

void controller::VelocityLoop(MATLAB sense_matrix) {
  //printf("------\n");
  //sense_matrix.disp();
  double u = sense_matrix.get(7,1);
  double velocityerror = velocity_command - u;
  //printf("Sense U = %lf \n",u);
  //printf("Velocity Error = %lf \n",velocityerror);
  double kp = 40.0;
  double ki = 3.0;
  throttle = OUTMIN + kp*velocityerror + ki*velocityint;
  throttle = CONSTRAIN(throttle,OUTMIN,OUTMAX);
  //Integrate but prevent integral windup
  if ((throttle > OUTMIN) && (throttle < OUTMAX)) {
    velocityint += elapsedTime*velocityerror;
  }
}

void controller::InnerLoop(MATLAB sense_matrix) {
    //Get States
    double roll = sense_matrix.get(4,1);
    double pitch = sense_matrix.get(5,1);
    //printf("PITCH = %lf \n",pitch);
    double roll_rate = sense_matrix.get(10,1); //For SIL/SIMONLY see Sensors.cpp
    double pitch_rate = sense_matrix.get(11,1); //These are already in deg/s
    //printf("PQR Rate in Controller %lf %lf %lf \n",roll_rate,pitch_rate,yaw_rate);
    double kpa = 10.0;
    double kda = 2.0;
    aileron = kpa*(roll-roll_command) + kda*(roll_rate);
    double kpe = 25.0;
    double kde = 1.0;
    elevator = kpe*(pitch-pitch_command) + kde*(pitch_rate);

    //Rudder signal will be proportional to aileron
    double kr = 0.2;
    rudder = kr*aileron;

    //CONSTRAIN
    rudder = CONSTRAIN(rudder,-500,500) + OUTMID;
    aileron = -CONSTRAIN(aileron,-500,500) + OUTMID;
    elevator = CONSTRAIN(elevator,-500,500) + OUTMID;
}
