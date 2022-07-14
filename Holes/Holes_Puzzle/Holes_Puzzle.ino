#include "FastLED.h"

#define NUM_SENSORS 20
#define SENSORS_PER_QUADRANT 5
#define HIGH_THRESHOLD 20 // if value above this registers icicle there

const int trigPins[NUM_SENSORS] =     {   A1,    A3,    A5,    A7,    A9,   A11,   A15,    29,    31,    33,    35,    37,    39,    41,    43,    45,    47,    49,    51,    53};
const int echoPins[NUM_SENSORS] =     {   A0,    A2,    A4,    A6,    A8,   A10,   A14,    28,    30,    32,    34,    36,    38,    40,    42,    44,    46,    48,    50,    52};
const int identifiers[NUM_SENSORS] =  {   13,    14,    15,    16,    17,    18,    11,    20,    19,     5,     6,     4,     3,     2,     1,     9,    10,     8,     7,    12}; // the numbers on the board
const float thresholds[NUM_SENSORS] = { 5.84,  10.5,  5.29,     5,   6.5,  4.03,  3.09,  7.81,  6.45,     6,  5.92,  5.29,  3.42,  3.64,  4.65,     5,     5,  7.48,     6.5,  4.21}; // registers as there if icicle below this
//                                         0,     1,     2,     3,     4,     5,     6,     7,     8,     9,    10,    11,    12,    13,    14,    15,    16,    17,    18,    19


// 3 2
// 1 0

// 4 house
// V L


/*
   X  18 X   X  14 X
   15 17 16  13 X  20
   X  X  11  12 X  19

   7  X  X   X  X  6
   9  8  X   X  X  5
   10 X  2   1  3  4
*/

//assuming no changes: 4 is blue, house is red, V is green, L is yellow. this makes the new order yellow green red blue (reverse of current)

//13:55:16.436 ->          {14.59, 18.31,  9.41, 16.15, 11.79, 10.13,  7.35, 12.36, 10.71, 17.28, 10.49, 10.49,  7.97,  7.90,  8.77, 11.54, 18.17, 15.07, 10.63,  8.36}

const int correctSensors[][SENSORS_PER_QUADRANT] =        {{1, 3, 4, 5, 6}, {2, 8, 7, 9, 10}, {12, 13, 14, 20, 19}, {11, 15, 17, 16, 18}}; // declares with sensors are in which quadrant (using identifiers to specify which is whic)
const int correctSensorsByIndex[][SENSORS_PER_QUADRANT] = { {14, 12, 11, 9, 10}, {13, 17, 18, 15, 16}, {19, 0, 1, 7, 8}, {6, 2, 4, 3, 5}}; // declares with sensors are in which quadrant (using identifiers to specify which is whic)
byte overrides[][SENSORS_PER_QUADRANT] = {{0, 0, 0, 0, 0}, {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0}};
int prevStates[][SENSORS_PER_QUADRANT] = {{ -1, -1, -1, -1, -1}, { -1, -1, -1, -1, -1}, { -1, -1, -1, -1, -1}, { -1, -1, -1, -1, -1}}; //0 is out, 1 is in, 2 is overridden
int activeQuadrant = 0;
bool quadOverrides[] = {false, false, false, false};
bool correctQuads[] = {false, false, false, false};
bool solved = false;
bool found[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};


void(* resetFunc) (void) = 0; //declare reset function @ address 0

void setup() {
  Serial.begin(9600);
  while (!Serial) {
    delay(10);
  }
  Serial.println("STARTING-HOLES");
  for (int i = 0; i < 20; i++) {
    pinMode(trigPins[i], OUTPUT);
    pinMode(echoPins[i], INPUT);
  }
}

