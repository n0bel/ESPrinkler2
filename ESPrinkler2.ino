/*
 ESPrinkler2 -- Web Server Enabled Sprinkler Controller
 https://github.com/n0bel/ESPrinkler2
 Original Base code is https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266WebServer/examples/FSBrowser/FSBrowser.ino
 Base Code Copyright (c) 2015 Hristo Gochkov. All rights reserved. LGPL
 This Code Copyright (c) 2017 Kevin Uhlir. All rights reserved. LGPL
 This is a rewrite of ESPrinkler https://github.com/n0bel/ESPrinkler but based
 on the Arduino toolchain instead of ExperssIF.  This should make the project
 more accessable to people.
*/

/* Requirements:
  Arduino-1.6.11
  ESP8266/Arduino :Additional Boards Manager URL: http://arduino.esp8266.com/stable/package_esp8266com_index.json
  ESP8266FS plugin, installed in tools https://github.com/esp8266/arduino-esp8266fs-plugin/releases/download/0.2.0/ESP8266FS-0.2.0.zip
  Bounce2 Library, installed in library https://github.com/thomasfredericks/Bounce2/releases/tag/V2.21
  ArduinoJson Library, install in libarry https://github.com/bblanchon/ArduinoJson/releases/tag/v5.6.7
      (the libraries can be installed with the library manager instead)

  Don't forget to restart the Arduino IDE after installing these things.


  Set your esp settings.. the board, program method, flash size and spiffs size.

  This uses the SPIFFS file system.  So we need to load that in your esp-xx first.
  Upload the contents of the data folder with MkSPIFFS Tool ("ESP8266 Sketch Data Upload" in Tools menu in Arduino IDE)

  Then compile and upload the .ino.

  */

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <NtpClientLib.h>
#include <FS.h>
#include <stdarg.h>

#include <ArduinoJson.h>
#include <string.h>
#include <U8g2lib.h>
#include <SimpleTimer.h>
#include <SPI.h>

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);


#define DBG_OUTPUT_PORT Serial

const char* configFile = "/config.json";   // The config file name

// Note that these are the default values if no /config.json exists, or items are missing from it.

char ssid[31] = { "SSID" };               // This is the access point to connect to
char password[31] = { "PASSWORD" };       // And its password
char host[31] = { "ESPrinkler2" };          // The host name for .local (mdns) hostname

int offsetGMT = -18000;       // Local timezone offset in seconds
char offsetGMTstring[10] = { "-18000" };
int relayState = 0;           // The current state of the relayState

bool apMode = false;           // Are we in Acess Point mode?

char timeServer[31] = { "0.pool.ntp.org" };   // the NTP timeServer to use
char getTime[10] = { "02:01" };               // what time to resync with the NTP server
int getHour = 2;                              // parsed hour of above
int getMinute = 1;                            // parsed minute of above
char resetTime[10] = { "00:00" };             // what time to auto reset 00:00=no reset
int resetHour = 0;                            // parsed hour of above
int resetMinute = 0;                          // parsed minute of above


ESP8266WebServer server(80);  // The Web Server
File fsUploadFile;            //holds the current upload when files are uploaded (see edit.htm)
WiFiUDP udp;
SimpleTimer timer;

IPAddress timeServerIP;
unsigned int localPort = 2390; // local port to listen for UDP packets
const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets
// A UDP instance to let us send and receive packets over UDP

time_t timeNow = 0;       // current time is stored here
time_t ms = 0;            // tracking milliseconds
int lastMinute = -1;      // tracking if minute changed
int gettingNtp = false;
int flagRestart = false;

//format bytes
String formatBytes(size_t bytes){
  if (bytes < 1024){
    return String(bytes)+"B";
  } else if(bytes < (1024 * 1024)){
    return String(bytes/1024.0)+"KB";
  } else if(bytes < (1024 * 1024 * 1024)){
    return String(bytes/1024.0/1024.0)+"MB";
  } else {
    return String(bytes/1024.0/1024.0/1024.0)+"GB";
  }
}

