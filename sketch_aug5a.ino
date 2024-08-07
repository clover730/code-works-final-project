#include <OneWire.h>          // OneWire library for DS18B20
#include <TM1637Display.h>    // TM1637 library for the 7-segment display
#include "Wire.h"
#include "HOT.h"
#include "COLD.h"
#include <U8g2lib.h>
#include "GOOD.h"

// Pin definitions for TM1637 display
const byte CLK_PIN = 6;
const byte DIO_PIN = 5;
const byte RED_PIN = 11;    // PWM pin controlling the red leg of our RGB LED
const byte GREEN_PIN = 10;  // PWM pin controlling the green leg of our RGB LED
const byte BLUE_PIN = 9;    // PWM pin controlling the blue leg of our RGB LED
const byte BUZZER_PIN = 12;

// Create display object
TM1637Display lander_display = TM1637Display(CLK_PIN, DIO_PIN);
#define DEBUG false          // Set to true for debug output
U8G2_SSD1306_128X64_NONAME_F_HW_I2C screen(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

// Pin connected to KY-001 sensor
const uint8_t KY_001_PIN = 7;

// OneWire object for DS18B20
OneWire ds(KY_001_PIN);

// Maximum number of sensors
const int MAX_DS_DEVICES = 5;
byte ds_addresses[MAX_DS_DEVICES][8];
int ds_count = 0;

// Function to print sensor addresses
void print_address(byte address[8]);

void setup(void) {
  pinMode(BUZZER_PIN, OUTPUT);
  Serial.begin(9600);
  lander_display.setBrightness(7);
  screen.begin();

  // Search for DS18B20 devices
  for (int i = 0; i < MAX_DS_DEVICES; i++) {
    if (DEBUG) Serial.println("Searching for valid devices on OneWire");
    if (ds.search(ds_addresses[ds_count])) {
      if (DEBUG) {
        Serial.print("Device Address: ");
        print_address(ds_addresses[ds_count]);
      }

      if (OneWire::crc8(ds_addresses[ds_count], 7) != ds_addresses[ds_count][7]) {
        if (DEBUG) Serial.println(" CRC is not valid (skipped)");
        break;
      }

      switch (ds_addresses[ds_count][0]) {
      case 0x10:
        if (DEBUG) Serial.println(", DS18S20");
        break;
      case 0x28:
        if (DEBUG) Serial.println(", DS18B20");
        break;
      default:
        if (DEBUG) Serial.println(", UNKNOWN (skipped)");
        continue;
      }
      ds_count++;
    } else {
      break;
    }
    delay(1000);
  }
  Serial.print("Found ");
  Serial.print(ds_count);
  Serial.println(" devices.\n");
}

void loop(void) {
  byte data[12];
  
  for (int i = 0; i < ds_count; i++) {
    ds.reset();
    ds.select(ds_addresses[i]);
    ds.write(0x44, false);  // Start temperature conversion

    while (!ds.read_bit()) {}  // Wait for conversion to complete

    ds.reset();
    ds.select(ds_addresses[i]);
    ds.write(0xBE);  // Read Scratchpad

    for (int j = 0; j < 9; j++) {
      data[j] = ds.read();
    }

    uint8_t crc = OneWire::crc8(data, 8);
    if (crc != data[8]) {
      if (DEBUG) {
        Serial.print(" BAD CRC! (0x");
        char hex_string[3];
        sprintf(hex_string, "%02x", crc);
        Serial.print(hex_string);
        Serial.println(")");
      }
      return;
    }

    float TReading = (data[1] << 8) + data[0];
    float celsius = TReading * 0.0625;
    float fahrenheit = celsius * 1.8 + 32;
    int temp = (int)fahrenheit;
    int tempRound = round(10 * temp) / 10;
    Serial.println(tempRound);

    lander_display.showNumberDec(tempRound);

    if (tempRound > 79){ //hot 80 or more
      displayColor(255, 0, 0);
      tone(BUZZER_PIN, 1000); // Play tone at 1000Hz
      displayImage(HOT);
    }
    else if (tempRound < 76){ //cold 75 or less
      displayColor(0, 0, 255);
      tone(BUZZER_PIN, 500); // Play tone at 500Hz
      displayImage(COLD);
    }
    else {
      displayColor(255, 255, 255); //between 75 and 80
      noTone(BUZZER_PIN); // Stop the buzzer
      displayImage(GOOD);
    }

    if (DEBUG) {
      Serial.print("Temperature: ");
      Serial.println(" F");
    }
  }
  delay(1000);  // Delay between readings
}

void print_address(byte address[8]) {
  char hex_string[3];
  for (int i = 0; i < 8; i++) {
    sprintf(hex_string, "%02x", address[i]);
    Serial.print(hex_string);
    if (i == 3) Serial.print(":");
  }
}

void displayColor(
  byte red_intensity,    // red LED intensity (0-255)
  byte green_intensity,  // green LED intensity (0-255)
  byte blue_intensity    // blue LED intensity (0-255)
) {
  analogWrite(RED_PIN, red_intensity);      // Set red LED intensity using PWM
  analogWrite(GREEN_PIN, green_intensity);  // Set green LED intensity using PWM
  analogWrite(BLUE_PIN, blue_intensity);    // Set blue LED intensity using PWM
}

void displayImage(const unsigned char* bitmap) {
  screen.clearBuffer();                    // clear the internal memory
  screen.drawXBMP(0, 0, 128, 64, bitmap);  // draw the bitmap
  screen.sendBuffer();                     // transfer internal memory to the display
}
