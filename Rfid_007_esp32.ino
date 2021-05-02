#include <Adafruit_NeoPixel.h>

#include "FS.h"
#include "SD.h"
//#include "SDAccess.h"


#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <MFRC522.h>

const String VERSION = "1.08";
const int BRIGHTNESS = 10;

/*
 * Anzeige nach Moduswechsel wieder recreaten
 * Leere Zeilen in lock.csv aufiltern
 * Versionsanzeige
 * Was ist mit der LED?
 * 
 */

String sTokenName = "";


struct rfid{
   String Name;
   String ID;
};

struct color{
  int Red;
  int Green;
  int Blue;
};

color rgbCol;


//typedef struct rfid Record;
#define ARRAY_SIZE 23
rfid arrRFID[ARRAY_SIZE];

#define DEBUGLOGGING 

#define OP_MODE_NORMAL 0x00
#define OP_MODE_LISTTOKENS 0x01
#define OP_MODE_STORETOKENS 0x02
#define OP_MODE_TEST 0x03
#define OP_MODE_COUNT 4 //Wieviele Modus gibt es?
int iOP_Mode = OP_MODE_NORMAL;

const char FILENAME[] = "/rfid.csv";
const char LOGFILE[] = "/log.csv";
#define LED_PIN 2
#define LED_COUNT 1

