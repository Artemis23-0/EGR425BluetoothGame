///////////////////////////////////////////////////////////////
// Imports
///////////////////////////////////////////////////////////////
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <M5Core2.h>
#include <Adafruit_seesaw.h>
#include "../include/game_image_bitmaps.h"

///////////////////////////////////////////////////////////////
// Variables
///////////////////////////////////////////////////////////////

// Connection Variables
BLEServer *bleServer;
BLEService *bleService;
bool deviceConnected = false;
bool previouslyConnected = false;

// Location Characteristics/Variables
BLECharacteristic *bleReadXCharacteristic;
BLECharacteristic *bleReadYCharacteristic;
BLECharacteristic *bleReadWriteXCharacteristic;
BLECharacteristic *bleReadWriteYCharacteristic;
bool locationWasUpdated = true;

// Gameplay Characteristics/Variables
bool screenUpdated = false;
bool gameEnded = false;
bool playingAgain = false;

// Characteristics for each player's player type (2) and when an opponents powerup is used (2), and gamestate (1)

BLECharacteristic *bleLocalPlayerSelectionCharacteristic; // 1 (Princess), 2 (Dragon), 3 (Unchosen)
BLECharacteristic *bleOpponentPlayerSelectionCharacteristic; // 1 (Princess), 2 (Dragon), 3 (Unchosen)
BLECharacteristic *bleGameStateCharacteristic; // 1 (intro), 2 (tutorial), 3 (playing), 4 (end)

// If game ends before time is up, princess wins

// Location Unique IDs
#define SERVICE_UUID "7d7a7768-a9d0-4fb8-bf2b-fc994c662eb6"
#define READ_X_CHARACTERISTIC_UUID "563c64b2-9634-4f7a-9f4f-d9e3231faa56"
#define READ_Y_CHARACTERISTIC_UUID "aa88ac15-3e2b-4735-92ff-4c712173e9f3"
#define READ_WRITE_X_CHARACTERISTIC_UUID "1da468d6-993d-4387-9e71-1c826b10fff9"
#define READ_WRITE_Y_CHARACTERISTIC_UUID "cf7b4787-d412-4e69-8b61-e2cfba89ff19"

// Gameplay Unique IDs
#define LOCAL_PLAYER_SELECTION_UUID "ecaaac5c-5057-49dc-83ab-e0e2322f2703"
#define OPPONENT_PLAYER_SELECTION_UUID "cad4571b-2ca1-47c9-ae9d-75bbce0d814f"
#define GAME_STATE_UUID "07471f02-b963-449e-a39c-9a44ae312b78"

// State
enum Gameplay { S_PLAYER_SELECT, S_TUTORIAL, S_GAME, S_GAME_OVER };
static Gameplay gameState = S_PLAYER_SELECT;

enum PlayerType { PRINCESS, DRAGON, UNCHOSEN };
static PlayerType chosenPlayer = UNCHOSEN;
static PlayerType opponentPlayer = UNCHOSEN;

// Gameplay Variables
Adafruit_seesaw gamePad;

#define BUTTON_SELECT    0
#define BUTTON_START    16
uint32_t button_mask = (1UL << BUTTON_START) | (1UL << BUTTON_SELECT);

// Character Select Buttons
ButtonColors onCol = {BLACK, WHITE, WHITE};
ButtonColors offCol = {TFT_RED, BLACK, NODRAW};
ButtonColors offColTut = {TFT_CYAN, BLACK, NODRAW};
ButtonColors offColStart = {TFT_GREEN, BLACK, NODRAW};
Button DRAGON_BTN(50, 100, 100, 50, false, "DRAGON", offCol, onCol);
Button PRINCESS_BTN(160, 100, 100, 50, false, "PRINCESS", offCol, onCol);
Button TUTORIAL(10, 190, 100, 50, false, "Tutorial", offColTut, onCol);
Button START(210, 190, 100, 50, false, "Start", offColStart, onCol);
Button ENDTUTORIAL(210, 10, 100, 50, false, "X", offColTut, onCol);
Button PLAYAGAIN(100, 190, 100, 50, false, "Play Again", offColStart, onCol);

// joystick and button coordinates
int xServer = 10, yServer = 120, xClient = 0, yClient = 0;

// joystick and button acceleration
int acceleration = 5;

// Timer
int countdownTime = 120000; // Two minutes
int prevTime = 0;
int currTime = 0;
bool timerHasBeenStarted = false; 
bool timeRanOut = false;

// Powerup
int powerupsLeft = 3; // players start with 3 powerups
bool powerupsStillAvailable = true; 
int powerupTime = 3000; // powerups last 3 seconds
int powerupStartTime = 0;
bool dragonPowerupActive = false;
bool princessPowerupActive = false;

