// 简易版墨水屏时钟, for 2.9黑白E029A01-FPC-A1, cszhaoqm 2023

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

// ESP WEB配网例程，参考：https://blog.csdn.net/weixin_44220583/article/details/111562423
#include <EEPROM.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>

// select one and adapt to your mapping, can use full buffer size (full HEIGHT)
// for 2.9寸三色屏, 记得带马扎 tested on 2023/5/13 
//GxEPD2_3C<GxEPD2_290_C90c, GxEPD2_290_C90c::HEIGHT> display(GxEPD2_290_C90c(/*CS=D8*/ SS, /*DC=D3*/ 4, /*RST=D4*/ 2, /*BUSY=D2*/ 5)); // 128x296, SSD1680

// for 2.9黑白E029A01-FPC-A1
GxEPD2_BW<GxEPD2_290, GxEPD2_290::HEIGHT> display(GxEPD2_290(/*CS=D8*/ SS, /*DC=D3*/ 4, /*RST=D4*/ 2, /*BUSY=D2*/ 5)); // 2.9黑白 E029A01-FPC-A1

// note for partial update window and setPartialWindow() method:
// partial update window size and position is on byte boundary in physical x direction
// the size is increased in setPartialWindow() if x or w are not multiple of 8 for even rotation, y or h for odd rotation
// see also comment in GxEPD2_BW.h, GxEPD2_3C.h or GxEPD2_GFX.h for method setPartialWindow()

String glb_SSID;
String glb_PSWD;
String glb_SLOG;

//配网WEB页面代码
const char* page_html = "\
<!DOCTYPE html>\r\n\
<html lang='en'>\r\n\
<head>\r\n\
  <meta charset='UTF-8'>\r\n\
  <meta name='viewport' content='width=device-width, initial-scale=1.0'>\r\n\
  <title>请输入WiFi名称及密码</title>\r\n\
  <h3>WiFi名称及密码请正确输入大小写</h3>\r\n\
</head>\r\n\
<body>\r\n\
  <form name='input' action='/' method='POST'>\r\n\
        WiFi名称:<br>\r\n\
        <input type='text' name='ssid'><br>\r\n\
        WiFi密码:<br>\r\n\
        <input type='text' name='pswd'><br>\r\n\
        <input type='submit' value='保存'>\r\n\
    </form>\r\n\
</body>\r\n\
</html>\r\n\
";

//输入文本页面代码
const char* input_html = "\
<!DOCTYPE html>\r\n\
<html lang='en'>\r\n\
<head>\r\n\
  <meta charset='UTF-8'>\r\n\
  <meta name='viewport' content='width=device-width, initial-scale=1.0'>\r\n\
  <title>请输入显示的文本</title>\r\n\
</head>\r\n\
<body>\r\n\
  <form name='input' action='/input' method='POST'>\r\n\
        Text:<br>\r\n\
        <input type='text' name='slogan'><br>\r\n\
        <input type='submit' value='保存'>\r\n\
    </form>\r\n\
</body>\r\n\
</html>\r\n\
";

const byte DNS_PORT = 53;               // DNS端口号
IPAddress apIP(192, 168, 4, 1);         // esp8266-AP-IP地址
DNSServer dnsServer;                    // 创建dnsServer实例
ESP8266WebServer server(80);            // 创建WebServer
byte glb_byStatusFSM;

#define FSM_START_WIFI_CFG    0         // 启动WiFi配置，创建ESP热点供手机连接
#define FSM_WAIT_WIFI_CONN    1         // 等待手机连接ESP热点并设置SSID/PSWD
#define FSM_RUNNING           2         // 正常运行

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
  display.print(glb_SLOG.c_str());
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

// "ssid,your_ssid,pswd,your_pswd\0";
bool readEepromSsid(String &strSsid, String &strPswd)
{
  char buf[64];                        //LENG_EROM must bigger than 60
  memset(buf, 0, 64);
  EEPROM.begin(64);
  for (int i = 0; i < 60; i++)         //only read 60 bytes
  {
    buf[i] = EEPROM.read(i);
  }

  Serial.print(F("\r\nRead SSID/PSWD from EEPROM: "));
  Serial.println(buf);
  String strBuf = String(buf);
  int iPos1 = strBuf.indexOf("ssid,");
  int iPos2 = strBuf.indexOf("pswd,");

  if ( iPos1 >= 0 && iPos2 >= 0) {
    strSsid = strBuf.substring(iPos1 + 5, iPos2 - 1);
    Serial.print(F("SSID: "));
    Serial.print(strSsid);

    strPswd = strBuf.substring(iPos2 + 5);
    Serial.print(F(" Pswd: "));
    Serial.println(strPswd);
    return true;
  }

  Serial.print(F("SSID and PSWD not found."));
  return false;
}