Adafruit_NeoPixel strip = Adafruit_NeoPixel(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
#define BUTTON 15 // +GND
bool bButtonPress = false;

#define DOORPIN 5

// Init array that will store new NUID
byte nuidPICC[4];

bool bAusgeliehen = false;
byte byLeihID[4];
byte byCardID[4];

bool bNewButton = false;
bool bPrintNewState = false;

//----Display
//----SCL - 22
//----SDA - 21
#define OLED_RESET -1 // not used / nicht genutzt bei diesem Display
Adafruit_SSD1306 display(128, 64, &Wire, OLED_RESET);

//----RFID
//---- SCK - 18
//---- MISO/CLK - 19
//---- MOSI - 23
//---- RST - 17
//---- SS/ SDA - 16
#define RFID_SS_PIN 16
#define RST_PIN 17
MFRC522 rfid(RFID_SS_PIN, RST_PIN); // Instance of the class
MFRC522::MIFARE_Key key;
int iCounter = 0;

//----SD Card via SPI
//---- CS - 4
//---- Mosi - 23
//---- SCK - 18
//---- Miso - 19
//using namespace sdfat;
//#define SD_SS_PIN D0
//#define SD_SS_PIN D0
#define SD_CS_PIN 4
bool bCardOkay = false;


void setup() {
  rgbCol.Red = 0;
  rgbCol.Green = 0;
  rgbCol.Blue = 0;


  pinMode(BUTTON, INPUT_PULLUP);
  pinMode(DOORPIN, OUTPUT);
  digitalWrite(DOORPIN, LOW);
  
  Serial.begin(9600);

  // initialize with the I2C addr 0x3C
  display.begin(SSD1306_SWITCHCAPVCC, 0x3c);
  displayShowInfo();

  //----RFID
  SPI.begin();   // Init SPI bus
  rfid.PCD_Init(); // Init MFRC522
  delay(55);
  rfid.PCD_DumpVersionToSerial();  // Show details of PCD - MFRC522 Card Reader details

  delay(55);
  
  //----SD Card
  bCardOkay = initSD();
  if( bCardOkay ){
    readFileToRam( SD, FILENAME );
  }
  
  #ifdef DEBUGLOGGING
    Serial.println(F("Scan PICC to see UID, SAK, type, and data blocks..."));
  #endif

  delay(100);
  #ifdef DEBUGLOGGING
    Serial.println("geht los");
  #endif
  
  //setColor_blue();

  //getLastLineFromFile(SD, LOGFILE);
  recallState();


  strip.begin();
  strip.setBrightness(BRIGHTNESS);
  strip.setPixelColor(0, strip.Color(rgbCol.Red, rgbCol.Green, rgbCol.Blue));
  strip.show();
}


void loop() {
  strip.setPixelColor(0, strip.Color(rgbCol.Red, rgbCol.Green, rgbCol.Blue));
  strip.show();
  queryButton();
  if(iOP_Mode == OP_MODE_NORMAL){
    String tmp;
    if(bAusgeliehen){
      tmp = "Ausgeliehen";
      rgbCol.Red = 255; rgbCol.Green = 0; rgbCol.Blue = 0;
    }else{
      tmp = "Zurueck";
      rgbCol.Red = 0; rgbCol.Green = 255; rgbCol.Blue = 0;
    }
    if(bPrintNewState){
      if( (byLeihID[0]==0) && (byLeihID[1]==0) && (byLeihID[2]==0) && (byLeihID[3]==0)){
        //Nach Start, nichts ausgeliehen und zurueckgegeben ,einmal durch die Modus gecyclet.
        displayShowInfo();
      }else{
        displayShowText(tmp, String(byLeihID[0]) + " " + String(byLeihID[1]) + " " + String(byLeihID[2]) + " " + String(byLeihID[3]), sTokenName);
      }
      bPrintNewState = false;
    }
    operationNormal();
  }else if(iOP_Mode == OP_MODE_LISTTOKENS){
    rgbCol.Red = 0; rgbCol.Green = 0; rgbCol.Blue = 255;
    operationListTokens();
  }else if(iOP_Mode == OP_MODE_STORETOKENS){
    rgbCol.Red = 255; rgbCol.Green = 0; rgbCol.Blue = 255;
    operationStoreTokens();
  }else if(iOP_Mode == OP_MODE_TEST){
    rgbCol.Red = 255; rgbCol.Green = 255; rgbCol.Blue = 0;
    displayShowText("Version:", VERSION, "");
    bPrintNewState = true;
  }
  
/*
  if(iOP_Mode == OP_MODE_NORMAL){
    operationNormal();
  }else if(iOP_Mode == OP_MODE_LISTTOKENS){
    operationListTokens();
  }else if(iOP_Mode == OP_MODE_STORETOKENS){
    operationStoreTokens();
  }else if(iOP_Mode == OP_MODE_TEST){
    //displayShowText("Memory:", String( availableMemory() ) + "xxx", "");
    displayShowText("Version:", VERSION, "");
    //iOP_Mode = OP_MODE_NORMAL;
    
    //delay( 1000 );
    //displayShowModus(iOP_Mode);
    //recallState();
    //bNewMode = true;
  }else{}

  if(bNewMode==true){
    setColor(rgbCol.Red, rgbCol.Green, rgbCol.Blue);
    bNewMode = false;
  }
*/  
}

bool initSD(){
  bool bReturn = false;
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("Card Mount Failed");
    displayInfo("SD Card", "Mount failed");
    delay(1000);
    return bReturn;
  }
  uint8_t cardType = SD.cardType();

  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    displayInfo("SD Card", "No Card");
    delay(1000);
    return bReturn;
  }

  
  #ifdef DEBUGLOGGING
    Serial.print("SD Card Type: ");
    if (cardType == CARD_MMC) {
      Serial.println("MMC");
    } else if (cardType == CARD_SD) {
      Serial.println("SDSC");
    } else if (cardType == CARD_SDHC) {
      Serial.println("SDHC");
    } else {
      Serial.println("UNKNOWN");
    }
    displayInfo("SD Card", "Okay");
    delay(1000);
    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    Serial.printf("SD Card Size: %lluMB\n", cardSize);
    
  #endif

  File file = SD.open(FILENAME);
  if(!file) {
    #ifdef DEBUGLOGGING
      Serial.println("File doesn't exist");
      Serial.println("Creating file...");
    #endif
    writeFile(SD, FILENAME, "### Andyland.info RFID Lock ###\r\n");

    File file = SD.open(FILENAME);
    if(file){
      file.close();
      bReturn = true;
    }else{
      bReturn = false;
    }
    
  }else {
    #ifdef DEBUGLOGGING
      Serial.println("File already exists"); 
    #endif
    bReturn = true; 
  }
  file.close();
  return bReturn;
}



// Write the  line onto the SD card
// That's for RFID Recording Mode 
void recordToSDCard(String pLine) {
  #ifdef DEBUGLOGGING
    Serial.print("recordToSDCard(): ");
    Serial.println(pLine);
  #endif
  appendFile(SD, FILENAME, pLine.c_str());
}

