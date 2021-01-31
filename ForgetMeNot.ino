#include "Serial.h"

ServicePortSerial Sp;

///VARIABLES
byte numBlinks = 6;
Color gameColors0[] = {RED, YELLOW, GREEN, BLUE};
Color gameColors1[] = {MAGENTA, ORANGE, CYAN, WHITE};
Color gameColors2[] = {RED, MAGENTA, ORANGE, YELLOW};
Color gameColors3[] = {BLUE, GREEN, CYAN, WHITE};
Color chip = WHITE;
byte truecolor;
byte lostcolor;
byte lostBlink;
enum gameStates {SETUP, LOSTREADY, GAME, END};
byte gameState = SETUP;
enum displayStates {PREP, TRUE, HIDDEN, LOST, WIN, LOSE};
byte displayState = PREP;
bool canBeMaster;
bool isMaster;
byte masterFace;
byte blinkTrue[6]; //true colors index
byte blinkLost[6]; //indicates
Timer prepTimer;
Timer trueTimer;
Timer hiddenTimer;
Timer flashTimer;
Timer totalFlash;
bool onFlag = false;
byte level = 0;
byte over = 0;
byte winstate = 0;
byte numColors = 3;
byte levelup = 0;
bool runonce = true;
int showtime = 4000;
int hidetime = 3000;
byte difficulty = 0;

///for color ramp
int STEP_SIZE = 10;
#define STEP_TIME_MS 30
int brightness = 1;
int step = STEP_SIZE;
Timer nextStepTimer;

void setup() {
  randomize();
  Sp.begin();
  Sp.println("Serial Connection ONLINE");
}

void loop() {
  switch (gameState) {
    case SETUP:
      setupLoop();
      setupDisplay();
      FOREACH_FACE(f) {
        byte sendData = (gameState << 4) + (levelup << 3) + (blinkTrue[f] << 1) + blinkLost[f];
        setValueSentOnFace(sendData, f);
      }
      break;
    case LOSTREADY:
      //      Sp.println("LOSTREADY");
      lostReady();
      readyDisplay();
      runonce = true;
      FOREACH_FACE(f) {
        byte sendData = (gameState << 4) + (levelup << 3) + (blinkTrue[f] << 1) + blinkLost[f];
        setValueSentOnFace(sendData, f);
      }
      break;
    case END:
      //      Sp.println("END");
      endLoop();
      FOREACH_FACE(f) {
        byte sendData = (gameState << 4) + (over << 1) + (winstate);
        setValueSentOnFace(sendData, f);
      }
      break;
    case GAME:
      //      Sp.println("GAME");
      gameLoop();
      gameDisplay();
      runonce = true;
      byte sendData = (gameState << 4) + (over << 1) + (winstate);
      setValueSentOnFace(sendData, masterFace);
      break;
  }
  //clear button presses
  buttonSingleClicked();
}

void setupLoop() {
  resetLoop();
  byte numNeighbors = 0;
  FOREACH_FACE(f) {
    if (!isValueReceivedOnFaceExpired(f)) {
      byte neighborData = getLastValueReceivedOnFace(f);
      if (getGameState(neighborData) == SETUP) {
        numNeighbors++;
      }
    }
  }
  if (numNeighbors == 6) {
    canBeMaster = true;
  } else {
    canBeMaster = false;
  }
  if (buttonSingleClicked() && canBeMaster == true) {
    //    Sp.println("buttonx2");
    setLost();
    gameState = LOSTREADY;
    canBeMaster = false;
    isMaster = true;
    //    Sp.println("done button");
  }
  FOREACH_FACE(f) {
    if (!isValueReceivedOnFaceExpired(f)) {
      byte neighborData = getLastValueReceivedOnFace(f);
      //      Sp.print("data:");
      //      Sp.println(getGameState(neighborData), BIN);
      if (getGameState(neighborData) == LOSTREADY) {
        gameState = GAME;
        //        level = level + (neighborData >> 3 & 1);
        //        Sp.print("Level=");
        //        Sp.println(level);
        prepTimer.set(250);
        masterFace = f;
      }
    }
  }
}

