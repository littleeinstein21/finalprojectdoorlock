#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_Fingerprint.h>
#include <Keypad.h>
#include <SoftwareSerial.h>

// Configuration for I2C LCD
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Configuration for SoftwareSerial for fingerprint sensor
SoftwareSerial mySerial(2, 3);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

// Configuration for keypad
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};
byte rowPins[ROWS] = {4, 5, 6, 7};
byte colPins[COLS] = {8, 9, 10, 11};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// Define relay and buzzer pins
const int relayPin = 12;
const int relayPin2 = A0; // Second relay pin
const int buzzerPin = A1; // Buzzer pin
const int limitSwitchPin = 13; // Limit switch pin

// Function declarations
void setupSystem();
void enrollFingerprint();
void deleteFingerprint();
void lockSystem();
void unlockSystem();
void resetSystem();
bool isFingerprintMatched();
void setLocked(bool locked);
void handleEmergencyUnlock(char key);
void handleFingerprintDeletion();

bool isLocked = true; // State of the lock system
bool enteringEmergencyPassword = false; // State of emergency password entry
bool waitingForFingerprintDeletion = false; // State for fingerprint deletion entry

void setup() {
  Serial.begin(9600); // Initialize serial for debugging
  lcd.init(); // Initializing LCD
  lcd.backlight();
  
  lcd.setCursor(0, 0);
  lcd.print("Welcome");
  lcd.setCursor(0, 1);
  lcd.print("System Load");
  
  // Initialize relay pins
  pinMode(relayPin, OUTPUT);
  pinMode(relayPin2, OUTPUT);
  pinMode(buzzerPin, OUTPUT);
  pinMode(limitSwitchPin, INPUT_PULLUP); // Set the limit switch pin as input with internal pull-up resistor
  setLocked(true); // Ensure system starts locked

  // Initializing fingerprint sensor
  mySerial.begin(57600);
  finger.begin(57600);
  delay(1000); // Wait for sensor to initialize
  
  Serial.println("Initializing fingerprint sensor...");
  lcd.setCursor(0, 1);
  if (finger.verifyPassword()) {
    lcd.print("System Ok   ");
    Serial.println("Fingerprint sensor initialized successfully.");
  } else {
    lcd.print("Finger Err");
    Serial.println("Error: Unable to initialize fingerprint sensor.");
    while (1) {
      delay(1);
    }
  }
  delay(2000);
  lcd.clear();
  setupSystem();
}

void loop() {
  char key = keypad.getKey();
  if (key) {
    lcd.clear();
    Serial.print("Key pressed: ");
    Serial.println(key);
    if (enteringEmergencyPassword) {
      handleEmergencyUnlock(key);
    } else if (waitingForFingerprintDeletion) {
      // Ignore keypad input while waiting for fingerprint deletion
    } else {
      switch (key) {
        case 'A': // Use 'A' to enroll a fingerprint
          lcd.setCursor(0, 0);
          lcd.print("Place Finger    ");
          lcd.setCursor(0, 1);
          lcd.print("                ");
          enrollFingerprint();
          break;
        case 'B': // Use 'B' for emergency unlock protocol
          enteringEmergencyPassword = true;
          lcd.setCursor(0, 0);
          lcd.print("Emergency       ");
          lcd.setCursor(0, 1);
          lcd.print("Press 5         ");
          break;
        case 'C': // Use 'C' to lock the door
          if (digitalRead(limitSwitchPin) == HIGH) { // If the limit switch is not triggered (door open)
            digitalWrite(buzzerPin, HIGH); // Turn on the buzzer
            lcd.setCursor(0, 0);
            lcd.print("Not Closed      ");
            delay(1000); // Wait for 1 second
            digitalWrite(buzzerPin, LOW); // Turn off the buzzer
            Serial.println("Door not locked, buzzer ON");
          } else {
            // If the limit switch is triggered (door closed)
            lockSystem(); // Deactivate the relay
            Serial.println("Door locked, deactivating relay");
            lcd.setCursor(0, 0);
            lcd.print("D to delete     ");
            lcd.setCursor(0, 1);
            lcd.print("Locked          ");
          }
          break;
        case 'D': // Use 'D' to delete the fingerprint
          lcd.setCursor(0, 0);
          lcd.print("Place finger    ");
          lcd.setCursor(0, 1);
          lcd.print("to delete       ");
          waitingForFingerprintDeletion = true;
          break;
        case '#': // Use '#' to reset the system
          resetSystem();
          break;
        default:
          lcd.setCursor(0, 0);
          lcd.print("Invalid Option  ");
          delay(2000);
          lcd.clear();
          setupSystem(); // Ensure the system goes back to the initial state
          break;
      }
    }
  }

  // Check the limit switch state
  if (digitalRead(limitSwitchPin) == LOW) {
    // Limit switch is pressed (door closed)
    digitalWrite(relayPin2, LOW); // Deactivate the relay
    Serial.println("Limit switch activated, relay OFF");
  } else {
    // Limit switch is not pressed (door open)
    digitalWrite(relayPin2, HIGH); // Activate the relay
    Serial.println("Limit switch not activated, relay ON");
  }

  if (waitingForFingerprintDeletion) {
    handleFingerprintDeletion();
  } else if (isFingerprintMatched() && isLocked) {
    unlockSystem();
  }
}

