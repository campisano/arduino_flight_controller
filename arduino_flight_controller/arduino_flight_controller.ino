#define ESC_A 2
#define ESC_B 3
#define ESC_C 4
#define ESC_D 5

#define NO_MOTOR false
#define NO_SENSORS false

#define MAX_SIGNAL 2000
#define MIN_SIGNAL 1000

#define SOFT 0
#define HARD 1
#define FLIGHT_MODE SOFT

// Lib needed to connect to the motors by their ESCs
#include <Servo.h>

// Lib needed to connect to the MPU 6050
#include <Wire.h>
// Lib requestFrom
// https://github.com/jarzebski/Arduino-MPU6050
#include <MPU6050.h>

#include "Logger.h"

Servo motorA;
Servo motorB;
Servo motorC;
Servo motorD;

MPU6050 mpu;

Logger logger;

//MPU6050 address of I2C
const int MPU=0x68;

//Variaveis para armazenar valores dos sensores
int AcX,AcY,AcZ,Tmp,GyX,GyY,GyZ;

// Timers
unsigned long timer = 0;
float timeStep = 0.01;

// long timer, timerPrev;
// float compAngleX, compAngleY;

// Pitch, Roll and Yaw values
float pitchAngle = 0;
float rollAngle = 0;
float yawAngle = 0;

//We create variables for the time width values of each PWM input signal
unsigned long counter_1, counter_2, counter_3, counter_4, counter_5, counter_6, current_count;

//We create 4 variables to stopre the previous value of the input signal (if LOW or HIGH)
byte last_CH1_state, last_CH2_state, last_CH3_state, last_CH4_state, last_CH5_state, last_CH6_state;

//To store the 1000us to 2000us value we create variables and store each channel
int input_YAW;      //In my case channel 4 of the receiver and pin D12 of arduino
int input_PITCH;    //In my case channel 2 of the receiver and pin D9 of arduino
int input_ROLL;     //In my case channel 1 of the receiver and pin D8 of arduino
int input_THROTTLE; //In my case channel 3 of the receiver and pin D10 of arduino

int input_RIGHT;
int input_LEFT;

// PID
struct error {
  float yaw;
  float pitch;
  float roll;
};

struct PID {
  float yaw;
  float pitch;
  float roll;
};

struct error lastErrors;

int normalize(float value)
{
  value = map((int) value, 0, 1000, MIN_SIGNAL, MAX_SIGNAL);

  if (value > MAX_SIGNAL) {
      value = MAX_SIGNAL;
  } else if (value < MIN_SIGNAL) {
      value = MIN_SIGNAL;
  }

  return value;
}

void setup() {
  /*
   * Port registers allow for lower-level and faster manipulation of the i/o pins of the microcontroller on an Arduino board. 
   * The chips used on the Arduino board (the ATmega8 and ATmega168) have three ports:
     -B (digital pin 8 to 13)
     -C (analog input pins)
     -D (digital pins 0 to 7)
   
  //All Arduino (Atmega) digital pins are inputs when you begin...
  */  
   
  PCICR |= (1 << PCIE0);    //enable PCMSK0 scan                                                 
  PCMSK0 |= (1 << PCINT0);  //Set pin D8 trigger an interrupt on state change.
  PCMSK0 |= (1 << PCINT1);  //Set pin D9 trigger an interrupt on state change.
  PCMSK0 |= (1 << PCINT2);  //Set pin D10 trigger an interrupt on state change.
  PCMSK0 |= (1 << PCINT3);  //Set pin D11 trigger an interrupt on state change.
  PCMSK0 |= (1 << PCINT4);  //Set pin D12 trigger an interrupt on state change.
  PCMSK0 |= (1 << PCINT5);  //Set pin D13 trigger an interrupt on state change.
  
  // why 12?
  // PCMSK0 |= (1 << PCINT4);  //Set pin D12 trigger an interrupt on state change.  
  
  //Start the serial in order to see the result on the monitor
  //Remember to select the same baud rate on the serial monitor
  Serial.begin(57600);  
  // Serial.begin(9600);
  
  // input_THROTTLE = 0;
  initMotor();
  initSensors();
  // initRCReceiver();
  
  // Tempo para os motores armarem
  delay(5000);  
  logger.logStatus("Done....");
  logger.endChunk();
}