// "ssid,your_ssid,pswd,your_pswd\0";
bool saveEepromSsid(String &strSsid, String &strPswd)
{
  Serial.print(F("\r\nWrite SSID/PSWD to EEPROM: "));

  char buf[64];                        // must bigger than 60
  memset(buf, 0, 64);
  sprintf(buf, "ssid,%s,pswd,%s", strSsid.c_str(), strPswd.c_str());

  EEPROM.begin(64);
  for (int i = 0; i < 60; i++)        // only write 60 bytes
  {
    EEPROM.write(i, buf[i]);
  }

  Serial.println(buf);
  delay(1000);

  if (!EEPROM.commit()) {
    Serial.println("ERROR! ssid/pswd EEPROM commit failed");
    return false;
  } else {
    Serial.println("EEPROM ssid/pswd successfully committed");

  }
  return true;
}

bool connectNewWifi(void) 
{
  WiFi.mode(WIFI_STA);                          // 切换为STA模式
  WiFi.setAutoConnect(true);                    // 设置自动连接

  WiFi.begin(glb_SSID, glb_PSWD);               // 连接上一次连接成功的wifi
  Serial.println("");
  Serial.print(F("Connecting to WiFi: "));
  Serial.println(glb_SSID);

  int count = 0;
  while (WiFi.status() != WL_CONNECTED) {
    count++;
    delay(500);
    Serial.print(".");
    if (count > 20) {                           // 如果10秒内没有连上，就开启Web配网 可适当调整这个时间
        Serial.println(F("\r\nConnect failed!"));
      return false;                             // 失败退出，防止无限初始化
    }
  }

  Serial.println("");
  if (WiFi.status() == WL_CONNECTED) {          // 如果连接上，就输出IP信息 防止未连接上break后会误输出
    Serial.println(F("WiFi Connected, IP: "));
    Serial.println(WiFi.localIP());             // 打印esp8266的IP地址
    server.stop();
  }

  return true;
}

