
#include "DebugUtils.h"
#include "libs.h"
#include "credentials.h"


#define RTCMEMORYSTART 64
#define RTCMEMORYLEN 128

extern "C" {
  #include "user_interface.h" //for RTC memory read/write
}

//Pin 2 shall be used to switch LED/Relais/SSR
#define SWITCHPIN 2

//global variables
bool stateOfSwitch, lastStateOfSwitch = 99;
bool automatismIsActive = true;
uint8_t isValid = 0;
uint8_t hOn = HOUR_ON;
uint8_t minOn = 0;
uint8_t hOff = HOUR_OFF;
uint8_t minOff = 0;
uint8_t h = 0;
uint8_t min = 0;
uint8_t s = 0;
uint8_t d = 0;
uint8_t m = 0;
uint8_t y = 0;
time_t t;
uint8_t last_min = 99;
uint8_t last_s = 99;
// char valStr[5];
// String sBuff;
uint8_t buckets;
bool toggleFlag;
//char msg[50];
//debug help variables
//long t1,t2;

WiFiClientSecure espClient;

//function to prepend string with a zero if it's only single digit
String m2D(int s){
  String sReturn = "DD";
  if (s < 10){sReturn = ("0"+String(s));}
  else {sReturn = String(s);}
  return sReturn;
}
//Function to check whether given String is a valid number
bool isValidNumber(String str){
   for(byte i=0;i<str.length();i++){
     if(isDigit(str.charAt(i))) return true;
   }
   return false;
}

#ifdef TELEGRAM
String chat_id;
bool expectingMinute, expectingHour, expectingTimeOn;
bool alarmActive = true;
const char an[] PROGMEM = "AN";
const char aus[] PROGMEM = "AUS";
const char fail[] PROGMEM = "Fehler!";
const char ok[] PROGMEM = "OK!";
const char stunde[] PROGMEM = "Stunde:";
const char notification[] PROGMEM = "Benachrichtigungen aktiviert!";
const char silence[] PROGMEM = "Benachrichtigungen deaktiviert!";
#endif

