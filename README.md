# Remotely control an air conditioner (AC) with an ESP8266

## How it works
- An ESP8266 microcontroller controls an IR LED that activates the AC (Haier in this case).
- The ESP also serves as a webserver, hosting a small web app through which the user controls the AC.
- Basic HTTP authentication is used (username, password).
- A temperature and humidity sensor (AHT21) is additionally connected. The data is shown in the web app in real time, refreshing every 10 seconds. The ESP also sends the data by HTTP POST to another server every 60 seconds. The server logs it in a SQLite database and can also display it.

## Files
The main piece of code is `ESP8266-HaierAC-WebServer.ino` (with the associated `.h` file). These files should be directly uploaded to the ESP.

The `data` folder contains the web app files and should be uploaded to the ESP using LittleFS. The heart is `ui.js` and the file served is `ui.html`. 

The `sensor_log_server` folder contains the PHP files for inserting the data to the database (`insert_db.php`) and displaying it (`livingroom_sensor.php`). These files should be on the dedicated data recording/displaying server.
