//ARDUINO LINE FOLLOWER CAR USING L298N MOTOR DRIVE

int ENA = 9; //ENA connected to digital pin 9
int ENB = 3; //ENB connected to digital pin 3
int MOTOR_A1 = 7; // MOTOR_A1 connected to digital pin 7
int MOTOR_A2 = 6; // MOTOR_A2 connected to digital pin 6
int MOTOR_B1 = 5; // MOTOR_B1 connected to digital pin 5
int MOTOR_B2 = 4; // MOTOR_B2 connected to digital pin 4


int RIGHT = A0; // RIGHT sensor connected to analog pin A0
int LEFT = A1; // LEFT sensor connected to analog pin A1


// the setup function runs once when you press reset or power the board
void setup() {
  
pinMode(ENA, OUTPUT); // initialize ENA pin as an output
pinMode(ENB, OUTPUT); // initialize ENB pin as an output
pinMode(MOTOR_A1, OUTPUT); // initialize MOTOR_A1 pin as an output
pinMode(MOTOR_A2, OUTPUT); // initialize MOTOR_A2 pin as an output
pinMode(MOTOR_B1, OUTPUT); // initialize MOTOR_B1 pin as an output
pinMode(MOTOR_B2, OUTPUT); // initialize MOTOR_B2 pin as an output
pinMode(RIGHT, INPUT); // initialize RIGHT pin as an input
pinMode(LEFT, INPUT); // initialize ENA pin as an input

}

// the loop function runs over and over again forever
void loop() {
  
if (analogRead(RIGHT)<=35 && analogRead(LEFT)<=35) //compare the both sensor to decide the direction
{
  //MOVE FORWARD//
  
  analogWrite(ENA, 100); // set right motors speed
  analogWrite(ENB, 100); // set left motors speed
  
  //run right motors clockwise
  digitalWrite(MOTOR_A1, LOW);
  digitalWrite(MOTOR_A2, HIGH);
  //run left motors clockwise
  digitalWrite(MOTOR_B1, HIGH);
  digitalWrite(MOTOR_B2, LOW);
}
else if (analogRead(RIGHT)<=35 && !analogRead(LEFT)<=35) //compare the both sensor to decide the direction
{
  //MOVE RIGHT//
  
  analogWrite(ENA, 255); //set right motors speed
  analogWrite(ENB, 255); //set left motors speed
  
  //run right motors clockwise
  digitalWrite(MOTOR_A1, LOW);
  digitalWrite(MOTOR_A2, HIGH);
  //run left motors anti-clockwise
  digitalWrite(MOTOR_B1, LOW);
  digitalWrite(MOTOR_B2, HIGH);
}
else if (!analogRead(RIGHT)<=35 && analogRead(LEFT)<=35) //compare the both sensor to decide the direction
{ 
  //MOVE-LEFT//
  
  analogWrite(ENA, 255); //set right motors speed
  analogWrite(ENB, 255); //set left motors speed

  //run right motors anti-clockwise
  digitalWrite(MOTOR_A1, HIGH);
  digitalWrite(MOTOR_A2, LOW);
  //run left motors clockwise
  digitalWrite(MOTOR_B1, HIGH);
  digitalWrite(MOTOR_B2, LOW);
}
else if (!analogRead(RIGHT)<=35 && !analogRead(LEFT)<=35) //compare the both sensor to decide the direction
{ 
  //STOP//
  
  analogWrite(ENA, 0); //set right motors speed
  analogWrite(ENB, 0); //set left motors speed

  //stop right motors
  digitalWrite(MOTOR_A1, LOW);
  digitalWrite(MOTOR_A2, LOW);
  //stop left motors
  digitalWrite(MOTOR_B1, LOW);
  digitalWrite(MOTOR_B2, LOW);
  }
}