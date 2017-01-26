/*
MIT License

Copyright (c) 2017 Peter Buelow (goballstate at gmail dot com)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <Time.h>
#include <TimeLib.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiAP.h>
#include <ESP8266WiFiGeneric.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266WiFiScan.h>
#include <ESP8266WiFiSTA.h>
#include <ESP8266WiFiType.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <WiFiServer.h>
#include <WiFiUdp.h>

#include <NtpClientLib.h>
#include <DHT.h>
#include <Adafruit_GFX.h>
#include <gfxfont.h>
#include <Adafruit_SSD1306.h>
#include <FastLED.h>

#include "SunSet.h"

#define UV_LED            D7
#define NUM_LEDS          8
#define DHTTYPE           DHT22
#define ONE_HOUR          (1000*60*60)
#define ONE_MINUTE        (1000*60)
#define CST_OFFSET        -6
#define DST_OFFSET        (CST_OFFSET + 1)
#define TIME_BASE_YEAR    2015

// Arlington Heights, IL, USA
#define LATITUDE          42.058102
#define LONGITUDE         87.984189

// If using software SPI (the default case):
#define OLED_RESET  D0
#define OLED_CLK    D1
#define OLED_MOSI   D2
#define M_DETECT    D8  // Should be D3, testing
#define DHT_22      D4
#define OLED_DC     D5
#define OLED_CS     D6
#define UV_LED      D7
#define LED_DIN     D3  // Should be D8, testing
#define OLED_PW     RX
#define MOIST       A0

Adafruit_SSD1306 display(OLED_MOSI, OLED_CLK, OLED_DC, OLED_RESET, OLED_CS);

CRGB leds[NUM_LEDS];
DHT dht(DHT_22, DHTTYPE);
SunSet sun;
int g_sunposition;

float g_humidity;
float g_temp;
int g_moisture;

const uint8_t _usDSTStart[6] = {8,13,12,11,10, 8};
const uint8_t _usDSTEnd[22]   = {1, 6, 5, 4, 3, 1};

bool isSunrise()
{
  double sunrise_s = sun.calcSunrise() - 30;
  double sunrise_e = sun.calcSunrise() + 30;
  double sunrise = sun.calcSunrise();
  double minsPastMidnight = hour() * 60 + minute();

  if ((minsPastMidnight >= (sunrise - 30)) && (minsPastMidnight <= (sunrise + 30))) {
    g_sunposition = (sunrise_e - sunrise_s) * 4;
    return true;
  }

  return false;
}

bool isSunset()
{
  double sunset_s = sun.calcSunset() - 30;
  double sunset_e = sun.calcSunset() + 30;
  double sunset = sun.calcSunset();
  double minsPastMidnight = hour() * 60 + minute();

  if ((minsPastMidnight >= (sunset - 30)) && (minsPastMidnight <= (sunset + 30))) {
    g_sunposition = (sunset_e - sunset_s) * 4;
    return true;
  }

  return false;
}

bool isDaytime()
{
  double sunrise = sun.calcSunrise();
  double sunset = sun.calcSunset();
  double minsPastMidnight = hour() * 60 + minute();

  if ((minsPastMidnight >= (sunrise + 31)) && (minsPastMidnight < (sunset - 31))) {
    return true;
  }
  return false;
}

int currentTimeZone()
{
  int offset = CST_OFFSET;

    if (month() > 3 && month() < 11) {
        offset = DST_OFFSET;
    }
    else if (month() == 3) {
        if ((day() == _usDSTStart[year() -  TIME_BASE_YEAR]) && hour() >= 2)
            offset = DST_OFFSET;
        else if (day() > _usDSTStart[year() -  TIME_BASE_YEAR])
            offset = DST_OFFSET;
    }
    else if (month() == 11) {
        if ((day() == _usDSTEnd[year() -  TIME_BASE_YEAR]) && hour() <=2)
            offset = DST_OFFSET;
        else if (day() > _usDSTEnd[year() -  TIME_BASE_YEAR])
            offset = CST_OFFSET;
    }

    String debug(String(__FUNCTION__) + ": Current timezone is " + offset);
    Serial.println(debug);
    return offset;
}

void printDisplay()
{
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.print("Temperature: ");
  display.print(g_temp, 1);
  display.setCursor(0, 10);
  display.print("Moisture:    ");
  display.print(g_moisture);
  display.setCursor(0, 20);
  display.print("Humidity:    ");
  display.print(g_humidity, 1);
  display.display();
}

void getEnvironmental()
{
  Serial.println("Checking environment");
  g_humidity = dht.readHumidity();
  g_temp = dht.readTemperature(true);
}

void getSoilMoisture()
{
  digitalWrite(M_DETECT, HIGH);
  delay(10);
  g_moisture = analogRead(MOIST);
  digitalWrite(M_DETECT, LOW);
}

void switchUV(bool s)
{
  if (s)
    digitalWrite(UV_LED, HIGH);
  else
    digitalWrite(UV_LED, LOW);
}

void setup() 
{
  Serial.begin(115200);

  g_sunposition = 0;
  
  pinMode(UV_LED, OUTPUT);                // Setup the UV LED
  pinMode(M_DETECT, OUTPUT);              // Setup the On/Off for the soil moisture sensor
  pinMode(OLED_PW, OUTPUT);               // Make the RX line a normal 3v3 GPIO
  pinMode(DHT_22, INPUT);                 // Setup the 1-wire Temp and humidity

  digitalWrite(UV_LED, HIGH);
  
  FastLED.addLeds<NEOPIXEL, LED_DIN>(leds, NUM_LEDS);
  dht.begin();

  // by default, we'll generate the high voltage from the 3.3v line internally! (neat!)
  display.begin(SSD1306_SWITCHCAPVCC);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,5);
  display.println("Terrorarium");
  display.display();

  WiFi.persistent(false);
  WiFi.begin("LivingRoom", "Motorazr2V8");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  NTP.onNTPSyncEvent([](NTPSyncEvent_t error) {
    if (error) {
    Serial.print("Time Sync error: ");
    if (error == noResponse)
      Serial.println("NTP server not reachable");
    else if (error == invalidAddress)
      Serial.println("Invalid NTP server address");
    }
    else {
      Serial.print("Got NTP time: ");
      Serial.println(NTP.getTimeDateString(NTP.getLastNTPSync()));
      sun.setCurrentDate(year(), month(), day());
      NTP.setTimeZone(currentTimeZone());
    }
  });
  NTP.begin("pool.ntp.org", 1, true);
  NTP.setInterval(3600);
  NTP.setTimeZone(currentTimeZone());

  sun.setPosition(LATITUDE, LONGITUDE, currentTimeZone());
  delay(5000);
  getSoilMoisture();
  getEnvironmental();
}

void loop() 
{
  int sunrise;
  int sunset;
  
  EVERY_N_MILLISECONDS(ONE_HOUR)
  {
    sun.setTZOffset(currentTimeZone());
    getSoilMoisture();
  }

  EVERY_N_MILLISECONDS(ONE_MINUTE)
  {
    getEnvironmental();
  }

  // Update the display
  printDisplay();
  
  if (isSunrise()) {
    for (int i = 0; i < NUM_LEDS; i++) {
      leds[i].r += g_sunposition;
      leds[i].b += g_sunposition;
      leds[i].g = 0;
    }
    FastLED.show();
  }
  else if (isDaytime()) {
    for (int i = 0; i < NUM_LEDS; i++) {
      leds[i].r = 220;
      leds[i].b = 220;
      leds[i].g = 100;
    }
    FastLED.show();
  }
  else if (isSunset()) {
    for (int i = 0; i < NUM_LEDS; i++) {
      leds[i].r -= g_sunposition;
      leds[i].b -= g_sunposition;
      leds[i].g = 0;
    }
    FastLED.show();    
  }
  else {
    for (int i = 0; i < NUM_LEDS; i++) {
      leds[i] = CRGB::Black;
    }
    FastLED.show();
  }
  
  if (hour() > 10 && hour() < 14)
    switchUV(true);
  else
    switchUV(false);
    
  delay(1000);
}