#ifdef MQTT
//MQTT client instance
PubSubClient client(espClient);
void MQTTCallback(char* topic, byte* payload, unsigned int length){
  // Print topic and payload
  Serial.print("* ");
  for (int i = 0; i < 10; i++) {
    Serial.print((char)topic[i]);
  }
  Serial.print(": ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.print(" - ");
  // activate timer if a 1 was received
  if ((char)topic[9] == 't'){
    if (payload[0]-48 == 1){
      Serial.println("Timer activated!");
      automatismIsActive = true;
      // disable timer if a zero was received
    }else if (payload[0]-48 == 0){
      Serial.println("Timer disabled!");
      automatismIsActive = false;
    }
    else{
      Serial.println("Unexpected value!");
    }
  }
  else if ((char)topic[9] == 's'){
    // switch on SWITCHPIN if an 1 was received as first character
    if (payload[0]-48 == 1){
      Serial.println("Switch on!");
      stateOfSwitch = true;
    // switch off SWITCHPIN if a 0 was received as first character
    }else if (payload[0]-48 == 0){
      Serial.println("Switch off!");
      stateOfSwitch = false;
    }
    else{Serial.println("Unexpected value!");}
    //send present data to all via WS connected clients
    wsSendJson(99);
  }
  else{Serial.println("Unexpected topic!");}
}
void verifyFingerprint(){
  const char* host = MQTT_SERVER;
  Serial.print("Connecting to ");
  Serial.print(host);
  if (! espClient.connect(host, MQTT_PORT)) {
    Serial.println(": Connection failed. Halting execution.");
    while(1);
  }
  if (espClient.verify(fingerprint, host)) {
    Serial.println(": Connection secure.");
  } else {
    Serial.println(": Connection insecure! Halting execution.");
    while(1);
  }
}
bool connect2MQTTBroker(){
  // Attempt to connect
  if (client.connect("", USER, PASS, USER PREAMBLE F_TIMER, 1, 1, "CONNECTION LOST")) {
    // Once connected, publish an announcement...
    client.publish(USER PREAMBLE F_TIMER,"CONNECTION ESTABLISHED");
    // ...and subscribe to timer and switch topic
    client.subscribe(USER PREAMBLE F_TIMER,1);
    client.subscribe(USER PREAMBLE F_SWITCH,1);

  } else {
    Serial.print("failed, rc=");
    Serial.println(client.state());
  }
  return client.connected();
}
void MQTTCallback(char* topic, byte* payload, unsigned int length);
//void publishData();
#endif

#ifdef TELEGRAM
//WiFiClientSecure BOTclient;
//UniversalTelegramBot bot(botToken, espClient);
UniversalTelegramBot *bot;
void checkIfItIsHourOrMinute(String text){
  if (expectingHour){
    uint8_t h_temp = text.toInt();
    if(isValidNumber(text) && (h_temp >= 0) && (h_temp <= 24)){
      if(expectingTimeOn){hOn = h_temp;}else{hOff = h_temp;}
      // bot->sendMessage(chat_id, (PGM_P)F("Benachrichtigung bei ") + text + " °C", "");
       bot->sendMessage(chat_id, (PGM_P)F("Minute: "), "");
       expectingMinute = true;
    }
    else{
      bot->sendMessage(chat_id, F("Das ist keine gültige Stundenangabe!"), "");
      expectingTimeOn = false;
    }
    expectingHour = false;
  }
  else if (expectingMinute){
    uint8_t min_temp = text.toInt();
    if(isValidNumber(text) && (min_temp >= 0) && (min_temp <= 60)){
      if(expectingTimeOn){minOn = min_temp;}else{minOff = min_temp;}
      bot->sendMessage(chat_id, F("OK!"), "");
    }
    else{
      bot->sendMessage(chat_id, F("Das ist keine gültige Minutenangabe!"), "");
    }
    expectingMinute = false;
    expectingTimeOn = false;
  }
}
void handleNewMessages(int numNewMessages) {
  //Serial.println(F("No. Messages: "));
  //Serial.print(String(numNewMessages));
  for (int i=0; i<numNewMessages; i++) {
    chat_id = String(bot->messages[i].chat_id);
    String text = bot->messages[i].text;
    String from_name = bot->messages[i].from_name;
    String senderID = bot->messages[i].from_id;
    //if (from_name == "") from_name = "Guest";
    if (senderID == adminID) {
      Serial.println(text);
      if (expectingHour || expectingMinute){
        checkIfItIsHourOrMinute(text);
      }
      else if (text == "/start") {
        String welcome = "Hi " + from_name + ",\n";
        welcome += F("Wie kann ich helfen?\n\n");
        welcome += F("/on : Einschalten\n");
        welcome += F("/off : Ausschalten\n");
        welcome += F("/setOnTime : Einschaltzeit setzen\n");
        welcome += F("/setOffTime : Ausschaltzeit setzen\n");
        welcome += F("/AutoOn : Automatik aktivieren \n");
        welcome += F("/AutoOff : Automatik deaktivieren \n");
        welcome += F("/status : Anzeige des akt. Status\n");
        welcome += F("/notify: Benachrichtigungen aktivieren\n");
        welcome += F("/silence: Benachrichtigungen deaktivieren\n");
        welcome += F("/options : alle Optionen\n");
        bot->sendMessage(chat_id, welcome, "Markdown");
      }
      else if (text == "/options") {
        String keyboardJson = F("[[\"/on\", \"/off\",\"/AutoOn\",\"/AutoOff\"],[\"/setOnTime\", \"/setOffTime\"],[\"/notify\", \"/silence\"],[\"/status\"]]");
        bot->sendMessageWithReplyKeyboard(chat_id, (F("Wähle eine der folgenden Optionen:")), "", keyboardJson, true);
      }
      else if (text == "/on") {
        //digitalWrite(SWITCHPIN, HIGH);   // turn the LED on (HIGH is the voltage level)
        stateOfSwitch = 1;
        bot->sendMessage(chat_id, FPSTR(ok), "");
      }
      else if (text == "/off") {
        //digitalWrite(SWITCHPIN, LOW);    // turn the LED off (LOW is the voltage level)
        stateOfSwitch = 0;
        bot->sendMessage(chat_id, FPSTR(ok), "");
      }
      else if (text == "/setOnTime"){
        bot->sendMessage(chat_id, FPSTR(stunde), "");
        expectingHour = true;
        expectingTimeOn = true;
      }
      else if (text == "/setOffTime"){
        bot->sendMessage(chat_id, FPSTR(stunde), "");
        expectingHour = true;
      }
      else if (text == "/AutoOn"){
        bot->sendMessage(chat_id, FPSTR(an), "");
        automatismIsActive = true;
      }
      else if (text == "/AutoOff"){
        bot->sendMessage(chat_id, FPSTR(aus), "");
        automatismIsActive = false;
      }
      else if (text == "/status") {
        String buff;
        buff = m2D(hOn)+":"+m2D(minOn);
        bot->sendMessage(chat_id, (PGM_P)F("Einschaltzeit: ") + buff,"");
        buff = m2D(hOff)+":"+m2D(minOff);
        bot->sendMessage(chat_id, (PGM_P)F("Ausschaltzeit: ") + buff,"");
        if(stateOfSwitch){
          bot->sendMessage(chat_id, (PGM_P)F("Stern AN"), "");
        } else {
          bot->sendMessage(chat_id, (PGM_P)F("Stern AUS"), "");
        }
        if(automatismIsActive){
          bot->sendMessage(chat_id, (PGM_P)F("Automatik AN "), "");
        } else {
          bot->sendMessage(chat_id, (PGM_P)F("Automatik AUS"), "");
        }
        if (alarmActive){
          bot->sendMessage(chat_id, FPSTR(notification),"");
        }
        else {bot->sendMessage(chat_id, FPSTR(silence),"");}
        buff = (String)ESP.getFreeHeap();
        bot->sendMessage(chat_id, (PGM_P)F("Heap frei: ") + buff,"");
      }
      else if (text == "/notify") {
        alarmActive = true;
        bot->sendMessage(chat_id, FPSTR(notification),"");
      }
      else if (text == "/silence") {
        alarmActive = false;
        bot->sendMessage(chat_id, FPSTR(silence),"");
      }
      else {
        bot->sendMessage(chat_id, "?!?: " + text, "");
        bot->sendPhoto(chat_id, F("https://ih1.redbubble.net/image.31887377.4850/fc,550x550,white.jpg"), F("...feels bad man..."));
      }
    }
    else {
      //bot->sendPhoto(chat_id, "https://ih1.redbubble.net/image.31887377.4850/fc,550x550,white.jpg", from_name + "Please leave me alone...");
      bot->sendMessage(chat_id, (PGM_P)F("Zugriff verweigert. Deine ID: ") + senderID, "");
    }
  }
}
#endif

#ifdef WEBAPP
//WebServer for hosting GUI
//for WebSocket Connection to Client with GUI
ESP8266WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);
#endif

