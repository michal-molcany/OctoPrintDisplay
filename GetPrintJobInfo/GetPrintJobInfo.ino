#include <Arduino.h>
#include <U8g2lib.h>

#include <OctoPrintAPI.h> //This is where the magic happens... shazam!
#include <Wire.h>

#include <Time.h>         //We will need these two just to do some rough time math on the timestamps we get
#include <TimeLib.h>

#include <ESP8266WiFi.h>
#include <WiFiClient.h>

#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include "WiFiManager.h"          //https://github.com/tzapu/WiFiManager

WiFiClient client;

U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R2, SCL, SDA, U8X8_PIN_NONE);   // All Boards without Reset of the Display
const unsigned char width = 128;

// You only need to set one of the of follwowing:
IPAddress ip(10, 201, 91, 7);                         // Your IP address of your OctoPrint server (inernal or external)

const int octoprint_httpPort = 5000;  //If you are connecting through a router this will work, but you need a random port forwarded to the OctoPrint server from your router. Enter that port here if you are external
const String octoprint_apikey = "B948E9EBF9C143C78413B6ACB84BEA5F"; //See top of file or GIT Readme about getting API key

// Use one of the following:
OctoprintApi api(client, ip, octoprint_httpPort, octoprint_apikey);

const unsigned long api_mtbs = 3000; //mean time between api requests
unsigned long api_lasttime = 0;   //last time api request has been done

void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_7x13B_tr);
  u8g2.drawStr(0, 15, "Entered config mode");
  u8g2.setFont(u8g2_font_6x13_tr);
  u8g2.setCursor(0, 30);
  u8g2.print(WiFi.softAPIP());
  u8g2.setCursor(0, 45);
  u8g2.print(myWiFiManager->getConfigPortalSSID());
  u8g2.sendBuffer();

  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
}

void setup () {
  Serial.begin(9600);
  delay(10);
  u8g2.begin();

  u8g2.clearBuffer();
  u8g2.setFontMode(1);  // Transparent
  u8g2.setFontDirection(0);
  u8g2.setFont(u8g2_font_6x13_tr);
  u8g2.drawStr(0, 30, "Connecting...");
  u8g2.sendBuffer();

  // We start by connecting to a WiFi network
  Serial.println();
  Serial.println();
  Serial.print("Connecting");

  WiFiManager wifiManager;
  //reset settings - for testing
  //  wifiManager.resetSettings();

  //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wifiManager.setAPCallback(configModeCallback);

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect()) {
    Serial.println("failed to connect and hit timeout");
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(1000);
  }
  u8g2.clearDisplay();
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_7x13B_tr);
  u8g2.drawStr(0, 30, "Wifi connected to:");
  u8g2.setFont(u8g2_font_6x13_tr);

  u8g2.drawStr(0, 45, WiFi.SSID().c_str());
  u8g2.sendBuffer();

  //if you get here you have connected to the WiFi
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}
void displayPrinting()
{
  //Estimated print time if availble in human readable time HH:MM:SS
  int estrunHours = api.printJob.estimatedPrintTime / 3600;
  int estsecsRemaining = api.printJob.estimatedPrintTime % 3600;
  int estrunMinutes = estsecsRemaining / 60;
  int estrunSeconds = estsecsRemaining % 60;
  char estbuf[31];
  sprintf(estbuf, "Estimate: %02d:%02d:%02d", estrunHours, estrunMinutes, estrunSeconds);
  Serial.println(estbuf);

  //Percentage of current print job complete
  const float temp_percent = floor(api.printJob.progressCompletion * 100) / 100;
  Serial.print("Percentage complete:\t");
  Serial.print(temp_percent);
  Serial.println("%");

  //Print time left (if printing) in human readable time HH:MM:SS
  int runHours = api.printJob.progressPrintTimeLeft / 3600;
  int secsRemaining = api.printJob.progressPrintTimeLeft % 3600;
  int runMinutes = secsRemaining / 60;
  int runSeconds = secsRemaining % 60;
  char buf[31];

  if (runHours == 0 && runMinutes == 0 && runSeconds == 0)
  {
    sprintf(buf, "Left:     Unknown", runHours, runMinutes, runSeconds);
  }
  else
  {
    sprintf(buf, "Left:     %02d:%02d:%02d", runHours, runMinutes, runSeconds);
  }
  Serial.println(buf);
  Serial.println("----------------------------------------");
  Serial.println();

  u8g2.clearBuffer();
  char tempbuf[31];
  u8g2.setFont(u8g2_font_t0_12_tr);
  sprintf(tempbuf, "TT: %.1fC  BT: %.1fC", api.printerStats.printerTool0TempActual, api.printerStats.printerBedTempActual);
  u8g2.drawStr(0, 8, tempbuf);
  u8g2.setFont(u8g2_font_t0_14_tr);
  u8g2.drawStr(0, 21, api.printJob.printerState.c_str());
  u8g2.drawStr(0, 33, buf);
  u8g2.setCursor(0, 45);
  u8g2.print("Complete: ");
  u8g2.print(temp_percent);
  u8g2.println("%");

  float bar = ((float)(width - 4) / 100) * (int)temp_percent;
  u8g2.drawBox(1, 54, bar, 10);
  u8g2.drawFrame(0, 54, width, 10);
  u8g2.sendBuffer();
  Serial.println(api.printJob.progressCompletion);
}

void displayNotPrinting(String state, bool showTemperature)
{
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_t0_12_tr);
  u8g2.drawStr(0, 10 , "State:");
  u8g2.setFont(u8g2_font_t0_22_tr);
  u8g2.drawStr(0, 35, state.c_str());

  if (showTemperature)
  {
    char estbuf[31];
    u8g2.setFont(u8g2_font_t0_12_tr);
    sprintf(estbuf, "TT: %.1fC BT: %.1fC", api.printerStats.printerTool0TempActual, api.printerStats.printerBedTempActual);
    u8g2.drawStr(0, 60, estbuf);
  }
  u8g2.sendBuffer();
}

void loop() {

  if (millis() - api_lasttime > api_mtbs || api_lasttime == 0) { //Check if time has expired to go check OctoPrint
    if (WiFi.status() == WL_CONNECTED) { //Check WiFi connection status
      Serial.println(api.getPrintJob());
      if (api.getPrintJob())
      { //Get the print job API endpoint
        api.getPrinterStatistics();
        Serial.println(api.printJob.printerState);
        if (api.printJob.printerState == "Printing" )
        {
          displayPrinting();
        }
        else
        {
          displayNotPrinting(api.printJob.printerState, true);
        }
      }
      else
      {
        displayNotPrinting("Offline", false);
      }
    }
    api_lasttime = millis();  //Set api_lasttime to current milliseconds run
  }
}
