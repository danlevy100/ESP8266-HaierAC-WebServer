/* Dan Levy, July 2021.
 *  Web server control of Haier Buzz AC.
 */

/* Copyright 2019 Motea Marius

  This example code will create a webserver that will provide basic control to AC units using the web application
  build with javascript/css. User config zone need to be updated if a different class than Collix need to be used.
  Javasctipt file may also require minor changes as in current version it will not allow to set fan speed if Auto mode
  is selected (required for Coolix).

*/
#include "ESP8266-HaierAC-WebServer.h"
#if defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266WebServer.h>
#endif  // ESP8266

#if defined(ESP32)
#include <ESPmDNS.h>
#include <WebServer.h>
#include <WiFi.h>
#include <Update.h>
#endif  // ESP32

#include <WiFiUdp.h>
//#include <WiFiManager.h>
#include <ArduinoJson.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>

#include <EasyDDNS.h>
#include <Adafruit_Sensor.h>
//#include <Adafruit_BME280.h>
#include <Adafruit_AHTX0.h>
#include <Wire.h>
#include <SPI.h>

//#include "DHT.h"

//// ###### User configuration space for AC library classes ##########

#include <ir_Haier.h>  //  replace library based on your AC unit model, check https://github.com/crankyoldgit/IRremoteESP8266

#define AUTO_MODE kHaierAcYrw02Auto
#define COOL_MODE kHaierAcYrw02Cool
#define DRY_MODE kHaierAcYrw02Dry
#define HEAT_MODE kHaierAcYrw02Heat
#define FAN_MODE kHaierAcYrw02Fan

#define FAN_AUTO kHaierAcYrw02FanAuto
#define FAN_MIN kHaierAcYrw02FanLow
#define FAN_MED kHaierAcYrw02FanMed
#define FAN_HI kHaierAcYrw02FanHigh

#define TURBO_OFF kHaierAcYrw02TurboOff
#define TURBO_LOW kHaierAcYrw02TurboLow
#define TURBO_HI kHaierAcYrw02TurboHigh

// ESP8266 GPIO pin to use for IR blaster.
const uint16_t kIrLed = 0;
// Library initialization, change it according to the imported library file.
IRHaierACYRW02 ac(kIrLed);

#define DHTPIN 14  // what digital pin we're connected to
//#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
//DHT dht(DHTPIN, DHTTYPE);
//Adafruit_BME280 bme; // I2C
Adafruit_AHTX0 aht;

unsigned BMEstatus;
float home_temp, home_humidity, home_pressure;
float dht_temperature, dht_humidity;
float gamma_, dewPoint;
// Dew Point calculation constants 
float const a = 6.1121, b = 18.678, c = 257.14;

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

// NETWORK: Static IP details...
IPAddress ip(192, 168, 1, 14);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

#if defined(ESP8266)

ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdateServer;
#endif  // ESP8266
#if defined(ESP32)
WebServer server(80);
#endif  // ESP32

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

void setup() {

  Serial.begin(115200);

  /*
  // BME280 test
  Serial.println(F("BME280 test"));
  // default settings
  BMEstatus = bme.begin(0x76);  
  // You can also pass in a Wire library object like &Wire2
  // status = bme.begin(0x76, &Wire2)
  if (!BMEstatus) {
      Serial.println("Could not find a valid BME280 sensor, check wiring, address, sensor ID!");
      Serial.print("SensorID was: 0x"); Serial.println(bme.sensorID(),16);
      Serial.print("        ID of 0xFF probably means a bad address, a BMP 180 or BMP 085\n");
      Serial.print("   ID of 0x56-0x58 represents a BMP 280,\n");
      Serial.print("        ID of 0x60 represents a BME 280.\n");
      Serial.print("        ID of 0x61 represents a BME 680.\n");
      while (1) delay(10);
  }*/
  
  if (! aht.begin()) {
      Serial.println("Could not find AHT? Check wiring");
      while (1) delay(10);
    }
  Serial.println("AHT10 or AHT20 found");  
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  if(WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("WiFi Connect Failed! Rebooting...");
    delay(1000);
    ESP.restart();
  }  
  Serial.println(WiFi.localIP()); // Print the IP address

  //WiFi.config(ip, gateway, subnet);     
  
  EasyDDNS.service("noip");
  EasyDDNS.client("danhome.ddns.net","danlevy100","1qaz1qaz");  
  
  // Get Notified when your IP changes
  EasyDDNS.onUpdate([&](const char* oldIP, const char* newIP){
    Serial.print("EasyDDNS - IP Change Detected: ");
    Serial.println(newIP);
  });
  
  ac.begin();

  delay(1000);

  Serial.println("mounting " FILESYSTEMSTR "...");

  if (!FILESYSTEM.begin()) {
    // Serial.println("Failed to mount file system");
    return;
  }

  /*WiFiManager wifiManager;

  if (!wifiManager.autoConnect(deviceName)) {
    delay(3000);
    ESP.restart();
    delay(5000);
  }*/


#if defined(ESP8266)
  httpUpdateServer.setup(&server);
#endif  // ESP8266



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
  
  server.on("/read_temp", HTTP_GET, []() {
    // if the client requests the temperature
    //home_temp = bme.readTemperature();    
    //home_temp = dht.readTemperature();
    sensors_event_t home_humidity, home_temp;
    aht.getEvent(&home_humidity, &home_temp);// populate temp and humidity objects with fresh data
    String s = String(home_temp.temperature);
    server.send(200, "text/plain", String(s + " C"));
  });

  server.on("/read_humidity", HTTP_GET, []() {
    // if the client requests the temperature
    //home_humidity = bme.readHumidity();
    //home_humidity = dht.readHumidity();
    sensors_event_t home_humidity, home_temp;
    aht.getEvent(&home_humidity, &home_temp);// populate temp and humidity objects with fresh data    
    String s = String(home_humidity.relative_humidity);
    server.send(200, "text/plain", String(s + "%"));
  });

  /*server.on("/read_pressure", HTTP_GET, []() {
    // if the client requests the temperature
    home_pressure = bme.readPressure() / 100.0F * 0.02953;    
    String s = String(home_pressure);
    server.send(200, "text/plain", String(s + " inHg"));
  });*/


  server.on("/reset", []() {
    server.send(200, "text/html", "reset");
    delay(100);
    ESP.restart();
  });

  server.serveStatic("/", FILESYSTEM, "/", "max-age=86400");

  server.onNotFound(handleNotFound);

  server.begin();
}

void loop() {  
  // Check for new public IP every 60 seconds
  EasyDDNS.update(60000);
  
  server.handleClient();
}