//struct for RTC memory access
typedef struct {
  int isValid;
  int t;
  int hOn;
  int minOn;
  int hOff;
  int minOff;
} rtcStore;

rtcStore rtcMem;

#ifdef NTP
WiFiUDP ntpUDP;
// You can specify the time server pool and the offset (in seconds, can be
// changed later with setTimeOffset() ). Additionaly you can specify the
// update interval (in milliseconds, can be changed using setUpdateInterval() ).
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 3600, 60000);
time_t ntpSyncProvider() {
  return timeClient.getEpochTime();
}
// vereinfachte Umsetzung der CEST (Umstellung jeweils um 0:00 Uhr)
bool IsCEST(time_t t){
  int m = month(t);
  if (m < 3 || m > 10)  return false;
  if (m > 3 && m < 10)  return true;
  int previousSunday = day(t) - weekday(t);
  if (m == 3) return previousSunday >= 25;
  if (m == 10) return previousSunday < 25;
  return false; // this line should never gonna happen
}
void check4CEST(time_t t){
  if(IsCEST(t)){
    timeClient.setTimeOffset(7200);
    //Serial.println(F("CEST is on!"));
  }
  else{
    timeClient.setTimeOffset(3600);
    //Serial.println(F("CEST is off!"));
  }
}
#endif

