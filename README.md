# ESP8266-HaierAC-WebServer
 
Control an air conditioner (AC) with a web app!

- An ESP8266 microcontroller controls an IR LED that activates the AC (Haier in this case).
- The ESP also hosts a small web app which allows the user to control the AC. 
- A temperature and humidity sensor is connected. The data are shown on the web app and are also sent by the ESP to another server every 60 seconds. The server saves it in a sqlite database.
