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
const unsigned long pwmHz = 250L;
const unsigned long pwmPeriodUs = 1000000L / pwmHz;
const unsigned long samples = 64L;
const unsigned long samplePeriodUs = pwmPeriodUs / samples;
elapsedMicros timer1;

const int ecuInputPin = 2;
const int iacvOutputPin = 3;

unsigned long sample = 0;
unsigned long iacvDutyPct = 50L;
int iacvState = 1;

void setup() {
  pinMode(ecuInputPin, INPUT);
  pinMode(iacvOutputPin, OUTPUT);
  digitalWrite(iacvOutputPin, 1);

  Serial.begin(9600);
}

void loop() {
  
  if (Serial.available()) {
    int r = Serial.read();
    if (r == '-') {
      if (iacvDutyPct > 2) {
        iacvDutyPct -= 2;
      }
    }
    else if (r == '=') {
      if (iacvDutyPct < 98) {
        iacvDutyPct += 2;
      }
    }
  }

  if (timer1 > samplePeriodUs) {
    // Sample
    
    // Generate
    if (sample < ((samples * iacvDutyPct) / 100L)) {
      if (iacvState == 1) {
        digitalWrite(iacvOutputPin, 0);
        iacvState = 0;
      }
    } else {
      if (iacvState == 0) {
        digitalWrite(iacvOutputPin, 1);
        iacvState = 1;
      }
    }

    // Move through interval, wrapping if necessary
    if (++sample == samples) {
      sample = 0;    
    }
    // Reset timer
    timer1 = timer1 - samplePeriodUs;
  }
}
