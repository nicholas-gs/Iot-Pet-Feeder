/*
 Name:		PetFeeder.ino
 Created:	3/3/2020 10:09:06 AM
 Author:	Nicho
*/

#include "Arduino.h"
#include <SoftwareSerial.h>
#include <Servo.h>
#include "PiezoSpeaker.h"
#include <ESP8266_Lib.h>
#include <BlynkSimpleShieldEsp8266.h>
#include <WidgetRTC.h>
#include <TimeLib.h>

// Pin Definitions
#define PIR_PIN_SIG	2
#define SERVOSM_PIN_SIG	3
#define ESP_SERIAL_RECEIVE_PIN 5
#define ESP_SERIAL_TRANSMIT_PIN 6
#define THINSPEAKER_PIN_POS	12

 // Your ESP8266 baud rate
#define ESP8266_BAUD 9600

// ====================================================================
// vvvvvvvvvvvvvvvvvvvv ENTER YOUR SETTINGS  vvvvvvvvvvvvvvvvvvvv
//
char auth[] = "";	// The Blynk API key. Unique to the project. Please check your email account or Blynk App.
char ssid[] = ""; // Enter your Wi-Fi name 
char pass[] = ""; // Enter your Wi-Fi password
//
// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
// ====================================================================

// Object Initialization
Servo servoSM;
PiezoSpeaker thinSpeaker(THINSPEAKER_PIN_POS);
SoftwareSerial EspSerial(ESP_SERIAL_RECEIVE_PIN, ESP_SERIAL_TRANSMIT_PIN);
ESP8266 wifi(&EspSerial);
WidgetRTC rtc;
WidgetLED foodDispensedLED(V9);
WidgetLED petEatenLED(V5);

// ====================================================================
// vvvvvvvvvvvvvvvvvvvv Speaker settings  vvvvvvvvvvvvvvvvvvvv
//
unsigned int thinSpeakerHoorayLength = 6;															// amount of notes in melody
unsigned int thinSpeakerHoorayMelody[] = { NOTE_C4, NOTE_E4, NOTE_G4, NOTE_C5, NOTE_G4, NOTE_C5 }; // list of notes. List length must match HoorayLength!
unsigned int thinSpeakerHoorayNoteDurations[] = { 8, 8, 8, 4, 8, 4 }; // note durations; 4 = quarter note, 8 = eighth note, etc. List length must match HoorayLength!
//
// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
// ====================================================================

// Threshold that the PIR sensor must detect the pet to prevent false positive detections.
const uint8_t THRESHOLD_COUNT = 4;
// Count to keep track of times that the PIR sensor detects the pet
static uint8_t detectionCount = 0;

// Rest position of servo motor
const uint8_t servoSMRestPosition = 20;
// Dispense position of servo motor
const uint8_t servoSMTargetPosition = 150; //Position when event is detected

// boolean to check track if pet has been fed. Not sure if need??
bool pet_fed = false;

// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
// ====================================================================
// ^^^^^^^^^^^^ States of the FSM of the pet feeder ^^^^^^^^^^^^^^^^^^^
enum FSM_State_Enum {
	INIT = 0, IDLE = 1, FEED = 2, MONITOR = 3, ERROR = 4
};

typedef uint8_t FSM_STATE;
// Current FSM state
static FSM_STATE fsm_state = INIT;
//
// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
// ====================================================================

// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
// ====================================================================
/*
	Struct for hour and minutes
*/
typedef struct {
	uint8_t hour = -1;
	uint8_t minute = -1;
} TIME_STRUCT;

typedef struct {
	uint8_t start_hour = -1;
	uint8_t start_minute = -1;
	uint8_t end_hour = -1;
	uint8_t end_minute = -1;
} FEEDING_TIME;

// Keep track of the feeding time as input from the Blynk app.
FEEDING_TIME feeding_time;

// Current time
TIME_STRUCT current_time;

//
// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
// ====================================================================