// Write to the SD card (DON'T MODIFY THIS FUNCTION)
void writeFile(fs::FS &fs, const char * path, const char * message) {
  #ifdef DEBUGLOGGING
    Serial.printf("Writing file: %s\n", path);
  #endif
  File file = fs.open(path, FILE_WRITE);
  if(!file) {
    #ifdef DEBUGLOGGING
      Serial.println("Failed to open file for writing");
    #endif
    return;
  }
  if(file.print(message)) {
    #ifdef DEBUGLOGGING
      Serial.println("File written");
    #endif
  } else {
    #ifdef DEBUGLOGGING
      Serial.println("Write failed");
    #endif
  }
  file.close();
}

// Append data to the SD card (DON'T MODIFY THIS FUNCTION)
void appendFile(fs::FS &fs, const char * path, const char * message) {
  Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if(!file) {
    #ifdef DEBUGLOGGING
      Serial.println("Failed to open file for appending");
    #endif
    return;
  }
  if(file.print(message)) {
    #ifdef DEBUGLOGGING
      Serial.println("Message appended");
    #endif
  } else {
    #ifdef DEBUGLOGGING
      Serial.println("Append failed");
    #endif
  }
  file.close();
}

String getLastLineFromFile(fs::FS &fs, const char * path){
  String sReturn = "";
  String Buffer_Read_Line = "";
  String Sub_String_A = "";
  String Sub_String_B = ""; 
  int LastPosition=0;
  
  //Serial.printf("Reading from file: %s\n", path);

  File file = fs.open(path, FILE_READ);
  if(!file) {
    #ifdef DEBUGLOGGING
      Serial.println("Failed to open file for reading");
    #endif
    return sReturn;
  }

  int totalBytes = file.size();
  int tmpCounter = 0;
  while (file.available()) {
    for(LastPosition=0; LastPosition<= totalBytes; LastPosition++){
      char caracter=file.read();
      if(caracter!=10){   //ASCII new line
        Buffer_Read_Line=Buffer_Read_Line+caracter;
        sReturn = Buffer_Read_Line;
      }else{           
        //Serial.println("readFile():" + Buffer_Read_Line);
        if(!Buffer_Read_Line.startsWith("#")){
          if( Buffer_Read_Line[Buffer_Read_Line.length() - 1] == 13 ){ //Linebreak?
            if(Buffer_Read_Line.length() > 10){  // Leere Zeilen herausfiltern
              Buffer_Read_Line = Buffer_Read_Line.substring(0, Buffer_Read_Line.length() - 1);
              sReturn = Buffer_Read_Line;
            }
          }
          tmpCounter++;
        }
        Buffer_Read_Line="";      //string to 0
      }
    }
  }

  //Serial.print("letzte Zeile:");Serial.println(sReturn);
  
  file.close();
  return sReturn;
}



//----Daten aus der Datei in den Speicher einlesen
bool readFileToRam(fs::FS &fs, const char * path){
  boolean bReturn = false;
  String Buffer_Read_Line = "";
  String Sub_String_A = "";
  String Sub_String_B = ""; 
  int LastPosition=0;
  
  //Serial.printf("Reading from file: %s\n", path);

  File file = fs.open(path, FILE_READ);
  if(!file) {
    #ifdef DEBUGLOGGING
      Serial.println("Failed to open file for reading");
    #endif
    return bReturn;
  }

  int totalBytes = file.size();
  int tmpCounter = 0;
  while (file.available()) {
    for(LastPosition=0; LastPosition<= totalBytes; LastPosition++){
      char caracter=file.read();
      if(caracter!=10){   //ASCII new line
        Buffer_Read_Line=Buffer_Read_Line+caracter;
      }else{           
        if(tmpCounter<ARRAY_SIZE){
          //Serial.println("readFile():" + Buffer_Read_Line);
          if(!Buffer_Read_Line.startsWith("#")){
            if( Buffer_Read_Line[Buffer_Read_Line.length() - 1] == 13 ){
              Buffer_Read_Line = Buffer_Read_Line.substring(0, Buffer_Read_Line.length() - 1);
            }
            processLineFromSD( Buffer_Read_Line, tmpCounter );
            tmpCounter++;
          }
          Buffer_Read_Line="";      //string to 0
        }
      }
    }
  }
  file.close();
  bReturn = true;
  Serial.println("Ende readFile");
  return bReturn;
}