void loop() {
  // medir se precisa realocar isso como no exemplo
  // http://www.pitt.edu/~mpd41/Angle.ino
  // timerPrev = timer;  // the previous time is stored before the actual time read

  // https://github.com/jarzebski/Arduino-MPU6050/blob/master/MPU6050_gyro_pitch_roll_yaw/MPU6050_gyro_pitch_roll_yaw.ino
  timer = millis();  // actual time read

  readSensors();
  setMotor();

  float wait = (timeStep*1000) - (millis() - timer);
  logger.endChunk();
  
  // Wait to full timeStep period
  if (wait > 0) {
    delay((timeStep*1000) - (millis() - timer));
  }
}

void initMotor() {
  if (NO_MOTOR) {
    logger.logStatus("No motors");
    return;
  }

  int initValue = 0;

  logger.logStatus("Init motor 1");
  motorA.attach(ESC_A, MIN_SIGNAL, MAX_SIGNAL);
  motorA.writeMicroseconds(MIN_SIGNAL);

  logger.logStatus("Init motor 2");
  motorB.attach(ESC_B, MIN_SIGNAL, MAX_SIGNAL);
  motorB.writeMicroseconds(MIN_SIGNAL);

  logger.logStatus("Init motor 3");
  motorC.attach(ESC_C, MIN_SIGNAL, MAX_SIGNAL);
  motorC.writeMicroseconds(MIN_SIGNAL);

  logger.logStatus("Init motor 4");
  motorD.attach(ESC_D, MIN_SIGNAL, MAX_SIGNAL);
  motorD.writeMicroseconds(MIN_SIGNAL);
}

