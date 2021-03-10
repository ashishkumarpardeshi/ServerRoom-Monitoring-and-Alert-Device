/************************************************************************
  Sketch: Serverroom_HBCSE_OTAWU_telB.ino

  Programmer: Ashish Pardeshi

  Description: This sketch will read Temperature & Humidity (BME280)
               ,Smoke Sensor (MQ2) and DateTime Stamp.
               It will display data on OLED.
               It will send data on SNMP.
               It will send data on Telegram.

  Interfacings
  --------------------------------------------------------------------------------
  S.No            Device            Parameters                Peripherals/Pins
  
  1               BME280            Temperature &             I2C (SDA, SCL)
                                    Humidity

  2               MQ2               Smoke                     ADC (IO32)

 
  3               OLED              Display Module            I2C (SDA, SCL)
                  128 x 64   

  4               Buzz_IO           Buzzer                    IO25

  5               BoardLED          ON Board_LED              IO14

  --------------------------------------------------------------------------------
*/


#ifdef ESP32
  #include <WiFi.h>
#else
  #include <ESP8266WiFi.h>
#endif
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h> // Universal Telegram Bot Library written by Brian Lough: https://github.com/witnessmenow/Universal-Arduino-Telegram-Bot
#include <ArduinoJson.h>
#include <Wire.h>

#include <Adafruit_BME280.h>
#include <Adafruit_Sensor.h>

#include <Adafruit_SSD1306.h>

#include <ESP32_tone.h>

#include "DS3231.h"

int melody[] = {
  NOTE_C4, NOTE_G3, NOTE_G3, NOTE_A3, NOTE_G3
};

int noteDurations[] = {
  2, 4, 8, 8, 2
};

int buzzerPin = 25;
//create an instance named buzzer
ESP32_tone buzzer;

const char* host = "esp32";

/* Style */
String style =
"<style>#file-input,input{width:100%;height:44px;border-radius:4px;margin:10px auto;font-size:15px}"
"input{background:#f1f1f1;border:0;padding:0 15px}body{background:#3498db;font-family:sans-serif;font-size:14px;color:#777}"
"#file-input{padding:0;border:1px solid #ddd;line-height:44px;text-align:left;display:block;cursor:pointer}"
"#bar,#prgbar{background-color:#f1f1f1;border-radius:10px}#bar{background-color:#3498db;width:0%;height:10px}"
"form{background:#fff;max-width:258px;margin:75px auto;padding:30px;border-radius:5px;text-align:center}"
".btn{background:#3498db;color:#fff;cursor:pointer}</style>";

/* Login page */
String loginIndex = 
"<form name=loginForm>"
"<h1>Makerspace (121) MAD Login</h1>"
"<input name=userid placeholder='User ID'> "
"<input name=pwd placeholder=Password type=Password> "
"<input type=submit onclick=check(this.form) class=btn value=Login></form>"
"<script>"
"function check(form) {"
"if(form.userid.value=='makerspace' && form.pwd.value=='rocks')"
"{window.open('/serverIndex')}"
"else"
"{alert('Error Password or Username')}"
"}"
"</script>" + style;
 
/* Server Index Page */
String serverIndex = 
"<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
"<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
"<input type='file' name='update' id='file' onchange='sub(this)' style=display:none>"
"<label id='file-input' for='file'>   Choose file...</label>"
"<input type='submit' class=btn value='Update'>"
"<br><br>"
"<div id='prg'></div>"
"<br><div id='prgbar'><div id='bar'></div></div><br></form>"
"<script>"
"function sub(obj){"
"var fileName = obj.value.split('\\\\');"
"document.getElementById('file-input').innerHTML = '   '+ fileName[fileName.length-1];"
"};"
"$('form').submit(function(e){"
"e.preventDefault();"
"var form = $('#upload_form')[0];"
"var data = new FormData(form);"
"$.ajax({"
"url: '/update',"
"type: 'POST',"
"data: data,"
"contentType: false,"
"processData:false,"
"xhr: function() {"
"var xhr = new window.XMLHttpRequest();"
"xhr.upload.addEventListener('progress', function(evt) {"
"if (evt.lengthComputable) {"
"var per = evt.loaded / evt.total;"
"$('#prg').html('progress: ' + Math.round(per*100) + '%');"
"$('#bar').css('width',Math.round(per*100) + '%');"
"}"
"}, false);"
"return xhr;"
"},"
"success:function(d, s) {"
"console.log('success!') "
"},"
"error: function (a, b, c) {"
"}"
"});"
"});"
"</script>" + style;


