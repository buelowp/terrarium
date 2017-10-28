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
#define FASTLED_ALLOW_INTERRUPTS 0
#include <Adafruit_SSD1306.h>
#include <Adafruit_DHT.h>
#include <FastLED.h>
#include "SunSet.h"

FASTLED_USING_NAMESPACE;

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
#define OLED_RESET  D2
//#define OLED_SCL    D1
//#define OLED_SDA    D0
#define MD          D4
#define MOIST       A0
#define DHTPIN      D3
//#define OLED_DC     D2
//#define OLED_CS     D4
#define UV_LED      D5
#define LED_DIN     D6


Adafruit_SSD1306 display = Adafruit_SSD1306(OLED_RESET);

DHT dht(DHTPIN, DHTTYPE);
CRGB leds[NUM_LEDS];
SunSet sun;
int g_sunposition;
unsigned int DHTnextSampleTime;	    // Next time we want to start sample
bool bDHTstarted;		    // flag to indicate we started acquisition

float g_humidity;
float g_temp;
int g_moisture;

const uint8_t _usDSTStart[6] = {8,13,12,11,10, 8};
const uint8_t _usDSTEnd[6]   = {1, 6, 5, 4, 3, 1};

int sunrise()
{
  int mins = Time.hour() * 60 + Time.minute();
  return (mins - sun.calcSunrise() - 30) * 4;
}

bool isSunset()
{
  int sunset_p = ((Time.hour() * 60 + Time.minute()) - (sun.calcSunset() - 30));

  if (sunset_p >= 0 && sunset_p < 60) {
    Serial.print("Sunset_p is:");
    Serial.println(sunset_p);
    g_sunposition = (60 - sunset_p) * 4;
    return true;
  }

  return false;
}

bool isDaytime()
{
  bool test = (isSunset() || isSunrise());
  return test;
}

int currentTimeZone()
{
  int offset = CST_OFFSET;

    if (Time.month() > 3 && Time.month() < 11) {
        offset = DST_OFFSET;
    }
    else if (Time.month() == 3) {
        if ((Time.day() == _usDSTStart[Time.year() -  TIME_BASE_YEAR]) && Time.hour() >= 2)
            offset = DST_OFFSET;
        else if (Time.day() > _usDSTStart[Time.year() -  TIME_BASE_YEAR])
            offset = DST_OFFSET;
    }
    else if (Time.month() == 11) {
        if ((Time.day() == _usDSTEnd[Time.year() -  TIME_BASE_YEAR]) && Time.hour() <=2)
            offset = DST_OFFSET;
        else if (Time.day() > _usDSTEnd[Time.year() -  TIME_BASE_YEAR])
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
  g_humidity = dht.getHumidity();
  g_temp = dht.getTempFarenheit();
}

void getSoilMoisture()
{
  digitalWrite(MD, HIGH);
  delay(10);
  g_moisture = analogRead(MOIST);
  digitalWrite(MD, LOW);
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
  pinMode(MD, OUTPUT);              // Setup the On/Off for the soil moisture sensor

  digitalWrite(UV_LED, HIGH);

  FastLED.addLeds<NEOPIXEL, LED_DIN>(leds, NUM_LEDS);

  // by default, we'll generate the high voltage from the 3.3v line internally! (neat!)
  display.begin(SSD1306_SWITCHCAPVCC, 0x3D);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,5);
  display.println("Terrorarium");
  display.display();

  DHTnextSampleTime = 0;  // Start the first sample immediately

  sun.setPosition(LATITUDE, LONGITUDE, currentTimeZone());
  dht.begin();
  delay(5000);
  getSoilMoisture();
  getEnvironmental();
  Serial.println("Startup take 2");
}

void loop()
{
  int sunrise;
  int sunset;

  EVERY_N_MILLISECONDS(ONE_HOUR)
  {
    sun.setTZOffset(currentTimeZone());
//    getSoilMoisture();
  }

  EVERY_N_MILLISECONDS(ONE_MINUTE)
  {
    getSoilMoisture();
    getEnvironmental();
  }

  // Update the display
  printDisplay();

  if (isSunrise()) {
    for (int i = 0; i < NUM_LEDS; i++) {
      leds[i].r = g_sunposition;
      leds[i].b = g_sunposition;
      leds[i].g = (g_sunposition / 2);
    }
    FastLED.show();
  }
  else if (isDaytime()) {
    for (int i = 0; i < NUM_LEDS; i++) {
      leds[i] = CRGB::White;
    }
    FastLED.setBrightness(100);
    FastLED.show();
  }
  else if (isSunset()) {
    for (int i = 0; i < NUM_LEDS; i++) {
      leds[i].r = g_sunposition;
      leds[i].b = g_sunposition;
      leds[i].g = (g_sunposition / 2);
    }
    FastLED.show();
  }
  else {
    for (int i = 0; i < NUM_LEDS; i++) {
      leds[i] = CRGB::Black;
    }
    FastLED.show();
    g_sunposition = 0;
  }

  if (Time.hour() > 9 && Time.hour() < 15)
    switchUV(true);
  else
    switchUV(false);

  delay(1000);
}