//----Den Array mit Daten von der SD Karte fuellen
void processLineFromSD(String pLine, int pIndex){
  arrRFID[pIndex].Name=split(pLine, ':', 0);
  arrRFID[pIndex].ID=split(pLine, ':', 1); 
  #ifdef DEBUGLOGGING
    Serial.println("processLineFromSD("+ String(pIndex) + "):" + arrRFID[pIndex].Name + ":" + arrRFID[pIndex].ID +";");
  #endif
}

String split(String data, char separator, int index){
    int found = 0;
    int strIndex[] = { 0, -1 };
    int maxIndex = data.length() - 1;

    for (int i = 0; i <= maxIndex && found <= index; i++) {
        if (data.charAt(i) == separator || i == maxIndex) {
            found++;
            strIndex[0] = strIndex[1] + 1;
            strIndex[1] = (i == maxIndex) ? i+1 : i;
        }
    }
    return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}




void operationListTokens(){
  // Reset the loop if no new card present on the sensor/reader. This saves the entire process when idle.
  if (!rfid.PICC_IsNewCardPresent())
    return;

  // Verify if the NUID has been readed
  if (!rfid.PICC_ReadCardSerial())
    return;

  Serial.print(F("PICC type: "));
  MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);
  #ifdef DEBUGLOGGING
    Serial.println(rfid.PICC_GetTypeName(piccType));
  #endif
  // Check is the PICC of Classic MIFARE type
  if (piccType != MFRC522::PICC_TYPE_MIFARE_MINI &&
    piccType != MFRC522::PICC_TYPE_MIFARE_1K &&
    piccType != MFRC522::PICC_TYPE_MIFARE_4K)
  {
    Serial.println(F("Your tag is not of type MIFARE Classic."));
    displayShowText("Karte unbekannt", "***", "");
    return;
  }
  printHex(rfid.uid.uidByte, rfid.uid.size);
  String sID = String(rfid.uid.uidByte[0]) + " " + String(rfid.uid.uidByte[1]) + " " + String(rfid.uid.uidByte[2]) + " " + String(rfid.uid.uidByte[3]);
  displayShowText("Karte gelesen:", sID, getTokenName(sID) );

  //Serial.println();
  //Serial.print("TokenName:");
  //Serial.println( getTokenName(sID) );
    
  // Halt PICC
  rfid.PICC_HaltA();

  // Stop encryption on PCD
  rfid.PCD_StopCrypto1();
}

String getTokenName(String pID){
  //----Ist die Karte ggf mit einem Namen im Speicher hinterlegt?
  String sRet = "";
  
  for(int i=0; i<ARRAY_SIZE; i++){
    if(arrRFID[i].ID == pID){
      //Serial.println("Treffer, Karte bekannt:" + arrRFID[i].Name);
      sRet = arrRFID[i].Name;
      break;
    }
  }
  return sRet;
}

void operationStoreTokens(){
  
  // Reset the loop if no new card present on the sensor/reader. This saves the entire process when idle.
  if (!rfid.PICC_IsNewCardPresent())
    return;

  // Verify if the NUID has been readed
  if (!rfid.PICC_ReadCardSerial())
    return;

  Serial.print(F("PICC type: "));
  MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);
  #ifdef DEBUGLOGGING
    Serial.println(rfid.PICC_GetTypeName(piccType));
  #endif
  
  // Check is the PICC of Classic MIFARE type
  if (piccType != MFRC522::PICC_TYPE_MIFARE_MINI &&
    piccType != MFRC522::PICC_TYPE_MIFARE_1K &&
    piccType != MFRC522::PICC_TYPE_MIFARE_4K)
  {
    Serial.println(F("Your tag is not of type MIFARE Classic."));
    displayShowText("Karte unbekannt", "***", "");
    return;
  }
  printHex(rfid.uid.uidByte, rfid.uid.size);
  displayShowText("Token_" + String(iCounter), String(rfid.uid.uidByte[0]) + " " + String(rfid.uid.uidByte[1]) + " " + String(rfid.uid.uidByte[2]) + " " + String(rfid.uid.uidByte[3]), "" );
  String sLine = "Token_" + String(iCounter) + ":" + String(rfid.uid.uidByte[0]) + " " + String(rfid.uid.uidByte[1]) + " " + String(rfid.uid.uidByte[2]) + " " + String(rfid.uid.uidByte[3]);
  recordToSDCard(sLine + "\r\n");
  iCounter++;
  // Halt PICC
  rfid.PICC_HaltA();

  // Stop encryption on PCD
  rfid.PCD_StopCrypto1();
}