WebServer server(80);

#define DEVICE_ID 1     // 0 for CLAB Server and 1 for Makerspace room

#define BUZZER  25
#define BOARD_LED  14 

#define SMOKE_SENSOR 32

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)

// Creating Adafruit bme object for sensor BME280
Adafruit_BME280 bme;     
RTClib RTC; 
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define CHAT_ID *Your Chat ID*

// Initialize Telegram BOT
#define BOTtoken "Your Bot Taoken"  // your Bot Token

WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);

//Checks for new messages every 1 second.
int botRequestDelay = 1000;
unsigned long lastTimeBotRan;
String macAddr;

unsigned int lastMinute = 0;


// ******** Global Variables *********

char* ssid;//[] = { };

volatile float temperature, humidity;

volatile int smoke;

String messageTH, messageSmoke;

float maxTemp = 40.00;
float maxHumidity = 70.00;
int maxSmoke = 20;


String mon[12] = {"Jan", "Feb", "Mar", "Apr", "May", 
                  "Jun", "Jul", "Aug", "Sep", "Oct", 
                  "Nov", "Dec"
                 };


const unsigned char temprature_bmp [] PROGMEM = {
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x80, 0xff, 0xff, 0xff, 
  0xff, 0x00, 0x3f, 0xff, 0xff, 0xfe, 0x00, 0x1f, 0xff, 0xff, 0xfc, 0x3e, 0x1f, 0xff, 0xff, 0xfc, 
  0x7f, 0x0f, 0xff, 0xff, 0xf8, 0xff, 0x8f, 0xff, 0xff, 0xf8, 0xff, 0x8f, 0xff, 0xff, 0xf8, 0xff, 
  0xc7, 0xff, 0xff, 0xf8, 0xfc, 0x07, 0xff, 0xff, 0xf8, 0xf8, 0x07, 0xff, 0xff, 0xf8, 0xfc, 0x07, 
  0xff, 0xff, 0xf8, 0xff, 0xc7, 0xff, 0xff, 0xf8, 0xff, 0x87, 0xff, 0xff, 0xf8, 0xfc, 0x07, 0xff, 
  0xff, 0xf8, 0xf8, 0x07, 0xff, 0xff, 0xf8, 0xfc, 0x07, 0xff, 0xff, 0xf8, 0xff, 0x87, 0xff, 0xff, 
  0xf8, 0xff, 0xc7, 0xff, 0xff, 0xf8, 0xfc, 0x07, 0xff, 0xff, 0xf8, 0xf8, 0x07, 0xff, 0xff, 0xf8, 
  0xfc, 0x07, 0xff, 0xff, 0xf8, 0xff, 0xc7, 0xff, 0xff, 0xf8, 0xff, 0xc7, 0xff, 0xff, 0xf8, 0xf3, 
  0xc7, 0xff, 0xff, 0xf8, 0xe1, 0xc7, 0xff, 0xff, 0xf8, 0xe1, 0xc7, 0xff, 0xff, 0xf8, 0xe1, 0xc7, 
  0xff, 0xff, 0xf8, 0xe1, 0xc7, 0xff, 0xff, 0xf8, 0xe1, 0xc7, 0xff, 0xff, 0xf8, 0xe1, 0xc7, 0xff, 
  0xff, 0xf8, 0xe1, 0xc7, 0xff, 0xff, 0xf8, 0xe1, 0xc7, 0xff, 0xff, 0xf8, 0xe1, 0xc7, 0xff, 0xff, 
  0xe0, 0xe1, 0xc3, 0xff, 0xff, 0xc1, 0xc1, 0xc1, 0xff, 0xff, 0x83, 0xc0, 0xf0, 0xff, 0xff, 0x87, 
  0x80, 0xf8, 0x7f, 0xff, 0x0f, 0x00, 0x3c, 0x7f, 0xff, 0x1e, 0x00, 0x1c, 0x3f, 0xfe, 0x1c, 0x00, 
  0x0e, 0x3f, 0xfe, 0x38, 0x00, 0x0e, 0x1f, 0xfe, 0x38, 0x00, 0x07, 0x1f, 0xfc, 0x38, 0x00, 0x07, 
  0x1f, 0xfc, 0x70, 0x00, 0x07, 0x1f, 0xfc, 0x70, 0x00, 0x07, 0x1f, 0xfc, 0x70, 0x00, 0x07, 0x1f, 
  0xfc, 0x38, 0x00, 0x07, 0x1f, 0xfe, 0x38, 0x00, 0x07, 0x1f, 0xfe, 0x38, 0x00, 0x0e, 0x1f, 0xfe, 
  0x1c, 0x00, 0x0e, 0x3f, 0xff, 0x1e, 0x00, 0x1c, 0x3f, 0xff, 0x0f, 0x00, 0x3c, 0x7f, 0xff, 0x87, 
  0xc0, 0xf8, 0x7f, 0xff, 0xc3, 0xff, 0xe0, 0xff, 0xff, 0xe0, 0xff, 0xc1, 0xff, 0xff, 0xf0, 0x1e, 
  0x03, 0xff, 0xff, 0xf8, 0x00, 0x0f, 0xff, 0xff, 0xfe, 0x00, 0x1f, 0xff, 0xff, 0xff, 0x80, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};