void setupWiFi(){
  Serial.println();
  Serial.print("Connecting to ");
  Serial.print(SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
}

#ifdef WEBAPP
// identify file by extension and return appropriate content-type
String getContentType(String filename) {
    if (server.hasArg("download")) return "application/octet-stream";
    else if (filename.endsWith(".htm")) return "text/html";
    else if (filename.endsWith(".html")) return "text/html";
    else if (filename.endsWith(".css")) return "text/css";
    else if (filename.endsWith(".js")) return "application/javascript";
    else if (filename.endsWith(".png")) return "image/png";
    else if (filename.endsWith(".gif")) return "image/gif";
    else if (filename.endsWith(".jpg")) return "image/jpeg";
    else if (filename.endsWith(".ico")) return "image/x-icon";
    else if (filename.endsWith(".xml")) return "text/xml";
    else if (filename.endsWith(".pdf")) return "application/x-pdf";
    else if (filename.endsWith(".zip")) return "application/x-zip";
    else if (filename.endsWith(".gz")) return "text/plain\r\nCache-Control: public, max-age=3600";
    return "text/plain";
}
bool handleFileRead(String path) {
    Serial.println("handleFileRead: " + path);
    if (path.endsWith("/")) path += "index.htm";
    String contentType = getContentType(path);
    String pathWithGz = path + ".gz";
    if (SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)) {
        if (SPIFFS.exists(pathWithGz))
            path += ".gz";
        File file = SPIFFS.open(path, "r");
        size_t sent = server.streamFile(file, contentType);
        file.close();
        return true;
    }
    return false;
}
//format bytes
String formatBytes(size_t bytes) {
    if (bytes < 1024) {
        return String(bytes)+"B";
    } else if (bytes < (1024 * 1024)) {
        return String(bytes/1024.0)+"KB";
    } else if (bytes < (1024 * 1024 * 1024)) {
        return String(bytes/1024.0/1024.0)+"MB";
    } else {
        return String(bytes/1024.0/1024.0/1024.0)+"GB";
    }
}
//packing data into json string and sending it to client(s)
void wsSendJson(uint8_t num) {
	StaticJsonBuffer<400> jsonBuffer;
	JsonObject& root = jsonBuffer.createObject();
	root["S"] = stateOfSwitch;
  root["T"] = automatismIsActive;
  root["hOn"] = m2D(hOn) + ":" + m2D(minOn);
  root["hOff"] = m2D(hOff) + ":" + m2D(minOff);
  root["time"] = m2D(hour()) + ":" + m2D(minute());
  root["date"] = String(year()) + "-" + m2D(month()) + "-" + m2D(day());
	String output;
	// output is tinysized.
	root.printTo(output);
	// This one will retain formatting
	//root.prettyPrintTo(output);
  //broadcasting if num == 99
  if(num == 99){webSocket.broadcastTXT(output);}
  //unicasting otherwise
  else{webSocket.sendTXT(num,output);}
}
//websocket event handler
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.printf("Client [%u] Disconnected!\r\n", num);
    break;
    case WStype_CONNECTED:
      {
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("Client [%u] connected - ip: %d.%d.%d.%d: %s\r\n", num, ip[0], ip[1], ip[2], ip[3], payload);
				// send message to client
				// webSocket.sendTXT(num, "[SERVER] Connected");
        wsSendJson(num);
      }
    break;
    case WStype_TEXT:
      Serial.printf("Client [%u]: %s\r\n", num, payload);
			// checking for magic char

      //switch state update
      if (payload[0] == 'S') {
        if(payload[1] == '1'){
          //Serial.println("Switch On!");
          stateOfSwitch = true;
        }
        else if (payload[1] == '0'){
          //Serial.println("Switch Off!");
          stateOfSwitch = false;
        }
      }
      //timer state update
      else if (payload[0] == 'T') {
        if(payload[1] == '1'){
          Serial.println("Timer Activated!");
          automatismIsActive = true;
        }
        else if (payload[1] == '0'){
          Serial.println("Timer Disabled!");
          automatismIsActive = false;
        }
      }
      //Hour On/Off or Date or Time update
      else if (payload[0] == 'N') {
        //new Hour update
        if(payload[1] == 'H'){
          char cPayload[length];
          // copying the payload
          memcpy(cPayload, payload, length);
          //new On Hour
          if(payload[2] == 'A'){
            Serial.println("New t On!");
            // parsing the payload
            String sOnH;
            String sOnMin;
            for(int i=3;i<5;i++){sOnH += String(cPayload[i]);}
            for(int i=6;i<8;i++){sOnMin += String(cPayload[i]);}
            //setting values
            hOn = sOnH.toInt();
            minOn = sOnMin.toInt();
            //DEBUG_PRINT(hOn);
            //DEBUG_PRINT(minOn);
          }
          //new Off Hour
          else if(payload[2] == 'B'){
            Serial.println("New t Off!");
            // parsing the payload
            String sOffH;
            String sOffMin;
            for(int i=3;i<5;i++){sOffH += String(cPayload[i]);}
            for(int i=6;i<8;i++){sOffMin += String(cPayload[i]);}
            //setting values
            hOff = sOffH.toInt();
            minOff = sOffMin.toInt();
            //DEBUG_PRINT(hOff);
            //DEBUG_PRINT(minOff);
          }
        }
        //new Date update
        else if(payload[1] == 'D'){
          char cPayload[length];
          // copying the payload
          memcpy(cPayload, payload, length);
          //new Date
          Serial.println("New Date!");
          // parsing the payload of form: NDyyyy-MM-dd
          String sY;
          String sM;
          String sD;
          int i;
          for(i = 2; i < 6; i++){sY += String(cPayload[i]);}
          for(i = 7; i < 9; i++){sM += String(cPayload[i]);}
          for(i = 10; i < 12; i++){sD += String(cPayload[i]);}
          y = sY.toInt();
          //Serial.println(y);
          m = sM.toInt();
          //Serial.println(m);
          d = sD.toInt();
          //Serial.println(d);
          setTime(h,min,s,d,m,y);
        }
        //new time update
        else if(payload[1] == 'T'){
          char cPayload[length];
          // copying the payload
          memcpy(cPayload, payload, length);
          //new Date
          Serial.println("New Time!");
          // parsing the payload of form: NTHH:mm
          String sH;
          String sMin;
          int i;
          for(i = 2; i < 4; i++){sH += String(cPayload[i]);}
          for(i = 5; i < 7; i++){sMin += String(cPayload[i]);}
          h = sH.toInt();
          //Serial.println(h);
          min = sMin.toInt();
          //Serial.println(min);
          setTime(h,min,s,d,m,y);
        }
        else{Serial.printf("Client [%u] is talking nonsense!\r\n", num);}
      }
      //sending update to requesting client
      else if (payload[0] == 'P'){
        Serial.printf("updating Client [%u]!\r\n", num);
        wsSendJson(num);
      }
    break;
    case WStype_BIN:
      Serial.printf("[%u] got binary length: %u\r\n", num, length);
      hexdump(payload, length);
    break;
 /*
 send message to client
 webSocket.sendBIN(num, payload, length);*/
  }
}
void setupSpiffsAndWebServer(){
  // Init our file system
  int test = SPIFFS.begin();
  //Serial.println(test);
  delay(100);
  {
      Dir dir = SPIFFS.openDir("/");
      while (dir.next()) {
          String fileName = dir.fileName();
          size_t fileSize = dir.fileSize();
          Serial.printf("FS File: %s, size: %s\r\n", fileName.c_str(), formatBytes(fileSize).c_str());
      }
      Serial.printf("\n");
  }
  // Setup 404 handle on the web server
  server.onNotFound([]() {
    if (!handleFileRead(server.uri()))
      server.send(404, "text/plain", "FileNotFound");
    });
  // Start web server
  server.begin();
  Serial.println("HTTP server started");
}
#endif

