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
#include <FS.h>
#include <stdarg.h>

#include <ArduinoJson.h>
#include <Servo.h>
#include <Bounce2.h>
#include <string.h>
#include <U8g2lib.h>

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);


#define DBG_OUTPUT_PORT Serial

const char* configFile = "/config.json";   // The config file name

// Note that these are the default values if no /config.json exists, or items are missing from it.

char ssid[31] = { "SSID" };               // This is the access point to connect to
char password[31] = { "PASSWORD" };       // And its password
char host[31] = { "ESPrinkler2" };          // The host name for .local (mdns) hostname

int offsetGMT = -18000;       // Local timezone offset in seconds
char offsetGMTstring[10] = { "-18000" };
int servo1State = true;       // current servo state (true = latched)
int servo1Latched = 10;       // The servo value to set when latched
int servo1Unlatched = 90;     // The servo value to set when unlatched
int servo2State = true;       // current servo state (true = latched)
int servo2Latched = 10;       // The servo value to set when latched
int servo2Unlatched = 90;     // The servo value to set when unlatched
char servo1Time[10];          // Time string (HH:MM 24hr time) when to unlatch (feed the pet)
int servo1hour = 6;           // The time string hour is parsed to this variable
int servo1Minute = 0;         // The time string minute is parsed to this variable
char servo2Time[10];          // Time string (HH:MM 24hr time) when to unlatch (feed the pet)
int servo2hour = 18;          // The time string hour is parsed to this variable
int servo2Minute = 0;         // The time string minute is parsed to this variable

int apMode = false;           // Are we in Acess Point mode?

char timeServer[31] = { "0.pool.ntp.org" };   // the NTP timeServer to use
char getTime[10] = { "02:01" };               // what time to resync with the NTP server
int getHour = 2;                              // parsed hour of above
int getMinute = 1;                            // parsed minute of above
char resetTime[10] = { "00:00" };             // what time to auto reset (note the servos may unlatch) 00:00=no reset
int resetHour = 0;                            // parsed hour of above
int resetMinute = 0;                          // parsed minute of above


ESP8266WebServer server(80);  // The Web Server
File fsUploadFile;            //holds the current upload when files are uploaded (see edit.htm)

WiFiUDP udp;

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
  DBG_OUTPUT_PORT.print(timeNow);
  DBG_OUTPUT_PORT.println(" handleFileRead: " + path);
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
    DBG_OUTPUT_PORT.print("handleFileUpload Name: "); DBG_OUTPUT_PORT.println(filename);
    fsUploadFile = SPIFFS.open(filename, "w");
    filename = String();
  } else if(upload.status == UPLOAD_FILE_WRITE){
    //DBG_OUTPUT_PORT.print("handleFileUpload Data: "); DBG_OUTPUT_PORT.println(upload.currentSize);
    if(fsUploadFile)
      fsUploadFile.write(upload.buf, upload.currentSize);
  } else if(upload.status == UPLOAD_FILE_END){
    if(fsUploadFile)
      fsUploadFile.close();
    DBG_OUTPUT_PORT.print("handleFileUpload Size: "); DBG_OUTPUT_PORT.println(upload.totalSize);
    if (upload.filename == configFile)
    {
      loadConfig();
      setServos();
    }
  }
}

