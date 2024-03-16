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
bool chooseCharacterScreenUpdated = false;

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
// TODO: Add Ids for each player's player type (2) and when an opponents powerup is used (2), and gamestate (1)
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
Button ENDTUTORIAL(100, 60, 100, 50, false, "X", offColTut, onCol);
Button PLAYAGAIN(100, 190, 100, 50, false, "Start", offColStart, onCol);

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

///////////////////////////////////////////////////////////////
// BLE Server Callback Methods
///////////////////////////////////////////////////////////////
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer *pServer) {
        deviceConnected = true;
        chooseCharacterScreenUpdated = true;
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
              gameState = S_GAME;
            } else {
              gameState = S_GAME_OVER;
            }
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

void serverAccelIncrement();
String milis_to_seconds(long milis);


void playGame();
void endGame();
bool checkDistance();
void warpDot();

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
    
    // Broadcast the BLE server
    broadcastBleServer();

    // Draw the waiting for second player screen
    drawWaitingScreen();

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
}

///////////////////////////////////////////////////////////////
// Put your main code here, to run repeatedly
///////////////////////////////////////////////////////////////
void loop()
{
    M5.update();
    if (deviceConnected) {
      if (gameState != S_GAME && gameState != S_GAME_OVER) {
        if (gameState == S_PLAYER_SELECT && chooseCharacterScreenUpdated) {
          chooseCharacter();
        } else if (gameState == S_TUTORIAL) {
          startTutorial();
        }
      } else {
        timerHasBeenStarted = true;
        checkTimeAndPrint();
        bool stillPlaying = checkDistance();
        if (gameState == S_GAME && stillPlaying) {
          playGame();
          if (locationWasUpdated) {
          bleReadXCharacteristic->setValue(xServer);
          bleReadYCharacteristic->setValue(yServer);
          
          bleReadXCharacteristic->notify();
          delay(10);
          bleReadYCharacteristic->notify();
          delay(10);
        }
        locationWasUpdated = false;
        } else {
          endGame();
          delay(50000);
        }
      }
      chooseCharacterScreenUpdated = false;
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
    chooseCharacterScreenUpdated = true;
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
    chooseCharacterScreenUpdated = true;
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
}

///////////////////////////////////////////////////////////////
// Starts game
///////////////////////////////////////////////////////////////
void startTapped(Event& e) {
  if (opponentPlayer != UNCHOSEN && chosenPlayer != UNCHOSEN) {
    gameState = S_GAME;
    int gameStateLocal = 3;
    bleGameStateCharacteristic->setValue(gameStateLocal);
    bleGameStateCharacteristic->notify();
  }
}

void playAgainTapped(Event& e) {
  gameState = S_PLAYER_SELECT;
  int gameStateLocal = 1;
  bleGameStateCharacteristic->setValue(gameStateLocal);
  bleGameStateCharacteristic->notify();
}


///////////////////////////////////////////////////////////////
// Creates a tutorial screen
///////////////////////////////////////////////////////////////
void startTutorial() {

}

///////////////////////////////////////////////////////////
// Ends the tutorial
///////////////////////////////////////////////////////////////
void endTutorialTapped(Event& e) {
  gameState = S_PLAYER_SELECT;
  chooseCharacterScreenUpdated;
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
    bleService = bleServer->createService(SERVICE_UUID);
    Serial.println("Created Service");
    
    bleReadXCharacteristic = bleService->createCharacteristic(READ_X_CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_NOTIFY |
        BLECharacteristic::PROPERTY_INDICATE
    );
    bleReadXCharacteristic->setCallbacks(new MyCharacteristicCallbacks());
    Serial.println("Created Characteristic");

    bleReadXCharacteristic->setValue(xServer);
    Serial.println("set value");

    bleReadYCharacteristic = bleService->createCharacteristic(READ_Y_CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_NOTIFY |
        BLECharacteristic::PROPERTY_INDICATE
    );
    bleReadYCharacteristic->setValue(yServer);
    bleReadYCharacteristic->setCallbacks(new MyCharacteristicCallbacks());

    bleReadWriteXCharacteristic = bleService->createCharacteristic(READ_WRITE_X_CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_WRITE
    );
    bleReadWriteXCharacteristic->setCallbacks(new MyCharacteristicCallbacks());

    bleReadWriteYCharacteristic = bleService->createCharacteristic(READ_WRITE_Y_CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_WRITE
    );
    bleReadWriteYCharacteristic->setCallbacks(new MyCharacteristicCallbacks());
    bleService->start();

    //IMPORTANT: ADDED THESE
    bleLocalPlayerSelectionCharacteristic = bleService->createCharacteristic(LOCAL_PLAYER_SELECTION_UUID,
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_NOTIFY |
        BLECharacteristic::PROPERTY_INDICATE
    );
    bleLocalPlayerSelectionCharacteristic->setCallbacks(new MyCharacteristicCallbacks());
    bleService->start();

    bleOpponentPlayerSelectionCharacteristic = bleService->createCharacteristic(OPPONENT_PLAYER_SELECTION_UUID,
    BLECharacteristic::PROPERTY_WRITE
    );
    bleOpponentPlayerSelectionCharacteristic->setCallbacks(new MyCharacteristicCallbacks());
    bleService->start();

    bleGameStateCharacteristic = bleService->createCharacteristic(GAME_STATE_UUID,
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_NOTIFY |
        BLECharacteristic::PROPERTY_INDICATE |
        BLECharacteristic::PROPERTY_WRITE
    );
    bleGameStateCharacteristic->setCallbacks(new MyCharacteristicCallbacks());
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
    return false;
  }
  return true;
}
void printDistance() {
  long distance = abs(sqrt(pow((xServer - xClient), 2) + pow((yServer - yClient), 2)));
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
}

