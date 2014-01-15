#include <SPI.h>
// #include <Ardunio.h>
#include <Ethernet.h>
#include <Flash.h>
// #include <SD.h>
// #include <string.h>
#include <TinyWebServer.h>
#include <aJSON.h>
#include <OneWire.h>
#include "./pin.h"

#define DEBUG 1

boolean file_handler(TinyWebServer& web_server);
boolean index_handler(TinyWebServer& web_server);
boolean pins_handler(TinyWebServer& web_server);
// boolean setup_handler(TinyWebServer& web_server);
boolean digital_pin_handler(TinyWebServer& web_server);
// boolean analog_pin_handler(TinyWebServer& web_server);
// String parse_path_string(String pathstring, int size);
void parse_path_string(const char* str, int size, char **messages);
void get_request_data(TinyWebServer &web_server, char *str);

const char *HOST = "localhost:9000";
Pin pins[11] = {
  Pin(9, OUTPUT_T),
  Pin(8, OUTPUT_T),
  Pin(7, ONEWIRE),
};

OneWire ds(7);

int numPins = 3;

static uint8_t mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

// Don't forget to modify the IP to an available one on your home network
byte ip[] = { 10, 0, 1, 32 };

boolean index_handler(TinyWebServer& web_server) {
  web_server << "<html><head><title>Web server</title></head>";
  web_server << "<body>";
  web_server << "<script id=\"appscript\" src=\"http://" << HOST << "/scripts/scripts.js\"></script>";
  web_server << "</body></html>";
  return true;
}

// API
// GET /pins
boolean pins_handler(TinyWebServer& web_server) {
  web_server.send_error_code(200);
  web_server.send_content_type("application/javascript");
  web_server.end_headers();

  pinsToString(web_server);
  return true;
}

// POST /pins/digital
boolean digital_pin_handler(TinyWebServer& web_server) {
    const char* length_str = web_server.get_header_value("Content-Length");
    int len = atoi(length_str);

    char* data = (char*)malloc(len);
    if (data) memset(data, 0, sizeof(len));
    get_request_data(web_server, len, data);

    // DIGITAL
#if DEBUG
    Serial.print(F("digital_pin_handler: "));
    Serial.print(data);// << "JSON data ->" << data << "<-\n";
    Serial.print(F("\n"));
#endif
    aJsonObject* root = aJson.parse(data);
    free(data);
    aJsonObject* pins = aJson.getObjectItem(root, "pins");

    int arrSize = aJson.getArraySize(pins);
#if DEBUG
    Serial.print(F("\nArray of pins: "));// << "Array of pins: " << arrSize << "\n";
    Serial.print(arrSize);
    Serial.print(F("\n"));
#endif

    aJsonObject* pinObj;
    aJsonObject* pinName;
    aJsonObject* pinValue;
    aJsonObject* pinMode;
    aJsonObject* pinAction;

    for(int i=0; i<arrSize; i++){
      pinObj = aJson.getArrayItem(pins, i);

      pinName = aJson.getObjectItem(pinObj, "pin");
      pinValue = aJson.getObjectItem(pinObj, "value");
      pinMode = aJson.getObjectItem(pinObj, "mode");
      pinAction = aJson.getObjectItem(pinObj, "action");
      
#if DEBUG
      Serial.print(F("PIN:"));// << "Working with pin " << pinName->valueint << "\n";
      Serial.print(pinName->valueint);
      Serial.print(F("\n"));
#endif
      Pin *p = select_pin(pinName->valueint);
      if (pinMode != NULL) {
#if DEBUG
      Serial.print(F("MODE: "));// << "Working with pin " << pinName->valueint << "\n";
      Serial.print(pinMode->valueint);
      Serial.print(F("\n"));
#endif
        p->setMode(pinMode->valueint); 
        aJson.deleteItem(pinMode);
      }
      if (pinAction != NULL) {
#if DEBUG
        Serial.print(F("ACTION:"));// << "Working with pin " << pinName->valueint << "\n";
        Serial.print(pinAction->valuestring);
        Serial.print(F("\n"));
#endif
        if (strcmp(pinAction->valuestring, "getTemp") == 0) {
          float currTemp = getTemp(ds);
          p->setCurrentValue(currTemp);
        }
        aJson.deleteItem(pinAction);
      }
      if (pinValue != NULL) {
#if DEBUG
      Serial.print(F("VALUE:"));// << "Working with pin " << pinName->valueint << "\n";
      Serial.print(pinValue->valueint);
      Serial.print(F("\n"));
#endif      
        p->setState(pinValue->valueint == 1 ? HIGH : LOW);
      }
    }

  aJson.deleteItem(root);
  web_server.send_error_code(200);
  web_server.send_content_type("application/javascript");
  web_server.end_headers();

  pinToString(web_server, pinName->valueint);

#if DEBUG
    printProgStats();
#endif

  return true;
}

TinyWebServer::PathHandler handlers[] = {
  // {"/pins/analog", TinyWebServer::GET, &analog_pin_handler},
  {"/pins/digital", TinyWebServer::POST, &digital_pin_handler},
  {"/pins", TinyWebServer::GET, &pins_handler},
  {"/", TinyWebServer::GET, &index_handler },
  {NULL},
};

const char* headers[] = {
  "Content-Length",
  NULL
};
TinyWebServer web = TinyWebServer(handlers, headers);

const char* ip_to_str(const uint8_t* ipAddr)
{
  static char buf[16];
  sprintf(buf, "%d.%d.%d.%d\0", ipAddr[0], ipAddr[1], ipAddr[2], ipAddr[3]);
  return buf;
}