void handleFileDelete(){
  if(server.args() == 0) return server.send(500, "text/plain", "BAD ARGS");
  String path = server.arg(0);
  DBG_OUTPUT_PORT.println("handleFileDelete: " + path);
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
  DBG_OUTPUT_PORT.println("handleFileCreate: " + path);
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
  DBG_OUTPUT_PORT.print(timeNow);
  DBG_OUTPUT_PORT.println(" handleFileList: " + path);
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
        DBG_OUTPUT_PORT.println("json parse of configFile failed.");
    }
    else
    {
      if (root.containsKey("ssid")) strncpy(ssid,root["ssid"],30);
      if (root.containsKey("password")) strncpy(password,root["password"],30);
      if (root.containsKey("host")) strncpy(host,root["host"],30);
      if (root.containsKey("timeServer")) strncpy(timeServer,root["timeServer"],30);
      if (root.containsKey("servo1Latched")) servo1Latched = root["servo1Latched"];
      if (root.containsKey("servo1Unlatched")) servo1Unlatched = root["servo1Unlatched"];
      if (root.containsKey("servo2Latched")) servo2Latched = root["servo2Latched"];
      if (root.containsKey("servo2Unlatched")) servo2Unlatched = root["servo2Unlatched"];
      if (root.containsKey("servo1Time")) strncpy(servo1Time,root["servo1Time"],10);
      if (root.containsKey("servo2Time")) strncpy(servo2Time,root["servo2Time"],10);
      if (root.containsKey("getTime")) strncpy(getTime,root["getTime"],10);
      if (root.containsKey("resetTime")) strncpy(resetTime,root["resetTime"],10);

      if (root.containsKey("offsetGMT")) strncpy(offsetGMTstring,root["offsetGMT"],10);
      offsetGMT = atoi(offsetGMTstring);

      servo1hour = atoi(servo1Time);
      if (strchr(servo1Time,':')) servo1Minute = atoi(strchr(servo1Time,':')+1);
      servo2hour = atoi(servo2Time);
      if (strchr(servo2Time,':')) servo2Minute = atoi(strchr(servo2Time,':')+1);

      getHour = atoi(getTime);
      if (strchr(getTime,':')) getMinute = atoi(strchr(getTime,':')+1);
      resetHour = atoi(resetTime);
      if (strchr(resetTime,':')) resetMinute = atoi(strchr(resetTime,':')+1);

      DBG_OUTPUT_PORT.printf("Config: host: %s ssid: %s timeServer: %s\n",host,ssid,timeServer);
      DBG_OUTPUT_PORT.printf("servo1Time: %s %d %d servo2Time:%s %d %d servo1:%d %d servo2:%d %d\n",
        servo1Time, servo1hour, servo1Minute, servo2Time, servo2hour, servo2Minute, servo1Latched, servo1Unlatched, servo2Latched, servo2Unlatched);
      DBG_OUTPUT_PORT.printf("getTime: %s %d %d resetTime:%s %d %d offsetGMT:%d\n",
        getTime, getHour, getMinute, resetTime, resetHour, resetMinute, offsetGMT);

    }
  }
  else
  {
    DBG_OUTPUT_PORT.printf("config file: %s not found\n",configFile);
  }
}