///////////////////////////////////////////////////////////////
// Forward Declarations
///////////////////////////////////////////////////////////////
void broadcastBleServer();

// Gameplay (Order of appearance)
void drawTitleScreen();
void drawWaitingScreen();
void drawCenteredBackgroundImage(String iconName, int resizeMult);
void chooseCharacter();
void drawSelectedCharacterName();
void princessTapped(Event& e);
void dragonTapped(Event& e);
void tutorialTapped(Event& e);
void startTapped(Event& e);
void startTutorial();
void endTutorialTapped(Event& e);
void playAgainTapped(Event& e);
void hideButtons();

void drawCharacters(uint32_t serverX, uint32_t serverY, uint32_t clientX, uint32_t clientY);
void drawCharacterImage(String iconName, int resizeMult, int xLoc, int yLoc);
void printDistance();
void checkTimeAndPrint();
void addressPowerup();

void serverAccelIncrement();
String milis_to_seconds(long milis);


void playGame();
void endGame();
bool checkDistance();
void usePowerup();

///////////////////////////////////////////////////////////////
// BLE Server Callback Methods
///////////////////////////////////////////////////////////////
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer *pServer) {
        deviceConnected = true;
        screenUpdated = true;
        bleReadXCharacteristic->setValue(xServer);
        bleReadYCharacteristic->setValue(yServer);
        
        bleReadXCharacteristic->notify();
        delay(10);
        bleReadYCharacteristic->notify();
        delay(10);
        previouslyConnected = true;
        Serial.println("Device connected...");
    }
    void onDisconnect(BLEServer *pServer) {
        deviceConnected = false;
        Serial.println("Device disconnected...");
    }
};

//////////////////////////////////////////////////////////////
// BLE Client Characteristic Callback Methods
//////////////////////////////////////////////////////////////
class MyCharacteristicCallbacks: public BLECharacteristicCallbacks {
    // callback function to support a read request
    void onRead(BLECharacteristic* pCharacteristic) {
        String characteristicUUID = pCharacteristic->getUUID().toString().c_str();
        String characteristicValue = pCharacteristic->getValue().c_str();
        Serial.printf("Client JUST read from %s: %s", characteristicUUID, characteristicValue.c_str());
    }
    
    // callback function to support a write request
    void onWrite(BLECharacteristic* pCharacteristic) {
        String characteristicUUID = pCharacteristic->getUUID().toString().c_str();
        String characteristcValue = pCharacteristic->getValue().c_str();
        Serial.printf("Client JUST wrote to %s: %s", characteristicUUID, characteristcValue.c_str());

        // check if characteristicUUID matches a known UUID
        if (characteristicUUID.equals(READ_WRITE_X_CHARACTERISTIC_UUID)) {
            // extract the x value 
            std::string readXValue = pCharacteristic->getValue();
            String valXStr = readXValue.c_str();
            xClient = valXStr.toInt();
        }
        if (characteristicUUID.equals(READ_WRITE_Y_CHARACTERISTIC_UUID)) {
            // extrac the y value
            std::string readYValue = pCharacteristic->getValue();
            String valYStr = readYValue.c_str();
            yClient = valYStr.toInt();
        }

        if (characteristicUUID.equals(OPPONENT_PLAYER_SELECTION_UUID)) {
            // extrac the y value
            std::string readOpponentPlayer = pCharacteristic->getValue();
            String valOpponentPlayer = readOpponentPlayer.c_str();
            opponentPlayer = PlayerType(valOpponentPlayer.toInt() - 1);
        }

        if (characteristicUUID.equals(GAME_STATE_UUID)) {
            // extrac the y value
            std::string readGameState = pCharacteristic->getValue();
            String valGameState = readGameState.c_str();
            Serial.print("VAL GAME STATE: ");
            Serial.println(valGameState);
            if (valGameState.toInt() == 1 || valGameState.toInt() == 2) {
              gameState = gameState;
            } else if (valGameState.toInt() == 3) {
              if (gameState == S_GAME_OVER) {
                  xServer = 10, yServer = 120;
                  bleReadYCharacteristic->setValue(yServer);
                  bleReadXCharacteristic->setValue(xServer);
                  bleReadXCharacteristic->notify();
                  delay(10);
                  bleReadYCharacteristic->notify();
                  delay(10);
                  int chosenPlayerInt = 3;
                  bleLocalPlayerSelectionCharacteristic->setValue(chosenPlayerInt);
                  bleLocalPlayerSelectionCharacteristic->notify();
                  delay(10);
                  M5.Lcd.fillRect(10, 20, 10, 10, TFT_BLACK);
                  playingAgain = true;
              } else {
                prevTime = millis();
                gameEnded = true;
              }
              gameState = S_GAME;
            } else {
              gameState = S_GAME_OVER;
            }
            screenUpdated = true;
        }
    }