void playGame() {
  M5.Lcd.fillScreen(TFT_BLACK);
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
      }
    }
  } else if (x < 500) {
    for (int i = 0; i < acceleration; i++) {
      if ((xServer - 1) > 0) {
        xServer--;
        locationWasUpdated = true;
      }
    }
  }

  if (y < 480) {
    for (int i = 0; i < acceleration; i++) {
      if ((yServer + 1) < 240) {
        yServer++;
        locationWasUpdated = true;
      }
    }
  } else if (y > 560) {
    for (int i = 0; i < acceleration; i++) {
      if ((yServer - 1) > 0) {
        yServer--;
        locationWasUpdated = true;
      }
    }
  }

  // For the gamepad buttons
  uint32_t buttons = gamePad.digitalReadBulk(button_mask);
  if (! (buttons & (1UL << BUTTON_SELECT))) {
    serverAccelIncrement();
    Serial.print("Button Accel: "); Serial.print(acceleration);
    delay(500);
  }
  if (! (buttons & (1UL << BUTTON_START))) {
    warpDot();
    delay(1000);
  }
  drawCharacters(xServer, yServer, xClient, yClient); 
}

void warpDot() {
  // create random ints for x and y and have the dot drawn there next
  int randx = rand() % M5.Lcd.width();
  int randy = rand() % M5.Lcd.height();
  xServer = randx;
  yServer = randy;
  
  bleReadXCharacteristic->setValue(xServer);
  bleReadYCharacteristic->setValue(yServer);

  bleReadXCharacteristic->notify();
  delay(10);
  bleReadYCharacteristic->notify();
  delay(10);
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
  unsigned long remainingTime = countdownTime - (currTime - prevTime);
  if (remainingTime <= 0) {
    gameState = S_GAME_OVER;
    int val = 4; 
    bleGameStateCharacteristic->setValue(val);
    bleGameStateCharacteristic->notify();
    timeRanOut = true;
  }
  unsigned long minutes = remainingTime / 60;
  unsigned long seconds = remainingTime % 60;
  M5.Lcd.setTextColor(TFT_WHITE);
  M5.Lcd.setTextSize(1);
  M5.Lcd.setCursor(210, 20);
  M5.Lcd.print(minutes);
  M5.Lcd.print(":");
  M5.Lcd.print(seconds);
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