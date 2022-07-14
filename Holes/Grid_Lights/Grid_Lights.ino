#include <FastLED.h>
#define NUM_STRIPS 1
#define NUM_LEDS_PER_STRIP 252
#define NUM_EDGES 13
#define NUM_HOLES 9
CRGB leds[NUM_LEDS_PER_STRIP];
bool conflict = false;

//
const byte divisions[] = {0, 25, 50, 67, 86, 104, 122, 141, 160, 177, 193, 216, 237, 252};
//edge number             0,  1,  2,  3,  4,   5,   6,   7,   8,   9,  10,  11,  12
byte activated[] =      { 0,  0,  0,  0,  0,   0,   0,   0,   0,   0,   0,   0,   0};

byte edgeList[4][4] = { //indexed by [quadrant][edge#]
  {4, 5, 6, 7}, //L
  {0, 1, 2, 3}, //CHECK
  {3, 11, 10, 6}, //HOUSE
  {6, 9, 8, 12}, //4
};
byte colors[5][3] = {{0, 0, 0}, {255, 255, 0},  {0, 255, 0},  {255, 0, 0}, {0, 0, 255}}; //off red green yellow blue

byte holes[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0}; //status of each hole
byte holesPins[9] = {
  4,  7,  5,
  3,  9,  2,
  6,  8,  10
};

//an array of adjacency matrices
//represents four graphs, one for each shape. L, Check, House, then 4.
//if there is an edge between sensor 1 and sensor 2 in L, adjMat[0][1][2] will be the edge number of that edge
const char adjMat[4][5][5] = //[quad][sensor][sensor]
{
  {
    { -1, 4, -1, -1, -1},
    { 4, -1, 5, -1, -1},
    { -1, 5, -1, 6, -1},
    { -1, -1, 6, -1, 7},
    { -1, -1, -1, 7, -1}
  },
  {
    { -1, 0, -1, -1, -1},
    { 0, -1, 1, -1, -1},
    { -1, 1, -1, 2, -1},
    { -1, -1, 2, -1, 3},
    { -1, -1, -1, 3, -1}
  },
  {
    { -1, 3, -1, -1, -1},
    { 3, -1, 11, -1, -1},
    { -1, 11, -1, 10, -1},
    { -1, -1, 10, -1, 6},
    { -1, -1, -1, 6, -1}
  },
  {
    { -1, -1, -1, 6, -1},
    { -1, -1, 12, -1, -1},
    { -1, 12, -1, 9, 8},
    {  6, -1, 9, -1, -1},
    { -1, -1, 8, -1, -1}
  }
};

//current states of all the sensors, indexed by [quadrant][sensor]
bool grid[4][5] = {
  {0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0}
};
//converts from [quadrant][sensor] to the right hole number
const char gridToHoles[4][5] {
  {6, 7, 8, 5, 2},
  {8, 4, 0, 3, 6},
  {6, 3, 1, 5, 8},
  {8, 3, 4, 5, 1}
};


void(* resetFunc) (void) = 0; //declare reset function @ address 0
void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }
  Serial.println("STARTING-GRID");
  for (byte i = 0; i < NUM_HOLES; i++) {
    pinMode(holesPins[i], OUTPUT);
    setHole(i, false);
  }
  showHoles();
  FastLED.addLeds<WS2811, 11, GRB>(leds, NUM_LEDS_PER_STRIP).setCorrection( TypicalLEDStrip );
  setAllEdges(0, 0, 0);
}

