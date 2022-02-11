#include "modeling.h"

//Constructor
modeling::modeling() {
}

//Initialization 
void modeling::init(char root_folder_name[],MATLAB in_simulation_matrix,MATLAB in_configuration_matrix) {
  printf("Modeling Root Folder Name = %s \n",root_folder_name);
  //in_simulation_matrix.disp();

  ////////EXTRACT SIMULATION MATRIX VARIABLES
  TFINAL=in_simulation_matrix.get(1,1);
  TIMESTEP=in_simulation_matrix.get(2,1);

  //Initialize Matrices
  NUMVARS = 30; //Make sure this is the same as the sense states
  model_matrix.zeros(NUMVARS,1,"Model Matrix"); 
  NUMINTEGRATIONSTATES=13; //Only integrating 13 states for a 6DOF system
  integration_matrix.zeros(NUMINTEGRATIONSTATES,1,"Integration Matrix");

  //Set initial conditions of integration matrix
  for (int i = 1;i<NUMINTEGRATIONSTATES;i++) {
    integration_matrix.set(i,1,in_simulation_matrix.get(i+2,1));
  }
  //integration_matrix.disp();
  //PAUSE();

  //Copy the states over to the model matrix for opengl and hardware loops
  model_matrix.vecset(1,NUMINTEGRATIONSTATES,integration_matrix,1);

  //Initialize Integrator
  integrator.init(13,TIMESTEP);
  //Then initialize the Initial conditions
  integrator.set_ICs(integration_matrix);
}

///Loop
void modeling::loop(double currentTime,int pwm_array[]) {
  //printf("Modeling Loop \n");
  if (currentTime > TFINAL) {
    //Simulation is over.
    ok = 0;
    //break
    return;
  }

  //Run RK4 Loop
  rk4step(pwm_array);

  //Copy the states over to the model matrix for opengl and hardware loops
  model_matrix.vecset(1,13,integration_matrix,1);

  //Add sensor noise if needed
}

void modeling::rk4step(int pwm_array[]) {
  //Integrate one timestep by running a 4 step RK4 integrator
  for (int i = 1;i<=4;i++){
    vehicle.Derivatives(t,integrator.StateDel,integrator.k);
    integrator.integrate(i);
  }
  //Integrate time
  t += TIMESTEP;
}


void modeling::Derivatives(double t,MATLAB State,MATLAB k) {
  //The Derivatives are vehicle independent except for the 
  //forces and moments

  //Actuator Error Model is a simple first order filter
  /*if (NUMACTUATORS > 0) {
    ///Get the error actuator state
    double val = 0;
    for (int i = 0;i<NUMACTUATORS;i++) {  
      val = actuatorState.get(i+1,1)*actuatorErrorPercentage.get(i+1,1);
      actuatorError.set(i+1,1,val);  
    }
    //Integrate Actuator Dynamics
    //input will be ctlcomms and the output will be actuator_state
    for (int i = 0;i<NUMACTUATORS;i++) {
      k.set(i+NUMSTATES+1,1,actuatorTimeConstants.get(i+1,1)*(rcout.pwmcomms[i] - actuatorState.get(i+1,1)));
    }
  } else {
    //Otherwise just pass through the ctlcomms
    actuatorError.overwrite(ctl.ctlcomms);
    }*/

  ////////////////////KINEMATICS///////////////

  //Extract individual States from State Vector
  cgdotB.vecset(1,3,State,8);
  q0123.vecset(1,4,State,4);
  pqr.vecset(1,3,State,11);
  double q0 = q0123.get(1,1);
  double q1 = q0123.get(2,1);
  double q2 = q0123.get(3,1);
  double q3 = q0123.get(4,1);
  double p = pqr.get(1,1);
  double q = pqr.get(2,1);
  double r = pqr.get(3,1);
  //This is used to rotate things from body to inertial and vice versa
  ine2bod321.L321(q0123, 1);

  ///Linear Kinematics (xyzdot = TIB uvw)
  ine2bod321.rotateBody2Inertial(cgdotI,cgdotB);
  k.vecset(1,3,cgdotI,1);

  ///Rotational Kinematics (Quaternion Derivatives)
  k.set(4,1,(-p*q1-q*q2-r*q3)/2.0);
  k.set(5,1,(p*q0+r*q2-q*q3)/2.0);
  k.set(6,1,(q*q0-r*q1+p*q3)/2.0);
  k.set(7,1,(r*q0+q*q1-p*q2)/2.0);

  ////////////////FORCE AND MOMENT MODEL///////////////////////

  //Gravity Model and Magnetic Field model
  env.gravitymodel(State);
  env.groundcontactmodel(State,k);
  env.getCurrentMagnetic(t,State);
  //The getCurrentMagnetic routine populates env.BVECINE which is the magnetometer
  //value in the inertial frame. we need to rotate this to the body frame and
  //convert to teslas
  ine2bod321.rotateInertial2Body(BVECB,env.BVECINE);
  BVECB_Tesla.overwrite(BVECB);
  BVECB_Tesla.mult_eq(1e-9);
  //Send to environment model
  env.BVECB_Tesla.overwrite(BVECB_Tesla);

  //External Forces Model
  //Send the external forces model the actuator_state instead of the ctlcomms
  extforces.ForceMoment(t,State,k,actuatorError,env);

  //Add Up Forces and Moments
  FTOTALI.overwrite(env.FGRAVI); //add gravity 

  //Rotate Forces to body frame
  ine2bod321.rotateInertial2Body(FGNDB,env.FGNDI);
  ine2bod321.rotateInertial2Body(FTOTALB,FTOTALI);

  //Add External Forces and Moments
  //FTOTALB.disp();
  //env.FGRAVI.disp();
  FTOTALB.plus_eq(extforces.FB);
  FTOTALB.plus_eq(FGNDB);
  //extforces.FB.disp();
  //FGNDB.disp();
  //FTOTALB.disp();  
  //if (FTOTALB.get(1,1) > 0) {
  //  PAUSE();
  //}

  //Translational Dynamics
  Kuvw_pqr.cross(pqr,cgdotB);
  FTOTALB.mult_eq(1.0/m); 
  uvwdot.minus(FTOTALB,Kuvw_pqr); 
  k.vecset(8,10,uvwdot,1);

  //Rotate Ground Contact Friction to Body
  //env.MGNDI.disp();
  ine2bod321.rotateInertial2Body(MGNDB,env.MGNDI);
  //MGNDB.disp();
  //PAUSE();

  //Moments vector
  MTOTALB.overwrite(extforces.MB);
  MTOTALB.plus_eq(MGNDB);

  ///Rotational Dynamics
  //pqrskew = [0 -r q;r 0 -p;-q p 0];  
  //pqrdot = Iinv*(LMN-pqrskew*I*pqr);
  //pqr.disp();
  I_pqr.mult(I,pqr);
  //I.disp();
  //I_pqr.disp();
  pqrskew_I_pqr.cross(pqr,I_pqr);
  //pqrskew_I_pqr.disp();
  MTOTALB.minus_eq(pqrskew_I_pqr);
  //LMN.disp();
  //Iinv.disp();
  pqrdot.mult(Iinv,MTOTALB);
  
  //Save pqrdot
  k.vecset(11,13,pqrdot,1);

  ////////////////////////////////////////////////////////////////
}