void loop() {
  float duration, distance;
  /*
     For every group of sensors,
     loop through the sensors and see if they are all on
  */
  for (int quadrant = 0; quadrant < 4; quadrant++) {
    bool detected;
    bool quadCorrect = true;
    for (int sensor = 0; sensor < 5; sensor++) {

      int currIndex = correctSensorsByIndex[quadrant][sensor];
      delay(10);

      // do timing stuff to get distance
      delayMicroseconds(10);
      digitalWrite(trigPins[currIndex], LOW);
      delayMicroseconds(2);
      digitalWrite(trigPins[currIndex], HIGH);
      delayMicroseconds(10);
      digitalWrite(trigPins[currIndex], LOW);
      duration = pulseIn(echoPins[currIndex], HIGH, 75000);
      distance = (duration * .0343) / 2;

      //if the object wasn't here previously, check if it is now
      if (!found[currIndex]) {
        detected = false;
        if ((distance > 20) || (distance < (thresholds[currIndex] - 1.0))) { // check if we should register object
          detected = true;
          found[currIndex] = true;
        }
      }
      //if the object was here previously, check if it left
      else {
        detected = true;
        if ((distance < 20) && (distance > (thresholds[currIndex] + 0.0))) { // check if we lost the object
          detected = false;
          found[currIndex] = false;
        }
      }



      //if this quadrant is overridden and it wasn't last time we checked, tell node red
      if (quadOverrides[quadrant]) {
        if (prevStates[quadrant][sensor] != 3) {
          Serial.print ("HOLES-QUAD-"); Serial.print(quadrant); Serial.print ("-SENSOR-"); Serial.print(sensor); Serial.println("-QUADOVERRIDDEN");
          prevStates[quadrant][sensor] = 3;
        }
        continue;
      }
      //if this sensor is overridden and it wasn't last time we checked, tell node red
      if (overrides[quadrant][sensor] == 2) {
        if (prevStates[quadrant][sensor] != 2) {
          Serial.print ("HOLES-QUAD-"); Serial.print(quadrant); Serial.print ("-SENSOR-"); Serial.print(sensor); Serial.println("-OVERRIDDEN");
          prevStates[quadrant][sensor] = 2;
        }
        continue;
      }

      //if this sensor is disabled and it wasn't last time we checked, tell node red
      if (overrides[quadrant][sensor] == 1) {
        quadCorrect = false;
        if (prevStates[quadrant][sensor] != 3) {
          Serial.print ("HOLES-QUAD-"); Serial.print(quadrant); Serial.print ("-SENSOR-"); Serial.print(sensor); Serial.println("-DISABLED");
          prevStates[quadrant][sensor] = 3;
        }
        continue;
      }
      
      //if this sensor registers a new object or loses an old one, tell node red
      if (prevStates[quadrant][sensor] != detected ? 1 : 0) {
        Serial.print ("HOLES-QUAD-"); Serial.print(quadrant); Serial.print ("-SENSOR-"); Serial.print(sensor); Serial.println(detected ? "-IN" : "-OUT");
        prevStates[quadrant][sensor] = detected ? 1 : 0;
      }
      //if this sensor doesn't register an object, this quadrant is not full
      if (!detected) {
        quadCorrect = false;
      }
    }
    //only execute the code after this if the quadrant is correct
    if (!quadCorrect) {
      continue;
    }
    //if we already printed that this quad is correct, skip printing
    if (correctQuads[quadrant]) continue;

    Serial.print("HOLES-CORRECT-QUAD-"); Serial.println(quadrant);
    correctQuads[quadrant] = true;
  }

  //if all the quadrants have been solved at some point, the puzzle has been solved
  if (!solved) {
    bool totallyCorrect = true;
    for (int i = 0; i < 4; i++) {
      if (!correctQuads[i])totallyCorrect = false;
    }
    if (totallyCorrect) {
      Serial.println("HOLES-TOTALLY-CORRECT");
      solved = true;
    }
  }


  //accept serial inputs from nodered
  while (Serial.available() > 0) {
    delay(100); //wait for the serial to see all the input before we read

    //is this message for this arduino or a universal reset?
    char whichArduino = Serial.read();
    switch (whichArduino) {
      case 'R':
        Serial.println("HOLES-RESET");
        delay(50);
        resetFunc();
        break;
      case 'H':
        char whichCommand = Serial.read();
        if (whichCommand == '5') { //do a quadrant override
          char whichQuad = Serial.read();
          int quad = (int) whichQuad - (int)'1';
          quadOverrides[quad] = !quadOverrides[quad];
          break;
        }
        switch (whichCommand) {
          case '0':
            Serial.println("HOLES-RESET");
            delay(50);
            resetFunc();
            break;
          /*
             Overrides the sensor determined by the input string
             Accepts strings in the format "HQS" where Q is the quadrant number and S is the sensor number.
             For example, H14 overrides sensor 4 in quadrant 1
             IMPORTANT: these numbers arent zero-indexed. quadrant 0 and sensor 0 dont exist
          */
          case '1':
          case '2':
          case '3':
          case '4':
            byte quad = whichCommand - 49; //converts to value between 1 and 5
            char whichSensor = Serial.read();
            switch (whichSensor) {
              case '1':
              case '2':
              case '3':
              case '4':
              case '5':
                byte sens = whichSensor - 49; //converts to value betweeen 1 and 5
                overrides[quad][sens] = (overrides[quad][sens] + 2) % 3;
            }
            break;

        }
    }
  }
}
