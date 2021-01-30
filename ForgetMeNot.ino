#include "Serial.h"

ServicePortSerial Sp;

///VARIABLES
byte numBlinks = 6;
Color gameColors[] =  {RED, YELLOW, GREEN, BLUE};
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
byte blinkTrue[6];
byte blinkLost[6];
Timer trueTimer;
Timer hiddenTimer;
Timer flashTimer;
Timer totalFlash;
bool onFlag = false;
byte level = 0;
byte over = 0;
byte winstate = 0;

///for color ramp
#define STEP_SIZE 10
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
      break;
    case LOSTREADY:
      Sp.println("LOSTREADY");
      lostReady();
      readyDisplay();
      break;
    case END:
      Sp.println("END");
      endLoop();
      break;
    case GAME:
      Sp.println("GAME");
      gameLoop();
      gameDisplay();
      byte sendData = (gameState << 4) + (over << 1) + (winstate);
      setValueSentOnFace(sendData, masterFace);
      break;
  }
  //clear button presses
  buttonSingleClicked();
  //set communications
  if (gameState != GAME) {
    FOREACH_FACE(f) {
      byte sendData = (gameState << 4) + (blinkTrue[f] << 1) + blinkLost[f];
      setValueSentOnFace(sendData, f);
    }
  }
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
    Sp.println("buttonx2");
    setLost();
    gameState = LOSTREADY;
    canBeMaster = false;
    isMaster = true;
    Sp.println("done button");
  }
  FOREACH_FACE(f) {
    if (!isValueReceivedOnFaceExpired(f)) {
      byte neighborData = getLastValueReceivedOnFace(f);
      Sp.print("data:");
      Sp.println(getGameState(neighborData), BIN);
      if (getGameState(neighborData) == LOSTREADY) {
        gameState = GAME;
        // displayState = PREP;
        masterFace = f;//will only listen for packets on this face
      }
    }
  }
}

void lostReady() {
  //listen for results of the LEVEL;
  FOREACH_FACE(f) {
    if (!isValueReceivedOnFaceExpired(f)) {
      byte neighborData = getLastValueReceivedOnFace(f);
      Sp.print("READY: ");
      Sp.print(f);
      Sp.print(" ");
      Sp.println(neighborData, BIN);
      if ((neighborData & 3) == 3) {
        Sp.println("HERE");
        level++;
        gameState = END;
        over = 1;
        winstate = 1;
        break;
      }
      if ((neighborData & 3) == 2) {
        level = 0;
        gameState = END;
        over = 1;
        winstate = 0;
        break;
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
        displayState = TRUE;
        trueTimer.set(4000);
      }
      break;
    case TRUE:
      if (trueTimer.isExpired()) {
        displayState = HIDDEN;
        hiddenTimer.set(3000);
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
              lostcolor = random(3);
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
      Sp.println("PREP");
      setColor(OFF);
      setColorOnFace(GREEN, masterFace);
      break;
    case TRUE:
      Sp.println("TRUE");
      setColor(gameColors[truecolor]);
      break;
    case HIDDEN:
      Sp.println("HIDDEN");
      setColor(OFF);
      break;
    case LOST:
      Sp.println("LOST");
      if (lostBlink) {
        setColor(gameColors[lostcolor]);
      } else {
        setColor(gameColors[truecolor]);
      }
      break;
    case WIN:
      if (lostBlink) {
        if (flashTimer.isExpired()) {
          if (onFlag) {
            setColor(gameColors[truecolor]);
            onFlag = false;
          } else {
            setColor(gameColors[lostcolor]);
            onFlag = true;
          }
          flashTimer.set(500);
        }
      } else {
        setColor(gameColors[truecolor]);
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

void resetLoop() {
  displayState = PREP;
  onFlag = false;
  over = 0;
  winstate = 0;
  lostBlink = 0;
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
  Sp.println("setLost");
  lostBlink = random(numBlinks - 1);
  for (int i = 0; i < numBlinks; i++) {
    blinkTrue[i] = random(3);
    if (i == lostBlink) {
      blinkLost[i] = 1;
    } else {
      blinkLost[i] = 0;
    }
    //Sp.println(i);
  }
}
