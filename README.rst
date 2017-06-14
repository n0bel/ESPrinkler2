ESPrinkler2
===========

Arduino/ESP8266 based sprinkler Controller.
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

NOTICE ALPHA version and Incomplete Documentation
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

This is a totally rewritten second version of
https://github.com/n0bel/ESPrinkler Consider that code now totally
obsolete.

This project uses Arduino/ESP8266 to create a web based lawn/garden
sprinkler controller.

Features
--------

-  Web server based (ESP8266WebServer)
-  Responsive UI (desktop/mobile/tablet) written around Jquery,
   Foundation5, and jsoneditor
-  8 Zones (relays)
-  Up to 30 Schedules
-  NTP Time (or set from browser)
-  RTC Option pcf8563
-  OLED Display Option

Requirements
------------

Hardware
~~~~~~~~

-  ESP-12x (nodemcu, hazza, D1 Mini) (probably others)
-  74HC595
-  ST1306 OLED
-  PCF8563
-  Relays

Software
~~~~~~~~

-  Arduino-1.8.3
-  ESP8266/Arduino :Additional Boards Manager URL:
   http://arduino.esp8266.com/stable/package\_esp8266com\_index.json
-  Time 1.5.0 https://github.com/PaulStoffregen/Time
-  SimpleTimer https://github.com/jfturcot/SimpleTimer
   (http://playground.arduino.cc/Code/SimpleTimer)
-  NtpClientLib 2.0.5 https://github.com/gmag11/NtpClient
-  ArduinoJson 5.6.7 https://github.com/bblanchon/ArduinoJson
   (https://bblanchon.github.io/ArduinoJson/)
-  U8G2Lib 2.13.5 https://github.com/olikraus/u8g2
-  orbitalair-arduino-rtc-pcf8563
   https://bitbucket.org/orbitalair/arduino\_rtc\_pcf8563/downloads/
   (https://playground.arduino.cc/Main/RTC-PCF8563)

General Instructions
--------------------

Hardware
~~~~~~~~

*WIP*

Software
~~~~~~~~

Don't forget to restart the Arduino IDE after installing the libraries
and boards.

Set your esp settings.. the board, program method, flash size and spiffs
size.

This uses the SPIFFS file system. So we need to load that in your
esp-12x first. Upload the contents of the data folder with MkSPIFFS Tool
("ESP8266 Sketch Data Upload" in Tools menu in Arduino IDE)

Then compile and upload the .ino.

Setup
~~~~~

When initially powered on, the internet access point will not be setup.
The ESPrinkler will therefore switch to AP mode. It will be come an
access point in itself.

So connect to ESPrinkler2\_XXXXXX with a wifi enabled device (no
password).

Browse to 192.168.4.1

The initial page will let you toggle the relays on/off to test.

But the first thing you'll want to do is click "Configuration", and: \*
set your timezone offset (click set from browser if you wish) \* Set
your access point name and password. You have 2 choices here.. \* If you
want to connect the device to your netork, fill in your access point
SSID and password. \* If you want to leave it as a stand alone access
point all by itself, fill in the second set of SSID and Password

Click Save Configuration, then click Restart.

What is it's IP? If your computer supports mDNS (Anything but windows,
but even on windows it'll be there if you have loaded iTunes), you can
access it with the following url: http://ESPrinkler2.local/ If you don't
have mDNS available, you must find the IP address of the ESPrinkler2
through one of the following methods (or make up your own method)

-  Look at the OLED display (if you're using one)
-  Log into your router and look at the dhcp leases (sometimes called
   dhcp client list) find the entry that shows ESP\_xxxxxx
-  Connect a serial ttl dongle to the ESPrinkler2, set the baud rate to
   74880. During startup, you'll see the IP address shown.
-  Get mDNS on your computer: here's some info for windows:
   http://stackoverflow.com/questions/23624525/standard-mdns-service-on-windows
-  ping from a computer that does handle mDNS -- ping esprinkler2.local