void setServos()
{
  int v1 = servo1State?servo1Latched:servo1Unlatched;
  int v2 = servo2State?servo2Latched:servo2Unlatched;
//  servo1.write(v1);
//  servo2.write(v2);
  DBG_OUTPUT_PORT.printf("set servo1=%d servo2=%d\n",v1,v2);
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
      DBG_OUTPUT_PORT.print("\nLooking up:");
      DBG_OUTPUT_PORT.println(timeServer);

      WiFi.hostByName(timeServer, timeServerIP);
      DBG_OUTPUT_PORT.print("timeServer IP:");
      DBG_OUTPUT_PORT.println(timeServerIP);

      if (timeServerIP == INADDR_NONE)
      {
        DBG_OUTPUT_PORT.println("bad IP, try again");
        delay(1000);
        continue;
      }
    }
    DBG_OUTPUT_PORT.println("sending NTP packet...");
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
        DBG_OUTPUT_PORT.print(".");
      }
      else {
        DBG_OUTPUT_PORT.print("packet received, length=");
        DBG_OUTPUT_PORT.println(cb);
        // We've received a packet, read the data from it
        udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer

        //the timestamp starts at byte 40 of the received packet and is four bytes,
        // or two words, long. First, esxtract the two words:

        unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
        unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
        // combine the four bytes (two words) into a long integer
        // this is NTP time (seconds since Jan 1 1900):
        unsigned long secsSince1900 = highWord << 16 | lowWord;
        DBG_OUTPUT_PORT.print("Seconds since Jan 1 1900 = " );
        DBG_OUTPUT_PORT.println(secsSince1900);

        // now convert NTP time into everyday time:
        DBG_OUTPUT_PORT.print("Unix time = ");
        // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
        const unsigned long seventyYears = 2208988800UL;
        // subtract seventy years:
        unsigned long epoch = secsSince1900 - seventyYears;
        // print Unix time:
        DBG_OUTPUT_PORT.println(epoch);


        // print the hour, minute and second:
        DBG_OUTPUT_PORT.print("The UTC time is ");       // UTC is the time at Greenwich Meridian (GMT)
        DBG_OUTPUT_PORT.print((epoch  % 86400L) / 3600); // print the hour (86400 equals secs per day)
        DBG_OUTPUT_PORT.print(':');
        if ( ((epoch % 3600) / 60) < 10 ) {
          // In the first 10 minutes of each hour, we'll want a leading '0'
          DBG_OUTPUT_PORT.print('0');
        }
        DBG_OUTPUT_PORT.print((epoch  % 3600) / 60); // print the minute (3600 equals secs per minute)
        DBG_OUTPUT_PORT.print(':');
        if ( (epoch % 60) < 10 ) {
          // In the first 10 seconds of each minute, we'll want a leading '0'
          DBG_OUTPUT_PORT.print('0');
        }
        DBG_OUTPUT_PORT.println(epoch % 60); // print the second
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

void setup(void){


  apMode = false;

  pinMode(BUILTIN_LED, OUTPUT);

  DBG_OUTPUT_PORT.begin(74880);
  DBG_OUTPUT_PORT.print("\n");
  DBG_OUTPUT_PORT.setDebugOutput(true);

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

  //WIFI INIT

  WiFi.mode(WIFI_AP_STA);

  time_t wifims = millis();

  DBG_OUTPUT_PORT.printf("Connecting to %s\n", ssid);
  displayStatus(0, "Conn %s", ssid);

  WiFi.begin(ssid, password);

  bool ledtoggle = false;
  while (WiFi.status() != WL_CONNECTED) {
    ledtoggle = !ledtoggle;
    digitalWrite(BUILTIN_LED, ledtoggle?HIGH:LOW);
    delay(500);
    DBG_OUTPUT_PORT.print(".");
    displayStatus(0, "Conn %s %d", ssid, (millis() - wifims) / 1000);
    if ((millis() - wifims) > 60 * 1000) // 60 seconds of no wifi connect
    {
      break;
    }
  }
  if (WiFi.status() != WL_CONNECTED)
  {
    DBG_OUTPUT_PORT.println("\ngoing to AP mode ");
    delay(500);
    WiFi.mode(WIFI_AP);
    delay(500);
    apMode = true;
    uint8_t mac[6];
    delay(500);
    WiFi.softAPmacAddress(mac);
    delay(500);
    sprintf(ssid,"ESPrinkler2_%02x%02x%02x",mac[3],mac[4],mac[5]);   // making a nice unique SSID
    DBG_OUTPUT_PORT.print("SoftAP ssid:");
    DBG_OUTPUT_PORT.println(ssid);
    WiFi.softAP(ssid);
    DBG_OUTPUT_PORT.println("");
    DBG_OUTPUT_PORT.print("AP mode. IP address: ");
    DBG_OUTPUT_PORT.println(WiFi.softAPIP());
    displayStatus(0, "AP mode Set");
    displayStatus(1, "%s",ssid);
    displayStatus(2, "IP:%s",WiFi.softAPIP().toString().c_str());
  }
  else
  {
    digitalWrite(BUILTIN_LED, ledtoggle?HIGH:LOW);
    DBG_OUTPUT_PORT.println("");
    DBG_OUTPUT_PORT.print("Connected! IP address: ");
    DBG_OUTPUT_PORT.println(WiFi.localIP());
    displayStatus(0, "%s %s",WiFi.localIP().toString().c_str(),ssid);
  }

  MDNS.begin(host);
  DBG_OUTPUT_PORT.print("Open http://");
  DBG_OUTPUT_PORT.print(host);
  DBG_OUTPUT_PORT.print(".local/ or http://");
  if (apMode)
  {
    DBG_OUTPUT_PORT.print(WiFi.softAPIP());
  }
  else
  {
    DBG_OUTPUT_PORT.print(WiFi.localIP());
  }
  DBG_OUTPUT_PORT.println("/");

  // NTP init
  if (!apMode)    // if we're in AP Mode we have no internet, so no NTP
  {
    DBG_OUTPUT_PORT.println("Starting UDP for NTP");
    udp.begin(localPort);
    DBG_OUTPUT_PORT.print("Local port: ");
    DBG_OUTPUT_PORT.println(udp.localPort());

    delay(1000);

    getNTP();

  }

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
    json += String(  "\"servo1\":\"")+String(servo1State?"latched":"unlatched")+String("\"");
    json += String(", \"servo2\":\"")+String(servo2State?"latched":"unlatched")+String("\"");
    json += String(", \"time\":")+String(timeNow);
    json += "}";
    server.send(200, "text/json", json);
    DBG_OUTPUT_PORT.print("status ");
    DBG_OUTPUT_PORT.println(json);
    json = String();
  });

  server.on("/toggle1", HTTP_GET, [](){
    servo1State = ! servo1State;
    server.send(200, "text/text", "OK");
    DBG_OUTPUT_PORT.printf("toggle servo1 = %d\n",servo1State);
    setServos();
  });
  server.on("/toggle2", HTTP_GET, [](){
    servo2State = ! servo2State;
    server.send(200, "text/text", "OK");
    DBG_OUTPUT_PORT.printf("toggle servo2 = %d\n",servo2State);
    setServos();
  });
  server.on("/restart", HTTP_GET, [](){
    server.send(200, "text/text", apMode?"Stopping AP, Restarting... to connect to WiFi. Use your browser on your network to reconnect in a minute":"Restarting.... Wait a minute or so and then refresh.");
    delay(2000);
    ESP.restart();
  });

  server.begin();
  DBG_OUTPUT_PORT.println("HTTP server started");

}