void handleNotFound()
{
  Serial.println(F("handleNotFound "));
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++)
  {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void handleEmpty() {
  server.send(200, "text/plain", "Welcome to ESP8266 e-ink paper clock.");
}

void handleRoot() {                               //访问主页回调函数
  server.send(200, "text/html", page_html);
}

void handleRootPost() {                           // Post回调函数
  Serial.println("handleRootPost ");
  if (server.hasArg("ssid")) {                    // 判断是否有账号参数
    Serial.print("got ssid: ");
    glb_SSID = String(server.arg("ssid").c_str());// 将账号参数拷贝到glb_SSID中
    Serial.println(glb_SSID);
  } else {                                        // 没有参数
    Serial.println("error, not found ssid");
    server.send(200, "text/html", "<meta charset='UTF-8'>error, not found ssid");// 返回错误页面
    return;
  }

  if (server.hasArg("pswd")) {                    // 密码与账号同理
    Serial.print("got pswd: ");
    glb_PSWD = String(server.arg("pswd").c_str());
    Serial.println(glb_PSWD);
  } else {
    Serial.println("error, not found password");
    server.send(200, "text/html", "<meta charset='UTF-8'>error, not found password");
    return;
  }

  server.send(200, "text/html", "<meta charset='UTF-8'>保存成功!");   
  saveEepromSsid(glb_SSID, glb_PSWD);           // 保存SSID及密码
  
  Serial.println(F("ESP will be restart..."));
  delay(2000);

  ESP.restart();                                // 软复位ESP
}

void handleInput() {                               //访问主页回调函数
  server.send(200, "text/html", input_html);
}

void handleInputPost()
{ // Post回调函数
  Serial.println("handleInputPost ");
  if (server.hasArg("slogan"))
  { // 判断是否input参数
    Serial.print("got slogan: ");
    glb_SLOG = String(server.arg("slogan").c_str()); // 将账号参数拷贝到glb_SSID中
    Serial.println(glb_SLOG);
  }
  else
  { // 没有参数
    Serial.println("error, not found slogan text");
    server.send(200, "text/html", "<meta charset='UTF-8'>error, not found slogan text"); // 返回错误页面
    return;
  }

  server.send(200, "text/html", "<meta charset='UTF-8'>保存成功!");
}

void startWiFiWebConfig(void) 
{                  
  server.stop();         

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));

  if (WiFi.softAP("EspClockConfig")) {
    Serial.println();
    Serial.println(F("EspClockConfig SoftAP is ready!"));
  }

  //初始化WebServer
  server.on("/", HTTP_GET, handleRoot);         // 设置主页回调函数,必须以这种格式去写否则无法强制门户
  server.onNotFound(handleRoot);                // 设置无法响应的http请求的回调函数
  server.on("/", HTTP_POST, handleRootPost);    // 设置Post请求回调函数
  server.begin();                               // 启动WebServer
  Serial.println(F("Web Server is ready!"));
  
  //初始化DNS服务器
  if (dnsServer.start(DNS_PORT, "*", apIP))     // 判断将所有地址映射到esp8266的ip上是否成功
    Serial.println(F("DNS Server is ready!"));
  else
    Serial.println(F("DNS Server failed."));
}
bool glb_ServerFlag;
void setup()
{
  // initialize the pushbutton pin as an input:
  pinMode(0, INPUT);

  Serial.begin(115200);
  Serial.println();
  Serial.println("setup, cszhaoqm 20230513 via PlatformIO");

  display.init(115200, true, 2, false); // USE THIS for Waveshare boards with "clever" reset circuit, 2ms reset pulse
  display.setRotation(1);

  WiFi.hostname("EspClock");  //设置ESP8266主机名
  glb_SLOG = String("Impossible is nothing!");
  Serial.println(glb_SLOG);

  if (!readEepromSsid(glb_SSID, glb_PSWD)) 
  {
    startWiFiWebConfig();
  }
  else 
  {
    connectNewWifi();
  }

  timeClient.begin();                                   // ntp时间服务初始化
  timeClient.setTimeOffset(8*60*60);
  Serial.println("setup done!");
  ESP.wdtEnable(8000);                                  // 使能软件看门狗的触发间隔  

  showFullScreen();
  display.powerOff();
  glb_byStatusFSM = FSM_RUNNING;

  glb_ServerFlag = false;
}

unsigned long lastTimeHeartBeat = 0; // 记录上次心跳时间
void loop()
{
  ESP.wdtFeed(); // 喂狗
  
  // 采用有限状态机
  switch (glb_byStatusFSM)
  {
  case FSM_START_WIFI_CFG:
    startWiFiWebConfig();                 // 启动热点用于WEB配网
    glb_byStatusFSM = FSM_WAIT_WIFI_CONN; // 等待手机连接并设置SSID/PSWD
    break;

  case FSM_WAIT_WIFI_CONN:
    server.handleClient();
    dnsServer.processNextRequest();
    glb_byStatusFSM = FSM_WAIT_WIFI_CONN;
    break;

  case FSM_RUNNING:
    if(!glb_ServerFlag)
    {
      // 初始化WebServer
      server.stop();
      server.on("/", handleEmpty);      
      server.on("/input", HTTP_GET, handleInput);
      server.on("/input", HTTP_POST, handleInputPost); 
      server.onNotFound(handleNotFound);
      server.begin(); // 启动WebServer
      Serial.println(F("Web Server is ready!"));
      glb_ServerFlag = true;
    }

    server.handleClient();  
    if (((millis() - lastTimeHeartBeat) > 60 * 1000))
    {
      if ((gtm_Minute % 10) == 0)
          showFullScreen();
      else
          showPartialTime();
      display.powerOff();

      lastTimeHeartBeat = millis();
    }
    break;

  default:
    break;
  }

  // read the state of the pushbutton value:
  if(digitalRead(0) == LOW)
  {
    delay(3000);
    if(digitalRead(0) == LOW)
    {
      Serial.println(F("Button Pressed, Reconfig WiFi"));
      glb_byStatusFSM = FSM_START_WIFI_CFG;
    } 
  }

  delay(100);
}
