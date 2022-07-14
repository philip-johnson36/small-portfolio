#define NUM_SENSORS 5

// bottom left, bottom middle, middle right, top right, middle left
const int distanceSensors[NUM_SENSORS] = {A0, A1, A2, A3, A4};
const bool correctPositions[][NUM_SENSORS] = {{1, 0, 1, 0, 0}, {0, 0, 1, 1, 0}, {0, 1, 0, 1, 1}};
//true means valve open
//false means value closed

const String printouts[] = {"QUALITY", "FILTER", "DISPERSION"};

int sensorStates[NUM_SENSORS];
int oldSensorStates[NUM_SENSORS];
int pastReadings[NUM_SENSORS];

unsigned long debounceTimes[NUM_SENSORS];

int state = -1;
// -1 - waiting for GM input to start game
// 0  - quality
// 1  - filter
// 2  - dispersion
// 3  - DONE!!!

void setup() {
  Serial.begin(9600);
  Serial.println("STARTING-OXYGEN");
  for (int i = 0; i < NUM_SENSORS; i++) {
    pinMode(distanceSensors[i], INPUT_PULLUP);
  }
}

void loop() {

  if (state >= 0 && state < 3) { //make sure we're not done
    bool stateChanged = false;
    bool stateCorrect = true;
    for (int x = 0; x < NUM_SENSORS; x++) {
      oldSensorStates[x] = sensorStates[x];
      int reading = digitalRead(distanceSensors[x]);
      if (reading != pastReadings[x]) {
        pastReadings[x] = reading;
        debounceTimes[x] = millis();
      }
      else if (millis() - debounceTimes[x] > 250) {
        sensorStates[x] = reading;
      }
      if (sensorStates[x] != oldSensorStates[x]) {
        stateChanged = true;
      }
      if (sensorStates[x] != correctPositions[state][x]) {
        stateCorrect = false;
      }
    }
    if (stateChanged) {
      Serial.print("OXYGEN-");
      for (int i = 0; i < NUM_SENSORS; i++) {
        Serial.print(sensorStates[i]);
        Serial.print("-");
      }
      Serial.println("STATUS");
    }




//
//    bool stateCorrect = true;
//    for (int i = 0; i < NUM_SENSORS; i++) {
//      bool reading = digitalRead(distanceSensors[i]);
//      if (reading != correctPositions[state][i]) {
//        stateCorrect = false;
//      }
//      if (reading != oldReadings[i]) {
//        stateChanged = true;
//      }
//      oldReadings[i] = reading;
//    }
//    if (stateChanged) {
//      Serial.print("OXYGEN-");
//      for (int i = 0; i < NUM_SENSORS; i++) {
//        Serial.print(oldReadings[i]);
//        Serial.print("-");
//      }
//      Serial.println("STATUS");
//    }
    if (stateCorrect) {
      Serial.println(printouts[state]);
      state++;
    }
  }

  if (Serial.available() > 0) {           
    char receivedChar = Serial.read();
    switch (receivedChar) {
      case 'E':   // 0  - quality
        state = 0;
        break;
      case 'F':    // 1  - filter
        state = 1;
        Serial.println("QUALITY");
        break;
      case 'I':      // 2  - dispersion
        state = 2;
        Serial.println("FILTER");
        break;
      case 'H':      // 3  - DISPERSION!!!
        state = 3;
        Serial.println("DONE!!");
        break;
      case 'R':         //RESET
        state = -1;       // -1 - waiting for GM input to start game
        break;
    }
  }
}