String getContentType(String filename){
  if(server.hasArg("download")) return "application/octet-stream";
  else if(filename.endsWith(".htm")) return "text/html";
  else if(filename.endsWith(".html")) return "text/html";
  else if(filename.endsWith(".css")) return "text/css";
  else if(filename.endsWith(".js")) return "application/javascript";
  else if(filename.endsWith(".json")) return "text/json";
  else if(filename.endsWith(".png")) return "image/png";
  else if(filename.endsWith(".gif")) return "image/gif";
  else if(filename.endsWith(".jpg")) return "image/jpeg";
  else if(filename.endsWith(".ico")) return "image/x-icon";
  else if(filename.endsWith(".xml")) return "text/xml";
  else if(filename.endsWith(".pdf")) return "application/x-pdf";
  else if(filename.endsWith(".zip")) return "application/x-zip";
  else if(filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

bool handleFileRead(String path){
  DBG_OUTPUT_PORT.printf(" handleFileRead: %s", path.c_str());
  if(path.endsWith("/")) path += "index.html";

  String contentType = getContentType(path);
  String pathWithGz = path + ".gz";
  if(SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)){
    if(SPIFFS.exists(pathWithGz))
      path += ".gz";
    File file = SPIFFS.open(path, "r");
    size_t sent = server.streamFile(file, contentType);
    file.close();
    return true;
  }
  return false;
}

void handleFileUpload(){
  if(server.uri() != "/edit") return;
  HTTPUpload& upload = server.upload();
  if(upload.status == UPLOAD_FILE_START){
    String filename = upload.filename;
    if(!filename.startsWith("/")) filename = "/"+filename;
    DBG_OUTPUT_PORT.printf("handleFileUpload Name: %s\n", filename.c_str());
    fsUploadFile = SPIFFS.open(filename, "w");
    filename = String();
  } else if(upload.status == UPLOAD_FILE_WRITE){
    //DBG_OUTPUT_PORT.printf("handleFileUpload Data: %s\n",upload.currentSize);
    if(fsUploadFile)
      fsUploadFile.write(upload.buf, upload.currentSize);
  } else if(upload.status == UPLOAD_FILE_END){
    if(fsUploadFile)
      fsUploadFile.close();
    DBG_OUTPUT_PORT.printf("handleFileUpload Size: %s\n", upload.totalSize);
    if (upload.filename == configFile)
    {
      loadConfig();
    }
  }
}

void handleFileDelete(){
  if(server.args() == 0) return server.send(500, "text/plain", "BAD ARGS");
  String path = server.arg(0);
  DBG_OUTPUT_PORT.printf("handleFileDelete: %s\n",path.c_str());
  if(path == "/")
    return server.send(500, "text/plain", "BAD PATH");
  if(!SPIFFS.exists(path))
    return server.send(404, "text/plain", "FileNotFound");
  SPIFFS.remove(path);
  server.send(200, "text/plain", "");
  path = String();
}

void handleFileCreate(){
  if(server.args() == 0)
    return server.send(500, "text/plain", "BAD ARGS");
  String path = server.arg(0);
  DBG_OUTPUT_PORT.printf("handleFileCreate: %s\n",path.c_str());
  if(path == "/")
    return server.send(500, "text/plain", "BAD PATH");
  if(SPIFFS.exists(path))
    return server.send(500, "text/plain", "FILE EXISTS");
  File file = SPIFFS.open(path, "w");
  if(file)
    file.close();
  else
    return server.send(500, "text/plain", "CREATE FAILED");
  server.send(200, "text/plain", "");
  path = String();
}

void handleFileList() {
  if(!server.hasArg("dir")) {server.send(500, "text/plain", "BAD ARGS"); return;}

  String path = server.arg("dir");
  DBG_OUTPUT_PORT.printf(" handleFileList: %s\n",path.c_str());
  Dir dir = SPIFFS.openDir(path);
  path = String();

  String output = "[";
  while(dir.next()){
    File entry = dir.openFile("r");
    if (output != "[") output += ',';
    bool isDir = false;
    output += "{\"type\":\"";
    output += (isDir)?"dir":"file";
    output += "\",\"name\":\"";
    output += String(entry.name()).substring(1);
    output += "\"}";
    entry.close();
  }

  output += "]";
  server.send(200, "text/json", output);
}

void loadConfig()
{
  if(SPIFFS.exists(configFile))
  {
    File file = SPIFFS.open(configFile, "r");
    char json[500];
    memset(json,0,sizeof(json));
    file.readBytes(json,sizeof(json));
    file.close();
    StaticJsonBuffer<500> jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject(json);
    if (!root.success()) {
        DBG_OUTPUT_PORT.printf("json parse of configFile failed.\n");
    }
    else
    {
      if (root.containsKey("ssid")) strncpy(ssid,root["ssid"],30);
      if (root.containsKey("password")) strncpy(password,root["password"],30);
      if (root.containsKey("host")) strncpy(host,root["host"],30);
      if (root.containsKey("timeServer")) strncpy(timeServer,root["timeServer"],30);
      if (root.containsKey("getTime")) strncpy(getTime,root["getTime"],10);
      if (root.containsKey("resetTime")) strncpy(resetTime,root["resetTime"],10);

      if (root.containsKey("offsetGMT")) strncpy(offsetGMTstring,root["offsetGMT"],10);
      offsetGMT = atoi(offsetGMTstring);

      getHour = atoi(getTime);
      if (strchr(getTime,':')) getMinute = atoi(strchr(getTime,':')+1);
      resetHour = atoi(resetTime);
      if (strchr(resetTime,':')) resetMinute = atoi(strchr(resetTime,':')+1);

      DBG_OUTPUT_PORT.printf("Config: host: %s ssid: %s timeServer: %s\n",host,ssid,timeServer);
      DBG_OUTPUT_PORT.printf("getTime: %s %d %d resetTime:%s %d %d offsetGMT:%d\n",
        getTime, getHour, getMinute, resetTime, resetHour, resetMinute, offsetGMT);

    }
  }
  else
  {
    DBG_OUTPUT_PORT.printf("config file: %s not found\n",configFile);
  }
}


void setRelays()
{
  DBG_OUTPUT_PORT.printf("set relays=%02x\n",relayState);
  SPI.transfer(relayState);
  u8g2.setDrawColor(0);
  u8g2.drawBox(0,64-12,128,12);
  for (int i = 0; i < 8; i++)
  {
    if (relayState & (1<<i))
    {
      u8g2.setDrawColor(1);
      u8g2.drawBox(i*16+2,64-12,12,12);
      u8g2.setDrawColor(0);
      u8g2.drawGlyph(i*16+6,64-11,i+'1');
    }
    else
    {
      u8g2.setDrawColor(0);
      u8g2.drawBox(i*16+2,64-12,12,12);
      u8g2.setDrawColor(1);
      u8g2.drawGlyph(i*16+6,64-11,i+'1');
    }

  }
  u8g2.sendBuffer();

}

void getNTP()
{

  if (gettingNtp) return;
  time_t failms = millis();
  time_t ims = millis();
  int tries = 0;
  gettingNtp = true;
  while(gettingNtp)
  {
    tries++;
    if (timeNow > 100) // we have successfully got the time before
      if ((millis() - failms) > 60*1000 ) // 1 minutes
        {
          gettingNtp = false;
          return;  // lets just foreget it
        }
    if ((millis() - failms) > 5*60*1000) // 5 minutes
    {
      ESP.restart();
    }
    if (timeServerIP == INADDR_NONE || (tries % 3) == 1)
    {
      //get a random server from the pool
      DBG_OUTPUT_PORT.printf("Looking up:%s\n",timeServer);

      WiFi.hostByName(timeServer, timeServerIP);
      DBG_OUTPUT_PORT.printf("timeServer IP:%s\n",timeServerIP.toString().c_str());

      if (timeServerIP == INADDR_NONE)
      {
        DBG_OUTPUT_PORT.printf("bad IP, try again\n");
        delay(1000);
        continue;
      }
    }
    DBG_OUTPUT_PORT.printf("sending NTP packet...\n");
    // set all bytes in the buffer to 0
    memset(packetBuffer, 0, NTP_PACKET_SIZE);
    // Initialize values needed to form NTP request
    // (see URL above for details on the packets)
    packetBuffer[0] = 0b11100011;   // LI, Version, Mode
    packetBuffer[1] = 0;     // Stratum, or type of clock
    packetBuffer[2] = 6;     // Polling Interval
    packetBuffer[3] = 0xEC;  // Peer Clock Precision
    // 8 bytes of zero for Root Delay & Root Dispersion
    packetBuffer[12]  = 49;
    packetBuffer[13]  = 0x4E;
    packetBuffer[14]  = 49;
    packetBuffer[15]  = 52;

    // all NTP fields have been given values, now
    // you can send a packet requesting a timestamp:
    udp.beginPacket(timeServerIP, 123); //NTP requests are to port 123
    udp.write(packetBuffer, NTP_PACKET_SIZE);
    udp.endPacket();

    ims = millis();
    while(gettingNtp)
    {
      if ((millis() - ims) > 5000) break; // if > 15 seconds waiting for packet, send packet again (break into outer loop)
      // wait to see if a reply is available
      delay(1000);

      int cb = udp.parsePacket();
      if (!cb) {
        DBG_OUTPUT_PORT.printf(".");
      }
      else {
        DBG_OUTPUT_PORT.printf("packet received, length=%d\n",cb);
        // We've received a packet, read the data from it
        udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer

        //the timestamp starts at byte 40 of the received packet and is four bytes,
        // or two words, long. First, esxtract the two words:

        unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
        unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
        // combine the four bytes (two words) into a long integer
        // this is NTP time (seconds since Jan 1 1900):
        unsigned long secsSince1900 = highWord << 16 | lowWord;
        DBG_OUTPUT_PORT.printf("Seconds since Jan 1 1900 = %d\n",secsSince1900);

        // now convert NTP time into everyday time:
        // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
        const unsigned long seventyYears = 2208988800UL;
        // subtract seventy years:
        unsigned long epoch = secsSince1900 - seventyYears;
        // print Unix time:
        DBG_OUTPUT_PORT.printf("Unix time = %d\n",epoch);


        // print the hour, minute and second:
        DBG_OUTPUT_PORT.printf("The UTC time is %d:%02d:%02d\n",       // UTC is the time at Greenwich Meridian (GMT)
          (epoch  % 86400L) / 3600,  // print the hour (86400 equals secs per day)
          (epoch  % 3600) / 60, // print the minute (3600 equals secs per minute)
          epoch % 60); // print the second
        timeNow = epoch;
        gettingNtp = false;
      }
    }
  }


}

void displayStatus(int line, const char *fmt, ...)
{

  int count;
  char *buffer;
  va_list Arglist;

  count = 0;
  buffer = (char*)malloc(128);
  if(buffer == NULL)return;
  va_start(Arglist,fmt);
  vsnprintf(buffer,128,fmt,Arglist);

  u8g2.setDrawColor(0);
  u8g2.drawBox(0,line*12,128,12);
  u8g2.setDrawColor(1);
  u8g2.drawStr(0,line*12,buffer);
  u8g2.sendBuffer();
  free(buffer);

}

void clearTimedFunc(int *id, const char *name)
{
  DBG_OUTPUT_PORT.printf("clearTimedFunc %s\n",name);
  if (*id > -1)
  {
    DBG_OUTPUT_PORT.printf("clearingTimer id=%d\n",*id);
    timer.disable(*id);
    timer.deleteTimer(*id);
  }
  *id = -1;
}
int setTimedFunc(bool repeat, int *id, long t, void (*func)(), const char *name)
{
  if (*id > -1)
  {
    DBG_OUTPUT_PORT.printf("clearingTimer id=%d\n",*id);
    timer.disable(*id);
    timer.deleteTimer(*id);
  }
  *id = -1;
  if (t > 0)
  {
    if (repeat)
    {
      *id = timer.setInterval(t,func);
    }
    else
    {
      *id = timer.setTimeout(t,func);
    }
  }
  DBG_OUTPUT_PORT.printf("setTimedFunc %d %d %s %08x id=%d\n",repeat,t,name,func,*id);
  for (int i = 0; i < timer.MAX_TIMERS; i++) {
    if (timer.callbacks[i])
    {
      DBG_OUTPUT_PORT.printf("%d cb:%08x num:%d numMax:%d\n",
        i, timer.callbacks[i], timer.numRuns[i], timer.maxNumRuns[i]);
    }
  }
  return(*id);
}

// LED CONTROL
int blinkerTimerId = -1;
#define setBlinker(t) setTimedFunc(true,&blinkerTimerId,t,blinker,"blinker")
void blinker()
{
  static bool onoff = false;
  onoff = !onoff;
  digitalWrite(BUILTIN_LED,onoff?HIGH:LOW);

}

// mdns
bool mdnsStarted = false;
void startMDNS()
{
  if (mdnsStarted) return;
  mdnsStarted = true;
  MDNS.begin(host);
  DBG_OUTPUT_PORT.printf("MDNS Starting\nOpen http://%s.local or http://%s/\n",
    host, apMode? WiFi.softAPIP().toString().c_str() : WiFi.localIP().toString().c_str());
}

// NTP Enable/Disabled
bool ntpStarted = false;
void startNTP()
{
    if (ntpStarted) return;
    ntpStarted = true;
    // NTP init
    if (!apMode)    // if we're in AP Mode we have no internet, so no NTP
    {
      DBG_OUTPUT_PORT.printf("Starting UDP for NTP\n");
      udp.begin(localPort);
      DBG_OUTPUT_PORT.printf("Local port: %s\n",udp.localPort());

      delay(1000);

      getNTP();

    }
}
void stopNTP()
{

}

// WIFI STATUS CHANGES
int apModeTimerId = -1;
int staModeTimerId = -1;
#define setApModeTimeout(t) setTimedFunc(false,&apModeTimerId,t,apModeTimeout,"apModeTimeout")
#define setStaModeTimeout(t) setTimedFunc(false,&staModeTimerId,t,staModeTimeout,"staModeTimeout")
void apModeTimeout()
{
  DBG_OUTPUT_PORT.printf("apModeTimeout\n");
  apModeTimerId = -1;
  setApMode(true);
  setBlinker(100);
  setStaModeTimeout(600000);
}

void staModeTimeout()
{
  DBG_OUTPUT_PORT.printf("staModeTimeout\n");
  staModeTimerId = -1;
  setApMode(false);
  setBlinker(50);
  setApModeTimeout(60000);
}

void setApMode(bool mode)
{
    if (apMode == mode) return;
    apMode = mode;
    if (apMode)
    {
      DBG_OUTPUT_PORT.printf("going to AP mode\n");
      WiFi.disconnect();
      delay(500);
      WiFi.mode(WIFI_AP);
      apMode = true;
      uint8_t mac[6];
      delay(500);
      WiFi.softAPmacAddress(mac);
      delay(500);
      char assid[31];
      sprintf(assid,"%s_%02x%02x%02x",host,mac[3],mac[4],mac[5]);   // making a nice unique SSID
      DBG_OUTPUT_PORT.printf("SoftAP ssid:%s\n",assid);
      WiFi.softAP(assid);
      DBG_OUTPUT_PORT.printf("AP mode. IP address: %s\n",WiFi.softAPIP().toString().c_str());
      displayStatus(0, "AP:%s",WiFi.softAPIP().toString().c_str());
    }
    else
    {
      DBG_OUTPUT_PORT.printf("going to STA mode %s\n", ssid);
      displayStatus(0, "Try: %s", ssid);
      WiFi.mode(WIFI_STA);
      delay(500);
      WiFi.begin(ssid, password);
    }
}

WiFiEventHandler onSTAGotIPHandler;
void onSTAGotIP(WiFiEventStationModeGotIP ipInfo) {
  if (apMode) return;
  DBG_OUTPUT_PORT.printf("Got IP: %s\n", ipInfo.ip.toString().c_str());
  displayStatus(0, "STA:%s %s %d",WiFi.localIP().toString().c_str(),ssid,WiFi.RSSI());
  setApModeTimeout(0);
  setBlinker(500);
  timer.setTimeout(1000,startMDNS);
  timer.setTimeout(2000,startNTP);
}

WiFiEventHandler onSTADisconnectedHandler;
void onSTADisconnected(WiFiEventStationModeDisconnected event_info) {
  if (apMode) return;
  Serial.printf("Disconnected from SSID: %s Reason: %d\n", event_info.ssid.c_str(),event_info.reason);
  displayStatus(0, "DIS:%s %d",event_info.ssid.c_str(),event_info.reason);
  setBlinker(50);
  if (apModeTimerId < 0) setApModeTimeout(600000);
  timer.setTimeout(20,stopNTP);
}

void setup(void){

  pinMode(BUILTIN_LED, OUTPUT);
  setBlinker(50);

  SPI.setDataMode(SPI_MODE0);
  SPI.setBitOrder(MSBFIRST);
  SPI.setClockDivider(SPI_CLOCK_DIV16);
  SPI.setHwCs(true);
  SPI.begin();
  SPI.transfer(relayState);

  DBG_OUTPUT_PORT.begin(74880);
  DBG_OUTPUT_PORT.setDebugOutput(true);
  DBG_OUTPUT_PORT.printf("\n");


  u8g2.begin();
  u8g2.setFont(u8g2_font_helvR08_tf);
  u8g2.setFontRefHeightExtendedText();
  u8g2.setDrawColor(1);
  u8g2.setFontPosTop();
  u8g2.setFontDirection(0);
  u8g2.clearBuffer();
  displayStatus(0, "Start...");


  SPIFFS.begin();
  {
    Dir dir = SPIFFS.openDir("/");
    while (dir.next()) {
      String fileName = dir.fileName();
      size_t fileSize = dir.fileSize();
      DBG_OUTPUT_PORT.printf("FS File: %s, size: %s\n", fileName.c_str(), formatBytes(fileSize).c_str());
    }
    DBG_OUTPUT_PORT.printf("\n");
  }

  loadConfig();

  setRelays();

  //WIFI INIT

  onSTAGotIPHandler = WiFi.onStationModeGotIP(onSTAGotIP);
  onSTADisconnectedHandler = WiFi.onStationModeDisconnected(onSTADisconnected);
  WiFi.persistent(false);
  apMode=true; // forced mode change
  setApMode(false);
  setApModeTimeout(60000);


//SERVER INIT
  //list directory
  server.on("/list", HTTP_GET, handleFileList);
  //load editor
  server.on("/edit", HTTP_GET, [](){
    if(!handleFileRead("/edit.html")) server.send(404, "text/plain", "FileNotFound");
  });
  //create file
  server.on("/edit", HTTP_PUT, handleFileCreate);
  //delete file
  server.on("/edit", HTTP_DELETE, handleFileDelete);
  //first callback is called after the request has ended with all parsed arguments
  //second callback handles file uploads at that location
  server.on("/edit", HTTP_POST, [](){ server.send(200, "text/plain", ""); }, handleFileUpload);

  //called when the url is not defined here
  //use it to load content from SPIFFS
  server.onNotFound([](){
    if(!handleFileRead(server.uri()))
      server.send(404, "text/plain", "FileNotFound");
  });

  server.on("/status", HTTP_GET, [](){
    String json = "{";
    for(int i = 0; i < 8; i++)
    {
      json += String( "\"zone") + String(i) + String(relayState&(1<<i) ? "\":\"on\"," : "\":\"off\",");
    }
    json += String("\"time\":")+String(timeNow);
    json += "}";
    server.send(200, "text/json", json);
    DBG_OUTPUT_PORT.printf("status %s\n",json.c_str());
    json = String();
  });

  server.on("/toggle", HTTP_GET, [](){
    int i = 0;
    if (server.args() > 0)
      i = atoi(server.arg("zone").c_str());
    relayState ^= (1<<i);
    server.send(200, "text/text", "OK");
    DBG_OUTPUT_PORT.printf("toggle %d = %s\n",i,relayState&(1<<i)?"on":"off");
    setRelays();
  });
  server.on("/restart", HTTP_GET, [](){
    server.send(200, "text/text", "Restarting.... Wait a minute or so and then refresh.");
    delay(2000);
    ESP.restart();
  });

  server.begin();
  DBG_OUTPUT_PORT.printf("HTTP server started\n");

}


void loop(void)
{

// deal with timer (SimpleTimer)
  timer.run();
// deal with http server.
  server.handleClient();

}
