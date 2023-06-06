// Display Library example for SPI e-paper panels from Dalian Good Display and boards from Waveshare.
// Requires HW SPI and Adafruit_GFX. Caution: the e-paper panels require 3.3V supply AND data lines!
// Display Library based on Demo Example from Good Display: http://www.e-paper-display.com/download_list/downloadcategoryid=34&isMode=false.html
// Author: Jean-Marc Zingg
// Version: see library.properties
// Library: https://github.com/ZinggJM/GxEPD2

// see GxEPD2_wiring_examples.h for wiring suggestions and examples

// base class GxEPD2_GFX can be used to pass references or pointers to the display instance as parameter, uses ~1.2k more code
// enable or disable GxEPD2_GFX base class
#define ENABLE_GxEPD2_GFX 0

// uncomment next line to use class GFX of library GFX_Root instead of Adafruit_GFX
//#include <GFX.h>
// Note: if you use this with ENABLE_GxEPD2_GFX 1:
//       uncomment it in GxEPD2_GFX.h too, or add #include <GFX.h> before any #include <GxEPD2_GFX.h>

#include <ESP8266WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

#include <GxEPD2_BW.h>
#include <GxEPD2_3C.h>   // for 记得带马扎 2.9寸三色屏, tested on 2023/5/13
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold24pt7b.h>


// select the display class and display driver class in the following file (new style):
// note that many of these selections would require paged display to fit in ram
#include "GxEPD2_display_selection_new_style.h"

// select one and adapt to your mapping, can use full buffer size (full HEIGHT)
// for 2.9寸三色屏, 记得带马扎 tested on 2023/5/13 
//GxEPD2_3C<GxEPD2_290_C90c, GxEPD2_290_C90c::HEIGHT> display(GxEPD2_290_C90c(/*CS=D8*/ SS, /*DC=D3*/ 4, /*RST=D4*/ 2, /*BUSY=D2*/ 5)); // 128x296, SSD1680

// for 2.9黑白E029A01-FPC-A1
GxEPD2_BW<GxEPD2_290, GxEPD2_290::HEIGHT> display(GxEPD2_290(/*CS=D8*/ SS, /*DC=D3*/ 4, /*RST=D4*/ 2, /*BUSY=D2*/ 5)); // 2.9黑白 E029A01-FPC-A1

// note for partial update window and setPartialWindow() method:
// partial update window size and position is on byte boundary in physical x direction
// the size is increased in setPartialWindow() if x or w are not multiple of 8 for even rotation, y or h for odd rotation
// see also comment in GxEPD2_BW.h, GxEPD2_3C.h or GxEPD2_GFX.h for method setPartialWindow()

const char* ssid = "yourssid";
const char* pswd = "yourpswd";

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "ntp1.aliyun.com"); // udp，服务器地址，时间偏移量，更新间隔

int gtm_Year;
int gtm_Month;
int gtm_Date;
int gtm_Hour;
int gtm_Minute;
String gtm_weekDay;
String WEEKDAYS[7] = {"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};

void updateTime()
{
  timeClient.update();

  time_t epochTime = timeClient.getEpochTime();
  Serial.print("Epoch Time:");
  Serial.println(epochTime);

  gtm_Hour = timeClient.getHours();
  gtm_Minute = timeClient.getMinutes();
  gtm_weekDay = WEEKDAYS[timeClient.getDay()];

  //Get a time structure
  struct tm *ptm = gmtime ((time_t *)&epochTime); 
  gtm_Date = ptm->tm_mday;
  gtm_Month = ptm->tm_mon+1;
  gtm_Year = ptm->tm_year+1900;
}

void showFullScreen()
{
  updateTime();

  // full window mode is the initial mode, set it anyway
  display.setFullWindow();
  display.fillScreen(GxEPD_WHITE);
  display.setFont(&FreeMonoBold24pt7b);
  display.setTextColor(GxEPD_BLACK);  

  // 1. big Black Time
  char strTime[16]="";
  sprintf(strTime,"%02d:%02d",gtm_Hour,gtm_Minute);

  int16_t tbx, tby; 
  uint16_t tbw, tbh;
  display.getTextBounds(strTime, 0, 0, &tbx, &tby, &tbw, &tbh);
  // center bounding box by transposition of origin:
  uint16_t x = ((display.width() - tbw) / 2) - tbx;
  uint16_t y = ((display.height() - tbh) / 2) - tby;

  display.setCursor(x, y);
  display.print(strTime);

  // 2. small Black Date
  // 将epochTime换算成年月日
  display.setFont(&FreeMonoBold9pt7b);
  display.setTextSize(1);
  display.setTextColor(GxEPD_BLACK);
  display.setCursor(10, 12);     // x, y
  String currentDate = String(gtm_Year) + "/" + String(gtm_Month) + "/" + String(gtm_Date);
  display.print(currentDate);
  
  // 3. small Black Weekday
  display.setCursor(200, 12);  // x, y
  display.print(gtm_weekDay);

  display.setCursor(10, 124);  // x, y
  display.print("Impossible is nothing!");
  // end

  display.drawLine(0, 16, display.width(), 16, GxEPD_BLACK);
  display.drawLine(0, 108, display.width(), 108, GxEPD_BLACK);

  display.display(false);       // full update
}

void showPartialTime()
{
  updateTime();  
  // use asymmetric values for test
  char strTime[16]="";
  sprintf(strTime,"%02d:%02d",gtm_Hour,gtm_Minute);

  display.setFont(&FreeMonoBold24pt7b);
  display.setTextColor(GxEPD_BLACK);
  display.setFullWindow();

  int16_t tbx, tby; 
  uint16_t tbw, tbh;
  display.getTextBounds(strTime, 0, 0, &tbx, &tby, &tbw, &tbh);
  // center bounding box by transposition of origin:
  uint16_t x = ((display.width() - tbw) / 2) - tbx;
  uint16_t y = ((display.height() - tbh) / 2) - tby;

  display.fillRect(0, 24, 296, 80, GxEPD_WHITE);
  display.setCursor(x, y);
  display.print(strTime);
  display.displayWindow(0, 24, 296, 80);
}

void setup_wifi()
{
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, pswd);

  while (WiFi.status() != WL_CONNECTED) 
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.print("WiFi connected, ");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void setup()
{
  Serial.begin(115200);
  Serial.println();
  Serial.println("setup, cszhaoqm 20230513 via PlatformIO");

  display.init(115200, true, 2, false); // USE THIS for Waveshare boards with "clever" reset circuit, 2ms reset pulse
  display.setRotation(1);

  setup_wifi();

  timeClient.begin();                                   // ntp时间服务初始化
  timeClient.setTimeOffset(8*60*60);
  Serial.println("setup done");
  ESP.wdtEnable(8000);                                  // 使能软件看门狗的触发间隔  

  showFullScreen();
  display.powerOff();
}

unsigned long lastTimeHeartBeat = 0; // 记录上次心跳时间
void loop()
{
  ESP.wdtFeed(); // 喂狗
  if (((millis() - lastTimeHeartBeat) > 60 * 1000))
  {
    if ((gtm_Minute % 10) == 0)
      showFullScreen();
    else
      showPartialTime();
    display.powerOff();

    lastTimeHeartBeat = millis();
  }

  delay(2 * 1000);
}
