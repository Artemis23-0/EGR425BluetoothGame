///////////////////////////////////////////////////////////////
// Imports
///////////////////////////////////////////////////////////////
#include <BLEDevice.h>
#include <BLE2902.h>
#include <M5Core2.h>
#include <Adafruit_seesaw.h>
#include "../include/game_image_bitmaps.h"

///////////////////////////////////////////////////////////////
// Variables
///////////////////////////////////////////////////////////////

//Connection Variables
static BLEAdvertisedDevice *bleRemoteServer;
static boolean doConnect = false;
static boolean doScan = false;
bool deviceConnected = false;


// Location Characteristics/Variables
BLERemoteCharacteristic *bleReadXCharacteristic;
BLERemoteCharacteristic *bleReadYCharacteristic;
BLERemoteCharacteristic *bleReadWriteXCharacteristic;
BLERemoteCharacteristic *bleReadWriteYCharacteristic;
bool locationWasUpdated = true;

// Gameplay Characteristics/Variables
bool chooseCharacterScreenUpdated = false;
// TODO: Add characteristics for each player's player type (2) and when an opponents powerup is used (2), and gamestate (1)
BLERemoteCharacteristic *bleLocalPlayerSelectionCharacteristic; // Our local, corresponds to server's opponent
BLERemoteCharacteristic *bleOpponentPlayerSelectionCharacteristic; // Our opponent, corresponds to server's local
BLERemoteCharacteristic *bleGameStateCharacteristic;

// Location Unique IDs
static BLEUUID SERVICE_UUID("7d7a7768-a9d0-4fb8-bf2b-fc994c662eb6");
static BLEUUID READ_X_CHARACTERISTIC_UUID("563c64b2-9634-4f7a-9f4f-d9e3231faa56");
static BLEUUID READ_Y_CHARACTERISTIC_UUID("aa88ac15-3e2b-4735-92ff-4c712173e9f3");
static BLEUUID READ_WRITE_X_CHARACTERISTIC_UUID("1da468d6-993d-4387-9e71-1c826b10fff9");
static BLEUUID READ_WRITE_Y_CHARACTERISTIC_UUID("cf7b4787-d412-4e69-8b61-e2cfba89ff19");

// Gameplay Unique IDs
static BLEUUID LOCAL_PLAYER_SELECTION_UUID("cad4571b-2ca1-47c9-ae9d-75bbce0d814f"); // REMEMBER IT CORRESPONDS TO SERVER'S OPPONENT
static BLEUUID OPPONENT_PLAYER_SELECTION_UUID("ecaaac5c-5057-49dc-83ab-e0e2322f2703"); // REMEMBER IT CORRESPONDS TO SERVER'S LOCAL
static BLEUUID GAME_STATE_UUID("07471f02-b963-449e-a39c-9a44ae312b78");


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

// coordinates

int xServer = 0, yServer = 0, xClient = 300, yClient = 120;

// acceleration
int acceleration = 5;

// Timer
int countdownTime = 120000; // Two minutes
int prevTime = 0;
int currTime = 0;
bool timerHasBeenStarted = false; 
bool timeRanOut = false;

///////////////////////////////////////////////////////////////
// Forward Declarations
///////////////////////////////////////////////////////////////

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

void clientAccelIncrement();
String milis_to_seconds(long milis);
void playGame();
void endGame();
bool checkDistance();
void warpDot();

///////////////////////////////////////////////////////////////
// BLE Client Callback Methods
// This method is called when the server that this client is
// connected to NOTIFIES this client (or any client listening)
// that it has changed the remote characteristic
///////////////////////////////////////////////////////////////
static void notifyXCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify)
{
    String characteristicUUID = pBLERemoteCharacteristic->getUUID().toString().c_str();
    Serial.printf("Notify callback for characteristic %s of data length %d\n", pBLERemoteCharacteristic->getUUID().toString().c_str(), length);
      xServer = (int32_t)(pData[3] << 24 | pData[2] << 16 | pData[1] << 8 | pData[0]);
      Serial.printf("\tValue was: %i", xServer);
      delay(10);
}