#ifdef OTA
void setupOTA(){
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";
    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
}
#endif

void updateStateOfSwitchIfAutomatismIsActive(){
  //if timer is active, check time and update stateOfSwitch if necessary
  if(automatismIsActive){
    //combining hour and minute to get one value to compare
    int tOn = (hOn*60)+minOn;
    int tOff = (hOff*60)+minOff;
    h = hour(t);
    min = minute(t);
    int tNow = (h*60)+min;
    Serial.printf("hOn: %d, minOn: %d, hOff: %d, minOff: %d, hNow: %d, minNow: %d\n\r",hOn,minOn,hOff,minOff,h,min);
    Serial.printf("tOn: %d, tOff: %d, tNow: %d\n\r",tOn,tOff,tNow);
    //if t On is smaller than t Off (^= across 2 days)
    if (tOn > tOff){
      if (tNow >= tOn || tNow < tOff){stateOfSwitch = true;}
      else {stateOfSwitch = false;}
    }
    //if t On is smaller than t Off (^= on same day)
    else if (tOn < tOff){
      if (tNow >= tOn && tNow < tOff){stateOfSwitch = true;}
      else {stateOfSwitch = false;}
    }
    //if t On and t Off are identical switch Off
    else if (tOn == tOff){stateOfSwitch = false;}
  }
}

