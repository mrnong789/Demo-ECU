#pragma region Dependencies
#include <Arduino.h>
#include "max6675.h"
#include "motorController/Motor.h"
#include <EEPROM.h>
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#pragma endregion

#pragma region Define pinIO and setup
//define motor
Motor myMotor;
//define pin TPS
const int analogPin = A0;  // Analog input pin
const int numReadings = 1;  // Number of readings to average
int readings[numReadings];  // Array to store readings
int index = 0;  // Current index in the readings array
int total = 0;  // Running total of readings
int tpsDTC = 0;
int thermoDTC = 1;
int speedDTC = 2;
int tablesSize = 5;
int dtcSize = 3;

//CLK CS MISO
MAX6675 thermocouple(6, 5, 4);

LiquidCrystal_I2C lcd(0x27, 20, 4);

void setup() {

  for (int i = 0; i < numReadings; i++) {
    readings[i] = 0;  // Initialize readings array to 0
  }

  myMotor.setup();
  Serial.begin(9600);

  for (int i = 0; i < 10; i++) {
    EEPROM.write(i, 0x00);
  }

  lcd.init();
  lcd.backlight();
  lcd.setCursor(3, 0);
  lcd.print("Demo ECU");
  lcd.setCursor(0, 3);
  lcd.print("Power By MRNONG789");

  // wait for MAX chip to stabilize
  delay(500);
  lcd.clear();

}
#pragma endregion

#pragma region Define Maps and Table

// define motor speed tables
const byte motorSpeedTables[11][5] PROGMEM = {
  { 0,   0,  0,  0,  0 },
  { 10,  70,  80,  50,  40 },
  { 30,  70,  80,  50,  40 },
  { 50,  70,  80,  50,  40 },
  { 70,  70,  80,  50,  40 },
  { 90,  70,  80,  50,  40 },
  { 120,  70,  80,  50,  40 },
  { 150,  70,  80,  50,  40 },
  { 200,  100, 110, 70,  60 },
  { 230,  120, 130, 90,  80 },
  { 254, 150, 160, 120, 100 }
};

// define thermo maps
const byte thermoMaps[5] PROGMEM = {
  40, 50, 70, 90, 110
};

// define tps maps
const byte tpsMaps[11] PROGMEM = {
  0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100
};

// define tps limit map 
const byte tpsLimitMaps PROGMEM = 100;

// define thermo limit maps
const byte thermoLimitMaps PROGMEM = 60;

// define motor speed limit maps
const byte speedLimitMaps PROGMEM = 255;

// define motor speed safemode maps
const byte speedSafeModeMaps PROGMEM = 50;

// define DTC Code
// A1=APP, B2=Thermo, C3=Motor speed
const byte dtcListMaps[3][2] PROGMEM = {
  {0XA1, 0X00},
  {0XB2, 0X00},
  {0XC3, 0X00}
};

#pragma endregion

#pragma region Utility function


bool checkAndDisplayDTC() {

  lcd.setCursor(12, 2);
  lcd.print("DTC:");
  bool foundDTC = false;
  for (int i = 0; i < dtcSize; i++) {
    int dtcCode = EEPROM.read(i);
    lcd.setCursor(18, i + 1);
    if (dtcCode == 0xA1) {
      lcd.print("A1");
    } else if (dtcCode == 0xB2) {
      lcd.print("B2");
    } else if (dtcCode == 0xC3) {
      lcd.print("C3");
    } else {
      lcd.print("-");
    }
  }
  return foundDTC;

}

void addDTCList(byte dtcCode) {
  bool isExiteDTC = false;
  int freeIndex = -1;
  for (int i = 0; i < dtcSize; i++) {
    int DTCFound = EEPROM.read(i);
    if (DTCFound == dtcCode) {
      isExiteDTC = true;
    }
    if (DTCFound == 0x00) {
      freeIndex = i;
    }
  }

  if ((isExiteDTC == false) && freeIndex != -1) {
    EEPROM.write(freeIndex, dtcCode);
  }
}