static void notifyYCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify)
{
    String characteristicUUID = pBLERemoteCharacteristic->getUUID().toString().c_str();
    Serial.printf("Notify callback for characteristic %s of data length %d\n", pBLERemoteCharacteristic->getUUID().toString().c_str(), length);
          yServer = (int32_t)(pData[3] << 24 | pData[2] << 16 | pData[1] << 8 | pData[0]);
      Serial.printf("\tValue was: %i", yServer);
      delay(10);
}

static void notifyOpponentCharacterCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify)
{
    String characteristicUUID = pBLERemoteCharacteristic->getUUID().toString().c_str();
    Serial.printf("Notify callback for characteristic %s of data length %d\n", pBLERemoteCharacteristic->getUUID().toString().c_str(), length);
    Serial.print("VAL fucker:");
    Serial.println((int32_t)pData);
    int32_t opponentVal = (int32_t)(pData[3] << 24 | pData[2] << 16 | pData[1] << 8 | pData[0]);
    if (opponentVal == 1) {
      opponentPlayer = PRINCESS;
      Serial.println("\tOpponent is: Princess");
    } else if (opponentVal == 2) {
      opponentPlayer = DRAGON;
      Serial.println("\tOpponent is: Dragon");
    } else {
      opponentPlayer = UNCHOSEN;
      Serial.println("\tOpponent is: Unchosen");
    }
    Serial.printf("\tValue was: %i", opponentVal);
    delay(10);
}

static void notifyGameStateCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify)
{
    String characteristicUUID = pBLERemoteCharacteristic->getUUID().toString().c_str();
    Serial.printf("Notify callback for characteristic %s of data length %d\n", pBLERemoteCharacteristic->getUUID().toString().c_str(), length);
    int32_t gameVal = (int32_t)(pData[3] << 24 | pData[2] << 16 | pData[1] << 8 | pData[0]);
    if (gameVal == 1 || gameVal == 2) {
      gameState = gameState;
    } else if (gameVal == 3) {
      gameState = S_GAME;
    } else {
      gameState = S_GAME_OVER;
    }
    Serial.printf("\tValue was: %i", gameVal);
    delay(10);
}

///////////////////////////////////////////////////////////////
// BLE Server Callback Method
// These methods are called upon connection and disconnection
// to BLE service.
///////////////////////////////////////////////////////////////
class MyClientCallback : public BLEClientCallbacks
{
    void onConnect(BLEClient *pclient)
    {
        deviceConnected = true;
        chooseCharacterScreenUpdated = true;
        Serial.println("Device connected...");
    }

    void onDisconnect(BLEClient *pclient)
    {
        deviceConnected = false;
        chooseCharacterScreenUpdated = false;
        Serial.println("Device disconnected...");
    }
};