// 'noun_humidity_2580579', 40x64px
const unsigned char humidity_bmp [] PROGMEM = {
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf3, 
  0xff, 0xff, 0xff, 0xff, 0xe1, 0xff, 0xff, 0xff, 0xff, 0xe1, 0xff, 0xff, 0xff, 0xff, 0xc0, 0xff, 
  0xff, 0xff, 0xff, 0xc0, 0xff, 0xff, 0xff, 0xff, 0x80, 0x7f, 0xff, 0xff, 0xff, 0x80, 0x7f, 0xff, 
  0xff, 0xff, 0x00, 0x3f, 0xff, 0xff, 0xff, 0x00, 0x1f, 0xff, 0xff, 0xfe, 0x00, 0x1f, 0xff, 0xff, 
  0xfc, 0x00, 0x0f, 0xff, 0xff, 0xfc, 0x00, 0x0f, 0xff, 0xff, 0xf8, 0x00, 0x07, 0xff, 0xff, 0xf0, 
  0x00, 0x03, 0xff, 0xff, 0xf0, 0x00, 0x03, 0xff, 0xff, 0xe0, 0x00, 0x01, 0xff, 0xff, 0xc0, 0x00, 
  0x00, 0xff, 0xff, 0xc0, 0x00, 0x00, 0xff, 0xff, 0x80, 0x00, 0x00, 0x7f, 0xff, 0x80, 0x00, 0x00, 
  0x7f, 0xff, 0x00, 0x00, 0x00, 0x3f, 0xff, 0x00, 0x00, 0x00, 0x3f, 0xfe, 0x00, 0x00, 0x00, 0x1f, 
  0xfe, 0x00, 0x00, 0x00, 0x1f, 0xfc, 0x01, 0xc0, 0xe0, 0x0f, 0xfc, 0x01, 0xc1, 0xe0, 0x0f, 0xfc, 
  0x01, 0xc1, 0xc0, 0x0f, 0xf8, 0x00, 0x03, 0x80, 0x07, 0xf8, 0x00, 0x07, 0x00, 0x07, 0xf8, 0x00, 
  0x0f, 0x00, 0x07, 0xf8, 0x00, 0x1e, 0x00, 0x07, 0xf8, 0x00, 0x3c, 0x00, 0x07, 0xf8, 0x00, 0x78, 
  0x00, 0x0f, 0xfc, 0x00, 0xf0, 0x00, 0x0f, 0xfc, 0x00, 0xe0, 0xc0, 0x0f, 0xfc, 0x01, 0xc0, 0xe0, 
  0x0f, 0xfe, 0x01, 0x80, 0xe0, 0x1f, 0xfe, 0x00, 0x00, 0x00, 0x1f, 0xff, 0x00, 0x00, 0x00, 0x3f, 
  0xff, 0x80, 0x00, 0x00, 0x7f, 0xff, 0x80, 0x00, 0x00, 0x7f, 0xff, 0xe0, 0x00, 0x00, 0xff, 0xff, 
  0xe0, 0x00, 0x01, 0xff, 0xff, 0xf8, 0x00, 0x07, 0xff, 0xff, 0xfe, 0x00, 0x0f, 0xff, 0xff, 0xff, 
  0xc0, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};