    // callback function to support a Notify request
    void onNotify(BLECharacteristic* pCharacteristic) {
        String characteristicUUID = pCharacteristic->getUUID().toString().c_str();
        Serial.printf("Client JUST notified about change to %s: %s", characteristicUUID, pCharacteristic->getValue().c_str());
    }

    // callback function to support when a client subscribes to notifications/indications
    void onSubscribe(BLECharacteristic* pCharacteristic, uint16_t subValu) {}

    // calllback function to support a Notify/Indicate Status report
    void onStatus(BLECharacteristic* pCharacteristic, Status s, uint32_t code) {
        // print appropriate response
        String characteristicUUID = pCharacteristic->getUUID().toString().c_str();
        switch(s) {
            case SUCCESS_INDICATE:
                break;
            case SUCCESS_NOTIFY:
                Serial.printf("Status for %s: Successful Notification", characteristicUUID.c_str());
                break;
            case ERROR_INDICATE_DISABLED:
                Serial.printf("Status for %s: Failure; Indication Disabled on Client", characteristicUUID.c_str());
                break;
            case ERROR_NOTIFY_DISABLED:
                Serial.printf("Status for %s: Failure; Notification Disabled on Client", characteristicUUID.c_str());
                break;
            case ERROR_GATT:
                Serial.printf("Status for %s: Failure; GATT Issue", characteristicUUID.c_str());
                break;
            case ERROR_NO_CLIENT:
                Serial.printf("Status for %s: Failure; No BLE Client", characteristicUUID.c_str());
                break;
            case ERROR_INDICATE_TIMEOUT:
                Serial.printf("Status for %s: Failure; Indication Timeout", characteristicUUID.c_str());
                break;
            case ERROR_INDICATE_FAILURE:
                Serial.printf("Status for %s: Failure; Indication Failure", characteristicUUID.c_str());
                break;
        }
    }    

};

///////////////////////////////////////////////////////////////
// Put your setup code here, to run once
///////////////////////////////////////////////////////////////
void setup()
{
    // Init device
    M5.begin();
    M5.Lcd.setTextSize(3);

    // Initialize M5Core2 as a BLE server
    Serial.print("Starting BLE...");
    String bleDeviceName = "Princess of Fire";
    BLEDevice::init(bleDeviceName.c_str());

    // Draw the game intro screen
    drawTitleScreen();

    // Draw the waiting for second player screen
    drawWaitingScreen();

    // Broadcast the BLE server
    broadcastBleServer();

    // Gameplay setup
    if(!gamePad.begin(0x50)){
        Serial.println("ERROR! seesaw not found");
        while(1) delay(1);
    }
    gamePad.pinModeBulk(button_mask, INPUT_PULLUP);
    gamePad.setGPIOInterrupts(button_mask, 1);
    for (int i = 0; i < 10; i++) {
      bleReadXCharacteristic->setValue(xServer);
      bleReadYCharacteristic->setValue(yServer);
      
      bleReadXCharacteristic->notify();
      delay(10);
      bleReadYCharacteristic->notify();
      delay(500);
    }

    PRINCESS_BTN.addHandler(princessTapped, E_TAP);
    DRAGON_BTN.addHandler(dragonTapped, E_TAP);
    TUTORIAL.addHandler(tutorialTapped, E_TAP);
    START.addHandler(startTapped, E_TAP);
    ENDTUTORIAL.addHandler(endTutorialTapped, E_TAP);
    PLAYAGAIN.addHandler(playAgainTapped, E_TAP);
}