void loop() {
  bool prevConflict = conflict;
  while (Serial.available() > 0) {
    delay(150); //wait for the serial to see all the input before we read
    char whichArduino = Serial.read();
    if (whichArduino == 'R') {
      Serial.println("GRID-RESET");
      delay(150);
      resetFunc();
    }
    if (whichArduino == 'G') {
      char whichCommand = Serial.read();
      if (whichCommand == 'E') {
        byte quadOrder[] = {0, 1, 3, 2};
        byte sensorOrder[][5] = {{0, 1, 2, 3, 4}, {0, 1, 2, 3, 4}, {0, 1, 2, 3, 4}, {0, 3, 2, 1, 4}};
        for (int quad = 0; quad < 4; quad++) {
          for (int sensor = 0; sensor < 5; sensor++) {
            updateDisplay(quadOrder[quad], sensor, false);
          }
        }
        for (int quad = 0; quad < 4; quad++) {
          for (int sensor = 0; sensor < 5; sensor++) {
            updateDisplay(quadOrder[quad], sensorOrder[quad][sensor], true);
            delay(950);
          }
          for (int sensor = 0; sensor < 5; sensor++) {
            updateDisplay(quadOrder[quad], sensorOrder[quad][sensor], false);
          }
          delay(1000);
        }
      }
      else {
        //message is encoded by the following line in node red:
        //quad(0-3) + 4*sensor(0-4) + 20*on(0-1)
        //this part decodes it into a quadrant, a sensor, and whether or not it's on
        char cmd = whichCommand - 'H';
        byte quad = (cmd % 4);
        byte sensor = ((cmd % 20) / 4);
        bool on = (cmd / 20);
        updateDisplay(quad, sensor, on);
      }
    }
  }

  //if the conflict changed, display the lights and holes
  if (conflict != prevConflict) {
    showAllLights();
    showHoles();
  }
}

void updateDisplay(byte quad, byte sensor, bool on) {
  //conflict logic
  //turn on/off the new sensor, then check to see if multiple quandrants have active sensors in them. if so, set conflict to be true
  grid[quad][sensor] = on;
  char posCount = 0;
  for (char i = 0; i < 4; i++) {
    for (char j = 0; j < 5; j++) {
      if (grid[i][j]) {
        posCount++;
        break;
      }
    }
  }
  conflict = posCount > 1;

  //holes logic
  //turn on the right hole, then display the holes
  char hole = gridToHoles[quad][sensor];
  setHole(hole, on);
  showHoles();

  //edge logic
  //for each other sensor in the quadrant, if that sensor is on and there is an edge with the current sensor and that sensor, turn that edge on
  for (char j = 0; j < 5; j++) {
    if ((adjMat[quad][sensor][j] != -1) && (grid[quad][j])) {
      activated[adjMat[quad][sensor][j]] = on ? quad + 1 : 0;
      showLight(adjMat[quad][sensor][j]);
    }
  }
}

void setPixel(int Pixel, byte red, byte green, byte blue) {
  leds[Pixel].r = red;
  leds[Pixel].g = green;
  leds[Pixel].b = blue;
}


void setAll(byte edge, byte red, byte green, byte blue) {
  for (int i = divisions[edge]; i < divisions[edge + 1]; i++ ) {
    setPixel(i, red, green, blue);
  }
  showStrip();
}
void showStrip() {
  FastLED.show();
}

void setAllEdges(int red, int green, int blue) {
  for (int i = 0; i < NUM_EDGES; i++) {
    setAll(i, red, green, blue);
  }
}

void setHole(int which, bool on) {
  holes[which] = on;
//  holes[which] += on ? 1 : -1;
//  if ((holes[which] < 0) || (holes[which] > 4)) {
//    holes[which] = 0;
//  }

}

void showAllLights() {
  for (int i = 0; i < NUM_EDGES; i++) {
    if (conflict) {
      setAll(i, 0, 0, 0);
    }
    else {
      byte r = colors[activated[i]][0], g = colors[activated[i]][1], b = colors[activated[i]][2];
      setAll(i, r, g, b);
    }
  }
}
void showLight(int which) {
  if (conflict) {
    setAll(which, 0, 0, 0);
  }
  else {
    byte r = colors[activated[which]][0], g = colors[activated[which]][1], b = colors[activated[which]][2];
    setAll(which, r, g, b);
  }
}
void showHoles() {
  for (int i = 0; i < 9; i++) {
    //if there's a conflict, turn off every hole
    if (conflict) {
      digitalWrite(holesPins[i], i == 8 ? LOW : HIGH);
    }
    //if not, turn them on according to the array
    else {
      if (i == 8) {
        digitalWrite(holesPins[i], holes[i] == 0 ? LOW : HIGH);
      }
      else {
        digitalWrite(holesPins[i], holes[i] == 0 ? HIGH : LOW);

      }
    }
  }
  delay(30);
}