// 'noun_Smoke_3394237', 40x64px
const unsigned char smoke_bmp [] PROGMEM = {
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0xc0, 0x00, 0x03, 0xff, 0xfe, 0x00, 
  0x00, 0x00, 0x7f, 0xf8, 0x00, 0x00, 0x00, 0x1f, 0xf0, 0x07, 0xff, 0xe0, 0x0f, 0xe0, 0x7c, 0x00, 
  0x3f, 0x07, 0xe3, 0x80, 0xff, 0x81, 0xc7, 0xee, 0x3f, 0xe7, 0xfc, 0x77, 0xf9, 0xe0, 0x00, 0x07, 
  0x9f, 0xf7, 0x00, 0x08, 0x00, 0xef, 0xfc, 0x00, 0x00, 0x00, 0x3f, 0xf0, 0x00, 0xff, 0x00, 0x0f, 
  0xf0, 0x0f, 0x81, 0xf0, 0x07, 0xe4, 0x18, 0x00, 0x18, 0x37, 0xf0, 0x30, 0x08, 0x04, 0x07, 0xf0, 
  0x20, 0xc9, 0x04, 0x0f, 0xf8, 0x32, 0x4b, 0x44, 0x1f, 0xfe, 0x12, 0x4a, 0x48, 0x7f, 0xff, 0xdb, 
  0x00, 0x4b, 0xff, 0xff, 0xf8, 0x00, 0x1f, 0xff, 0xff, 0xfd, 0xff, 0x9f, 0xff, 0xff, 0xff, 0x00, 
  0xff, 0xff, 0xff, 0xfe, 0x08, 0x7f, 0xff, 0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};


void setup() 
{
  // initialize serial for debugging
  Serial.begin(9600);

  pinMode(BOARD_LED,OUTPUT);
  pinMode(BUZZER,OUTPUT);
  
  digitalWrite(BUZZER,LOW);
  digitalWrite(BOARD_LED,LOW);

   //initialize buzzer
  buzzer.ESP32_toneB(0);

  //set the compatibleMode of the library
  buzzer.setCompatibleMode(true);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) 
  { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    //for(;;); // Don't proceed, loop forever
  }
  
  display.clearDisplay();
  display.display();
  
  display.setTextSize(2); // Draw 2X-scale text
  display.setTextColor(WHITE);
  display.setCursor(5, 5);
  display.println(F("Makerspace"));
  display.setCursor(0, 25);
  display.println(F("    DIY    "));
  display.setCursor(0, 45);
  display.println(F("  Culture "));
  display.display();
  Serial.println("Welcome");
  delay(3000);
  intro_melody();
  display.clearDisplay();
  display.display();
  
  display.setTextSize(2); // Draw 2X-scale text
  display.setTextColor(WHITE);
  display.setCursor(5, 5);
  display.println(F("Temp & Hum"));
  display.setCursor(0, 25);
  display.println(F("    MAD   "));
  display.setTextSize(1); // Draw 2X-scale text
  display.setTextColor(WHITE);
  display.setCursor(0, 43);
  display.println(F(" Monitoring and Alert"));
  display.setCursor(0, 56);
  display.println(F("       Device        "));
  display.display();
  delay(3000);
  
  // MQ2 pre heat
  display.clearDisplay();
  display.display();
  
  display.setTextSize(1); // Draw 2X-scale text
  display.setTextColor(WHITE);
  display.setCursor(30, 20);
  display.println(F("MQ2 Pre Heat"));
  display.setCursor(30, 50);
  display.println(F("Time Starts"));
  display.display();
  
  Serial.println("MQ2 Pre Heat Starts");
  delay(10000);
  Serial.println("MQ2 Pre Heat Done");
  
  display.clearDisplay();
  display.display();
  
  display.setTextSize(1); // Draw 2X-scale text
  display.setTextColor(WHITE);
  display.setCursor(30, 20);
  display.println(F("MQ2 Pre Heat"));
  display.setCursor(30, 50);
  display.println(F("    Done     "));
  display.display();
  
  // Set WiFi to station mode and disconnect from an AP if it was previously connected
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  Serial.println("Setup Done");

  InitWiFi();

  if (!MDNS.begin(host)) { //http://esp32.local
    Serial.println("Error setting up MDNS responder!");
    while (1) {
      delay(1000);
    }
  }
  Serial.println("mDNS responder started");
  /*return index page which is stored in serverIndex */
  server.on("/", HTTP_GET, []() 
  {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", loginIndex);
  });
  
  server.on("/serverIndex", HTTP_GET, []() 
  {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", serverIndex);
  });
  /*handling uploading firmware file */
  server.on("/update", HTTP_POST, []() 
  {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("Update: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      /* flashing firmware to ESP*/
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    }
  });
  server.begin();

  
  if (!bme.begin(0x76)) {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    //while (1);
  }


  Wire.begin();

  Serial.println  ("Sensors Done");
}