///////////////////////////////////////////////////////////////
// Method is called to connect to server
///////////////////////////////////////////////////////////////
bool connectToServer()
{
    // Create the client
    Serial.printf("Forming a connection to %s\n", bleRemoteServer->getName().c_str());
    BLEClient *bleClient = BLEDevice::createClient();
    bleClient->setClientCallbacks(new MyClientCallback());
    Serial.println("\tClient connected");

    // Connect to the remote BLE Server.
    if (!bleClient->connect(bleRemoteServer))
        Serial.printf("FAILED to connect to server (%s)\n", bleRemoteServer->getName().c_str());
    Serial.printf("\tConnected to server (%s)\n", bleRemoteServer->getName().c_str());

    // Obtain a reference to the service we are after in the remote BLE server.
    BLERemoteService *bleRemoteService = bleClient->getService(SERVICE_UUID);
    if (bleRemoteService == nullptr) {
        Serial.printf("Failed to find our service UUID: %s\n", SERVICE_UUID.toString().c_str());
        bleClient->disconnect();
        return false;
    }
    Serial.printf("\tFound our service UUID: %s\n", SERVICE_UUID.toString().c_str());

    // Obtain a reference to the characteristic in the service of the remote BLE server.
    bleReadXCharacteristic = bleRemoteService->getCharacteristic(READ_X_CHARACTERISTIC_UUID);
    if (bleReadXCharacteristic == nullptr) {
        Serial.printf("Failed to find our characteristic UUID: %s\n", READ_X_CHARACTERISTIC_UUID.toString().c_str());
        bleClient->disconnect();
        return false;
    }
    Serial.printf("\tFound our characteristic UUID: %s\n", READ_X_CHARACTERISTIC_UUID.toString().c_str());
    bleReadYCharacteristic = bleRemoteService->getCharacteristic(READ_Y_CHARACTERISTIC_UUID);
    if (bleReadYCharacteristic == nullptr) {
        Serial.printf("Failed to find our characteristic UUID: %s\n", READ_Y_CHARACTERISTIC_UUID.toString().c_str());
        bleClient->disconnect();
        return false;
    }
    Serial.printf("\tFound our characteristic UUID: %s\n", READ_Y_CHARACTERISTIC_UUID.toString().c_str());

    
    bleReadWriteXCharacteristic = bleRemoteService->getCharacteristic(READ_WRITE_X_CHARACTERISTIC_UUID);
    if (bleReadWriteXCharacteristic == nullptr) {
        Serial.printf("Failed to find our characteristic UUID: %s\n", READ_WRITE_X_CHARACTERISTIC_UUID.toString().c_str());
        bleClient->disconnect();
        return false;
    }
    Serial.printf("\tFound our characteristic UUID: %s\n", READ_WRITE_X_CHARACTERISTIC_UUID.toString().c_str());

    bleReadWriteYCharacteristic = bleRemoteService->getCharacteristic(READ_WRITE_Y_CHARACTERISTIC_UUID);
    if (bleReadWriteYCharacteristic == nullptr) {
        Serial.printf("Failed to find our characteristic UUID: %s\n", READ_WRITE_Y_CHARACTERISTIC_UUID.toString().c_str());
        bleClient->disconnect();
        return false;
    }
    Serial.printf("\tFound our characteristic UUID: %s\n", READ_WRITE_Y_CHARACTERISTIC_UUID.toString().c_str());
    
    // ADDED THESE

    bleOpponentPlayerSelectionCharacteristic = bleRemoteService->getCharacteristic(OPPONENT_PLAYER_SELECTION_UUID);
    if (bleOpponentPlayerSelectionCharacteristic == nullptr) {
        Serial.printf("Failed to find our characteristic UUID: %s\n", OPPONENT_PLAYER_SELECTION_UUID.toString().c_str());
        bleClient->disconnect();
        return false;
    }
    Serial.printf("\tFound our characteristic UUID: %s\n", OPPONENT_PLAYER_SELECTION_UUID.toString().c_str());

    bleGameStateCharacteristic = bleRemoteService->getCharacteristic(GAME_STATE_UUID);
    if (bleGameStateCharacteristic == nullptr) {
        Serial.printf("Failed to find our characteristic UUID: %s\n", GAME_STATE_UUID.toString().c_str());
        bleClient->disconnect();
        return false;
    }
    Serial.printf("\tFound our characteristic UUID: %s\n", GAME_STATE_UUID.toString().c_str());

    bleLocalPlayerSelectionCharacteristic = bleRemoteService->getCharacteristic(LOCAL_PLAYER_SELECTION_UUID);
    if (bleGameStateCharacteristic == nullptr) {
        Serial.printf("Failed to find our characteristic UUID: %s\n", LOCAL_PLAYER_SELECTION_UUID.toString().c_str());
        bleClient->disconnect();
        return false;
    }
    Serial.printf("\tFound our characteristic UUID: %s\n", LOCAL_PLAYER_SELECTION_UUID.toString().c_str());

    // Check if server's characteristic can notify client of changes and register to listen if so
    if (bleReadXCharacteristic->canNotify()) {
      Serial.println("X can notify");
      bleReadXCharacteristic->registerForNotify(notifyXCallback);
    }
    if (bleReadYCharacteristic->canNotify()) {
      Serial.println("Y can notify");
      bleReadYCharacteristic->registerForNotify(notifyYCallback);
    }
    if (bleOpponentPlayerSelectionCharacteristic->canNotify()) {
      Serial.println("X can notify");
      bleOpponentPlayerSelectionCharacteristic->registerForNotify(notifyOpponentCharacterCallback);
    }
    if (bleGameStateCharacteristic->canNotify()) {
      Serial.println("X can notify");
      bleGameStateCharacteristic->registerForNotify(notifyGameStateCallback);
    }
    return true;
}