void setMotor() {
  int rxRoll;
  int rxPitch;
  int rxYaw;

  int rxLeft = map(input_LEFT, MIN_SIGNAL, MAX_SIGNAL, 0, 20);
  int rxRight = map(input_RIGHT, MIN_SIGNAL, MAX_SIGNAL, 0, 20);

  if (FLIGHT_MODE == SOFT)
  {
    rxRoll = map(input_ROLL, MIN_SIGNAL, MAX_SIGNAL, -45, 45);
    rxPitch = map(input_PITCH, MIN_SIGNAL, MAX_SIGNAL, -45, 45);   
    rxYaw = map(input_YAW, MIN_SIGNAL, MAX_SIGNAL, -180, 180);
  }
  else
  {
    rxRoll = map(input_ROLL, MIN_SIGNAL, MAX_SIGNAL, -180, 180);
    rxPitch = map(input_PITCH, MIN_SIGNAL, MAX_SIGNAL, -180, 180);
    rxYaw = map(input_YAW, MIN_SIGNAL, MAX_SIGNAL, -270, 270);
  }

  logger.logVariable("rxYaw", rxYaw);
  logger.logVariable("rxPitch", rxPitch);
  logger.logVariable("rxRoll", rxRoll);

  logger.logVariable("rxLeft", rxLeft);
  logger.logVariable("rxRight", rxRight);

  //Adjust difference - MIN_SIGNAL
  int throttle = map(input_THROTTLE, MIN_SIGNAL, MAX_SIGNAL, 0, 1000);
  float cmd_motA = throttle;
  float cmd_motB = throttle;
  float cmd_motC = throttle;
  float cmd_motD = throttle;

  logger.logVariable("throttle", throttle);
  
  //Somatório dos erros
  struct error sError = { .yaw = 0, .pitch = 0, .roll = 0};

  //Valor desejado - angulo atual
  struct error anguloAtual = { .yaw = yawAngle, .pitch = pitchAngle, .roll = rollAngle};
  // struct error anguloAtual = { .yaw = 0, .pitch = compAngleY, .roll = compAngleX};
  struct error anguloDesejado = { .yaw = rxYaw, .pitch = rxPitch, .roll = rxRoll};

  struct error errors = { 
      .yaw = anguloAtual.yaw - anguloDesejado.yaw,
      .pitch = anguloAtual.pitch - anguloDesejado.pitch,
      .roll = anguloAtual.roll - anguloDesejado.roll
  };
  
  struct error deltaError;

  //Variáveis de ajuste
  struct PID Kp = { .yaw = 1.5, .pitch = 5, .roll = 5};
  struct PID Ki = { .yaw = 0, .pitch = 0, .roll = 0};
  struct PID Kd = { .yaw = rxLeft, .pitch = rxRight, .roll = rxRight};
  
  if (throttle > 0) {
      // Calculate sum of errors : Integral coefficients
      sError.yaw += errors.yaw;
      sError.pitch += errors.pitch;
      sError.roll += errors.roll;

      // Calculate error delta : Derivative coefficients
      deltaError.yaw = errors.yaw - lastErrors.yaw;
      deltaError.pitch = errors.pitch - lastErrors.pitch;
      deltaError.roll = errors.roll - lastErrors.roll;

      // Save current error as lastErr for next time
      lastErrors.yaw = errors.yaw;
      lastErrors.pitch = errors.pitch;
      lastErrors.roll = errors.roll;

      // Yaw - Lacet (Z axis)
      int yaw_PID = (errors.yaw * Kp.yaw + sError.yaw * Ki.yaw + deltaError.yaw * Kd.yaw);
      cmd_motA -= yaw_PID;
      cmd_motB += yaw_PID;
      cmd_motC += yaw_PID;
      cmd_motD -= yaw_PID;

      // Pitch - Tangage (Y axis)
      int pitch_PID = (errors.pitch * Kp.pitch + sError.pitch * Ki.pitch + deltaError.pitch * Kd.pitch);
      cmd_motA += pitch_PID;
      cmd_motB += pitch_PID;
      cmd_motC -= pitch_PID;
      cmd_motD -= pitch_PID;

      // Roll - Roulis (X axis)
      int roll_PID = (errors.roll * Kp.roll + sError.roll * Ki.roll + deltaError.roll * Kd.roll);
      cmd_motA -= roll_PID;
      cmd_motB += roll_PID;
      cmd_motC -= roll_PID;
      cmd_motD += roll_PID;
  }

  logger.logVariable("Motor_A", cmd_motA);
  logger.logVariable("Motor_B", cmd_motB);
  logger.logVariable("Motor_C", cmd_motC);
  logger.logVariable("Motor_D", cmd_motD);

  // Apply speed for each motor
  if (NO_MOTOR) {
    logger.logStatus("No motors to set");
    return;
  }
  motorA.writeMicroseconds(normalize(cmd_motA));
  motorB.writeMicroseconds(normalize(cmd_motB));
  motorC.writeMicroseconds(normalize(cmd_motC));
  motorD.writeMicroseconds(normalize(cmd_motD));

  // if (value < MIN_SIGNAL) {
  //   value = MIN_SIGNAL;
  // } else if (value > MAX_SIGNAL) {
  //   value = MAX_SIGNAL;
  // }

  // // Serial.print("Set motor 1: ");
  // // logger.logStatus(value);
  // motor1.writeMicroseconds(value);
  // // Serial.print("Set motor 2: ");
  // // logger.logStatus(value);
  // motor2.writeMicroseconds(value);
  // // Serial.print("Set motor 3: ");
  // // logger.logStatus(value);
  // motor3.writeMicroseconds(value);
  // // Serial.print("Set motor 4: ");
  // // logger.logStatus(value);
  // motor4.writeMicroseconds(value);
}

