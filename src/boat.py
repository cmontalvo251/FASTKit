#############################################
#
#  FASTPy - Facility for Aerial Systems and Technology
#  Kit in Python for ROBOBOAT
#
#  SKID STEER BOAT
#  Created: Spring 2022
#  Primary Author: Julia Nelson
#  Secondary Author: Maxwell Cobar
#  TERTIARY AUTHOR: Carlos Montalvo
#
################################################


#Add in all needed libraries and modules
import sys, time

sys.path.append('../hardware/Util')
import util

sys.path.append('../hardware/GPS/')
import gps

sys.path.append('../hardware/IMU/')
import mpu9250

sys.path.append('../helper/Datalogger')
import datalogger

sys.path.append('../hardware/LED')
import leds

sys.path.append('../hardware/RCIO/Python')
import rcinput
import pwm

import numpy as np
#Create a time for elapsed time
StartTime = time.time()

#Make sure Ardupilot is off
util.check_apm()

#Setup datalogger
logger = datalogger.Datalogger()
logger.findfile(sys.argv[1])
logger.open()
#create an array for data
#arm,throttlerc,yawrc,lat,lon,alt,velocity,roll,pitch,heading
outdata = np.zeros(10)

#Setup GPS
gps_llh = gps.GPS()
gps_llh.initialize()

#Setup IMU
imu = mpu9250.MPU9250()
imu.initialize()

#Setup RCIO
rcin = rcinput.RCInput()
i = 0
num_channels = 9

#Setup LED
led = leds.Led()
led.setColor('Yellow')

#Setup Servos
SERVO_MIN = 0.995 #ms
SERVO_MID = 1.504 #ms
SERVO_MAX = 2.010 #ms
PWM_OUTPUT = [0,1] #Servo Rail Spots
print('PWM Channels: ',PWM_OUTPUT)

#Throttle - PWM Channel 1
pwm1 = pwm.PWM(PWM_OUTPUT[0])
pwm1.initialize()
pwm1.set_period(50)
pwm1.enable()
#Steering - PWM Channel 2
pwm2 = pwm.PWM(PWM_OUTPUT[1])
pwm2.initialize()
pwm2.set_period(50)
pwm2.enable()

#Short break to build suspense
time.sleep(1)

def SATURATE(pwm_in):
    #Add Saturation Block to ensure servo safety
    if(pwm_in < SERVO_MIN):
        pwm_in = SERVO_MIN
    if(pwm_in > SERVO_MAX):
        pwm_in = SERVO_MAX
    return pwm_in

#This runs on repeat until code is killed
while (True):
    RunTime = time.time()
    elapsedTime = RunTime - StartTime
    #print(elapsedTime)
    
    #Read in receiver commands
    period = []
    for i in range(num_channels):
        value = rcin.read(i)
        period.append(value)
    #print period

    #Turn receiver commands to floats
    throttlerc = float(period[0])/1000.
    rollrc = float(period[1])/1000.
    pitchrc = float(period[2])/1000.
    yawrc = float(period[3])/1000.
    armswitch = float(period[4])/1000.
    #print(throttlerc,rollrc,pitchrc,yawrc,armswitch)

    #Get GPS update
    if(elapsedTime > 1.0):
        StartTime = time.time()
        gps_llh.update()
        print(throttlerc,rollrc,gps_llh.longitude,gps_llh.latitude,gps_llh.altitude)

    #Set arm switch up for safety reasons
    if(armswitch < 1.495):
        led.setColor('Red')
        pwm1.set_duty_cycle(SERVO_MIN)
        pwm2.set_duty_cycle(SERVO_MID)
        #print('Disarmed for safety')
    elif(1.495 < armswitch < 1.995):
        led.setColor('Green')

        ##MIX THE CHANNELS
        throttle_left = throttlerc + 1.0*(rollrc - SERVO_MID)
        throttle_right = throttlerc - 1.0*(rollrc - SERVO_MID)

        throttle_left = SATURATE(throttle_left)
        throttle_right = SATURATE(throttle_right)

        #Send throttlerc to servo
        pwm1.set_duty_cycle(throttle_left)
        pwm2.set_duty_cycle(throttle_right)

    elif(armswitch > 1.995):
        led.setColor('Blue')
        pwm1.set_duty_cycle(SERVO_MIN)
        pwm2.set_duty_cycle(SERVO_MID)
        #print('Autonomous Control')
    #print(armswitch,throttlerc,yawrc)

    #Log data
    outdata[0] = armswitch
    outdata[1] = throttlerc
    outdata[2] = yawrc
    #outdata[]
    logger.println(outdata)