void updateSwitch(){
  //update if switch state or timer state have changed
  if (stateOfSwitch != lastStateOfSwitch){
    //Serial.printf("stateOfSwitch: %d | automatismIsActive: %d\n\r", stateOfSwitch, automatismIsActive);
    //set output pin according to stateOfSwitch
    digitalWrite(SWITCHPIN,stateOfSwitch ? HIGH : LOW);

    #ifdef TELEGRAM
    String buff = (stateOfSwitch ? FPSTR(an) : FPSTR(aus));
    bot->sendMessage(chat_id, buff,"");
    #endif

    #ifdef WEBAPP
    //update all connected clients
    wsSendJson(99);
    #endif

    #ifdef MQTT
    //publish status to MQTT broker
    client.publish(USER PREAMBLE F_SWITCH, stateOfSwitch ? "1" : "0");
    client.publish(USER PREAMBLE F_TIMER, automatismIsActive ? "1" : "0");
    #endif

    //safe actual switch and timer states
    lastStateOfSwitch = stateOfSwitch;
  }
}

bool secondGone(){
  t = now();
  s = second(t);
  if (s != last_s){
    last_s = s;
    return 1;
  }
  else{return 0;};
}
bool minuteGone(){
  m = minute(t);
  if (m != last_min){
    last_min = m;
    return 1;
  }
  else{return 0;}
}

void writeOnTimeAndOffTimeIntoRtcMemory(){
  if(timeStatus()==timeSet){
  //DEBUG_PRINT("w RTC mem...");
    Serial.printf("write: t:%i, A:%i:%i, B:%i:%i\r\n",t,hOn,minOn,hOff,minOff);
    rtcMem.isValid = 99;
    rtcMem.t = t;
    rtcMem.hOn = hOn;
    rtcMem.hOff = hOff;
    rtcMem.minOn = minOn;
    rtcMem.minOff = minOff;
    system_rtc_mem_write(RTCMEMORYSTART, &rtcMem, buckets * 4);
  }
}