///////////////////////////////////////////////////////////////
// Put your main code here, to run repeatedly
///////////////////////////////////////////////////////////////
void loop()
{
    M5.update();
    if (deviceConnected) {
      if (gameState != S_GAME && gameState != S_GAME_OVER) {
        if (gameState == S_PLAYER_SELECT && screenUpdated) {
          chooseCharacter();
        } else if (gameState == S_TUTORIAL && screenUpdated) {
          startTutorial();
        }
      } else {
        timerHasBeenStarted = true;
        M5.Lcd.fillRect(200, 10, 50, 50, TFT_BLACK);
        M5.Lcd.fillRect(0, 0, 50, 50, TFT_BLACK);
        if (gameState == S_GAME) {
          checkTimeAndPrint();
          if (checkDistance()) {
            playGame();
            if (locationWasUpdated || playingAgain) {
            M5.Lcd.fillScreen(TFT_BLACK);
            drawCharacters(xServer, yServer, xClient, yClient); 

            bleReadXCharacteristic->setValue(xServer);
            bleReadYCharacteristic->setValue(yServer);
            
            bleReadXCharacteristic->notify();
            delay(10);
            bleReadYCharacteristic->notify();
            delay(10);
            playingAgain = false;
          }

          currTime = millis();
            if ((currTime - powerupStartTime < powerupTime) && (dragonPowerupActive || princessPowerupActive)) {
              addressPowerup();
              Serial.println("Powerup is active");
            } 
            
            if (!((currTime - powerupStartTime < powerupTime)) && (dragonPowerupActive || princessPowerupActive)) {
              Serial.println("Powerup has ended");
              playingAgain = true;
              if (dragonPowerupActive) {
                dragonPowerupActive = false;
              } else {
                princessPowerupActive = false;
              }
          }
        }
        locationWasUpdated = false;
        } else {
          if (gameEnded) {
            endGame();
            gameEnded = false;
          }
        }
      }
      screenUpdated = false;
    } else if (previouslyConnected) {
    }
}

///////////////////////////////////////////////////////////////
// Creates a game introduction
///////////////////////////////////////////////////////////////
void drawTitleScreen() {
  // Draw the background
  M5.Lcd.fillScreen(TFT_BLACK);
  drawCenteredBackgroundImage("cave", 2.25);

  // Draw the title text
  M5.Lcd.setTextColor(TFT_RED);
  M5.Lcd.setTextFont(2);
  M5.Lcd.setCursor(50, 30);
  M5.Lcd.println("Princess of");
  M5.Lcd.setCursor(120, 80);
  M5.Lcd.println("Fire");

  // Draw the subtitle text
  M5.Lcd.setTextFont(1);
  M5.Lcd.setCursor(50, 150);
  M5.Lcd.println("Hunt for the");
  M5.Lcd.setCursor(100, 180);
  M5.Lcd.println("Dragon");
  

  // Give you a good pause to see it (DRAMA)
  delay(2000);
}

///////////////////////////////////////////////////////////////
// Creates a waiting screen for connection
///////////////////////////////////////////////////////////////
void drawWaitingScreen() {
  // Change the background color
  M5.Lcd.fillScreen(TFT_BLACK);

  // Add image
  drawCenteredBackgroundImage("crossedSwords", 2.25);

  // Show waiting text
  M5.Lcd.setTextSize(2);
  M5.Lcd.setTextColor(TFT_RED);
  M5.Lcd.setCursor(10, 210);
  M5.Lcd.println("Waiting for More Players");
}

///////////////////////////////////////////////////////////////
// Creates a character select screen
///////////////////////////////////////////////////////////////
void chooseCharacter() {
// Draw the screen
  M5.Lcd.fillScreen(TFT_BLACK);
  drawSelectedCharacterName();
  PRINCESS_BTN.draw();
  DRAGON_BTN.draw();
  TUTORIAL.draw();
  START.draw();
}

///////////////////////////////////////////////////////////////
// Draws the name of the currently selected character
///////////////////////////////////////////////////////////////
void drawSelectedCharacterName() {
  M5.Lcd.setTextColor(TFT_WHITE);
  M5.Lcd.setCursor(120, 40);
  M5.Lcd.setTextSize(1);
  M5.Lcd.println("Character: ");
  if (chosenPlayer == UNCHOSEN) {
    M5.Lcd.setTextColor(TFT_BLUE);
    M5.Lcd.setCursor(120, 60);
    M5.Lcd.setTextSize(1);
    M5.Lcd.print("UNCHOSEN");
  } else if (chosenPlayer == PRINCESS) {
    M5.Lcd.setTextColor(TFT_PINK);
    M5.Lcd.setCursor(120, 60);
    M5.Lcd.setTextSize(1);
    M5.Lcd.print("PRINCESS");
  } else {
    M5.Lcd.setTextColor(TFT_GREEN);
    M5.Lcd.setCursor(120, 60);
    M5.Lcd.setTextSize(1);
    M5.Lcd.print("DRAGON");
  }
}

///////////////////////////////////////////////////////////////
// Selects Princess Character
///////////////////////////////////////////////////////////////
void princessTapped(Event& e) {
  if (opponentPlayer != PRINCESS) {
    chosenPlayer = PRINCESS;
    int chosenPlayerInt = 1;
    bleLocalPlayerSelectionCharacteristic->setValue(chosenPlayerInt);
    bleLocalPlayerSelectionCharacteristic->notify();
    delay(10);
    screenUpdated = true;
  }
}