bool createLogFile( fs::FS &fs, const char * path, const char * message ){
  bool bFileExists = true;
  File file = SD.open( path );
  if(!file) {
    #ifdef DEBUGLOGGING
      Serial.println("Logfile doesn't exist");
      Serial.println("Creating Logfile...");
    #endif
    
    writeFile(SD, path, message);

    File file = SD.open(path);
    if(file){
      file.close();
      bFileExists = true;
    }else{
      bFileExists = false;
    }
    
  }else {
    #ifdef DEBUGLOGGING
      Serial.println("Logfile already exists"); 
    #endif
    bFileExists = true; 
  }
  file.close();
  return bFileExists;
}
// Logging, wer zuletzt ausgeliehen / zurueckgebracht hat
void logToSDCard(String pMessage){
  // String sID = String(rfid.uid.uidByte[0]) + " " + String(rfid.uid.uidByte[1]) + " " + String(rfid.uid.uidByte[2]) + " " + String(rfid.uid.uidByte[3]);
  // String sName = getTokenName(sID)
  // MAXLOGLINE
  if( createLogFile(SD, LOGFILE, "### Logfile ###\r\n") ){
    appendFile(SD, LOGFILE, pMessage.c_str());
    
  }else{
    Serial.println("Logfile konnte nicht erstellt werden.");
  }


  
}