void lostReady() {
  //listen for results of the LEVEL;
  FOREACH_FACE(f) {
    if (!isValueReceivedOnFaceExpired(f)) {
      byte neighborData = getLastValueReceivedOnFace(f);
      //      Sp.print("READY: ");
      //      Sp.print(f);
      //      Sp.print(" ");
      //      Sp.println(neighborData, BIN);
      if ((neighborData & 3) == 3) {
        over = 1;
        winstate = 1;
        levelup = 1;
        gameState = END;
      }
      if ((neighborData & 3) == 2) {
        over = 1;
        winstate = 0;
        levelup = 0;
        gameState = END;
      }
    }
  }
}

void gameLoop() {
  switch (displayState) {
    case PREP:
      if (!isValueReceivedOnFaceExpired(masterFace)) {
        byte neighborData = getLastValueReceivedOnFace(masterFace);
        // Sp.println(neighborData , BIN);
        truecolor = getInfo(neighborData);
        if (prepTimer.isExpired()) {
          setColor(OFF);
          displayState = TRUE;
        }
        trueTimer.set(showtime);
      }
      break;
    case TRUE:
      if (trueTimer.isExpired()) {
        displayState = HIDDEN;
        hiddenTimer.set(hidetime);
      }
      break;
    case HIDDEN:
      if (hiddenTimer.isExpired()) {
        displayState = LOST;
        if (!isValueReceivedOnFaceExpired(masterFace)) {
          byte neighborData = getLastValueReceivedOnFace(masterFace);
          if (getLost(neighborData)) {
            lostBlink = true;
            do {
              lostcolor = random(numColors);
            } while (lostcolor == truecolor);
          }
        }
      }
      break;
    case LOST:
      if (buttonSingleClicked()) {
        if (lostBlink) {
          displayState = WIN;
          winstate = 1;
          over = 1;
          totalFlash.set(5000);
        } else {
          winstate = 0;
          over = 1;
          displayState = LOSE;
          totalFlash.set(5000);
        }
      } else if (getGameState(getLastValueReceivedOnFace(masterFace)) == END) {
        getGlobalInfo();
        gameState = SETUP;
      }
      break;
    case WIN:
      if (totalFlash.isExpired()) {
        gameState = SETUP;
      }
      break;
    case LOSE:
      if (totalFlash.isExpired()) {
        gameState = SETUP;
      }
      break;
  }
}

void setupDisplay() {
  if (canBeMaster) {
    if (nextStepTimer.isExpired()) {
      if ( (brightness + step > MAX_BRIGHTNESS ) || (brightness + step < 0 ) ) {
        step = -step;
      }
      brightness += step;
      setColor( dim( GREEN ,  brightness  ) );
      nextStepTimer.set( STEP_TIME_MS );
    }
  } else {
    setColor(OFF);
  }
}

void readyDisplay() {
  setColor(YELLOW);
  if (over == 1) {
    switch (winstate) {
      case 0:
        setColor(RED);
        break;
      case 1:
        setColor(GREEN);
        break;
    }
  }
}

