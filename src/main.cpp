/**
 * NodeMCU Client.
 * 
 * The client main loop will listen to server commands such as enrolling
 * a fingerprint and will also send scanned fingerprint id to be 
 * processed by the server.
 * 
 * Wiring for Fingerprint Scanner: ----------------------------------------- *
 *  VCC to NodeMCU Vin
 *  GND to NodeMCU GND
 *  TX  to NodeMCU d5 (GPIO14, FINGER_RX)
 *  RX  to NodeMCU d6 (GPIO12, FINGER_TX)
 * 
 * Wiring for Liquid Crystal Display: -------------------------------------- *
 * VCC to NodeMCU Vin
 * GND to NodeMCU GND
 * SDA to NodeMCU d2 (SDA)
 * SCL to NodeMCU d1 (SCL)
*/

#include "Arduino.h"
#include "Adafruit_Fingerprint.h"
#include "ESP8266Wifi.h"
#include "secrets.h"
#include "SoftwareSerial.h"
#include "WiFiClient.h"
#include "LiquidCrystal_I2C.h"
#include "string.h"

#define DISCON    0x0D // d7
#define FINGER_RX 0x0E // d5
#define FINGER_TX 0x0C // d6

WiFiClient client;
SoftwareSerial s_serial(FINGER_RX, FINGER_TX);
Adafruit_Fingerprint finger_scanner = Adafruit_Fingerprint(&s_serial);
LiquidCrystal_I2C lcd = LiquidCrystal_I2C(0x27, 0x10, 0x02);
String message;
int discon_btn_state = 0;
int discon_btn_old_state = 0;
bool is_connected = false;
byte sprites_pos[4] = { 0x03, 0x02, 0x01, 0x00 };
byte scan_mode = 0x00;

byte head_sprite[8] = {
  0b00000,
  0b00000,
  0b01110,
  0b11111,
  0b11111,
  0b11111,
  0b01110,
  0b00000
};

byte tail_sprite[8] = {
  0b00000,
  0b00000,
  0b00000,
  0b00100,
  0b01110,
  0b00100,
  0b00000,
  0b00000
};


/**
 * Initialize The Fingerprint Scanner.
 * 
 * The function will check if fingerprint scanner is 
 * responding. Without the scanner then the following
 * functions of the code will be pointless therefore,
 * it will keep looping until a fingerprint scanner
 * is found.
*/
void initFingerprintScanner() {
    finger_scanner.begin(57600);
    Serial.print("\n[i] Starting Fingerprint Scanner.");

    while (true) {
        if (finger_scanner.verifyPassword()) {
            Serial.print("\n[i] Scanner Found !");
            finger_scanner.emptyDatabase(); // TODO: Remove this line after prototyping phase.
            break;
        }
        else {
            Serial.print("\n[i] Scanner not Found. Retrying...");
        }
        delay(50);
    }
}


/**
 * Display the text to the Liquid Crystal Display.
 * @param text the text to be displayed. 
*/
void displayText(String firstLine, String secondLine) {
  lcd.setCursor(0, 0);
  lcd.print(firstLine);
  lcd.setCursor(0, 1);
  lcd.print(secondLine);
  delay(2);
}


/**
 * Initialize the Liquid Crystal Display
*/
void initLCD() {
    Serial.print("\n[i] Starting LCD.");
    lcd.init();
    lcd.backlight();

    lcd.createChar(0, head_sprite);
    lcd.createChar(1, tail_sprite);

    lcd.setCursor(0, 0);
    displayText("  Client Start  ", "");
}


/**
 * This function updates the animation when waiting for a fingerprint scan.
*/
void scanAnimation() {
  switch (scan_mode) {
    case 0x00:
      displayText("Scan Your Finger", "                ");
      break;
    case 0x01:
      displayText(" Enroll  Finger ", "                ");
      break;
  }
  
  for (int i = 0; i < sizeof(sprites_pos) / sizeof(byte); i++) {
    if (sprites_pos[i] > 0x0F) {
      sprites_pos[i] = 0x00;
    }

    lcd.setCursor(sprites_pos[i], 1);
    if (i == 0) {
      lcd.write(0);
    }
    else {
      lcd.write(1);
    }
      sprites_pos[i]++;
  }
  delay(2);
}


/**
 * Connect the board to a Wi-Fi network.
 * 
 * The board is fixed to a specific SSID and PASS.
 * The scanning process will not stop until it 
 * successfully connects to this specific network.
*/
void connectToWiFi() {
    Serial.print("\n[i] Connecting to Wi-Fi");
    displayText("  Client Start  ", "   conn WiFi   ");
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.print(".");
    }

    Serial.print("\n[i] Connected to ");
    Serial.print(WiFi.localIP());
    displayText("  Client Start  ", "   conn WiFi.   ");
}


