#include <Trill.h>
#include "MIDIUSB.h"
#include <Adafruit_NeoPixel.h>

Trill trillSensor;
boolean touchActive = false;
int prevButtonState[2] = { 0 , 0 };

#define LED_PIN 5
#define LED_COUNT 24

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

int prevPos;
float ledPos;
float ledsC, ledsL;

void setup() {
  // Initialise serial and touch sensor
  Serial.begin(115200);
  int ret = trillSensor.begin(Trill::TRILL_RING);
  if(ret != 0) {
    Serial.println("failed to initialise trillSensor");
    Serial.print("Error code: ");
    Serial.println(ret);
  }

  strip.begin();
  turnOffLeds();

  prevPos = 0;
}

void loop() {
  delay(10);
  trillSensor.read();
 
  if(trillSensor.getNumTouches() > 0) { // if there is a touch
    int newSize = constrain(trillSensor.touchSize(0)/4000.,0,1)*127; // scale the touch size
    controlChange(0, 0, newSize); // send the MIDI message
    
    if(!touchActive) { // note on
      touchActive = true;
      prevPos = trillSensor.touchLocation(0);
    }
    
    int newPos = trillSensor.touchLocation(0);
    
    int newSpeed = abs(newPos - prevPos); // calculate the speed
    if(newSpeed > 1000) newSpeed = 3583-newSpeed;
    controlChange(0, 1, constrain(3*newSpeed,0,127)); // send the MIDI message
    
    MidiUSB.flush();
    prevPos = newPos;
  }
  else if(touchActive) { // note off
    controlChange(0, 0, 0);
    controlChange(0, 1, 0); // send MIDI messages with 0 when no touch
    MidiUSB.flush();
    touchActive = false;
    prevSpeed = 0;
  }

  handleMidi(); // MIDI in for the light control
  drawLights();
}

void controlChange(byte channel, byte control, byte value) {
  midiEventPacket_t event = {0x0B, 0xB0 | channel, control, value};
  MidiUSB.sendMIDI(event);
}

void handleMidi() {
  midiEventPacket_t rx;
  do {
    rx = MidiUSB.read();
    if (rx.header != 0) {
      if(rx.byte2 == 1) {
        ledsC = rx.byte3/127.;
      }
      else if(rx.byte2 == 2) {
        ledsL = rx.byte3/127.;
      }
    }
  } while (rx.header != 0);
}

void drawLights() {
  if(ledsC < 0.001 && ledsL < 0.001){
    turnOffLeds();
    return;
  }
  int ringLeds[3*LED_COUNT];
  for(int l = 0; l < 3*LED_COUNT; l++) {
    ringLeds[l] = 0;
  }
  float n, c, l;
  n = 0;
  c = ledsC;
  l = ledsL;
  for(int x = 0; x < LED_COUNT; x++) { // here we do some math to have a smooth visual effect
    float w = constrain(50.*l*(1/(1+pow((x-n-LED_COUNT)/(2.6*l+0.4),2))+
                             1/(1+pow((x-n)   /(2.6*l+0.4),2))+
                             1/(1+pow((x-n+LED_COUNT)/(2.6*l+0.4),2))-0.1),0,255)/255.;
    int r = (constrain(c,0.5,0.75)-0.5)*4*255;
    int g = constrain(c,0,0.25)*4*255-(constrain(c,0.75,1)-0.75)*4*255;
    int b = 255-(constrain(c,0.25,0.5)-0.25)*4*255;
    ringLeds[3*x]   += w*r;
    ringLeds[3*x+1] += w*g;
    ringLeds[3*x+2] += w*b;
  }
  for(int x = 0; x < LED_COUNT; x++) {
    strip.setPixelColor(x,ringLeds[3*x],ringLeds[3*x+1],ringLeds[3*x+2]);
  }
  strip.show();
}

void turnOffLeds() {
  for(int i = 0; i < LED_COUNT; i++) {
    strip.setPixelColor(i,0,0,0);
    strip.show();
  }
}