void operationNormal(){

  // Reset the loop if no new card present on the sensor/reader. This saves the entire process when idle.
  if (!rfid.PICC_IsNewCardPresent())
    return;

  // Verify if the NUID has been readed
  if (!rfid.PICC_ReadCardSerial())
    return;
  #ifdef DEBUGLOGGING
    Serial.print(F("PICC type: "));
  #endif
  MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);

  #ifdef DEBUGLOGGING
    Serial.println(rfid.PICC_GetTypeName(piccType));
  #endif
  
  // Check is the PICC of Classic MIFARE type
  if (piccType != MFRC522::PICC_TYPE_MIFARE_MINI &&
    piccType != MFRC522::PICC_TYPE_MIFARE_1K &&
    piccType != MFRC522::PICC_TYPE_MIFARE_4K)
  {
    #ifdef DEBUGLOGGING
      Serial.println(F("Your tag is not of type MIFARE Classic."));
    #endif
    return;
  }

  //----
  for (byte i = 0; i < 4; i++){
    byCardID[i] = rfid.uid.uidByte[i];
  }
  //-----
  String sID = String(rfid.uid.uidByte[0]) + " " + String(rfid.uid.uidByte[1]) + " " + String(rfid.uid.uidByte[2]) + " " + String(rfid.uid.uidByte[3]);
  //displayShowText("Karte gelesen:", sID, getTokenName(sID) );
  if(getTokenName(sID)==""){
    //----Token nicht auf der SD Karte vorhanden
  }else{
    if (rfid.uid.uidByte[0] != nuidPICC[0] ||
    rfid.uid.uidByte[1] != nuidPICC[1] ||
    rfid.uid.uidByte[2] != nuidPICC[2] ||
    rfid.uid.uidByte[3] != nuidPICC[3])
    {
      //unbekannte Karte
      #ifdef DEBUGLOGGING
        Serial.println(F("A new card has been detected."));
      #endif
      // Store NUID into nuidPICC array
      for (byte i = 0; i < 4; i++)
      {
        nuidPICC[i] = rfid.uid.uidByte[i];
      }

      #ifdef DEBUGLOGGING
        Serial.println(F("The NUID tag is:"));
        Serial.print(F("In hex: "));
        printHex(rfid.uid.uidByte, rfid.uid.size);
        Serial.println();
        Serial.print(F("In dec: "));
        printDec(rfid.uid.uidByte, rfid.uid.size);
        Serial.println();
      #endif
    }else{
      //Bekannte Karte
      //Serial.println(F("Card read previously."));
    }

    //----Uns ist das alles egal: Token vorm Sensor sorgt fuer Vergleich mit der gespeicherten ID
    bool bKnownToken = true;
    for (int i = 0; i < 4; i++){
      bKnownToken = bKnownToken && (byLeihID[i] == byCardID[i]);
    }
  
    if (bKnownToken){ 
      openDoor();
      #ifdef DEBUGLOGGING
        Serial.println("Token bekannt");
      #endif
      bPrintNewState = true;
      if (bAusgeliehen){
        #ifdef DEBUGLOGGING
          Serial.println("Ausgeliehen war TRUE wird FALSE");
        #endif
        bAusgeliehen = false;
        logToSDCard("Zurueck\r\n");
        sTokenName = getTokenName(sID);
        //displayShowText("Zurueck", String(byLeihID[0]) + " " + String(byLeihID[1]) + " " + String(byLeihID[2]) + " " + String(byLeihID[3]), getTokenName(sID));
      }else{
        #ifdef DEBUGLOGGING
          Serial.println("Ausgeliehen 1  war FALSE wird TRUE");
        #endif
        for (int i = 0; i < 4; i++){
          byLeihID[i] = byCardID[i];
        }
        bAusgeliehen = true;
        logToSDCard("Ausgeliehen;" + getTokenName(sID) + ";" + sID + ";");
        sTokenName = getTokenName(sID);
        //displayShowText("Ausgeliehen", String(byLeihID[0]) + " " + String(byLeihID[1]) + " " + String(byLeihID[2]) + " " + String(byLeihID[3]), getTokenName(sID));
      }
    }else{
      #ifdef DEBUGLOGGING
        Serial.println("Token unbekannt");
      #endif
      if (bAusgeliehen){
        #ifdef DEBUGLOGGING
          Serial.println("Ausgeliehen == TRUE - nix machen");
        #endif
        //displayInfo("Ausgeliehen", String(byLeihID[0]) + " " + String(byLeihID[1]) + " " + String(byLeihID[2]) + " " +String(byLeihID[3]) );
      }else{
        openDoor();
        #ifdef DEBUGLOGGING
          Serial.println("Ausgeliehen 2 war FALSE wird TRUE");
        #endif
        for (int i = 0; i < 4; i++){
          byLeihID[i] = byCardID[i];
        }
        bPrintNewState = true;
        bAusgeliehen = true;
        sTokenName = getTokenName(sID);
        logToSDCard("Ausgeliehen;" + getTokenName(sID) + ";" + sID + ";");
        //displayShowText("Ausgeliehen", String(byLeihID[0]) + " " + String(byLeihID[1]) + " " + String(byLeihID[2]) + " " + String(byLeihID[3]), getTokenName(sID));
      }
    }
  }

  // Halt PICC
  rfid.PICC_HaltA();

  // Stop encryption on PCD
  rfid.PCD_StopCrypto1();
}




void recallState(){
  Serial.println("recallState()");
  File file = SD.open(LOGFILE);
  if(!file) {
    return;
  }
  
  String sLastLine = getLastLineFromFile(SD, LOGFILE);
  Serial.print("recallState(): Lastline:");Serial.println(sLastLine);
  if(sLastLine.length()>5){
    if(sLastLine.indexOf("urueck") <= 0){
      //  Ausgeliehen;Christian Mastroiacovo;151 141 128 200;⸮
  
      String sName = split(sLastLine, ';', 1);
      String sID = split(sLastLine, ';', 2);
  
      Serial.println("recallState():" + sName + ";" + sID);
      String sByte0 = split(sID, ' ', 0);
      String sByte1 = split(sID, ' ', 1);
      String sByte2 = split(sID, ' ', 2);
      String sByte3 = split(sID, ' ', 3);
  
      byLeihID[0] = sByte0.toInt();
      byLeihID[1] = sByte1.toInt();
      byLeihID[2] = sByte2.toInt();
      byLeihID[3] = sByte3.toInt();
  
      bAusgeliehen = true;

      //sStateText = String(byLeihID[0]) + " " + String(byLeihID[1]) + " " + String(byLeihID[2]) + " " + String(byLeihID[3]), getTokenName(sID);
      //displayShowText("Ausgeliehen", String(byLeihID[0]) + " " + String(byLeihID[1]) + " " + String(byLeihID[2]) + " " + String(byLeihID[3]), getTokenName(sID);
      sTokenName = getTokenName(sID);
      bPrintNewState=true;
    }
  }else{
    displayShowInfo();
  }
}


