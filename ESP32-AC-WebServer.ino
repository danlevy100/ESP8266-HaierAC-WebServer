/* Dan Levy, September 2022.
 *  AC remote web server with ESP-32.
 */

/* Based on:

  Copyright 2019 Motea Marius

  This example code will create a webserver that will provide basic control to AC units using the web application
  build with javascript/css. User config zone need to be updated if a different class than Collix need to be used.
  Javasctipt file may also require minor changes as in current version it will not allow to set fan speed if Auto mode
  is selected (required for Coolix).

*/
#include "ESP32-AC-WebServer.h"

#include <ESPmDNS.h>
#include <WebServer.h>
#include <WiFi.h>
#include <Update.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_AHTX0.h>
#include <Wire.h>
#include <SPI.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <arduino-timer.h>

auto timer = timer_create_default();

HTTPClient http;
WiFiClientSecure client;

//// ###### User configuration space for AC library classes ##########

//#include <ir_Haier.h>  //  replace library based on your AC unit model, check https://github.com/crankyoldgit/IRremoteESP8266
#include <ir_Goodweather.h>  //  replace library based on your AC unit model, check https://github.com/crankyoldgit/IRremoteESP8266

#define AUTO_MODE kGoodweatherAuto
#define COOL_MODE kGoodweatherCool
#define DRY_MODE kGoodweatherDry
#define HEAT_MODE kGoodweatherHeat
#define FAN_MODE kGoodweatherFan

#define FAN_AUTO kGoodweatherFanAuto
#define FAN_MIN kGoodweatherFanLow
#define FAN_MED kGoodweatherFanMed
#define FAN_HI kGoodweatherFanHigh

// GPIO pin to use for IR blaster.
const uint16_t kIrLed = 4;
// Library initialization, change it according to the imported library file.

//IRHaierACYRW02 ac(kIrLed);
IRGoodweatherAc ac(kIrLed);

Adafruit_AHTX0 aht;

String apiKeyValue = "tPmAT5Ab3j7F9";

String sensorName = "AHT21";
String sensorLocation = "LivingRoom";

/// ##### End user configuration ######

struct state {
  uint8_t temperature = 22, fan = 0, operation = 1;
  bool powerStatus;
};

File fsUploadFile;

// core

state acState;

// settings
char deviceName[] = "AC Remote Control";

const char* ssid = "Dan";
const char* password = "0528561582";

const char* www_username = "Shachar";
const char* www_password = "yafa";

// php file location on server for inserting data to database
//const char* serverName = "https://192.168.1.200/insert_db.php";
const char* serverName = "https://danhome.ddns.net/insert_db.php";
// SSL SHA1 fingerprint 
const char* fingerprint = "64 60 19 17 91 68 C3 48 19 32 CF 7D 78 C0 63 C3 C3 E1 7D 91";

WebServer server(80);

float delta_T = 4.0; // temperature offset to be corrected due to heating of sensor

float temp_correct(float temp_original) {
  // correct temperature offset due to heating of sensor  
  float temp_correct = temp_original - delta_T;
  return temp_correct;
}

float humidity_correct(float temp_original, float humidity_original) {
  // correct relative humidity after correction of temperature offset
  // Taken from "The relationship between relative humidity and the dewpoint temperature in moist air"
  // - A paper by Mark G. Lawrence.
  const float L = 2441700; // Heat of vaporization of water at 25 C, [J/kg]
  const float Rw = 461.5; // The gas constant for water vapor, [J/kg/K] 
  const float T = temp_original + 273.15; // Convert to Kelvin

  const float humidity_correct = humidity_original*(1+L/Rw*delta_T/(T*T));
  // This is an approximation. The non-approximated formula is:
  // RH_2 = RH_1 * exp(-L/Rw(1/T_1-1/T_2))

  return humidity_correct;
}

float dewpoint(float temp_celsius, float RH) {
  const float L = 2441700; // Heat of vaporization of water at 25 C, [J/kg]
  const float Rw = 461.5; // The gas constant for water vapor, [J/kg/K] 
  const float T = temp_celsius + 273.15; // Convert to Kelvin  
  
  const float T_d = T/(1-T*log(RH/100)*Rw/L) - 273.15; 

  return T_d;
}