float getTemp(OneWire sensor){
  //returns the temperature from one DS18S20 in DEG Celsius
  byte data[12];
  byte addr[8];

  if ( !sensor.search(addr)) {
     //no more sensors on chain, reset search
     sensor.reset_search();
     return -1000;
  }

  if ( OneWire::crc8( addr, 7) != addr[7]) {
     Serial.println("CRC is not valid!");
     return -1000;
  }

  if ( addr[0] != 0x10 && addr[0] != 0x28) {
     Serial.print("Device is not recognized");
     return -1000;
  }

  sensor.reset();
  sensor.select(addr);
  sensor.write(0x44,1); // start conversion, with parasite power on at the end

  byte present = sensor.reset();
  sensor.select(addr);  
  sensor.write(0xBE); // Read Scratchpad


  for (int i = 0; i < 9; i++) { // we need 9 bytes
    data[i] = sensor.read();
  }

  sensor.reset_search();

  byte MSB = data[1];
  byte LSB = data[0];

  float tempRead = ((MSB << 8) | LSB); //using two's compliment
  float TemperatureSum = tempRead / 16;

  return ((TemperatureSum*9)/5) + 32;
}

void setup() {

#if DEBUG
  Serial.begin(9600);
#endif
  // pinMode(10, OUTPUT); // set the SS pin as an output (necessary!)
  // digitalWrite(10, HIGH); // but turn off the W5100 chip!

#if DEBUG
  Serial.print(F("Setting up pins...\n"));
#endif

  // numPins = SetupArduino(pins);
  for(int i=0; i < numPins; i++){
    pins[i].InitializeState();
  }

  UpdatePinsState();

#if DEBUG
  for(int i=0; i < numPins; i++){
    pins[i].Print();
  }
#endif

  // if (!card.init(SPI_FULL_SPEED, 4)) {
  //   Serial << F("card failed\n");
  // }
  // // initialize a FAT volume
  // if (!volume.init(&card)) {
  //   Serial << F("vol.init failed!\n");
  // }
  // root.openRoot(volume);
  // root.ls();

#if DEBUG
  Serial.print(F("Setting up the Ethernet card...\n"));
#endif
  Ethernet.begin(mac, ip);

  // Start the web server.
#if DEBUG
  Serial.print(F("Web server starting...\n"));
#endif

  web.begin();

#if DEBUG  
  Serial.print(F("Ready to accept HTTP requests.\n\n"));
#endif

#if DEBUG
    printProgStats();
#endif
}

void loop() {
  web.process();
}

// Iterate over pins and update each of their states
void UpdatePinsState() {
  for(int i=0; i<numPins; i++){ pins[i].getState(); }
}

/*
*   Pin to string on the webserver helper
*   This takes a web_server instance and
*   writes the JSON array associated with
*   each of the pins
*/
bool pinsToString(TinyWebServer& web_server) {
  UpdatePinsState();

  web_server << F("{\"pins\":[");
  int len = numPins;
  for(int i=0; i<len; i++){
    // digitalPins[i]
    web_server << F("{\"pin\":");
    web_server << pins[i].getPin();
    web_server << F(",\"value\":");
    web_server << pins[i].getCurrentValue();
    web_server << F("}");
    if ((i+1) < len) web_server << F(",");
  }
  web_server << F("]}");
  // web_server << F("}");
  return true;
}

bool pinToString(TinyWebServer& web_server, int pin_number) {
  UpdatePinsState();

  // web_server << F("{\"pins\": ");
  // web_server << F("{\"digital\":[");
  int len = numPins;
  Pin *p = select_pin(pin_number);
  // for(int i=0; i<len; i++){
    // digitalPins[i]
    web_server << F("{\"pin\":");
    web_server << p->getPin();
    web_server << F(",\"value\":");
    web_server << p->getCurrentValue();
    web_server << F("}");
  // }
  // web_server << F("}");
  // web_server << F("}");
  return true;
}

/*
*   Count the number of '/'
*   characters in the pathstring
*/
int count_forward_slashes(String pathstring) {
  int p = 0;
  int count = 0;
  while (pathstring[p] != '\0') {
    if (pathstring[p] == '/') count++;
    p++;
  }
  return count;
}

/*
*   Parse the requested URL into
*   an array of strings
*/
void parse_path_string(const char* cStr, int size, char** messages) { 
  char* str = strdup(cStr);
  char *p;
  int i = 0;
#if DEBUG
    Serial << "string: " << str << "\n";
#endif

  for(str = strtok_r(str,"/",&p); str; str = strtok_r(NULL, "/", &p))
  {
    int len = strlen(str);
    char *msg = (char *)malloc((len + 1) * sizeof(char)); // sizeof(char) == 1
    strncpy(msg, str, len + 1);
    // char *msg = strdup(str);
#if DEBUG
    Serial << "Writing " << i << " as " << msg << " (" << str << ")\n";
#endif
    messages[i] = msg;
    i++;
  }
}

/*
*   Read the remaining data from a request
*   based upon the 'Content-Length' header
*/
void get_request_data(TinyWebServer& web_server, int length_str, char* str)
{
  char ch;
  Client& client = web_server.get_client();
  if(client.available()) {
    for(int i=0; i<length_str; i++){
      ch = (char)client.read();
      if (ch != NULL && ch != '\n') {
        str[i] = ch;
      }
    }
  }
}

/*
*   Select the pin object from the 
*   pin array based upon the pinNumber
*/
Pin *select_pin(uint8_t pinNumber) {
  for(int i=0; i<numPins; i++){
    if (pins[i].getPin() == pinNumber) return &pins[i];
  }
  return NULL;
}

/*
*   Get the free ram
*/
int freeRam() {
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}

void printProgStats() {
#if DEBUG
  int freeR = freeRam();
  Serial.print(F("Free ram: "));
  Serial.print(freeR);
  Serial.print(F("\n"));
#endif
}