void setupSystem() {
  lcd.setCursor(0, 0);
  lcd.print("A to use finger ");
  lcd.setCursor(0, 1);
  lcd.print("                ");
}

void enrollFingerprint() {
  int id = 1; // Fixed ID for simplicity
  lcd.setCursor(0, 1);
  lcd.print("                ");

  while (finger.getImage() != FINGERPRINT_OK);
  if (finger.image2Tz(1) != FINGERPRINT_OK) {
    lcd.setCursor(0, 1);
    lcd.print("Error           ");
    delay(2000);
    lcd.clear();
    setupSystem();
    return;
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Remove Finger   ");
  delay(2000);

  while (finger.getImage() != FINGERPRINT_NOFINGER);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Place Again     ");

  while (finger.getImage() != FINGERPRINT_OK);
  if (finger.image2Tz(2) != FINGERPRINT_OK) {
    lcd.setCursor(0, 1);
    lcd.print("Error           ");
    delay(2000);
    lcd.clear();
    setupSystem();
    return;
  }

  if (finger.createModel() != FINGERPRINT_OK) {
    lcd.setCursor(0, 1);
    lcd.print("Error           ");
    delay(2000);
    lcd.clear();
    setupSystem();
    return;
  }

  if (finger.storeModel(id) != FINGERPRINT_OK) {
    lcd.setCursor(0, 1);
    lcd.print("Error           ");
    delay(2000);
    lcd.clear();
    setupSystem();
    return;
  }

  lcd.setCursor(0, 0);
  lcd.print("Locked          ");
  lcd.setCursor(0, 1);
  lcd.print("                ");
  lockSystem();
  delay(2000);
  lcd.clear();
}

void deleteFingerprint() {
  int id = 1; // Fixed ID for simplicity
  lcd.setCursor(0, 1);
  if (finger.deleteModel(id) == FINGERPRINT_OK) {
    lcd.print("Deleted         ");
  } else {
    lcd.print("Error           ");
  }
  delay(2000);
  lcd.clear();
}

bool isFingerprintMatched() {
  int id = 1; // Fixed ID for simplicity
  if (finger.getImage() != FINGERPRINT_OK) return false;
  if (finger.image2Tz() != FINGERPRINT_OK) return false;
  if (finger.fingerFastSearch() != FINGERPRINT_OK) return false;
  if (finger.fingerID == id) return true;
  return false;
}

void setLocked(bool locked) {
  isLocked = locked;
  digitalWrite(relayPin, locked ? HIGH : LOW); // Activate relay (lock) if locked, deactivate if unlocked
  lcd.setCursor(0, 0);
  if (locked) {
    lcd.print("Locked          ");
    lcd.setCursor(0, 1);
    lcd.print("                ");
  } else {
        lcd.print("Unlocked        ");
    lcd.setCursor(0, 1);
    lcd.print("C to lock       ");
    delay(1000);
    lcd.clear();
  }
}

void lockSystem() {
  setLocked(true);
  digitalWrite(relayPin2, LOW); // Deactivate the second relay
  delay(2000);
  lcd.clear();
}

void unlockSystem() {
  setLocked(false);
  lcd.setCursor(0, 0);
  lcd.print("Unlocked        ");
  lcd.setCursor(0, 1);
  lcd.print("C to lock       ");
}

void resetSystem() {
  Serial.println("System reset");
  lcd.clear();
  setup();
}

void handleEmergencyUnlock(char key) {
  if (key == '5') { // If the key pressed is '5'
    unlockSystem();
    enteringEmergencyPassword = false;
  } else { // Wrong key
    enteringEmergencyPassword = false;
    lcd.setCursor(0, 0);
    lcd.print("Wrong Password  ");
    delay(2000);
    lcd.clear();
    setupSystem();
  }
}

void handleFingerprintDeletion() {
  lcd.setCursor(0, 0);
  lcd.print("Place finger    ");
  lcd.setCursor(0, 1);
  lcd.print("to delete       ");
  
  while (finger.getImage() != FINGERPRINT_OK);
  if (finger.image2Tz() != FINGERPRINT_OK) {
    lcd.setCursor(0, 1);
    lcd.print("Error           ");
    delay(2000);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Press D to      ");
    lcd.setCursor(0, 1);
    lcd.print("delete fingerprint");
    waitingForFingerprintDeletion = false;
    return;
  }

  if (finger.fingerFastSearch() != FINGERPRINT_OK) {
    lcd.setCursor(0, 1);
    lcd.print("Not Found       ");
    delay(2000);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Press D to      ");
    lcd.setCursor(0, 1);
    lcd.print("delete fingerprint");
    waitingForFingerprintDeletion = false;
    return;
  }

  if (finger.fingerID == 1) { // Assuming we are deleting the fingerprint with ID 1
    deleteFingerprint();
    unlockSystem(); // Unlock the system after deleting fingerprint data
    setupSystem(); // Ensure the system goes back to the initial state after deleting
    waitingForFingerprintDeletion = false;
  } else {
    lcd.setCursor(0, 1);
    lcd.print("Wrong Finger    ");
    delay(2000);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Press D to      ");
    lcd.setCursor(0, 1);
    lcd.print("delete fingerprint");
    waitingForFingerprintDeletion = false;
  }
}