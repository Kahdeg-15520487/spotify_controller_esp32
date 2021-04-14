#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <WiFi.h>
#include <IRremote.h>
#include <Adafruit_AHTX0.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_APDS9960.h>
#include <Adafruit_SGP30.h>
#include <U8g2lib.h>
#include <pcf8563.h>
#include <ArduinoSpotify.h>
#include <ArduinoJson.h>
#include <ArduinoSpotifyCert.h>

#include "cmd.h"
#include "pin.h"

// Country code, including this is advisable

char ssid[] = _SSID;                           // your network SSID (name)
char password[] = _SSID_PWD;                   // your network password
char clientId[] = SPOTIFY_CLIENT_ID;     // Your client ID of your spotify APP
char clientSecret[] = SPOTIFY_CLIENT_SECRET; // Your client Secret of your spotify APP (Do Not share this!)

WiFiClientSecure client;
ArduinoSpotify spotify(client, clientId, clientSecret, SPOTIFY_REFRESH_TOKEN);

unsigned long delayBetweenRequests = 60000; // Time between requests (1 minute)
unsigned long requestDueTime;               //time when request due
unsigned long delayBetweenRolls = 200;
unsigned long rollDueTime;
bool isFirstRoll = true;

String currentSongName;
String currentArtistName;
bool isPlaying = false;


U8G2_SSD1309_128X64_NONAME2_F_4W_SW_SPI u8g2(U8G2_R0,
        /* clock=*/ OLED_SCLK,
        /* data=*/  OLED_MOSI,
        /* cs=*/    OLED_CS,
        /* dc=*/    OLED_DC,
        /* reset=*/ OLED_RST);

IRrecv irrecv(IR_RECV);
PCF8563_Class rtc;

const char *ntpServer           = "pool.ntp.org";
const long  gmtOffset_sec       = 28800;
const int   daylightOffset_sec  = 0;

void setup()
{    
    Serial.begin(115200);

    /*Turn on power control*/
    pinMode(PWR_ON, OUTPUT);
    digitalWrite(PWR_ON, HIGH);

    /*Power on the display*/
    pinMode(OLED_PWR, OUTPUT);
    digitalWrite(OLED_PWR, HIGH);

    /*Set touch Pin as input*/
    pinMode(TOUCH_PIN, INPUT);

    /*Touch chip power control*/
    pinMode(TOUCH_PWR, PULLUP);
    digitalWrite(TOUCH_PWR, HIGH);

    int16_t ypos = 16;

    u8g2.begin();
    u8g2.enableUTF8Print();
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_unifont_t_vietnamese2);
    u8g2.setDrawColor(1);
    u8g2.sendBuffer();

    Wire.begin(RTC_SDA, RTC_SCL);
    rtc.begin();
    RTC_Date t = rtc.getDateTime();
    Serial.print(t.hour);
    Serial.print(":");
    Serial.print(t.minute);
    Serial.print(":");
    Serial.println(t.second);
//
//    if (ESP_RST_DEEPSLEEP !=  esp_reset_reason()) {
//        delay(5000);
//    }

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    Serial.println("");

    // Wait for connection
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    client.setCACert(spotify_server_cert);

    // If you want to enable some extra debugging
    // uncomment the "#define SPOTIFY_DEBUG" in ArduinoSpotify.h

    Serial.println("Refreshing Access Tokens");
    if (!spotify.refreshAccessToken())
    {
        Serial.println("Failed to get access tokens");
    }
    
    irrecv.enableIRIn(); // Start the receiver
}

String rollString(String s){
  char firstChar = s[0];
  uint8_t l = s.length();
  String b = "";
  if(l==0) return b;
  for (uint8_t j = 0; j < l - 1; j++)
  {
      b += s[j + 1];
  }
  b += firstChar;
  return b;
}

void singleCharPrintToLCD(String s,uint8_t l){
  l = s.length() < l ? s.length() : l;

  if(l==0) return;
  
  char sc = '\0';
  String b = String("");
  for(uint8_t i=0;i<l;i++){
    b += s[i];
  }
  u8g2.print(b);
}

const char* serverName = "http://kahdeg.ddns.net:10900/api/vs/";