///////////////////////////////////////////////////////////////
// Selects Dragon Character
///////////////////////////////////////////////////////////////
void dragonTapped(Event& e) {
  if (opponentPlayer != DRAGON) {
    chosenPlayer = DRAGON;
    int chosenPlayerInt = 2;
    bleLocalPlayerSelectionCharacteristic->setValue(chosenPlayerInt);
    bleLocalPlayerSelectionCharacteristic->notify();
    String val = bleLocalPlayerSelectionCharacteristic->getValue().c_str();
    Serial.print("NOTIFIED CLIENT OF VALUE: ");
    Serial.println(val);
    delay(10);
    screenUpdated = true;
  }
}

///////////////////////////////////////////////////////////////
// Starts tutorial
///////////////////////////////////////////////////////////////
void tutorialTapped(Event& e) {
 gameState = S_TUTORIAL;
 int gameStateLocal = 2;
 bleGameStateCharacteristic->setValue(gameStateLocal);
 bleGameStateCharacteristic->notify();
 screenUpdated = true;
}

///////////////////////////////////////////////////////////////
// Starts game
///////////////////////////////////////////////////////////////
void startTapped(Event& e) {
  hideButtons();
  if (opponentPlayer != UNCHOSEN && chosenPlayer != UNCHOSEN) {
    gameState = S_GAME;
    int gameStateLocal = 3;
    screenUpdated = true;
    bleGameStateCharacteristic->setValue(gameStateLocal);
    bleGameStateCharacteristic->notify();
    delay(10);
    prevTime = millis();
  }
}

void playAgainTapped(Event& e) {
  hideButtons();
  playingAgain = true;
  gameState = S_PLAYER_SELECT;
  chosenPlayer = UNCHOSEN;
  int player = 3;
  bleLocalPlayerSelectionCharacteristic->setValue(player);
  bleLocalPlayerSelectionCharacteristic->notify();
  delay(10);
  screenUpdated = true;
  int gameStateLocal = 1;
  bleGameStateCharacteristic->setValue(gameStateLocal);
  bleGameStateCharacteristic->notify();
  delay(10);
  powerupsLeft = 3;
  powerupsStillAvailable = true;
  prevTime = millis();
}

void hideButtons() {
  PRINCESS_BTN.hide();
  DRAGON_BTN.hide();
  TUTORIAL.hide();
  START.hide();
  ENDTUTORIAL.hide();
  PLAYAGAIN.hide();
}


///////////////////////////////////////////////////////////////
// Creates a tutorial screen
///////////////////////////////////////////////////////////////
void startTutorial() {
  M5.Lcd.fillScreen(TFT_BLACK);
  M5.Lcd.setCursor(0, 100);
  M5.Lcd.setTextColor(TFT_RED);
  M5.Lcd.setTextSize(1);
  ENDTUTORIAL.draw();

  M5.Lcd.println("- Princess has 2min to catch dragon");
  M5.Lcd.println("- Dragon see princess for 3sec (3x)");
  M5.Lcd.println("  * Press Select");
  M5.Lcd.println("- Princess can see dragon for 3sec (3x)");
  M5.Lcd.println("  * Press Select");
  M5.Lcd.println("- Timer: Top right");
  M5.Lcd.println("- Distance: Top left");
}

///////////////////////////////////////////////////////////
// Ends the tutorial
///////////////////////////////////////////////////////////////
void endTutorialTapped(Event& e) {
  gameState = S_PLAYER_SELECT;
  screenUpdated = true;
  int val = 1;
  bleGameStateCharacteristic->setValue(val);
  bleGameStateCharacteristic->notify();
  delay(10);
}