///////////////////////////////////////////////////////////////
// Scan for BLE servers and find the first one that advertises
// the service we are looking for.
///////////////////////////////////////////////////////////////
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{
    /**
     * Called for each advertising BLE server.
     */
    void onResult(BLEAdvertisedDevice advertisedDevice)
    {
        // Print device found
        Serial.print("BLE Advertised Device found:");
        Serial.printf("\tName: %s\n", advertisedDevice.getName().c_str());

        if (advertisedDevice.haveServiceUUID() && 
                advertisedDevice.isAdvertisingService(SERVICE_UUID) && 
                advertisedDevice.getName() == "Princess of Fire") {
            BLEDevice::getScan()->stop();
            bleRemoteServer = new BLEAdvertisedDevice(advertisedDevice);
            doConnect = true;
            doScan = true;
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

    // Init M5Core2 as a BLE Client
    Serial.print("Starting BLE...");
    String bleClientDeviceName = "";
    BLEDevice::init(bleClientDeviceName.c_str());

    // Draw the game intro screen
    drawTitleScreen();

    // Retrieve a Scanner and set the callback we want to use to be informed when we
    // have detected a new device.  Specify that we want active scanning and start the
    // scan to run for 5 seconds.
    BLEScan *pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    pBLEScan->setInterval(1349);
    pBLEScan->setWindow(449);
    pBLEScan->setActiveScan(true);
    pBLEScan->start(0, false);

    // Draw the waiting for second player screen
    drawWaitingScreen();
    
    // Gameplay setup
    if(!gamePad.begin(0x50)){
        Serial.println("ERROR! seesaw not found");
        while(1) delay(1);
    }
    gamePad.pinModeBulk(button_mask, INPUT_PULLUP);
    gamePad.setGPIOInterrupts(button_mask, 1);

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
    
    // If the flag "doConnect" is true then we have scanned for and found the desired
    // BLE Server with which we wish to connect.  Now we connect to it.  Once we are
    // connected we set the connected flag to be false.
    if (doConnect == true)
    {
        if (connectToServer()) {
            Serial.println("We are now connected to the BLE Server.");
            String x = String(xClient);
            bleReadWriteXCharacteristic->writeValue(x.c_str(), false);
            String y = String(yClient);
            bleReadWriteYCharacteristic->writeValue(y.c_str(), false);
            doConnect = false;
            delay(3000);
        }
        else {
            Serial.println("We have failed to connect to the server; there is nothin more we will do.");
            delay(3000);
        }
    }

    // If we are connected to a peer BLE Server, update the characteristic each time we are reached
    // with the current time since boot.
    if (deviceConnected)
    {
      if (gameState != S_GAME && gameState != S_GAME_OVER) {
        if (gameState == S_PLAYER_SELECT && chooseCharacterScreenUpdated) {
          chooseCharacter();
        } else if (gameState == S_TUTORIAL) {
          startTutorial();
        }
        prevTime = millis();
      } else {
        timerHasBeenStarted = true;
        checkTimeAndPrint();
        bool stillPlaying = checkDistance();
          if (gameState == S_GAME && stillPlaying) {
            playGame();
            locationWasUpdated = false;
        } else {
            endGame();
            delay(50000);
        }
      }
      chooseCharacterScreenUpdated = false;
    } else if (doScan) {
        BLEDevice::getScan()->start(0); // this is just example to start scan after disconnect, most likely there is better way to do it in arduino
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

void princessTapped(Event& e) {
  if (opponentPlayer != PRINCESS) {
    chosenPlayer = PRINCESS;
    String val = String(1);
    bleLocalPlayerSelectionCharacteristic->writeValue(val.c_str(), false);
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
    String val = String(2);
    bleLocalPlayerSelectionCharacteristic->writeValue(val.c_str(), false);
    delay(10);
    chooseCharacterScreenUpdated = true;
  }
}

///////////////////////////////////////////////////////////////
// Starts tutorial
///////////////////////////////////////////////////////////////
void tutorialTapped(Event& e) {
  gameState = S_TUTORIAL;
  String val = String(2);
  bleGameStateCharacteristic->writeValue(val.c_str(), false);
  delay(10);
 }

///////////////////////////////////////////////////////////////
// Starts game
///////////////////////////////////////////////////////////////
void startTapped(Event& e) {
  if (opponentPlayer != UNCHOSEN && chosenPlayer != UNCHOSEN) {
    gameState = S_GAME;
    String val = String(3);
    bleGameStateCharacteristic->writeValue(val.c_str(), false);
    delay(10);
  }
}

void playAgainTapped(Event& e) {
  gameState = S_PLAYER_SELECT;
  String val = String(1);
  bleGameStateCharacteristic->writeValue(val.c_str(), false);
  delay(10);
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

String milis_to_seconds(long milis) {
    unsigned long seconds = milis / 1000;
    String secondStr = seconds < 10 ? "0" + String(seconds) : String(seconds);
    unsigned long miliseconds = milis % 60;
    String milisecondsStr = miliseconds < 10 ? "0" + String(miliseconds) : String(miliseconds);
    return secondStr + "." + milisecondsStr + "s";
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

void checkTimeAndPrint() {
  currTime = millis();
  unsigned long remainingTime = countdownTime - (currTime - prevTime);
  if (remainingTime <= 0) {
    gameState = S_GAME_OVER;
    String x = String(4);
    bleGameStateCharacteristic->writeValue(x.c_str(), false);
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

void clientAccelIncrement() {
  if (acceleration == 5) {
    acceleration = 1;
  } else {
    acceleration++;
  }
}

void endGame() {
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

void drawCharacters(uint32_t serverX, uint32_t serverY, uint32_t clientX, uint32_t clientY){
  if (chosenPlayer == PRINCESS) {
    drawCharacterImage("princess", 1, clientX, clientY);
  } else {
    drawCharacterImage("dragon", 1, clientX, clientY);
  }
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
      if ((xClient + 1) < 320) {
        xClient++;
        String x = String(xClient);
        bleReadWriteXCharacteristic->writeValue(x.c_str(), false);
      }
    }
  } else if (x < 500) {
    for (int i = 0; i < acceleration; i++) {
      if ((xClient - 1) > 0) {
        xClient--;
        String x = String(xClient);
        bleReadWriteXCharacteristic->writeValue(x.c_str(), false);
      }
    }
  }

  if (y < 480) {
    for (int i = 0; i < acceleration; i++) {
      if ((yClient + 1) < 240) {
        yClient++;
        String y = String(yClient);
        bleReadWriteYCharacteristic->writeValue(y.c_str(), false);
      }
    }
  } else if (y > 560) {
    for (int i = 0; i < acceleration; i++) {
      if ((yClient - 1) > 0) {
        yClient--;
        String y = String(yClient);
        bleReadWriteYCharacteristic->writeValue(y.c_str(), false);
      }
    }
  }

  // For the gamepad buttons
  uint32_t buttons = gamePad.digitalReadBulk(button_mask);
  if (! (buttons & (1UL << BUTTON_SELECT))) {
    clientAccelIncrement();
    Serial.print("Button Accel: "); Serial.print(acceleration);
    delay(500);
  }
  if (! (buttons & (1UL << BUTTON_START))) {
    warpDot();
    delay(500);
  }
  drawCharacters(xServer, yServer, xClient, yClient); 
}

void warpDot() {
  // create random ints for x and y and have the dot drawn there next
  int randx = rand() % M5.Lcd.width();
  int randy = rand() % M5.Lcd.height();
  xClient = randx;
  yClient = randy;
  
  String x = String(xClient);
  String y = String(yClient);
  bleReadWriteXCharacteristic->writeValue(x.c_str(), false); //x.length()
  bleReadWriteYCharacteristic->writeValue(y.c_str(), false); //y.length()
  drawCharacters(xServer, yServer, xClient, yClient);
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