void openDoor(){
  Serial.println("openDoor");
  digitalWrite(DOORPIN, HIGH);
  delay(750);
  digitalWrite(DOORPIN, LOW);
}


void setColor(int pRed, int pGreen, int pBlue){
  //strip.clear();
  for (int i=0; i<LED_COUNT; i++){
    strip.setPixelColor(i, pRed, pGreen, pBlue);
  }
  strip.setBrightness(BRIGHTNESS);
  strip.show();
  //bNewMode = false;
}

/*
void setColor_red(){
  setColor(255, 0, 0);
}

void setColor_green(){
  setColor(0, 255, 0);
}

void setColor_blue(){
  setColor(0, 0, 255);
}
*/
void queryButton(){
  //----Setting the OperationMode
   if(digitalRead(BUTTON)==true){ 
    bButtonPress = false;
  }else{
      if(bButtonPress==false){
        iOP_Mode++;
        iOP_Mode = iOP_Mode % OP_MODE_COUNT;
        displayShowModus(iOP_Mode);
        #ifdef DEBUGLOGGING
            Serial.print("OP Mode:");Serial.println(iOP_Mode, DEC);
        #endif
        bButtonPress = true;
      }
      delay(100);//Debounce
  }
}

void displayShowInfo(){
  display.clearDisplay();
  // set text color / Textfarbe setzen
  display.setTextColor(WHITE);
  // set text size / Textgroesse setzen
  display.setTextSize(1);
  // set text cursor position / Textstartposition einstellen
  display.setCursor(1, 0);
  // show text / Text anzeigen
  display.println("Andyland.info");
  display.setCursor(0, 10);
  display.println("-------------");
  display.display();
}

void displayShowText(String pLine1, String pLine2, String pLine3){
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(1, 0);
  display.println(pLine1);
  display.setCursor(0, 10);
  display.println(pLine2);
  display.setCursor(0, 20);
  display.println(pLine3);
  display.display();
}

void displayInfo(String pText1, String pText2)
{
  display.clearDisplay();
  // set text color / Textfarbe setzen
  display.setTextColor(WHITE);
  // set text size / Textgroesse setzen
  display.setTextSize(1);
  // set text cursor position / Textstartposition einstellen
  display.setCursor(1, 0);
  // show text / Text anzeigen
  display.println(pText1);
  display.setCursor(0, 10);
  display.println(pText2);
  display.display();
}


void displayShowModus(int pModus){
  String sTmp;
  if(pModus==OP_MODE_NORMAL){
    sTmp = "Normal";
  }else if(pModus == OP_MODE_LISTTOKENS){
    sTmp = "List Tokens";
  }else if(pModus == OP_MODE_STORETOKENS){
  sTmp = "Store Tokens";
  iCounter = 0;
  }else if(pModus == OP_MODE_TEST){
    sTmp = "Misc";
  }else{
    sTmp = String(pModus);
  }
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(1, 0);
  display.print("Modus:");
  display.println(sTmp);
  display.display();
}

/**
 * Helper routine to dump a byte array as hex values to Serial. 
 */
void printHex(byte *buffer, byte bufferSize)
{
  for (byte i = 0; i < bufferSize; i++)
  {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
  }
}


/**
 * Helper routine to dump a byte array as dec values to Serial.
 */
void printDec(byte *buffer, byte bufferSize)
{
  for (byte i = 0; i < bufferSize; i++)
  {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], DEC);
  }
}