void gameDisplay() {
  switch (displayState) {
    case PREP:
      //      Sp.println("PREP");
      setColor(OFF);
      setColorOnFace(GREEN, masterFace);
      break;
    case TRUE:
      //      Sp.println("TRUE");
      if (level == 1) setColor(gameColors0[truecolor]);
      if (level == 2) setColor(gameColors1[truecolor]);
      if (level == 3) setColor(gameColors2[truecolor]);
      if (level == 4) setColor(gameColors3[truecolor]);
      if (level == 5) setColorOnFace(chip, truecolor);
      break;
    case HIDDEN:
      //      Sp.println("HIDDEN");
      setColor(OFF);
      break;
    case LOST:
      //      Sp.println("LOST");
      if (lostBlink) {
        if (level == 1) setColor(gameColors0[lostcolor]);
        if (level == 2) setColor(gameColors1[lostcolor]);
        if (level == 3) setColor(gameColors2[lostcolor]);
        if (level == 4) setColor(gameColors3[lostcolor]);
        if (level == 5) setColorOnFace(chip, lostcolor);
      } else {
        if (level == 1) setColor(gameColors0[truecolor]);
        if (level == 2) setColor(gameColors1[truecolor]);
        if (level == 3) setColor(gameColors2[truecolor]);
        if (level == 4) setColor(gameColors3[truecolor]);
        if (level == 5) setColorOnFace(chip, truecolor);
      }
      break;
    case WIN:
      if (lostBlink) {
        if (flashTimer.isExpired()) {
          if (onFlag) {
            if (level == 1) setColor(gameColors0[truecolor]);
            if (level == 2) setColor(gameColors1[truecolor]);
            if (level == 3) setColor(gameColors2[truecolor]);
            if (level == 4) setColor(gameColors3[truecolor]);
            if (level == 5) setColorOnFace(chip, truecolor);
            onFlag = false;
          } else {
            if (level == 1) setColor(OFF);
            if (level == 2) setColor(OFF);
            if (level == 3) setColor(OFF);
            if (level == 4) setColor(OFF);
            if (level == 5) setColor(OFF);
            onFlag = true;
          }
          flashTimer.set(500);
        }
      }
      break;
    case LOSE:
      if (flashTimer.isExpired()) {
        if (onFlag) {
          setColor(RED);
          onFlag = false;
        } else {
          setColor(OFF);
          onFlag = true;
        }
        flashTimer.set(100);
      }
  }
}

void endLoop() {
  gameState = SETUP;
  FOREACH_FACE(f) {
    if (!isValueReceivedOnFaceExpired(f)) {
      if (getGameState(getLastValueReceivedOnFace(f)) == GAME) {
        gameState = END;
      }
    }
  }
}

void getGlobalInfo() {
  if (!isValueReceivedOnFaceExpired(masterFace)) {
    byte neighborData = getLastValueReceivedOnFace(masterFace);
    Sp.print("Global: ");
    Sp.println(neighborData, BIN);
    if ((neighborData & 3) == 3) {
      over = 1;
      winstate = 1;
      gameState = SETUP;
    }
    if ((neighborData & 3) == 2) {
      over = 1;
      winstate = 0;
      gameState = SETUP;
    }
  }
}

void resetLoop() {
  if (runonce) {
    level++;
    showtime = 4000 - (difficulty * 1000);
    if (showtime < 1000) showtime = 1000;
    hidetime = 2000 + (difficulty * 1000);
    if (hidetime > 5000) hidetime = 5000;
    STEP_SIZE = 10+ (difficulty * 20);
    if (STEP_SIZE > 70) STEP_SIZE = 70; 
  }
  if (level > 5) {
    level = 1;
    difficulty++;
  }
  runonce = false;
  Sp.print("Level: ");
  Sp.println(level);



  displayState = PREP;
  onFlag = false;
  over = 0;
  winstate = 0;
  lostBlink = 0;
  levelup = 0;
}

byte getLost(byte data) {
  return (data & 1);
}

byte getInfo(byte data) {
  return (data >> 1 & 3);
}

byte getGameState(byte data) {
  return (data >> 4);
}

void setLost() {
  //  Sp.println("setLost");
  lostBlink = random(numBlinks - 1);            // calculate which blink if lost
  for (int i = 0; i < numBlinks; i++) {
    blinkTrue[i] = random(numColors);           // give each blink a colour index
    if (i == lostBlink) {
      blinkLost[i] = 1;                         // set an array to let the others know who will be lost
    } else {                                    // and
      blinkLost[i] = 0;                         // who won't
    }
    Sp.print(i);
    Sp.print("-");
    Sp.println(blinkTrue[i]);
  }
}