/**
 * Connect the board to a server as a client.
 * 
 * The client object is a socket that will try to 
 * find a server program on a specific address.
*/
void connectToServer() {
    Serial.print("\n[i] Connecting to Server");
    displayText("  Client Start  ", "  conn Server   ");

    while (!client.connect(HOST, PORT)) {
        delay(1000);
        Serial.print(".");
    }

    Serial.print("\n[i] Connected !");
    is_connected = true;
    client.print("Client connected successfully. // Hello Server // \n");
    displayText("  Client Start  ", "  conn Server.   ");
}


/**
 * Close the socket connection.
 * @note To reconnect from a server, just restart the client.
*/
void disconnectFromServer() {
    if (is_connected) {
        client.print("disconnect\n");
        client.flush();
        Serial.print("\n[i] Disconnecting...");
        client.stop();
        is_connected = false;
        Serial.print("\n[i] Disconnected from server !");
        displayText("  Disconnected  ", "  please reset  ");
    }
}


/**
 * Enroll a fingerprint
*/
bool getFingerprintEnroll(uint8_t id) {
  Serial.print("Waiting for valid finger to enroll as #");
  Serial.println(id);

  int p = -1;
  while (p != FINGERPRINT_OK) {
    p = finger_scanner.getImage();
    scanAnimation();
    delay(2000);
    switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image taken");
      displayText("  Image  Taken  ", " please wait... ");
      break;
    case FINGERPRINT_NOFINGER:
      Serial.println(".");
      break;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      break;
    case FINGERPRINT_IMAGEFAIL:
      Serial.println("Imaging error");
      break;
    default:
      Serial.println("Unknown error");
      break;
    }
  }

  // OK success!
  displayText("   Processing   ", "    Image...    ");
  p = finger_scanner.image2Tz(1);
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image converted");
      delay(100);
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");
      return 0;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return 0;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      return 0;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");
      return 0;
    default:
      Serial.println("Unknown error");
      return 0;
  }


  Serial.println("Remove finger");
  displayText("      ----      ", " Remove Finger  ");
  
  delay(2000);
  p = 0;
  while (p != FINGERPRINT_NOFINGER) {
    p = finger_scanner.getImage();
  }
  Serial.print("ID "); Serial.println(id);
  p = -1;
  Serial.println("Place same finger again");
  displayText("   Place Same   ", "  Finger again  ");
  while (p != FINGERPRINT_OK) {
    p = finger_scanner.getImage();
    delay(2000);
    switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image taken");
      displayText("  Image  Taken  ", " please wait... ");
      break;
    case FINGERPRINT_NOFINGER:
      Serial.print(".");
      break;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      break;
    case FINGERPRINT_IMAGEFAIL:
      Serial.println("Imaging error");
      break;
    default:
      Serial.println("Unknown error");
      break;
    }
  }


  // OK success!
  displayText("   Processing   ", "    Image...    ");
  p = finger_scanner.image2Tz(2);
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image converted");
      delay(100);
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");
      return 0;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return 0;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      return 0;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");
      return 0;
    default:
      Serial.println("Unknown error");
      return 0;
  }

  // OK converted!
  Serial.print("Creating model for #");  
  Serial.println(id);

  p = finger_scanner.createModel();
  if (p == FINGERPRINT_OK) {
    Serial.println("Prints matched!");
    displayText("  Fingerprints  ", "    Matched     ");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    displayText(" Communication  ", "     Error      ");
    return 0;
  } else if (p == FINGERPRINT_ENROLLMISMATCH) {
    Serial.println("Fingerprints did not match");
    displayText("  Fingerprints  ", " Did Not Match  ");
    return 0;
  } else {
    Serial.println("Unknown error");
    displayText("    Unknown     ", "     Error      ");
    return 0;
  }


  Serial.print("ID "); 
  Serial.println(id);
  p = finger_scanner.storeModel(id);
  if (p == FINGERPRINT_OK) {
    Serial.println("Stored to internal database");
    displayText("  Sending Data  ", "  to Database   ");
    delay(1000);
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    return 0;
  } else if (p == FINGERPRINT_BADLOCATION) {
    Serial.println("Could not store in that location");
    return 0;
  } else if (p == FINGERPRINT_FLASHERR) {
    Serial.println("Error writing to flash");
    return 0;
  } else {
    Serial.println("Unknown error");
    return 0;
  }

  return 1;
}


/**
 * Send the fingerprint id to the server and wait for
 * the server's response.
 * 
 * @note this function blocks the main thread.
*/
void sendFinger() {

}