///////////////////////////////////////////////////////////////
// This code creates the BLE server and broadcasts it
///////////////////////////////////////////////////////////////
void broadcastBleServer() {    
    // Initializing the server, a service and a characteristic 
    Serial.println("Broadcasting!!!");
    bleServer = BLEDevice::createServer();
    Serial.println("Created Server");
    bleServer->setCallbacks(new MyServerCallbacks());
    Serial.println("Set Callbacks");
    bleService = bleServer->createService(BLEUUID(SERVICE_UUID), 32);
    Serial.println("Created Service");
    
    bleReadXCharacteristic = bleService->createCharacteristic(READ_X_CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_NOTIFY |
        BLECharacteristic::PROPERTY_INDICATE
    );
    bleReadXCharacteristic->setCallbacks(new MyCharacteristicCallbacks());
    Serial.println("Created Read X Characteristic");

    bleReadXCharacteristic->setValue(xServer);

    bleReadYCharacteristic = bleService->createCharacteristic(READ_Y_CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_NOTIFY |
        BLECharacteristic::PROPERTY_INDICATE
    );
    bleReadYCharacteristic->setValue(yServer);
    bleReadYCharacteristic->setCallbacks(new MyCharacteristicCallbacks());
    
    Serial.println("Created Read y Characteristic");
    
    bleReadWriteXCharacteristic = bleService->createCharacteristic(READ_WRITE_X_CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_WRITE
    );
    bleReadWriteXCharacteristic->setCallbacks(new MyCharacteristicCallbacks());

    Serial.println("Created Write X Characteristic");

    bleReadWriteYCharacteristic = bleService->createCharacteristic(READ_WRITE_Y_CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_WRITE
    );
    bleReadWriteYCharacteristic->setCallbacks(new MyCharacteristicCallbacks());

    Serial.println("Created Write Y Characteristic");

    //IMPORTANT: ADDED THESE
    bleLocalPlayerSelectionCharacteristic = bleService->createCharacteristic(LOCAL_PLAYER_SELECTION_UUID,
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_NOTIFY |
        BLECharacteristic::PROPERTY_INDICATE
    );
    bleLocalPlayerSelectionCharacteristic->setCallbacks(new MyCharacteristicCallbacks());

    Serial.println("Created Local Player Characteristic");

    bleOpponentPlayerSelectionCharacteristic = bleService->createCharacteristic(OPPONENT_PLAYER_SELECTION_UUID,
    BLECharacteristic::PROPERTY_WRITE
    );
    bleOpponentPlayerSelectionCharacteristic->setCallbacks(new MyCharacteristicCallbacks());

    Serial.println("Created Opponent Selection Characteristic");

    bleGameStateCharacteristic = bleService->createCharacteristic(GAME_STATE_UUID,
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_NOTIFY |
        BLECharacteristic::PROPERTY_INDICATE |
        BLECharacteristic::PROPERTY_WRITE
    );
    bleGameStateCharacteristic->setCallbacks(new MyCharacteristicCallbacks());

    Serial.println("Created game state Characteristic");

    bleService->start();

    // Start broadcasting (advertising) BLE service
    BLEAdvertising *bleAdvertising = BLEDevice::getAdvertising();
    bleAdvertising->addServiceUUID(SERVICE_UUID);
    bleAdvertising->setScanResponse(true);
    bleAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();
    Serial.println("Characteristic defined...you can connect with your phone!"); 
}

bool checkDistance() {
  long distance = abs(sqrt(pow((xServer - xClient), 2) + pow((yServer - yClient), 2)));
  if (distance <= 10) {
    screenUpdated = true;
    gameEnded = true;
    gameState = S_GAME_OVER;
    return false;
  }
  return true;
}
void printDistance() {
  long distance = abs(sqrt(pow((xServer - xClient), 2) + pow((yServer - yClient), 2)));
  M5.Lcd.fillRect(10, 20, 10, 10, TFT_BLACK);
  M5.Lcd.setCursor(10, 20);
  if (chosenPlayer == DRAGON) {
    M5.Lcd.setTextColor(TFT_PINK);
  } else {
    M5.Lcd.setTextColor(TFT_GREEN);
  }
  M5.Lcd.setTextSize(1);
  M5.Lcd.print(distance);
}

void serverAccelIncrement() {
  if (acceleration == 5) {
    acceleration = 1;
  } else {
    acceleration++;
  }
}

String milis_to_seconds(long milis) {
    unsigned long seconds = milis / 1000;
    String secondStr = seconds < 10 ? "0" + String(seconds) : String(seconds);
    unsigned long miliseconds = milis % 60;
    String milisecondsStr = miliseconds < 10 ? "0" + String(miliseconds) : String(miliseconds);
    return secondStr + "." + milisecondsStr + "s";
}

void endGame() {
  int val = 4;
  bleGameStateCharacteristic->setValue(val);
  bleGameStateCharacteristic->notify();
  delay(10);
  xServer = 10, yServer = 120;
  bleReadYCharacteristic->setValue(yServer);
  bleReadXCharacteristic->setValue(xServer);
  bleReadXCharacteristic->notify();
  delay(10);
  bleReadYCharacteristic->notify();
  delay(10);

  M5.Lcd.fillScreen(TFT_BLACK);
  M5.Lcd.setTextColor(TFT_RED);
  M5.Lcd.setTextSize(3);
  if (timeRanOut && chosenPlayer == PRINCESS) {
    M5.Lcd.drawString("YOU LOST", M5.Lcd.width() / 4, M5.Lcd.height() / 2 - 30);
  } else if (timeRanOut && chosenPlayer == DRAGON) {
      M5.Lcd.drawString("YOU WON", M5.Lcd.width() / 4, M5.Lcd.height() / 2 - 30);
  } else if (!timeRanOut && chosenPlayer == DRAGON)  {
    M5.Lcd.drawString("YOU LOST", M5.Lcd.width() / 4, M5.Lcd.height() / 2 - 30);
  } else {
    M5.Lcd.drawString("YOU WON", M5.Lcd.width() / 4, M5.Lcd.height() / 2 - 30);
  }
  PLAYAGAIN.draw();
  int chosenPlayerInt = 3;
  bleLocalPlayerSelectionCharacteristic->setValue(chosenPlayerInt);
  bleLocalPlayerSelectionCharacteristic->notify();
  delay(10);
}