void readOnTimeAndOffTimeFromRtcMemory(){
  //DEBUG_PRINT("r RTC mem...");
  system_rtc_mem_read(RTCMEMORYSTART, &rtcMem, sizeof(rtcMem));
  isValid = rtcMem.isValid;
  if (isValid == 99){
    //t = rtcMem.t;
    hOn = rtcMem.hOn;
    hOff = rtcMem.hOff;
    minOn = rtcMem.minOn;
    minOff = rtcMem.minOff;
    //Serial.printf("read: A:%i:%i, B:%i:%i\r\n",hOn,minOn,hOff,minOff);
  }
  //else Serial.println(F("no values stored yet!"));
}

void printIpAddress(){
  Serial.print(F("WiFi connected! - IP address: "));
  IPAddress ip = WiFi.localIP();
  Serial.println(ip);
}

void printDateAndTime(){
  String sBuff = (m2D(hour())+":"+m2D(minute())+":"+m2D(second()));
  Serial.print(sBuff);
  Serial.print(" - ");
  sBuff = (m2D(day())+"."+m2D(month())+"."+String(year()));
  Serial.println(sBuff);
}
// void printDateAndTime2(){
//   t1 = millis();
//   snprintf (msg, 75, "%i:%i:%i", hour(), minute(), second());
//   Serial.print(msg);
//   Serial.print(" - ");
//   snprintf (msg, 75, "%i.%i.%i", day(), month(), year());
//   Serial.println(msg);
//   t2 = millis();
//   Serial.println(t2-t1);
// }

void setup() {
  pinMode(SWITCHPIN, OUTPUT);
  Serial.begin(115200);
  delay(500);
  Serial.println("");
  Serial.printf("Flash size: %d Bytes \n", ESP.getFlashChipRealSize());

  #ifdef WIFIMANAGER
  WiFiManager wifiManager;
  wifiManager.autoConnect("Kuckuck");
  #else
  setupWiFi();
  #endif

  printIpAddress();

  #ifdef TELEGRAM
  Serial.print(F("setup Telegram... "));
  bot = new UniversalTelegramBot(botToken, espClient);
  #endif

  #ifdef WEBAPP
  Serial.print(F("WebApp... "));
  webSocket.begin();
  //register WebSocket Callback event
  webSocket.onEvent(webSocketEvent);
  setupSpiffsAndWebServer();
  #endif

  #ifdef OTA
  Serial.print(F("OTA... "));
  setupOTA();
  #endif

  #ifdef MQTT
  Serial.print(F("MQTT... "));
  verifyFingerprint();
  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setCallback(MQTTCallback);
  #endif

  #ifdef NTP
  Serial.print(F("NTP... "));
  timeClient.begin();
  setSyncProvider(&ntpSyncProvider);
  setSyncInterval(60);
  #endif

  readOnTimeAndOffTimeFromRtcMemory();

  buckets = (sizeof(rtcMem)/4);
  if(buckets == 0) buckets = 1;

}

void loop() {
  yield();

  if (WiFi.status() != WL_CONNECTED) {ESP.reset();}

  #ifdef MQTT
  //check MQTT connection and connect if neccessary
  if (!client.connected()) {connect2MQTTBroker();}else{client.loop();}
  #endif

  #ifdef WEBAPP
  //handling WebSocket connection
  webSocket.loop();
  //handling http requests from clients
  server.handleClient();
  #endif
  //feeding watchdog

  #ifdef NTP
  timeClient.update();
  #endif

  #ifdef OTA
  ArduinoOTA.handle();
  #endif

  if(secondGone()){
    updateSwitch();
    #ifdef TELEGRAM
    int numNewMessages = bot->getUpdates(bot->last_message_received + 1);
    while(numNewMessages) {
      //Serial.println(F("Nachricht erhalten!"));
      handleNewMessages(numNewMessages);
      numNewMessages = bot->getUpdates(bot->last_message_received + 1);
    }
    #endif
  }

  if(minuteGone()){
    check4CEST(t);
    printDateAndTime();
    updateStateOfSwitchIfAutomatismIsActive();
    }
}
