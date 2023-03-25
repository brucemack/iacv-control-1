/**
 * Idle Air Control Valve (IACV) control software for 2-wire PWM systems.
 * This has been tested on a 1999 Honda Civic D16Y7 engines.
 * 
 * Bruce MacKinnon 18-Feb-2023
 * 
 * Targeted at a Teensy 3.2 controller.
 * 
 * Serial commands can be used to change the duty cycle.
 */
#include <limits.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C 

static Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
 
const unsigned long pwmHz = 250L;
const unsigned long pwmPeriodUs = 1000000L / pwmHz;
const unsigned long samples = 64L;
const unsigned long samplePeriodUs = pwmPeriodUs / samples;
elapsedMicros timer1;

const int ecuInputPin = 2;
const int iacvOutputPin = 3;

#define SWITCH_A_PIN 4
#define SWITCH_B_PIN 5

const unsigned int dutyStep = 4;
unsigned long sample = 0;
unsigned int iacvDutyPct = 50;
int iacvState = 1;

// This is used to measure the duty cycle of the input from the ECU
unsigned int ecuInputCount = 0;
unsigned int ecuInputDutyPct = 0;

bool displayDirty = true;

// Used to create a low-pass filter of the ECU 
static unsigned int ecuAvg[16];
static int ecuAvgPtr = 0;

static long switchATimer = 0;
static long switchBTimer = 0;

static boolean readSwitch(int pin, long* switchTimer, long intervalMs = 25) {
  // Read the sample
  int sample = digitalRead(pin);
  // Get time
  long now = millis();

  // When in the up position 
  if (sample == 1) {
    *switchTimer = 0;
  } else {
    // Look for transition
    if (*switchTimer == 0) {
      *switchTimer = now;
    } else if (*switchTimer == LONG_MAX) {
    } else {
      if (now - *switchTimer < intervalMs) {
      } else {
        // Make sure we don't report any more trues
        *switchTimer = LONG_MAX;
        return true;
      }
    }
  }
  return false;
}

void updateDisplay() {        
  char buf[32];
  display.clearDisplay();
  display.setTextSize(1);      // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE); // Draw white text
  display.setCursor(0, 0);
  sprintf(buf, "IACV Out: %u%%", iacvDutyPct);
  display.write(buf);          
  display.setCursor(0, 10);
  int avg = 0;
  for (int i = 0; i < 16; i++)
    avg += ecuAvg[i];
  avg /= 16;
  sprintf(buf, "  ECU In: %d%%", avg);
  display.write(buf);          
  display.display();
  displayDirty = false;
}

void updateOutput() {
  analogWrite(iacvOutputPin, (256 * iacvDutyPct) / 100);
}

void setup() {

  pinMode(ecuInputPin, INPUT);
  pinMode(SWITCH_A_PIN, INPUT_PULLUP);
  pinMode(SWITCH_B_PIN, INPUT_PULLUP);
  
  // Change PMW frequency on the output pin
  analogWriteFrequency(iacvOutputPin, 250);
  updateOutput();
  
  Serial.begin(9600);
  Serial.print("IACV Controller");

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
  }
}

void loop() {
  
  if (Serial.available()) {
    int r = Serial.read();
    if (r == '-') {
      if (iacvDutyPct > dutyStep) {
        iacvDutyPct -= dutyStep;
        displayDirty = true;
        updateOutput();
      }
    }
    else if (r == '=') {
      if (iacvDutyPct < 100 - dutyStep) {
        iacvDutyPct += dutyStep;        
        displayDirty = true;
        updateOutput();
      }
    }
  }

  // Check switches
  if (readSwitch(SWITCH_A_PIN, &switchATimer)) {
    if (iacvDutyPct > dutyStep) {
      iacvDutyPct -= dutyStep;
      displayDirty = true;
      updateOutput();
    }
  }
  if (readSwitch(SWITCH_B_PIN, &switchBTimer)) {
    if (iacvDutyPct < 100 - dutyStep) {
      iacvDutyPct += dutyStep;        
      displayDirty = true;
      updateOutput();
    }
  }
  
  // Look to see if it's time to take a sample
  if (timer1 > samplePeriodUs) {

    // Reset timer
    timer1 = timer1 - samplePeriodUs;

    // Sample input
    if (digitalRead(ecuInputPin)) {
      ecuInputCount++;
    }
    
    // Move through interval, wrapping if necessary
    if (++sample == samples) {
      sample = 0;  
      // Compute the input cycle  
      ecuInputDutyPct = (100 * ecuInputCount) / samples;     
      // Accumulate moving average
      ecuAvg[ecuAvgPtr] = ecuInputDutyPct;
      ecuAvgPtr = (ecuAvgPtr + 1) % 16;
      ecuInputCount = 0;
      displayDirty = true;
    }
  }
  
  if (displayDirty) {
    updateDisplay();
    // Reset timer to avoid any skew caused by the display writing
    timer1 = 0;
    sample = 0;
  }
}