void initSensors() {
  if (NO_SENSORS) {
    logger.logStatus("No sensors");
    return;
  }

  // Initialize MPU6050
  while(!mpu.begin(MPU6050_SCALE_2000DPS, MPU6050_RANGE_2G))
  {
    logger.logStatus("Could not find a valid MPU6050 sensor, check wiring!");
    delay(500);
  }

  // Calibrate gyroscope. The calibration must be at rest.
  // If you don't want calibrate, comment this line.
  mpu.calibrateGyro();
  
  // Set threshold sensivty. Default 3.
  // If you don't want use threshold, comment this line or set 0.
  mpu.setThreshold(3);

  // Wire.begin();
  // Wire.beginTransmission(MPU);
  // Wire.write(0x6B);
  // Wire.write(0); 
  // Wire.endTransmission(true);
  // delay(100);

  // //setup starting angle
  // //1) collect the data
  // Wire.beginTransmission(MPU);
  // Wire.write(0x3B);  // starting with register 0x3B (ACCEL_XOUT_H)
  // Wire.endTransmission(false);
  // Wire.requestFrom(MPU,6,true);  // request a total of 14 registers
  // AcX=Wire.read()<<8|Wire.read();  // 0x3B (ACCEL_XOUT_H) & 0x3C (ACCEL_XOUT_L)     
  // AcY=Wire.read()<<8|Wire.read();  // 0x3D (ACCEL_YOUT_H) & 0x3E (ACCEL_YOUT_L)
  // AcZ=Wire.read()<<8|Wire.read();  // 0x3F (ACCEL_ZOUT_H) & 0x40 (ACCEL_ZOUT_L)
  // // Tmp=Wire.read()<<8|Wire.read();  // 0x41 (TEMP_OUT_H) & 0x42 (TEMP_OUT_L)
  
  // Wire.beginTransmission(MPU);
  // Wire.write(0x43);  // starting with register 0x3B (ACCEL_XOUT_H)
  // Wire.endTransmission(false);
  // Wire.requestFrom(MPU,6,true);  // request a total of 14 registers
  // GyX=Wire.read()<<8|Wire.read();  // 0x43 (GYRO_XOUT_H) & 0x44 (GYRO_XOUT_L)
  // GyY=Wire.read()<<8|Wire.read();  // 0x45 (GYRO_YOUT_H) & 0x46 (GYRO_YOUT_L)
  // GyZ=Wire.read()<<8|Wire.read();  // 0x47 (GYRO_ZOUT_H) & 0x48 (GYRO_ZOUT_L)

  // //2) calculate pitch and roll
  // double roll = atan2(AcY, AcZ)*RAD_TO_DEG;
  // double pitch = atan2(-AcX, AcZ)*RAD_TO_DEG;

  // //3) set the starting angle to this pitch and roll
  // // double gyroXangle = roll;
  // // double gyroYangle = pitch;
  // compAngleX = roll;
  // compAngleY = pitch;

  // //start a timer
  // timer = micros();
}

void readSensors() {
  if (NO_SENSORS) {
    return;
  }

  // Read normalized values
  Vector norm = mpu.readNormalizeGyro();

  // Calculate Pitch, Roll and Yaw
  pitchAngle = pitchAngle + norm.YAxis * timeStep;
  rollAngle = rollAngle + norm.XAxis * timeStep;
  yawAngle = yawAngle + norm.ZAxis * timeStep;

  // Wire.beginTransmission(MPU);
  // Wire.write(0x3B);  // starting with register 0x3B (ACCEL_XOUT_H)
  // Wire.endTransmission(false);
  // Wire.requestFrom(MPU,6,true);  // request a total of 14 registers
  // AcX=Wire.read()<<8|Wire.read();  // 0x3B (ACCEL_XOUT_H) & 0x3C (ACCEL_XOUT_L)     
  // AcY=Wire.read()<<8|Wire.read();  // 0x3D (ACCEL_YOUT_H) & 0x3E (ACCEL_YOUT_L)
  // AcZ=Wire.read()<<8|Wire.read();  // 0x3F (ACCEL_ZOUT_H) & 0x40 (ACCEL_ZOUT_L)
  // // Tmp=Wire.read()<<8|Wire.read();  // 0x41 (TEMP_OUT_H) & 0x42 (TEMP_OUT_L)
  
  // Wire.beginTransmission(MPU);
  // Wire.write(0x43);  // starting with register 0x3B (ACCEL_XOUT_H)
  // Wire.endTransmission(false);
  // Wire.requestFrom(MPU,6,true);  // request a total of 14 registers
  // GyX=Wire.read()<<8|Wire.read();  // 0x43 (GYRO_XOUT_H) & 0x44 (GYRO_XOUT_L)
  // GyY=Wire.read()<<8|Wire.read();  // 0x45 (GYRO_YOUT_H) & 0x46 (GYRO_YOUT_L)
  // GyZ=Wire.read()<<8|Wire.read();  // 0x47 (GYRO_ZOUT_H) & 0x48 (GYRO_ZOUT_L)


  // double dt = (double)(timer - timerPrev) / 1000000; //This line does three things: 1) stops the timer, 2)converts the timer's output to seconds from microseconds, 3)casts the value as a double saved to "dt".

  // //the next two lines calculate the orientation of the accelerometer relative to the earth and convert the output of atan2 from radians to degrees
  // //We will use this data to correct any cumulative errors in the orientation that the gyroscope develops.
  // double roll = atan2(AcY, AcZ)*RAD_TO_DEG;
  // double pitch = atan2(-AcX, AcZ)*RAD_TO_DEG;

  // //The gyroscope outputs angular velocities.  To convert these velocities from the raw data to deg/second, divide by 131.  
  // //Notice, we're dividing by a double "131.0" instead of the int 131.
  // double gyroXrate = GyX/131.0;
  // double gyroYrate = GyY/131.0;

  // //THE COMPLEMENTARY FILTER
  // //This filter calculates the angle based MOSTLY on integrating the angular velocity to an angular displacement.
  // //dt, recall, is the time between gathering data from the MPU6050.  We'll pretend that the 
  // //angular velocity has remained constant over the time dt, and multiply angular velocity by 
  // //time to get displacement.
  // //The filter then adds a small correcting factor from the accelerometer ("roll" or "pitch"), so the gyroscope knows which way is down. 
  // compAngleX = 0.99 * (compAngleX + gyroXrate * dt) + 0.01 * roll; // Calculate the angle using a Complimentary filter
  // compAngleY = 0.99 * (compAngleY + gyroYrate * dt) + 0.01 * pitch; 

  logger.logVariable("yawAngle", yawAngle);
  logger.logVariable("pitchAngle", pitchAngle);
  logger.logVariable("rollAngle", rollAngle);
  
  /*Now we have our angles in degree and values from -10º0 to 100º aprox*/

  //Envia valor da temperatura para a serial e o LCD
  //Calcula a temperatura em graus Celsius
  // Serial.print("Tmp="); Serial.print(Tmp/340.00+36.53);
}