/*
	This function is runned when the hardware connects to Blynk cloud.
*/
BLYNK_CONNECTED() {
	// Synchronize time on connection
	rtc.begin();
}

/*
	The virtual pin V0 is connected to the the "Feed Pet" button in the Blynk app.
	It is automatically called ONCE when the state of the V0 pin changes.
*/
BLYNK_WRITE(V0) {
	uint8_t val = param.asInt();
	if (val) {				// Feed the pet once if the button state is HIGH
		feedPet();
		playTune();
	}
}

/*
	Virtual pin V8 is connected to the time widget in the Blynk app, for the user to select the feed time. 
	When the user changes the feed time, this function gets called ONCE.
	In this function, save the user chosen time in the struct "feeding_time".
*/
BLYNK_WRITE(V8) {
	pet_fed = false;
	fsm_state = IDLE;
	// Clear food dispensed & pet eaten led
	foodDispensedLED.setValue(0);
	petEatenLED.setValue(0);
	// Clear the pet eaten time in the LCD
	// Clear the LCD screen
	Blynk.virtualWrite(V1, " ");
	Blynk.virtualWrite(V2, " ");

	TimeInputParam t(param);
	feeding_time.start_hour = t.getStartHour();
	feeding_time.start_minute = t.getStartMinute();
	feeding_time.end_hour = t.getStopHour();
	feeding_time.end_minute = t.getStopMinute();
	Serial.println(F("****** Feeding time ******"));
	Serial.print(feeding_time.start_hour); Serial.print(F(":")); Serial.println(feeding_time.start_minute);
	Serial.print(F(" - ")); Serial.print(feeding_time.end_hour); Serial.print(F(":")); Serial.println(feeding_time.end_minute);
}

/*
	Virtual pin V10 is connected to the clear button of the Blynk App. "Reset" the fet feeder,
	as well as clear the widgets in the Blynk App.
*/
BLYNK_WRITE(V10) {
	pet_fed = false;
	fsm_state = IDLE;
	// Clear food dispensed & pet eaten led
	foodDispensedLED.setValue(0);
	petEatenLED.setValue(0);
	// Clear the pet eaten time in the LCD
	// Clear the LCD screen
	Blynk.virtualWrite(V1, " ");
	Blynk.virtualWrite(V2, " ");
}

// the setup function runs once when you press reset or power the board
void setup() {
	Serial.begin(9600);

	EspSerial.begin(ESP8266_BAUD);

	// PIR Sensor initialisation
	pinMode(PIR_PIN_SIG, INPUT);

	// Init Blynk -- Connect to the Blynk Server.
	Serial.println(F("Connecting to Blynk..."));
	Blynk.begin(auth, wifi, ssid, pass);
	Serial.println(F("Blynk Connected"));

	// Sync interval in seconds (10 minutes)
	setSyncInterval(10 * 60);
	delay(100);
}

// the loop function runs over and over again until power down or reset
void loop() {
	Blynk.run();
	getCurrentTime();
	fsm_Run();
	delay(200);
}

/*
	FSM run
*/
void fsm_Run() {
	switch (fsm_state) {
	case INIT:
		initFSM();
		fsm_state = IDLE;
		break;
	case IDLE:
		if ((!pet_fed) && idleToFeedTransition()) {
			foodDispensedLED.setValue(250);
			fsm_state = FEED;
		}
		break;
	case FEED:
		pet_fed = true;
		feedPet();
		playTune();
		fsm_state = MONITOR;
		break;
	case MONITOR:
		if (petDetection()) {
			// If pet has been detected, send information to the Blynk dashboard, including time stamp to Blynk
			petHasEaten();
			fsm_state = IDLE;
		}
		break;
	}
}

/*
	Read in the signal pin of the PIR Sensor. If it returns true(1), then pet is detected. If return false(0), then pet not detected.
*/
bool readInPIR() {
	return (digitalRead(PIR_PIN_SIG));
}

