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

#define UV_LED    D7
#define NUM_LEDS  8
#define DHTTYPE   DHT22
#define ONE_HOUR  (1000*60*60)
#define CST_OFFSET        -6
#define DST_OFFSET        (CST_OFFSET + 1)
#define TIME_BASE_YEAR      2015

#define LATITUDE          42.058102
#define LONGITUDE         87.984189

// If using software SPI (the default case):
#define OLED_MOSI  D2
#define OLED_CLK   D1
#define OLED_DC    D5
#define OLED_CS    D6
#define OLED_RESET D0
Adafruit_SSD1306 display(OLED_MOSI, OLED_CLK, OLED_DC, OLED_RESET, OLED_CS);

CRGB leds[NUM_LEDS];
DHT dht(D4, DHTTYPE);

float g_humidity;
float g_temp;

const uint8_t _usDSTStart[6] = {8,13,12,11,10, 8};
const uint8_t _usDSTEnd[22]   = {1, 6, 5, 4, 3, 1};

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

void getEnvironmental()
{
  Serial.println("Checking environment");
  g_humidity = dht.readHumidity();
  g_temp = dht.readTemperature(true);
}

void getSoilMoisture()
{
  int moisture;
  digitalWrite(D3, HIGH);
  delay(10);
  moisture = analogRead(A0);
  digitalWrite(D3, LOW);
}

void setup() 
{
  Serial.begin(115200);
  Serial.println("Personal firmware image running");
  pinMode(D7, OUTPUT);    // Setup the UV LED
  pinMode(D3, OUTPUT);    // Setup the On/Off for the soil moisture sensor
  pinMode(D4, INPUT);     // Setup the 1-wire Temp and humidity
  digitalWrite(D3, LOW);
  FastLED.addLeds<WS2812, D8>(leds, NUM_LEDS);
  dht.begin();

  // by default, we'll generate the high voltage from the 3.3v line internally! (neat!)
  display.begin(SSD1306_SWITCHCAPVCC);
  display.display();
  delay(2000);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println("Personal Firmware");
  display.display();

  WiFi.persistent(false);
//  WiFi.mode(WIFI_STA);
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
    }
  });
  NTP.begin("pool.ntp.org", 1, true);
  NTP.setInterval(1800);
  NTP.setTimeZone(currentTimeZone());
}

void loop() 
{
  EVERY_N_MILLISECONDS(ONE_HOUR)
  {
    getEnvironmental();
    getSoilMoisture();
    NTP.setTimeZone(currentTimeZone());
  }

  display.clearDisplay();

  // text display tests
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.print(hour());
  display.print(":");
  display.print(minute());
  display.print(":");
  display.print(second());
  display.setCursor(0, 10);
  display.print(month());
  display.print("-");
  display.print(day());
  display.print("-");
  display.print(year());
  display.setCursor(0, 20);
  display.print("Humidity");
  display.display();

  if (hour() > 6 && hour() < 23) {
    for (int i = 0; i < NUM_LEDS; i++) {
      leds[i].r = 255;
      leds[i].b = 255;
      leds[i].g = 200;
    }
    FastLED.show();
  }
  if (hour() > 9 && hour() < 15) {
    digitalWrite(UV_LED, HIGH);
  }
  delay(1000);
}