void loop(void){

// deal with http server.
  server.handleClient();


// keep track of the time
  if (!apMode && ms != millis())
  {
    ms = millis();
    if ( (ms % 1000) == 0)
    {
      timeNow++;
      time_t t = timeNow + offsetGMT;
      int hour = (t % 86400) / 3600;
      int minute = (t % 3600) / 60;
      if (lastMinute != minute)  // time to check for things to do?
      {
        DBG_OUTPUT_PORT.printf("%d:%02d\n",hour,minute);
        lastMinute = minute;
        if (flagRestart)        // if we flagged a reset
        {
          flagRestart = false;
          ESP.restart();
        }
        if (servo1State)        // is the servo latched
        {
          if (servo1hour == hour && servo1Minute == minute) // time to unlatch?
          {
            servo1State = false;
            setServos();
          }
        }
        if (servo2State)        // is the servo latched
        {
          if (servo2hour == hour && servo2Minute == minute) // time to unlatch?
          {
            servo2State = false;
            setServos();
          }
        }
        if (getHour == hour && getMinute == minute)   // time to resync the time?
        {
          if (!gettingNtp && !apMode) getNTP();
        }
        if (resetHour == hour && resetMinute == minute && resetHour != 0 && resetMinute != 0) // time to reset?
        {
          flagRestart = true;
        }
      }
    }
  }
}
