// Project LASERGAME by F.Mojard et T.Sylvestre
// --------------------------------------------

#include <TimerOne.h> // https://www.pjrc.com/teensy/td_libs_TimerOne.html
#include <FrequencyTimer2.h> // https://playground.arduino.cc/Code/FrequencyTimer2/
#include <PinChangeInt.h> // https://github.com/NicoHood/PinChangeInterrupt
#include <Wire.h> // https://www.arduino.cc/en/reference/wire
#include <EEPROM.h> // https://www.arduino.cc/en/Reference/EEPROM

// Pin assignment for our infrared receiver, our button, our shot signal and our signal for the sound effects
#define IR_signal 7
#define K1 9
#define PWM_38k 10
#define sound_signal 11

// Time management
long int gameStartTime = 0;
long int lastTimeTouched = 0;
long int lastTimeShot = 0;
long int timeOn = 500; // Configuration of our burst of fire period
long int timeOff = 500; // Configuration of our burst of fire period
long int lastTimePWM_stateChange = 0;
long int lastTimeChangeSong = 0;
byte secondsCounter;
byte currentSecond = 0;
byte minutesCounter;

// Array and tools for saving shots received in real time
long int shootersPeriods[50]; // identification of a shooter by his burst period (PWM)
long int touchedSeconds[50];
long int touchedMinutes[50];
long int counterShooterPeriod = 0;
int nbTouched = 0;

// Sound management
int soundPeriod = 5000;

// States of game
bool isShooting = false;
bool isPWM_highState = true;
bool isDisplayingPeriod = false;
bool isPausedTouched = false;

void setup() {
  Serial.begin(9600);
  
  Wire.begin();
  Wire.beginTransmission(0x68);
  Wire.write(0);
  Wire.write(0x00);
  Wire.write(0x00);
  Wire.endTransmission();

  pinMode(IR_signal, INPUT); 
  pinMode(K1, INPUT_PULLUP); // K1 = pistol firing button
  pinMode(PWM_38k, OUTPUT); 
  pinMode(sound_signal, OUTPUT);
  DDRD = B00000100; // Player Status LED: On = Alive

  attachPinChangeInterrupt(K1, shot, FALLING);
  attachPinChangeInterrupt(IR_signal, touched, FALLING);

  PORTD = PORTD | B00000100; // Set the Player Status LED on Alive
}


void shot()  // If we press K1 (trigger)
{
  Timer1.initialize(26); // Creation of a frequency of 38kHz for the PWM
  Timer1.pwm(PWM_38k, 512); // PWM assignment on pin 10 (PWM_38k)
  soundPeriod = 5000;
  FrequencyTimer2::setPeriod(soundPeriod); //Son
  FrequencyTimer2::enable(); //Son

  lastTimeShot = micros();
  lastTimePWM_stateChange = lastTimeShot;
  lastTimeChangeSong = lastTimeShot;

  isShooting = true; // One shot lasts one second
}

void touched() {
  if (!isPausedTouched) {
    counterShooterPeriod = micros() - counterShooterPeriod;
    
    if (counterShooterPeriod < 3000) { // Our burst periods are less than 3000 uS
      lastTimeTouched = millis();
      PORTD = PORTD & B11100011; // Set the Player Status LED on Dead
      isPausedTouched = true;
      
      soundPeriod = 1000;
      FrequencyTimer2::setPeriod(soundPeriod);
      FrequencyTimer2::enable();
      
      nbTouched++;
      // Temporary save of the current game
      touchedSeconds[nbTouched - 1] = secondsCounter;
      touchedMinutes[nbTouched - 1] = minutesCounter;
      shootersPeriods[nbTouched - 1] = counterShooterPeriod;

      isDisplayingPeriod = true;
    }
  }
}