String getSafeName(String s){
  HTTPClient http;
  
  // Your Domain name with URL path or IP address with path
  http.begin(serverName);
  
  // Your Domain name with URL path or IP address with path
  http.begin(serverName);

  // Set content-type
//  http.addHeader("Content-Type", "text/plain");
http.addHeader("Content-Type", "application/json");
  http.addHeader("Accept", "text/plain");
  
  // Send HTTP GET request
  String body = "\"" + s + "\"";
  int httpResponseCode = http.POST(body);
  
  if (httpResponseCode>0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    String payload = http.getString();
    Serial.println(payload);
    for(uint8_t i = 0; i < http.headers(); i++){
      Serial.print(http.headerName(i));
      Serial.print(": ");
      Serial.println(http.header(i));
    }
    http.end();
    return payload;
  }
  else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  // Free resources
  http.end();
}

void printCurrentlyPlayingToSerial(CurrentlyPlaying currentlyPlaying)
{
    if (!currentlyPlaying.error)
    {
        Serial.println("--------- Currently Playing ---------");

        Serial.print("Is Playing: ");
        isPlaying = currentlyPlaying.isPlaying;
        if (currentlyPlaying.isPlaying)
        {
            Serial.println("Yes");
        }
        else
        {
            Serial.println("No");
        }

        Serial.print("Track: ");
        Serial.println(currentlyPlaying.trackName);
//        Serial.print("Track URI: ");
//        Serial.println(currentlyPlaying.trackUri);
//        Serial.println();

        Serial.print("Artist: ");
        Serial.println(currentlyPlaying.firstArtistName);
//        Serial.print("Artist URI: ");
//        Serial.println(currentlyPlaying.firstArtistUri);
//        Serial.println();
//
//        Serial.print("Album: ");
//        Serial.println(currentlyPlaying.albumName);
//        Serial.print("Album URI: ");
//        Serial.println(currentlyPlaying.albumUri);
//        Serial.println();
//
//        long progress = currentlyPlaying.progressMs; // duration passed in the song
//        long duration = currentlyPlaying.duraitonMs; // Length of Song
//        Serial.print("Elapsed time of song (ms): ");
//        Serial.print(progress);
//        Serial.print(" of ");
//        Serial.println(duration);
//        Serial.println();
//
//        float precentage = ((float)progress / (float)duration) * 100;
//        int clampedPrecentage = (int)precentage;
//        Serial.print("<");
//        for (int j = 0; j < 50; j++)
//        {
//            if (clampedPrecentage >= (j * 2))
//            {
//                Serial.print("=");
//            }
//            else
//            {
//                Serial.print("-");
//            }
//        }
//        Serial.println(">");
//        Serial.println();
//
//        // will be in order of widest to narrowest
//        // currentlyPlaying.numImages is the number of images that
//        // are stored
//        for (int i = 0; i < currentlyPlaying.numImages; i++)
//        {
//            Serial.println("------------------------");
//            Serial.print("Album Image: ");
//            Serial.println(currentlyPlaying.albumImages[i].url);
//            Serial.print("Dimensions: ");
//            Serial.print(currentlyPlaying.albumImages[i].width);
//            Serial.print(" x ");
//            Serial.print(currentlyPlaying.albumImages[i].height);
//            Serial.println();
//        }
//        Serial.println("------------------------");
        
        currentSongName = getSafeName(currentlyPlaying.trackName);
        currentSongName += "          ";
        currentArtistName = getSafeName(currentlyPlaying.firstArtistName);
        currentArtistName += "          ";
        isFirstRoll = true;
    }
}

void printCurrentlyPlayingToLCD()
{            
  if (!isFirstRoll){
    currentSongName = rollString(currentSongName);
    currentArtistName = rollString(currentArtistName);
  }
  u8g2.clearBuffer();
  u8g2.setCursor(0, 13);
  singleCharPrintToLCD(currentSongName,15);
  u8g2.setCursor(0, 30);  //11 + 10 + 5
  singleCharPrintToLCD(currentArtistName,15);  
  u8g2.sendBuffer();

  if (!isFirstRoll){
    rollDueTime = millis() + delayBetweenRolls;
  } else {
    rollDueTime = millis() + 2000;
    isFirstRoll = false;
  }
}