void loop() 
{
  server.handleClient();

  buzzAlert();
  
  getBmeReadings();
  oledTempHum();
  
  getSmokeReading();
  oledSmoke();

  telegramBotRoutine();
}

void telegramBotRoutine()
{
  
  buzzAlert();
  if (WiFi.status() != WL_CONNECTED) 
  {
    Serial.println("Reconnecting");
    AP_Connection();
  }

  //if (millis() > lastTimeBotRan + botRequestDelay)  
  //{
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    while(numNewMessages) 
    {
      buzzAlert();
      Serial.println("got response");
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    //lastTimeBotRan = millis();
  //}

  String alertMsg = "HIGH ALERT \n";
  if(DEVICE_ID == 0)
    alertMsg += "CLAB_Server \n";
  else
    alertMsg += "Makerspace (121) \n";
  alertMsg += messageTH;
  alertMsg += messageSmoke;
  buzzAlert();
  if(temperature >= maxTemp || humidity >= maxHumidity || smoke >= maxSmoke)
  {
    bot.sendMessage(CHAT_ID, alertMsg, "");
    //delay(10);
  }

  routineUpdates();
}

void getBmeReadings()
{
  temperature = bme.readTemperature();
  humidity = bme.readHumidity();
  buzzAlert();
  messageTH = "Temperature: " + String(temperature) + " ºC \n";
  messageTH += "Humidity: " + String (humidity) + " % \n";

  /*String strH = String(humidity);
  strH.toCharArray(strHumidity, strH.length());
  String strT = String(temperature);
  strT.toCharArray(strTemperature, strT.length());*/
}

void getSmokeReading()
{
  smoke = analogRead(SMOKE_SENSOR);
  messageSmoke = "Smoke: " + String(smoke) + " \n";
  buzzAlert();
}


void oledTempHum()
{
  buzzAlert();
  display.clearDisplay();
  oledDataTime();
  display.drawBitmap(0, 0,temprature_bmp, 40, 64, WHITE); 

  display.setTextSize(2); // Draw 2X-scale text
  display.setTextColor(WHITE);
  display.setCursor(53, 25);
  display.print(temperature);
  display.setTextSize(1); // Draw 2X-scale text
  display.setCursor(52, 50);
  display.println(F("Deg Celcius"));
  display.display();
  
  
  delay(500);
  buzzAlert();
  display.clearDisplay();
  oledDataTime();
  display.drawBitmap(0, 0,humidity_bmp, 40, 64, WHITE); 

  display.setTextSize(2); // Draw 2X-scale text
  display.setTextColor(WHITE);
  display.setCursor(53, 25);
  display.print(humidity);
  display.setTextSize(1); // Draw 2X-scale text
  display.setCursor(52, 50);
  display.println(F("Percent (%)"));
  display.display();
  delay(500);
}

void oledSmoke()
{
  buzzAlert();
  display.clearDisplay();
  oledDataTime();
  display.drawBitmap(0, 0,smoke_bmp, 40, 64, WHITE); 

  display.setTextSize(2); // Draw 2X-scale text
  display.setTextColor(WHITE);
  display.setCursor(60, 25);
  display.println(smoke);
  display.setTextSize(1); // Draw 2X-scale text
  display.setCursor(52, 50);
  display.println(F("ADC Value"));

  display.display();
  
  delay(500);
}

void routineUpdates()
{
  buzzAlert();
  int i = 0;
  unsigned int intervalMinuteUpdate[4] = {0,15,30,45};
  bool onceFlag = false;
  String routineMsg = " ";
  DateTime now = RTC.now();

  if(now.minute() != lastMinute)
  {
    for(i=0;i<=3;i++)
    {
      if(now.minute() == intervalMinuteUpdate[i])
      {
         onceFlag = true;
         lastMinute = now.minute();
         break;
      }
    }
    
  }
  
  if(onceFlag == true)
  {
    getBmeReadings();
    getSmokeReading();

    if(DEVICE_ID == 0)
      routineMsg = "CLAB_ServerRoom routine Update \n";
    else
      routineMsg = "Makerspace (121) routine Update \n";
      
    routineMsg += messageTH;
    routineMsg += messageSmoke;
    
    bot.sendMessage(CHAT_ID, routineMsg, "");
    
    onceFlag = false;
  }
}

void oledDataTime()
{
  DateTime now = RTC.now();

  String dateTimeStamp;
  dateTimeStamp = String(now.day()) + " " + String(mon[now.month() - 1]) + ", ";
  dateTimeStamp += String(now.hour())+ ":" + String(now.minute());
  //dateTimeStamp = "26 Jan, 20:59";
  display.setTextSize(1); // Draw 2X-scale text
  display.setTextColor(WHITE);
  display.setCursor(50, 5);
  Serial.println(dateTimeStamp);
  display.println(dateTimeStamp);
}

void buzzBeep()
{
  digitalWrite(BUZZER, HIGH);
  delay(500);
  digitalWrite(BUZZER, LOW);
}

void buzzBeepInd()
{
  digitalWrite(BUZZER, HIGH);
  delay(50);
  digitalWrite(BUZZER, LOW);
}

void buzzAlert()
{
  if(temperature >= maxTemp || humidity >= maxHumidity || smoke >= maxSmoke)
  {
    buzzBeep();
  } 
}

void InitWiFi()
{
  Serial.println("Initialising WiFi");
  WiFi.mode(WIFI_STA);
  
  printMacAddress(); 
  
  AP_Connection();  
}

void AP_Connection()
{
  scanNetwork();

  int connect_count = 0;
  
  String ssid = WiFi.SSID(0);
  int ssid_len = ssid.length() + 1;
  char ssid_charArray[ssid_len];
  ssid.toCharArray(ssid_charArray, ssid_len);
  Serial.print("ssid CharArray[0]: ");
  
  Serial.println("Attemp to connect SSID: ");
  Serial.println(ssid_charArray);

  while ( WiFi.status() != WL_CONNECTED) 
  {
    delay(1000);
    Serial.println("..");
    WiFi.begin(ssid_charArray);
    connect_count ++;
    if(connect_count == 3)
    {
      ssid = WiFi.SSID(1);
      int ssid_len = ssid.length() + 1;
      char ssid_charArray[ssid_len];
      ssid.toCharArray(ssid_charArray, ssid_len);
      Serial.print("ssid CharArray[1]: ");
    }
    if(connect_count == 6)
    {
      ssid = WiFi.SSID(2);
      int ssid_len = ssid.length() + 1;
      char ssid_charArray[ssid_len];
      ssid.toCharArray(ssid_charArray, ssid_len);
      Serial.print("ssid CharArray[2]: ");
    }

    if(connect_count > 8)
    {
      ESP.restart();
    }
  }
  
  Serial.print("Connected to AP: ");
  Serial.println(ssid_charArray);
  
  Serial.println("Obtaining IP: ");
  IPAddress ip = WiFi.localIP();
  //String ipS = String(ip);
  //int ip_len = strlen(ip) + 1;
  //char ip_charArray[ip_len];
  /*for(int i = 0; i<=ip_len,i++)
  {
    ip_charArray[i] = ip[i];
  }*/
  
  Serial.print("IP Address: ");
  Serial.println(ip);
  
  display.clearDisplay();
  display.display();
  
  display.setTextSize(1); // Draw 2X-scale text
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println(F("Connected to AP: "));
  display.setCursor(0,15);
  display.println(ssid_charArray);
  display.setCursor(0,30);
  display.println(macAddr);
  display.setTextSize(2);
  display.setCursor(0, 50);
  display.println(ip);
  display.display();

  delay(4000);

  buzzBeepInd();

  /*String ipAdd = WiFi.localIP();
  int ip_len = ipAdd.length() + 1;
  char ip_charArray[ip_len];
  ip.toCharArray(ip_charArray, ip_len);*/

  
  Serial.println("initialization Done");

  delay(50);

    String botConnectMsg = " ";
    
    if(DEVICE_ID == 1)
       botConnectMsg = "Makerspace (121) MAD \n";
    else
       botConnectMsg = "CLAB_ServerRoom MAD \n";

    botConnectMsg += "Connected to AP: ";
      
    if(connect_count < 3)
      botConnectMsg += String(WiFi.SSID(0));
    else if(connect_count >= 3 && connect_count < 6)
      botConnectMsg +=  String(WiFi.SSID(1));
    else
      botConnectMsg += String(WiFi.SSID(2));

    //botConnectMsg += "\n" + macAddr + "\n";
    //botConnectMsg += "IP: " + WiFi.localIP() + "\n";
    
      
    Serial.println(botConnectMsg);

    if(WiFi.status() == WL_CONNECTED) 
    {
      Serial.println("connected");
      bot.sendMessage(CHAT_ID, botConnectMsg, "");
    }
}

void printMacAddress()
{
  // get your MAC address
  byte mac[6];
  WiFi.macAddress(mac);
  
  // print MAC address
  char buf[20];
  sprintf(buf, "%02X:%02X:%02X:%02X:%02X:%02X", mac[5], mac[4], mac[3], mac[2], mac[1], mac[0]);
  Serial.print("MAC address: ");
  Serial.println(buf);
  macAddr = "MAC "+ String(buf); //+ mac[5] + ":" + mac[4] + ":" + mac[3] + ":" + mac[2] + ":" + mac[1] + ":" + mac[0];
}

void scanNetwork()
{
  Serial.println("scan start");

    // WiFi.scanNetworks will return the number of networks found
    int n = WiFi.scanNetworks();
    Serial.println("scan done");
    if (n == 0) 
    {
        Serial.println("no networks found");
    } 
    else 
    {
        Serial.print(n);
        Serial.println(" networks found");
        for (int i = 0; i < n; ++i) 
        {
            // Print SSID and RSSI for each network found
            Serial.print(i + 1);
            Serial.print(": ");
            Serial.print(WiFi.SSID(i));
            Serial.print(" (");
            Serial.print(WiFi.RSSI(i));
            Serial.print(")");
            Serial.println((WiFi.encryptionType(i) == WIFI_AUTH_OPEN)?" ":"*");
            delay(10);
        }
    }
}

//Handle what happens when you receive new messages
void handleNewMessages(int numNewMessages) 
{
  Serial.println("handleNewMessages");
  Serial.println(String(numNewMessages));
  //inti = 
   for (int i=0; i<numNewMessages; i++) 
   {
    // Chat id of the requester
    String chat_id = String(bot.messages[i].chat_id);
    Serial.println(chat_id);
    if (chat_id != CHAT_ID)
    {
      bot.sendMessage(chat_id, "Unauthorized user", "");
      continue;
    }
    
    // Print the received message
    String text = bot.messages[i].text;
    Serial.println(text);

    String from_name = bot.messages[i].from_name;

    if (text == "/start") {
      String welcome = "Welcome, " + from_name + ".\n";
      welcome += "Use the following command to get current Sensor Data.\n\n";
      welcome += "/Sensordata \n";
      bot.sendMessage(chat_id, welcome, "");
    }

    if (text == "/Sensordata") 
    {
      String readings = " ";
      if(DEVICE_ID == 0)
      {
        readings = "CLAB_Server\n"; 
      }
      else
      {
        readings = "Makerspace (121)\n"; 
      }
      
      readings +=  messageTH + messageSmoke;
      bot.sendMessage(chat_id, readings, "");
    }  
  }
}

void intro_melody()
{
  for (int thisNote = 0; thisNote < 8; thisNote++) {

    // to calculate the note duration, take one second divided by the note type.
    //e.g. quarter note = 1000 / 4, eighth note = 1000/8, etc.
    int noteDuration = 1000 / noteDurations[thisNote];
    buzzer.tone(25, melody[thisNote], noteDuration);

    // to distinguish the notes, set a minimum time between them.
    // the note's duration + 30% seems to work well:
    int pauseBetweenNotes = noteDuration * 1.30;
    delay(pauseBetweenNotes);
    // stop the tone playing:
    buzzer.noTone(25);
  }
}