bool handleFileRead(String path) {
  //  send the right file to the client (if it exists)
  // Serial.println("handleFileRead: " + path);
  if (path.endsWith("/")) path += "index.html";
  // If a folder is requested, send the index file
  String contentType = getContentType(path);
  // Get the MIME type
  String pathWithGz = path + ".gz";
  if (FILESYSTEM.exists(pathWithGz) || FILESYSTEM.exists(path)) {
    // If the file exists, either as a compressed archive, or normal
    // If there's a compressed version available
    if (FILESYSTEM.exists(pathWithGz))
      path += ".gz";  // Use the compressed verion
    File file = FILESYSTEM.open(path, "r");
    //  Open the file
    server.streamFile(file, contentType);
    //  Send it to the client
    file.close();
    // Close the file again
    // Serial.println(String("\tSent file: ") + path);
    return true;
  }
  // Serial.println(String("\tFile Not Found: ") + path);
  // If the file doesn't exist, return false
  return false;
}

String getContentType(String filename) {
  // convert the file extension to the MIME type
  if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  else if (filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

void handleFileUpload() {  // upload a new file to the FILESYSTEM
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    String filename = upload.filename;
    if (!filename.startsWith("/")) filename = "/" + filename;
    // Serial.print("handleFileUpload Name: "); //Serial.println(filename);
    fsUploadFile = FILESYSTEM.open(filename, "w");
    // Open the file for writing in FILESYSTEM (create if it doesn't exist)
    filename = String();
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (fsUploadFile)
      fsUploadFile.write(upload.buf, upload.currentSize);
      // Write the received bytes to the file
  } else if (upload.status == UPLOAD_FILE_END) {
    if (fsUploadFile) {
      // If the file was successfully created
      fsUploadFile.close();
      // Close the file again
      // Serial.print("handleFileUpload Size: ");
      // Serial.println(upload.totalSize);
      server.sendHeader("Location", "/success.html");
      // Redirect the client to the success page
      server.send(303);
    } else {
      server.send(500, "text/plain", "500: couldn't create file");
    }
  }
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

bool postToDB(void *) {  
  
  client.setInsecure();  
  http.begin(client, serverName);

  // if the client requests the temperature
  sensors_event_t humidity, temperature;
  aht.getEvent(&humidity, &temperature);// populate temp and humidity objects with fresh data       
    
  const float T = temp_correct(temperature.temperature); //correct for offset
  // correct relative humidity for temperature offset
  const float RH = humidity_correct(temperature.temperature, humidity.relative_humidity);
  // calculate dew point
  const float T_dewpoint = dewpoint(T, RH);

  String temp_str = String(T);
  String humid_str = String(RH);
  String dewpoint_str = String(T_dewpoint);
  
  // Specify content-type header
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  
  // Prepare your HTTP POST request data
  String httpRequestData = "api_key=" + apiKeyValue + "&sensor=" + sensorName
                        + "&location=" + sensorLocation + "&value1=" + temp_str
                        + "&value2=" + humid_str + "&value3=" + "";
  Serial.print("httpRequestData: ");
  Serial.println(httpRequestData);
    
  // You can comment the httpRequestData variable above
  // then, use the httpRequestData variable below (for testing purposes without the BME280 sensor)
  //String httpRequestData = "api_key=tPmAT5Ab3j7F9&sensor=BME280&location=Office&value1=24.75&value2=49.54&value3=1005.14";

  Serial.println(httpRequestData);

  // Send HTTP POST request
  int httpResponseCode = http.POST(httpRequestData);
   
  // If you need an HTTP request with a content type: text/plain
  //http.addHeader("Content-Type", "text/plain");
  //int httpResponseCode = http.POST("Hello, World!");
  
  // If you need an HTTP request with a content type: application/json, use the following:
  //http.addHeader("Content-Type", "application/json");
  //int httpResponseCode = http.POST("{\"value1\":\"19\",\"value2\":\"67\",\"value3\":\"78\"}");
      
  if (httpResponseCode>0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
  }
  else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  // Free resources
  http.end();

  return true; // keep timer active? true
}

void WiFiStationConnected(WiFiEvent_t event, WiFiEventInfo_t info){
  Serial.println("Connected to AP successfully!");
}

void WiFiGotIP(WiFiEvent_t event, WiFiEventInfo_t info){
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void WiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info){
  Serial.println("Disconnected from WiFi access point");
  Serial.print("WiFi lost connection. Reason: ");
  Serial.println(info.wifi_sta_disconnected.reason);
  Serial.println("Trying to Reconnect");
  WiFi.begin(ssid, password);
}

void setup() {

  Serial.begin(115200);  
  
  if (! aht.begin()) {
      Serial.println("Could not find AHT? Check wiring");
      while (1) delay(10);
    }
  Serial.println("AHT10 or AHT20 found");  
  
  // delete old config
  WiFi.disconnect(true);

  WiFi.onEvent(WiFiStationConnected, ARDUINO_EVENT_WIFI_STA_CONNECTED);
  WiFi.onEvent(WiFiGotIP, ARDUINO_EVENT_WIFI_STA_GOT_IP);
  WiFi.onEvent(WiFiStationDisconnected, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("Waiting for WIFI network...");

  Serial.println(WiFi.localIP()); // Print the IP address
  
  ac.begin();

  delay(1000);

  Serial.println("mounting " FILESYSTEMSTR "...");

  if (!FILESYSTEM.begin()) {
    // Serial.println("Failed to mount file system");
    return;
  }  

  server.on("/state", HTTP_PUT, []() {
    DynamicJsonDocument root(1024);
    DeserializationError error = deserializeJson(root, server.arg("plain"));
    if (error) {
      server.send(404, "text/plain", "FAIL. " + server.arg("plain"));
    } else {
      if (root.containsKey("temp")) {
        acState.temperature = (uint8_t) root["temp"];
      }

      if (root.containsKey("fan")) {
        acState.fan = (uint8_t) root["fan"];
      }

      if (root.containsKey("power")) {
        acState.powerStatus = root["power"];
      }

      if (root.containsKey("mode")) {
        acState.operation = root["mode"];
      }

      String output;
      serializeJson(root, output);
      server.send(200, "text/plain", output);

      delay(200);

      if (acState.powerStatus) {
        ac.setPower(true);
        ac.setTemp(acState.temperature);
        if (acState.operation == 0) {
          ac.setMode(AUTO_MODE);
          ac.setFan(FAN_AUTO);
          acState.fan = 0;
        } else if (acState.operation == 1) {
          ac.setMode(COOL_MODE);
        } else if (acState.operation == 2) {
          ac.setMode(DRY_MODE);
        } else if (acState.operation == 3) {
          ac.setMode(HEAT_MODE);
        } else if (acState.operation == 4) {
          ac.setMode(FAN_MODE);
        }

        if (acState.operation != 0) {
          if (acState.fan == 0) {
            ac.setFan(FAN_AUTO);
          } else if (acState.fan == 1) {
            ac.setFan(FAN_MIN);
          } else if (acState.fan == 2) {
            ac.setFan(FAN_MED);
          } else if (acState.fan == 3) {
            ac.setFan(FAN_HI);
          }
        }
      } else {
        ac.setPower(false);
      }
      ac.send();
    }
  });

  server.on("/file-upload", HTTP_POST,
  // if the client posts to the upload page
  []() {
    // Send status 200 (OK) to tell the client we are ready to receive
    server.send(200);
  },
  handleFileUpload);  // Receive and save the file

  server.on("/file-upload", HTTP_GET, []() {
    // if the client requests the upload page

    String html = "<form method=\"post\" enctype=\"multipart/form-data\">";
    html += "<input type=\"file\" name=\"name\">";
    html += "<input class=\"button\" type=\"submit\" value=\"Upload\">";
    html += "</form>";
    server.send(200, "text/html", html);
  });

  server.on("/", []() {
    
    if(!server.authenticate(www_username, www_password))
      return server.requestAuthentication(); 
    
    server.sendHeader("Location", String("ui.html"), true);
    server.send(302, "text/plain", "");
  });

  server.on("/state", HTTP_GET, []() {
    DynamicJsonDocument root(1024);
    root["mode"] = acState.operation;
    root["fan"] = acState.fan;
    root["temp"] = acState.temperature;
    root["power"] = acState.powerStatus;
    String output;
    serializeJson(root, output);
    server.send(200, "text/plain", output);
  });  

  server.on("/read_sensor_data", HTTP_GET, []() {
    // if the client requests the temperature
    sensors_event_t humidity, temperature;
    aht.getEvent(&humidity, &temperature);// populate temp and humidity objects with fresh data       
    
    const float T = temp_correct(temperature.temperature); //correct for offset
    // correct relative humidity for temperature offset
    const float RH = humidity_correct(temperature.temperature, humidity.relative_humidity);
    // calculate dew point
    const float T_dewpoint = dewpoint(T, RH);

    String temp_str = String(T);
    String humid_str = String(RH);
    String dewpoint_str = String(T_dewpoint);

    server.send(200, "text/plain", temp_str + ',' + humid_str + ',' + dewpoint_str);
  });


  server.on("/reset", []() {
    server.send(200, "text/html", "reset");
    delay(100);
    ESP.restart();
  });

  server.serveStatic("/", FILESYSTEM, "/", "max-age=86400");

  server.onNotFound(handleNotFound);

  server.begin();

  timer.every(60000, postToDB);
}

void loop() {      
  server.handleClient();
  timer.tick(); //tick the timer  
}