uint8_t handleIRRemote(){
  uint8_t result_msg = CMD_NULL;   /* invalid message, no event */
  decode_results results;
  static uint32_t prev_value = 0;
  int  ret = irrecv.decode(&results);
  if (!ret) {
//    goto endd;
    return CMD_NULL;
  }
  Serial.println(results.value, HEX);
  if (results.value != 0xFFFFFFFF) {
      prev_value = results.value;
  } 
//  else {
//    goto endd;
//  }
  switch (prev_value) {
  case 0xFFA25D:  //CH-   //off
//      result_msg = "off";
      result_msg = CMD_SLEEP;
      break;
  case 0xFF629D:  //CH
//      result_msg = "CH";
      break;
  case 0xFFE21D:  //CH+
//      result_msg = U8X8_MSG_GPIO_MENU_HOME;
//      result_msg = "menu";
      break;
  case 0xFF6897:  //0
//      result_msg = "1000";
      break;
  case 0xFF9867:  //100+
      //result_msg = U8X8_MSG_GPIO_MENU_DOWN;
//      result_msg = "down";
      result_msg = CMD_VOLDOWN;
      break;
  case 0xFFB04F:  //200+
//      result_msg = "CH+";
      break;
  case 0xFF30CF:  //1
//      result_msg = "1001";
      break;
  case 0xFF18E7:  //2
//      result_msg = "1002";
      break;
  case 0xFF7A85://3
//      result_msg = "1003";
      break;
  case 0xFF10EF://4
//      result_msg = "1004";
      break;
  case 0xFF38C7://5
//      result_msg = "1005";
      break;
  case 0xFF5AA5://6
//      result_msg = "1006";
      break;
  case 0xFF42BD://7
//      result_msg = "1007";
      break;
  case 0xFF4AB5://8
//      result_msg = "1008";
      break;
  case 0xFF52AD://9
//      result_msg = "1009";
      break;
  case 0xFFA857:  //+
      //result_msg = U8X8_MSG_GPIO_MENU_SELECT;
//      result_msg = "play";
      result_msg = CMD_PLAY;
      break;
  case 0xFFE01F:  // -
      //result_msg = U8X8_MSG_GPIO_MENU_PREV;
//      result_msg = "prev";
      result_msg = CMD_PREV;
      break;
  case 0xFF22DD://NEXT
//      result_msg = "test";
      result_msg = CMD_CHECK;
      break;
  case 0xFFC23D://PLAY
//      result_msg = "back";
      result_msg = CMD_NULL;
      break;
  case 0xFF02FD://PREV
      //result_msg = U8X8_MSG_GPIO_MENU_UP;
//      result_msg = "up";
      result_msg = CMD_VOLUP;
      break;
  case 0xFF906F:  //EQ
      //result_msg = U8X8_MSG_GPIO_MENU_NEXT;
//      result_msg = "next";
      result_msg = CMD_NEXT;
      break;
  default:
      break;
  }
endd:
  irrecv.resume(); // Receive the next value
  if (result_msg != CMD_NULL){
    Serial.print("cmd: ");
    Serial.println(result_msg);
  }
  return result_msg;
}

void getSpotifyPlayingSong(){
    Serial.print("Free Heap: ");
    Serial.println(ESP.getFreeHeap());

    Serial.println("getting currently playing song:");
    // Market can be excluded if you want e.g. spotify.getCurrentlyPlaying()
    CurrentlyPlaying currentlyPlaying = spotify.getCurrentlyPlaying();

    printCurrentlyPlayingToSerial(currentlyPlaying);

    requestDueTime = millis() + delayBetweenRequests;
}

void loop()
{
  if (millis() > requestDueTime)
  {
    getSpotifyPlayingSong();
  }

  if (isPlaying && millis() > rollDueTime){
    printCurrentlyPlayingToLCD();
  }

  uint8_t cmd = handleIRRemote();

  switch (cmd){
    case CMD_NULL:
      break;
      
    case CMD_SLEEP:
      break;
      
    case CMD_PLAY:
      if(isPlaying){
        Serial.print("Pausing ...");
        spotify.pause();
        isPlaying = false;
      } else {
        Serial.print("Resuming ...");
        spotify.play();
        isPlaying = true;
      }
      break;
      
    case CMD_NEXT:
      Serial.print("Skipping to next track...");
      if(spotify.nextTrack()){
          Serial.println("done!");
      }
      getSpotifyPlayingSong();
      break;
      
    case CMD_PREV:
      Serial.print("Going to previous track...");
      if(spotify.previousTrack()){
          Serial.println("done!");
      }
      getSpotifyPlayingSong();
      break;
      
    case CMD_VOLUP:
      break;
      
    case CMD_VOLDOWN:
      break;
      
    case CMD_CHECK:
      {
        getSpotifyPlayingSong();
      }
      break;
      
    default:
      break;
  }

  delay(100);
}
