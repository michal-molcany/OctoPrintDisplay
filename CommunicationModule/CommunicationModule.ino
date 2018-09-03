
#include <OctoPrintAPI.h> //This is where the magic happens... shazam!
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <Time.h>         //We will need these two just to do some rough time math on the timestamps we get
#include <TimeLib.h>
#include <Wire.h>

#include <ESP8266WiFi.h>
#include <WiFiClient.h>

const char* ssid = "ERNI_WPA2-PSK";
const char* password = "E4R5N9I9-w2p0a12X";
WiFiClient client;

#define OLED_RESET 0  // GPIO0
Adafruit_SSD1306 OLED(OLED_RESET);
const unsigned char width = 128;

// You only need to set one of the of follwowing:
IPAddress ip(10, 201, 91, 7);                         // Your IP address of your OctoPrint server (inernal or external)

const int octoprint_httpPort = 5000;  //If you are connecting through a router this will work, but you need a random port forwarded to the OctoPrint server from your router. Enter that port here if you are external
const String octoprint_apikey = "4D499A5503A24198AA946EFB74C3AF36"; //See top of file or GIT Readme about getting API key

// Use one of the following:
OctoprintApi api(client, ip, octoprint_httpPort, octoprint_apikey);

const unsigned long api_mtbs = 3000; //mean time between api requests
unsigned long api_lasttime = 0;   //last time api request has been done


void setup () {
  Serial.begin(9600);
  delay(10);
  Wire.begin();
  delay(10);

  OLED.begin();
  OLED.clearDisplay();
  OLED.setTextWrap(false);
  OLED.setTextSize(1);
  OLED.setTextColor(WHITE);
  OLED.setRotation(2);

  // We start by connecting to a WiFi network
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  OLED.setCursor(0, 0);
  OLED.println("Connecting to");
  OLED.println(ssid);
  OLED.display();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    OLED.print(".");
    OLED.display();
  }
  OLED.clearDisplay();
  OLED.setCursor(10, 10);
  OLED.println("WiFi connected");
  OLED.display();

  //if you get here you have connected to the WiFi
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void SendToSlave()
{
  Wire.beginTransmission(8); // transmit to device #8
  String dataString  = String(ssid) + ";" + api.printJob.printerState + ";"
                       + api.printJob.progressCompletion + ";" + api.printJob.estimatedPrintTime + ";"
                       + api.printerStats.printerTool0TempActual + ";" + api.printerStats.printerBedTempActual;


  char dataCharArray[sizeof(dataString)];
  dataString.toCharArray(dataCharArray, sizeof(dataCharArray));

  Wire.write(dataCharArray);        // sends five bytes
  Wire.endTransmission();    // stop transmitting

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

  OLED.setTextSize(1);
  OLED.clearDisplay();
  OLED.setCursor(0, 0);
  OLED.print("State:    ");
  OLED.println(api.printJob.printerState);

  if (api.printJob.progressCompletion <= 0.02)
  {
    OLED.print("Tool temp: ");
    OLED.print(api.printerStats.printerTool0TempActual);
    OLED.println("C");
    OLED.print("Bed temp:  ");
    OLED.print(api.printerStats.printerBedTempActual);
    OLED.println("C");
  }
  else
  {

    OLED.println(buf);
    OLED.print("Complete: ");
    OLED.print(temp_percent);
    OLED.println("%");

    float bar = ((float)(width - 4) / 100) * (int)temp_percent;

    OLED.fillRect(0, 25, bar , 5, WHITE);
    OLED.drawRect(0, 25, width, 5, WHITE);
  }


  OLED.display();
  Serial.println(api.printJob.progressCompletion);
}

void displayNotPrinting(String state, bool showTemperature)
{
  OLED.setTextSize(1);
  OLED.clearDisplay();
  OLED.setCursor(0, 0);
  OLED.println("State:");
  OLED.setTextSize(2);
  OLED.println(state);
  if (showTemperature && api.printerStats.printerBedTempActual > 35)
  {
    OLED.setTextSize(1);
    OLED.print("Bed temp:  ");
    OLED.print(api.printerStats.printerBedTempActual);
    OLED.println("C");
  }

  OLED.display();
}

void loop() {

  if (millis() - api_lasttime > api_mtbs || api_lasttime == 0) { //Check if time has expired to go check OctoPrint
    if (WiFi.status() == WL_CONNECTED) { //Check WiFi connection status
      if (api.getPrintJob())
      { //Get the print job API endpoint
        api.getPrinterStatistics();
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
    SendToSlave();
  }
}