void playGame() {
  printDistance();
  // Reverse x/y values to match joystick orientation
  int x = 1023 - gamePad.analogRead(14);
  int y = 1023 - gamePad.analogRead(15);

  // Left & Right For Joystick
  if (x > 600) {
    for (int i = 0; i < acceleration; i++) {
      if ((xServer + 1) < 320) {
        xServer++;
        locationWasUpdated = true;
      } else {
        xServer = 0;
        locationWasUpdated = true;
      }
    }
  } else if (x < 500) {
    for (int i = 0; i < acceleration; i++) {
      if ((xServer - 1) > 0) {
        xServer--;
        locationWasUpdated = true;
      } else {
        xServer = 320;
        locationWasUpdated = true;
      }
    }
  }

  if (y < 480) {
    for (int i = 0; i < acceleration; i++) {
      if ((yServer + 1) < 240) {
        yServer++;
        locationWasUpdated = true;
      } else {
        yServer = 0;
        locationWasUpdated = true;
      }
    }
  } else if (y > 560) {
    for (int i = 0; i < acceleration; i++) {
      if ((yServer - 1) > 0) {
        yServer--;
        locationWasUpdated = true;
      } else {
        yServer = 240;
        locationWasUpdated = true;
      }
    }
  }

  // For the gamepad buttons
  uint32_t buttons = gamePad.digitalReadBulk(button_mask);
  if (! (buttons & (1UL << BUTTON_SELECT))) {
    usePowerup();
  }
}

void usePowerup() {
  Serial.print("Made it to powerup");
  if (powerupsStillAvailable) {
    // If we're the dragon, set the bit to the princess
    if (chosenPlayer == DRAGON && !dragonPowerupActive) {
      powerupStartTime = millis();
      dragonPowerupActive = true;
      // Update the amount of powerups left 
      if (powerupsStillAvailable && powerupsLeft > 0) {
        powerupsLeft--; 
      } else {
        powerupsStillAvailable = false;
      }
      delay(10);
    } else if (chosenPlayer == PRINCESS && !princessPowerupActive) {
      powerupStartTime = millis();
      princessPowerupActive = true;
      // Update the amount of powerups left 
      if (powerupsStillAvailable && powerupsLeft > 0) {
        powerupsLeft--; 
      } else {
        powerupsStillAvailable = false;
      }
    }
  }
  screenUpdated = true;
  locationWasUpdated = true;
}

void addressPowerup() {
  if (dragonPowerupActive && chosenPlayer == DRAGON) {
    M5.Lcd.fillScreen(TFT_BLACK);
    drawCharacters(xServer, yServer, xClient, yClient); 
    M5.Lcd.drawPixel(xClient, yClient, TFT_PINK);
  }

  if (princessPowerupActive && chosenPlayer == PRINCESS) {
    M5.Lcd.fillScreen(TFT_BLACK);
    drawCharacters(xServer, yServer, xClient, yClient); 
    M5.Lcd.drawPixel(xClient, yClient, TFT_GREEN);
  }
}

void drawCharacters(uint32_t serverX, uint32_t serverY, uint32_t clientX, uint32_t clientY) {
  if (chosenPlayer == PRINCESS) {
    drawCharacterImage("princess", 1, serverX, serverY);
  } else {
    drawCharacterImage("dragon", 1, serverX, serverY);
  }
}

void checkTimeAndPrint() {
  currTime = millis();
  unsigned long remainingTime = 0;
  if (countdownTime - (currTime - prevTime) >= 0) {
    remainingTime = countdownTime - (currTime - prevTime);
  }
  if (remainingTime <= 0) {
    Serial.print("Made it to game over");
    gameState = S_GAME_OVER;
    int val = 4; 
    bleGameStateCharacteristic->setValue(val);
    bleGameStateCharacteristic->notify();
    delay(10);
    timeRanOut = true;
    gameEnded = true;
    screenUpdated = true;
    endGame();
  }
  unsigned long minutes = (remainingTime / 1000) / 60;
  unsigned long seconds = (remainingTime  / 1000) % 60;
  
  if (gameState == S_GAME && remainingTime >= 0) {
    M5.Lcd.setTextSize(1);
    M5.Lcd.setCursor(210, 20);
    M5.Lcd.print(minutes);
    M5.Lcd.print(":");
    if (seconds < 10) {
      String secs = String(seconds);
      M5.Lcd.print("0"+secs);
    } else {
      M5.Lcd.print(seconds);
    }
  }
}