// A special thanks to Electronoobs
// https://www.youtube.com/watch?v=if9LZTcy_uk&index=1&list=LL70ziqu5-D8VBDmxa5HUYwg
//This is the interruption routine
//----------------------------------------------

ISR(PCINT0_vect){
  //First we take the current count value in micro seconds using the micros() function
    
  current_count = micros();
  ///////////////////////////////////////Channel 1
  if(PINB & B00000001){                              //We make an AND with the pin state register, We verify if pin 8 is HIGH???
    if(last_CH1_state == 0){                         //If the last state was 0, then we have a state change...
      last_CH1_state = 1;                            //Store the current state into the last state for the next loop
      counter_1 = current_count;                     //Set counter_1 to current value.
    }
  }
  else if(last_CH1_state == 1){                      //If pin 8 is LOW and the last state was HIGH then we have a state change      
    last_CH1_state = 0;                              //Store the current state into the last state for the next loop
    input_ROLL = current_count - counter_1;   //We make the time difference. Channel 1 is current_time - timer_1.
  }
  ///////////////////////////////////////Channel 2
  if(PINB & B00000010 ){                             //pin D9 -- B00000010                                              
    if(last_CH2_state == 0){                                               
      last_CH2_state = 1;                                                   
      counter_2 = current_count;                                             
    }
  }
  else if(last_CH2_state == 1){                                           
    last_CH2_state = 0;                                                     
    input_PITCH = current_count - counter_2;                             
  }
  ///////////////////////////////////////Channel 3
  if(PINB & B00000100 ){                             //pin D10 - B00000100                                         
    if(last_CH3_state == 0){                                             
      last_CH3_state = 1;                                                  
      counter_3 = current_count;                                               
    }
  }
  else if(last_CH3_state == 1){                                             
    last_CH3_state = 0;                                                    
    input_THROTTLE = current_count - counter_3;                            

  }
  ///////////////////////////////////////Channel 4
  if(PINB & B00001000 ){                             //pin D11  -- B00001000
    if(last_CH4_state == 0){                                               
      last_CH4_state = 1;                                                   
      counter_4 = current_count;                                              
    }
  }
  else if(last_CH4_state == 1){                                             
    last_CH4_state = 0;                                                  
    input_YAW = current_count - counter_4;                            
  }
  ///////////////////////////////////////Channel 5
  if(PINB & B00010000 ){                             //pin D12  -- B00010000
    if(last_CH5_state == 0){                                               
      last_CH5_state = 1;                                                   
      counter_5 = current_count;                                              
    }
  }
  else if(last_CH5_state == 1){                                             
    last_CH5_state = 0;                                                  
    input_LEFT = current_count - counter_5;                            
  }
  ///////////////////////////////////////Channel 6
  if(PINB & B00100000 ){                             //pin D13  -- B00100000
    if(last_CH6_state == 0){                                               
      last_CH6_state = 1;                                                   
      counter_6 = current_count;                                              
    }
  }
  else if(last_CH6_state == 1){                                             
    last_CH6_state = 0;                                                  
    input_RIGHT = current_count - counter_6;                            
  }   
}
