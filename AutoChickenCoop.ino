#include <RTClib.h>

# define CLOSING 1
# define CLOSED 2
# define OPENING 3
# define OPEN 4

RTC_DS3231 rtc;

char daysOfWeek[7][12] = {
  "Sunday",
  "Monday",
  "Tuesday",
  "Wednesday",
  "Thursday",
  "Friday",
  "Saturday"
};

int motor1PinOpen = 27; 
int motor1PinClose = 26;
int buttonPin = 35;

int state = 3;
int openingHour = 06;
int openingMinute = 00;
int openingDuration = 300; // buffer for end switch

int closingHour = 20;
int closingMinute = 00;
int closingDuration = 185;

int testPause = 100;

int loopCounter = 0;
int buttonState = 0;
int cycleCounter = 0;

DateTime now;

void setup()
{
	Serial.begin(115200);
	pinMode(motor1PinOpen, OUTPUT);
  pinMode(motor1PinClose, OUTPUT);

	digitalWrite(motor1PinOpen, LOW); 
  digitalWrite(motor1PinClose, LOW);

  pinMode(buttonPin, INPUT_PULLUP);
  // attachInterrupt(digitalPinToInterrupt(buttonPin), topStop, RISING);

	Serial.println("Testing DC Motor...");
	Serial.println("Noch eine Meldung");

  // SETUP RTC MODULE
  if (! rtc.begin()) {
    Serial.println("RTC module is NOT found");
    Serial.flush();
    while (1);
  }
  now = rtc.now();
  setMotorState();
}

void loop()
{
  now = rtc.now();
	// Serial.println("Current State: ");
	// Serial.println(state);
  // Serial.println(loopCounter);
  delay(100);
	if(state == CLOSING || state == OPENING) 
    loopCounter++;
	
	// Serial.print("loop Counter: ");
	// Serial.println(loopCounter);
  buttonState = digitalRead(buttonPin);

  checkNextMotorState();
  // Serial.print(now.hour(), DEC);
  // Serial.print(':');
  // Serial.print(now.minute(), DEC);
  // Serial.print(':');
  // Serial.println(now.second(), DEC);
}

void checkNextMotorState()
{
	if(	
    state == CLOSING && loopCounter >= closingDuration ||
		state == CLOSED && now.hour() == openingHour && now.minute() == openingMinute ||
    // state == CLOSED && loopCounter >= testPause ||
    state == OPENING && ( buttonState == 0 || loopCounter >= openingDuration ) ||
    state == OPEN && now.hour() == closingHour && now.minute() == closingMinute
    // state == OPEN && loopCounter >= testPause
    )
	{
		nextMotorState();
	}
}

void setMotorState()
{
	digitalWrite(motor1PinOpen, LOW); 
  digitalWrite(motor1PinClose, LOW);
	if(state == 1) {
		digitalWrite(motor1PinClose, HIGH);
	}
	if(state == 3) {
		digitalWrite(motor1PinOpen, HIGH);
	}
}

void nextMotorState() {
	loopCounter = 0;
	state++;
  // Serial.println("next Motor State");
	if(state > 4) {
    state = 1;
    cycleCounter ++;
  }
  // Serial.println(state);
	setMotorState();
}  // */