void displayLED(int tps, int temp, int motorSpeed) {
  lcd.setCursor(1, 0);
  lcd.print("Temp:");
  lcd.setCursor(6, 0);
  lcd.print("   ");
  lcd.setCursor(6, 0);
  lcd.print(temp);

  lcd.setCursor(12, 0);
  lcd.print("APP:");
  lcd.setCursor(16, 0);
  lcd.print("   ");
  lcd.setCursor(16, 0);
  lcd.print(tps);

  lcd.setCursor(1, 2);
  lcd.print("Speed:");
  lcd.setCursor(7, 2);
  lcd.print("   ");
  lcd.setCursor(7, 2);
  lcd.print(motorSpeed);
}

#pragma endregion

#pragma region Read Sensor function

// Read TPS sensor
int readTPS() {
  total -= readings[index];
  readings[index] = analogRead(analogPin);
  total += readings[index];
  index = (index + 1) % numReadings;
  int average = total / numReadings;

  int mappedValue = map(average, 0, 1024, 0, 100);
  return mappedValue;
}

#pragma endregion

#pragma region Read Maps and Tables

bool DTCAlert(int dtcIndex) {

  byte dtcCode = pgm_read_byte(&(dtcListMaps[dtcIndex][0]));
  byte dtcEnable = pgm_read_byte(&(dtcListMaps[dtcIndex][1]));
  if (dtcEnable == 0x00) {
    addDTCList(dtcCode);
    return true;
  }
  return false;
}

// Get Thermo map index from sensor value
int searchThermoIndex(int value) {
  for (int i = 0; i < 5; i++) {
    byte thermoMap = pgm_read_byte(&(thermoMaps[i]));
    if ((value < (int)thermoMap)) {
      return i;
    }
  }
  return -1;
}

// Get TPS map index from sensor value
int searchTPSIndex(int value) {
  //0-20, 21-40, 41-60, 61-80, 81-100
  for (int i = 0; i < 11; i++) {
    byte tpsMap = pgm_read_byte(&(tpsMaps[i]));
    if ((value < (int)tpsMap)) {
      return i;
    }
  }
  return -1;  // Return -1 if the value is not found
}

// Calculate motor speed
int calculateMotorSpeed(int tps, int thermo) {

  byte tpsLimit = pgm_read_byte(&(tpsLimitMaps));
  if (tps > tpsLimit) {
    bool responseDTC = DTCAlert(tpsDTC);
    if (responseDTC) {
      return (int)tpsLimit;
    }
  }

  // Search tps and thermo in maps
  int foundTPSIndex = searchTPSIndex(tps);
  if (foundTPSIndex == -1) {
    bool responseDTC = DTCAlert(tpsDTC);
    if (responseDTC) {
      return 0;
    }
  }

  int foundThermoIndex = searchThermoIndex(thermo);
  if (foundThermoIndex == -1) {
    bool responseDTC = DTCAlert(thermoDTC);
    if (responseDTC) {
      return 0;
    }
  }

  byte thermoFound = pgm_read_byte(&(thermoMaps[foundThermoIndex]));
  byte thermoLimit = pgm_read_byte(&(thermoLimitMaps));
  if (thermoFound > thermoLimit) {
    bool responseDTC = DTCAlert(thermoDTC);
    if (responseDTC) {
      byte speedSafeMode = pgm_read_byte(&(speedSafeModeMaps));
      return (int)speedSafeMode;
    }
  }

  byte speedMap = pgm_read_byte(&(motorSpeedTables[foundTPSIndex][foundThermoIndex]));
  byte speedLimit = pgm_read_byte(&(speedLimitMaps));
  if (speedMap > speedLimit) {
    bool responseDTC = DTCAlert(speedDTC);
    if (responseDTC) {
      return (int)speedLimit;
    }
  }
  return (int)speedMap;
}

#pragma endregion

#pragma region Main function

void loop() {

  delay(250);

  // Read TPS Sensor
  int tpsReader = readTPS();

  // Read Thermo sensor
  int thermoReader = thermocouple.readCelsius();

  // Calculate motor speed from Maps and Tables
  int motorSpeed = calculateMotorSpeed(tpsReader, thermoReader);
  if (motorSpeed > -1) {
    // Control motor speed     `
    myMotor.forward(motorSpeed);
  }

  //Display LED
  displayLED(tpsReader, thermoReader, motorSpeed);

  //check DTC 
  checkAndDisplayDTC();

}

#pragma endregion