/**
 * Scan a fingerprint and match it on the database.
*/
int getFingerprintID() {
  uint8_t p = finger_scanner.getImage();
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image taken");
      displayText("  Image  Taken  ", " please wait... ");
      break;
    case FINGERPRINT_NOFINGER:
      Serial.println("No Finger detected");
      return -1;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return -1;
    case FINGERPRINT_IMAGEFAIL:
      Serial.println("Imaging error");
      return -1;
    default:
      Serial.println("Unknown error");
      return -1;
  }


  // OK success!
  p = finger_scanner.image2Tz();
  displayText("   Processing   ", "    Image...    ");
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image converted");
      delay(100);
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");
      return -1;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return -1;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      return -1;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");
      return -1;
    default:
      Serial.println("Unknown error");
      return -1;
  }


  // OK converted!
  p = finger_scanner.fingerSearch();
  if (p == FINGERPRINT_OK) {
    Serial.println("Found a print match!");
    displayText("  Fingerprint   ", "    is found    ");
    delay(1000);
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    return -1;
  } else if (p == FINGERPRINT_NOTFOUND) {
    Serial.println("Did not find a match");
    displayText("  Did not Find  ", "     Match      ");
    delay(1000);
    return -1;
  } else {
    Serial.println("Unknown error");
    return -1;
  }

  // found a match!
  Serial.print("Found ID #"); Serial.print(finger_scanner.fingerID);
  Serial.print(" with confidence of "); Serial.println(finger_scanner.confidence);

  return finger_scanner.fingerID;
}


 
void enrollFinger() {
    scan_mode = 0x01;
    Serial.print("\n[i] Ready to enroll a fingerprint.");
    displayText("   Enrollment   ", "      Mode      ");
    String finger_id_unparsed = client.readStringUntil('\n');
    String first_name = client.readStringUntil('\n');
    String middle_name = client.readStringUntil('\n');
    String last_name = client.readStringUntil('\n');
    String age = client.readStringUntil('\n');
    String gender = client.readStringUntil('\n');
    String phone_number = client.readStringUntil('\n');
    String address = client.readStringUntil('\n');

    client.println("enrollFinger");
    uint8_t id = finger_id_unparsed.toInt();
    while (!getFingerprintEnroll(id));

    client.println(first_name);
    delay(30);
    client.println(middle_name);
    delay(30);
    client.println(last_name);
    delay(30);
    client.println(age);
    delay(30);
    client.println(gender);
    delay(30);
    client.println(phone_number);
    delay(30);
    client.println(address);
    delay(30);
    client.println(id);

    displayText(" Waiting for  ", "  Feedback...   ");
    String feedback = client.readStringUntil('\n');
    if (feedback == "OK") {
      displayText("   Enrollment   ", "    Success!    ");
    }
    else {
      displayText("  Enroll Fail!  ", "   Try  Again   ");
    }
    delay(2000);
}


void scanFinger() {
  scan_mode = 0x00;
  int fingerprint_id = getFingerprintID();
  if (fingerprint_id != -1) {
    client.println("scanFinger");
    client.println(fingerprint_id);
    displayText("    Logging     ", "   Attendance   ");
    // TODO: to be logged into database, get feedback.
    String feedback = client.readStringUntil('\n');
    if (feedback == "OK") {
      displayText("  Successfully  ", "  Logged to DB  ");
      String attendee_first_name = client.readStringUntil('\n');
      delay(2000);
      displayText("                ", "                ");
      displayText("Welcome:        ", attendee_first_name);
    }
    else {
      displayText(" Failed logging ", "   Attendance   ");
    }
    delay(3000);
  }
  delay(50);
}


/**
 * Initialize all connections.
*/
void setup() {
    Serial.begin(115200);
    Serial.print("\n[i] Starting Client...");

    delay(50);
    initFingerprintScanner();
    delay(50);
    initLCD();
    delay(50);
    connectToWiFi();
    delay(50);
    connectToServer();
    delay(50);
    pinMode(DISCON, INPUT);

    lcd.setCursor(0, 0);
    lcd.print("  Scan  Finger  ");
    delay(2);
}


/**
 * The Main event loop. will listen for events and execute 
 * functions related to events. 
*/
void loop() {
    discon_btn_state = digitalRead(DISCON);
    
    // disconnect the client from the server when the disconnect button is clicked.
    if (discon_btn_state != discon_btn_old_state) {
        if (discon_btn_state == 1) {
            disconnectFromServer();
        }
    }

    // check if there is available data to be read.
    if (client.available()) {
        message = client.readStringUntil('\n');

        if (message == "disconnect") {
            disconnectFromServer();
        }

        else if (message == "reboot") {
            disconnectFromServer();
            WiFi.disconnect();
            delay(50);
            ESP.restart();
        }

        else if (message == "enroll") {
            enrollFinger();
        }
    }
    
    // check if the client is still connected to a server before scanning finger.
    if (is_connected) {
      scanFinger();
      scanAnimation();
    }
    discon_btn_old_state = discon_btn_state;
}