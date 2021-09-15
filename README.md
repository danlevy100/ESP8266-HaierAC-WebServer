# Control an air conditioner (AC) with a web app 

- An ESP8266 microcontroller controls an IR LED that activates the AC (Haier in this case).
- The ESP also serves as a webserver, hosting a small web app which allows the user to control the AC. 
- A temperature and humidity sensor is connected. The data are shown in the web app in real time, refreshing every 10 seconds. The ESP also sends the data by HTTP POST to another server every 60 seconds. The server logs it in a SQLite database.

The main piece of code is `ESP8266-HaierAC-WebServer.ino` with the associated `ESP8266-HaierAC-WebServer.h`. These files should be uploaded to the ESP.

The `data` folder has some graphics for the web app and should be uploaded to the ESP using LittleFS.

The `sensor_log_server` contains the PHP files for inserting the data to the database (`insert_db.php`) and displaying it (`livingroom_sensor.php`). These files should be on the dedicated data recording server.