void loop()
{
  if (millis() - gameStartTime >= 10000 && gameStartTime != 0) { // If the game is over

    // Saving and viewing scores
    gameStartTime = 0;
    EEPROM.write(0, 12); // Code indicating that at least one part has been saved (safety for reading in the EEPROM)
    EEPROM.write(1, nbTouched);
    Serial.println("\n ----- RESULTAT : -----");
    if (nbTouched == 0) {
      Serial.println("Pas de touche");
    }
    for (int i = 0; i < nbTouched; i++) {
      Serial.print("Touche à ");
      Serial.print(touchedMinutes[i], HEX);
      Serial.print("min");
      Serial.print(touchedSeconds[i], HEX);
      Serial.print(" par ");
      Serial.print(shootersPeriods[i]);
      Serial.println(" ");
      EEPROM.write(i + 2, touchedMinutes[i]);
      EEPROM.write(nbTouched + i + 2, touchedSeconds[i]);
      EEPROM.write(nbTouched * 2 + i + 2, shootersPeriods[i]);
    }
    
  } else if (gameStartTime > 0) { // If a game is in progress
    
    // I2C communication with the arduino clock
    Wire.beginTransmission(0x68); 
    Wire.write(0); // read
    Wire.requestFrom(0x68, 2); // Getting the two bytes, seconds and minutes
    secondsCounter = Wire.read();
    minutesCounter = Wire.read();
    Wire.endTransmission();
    if (secondsCounter != currentSecond) { // Do it every seconds
      Serial.print(minutesCounter, HEX);
      Serial.print("min");
      Serial.println(secondsCounter, HEX);
      currentSecond = secondsCounter;
    }

    // Management of bursts of fire
    if (isShooting == true) 
    {
      if (micros() - lastTimeShot >=  1000000)
      {
        Timer1.stop();
        FrequencyTimer2::disable();
        isShooting = false;
      }
      else
      {
        if (micros() - lastTimePWM_stateChange >= timeOn && isPWM_highState)
        {
          Timer1.stop();
          lastTimePWM_stateChange = micros();
          isPWM_highState = false;
        }
        else if (micros() - lastTimePWM_stateChange >= timeOff && !isPWM_highState)
        {
          Timer1.initialize(26);
          Timer1.pwm(PWM_38k, 512);
          lastTimePWM_stateChange = micros();
          isPWM_highState = true;
        }

        // Song of fire
        if (micros() - lastTimeChangeSong >= 10000)
        {
          if (soundPeriod >= 500)
          {
            soundPeriod -= 50;
            FrequencyTimer2::setPeriod(soundPeriod);
            lastTimeChangeSong = micros();
          }
          else
          {
            soundPeriod = 5000;
            FrequencyTimer2::setPeriod(soundPeriod);
            lastTimeChangeSong = micros();
          }
        }
      }
    }

    // Management of shooting reception
    if (isDisplayingPeriod) {
      Serial.print("Touched by ");
      Serial.println(counterShooterPeriod);
      counterShooterPeriod = 0;
      isDisplayingPeriod = false;
    }
    
    if (isPausedTouched) {
      // Song of touched
      if (micros() - lastTimeChangeSong >= 10000) {
        if (soundPeriod <= 5000) {
          soundPeriod += 50;
          FrequencyTimer2::setPeriod(soundPeriod);
          lastTimeChangeSong = micros();
        }
        else {
          soundPeriod = 1000;
          FrequencyTimer2::setPeriod(soundPeriod);
          lastTimeChangeSong = micros();
        }
      }
      if (millis() - lastTimeTouched >= 5000) {
        isPausedTouched = false;
        PORTD = PORTD | B00000100;
        FrequencyTimer2::disable();
      }
    }
    
  } else if (!digitalRead(9)) { // If we start a game
    Wire.beginTransmission(0x68);
    Wire.write(0);
    Wire.write(0x00);
    Wire.write(0x00);
    Wire.endTransmission();
    
    if (EEPROM.read(0) == 12) {
      Serial.println("\n ----- LAST GAME ------");
      nbTouched = 0;
      int nbreValeurs = EEPROM.read(1);
      if (nbreValeurs == 0) {
        Serial.println("No touched");
      }
      for (int i = 0; i < nbreValeurs; i++) {
        Serial.print("Touched at ");
        Serial.print(EEPROM.read(i + 2), HEX);
        Serial.print("min");
        Serial.print(EEPROM.read(nbreValeurs + i + 2), HEX);
        Serial.print(" by ");
        Serial.print(EEPROM.read(nbreValeurs * 2 + i + 2));
        Serial.println(" ");
        touchedMinutes[i] = 0;
        touchedSeconds[i] = 0;
        shootersPeriods[i] = 0;
      }
    }
    Serial.println("\n ----- START GAME -----");
    gameStartTime = millis();
  } else {
    FrequencyTimer2::disable();
  }
}