/////////////////////////////////////////////////////////////////
// This method takes in an image icon string (from API) and a 
// resize multiple and draws the corresponding image (bitmap byte
// arrays found in EGR425_Phase1_weather_bitmap_images.h) to scale (for 
// example, if resizeMult==2, will draw the image as 200x200 instead
// of the native 100x100 pixels) on the right-hand side of the
// screen (centered vertically). 
/////////////////////////////////////////////////////////////////
void drawCenteredBackgroundImage(String iconName, int resizeMult) {
    // Get the corresponding byte array
    const uint16_t * iconBitmap = getIconBitmap(iconName);

    // Compute offsets so that the image is centered vertically and is
    // right-aligned
    int yOffset = -(resizeMult * imgSqDim - M5.Lcd.height()) / 2;
    // int xOffset = sWidth - (imgSqDim*resizeMult*.8); // Right align (image doesn't take up entire array)
    int xOffset = (M5.Lcd.width() / 2) - (imgSqDim * resizeMult / 2); // center horizontally
    
    // Iterate through each pixel of the imgSqDim x imgSqDim (100 x 100) array
    for (int y = 0; y < imgSqDim; y++) {
        for (int x = 0; x < imgSqDim; x++) {
            // Compute the linear index in the array and get pixel value
            int pixNum = (y * imgSqDim) + x;
            uint16_t pixel = iconBitmap[pixNum];

            // If the pixel is black, do NOT draw (treat it as transparent);
            // otherwise, draw the value
            if (pixel != 0) {
                // 16-bit RBG565 values give the high 5 pixels to red, the middle
                // 6 pixels to green and the low 5 pixels to blue as described
                // here: http://www.barth-dev.de/online/rgb565-color-picker/
                byte red = (pixel >> 11) & 0b0000000000011111;
                red = red << 3;
                byte green = (pixel >> 5) & 0b0000000000111111;
                green = green << 2;
                byte blue = pixel & 0b0000000000011111;
                blue = blue << 3;

                // Scale image; for example, if resizeMult == 2, draw a 2x2
                // filled square for each original pixel
                for (int i = 0; i < resizeMult; i++) {
                    for (int j = 0; j < resizeMult; j++) {
                        int xDraw = x * resizeMult + i + xOffset;
                        int yDraw = y * resizeMult + j + yOffset;
                        M5.Lcd.drawPixel(xDraw, yDraw, M5.Lcd.color565(red, green, blue));
                    }
                }
            }
        }
    }
}

void drawCharacterImage(String iconName, int resizeMult, int xLoc, int yLoc) {
    // Get the corresponding byte array
    const uint16_t * iconBitmap = getIconBitmap(iconName);

    // Compute offsets so that the image is centered vertically and is
    // right-aligned
    int yOffset = yLoc - (imgSqDim * resizeMult / 2); // center vertically    // int xOffset = sWidth - (imgSqDim*resizeMult*.8); // Right align (image doesn't take up entire array)
    int xOffset = xLoc - (imgSqDim * resizeMult / 2); // center horizontally
    
    // Iterate through each pixel of the imgSqDim x imgSqDim (100 x 100) array
    for (int y = 0; y < imgSqDim; y++) {
        for (int x = 0; x < imgSqDim; x++) {
            // Compute the linear index in the array and get pixel value
            int pixNum = (y * imgSqDim) + x;
            uint16_t pixel = iconBitmap[pixNum];

            // If the pixel is black, do NOT draw (treat it as transparent);
            // otherwise, draw the value
            if (pixel != 0) {
                // 16-bit RBG565 values give the high 5 pixels to red, the middle
                // 6 pixels to green and the low 5 pixels to blue as described
                // here: http://www.barth-dev.de/online/rgb565-color-picker/
                byte red = (pixel >> 11) & 0b0000000000011111;
                red = red << 3;
                byte green = (pixel >> 5) & 0b0000000000111111;
                green = green << 2;
                byte blue = pixel & 0b0000000000011111;
                blue = blue << 3;

                // Scale image; for example, if resizeMult == 2, draw a 2x2
                // filled square for each original pixel
                for (int i = 0; i < resizeMult; i++) {
                    for (int j = 0; j < resizeMult; j++) {
                        int xDraw = x * resizeMult + i + xOffset;
                        int yDraw = y * resizeMult + j + yOffset;
                        M5.Lcd.drawPixel(xDraw, yDraw, M5.Lcd.color565(red, green, blue));
                    }
                }
            }
        }
    }
}