/*
	Turn the servo motor to open the feeder, then close the feeder.
	NOTE: THERE MUST BE A DELAY BETWEEN WRITE(...) ELSE THE SERVO MOTOR WILL NOT TURN PROPERLY
*/
void feedPet() {
	servoSM.attach(SERVOSM_PIN_SIG);         // 1. attach the servo to correct pin to control it.
	servoSM.write(servoSMTargetPosition);  // 2. turns servo to target position. Modify target position by modifying the 'ServoTargetPosition' definition above.
	delay(1500);                              // 3. waits 500 milliseconds (0.5 sec). change the value in the brackets (500) for a longer or shorter delay in milliseconds.
	servoSM.write(servoSMRestPosition);    // 4. turns servo back to rest position. Modify initial position by modifying the 'ServoRestPosition' definition above.
	delay(1500);                              // 5. waits 500 milliseconds (0.5 sec). change the value in the brackets (500) for a longer or shorter delay in milliseconds.
	servoSM.detach();
}

/*
	Reset the FSM
*/
void initFSM() {
	Serial.println(F("Reset the FSM"));

	// Reset detection count
	detectionCount = 0;
	pet_fed = false;
	foodDispensedLED.setValue(0);
	petEatenLED.setValue(0);
	
	// Reset the servo to its default position
	servoSM.attach(SERVOSM_PIN_SIG);
	servoSM.write(servoSMRestPosition);
	delay(1000);
	servoSM.detach();

	// Clear the LCD screen in the Blynk App
	Blynk.virtualWrite(V1, " ");
	Blynk.virtualWrite(V2, " ");
}

/*
	Returns true if pet has been detected, false if not. There is a threshold of 4 consecutive detections to prevent
	false positive detection.
*/
bool petDetection() {
	if (readInPIR()) {			// If the pet is detected, then increment the detection count
		detectionCount += 1;
	}
	else {
		detectionCount = 0;		// If pet is not detected, then reset the detection count
	}
	if (detectionCount > THRESHOLD_COUNT) {	// If the pet has been detected for at least 4 consecutive times, return true
		detectionCount = 0;		// Reset detection count
		return true;
	}
	else {
		return false;
	}
}

/*
	Get the current time from the Blynk App Widget RTC
*/
void getCurrentTime() {
	current_time.hour = hour();
	current_time.minute = minute();
}

/*
	Controls the transition from IDLE to FEED state. This transition happens if the current time is within the 
	feeding time period as defined by the user. If so, return true, else return false
*/
bool idleToFeedTransition() {
	if ((current_time.hour >= feeding_time.start_hour) && (current_time.minute >= feeding_time.start_minute)
		&& (current_time.hour <= feeding_time.end_hour) && (current_time.minute <= feeding_time.end_minute)) {
		return true;
	}
	return false;
}

/*
	Speaker will play tune to attract pet to the food
*/
void playTune() {
	thinSpeaker.playMelody(thinSpeakerHoorayLength, thinSpeakerHoorayMelody, thinSpeakerHoorayNoteDurations);
	thinSpeaker.playMelody(thinSpeakerHoorayLength, thinSpeakerHoorayMelody, thinSpeakerHoorayNoteDurations);
	thinSpeaker.playMelody(thinSpeakerHoorayLength, thinSpeakerHoorayMelody, thinSpeakerHoorayNoteDurations);
}

// Tell the Blynk app that the pet has been detected eating the food. 
// 1. Light up the LED in the app
// 2. Display the time in the LCD in the app
void petHasEaten()
{
	petEatenLED.setValue(250);
	clockDisplay();
}

// Digital clock display of the time in the Blynk app LCD display
void clockDisplay()
{
	String currentTime = String(hour()) + ":" + minute() + ":" + second();
	String currentDate = String(day()) + " " + month() + " " + year();

	// Send time to the App
	Blynk.virtualWrite(V1, currentTime);
	// Send date to the App
	Blynk.virtualWrite(V2, currentDate);
}
