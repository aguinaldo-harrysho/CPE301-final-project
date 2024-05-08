#include <Stepper.h>
#include <DS3231.h>
#include <Wire.h>

// Port H - Stepper motor
volatile unsigned char* DDRH_REG = (volatile unsigned char*) 0x87;
volatile unsigned char* PORTH_REG = (volatile unsigned char*) 0x86;

// Port E - Start, Stop button Port
volatile unsigned char* DDRE_REG = (volatile unsigned char*) 0x21;
volatile unsigned char* PORTE_REG = (volatile unsigned char*) 0x2C;

// Port B - LED Ports
volatile unsigned char *DDRB_REG = (volatile unsigned char*)0x24;
volatile unsigned char *PORTB_REG = (volatile unsigned char*)0x25;

// USART Registers
#define TBE 0x20
volatile unsigned char *_UCSR0A = (unsigned char *)0x00C0; // USART control status registers
volatile unsigned char *_UCSR0B = (unsigned char *)0x00C1;
volatile unsigned char *_UCSR0C = (unsigned char *)0x00C2;
volatile unsigned int *_UBRR0 = (unsigned int *)0x00C4;
volatile unsigned char *_UDR0 = (unsigned char *)0x00C6; // USART buffer

// State flag declaration
enum states{DISABLED, IDLE, ERROR, RUNNING};
volatile enum states coolerState = IDLE;

// Fan flag
volatile bool fanOn = false;

void setup() {
  // put your setup code here, to run once:

  // Initialize UART for Serial Out
  U0init(9600);

  //PH3 - PH6 in OUTPUT mode
  *DDRH_REG |= (0x1 << 3);
  *DDRH_REG |= (0x1 << 4);
  *DDRH_REG |= (0x1 << 5);
  *DDRH_REG |= (0x1 << 6);

  //PB4 - PB7 in OUTPUT mode
  *DDRB_REG |= (0x1 << 4);
  *DDRB_REG |= (0x1 << 5);
  *DDRB_REG |= (0x1 << 6);
  *DDRB_REG |= (0x1 << 7);

  // Start, Stop interrupt initializer
  *DDRE_REG |= (0x0 << 4); // Pin D2 (Port E4) to INPUT mode
  attachInterrupt(digitalPinToInterrupt(2), startStopInterrupt, HIGH);

  // I2C Communication for Real Time Clock
  Wire.begin();

}

void loop() {

  // Report Humidity, Temperature
  float temperature;
  float humidity;

  if (coolerState != DISABLED){

    temperature = 0.0;
    humidity = 0.0;

    // Output to LCD

  }

  // Handle Vent Angle change
  bool upButton = 0;
  bool downButton = 0;
  if (upButton){

    ventAngle(0);

  } else if (downButton) {

    ventAngle(1);

  }

  // Fan


  // State
  switch(coolerState){

    case DISABLED:
      disabledState();
      break;

    case IDLE:
      idleState();
      break;

    case ERROR:
      errorState();
      break;

    case RUNNING:
      runningState(temperature);
      break;

    default:
      transitionTo(ERROR);
  }

}

void disabledState(){

  // Fan off
  fanOn = false;

  // Yellow LED ON
  setLED(1, 0, 0, 0);

}

void idleState(){

  // Green LED ON

  setLED(0, 1, 0, 0);

  // Monitor water level
  bool waterLevelCritical = false;

  if (waterLevelCritical) {

    transitionTo(ERROR);

  } 

}

void errorState(){

  // Turn off Fan
  fanOn = false;

  // Display Error to LCD


  // Red LED ON
  setLED(0, 0, 1, 0);

}

void runningState(float temperature){

  int threshold = 1;

  // Run Cooler

  fanOn = true;

  // Check water level

  bool waterLevelCritical = false;

  if (waterLevelCritical) {

    transitionTo(ERROR);

  } else if (temperature <= threshold) {

    transitionTo(IDLE);

  }

}

void transitionTo(enum states nextState){
  
  // Change state flag
  coolerState = nextState;

  char nextStateString[9];

  switch(nextState){

      case DISABLED:
        strcpy(nextStateString, "DISABLED"); 
        break;

      case IDLE:
        strcpy(nextStateString, "IDLE");
        break;

      case ERROR:
        strcpy(nextStateString, "ERROR");
        break;

      case RUNNING:
        strcpy(nextStateString, "RUNNING");
        break;
  }

  char message[50]; // Increase the size of message array to accommodate the string
  // Use snprintf to format the string and store it in message array
  int messageLength = snprintf(message, sizeof(message), "State: %s", nextStateString);


  // Report Timestamp
  DS3231 myRTC;

  bool twelveHr = 0;
  bool pm = 0;

  int hour = myRTC.getHour(twelveHr, pm);
  int min = myRTC.getMinute();
  int sec = myRTC.getSecond();

  unsigned char timeString[16];
  sprintf(timeString, "Time: %d:%d:%d", hour, min, sec);
  printToSerial(timeString, 16);

  // Report vent angle


}

void setLED(const bool Y, const bool G, const bool R, const bool B){

  *PORTB_REG |= (Y == 1 ? 0x1 : 0x0) << 4;
  *PORTB_REG |= (G == 1 ? 0x1 : 0x0) << 5;
  *PORTB_REG |= (R == 1 ? 0x1 : 0x0) << 6;
  *PORTB_REG |= (B == 1 ? 0x1 : 0x0) << 7;

}

void ventAngle(bool direction){

  const int stepsPerRevolution = 2038;  // Defines the number of steps per rotation
  // Creates an instance of stepper class
  // Pins entered in sequence IN1-IN3-IN2-IN4 for proper step sequence
  Stepper myStepper = Stepper(stepsPerRevolution, 6, 8, 7, 9);

  myStepper.setSpeed(5);

  if(direction){

    myStepper.step(stepsPerRevolution/4);

  } else if (!direction){

    myStepper.step(-1*stepsPerRevolution/4);

  }

}

void startStopInterrupt(){

  if (coolerState == IDLE || coolerState == ERROR || coolerState == RUNNING){
    transitionTo(DISABLED);
  } else {
    transitionTo(IDLE);
  }

}

void U0init(unsigned long U0baud) {

  unsigned long FCPU = 16000000;
  unsigned int tbaud;
  tbaud = (FCPU / 16 / U0baud - 1); //baud rate operation
 
  *_UCSR0A = 0x20; // double-speed operation
  *_UCSR0B = 0x18; // enable transmitter and receiver
  *_UCSR0C = 0x06; // 8 bit data communication
  *_UBRR0 = tbaud; // set baud rate

}

void printToSerial(char message[], int size){

  for (int i = 0; i < size; i++) {

    U0putchar((unsigned char)message[i]);

  }

}

void U0putchar(unsigned char U0pdata) {

  while (!(*_UCSR0A & TBE)); // Transmitter Buffer Empty

  *_UDR0 = U0pdata; // Put data to buffer

}

