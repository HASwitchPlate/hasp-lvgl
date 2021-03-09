/* MIT License - Copyright (c) 2019-2021 Francis Van Roie
   For full license information read the LICENSE file in the project folder */

//#include "webServer.h"
#include "ArduinoJson.h"
#include "ArduinoLog.h"
#include "lvgl.h"

#if defined(ARDUINO_ARCH_ESP32)
#include "Update.h"
#endif

#include "hasp_conf.h"
#include "dev/device.h"

#include "hasp_gui.h"
#include "hal/hasp_hal.h"
#include "hasp_debug.h"
#include "hasp_config.h"

#include "hasp/hasp_utilities.h"
#include "hasp/hasp_dispatch.h"
#include "hasp/hasp.h"

#if HASP_USE_HTTP > 0

#if defined(ARDUINO_ARCH_ESP8266) || defined(ARDUINO_ARCH_ESP32)
File fsUploadFile;
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
bool webServerStarted = false;

// bool httpEnable       = true;
// uint16_t httpPort     = 80;
// char httpUser[32]     = "";
// char httpPassword[32] = "";
hasp_http_config_t http_config;

#define HTTP_PAGE_SIZE (6 * 256)

#if defined(STM32F4xx) && HASP_USE_ETHERNET > 0
#include <EthernetWebServer_STM32.h>
EthernetWebServer webServer(80);
#endif

#if defined(STM32F4xx) && HASP_USE_WIFI > 0
#include <EthernetWebServer_STM32.h>
// #include <WiFi.h>
EthernetWebServer webServer(80);
#endif

#if defined(ARDUINO_ARCH_ESP8266)
#include "StringStream.h"
#include <ESP8266WebServer.h>
#include <detail/mimetable.h>
ESP8266WebServer webServer(80);
#endif

#if defined(ARDUINO_ARCH_ESP32)
#include <WebServer.h>
#include <detail/mimetable.h>
WebServer webServer(80);
#endif // ESP32

HTTPUpload* upload;

static const char HTTP_MENU_BUTTON[] PROGMEM =
    "<p><form method='get' action='%s'><button type='submit'>%s</button></form></p>";

const char MAIN_MENU_BUTTON[] PROGMEM =
    "</p><p><form method='get' action='/'><button type='submit'>" D_HTTP_MAIN_MENU "</button></form>";
const char MIT_LICENSE[] PROGMEM = "</br>MIT License</p>";

const char HTTP_DOCTYPE[] PROGMEM =
    "<!DOCTYPE html><html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width,initial-scale=1,"
    "user-scalable=no\"/>";
const char HTTP_META_GO_BACK[] PROGMEM = "<meta http-equiv='refresh' content='15;url=/'/>";
const char HTTP_HEADER[] PROGMEM       = "<title>%s</title>";
const char HTTP_STYLE[] PROGMEM =
    "<style>"
    "body,.c{text-align:center;}"
    "div,input{padding:5px;font-size:1em;}"
    "input{width:90%;}"
    "input[type=checkbox],input[type=radio]{width:1em;}"
    "input:invalid{border:1px solid red;}"
    //"#hue{width:100%;}"
    "body{font-family:verdana;width:60%;margin:auto;}"
    "button{border:0;border-radius:0.6rem;background-color:#0af;color:#eee;line-height:2.4rem;font-size:1.2rem;"
    "width:100%;}"
    //".q{float:right;width:64px;text-align:right;}"
    ".red{background-color:#f33;}"
    // ".button3{background-color:#f44336;}"
    // ".button4{background-color:#e7e7e7;color:black;}"
    // ".button5{background-color:#555555;}"
    // ".button6{background-color:#4CAF50;}"
    "</style>";
const char HTTP_SCRIPT[] PROGMEM = "<script>function "
                                   "c(l){document.getElementById('s').value=l.innerText||l.textContent;document."
                                   "getElementById('p').focus();}</script>";
const char HTTP_HEADER_END[] PROGMEM =
    "</head><body><div style='text-align:left;display:inline-block;min-width:260px;'>";
const char HTTP_END[] PROGMEM = "<div style='text-align:right;font-size:11px;'><hr/><a href='/about' "
                                "style='color:#aaa;'>HASP ";
const char HTTP_FOOTER[] PROGMEM = " by Francis Van Roie</div></body></html>";

////////////////////////////////////////////////////////////////////////////////////////////////////

// URL for auto-update "version.json"
// const char UPDATE_URL[] PROGMEM = "http://haswitchplate.com/update/version.json";
// // Default link to compiled Arduino firmware image
// String espFirmwareUrl = "http://haswitchplate.com/update/HASwitchPlate.ino.d1_mini.bin";
// // Default link to compiled Nextion firmware images
// String lcdFirmwareUrl = "http://haswitchplate.com/update/HASwitchPlate.tft";

// #if HASP_USE_MQTT > 0
// extern char mqttNodeName[16];
// #else
// char mqttNodeName[3] = "na";
// #endif

////////////////////////////////////////////////////////////////////////////////////////////////////
String getOption(int value, String label, bool selected)
{
    char buffer[128];
    snprintf_P(buffer, sizeof(buffer), PSTR("<option value='%d'%s>%s</option>"), value,
               (selected ? PSTR(" selected") : ""), label.c_str());
    return buffer;
}

String getOption(String value, String label, bool selected)
{
    char buffer[128];
    snprintf_P(buffer, sizeof(buffer), PSTR("<option value='%s'%s>%s</option>"), value.c_str(),
               (selected ? PSTR(" selected") : ""), label.c_str());
    return buffer;
}

static void add_gpio_select_option(String& str, uint8_t gpio, uint8_t bcklpin)
{
    char buffer[10];
    snprintf_P(buffer, sizeof(buffer), PSTR("GPIO %d"), gpio);
    str += getOption(gpio, buffer, bcklpin == gpio);
}

static void add_button(String& str, const __FlashStringHelper* label, const __FlashStringHelper* extra)
{
    str += F("<button type='submit' ");
    str += extra;
    str += F(">");
    str += label;
    str += F("</button>");
}

static void close_form(String& str)
{
    str += F("</form></p>");
}

static void add_form_button(String& str, const __FlashStringHelper* label, const __FlashStringHelper* action,
                            const __FlashStringHelper* extra)
{
    str += F("<p><form method='get' action='");
    str += action;
    str += F("'>");
    add_button(str, label, extra);
    close_form(str);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void webHandleHaspConfig();

// static inline char* haspDevice.get_hostname()
// {
//     return mqttNodeName;
// }

////////////////////////////////////////////////////////////////////////////////////////////////////
bool httpIsAuthenticated(const __FlashStringHelper* fstr_page)
{
    if(http_config.password[0] != '\0') { // Request HTTP auth if httpPassword is set
        if(!webServer.authenticate(http_config.user, http_config.password)) {
            webServer.requestAuthentication();
            return false;
        }
    }

#if defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_ESP8266)
    LOG_TRACE(TAG_HTTP, F("Sending %S page to client connected from: %s"), fstr_page,
              webServer.client().remoteIP().toString().c_str());
#else
    // LOG_INFO(TAG_HTTP,F("Sending %s page to client connected from: %s"), page,
    //             String(webServer.client().remoteIP()).c_str());
#endif

    return true;
}

void webSendFooter()
{
    char buffer[16];
    haspGetVersion(buffer, sizeof(buffer));

#if defined(STM32F4xx)
    webServer.sendContent(HTTP_END);
    webServer.sendContent(buffer);
    webServer.sendContent(HTTP_FOOTER);
#else
    webServer.sendContent_P(HTTP_END);
    webServer.sendContent(buffer);
    webServer.sendContent_P(HTTP_FOOTER);
#endif
}

void webSendPage(const char* nodename, uint32_t httpdatalength, bool gohome = false)
{
    {
        char buffer[64];
        haspGetVersion(buffer, sizeof(buffer));

        /* Calculate Content Length upfront */
        uint16_t contentLength = strlen(buffer); // verion length
        contentLength += sizeof(HTTP_DOCTYPE) - 1;
        contentLength += sizeof(HTTP_HEADER) - 1 - 2 + strlen(nodename); // -2 for %s
        contentLength += sizeof(HTTP_SCRIPT) - 1;
        contentLength += sizeof(HTTP_STYLE) - 1;
        // contentLength += sizeof(HASP_STYLE) - 1;
        if(gohome) contentLength += sizeof(HTTP_META_GO_BACK) - 1;
        contentLength += sizeof(HTTP_HEADER_END) - 1;
        contentLength += sizeof(HTTP_END) - 1;
        contentLength += sizeof(HTTP_FOOTER) - 1;

        if(httpdatalength > HTTP_PAGE_SIZE) {
            LOG_WARNING(TAG_HTTP, F("Sending page with %u static and %u dynamic bytes"), contentLength, httpdatalength);
        }

        webServer.setContentLength(contentLength + httpdatalength);
#if defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_ESP8266)
        webServer.send_P(200, PSTR("text/html"), HTTP_DOCTYPE); // 122
#else
        webServer.send(200, ("text/html"), HTTP_DOCTYPE); // 122
#endif

        snprintf_P(buffer, sizeof(buffer), HTTP_HEADER, nodename);
        webServer.sendContent(buffer); // 17-2+len
    }

#if defined(STM32F4xx)
    webServer.sendContent(HTTP_SCRIPT); // 131
    webServer.sendContent(HTTP_STYLE);  // 487
    // webServer.sendContent(HASP_STYLE);                   // 145
    if(gohome) webServer.sendContent(HTTP_META_GO_BACK); // 47
    webServer.sendContent(HTTP_HEADER_END);              // 80
#else
    webServer.sendContent_P(HTTP_SCRIPT);                 // 131
    webServer.sendContent_P(HTTP_STYLE);                  // 487
    // webServer.sendContent_P(HASP_STYLE);                   // 145
    if(gohome) webServer.sendContent_P(HTTP_META_GO_BACK); // 47
    webServer.sendContent_P(HTTP_HEADER_END);              // 80
#endif
}

void saveConfig()
{
    if(webServer.method() == HTTP_POST) {
        if(webServer.hasArg(PSTR("save"))) {
            String save = webServer.arg(PSTR("save"));

            StaticJsonDocument<256> settings;
            for(int i = 0; i < webServer.args(); i++) settings[webServer.argName(i)] = webServer.arg(i);

            if(save == String(PSTR("hasp"))) {
                haspSetConfig(settings.as<JsonObject>());

#if HASP_USE_MQTT > 0
            } else if(save == String(PSTR("mqtt"))) {
                mqttSetConfig(settings.as<JsonObject>());
#endif

            } else if(save == String(PSTR("gui"))) {
                settings[FPSTR(FP_GUI_POINTER)] = webServer.hasArg(PSTR("cur"));
                settings[FPSTR(FP_GUI_INVERT)]  = webServer.hasArg(PSTR("inv"));
                guiSetConfig(settings.as<JsonObject>());

            } else if(save == String(PSTR("debug"))) {
                debugSetConfig(settings.as<JsonObject>());

            } else if(save == String(PSTR("http"))) {
                httpSetConfig(settings.as<JsonObject>());

                // Password might have changed
                if(!httpIsAuthenticated(F("config"))) return;

#if HASP_USE_WIFI > 0
            } else if(save == String(PSTR("wifi"))) {
                wifiSetConfig(settings.as<JsonObject>());
#endif
            }
        }
    }
}

void webHandleRoot()
{
    if(!httpIsAuthenticated(F("root"))) return;

    saveConfig();
    {
        String httpMessage((char*)0);
        httpMessage.reserve(HTTP_PAGE_SIZE);
        httpMessage += F("<h1>");
        httpMessage += haspDevice.get_hostname();
        httpMessage += F("</h1><hr>");

        httpMessage += F("<p><form method='get' action='/config/hasp'><button type='submit'>" D_HTTP_HASP_DESIGN
                         "</button></form></p>");

        httpMessage += F("<p><form method='get' action='screenshot'><button type='submit'>" D_HTTP_SCREENSHOT
                         "</button></form></p>");
        httpMessage +=
            F("<p><form method='get' action='info'><button type='submit'>" D_HTTP_INFORMATION "</button></form></p>");
        add_form_button(httpMessage, F(D_HTTP_CONFIGURATION), F("/config"), F(""));
        // httpMessage += F("<p><form method='get' action='config'><button type='submit'>" D_HTTP_CONFIGURATION
        //                  "</button></form></p>");

        httpMessage += F("<p><form method='get' action='firmware'><button type='submit'>" D_HTTP_FIRMWARE_UPGRADE
                         "</button></form></p>");

#if HASP_USE_SPIFFS > 0 || HASP_USE_LITTLEFS > 0
        if(HASP_FS.exists(F("/edit.htm.gz"))) {
            httpMessage +=
                F("<p><form method='get' action='edit.htm.gz?path=/'><button type='submit'>" D_HTTP_FILE_BROWSER
                  "</button></form></p>");
        }
#endif

        httpMessage += F("<p><form method='get' action='reboot'><button class='red' type='submit'>" D_HTTP_REBOOT
                         "</button></form></p>");

        webSendPage(haspDevice.get_hostname(), httpMessage.length(), false);
        webServer.sendContent(httpMessage);
    }
    // httpMessage.clear();
    webSendFooter();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void httpHandleReboot()
{ // http://plate01/reboot
    if(!httpIsAuthenticated(F("reboot"))) return;

    {
        String httpMessage((char*)0);
        httpMessage.reserve(HTTP_PAGE_SIZE);
        httpMessage += F("<h1>");
        httpMessage += haspDevice.get_hostname();
        httpMessage += F("</h1><hr>");
        httpMessage = F(D_DISPATCH_REBOOT);

        webSendPage(haspDevice.get_hostname(), httpMessage.length(), true);
        webServer.sendContent(httpMessage);
    }
    // httpMessage.clear();
    webSendFooter();

    delay(200);
    dispatch_reboot(true);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void webHandleScreenshot()
{ // http://plate01/screenshot
    if(!httpIsAuthenticated(F("screenshot"))) return;

    if(webServer.hasArg(F("a"))) {
        if(webServer.arg(F("a")) == F("next")) {
            dispatch_page_next();
        } else if(webServer.arg(F("a")) == F("prev")) {
            dispatch_page_prev();
        }
    }

    if(webServer.hasArg(F("q"))) {
        lv_disp_t* disp = lv_disp_get_default();
        webServer.setContentLength(122 + disp->driver.hor_res * disp->driver.ver_res * sizeof(lv_color_t));
        webServer.send_P(200, PSTR("image/bmp"), "");
        guiTakeScreenshot();
        webServer.client().stop();

    } else {
        {
            String httpMessage((char*)0);
            httpMessage.reserve(HTTP_PAGE_SIZE);
            httpMessage += F("<h1>");
            httpMessage += haspDevice.get_hostname();
            httpMessage += F("</h1><hr>");

            httpMessage +=
                F("<script>function aref(t){setTimeout(function() {ref('');}, t*1000)} function ref(a){ var t=new "
                  "Date().getTime();document.getElementById('bmp').src='?a='+a+'&q='+t;return false;}</script>");
            httpMessage += F("<p class='c'><img id='bmp' src='?q=0'");

            // Automatic refresh
            httpMessage += F(" onload=\"aref(5)\" onerror=\"aref(5)\"/></p>");

            httpMessage += F("<p><form method='get' onsubmit=\"return ref('')\"><button type='submit'>" D_HTTP_REFRESH
                             "</button></form></p>");
            httpMessage +=
                F("<p><form method='get' onsubmit=\"return ref('prev');\"><button type='submit'>" D_HTTP_PREV_PAGE
                  "</button></form></p>");
            httpMessage +=
                F("<p><form method='get' onsubmit=\"return ref('next');\"><button type='submit'>" D_HTTP_NEXT_PAGE
                  "</button></form></p>");
            httpMessage += FPSTR(MAIN_MENU_BUTTON);

            webSendPage(haspDevice.get_hostname(), httpMessage.length(), false);
            webServer.sendContent(httpMessage);
        }
        // httpMessage.clear();
        webSendFooter();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void webHandleAbout()
{ // http://plate01/about
    if(!httpIsAuthenticated(F("about"))) return;

    {
        String httpMessage((char*)0);
        httpMessage.reserve(HTTP_PAGE_SIZE);

        httpMessage += F("<p><h3>HASP OpenHardware edition</h3>Copyright&copy; 2020 Francis Van Roie ");
        httpMessage += FPSTR(MIT_LICENSE);
        httpMessage += F("<p>Based on the previous work of the following open source developers.</p><hr>");
        httpMessage += F("<p><h3>HASwitchPlate</h3>Copyright&copy; 2019 Allen Derusha allen@derusha.org</b>");
        httpMessage += FPSTR(MIT_LICENSE);
        httpMessage +=
            F("<p><h3>LittlevGL</h3>Copyright&copy; 2016 G&aacute;bor Kiss-V&aacute;mosi</br>Copyright&copy; 2019 "
              "LittlevGL");
        httpMessage += FPSTR(MIT_LICENSE);
        httpMessage += F("<p><h3>zi Font Engine</h3>Copyright&copy; 2020 Francis Van Roie");
        httpMessage += FPSTR(MIT_LICENSE);
        httpMessage += F("<p><h3>TFT_eSPI Library</h3>Copyright&copy; 2020 Bodmer (https://github.com/Bodmer) All "
                         "rights reserved.</br>FreeBSD License</p>");
        httpMessage +=
            F("<p><i>includes parts from the <b>Adafruit_GFX library</b></br>Copyright&copy; 2012 Adafruit Industries. "
              "All rights reserved</br>BSD License</i></p>");
        httpMessage += F("<p><h3>ArduinoJson</h3>Copyright&copy; 2014-2020 Benoit BLANCHON");
        httpMessage += FPSTR(MIT_LICENSE);
        httpMessage += F("<p><h3>PubSubClient</h3>Copyright&copy; 2008-2015 Nicholas O'Leary");
        httpMessage += FPSTR(MIT_LICENSE);
        httpMessage +=
            F("<p><h3>ArduinoLog</h3>Copyright&copy; 2017,2018 Thijs Elenbaas, MrRobot62, rahuldeo2047, NOX73, "
              "dhylands, Josha blemasle, mfalkvidd");
        httpMessage += FPSTR(MIT_LICENSE);
#if HASP_USE_SYSLOG > 0
        // Replaced with WiFiUDP client
        // httpMessage += F("<p><h3>Syslog</h3>Copyright&copy; 2016 Martin Sloup");
        // httpMessage += FPSTR(MIT_LICENSE);
#endif
#if HASP_USE_QRCODE > 0
        httpMessage += F("<p><h3>QR Code generator</h3>Copyright&copy; Project Nayuki");
        httpMessage += FPSTR(MIT_LICENSE);
#endif
        httpMessage += F("<p><h3>AceButton</h3>Copyright&copy; 2018 Brian T. Park");
        httpMessage += FPSTR(MIT_LICENSE);

        httpMessage += FPSTR(MAIN_MENU_BUTTON);

        webSendPage(haspDevice.get_hostname(), httpMessage.length(), false);
        webServer.sendContent(httpMessage);
    }
    // httpMessage.clear();
    webSendFooter();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void webHandleInfo()
{ // http://plate01/
    if(!httpIsAuthenticated(F("info"))) return;

    {
        char size_buf[32];
        String httpMessage((char*)0);
        httpMessage.reserve(HTTP_PAGE_SIZE);
        httpMessage += F("<h1>");
        httpMessage += haspDevice.get_hostname();
        httpMessage += F("</h1><hr>");

        /* HASP Stats */
        httpMessage += F("<b>HASP Version: </b>");
        {
            char version[32];
            haspGetVersion(version, sizeof(version));
            httpMessage += version;
        }
        httpMessage += F("<br/><b>Build DateTime: </b>");
        httpMessage += __DATE__;
        httpMessage += F(" ");
        httpMessage += __TIME__;
        httpMessage += F(" CET<br/><b>Uptime: </b>");

        unsigned long time = millis() / 1000;
        uint16_t day       = time / 86400;
        time               = time % 86400;
        uint8_t hour       = time / 3600;
        time               = time % 3600;
        uint8_t min        = time / 60;
        time               = time % 60;
        uint8_t sec        = time;

        if(day > 0) {
            httpMessage += String(day);
            httpMessage += F("d ");
        }
        if(day > 0 || hour > 0) {
            httpMessage += String(hour);
            httpMessage += F("h ");
        }
        if(day > 0 || hour > 0 || min > 0) {
            httpMessage += String(min);
            httpMessage += F("m ");
        }
        httpMessage += String(sec);
        httpMessage += F("s");

        httpMessage += F("<br/><b>Free Memory: </b>");
        Utilities::format_bytes(haspDevice.get_free_heap(), size_buf, sizeof(size_buf));
        httpMessage += size_buf;
        httpMessage += F("<br/><b>Memory Fragmentation: </b>");
        httpMessage += String(haspDevice.get_heap_fragmentation());

#if ARDUINO_ARCH_ESP32
        if(psramFound()) {
            httpMessage += F("<br/><b>Free PSRam: </b>");
            Utilities::format_bytes(ESP.getFreePsram(), size_buf, sizeof(size_buf));
            httpMessage += size_buf;
            httpMessage += F("<br/><b>PSRam Size: </b>");
            Utilities::format_bytes(ESP.getPsramSize(), size_buf, sizeof(size_buf));
            httpMessage += size_buf;
        }
#endif

        /* LVGL Stats */
        lv_mem_monitor_t mem_mon;
        lv_mem_monitor(&mem_mon);
        httpMessage += F("</p><p><b>LVGL Memory: </b>");
        Utilities::format_bytes(mem_mon.total_size, size_buf, sizeof(size_buf));
        httpMessage += size_buf;
        httpMessage += F("<br/><b>LVGL Free: </b>");
        Utilities::format_bytes(mem_mon.free_size, size_buf, sizeof(size_buf));
        httpMessage += size_buf;
        httpMessage += F("<br/><b>LVGL Fragmentation: </b>");
        httpMessage += mem_mon.frag_pct;

        // httpMessage += F("<br/><b>LCD Model: </b>")) + String(LV_HASP_HOR_RES_MAX) + " x " +
        // String(LV_HASP_VER_RES_MAX); httpMessage += F("<br/><b>LCD Version: </b>")) +
        // String(lcdVersion);
        httpMessage += F("</p/><p><b>LCD Active Page: </b>");
        httpMessage += String(haspGetPage());

        /* Wifi Stats */
#if HASP_USE_WIFI > 0
        httpMessage += F("</p/><p><b>SSID: </b>");
        httpMessage += String(WiFi.SSID());
        httpMessage += F("</br><b>Signal Strength: </b>");

        int8_t rssi = WiFi.RSSI();
        httpMessage += String(rssi);
        httpMessage += F("dBm (");

        if(rssi >= -50) {
            httpMessage += F("Excellent)");
        } else if(rssi >= -60) {
            httpMessage += F("Good)");
        } else if(rssi >= -70) {
            httpMessage += F("Fair)");
        } else if(rssi >= -80) {
            httpMessage += F("Weak)");
        } else {
            httpMessage += F("Very Bad)");
        }
#if defined(STM32F4xx)
        byte mac[6];
        WiFi.macAddress(mac);
        char macAddress[16];
        snprintf_P(macAddress, sizeof(macAddress), PSTR("%02x%02x%02x"), mac[0], mac[1], mac[2], mac[3], mac[4],
                   mac[5]);
        httpMessage += F("</br><b>IP Address: </b>");
        httpMessage += String(WiFi.localIP());
        httpMessage += F("</br><b>Gateway: </b>");
        httpMessage += String(WiFi.gatewayIP());
        httpMessage += F("</br><b>MAC Address: </b>");
        httpMessage += String(macAddress);
#else
        httpMessage += F("</br><b>IP Address: </b>");
        httpMessage += String(WiFi.localIP().toString());
        httpMessage += F("</br><b>Gateway: </b>");
        httpMessage += String(WiFi.gatewayIP().toString());
        httpMessage += F("</br><b>DNS Server: </b>");
        httpMessage += String(WiFi.dnsIP().toString());
        httpMessage += F("</br><b>MAC Address: </b>");
        httpMessage += String(WiFi.macAddress());
#endif
#endif
#if HASP_USE_ETHERNET > 0
#if defined(ARDUINO_ARCH_ESP32)
        httpMessage += F("</p/><p><b>Ethernet: </b>");
        httpMessage += String(ETH.linkSpeed());
        httpMessage += F(" Mbps");
        if(ETH.fullDuplex()) {
            httpMessage += F(" FULL_DUPLEX");
        }
        httpMessage += F("</br><b>IP Address: </b>");
        httpMessage += String(ETH.localIP().toString());
        httpMessage += F("</br><b>Gateway: </b>");
        httpMessage += String(ETH.gatewayIP().toString());
        httpMessage += F("</br><b>DNS Server: </b>");
        httpMessage += String(ETH.dnsIP().toString());
        httpMessage += F("</br><b>MAC Address: </b>");
        httpMessage += String(ETH.macAddress());
#endif
#endif
/* Mqtt Stats */
#if HASP_USE_MQTT > 0
        httpMessage += F("</p/><p><b>MQTT Status: </b>");
        if(mqttIsConnected()) { // Check MQTT connection
            httpMessage += F("Connected");
        } else {
            httpMessage += F("<font color='red'><b>Disconnected</b></font>, return code: ");
            //     +String(mqttClient.returnCode());
        }
        httpMessage += F("<br/><b>MQTT ClientID: </b>");

        {
            char mqttClientId[64];
            String mac = halGetMacAddress(3, "");
            mac.toLowerCase();
            snprintf_P(mqttClientId, sizeof(mqttClientId), PSTR("%s-%s"), haspDevice.get_hostname(), mac.c_str());
            httpMessage += mqttClientId;
        }

#endif // MQTT

        /* ESP Stats */
        httpMessage += F("</p/><p><b>MCU Model: </b>");
        httpMessage += halGetChipModel();
        httpMessage += F("<br/><b>CPU Frequency: </b>");
        httpMessage += String(haspDevice.get_cpu_frequency());
        httpMessage += F("MHz");

#if defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_ESP8266)
        httpMessage += F("<br/><b>Flash Chip Size: </b>");
        Utilities::format_bytes(ESP.getFlashChipSize(), size_buf, sizeof(size_buf));
        httpMessage += size_buf;

        httpMessage += F("</br><b>Program Size: </b>");
        Utilities::format_bytes(ESP.getSketchSize(), size_buf, sizeof(size_buf));
        httpMessage += size_buf;

        httpMessage += F("<br/><b>Free Program Space: </b>");
        Utilities::format_bytes(ESP.getFreeSketchSpace(), size_buf, sizeof(size_buf));
        httpMessage += size_buf;
#endif

        //#if defined(ARDUINO_ARCH_ESP32)
        //        httpMessage += F("<br/><b>ESP SDK version: </b>");
        //        httpMessage += String(ESP.getSdkVersion());
        //#else
        httpMessage += F("<br/><b>Core version: </b>");
        httpMessage += halGetCoreVersion();
        //#endif
        httpMessage += F("<br/><b>Last Reset: </b>");
        httpMessage += halGetResetInfo();

        httpMessage += FPSTR(MAIN_MENU_BUTTON);

        webSendPage(haspDevice.get_hostname(), httpMessage.length(), false);
        webServer.sendContent(httpMessage);
    }
    // httpMessage.clear();
    webSendFooter();
}

// String getContentType(String filename)
// {
//     if(webServer.hasArg(F("download"))) {
//         return F("application/octet-stream");
//     } else if(filename.endsWith(F(".htm")) || filename.endsWith(F(".html"))) {
//         return F("text/html");
//     } else if(filename.endsWith(F(".css"))) {
//         return F("text/css");
//     } else if(filename.endsWith(F(".js"))) {
//         return F("application/javascript");
//     } else if(filename.endsWith(F(".png"))) {
//         return F("image/png");
//     } else if(filename.endsWith(F(".gif"))) {
//         return F("image/gif");
//     } else if(filename.endsWith(F(".jpg"))) {
//         return F("image/jpeg");
//     } else if(filename.endsWith(F(".ico"))) {
//         return F("image/x-icon");
//     } else if(filename.endsWith(F(".xml"))) {
//         return F("text/xml");
//     } else if(filename.endsWith(F(".pdf"))) {
//         return F("application/x-pdf");
//     } else if(filename.endsWith(F(".zip"))) {
//         return F("application/x-zip");
//     } else if(filename.endsWith(F(".gz"))) {
//         return F("application/x-gzip");
//     }
//     return F("text/plain");
// }

static String getContentType(const String& path)
{
    char buff[sizeof(mime::mimeTable[0].mimeType)];
    // Check all entries but last one for match, return if found
    for(size_t i = 0; i < sizeof(mime::mimeTable) / sizeof(mime::mimeTable[0]) - 1; i++) {
        strcpy_P(buff, mime::mimeTable[i].endsWith);
        if(path.endsWith(buff)) {
            strcpy_P(buff, mime::mimeTable[i].mimeType);
            return String(buff);
        }
    }
    // Fall-through and just return default type
    strcpy_P(buff, mime::mimeTable[sizeof(mime::mimeTable) / sizeof(mime::mimeTable[0]) - 1].mimeType);
    return String(buff);
}

/* String urldecode(String str)
{
    String encodedString = "";
    char c;
    for(unsigned int i = 0; i < str.length(); i++) {
        c = str.charAt(i);
        if(c == '+') {
            encodedString += ' ';
        } else if(c == '%') {
            // char buffer[3];
            char buffer[128];
            i++;
            buffer[0] = str.charAt(i);
            i++;
            buffer[1] = str.charAt(i);
            buffer[2] = '\0';
            c         = (char)strtol((const char *)&buffer, NULL, 16);
            encodedString += c;
        } else {
            encodedString += c;
        }
        yield();
    }
    return encodedString;
} */

static unsigned long htppLastLoopTime = 0;
void webUploadProgress()
{
    long t = webServer.header("Content-Length").toInt();
    if(millis() - htppLastLoopTime >= 1250) {
        LOG_VERBOSE(TAG_HTTP, F(D_BULLET "Uploaded %u / %d bytes"), upload->totalSize + upload->currentSize, t);
        htppLastLoopTime = millis();
    }
    if(t > 0) t = (upload->totalSize + upload->currentSize) * 100 / t;
    haspProgressVal(t);
}

#if defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_ESP8266)
static inline void webUpdatePrintError()
{
#if defined(ARDUINO_ARCH_ESP8266)
    String output((char*)0);
    output.reserve(128);
    StringStream stream((String&)output);
    Update.printError(stream); // ESP8266 only has printError()
    LOG_ERROR(TAG_HTTP, output.c_str());
    haspProgressMsg(output.c_str());
#elif defined(ARDUINO_ARCH_ESP32)
    LOG_ERROR(TAG_HTTP, Update.errorString()); // ESP32 has errorString()
    haspProgressMsg(Update.errorString());
#endif
}

void webUpdateReboot()
{
    LOG_INFO(TAG_HTTP, F("Update Success: %u bytes received. Rebooting..."), upload->totalSize);

    {
        String httpMessage((char*)0);
        httpMessage.reserve(HTTP_PAGE_SIZE);
        httpMessage += F("<h1>");
        httpMessage += haspDevice.get_hostname();
        httpMessage += F("</h1><hr>");
        httpMessage += F("<b>Upload complete. Rebooting device, please wait...</b>");

        webSendPage(haspDevice.get_hostname(), httpMessage.length(), true);
        webServer.sendContent(httpMessage);
    }
    // httpMessage.clear();
    webSendFooter();

    delay(250);
    dispatch_reboot(true); // Save the current config
}

void webHandleFirmwareUpload()
{
    upload = &webServer.upload();

    if(upload->status == UPLOAD_FILE_START) {
        if(!httpIsAuthenticated(F("update"))) return;
        LOG_TRACE(TAG_HTTP, F("Update: %s"), upload->filename.c_str());
        haspProgressMsg(upload->filename.c_str());
        // WiFiUDP::stopAll();
        uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
        // if(!Update.begin(UPDATE_SIZE_UNKNOWN)) { // start with max available size
        if(!Update.begin(maxSketchSpace)) { // start with max available size
            webUpdatePrintError();
        }

    } else if(upload->status == UPLOAD_FILE_WRITE) {
        // flashing firmware to ESP
        if(Update.write(upload->buf, upload->currentSize) != upload->currentSize) {
            webUpdatePrintError();
        } else {
            webUploadProgress();
        }

    } else if(upload->status == UPLOAD_FILE_END) {
        haspProgressVal(100);
        if(Update.end(true)) { // true to set the size to the current progress
            haspProgressMsg(F(D_OTA_UPDATE_APPLY));
            webUpdateReboot();
        } else {
            webUpdatePrintError();
        }
    }
}
#endif

#if HASP_USE_SPIFFS > 0 || HASP_USE_LITTLEFS > 0
bool handleFileRead(String path)
{
    if(!httpIsAuthenticated(F("fileread"))) return false;

    path = webServer.urlDecode(path).substring(0, 31);
    if(path.endsWith("/")) {
        path += F("index.htm");
    }
    String pathWithGz = path + F(".gz");
    if(HASP_FS.exists(pathWithGz) || HASP_FS.exists(path)) {
        if(HASP_FS.exists(pathWithGz)) path += F(".gz");

        File file          = HASP_FS.open(path, "r");
        String contentType = getContentType(path);
        if(path == F("/edit.htm.gz")) {
            contentType = F("text/html");
        }
        webServer.streamFile(file, contentType);
        file.close();
        return true;
    }
    return false;
}

void handleFileUpload()
{
    if(webServer.uri() != "/edit") {
        return;
    }
    upload = &webServer.upload();
    if(upload->status == UPLOAD_FILE_START) {
        if(!httpIsAuthenticated(F("fileupload"))) return;
        LOG_INFO(TAG_HTTP, F("Total size: %s"), webServer.headerName(0).c_str());
        String filename((char*)0);
        filename.reserve(128);
        filename = upload->filename;
        if(!filename.startsWith("/")) {
            filename = "/";
            filename += upload->filename;
        }
        if(filename.length() < 32) {
            fsUploadFile = HASP_FS.open(filename, "w");
            LOG_TRACE(TAG_HTTP, F("handleFileUpload Name: %s"), filename.c_str());
            haspProgressMsg(fsUploadFile.name());
        } else {
            LOG_ERROR(TAG_HTTP, F("Filename %s is too long"), filename.c_str());
        }
    } else if(upload->status == UPLOAD_FILE_WRITE) {
        // DBG_OUTPUT_PORT.print("handleFileUpload Data: "); debugPrintln(upload.currentSize);
        if(fsUploadFile) {
            if(fsUploadFile.write(upload->buf, upload->currentSize) != upload->currentSize) {
                LOG_ERROR(TAG_HTTP, F("Failed to write received data to file"));
            } else {
                webUploadProgress(); // Moved to httpEverySecond Loop
            }
        }
    } else if(upload->status == UPLOAD_FILE_END) {
        if(fsUploadFile) {
            LOG_INFO(TAG_HTTP, F("Uploaded %s (%u bytes)"), fsUploadFile.name(), upload->totalSize);
            fsUploadFile.close();
        }
        haspProgressVal(255);

        // Redirect to /config/hasp page. This flushes the web buffer and frees the memory
        webServer.sendHeader(String(F("Location")), String(F("/config/hasp")), true);
        webServer.send_P(302, PSTR("text/plain"), "");
        // httpReconnect();
    }
}

void handleFileDelete()
{
    if(!httpIsAuthenticated(F("filedelete"))) return;

    char mimetype[16];
    snprintf_P(mimetype, sizeof(mimetype), PSTR("text/plain"));

    if(webServer.args() == 0) {
        return webServer.send_P(500, mimetype, PSTR("BAD ARGS"));
    }
    String path = webServer.arg(0);
    LOG_TRACE(TAG_HTTP, F("handleFileDelete: %s"), path.c_str());
    if(path == "/") {
        return webServer.send_P(500, mimetype, PSTR("BAD PATH"));
    }
    if(!HASP_FS.exists(path)) {
        return webServer.send_P(404, mimetype, PSTR("FileNotFound"));
    }
    HASP_FS.remove(path);
    webServer.send_P(200, mimetype, PSTR(""));
    // path.clear();
}

void handleFileCreate()
{
    if(!httpIsAuthenticated(F("filecreate"))) return;

    if(webServer.args() == 0) {
        return webServer.send(500, PSTR("text/plain"), PSTR("BAD ARGS"));
    }
    String path = webServer.arg(0);
    LOG_TRACE(TAG_HTTP, F("handleFileCreate: %s"), path.c_str());
    if(path == "/") {
        return webServer.send(500, PSTR("text/plain"), PSTR("BAD PATH"));
    }
    if(HASP_FS.exists(path)) {
        return webServer.send(500, PSTR("text/plain"), PSTR("FILE EXISTS"));
    }
    File file = HASP_FS.open(path, "w");
    if(file) {
        file.close();
    } else {
        return webServer.send(500, PSTR("text/plain"), PSTR("CREATE FAILED"));
    }
    webServer.send(200, PSTR("text/plain"), "");
    path.clear();
}

void handleFileList()
{
    if(!httpIsAuthenticated(F("filelist"))) return;

    if(!webServer.hasArg(F("dir"))) {
        webServer.send(500, PSTR("text/plain"), PSTR("BAD ARGS"));
        return;
    }

    String path = webServer.arg(F("dir"));
    LOG_TRACE(TAG_HTTP, F("handleFileList: %s"), path.c_str());
    path.clear();

#if defined(ARDUINO_ARCH_ESP32)
    File root     = HASP_FS.open("/", FILE_READ);
    File file     = root.openNextFile();
    String output = "[";

    while(file) {
        if(output != "[") {
            output += ',';
        }
        bool isDir = false;
        output += F("{\"type\":\"");
        output += (isDir) ? F("dir") : F("file");
        output += F("\",\"name\":\"");
        if(file.name()[0] == '/') {
            output += &(file.name()[1]);
        } else {
            output += file.name();
        }
        output += F("\"}");

        // file.close();
        file = root.openNextFile();
    }
    output += "]";
    webServer.send(200, PSTR("text/json"), output);
#elif defined(ARDUINO_ARCH_ESP8266)
    Dir dir = HASP_FS.openDir(path);
    String output = "[";
    while(dir.next()) {
        File entry = dir.openFile("r");
        if(output != "[") {
            output += ',';
        }
        bool isDir = false;
        output += F("{\"type\":\"");
        output += (isDir) ? F("dir") : F("file");
        output += F("\",\"name\":\"");
        if(entry.name()[0] == '/') {
            output += &(entry.name()[1]);
        } else {
            output += entry.name();
        }
        output += F("\"}");
        entry.close();
    }
    output += "]";
    webServer.send(200, PSTR("text/json"), output);
#endif
}
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
#if HASP_USE_CONFIG > 0
void webHandleConfig()
{ // http://plate01/config
    if(!httpIsAuthenticated(F("config"))) return;

    saveConfig();

// Reboot after saving wifi config in AP mode
#if HASP_USE_WIFI > 0 && !defined(STM32F4xx)
    if(WiFi.getMode() != WIFI_STA) {
        httpHandleReboot();
    }
#endif

    {
        String httpMessage((char*)0);
        httpMessage.reserve(HTTP_PAGE_SIZE);
        httpMessage += F("<h1>");
        httpMessage += haspDevice.get_hostname();
        httpMessage += F("</h1><hr>");

#if HASP_USE_WIFI > 0
        httpMessage += F("<p><form method='get' action='/config/wifi'><button type='submit'>" D_HTTP_WIFI_SETTINGS
                         "</button></form></p>");
#endif

#if HASP_USE_MQTT > 0
        httpMessage += F("<p><form method='get' action='/config/mqtt'><button type='submit'>" D_HTTP_MQTT_SETTINGS
                         "</button></form></p>");
#endif

        httpMessage += F("<p><form method='get' action='/config/http'><button type='submit'>" D_HTTP_HTTP_SETTINGS
                         "</button></form></p>");

        httpMessage += F("<p><form method='get' action='/config/gui'><button type='submit'>" D_HTTP_GUI_SETTINGS
                         "</button></form></p>");

        // httpMessage +=
        //     F("<p><form method='get' action='/config/hasp'><button type='submit'>HASP
        //     Settings</button></form></p>");

#if HASP_USE_GPIO > 0
        httpMessage += F("<p><form method='get' action='/config/gpio'><button type='submit'>" D_HTTP_GPIO_SETTINGS
                         "</button></form></p>");
#endif

        httpMessage += F("<p><form method='get' action='/config/debug'><button type='submit'>" D_HTTP_DEBUG_SETTINGS
                         "</button></form></p>");

        httpMessage +=
            F("<p><form method='get' action='resetConfig'><button class='red' type='submit'>" D_HTTP_FACTORY_RESET
              "</button></form>");

        httpMessage += FPSTR(MAIN_MENU_BUTTON);

        webSendPage(haspDevice.get_hostname(), httpMessage.length(), false);
        webServer.sendContent(httpMessage);
    }
    // httpMessage.clear();
    webSendFooter();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
#if HASP_USE_MQTT > 0
void webHandleMqttConfig()
{ // http://plate01/config/mqtt
    if(!httpIsAuthenticated(F("config/mqtt"))) return;

    StaticJsonDocument<256> settings;
    mqttGetConfig(settings.to<JsonObject>());

    {
        // char buffer[128];
        String httpMessage((char*)0);
        httpMessage.reserve(HTTP_PAGE_SIZE);
        httpMessage += F("<h1>");
        httpMessage += haspDevice.get_hostname();
        httpMessage += F("</h1><hr>");

        httpMessage += F("<form method='POST' action='/config'>");
        httpMessage += F("<b>HASP Node Name</b> <i><small>(required. lowercase letters, numbers, and _ only)</small>"
                         "</i><input id='name' required name='name' maxlength=15 "
                         "placeholder='HASP Node Name' pattern='[a-z0-9_]*' value='");
        httpMessage += settings[FPSTR(FP_CONFIG_NAME)].as<String>();
        httpMessage += F("'><br/><br/><b>Group Name</b> <i><small>(required)</small></i><input id='group' required "
                         "name='group' maxlength=15 placeholder='Group Name' value='");
        httpMessage += settings[FPSTR(FP_CONFIG_GROUP)].as<String>();
        httpMessage += F("'><br/><br/><b>MQTT Broker</b> <i><small>(required)</small></i><input id='host' required "
                         "name='host' maxlength=63 placeholder='mqttServer' value='");
        httpMessage += settings[FPSTR(FP_CONFIG_HOST)].as<String>();
        httpMessage += F("'><br/><b>MQTT Port</b> <i><small>(required)</small></i><input id='port' required "
                         "name='port' type='number' maxlength=5 placeholder='mqttPort' value='");
        httpMessage += settings[FPSTR(FP_CONFIG_PORT)].as<uint16_t>();
        httpMessage += F("'><br/><b>MQTT User</b> <i><small>(optional)</small></i><input id='mqttUser' name='user' "
                         "maxlength=31 placeholder='user' value='");
        httpMessage += settings[FPSTR(FP_CONFIG_USER)].as<String>();
        httpMessage += F("'><br/><b>MQTT Password</b> <i><small>(optional)</small></i><input id='pass' "
                         "name='pass' type='password' maxlength=31 placeholder='mqttPassword' value='");
        if(settings[FPSTR(FP_CONFIG_PASS)].as<String>() != "") httpMessage += F(D_PASSWORD_MASK);

        httpMessage +=
            F("'><p><button type='submit' name='save' value='mqtt'>" D_HTTP_SAVE_SETTINGS "</button></form></p>");

        add_form_button(httpMessage, F("&#8617; " D_HTTP_CONFIGURATION), F("/config"), F(""));
        // httpMessage += PSTR("<p><form method='get' action='/config'><button type='submit'>&#8617; "
        // D_HTTP_CONFIGURATION
        //                     "</button></form></p>");

        webSendPage(haspDevice.get_hostname(), httpMessage.length(), false);
        webServer.sendContent(httpMessage);
    }
    // httpMessage.clear();
    webSendFooter();
}
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
void webHandleGuiConfig()
{ // http://plate01/config/wifi
    if(!httpIsAuthenticated(F("config/gui"))) return;

    {
        StaticJsonDocument<256> settings;
        guiGetConfig(settings.to<JsonObject>());

        String httpMessage((char*)0);
        httpMessage.reserve(HTTP_PAGE_SIZE);
        httpMessage += F("<h1>");
        httpMessage += haspDevice.get_hostname();
        httpMessage += F("</h1><hr>");

        httpMessage += F("<form method='POST' action='/config'>");

        httpMessage += F("<p><b>Short Idle</b> <input id='idle1' required "
                         "name='idle1' type='number' min='0' max='32400' value='");
        httpMessage += settings[FPSTR(FP_GUI_IDLEPERIOD1)].as<String>();
        httpMessage += F("'></p>");

        httpMessage += F("<p><b>Long Idle</b> <input id='idle2' required "
                         "name='idle2' type='number' min='0' max='32400' value='");
        httpMessage += settings[FPSTR(FP_GUI_IDLEPERIOD2)].as<String>();
        httpMessage += F("'></p>");

        int8_t rotation = settings[FPSTR(FP_GUI_ROTATION)].as<int8_t>();
        httpMessage += F("<p><b>Orientation</b> <select id='rotate' name='rotate'>");
        httpMessage += getOption(0, F("0 degrees"), rotation == 0);
        httpMessage += getOption(1, F("90 degrees"), rotation == 1);
        httpMessage += getOption(2, F("180 degrees"), rotation == 2);
        httpMessage += getOption(3, F("270 degrees"), rotation == 3);
        httpMessage += getOption(6, F("0 degrees - mirrored"), rotation == 6);
        httpMessage += getOption(7, F("90 degrees - mirrored"), rotation == 7);
        httpMessage += getOption(4, F("180 degrees - mirrored"), rotation == 4);
        httpMessage += getOption(5, F("270 degrees - mirrored"), rotation == 5);
        httpMessage += F("</select></p>");

        httpMessage += F("<p><input id='inv' name='inv' type='checkbox' ");
        if(settings[FPSTR(FP_GUI_INVERT)].as<bool>()) httpMessage += F(" checked");
        httpMessage += F("><b>Invert Colors</b>");

        httpMessage += F("<p><input id='cur' name='cur' type='checkbox' ");
        if(settings[FPSTR(FP_GUI_POINTER)].as<bool>()) httpMessage += F(" checked");
        httpMessage += F("><b>Show Pointer</b>");

        int8_t bcklpin = settings[FPSTR(FP_GUI_BACKLIGHTPIN)].as<int8_t>();
        httpMessage += F("<p><b>Backlight Control</b> <select id='bckl' name='bckl'>");
        httpMessage += getOption(-1, F("None"), bcklpin == -1);
#if defined(ARDUINO_ARCH_ESP32)
        add_gpio_select_option(httpMessage, 5, bcklpin);  // D8 on ESP32 for D1 mini 32
        add_gpio_select_option(httpMessage, 12, bcklpin); // TFT_LED on the Liligo Pi
        add_gpio_select_option(httpMessage, 16, bcklpin); // D4 on ESP32 for D1 mini 32
        add_gpio_select_option(httpMessage, 17, bcklpin); // D3 on ESP32 for D1 mini 32
        add_gpio_select_option(httpMessage, 18, bcklpin); // D5 on ESP32 for D1 mini 32
        add_gpio_select_option(httpMessage, 19, bcklpin); // D6 on ESP32 for D1 mini 32
        add_gpio_select_option(httpMessage, 21, bcklpin); // D1 on ESP32 for D1 mini 32
        add_gpio_select_option(httpMessage, 22, bcklpin); // D2 on ESP32 for D1 mini 32
        add_gpio_select_option(httpMessage, 23, bcklpin); // D7 on ESP32 for D1 mini 32
        add_gpio_select_option(httpMessage, 32, bcklpin); // TFT_LED on the Lolin D32 Pro
#else
        httpMessage += getOption(5, F("D1 - GPIO 5"), bcklpin == 5);
        httpMessage += getOption(4, F("D2 - GPIO 4"), bcklpin == 4);
        httpMessage += getOption(0, F("D3 - GPIO 0"), bcklpin == 0);
        httpMessage += getOption(2, F("D4 - GPIO 2"), bcklpin == 2);
#endif
        httpMessage += F("</select></p>");

        add_button(httpMessage, F(D_HTTP_SAVE_SETTINGS), F("name='save' value='gui'"));
        close_form(httpMessage);
        // httpMessage +=
        //     F("<p><button type='submit' name='save' value='gui'>" D_HTTP_SAVE_SETTINGS "</button></p></form>");

#if TOUCH_DRIVER == 2046 && defined(TOUCH_CS)
        add_form_button(httpMessage, F(D_HTTP_CALIBRATE), F("/config/gui"), F("name='action' value='calibrate'"));

// httpMessage += PSTR("<p><form method='get' action='/config/gui'><button type='submit' "
//                     ">" D_HTTP_CALIBRATE "</button></form></p>");
#endif

        add_form_button(httpMessage, F("&#8617; " D_HTTP_CONFIGURATION), F("/config"), F(""));

        // httpMessage += PSTR("<p><form method='get' action='/config'><button type='submit'>&#8617; "
        // D_HTTP_CONFIGURATION
        //                     "</button></form></p>");

        webSendPage(haspDevice.get_hostname(), httpMessage.length(), false);
        webServer.sendContent(httpMessage);
    }
    webSendFooter();

    if(webServer.hasArg(F("action"))) dispatch_text_line(webServer.arg(F("action")).c_str());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
#if HASP_USE_WIFI > 0
void webHandleWifiConfig()
{ // http://plate01/config/wifi
    if(!httpIsAuthenticated(F("config/wifi"))) return;

    StaticJsonDocument<256> settings;
    wifiGetConfig(settings.to<JsonObject>());

    String httpMessage((char*)0);
    httpMessage.reserve(HTTP_PAGE_SIZE);
    httpMessage += F("<h1>");
    httpMessage += haspDevice.get_hostname();
    httpMessage += F("</h1><hr>");

    httpMessage += F("<form method='POST' action='/config'>");
    httpMessage += F("<b>WiFi SSID</b> <i><small>(required)</small></i><input id='ssid' required "
                     "name='ssid' maxlength=31 placeholder='WiFi SSID' value='");
    httpMessage += settings[FPSTR(FP_CONFIG_SSID)].as<String>();
    httpMessage += F("'><br/><b>WiFi Password</b> <i><small>(required)</small></i><input id='pass' required "
                     "name='pass' type='password' maxlength=63 placeholder='WiFi Password' value='");
    if(settings[FPSTR(FP_CONFIG_PASS)].as<String>() != "") {
        httpMessage += F(D_PASSWORD_MASK);
    }
    httpMessage +=
        F("'><p><button type='submit' name='save' value='wifi'>" D_HTTP_SAVE_SETTINGS "</button></p></form>");

#if HASP_USE_WIFI > 0 && !defined(STM32F4xx)
    if(WiFi.getMode() == WIFI_STA) {
        add_form_button(httpMessage, F("&#8617; " D_HTTP_CONFIGURATION), F("/config"), F(""));
        // httpMessage += PSTR("<p><form method='get' action='/config'><button type='submit'>&#8617; "
        // D_HTTP_CONFIGURATION
        //                     "</button></form></p>");
    }
#endif

    webSendPage(haspDevice.get_hostname(), httpMessage.length(), false);
    webServer.sendContent(httpMessage);
#if defined(STM32F4xx)
    httpMessage = "";
#else
    httpMessage.clear();
#endif
    webSendFooter();
}
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
#if HASP_USE_HTTP > 0
void webHandleHttpConfig()
{ // http://plate01/config/http
    if(!httpIsAuthenticated(F("config/http"))) return;

    {
        StaticJsonDocument<256> settings;
        httpGetConfig(settings.to<JsonObject>());

        // String httpMessage((char *)0);
        // httpMessage.reserve(HTTP_PAGE_SIZE);
        // httpMessage += F("<h1>");
        // httpMessage += haspDevice.get_hostname();
        // httpMessage += F("</h1><hr>");

        // httpMessage += F("<form method='POST' action='/config'>");
        // httpMessage += F("<b>Web Username</b> <i><small>(optional)</small></i><input id='user' "
        //                  "name='user' maxlength=31 placeholder='admin' value='");
        // httpMessage += settings[FPSTR(FP_CONFIG_USER)].as<String>();
        // httpMessage += F("'><br/><b>Web Password</b> <i><small>(optional)</small></i><input id='pass' "
        //                  "name='pass' type='password' maxlength=63 placeholder='Password' value='");
        // if(settings[FPSTR(FP_CONFIG_PASS)].as<String>() != "") {
        //     httpMessage += F(D_PASSWORD_MASK);
        // }
        // httpMessage +=
        //     F("'><p><button type='submit' name='save' value='http'>" D_HTTP_SAVE_SETTINGS "</button></p></form>");

        // httpMessage += PSTR("<p><form method='get' action='/config'><button type='submit'>&#8617; "
        // D_HTTP_CONFIGURATION
        //                     "</button></form></p>");

        char httpMessage[HTTP_PAGE_SIZE];

        size_t len = snprintf_P(
            httpMessage, sizeof(httpMessage),
            PSTR("<h1>%s</h1><hr>"
                 "<form method='POST' action='/config'>"
                 "<b>Web Username</b> <i><small>(optional)</small></i>"
                 "<input id='user' name='user' maxlength=31 placeholder='admin' value='%s'><br/>"
                 "<b>Web Password</b> <i><small>(optional)</small></i>"
                 "<input id='pass' name='pass' type='password' maxlength=63 placeholder='Password' value='%s'>"
                 "<p><button type='submit' name='save' value='http'>" D_HTTP_SAVE_SETTINGS "</button></p></form>"
                 "<p><form method='get' action='/config'><button type='submit'>&#8617; " D_HTTP_CONFIGURATION
                 "</button></form></p>"),
            haspDevice.get_hostname(), settings[FPSTR(FP_CONFIG_USER)].as<String>().c_str(),
            settings[FPSTR(FP_CONFIG_PASS)].as<String>().c_str());

        // if(settings[FPSTR(FP_CONFIG_PASS)].as<String>() != "") {
        //     httpMessage += F(D_PASSWORD_MASK);
        // }

        webSendPage(haspDevice.get_hostname(), len, false);
        webServer.sendContent(httpMessage);
    }
    // httpMessage.clear();
    webSendFooter();
}
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
#if HASP_USE_GPIO > 0
void webHandleGpioConfig()
{ // http://plate01/config/gpio
    if(!httpIsAuthenticated(F("config/gpio"))) return;
    uint8_t configCount = 0;

    //   StaticJsonDocument<256> settings;
    // gpioGetConfig(settings.to<JsonObject>());

    if(webServer.hasArg(PSTR("save"))) {
        uint8_t id      = webServer.arg(F("id")).toInt();
        uint8_t pin     = webServer.arg(F("pin")).toInt();
        uint8_t type    = webServer.arg(F("type")).toInt() + webServer.arg(F("state")).toInt();
        uint8_t group   = webServer.arg(F("group")).toInt();
        uint8_t pinfunc = webServer.arg(F("func")).toInt();
        gpioSavePinConfig(id, pin, type, group, pinfunc);
    }
    if(webServer.hasArg(PSTR("del"))) {
        uint8_t id  = webServer.arg(F("id")).toInt();
        uint8_t pin = webServer.arg(F("pin")).toInt();
        gpioSavePinConfig(id, pin, HASP_GPIO_FREE, 0, 0);
    }

    {
        String httpMessage((char*)0);
        httpMessage.reserve(HTTP_PAGE_SIZE);
        httpMessage += F("<h1>");
        httpMessage += haspDevice.get_hostname();
        httpMessage += F("</h1><hr>");

        httpMessage += F("<form method='POST' action='/config'>");

        httpMessage += F("<table><tr><th>Pin</th><th>Type</th><th>Group</th><th>Default</th><th>Action</th></tr>");

        for(uint8_t gpio = 0; gpio < NUM_DIGITAL_PINS; gpio++) {
            for(uint8_t id = 0; id < HASP_NUM_GPIO_CONFIG; id++) {
                hasp_gpio_config_t conf = gpioGetPinConfig(id);
                if((conf.pin == gpio) && gpioConfigInUse(id) && gpioInUse(gpio) && !gpioIsSystemPin(gpio)) {
                    httpMessage += F("<tr><td>");
                    httpMessage += halGpioName(gpio);
                    httpMessage += F("</td><td>");

                    switch(conf.type & 0xfe) {
                        case HASP_GPIO_SWITCH:
                            // case HASP_GPIO_SWITCH_INVERTED:
                            httpMessage += F("Switch");
                            break;
                        case HASP_GPIO_BUTTON:
                            // case HASP_GPIO_BUTTON_INVERTED:
                            httpMessage += F("Button");
                            break;
                        case HASP_GPIO_LED:
                            // case HASP_GPIO_LED_INVERTED:
                            httpMessage += F("Led");
                            break;
                        case HASP_GPIO_LED_R:
                        case HASP_GPIO_LED_G:
                        case HASP_GPIO_LED_B:
                            // case HASP_GPIO_LED_INVERTED:
                            httpMessage += F("Mood ");
                            break;
                        case HASP_GPIO_RELAY:
                            // case HASP_GPIO_RELAY_INVERTED:
                            httpMessage += F("Relay");
                            break;
                        case HASP_GPIO_PWM:
                            // case HASP_GPIO_PWM_INVERTED:
                            httpMessage += F("PWM");
                            break;
                        default:
                            httpMessage += F("Unknown");
                    }

                    switch(conf.type & 0xfe) {
                        case HASP_GPIO_LED_R:
                            httpMessage += F("Red");
                            break;
                        case HASP_GPIO_LED_G:
                            httpMessage += F("Green");
                            break;
                        case HASP_GPIO_LED_B:
                            httpMessage += F("Blue");
                            break;
                    }

                    httpMessage += F("</td><td>");
                    httpMessage += conf.group;
                    httpMessage += F("</td><td>");

                    // bool inverted = (conf.type == HASP_GPIO_BUTTON_INVERTED) ||
                    //                 (conf.type == HASP_GPIO_SWITCH_INVERTED) || (conf.type == HASP_GPIO_LED_INVERTED)
                    //                 || (conf.type == HASP_GPIO_RELAY_INVERTED) || (conf.type ==
                    //                 HASP_GPIO_PWM_INVERTED);

                    httpMessage += (conf.type & 0x1) ? F("High") : F("Low");

                    httpMessage += F("</td><td><a href='/config/gpio/options?id=");
                    httpMessage += id;
                    httpMessage += ("'>Edit</a> <a href='/config/gpio?del=&id=");
                    httpMessage += id;
                    httpMessage += ("&pin=");
                    httpMessage += conf.pin;
                    httpMessage += ("'>Delete</a></td><tr>");
                    configCount++;
                }
            }
        }

        httpMessage += F("</table></form>");

        if(configCount < HASP_NUM_GPIO_CONFIG) {
            httpMessage += F("<p><form method='GET' action='gpio/options'>");
            httpMessage += F("<input type='hidden' name='id' value='");
            httpMessage += gpioGetFreeConfigId();
            httpMessage += F("'><button type='submit'>" D_HTTP_ADD_GPIO "</button></form></p>");
        }

        add_form_button(httpMessage, F("&#8617; " D_HTTP_CONFIGURATION), F("/config"), F(""));
        //    httpMessage += F("<p><form method='get' action='/config'><button type='submit'>&#8617; "
        //    D_HTTP_CONFIGURATION
        //                      "</button></form></p>");

        webSendPage(haspDevice.get_hostname(), httpMessage.length(), false);
        webServer.sendContent(httpMessage);
    }
    // httpMessage.clear();
    webSendFooter();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void webHandleGpioOptions()
{ // http://plate01/config/gpio/options
    if(!httpIsAuthenticated(F("config/gpio/options"))) return;

    {
        StaticJsonDocument<256> settings;
        guiGetConfig(settings.to<JsonObject>());

        uint8_t config_id = webServer.arg(F("id")).toInt();

        String httpMessage((char*)0);
        httpMessage.reserve(HTTP_PAGE_SIZE);
        httpMessage += F("<h1>");
        httpMessage += haspDevice.get_hostname();
        httpMessage += F("</h1><hr>");

        httpMessage += F("<form method='GET' action='/config/gpio'>");
        httpMessage += F("<input type='hidden' name='id' value='");
        httpMessage += config_id;
        httpMessage += F("'>");

        httpMessage += F("<p><b>GPIO Options");
        httpMessage += config_id;
        httpMessage += F(" Options</b></p>");

        httpMessage += F("<p><b>Pin</b> <select id='pin' name='pin'>");
        hasp_gpio_config_t conf = gpioGetPinConfig(config_id);

        for(uint8_t io = 0; io < NUM_DIGITAL_PINS; io++) {
            if(((conf.pin == io) || !gpioInUse(io)) && !gpioIsSystemPin(io)) {
                httpMessage += getOption(io, halGpioName(io), conf.pin == io);
            }
        }
        httpMessage += F("</select></p>");

        bool selected;
        httpMessage += F("<p><b>Type</b> <select id='type' name='type'>");
        // httpMessage += getOption(HASP_GPIO_FREE, F("Unused"), false);

        selected = (conf.type == HASP_GPIO_SWITCH) || (conf.type == HASP_GPIO_SWITCH_INVERTED);
        httpMessage += getOption(HASP_GPIO_SWITCH, F("Switch"), selected);

        selected = (conf.type == HASP_GPIO_BUTTON) || (conf.type == HASP_GPIO_BUTTON_INVERTED);
        httpMessage += getOption(HASP_GPIO_BUTTON, F("Button"), selected);

        selected = (conf.type == HASP_GPIO_LED) || (conf.type == HASP_GPIO_LED_INVERTED);
        httpMessage += getOption(HASP_GPIO_LED, F("Led"), selected);

        selected = (conf.type == HASP_GPIO_LED_R) || (conf.type == HASP_GPIO_LED_R_INVERTED);
        httpMessage += getOption(HASP_GPIO_LED_R, F("Mood Red"), selected);

        selected = (conf.type == HASP_GPIO_LED_G) || (conf.type == HASP_GPIO_LED_G_INVERTED);
        httpMessage += getOption(HASP_GPIO_LED_G, F("Mood Green"), selected);

        selected = (conf.type == HASP_GPIO_LED_B) || (conf.type == HASP_GPIO_LED_B_INVERTED);
        httpMessage += getOption(HASP_GPIO_LED_B, F("Mood Blue"), selected);

        selected = (conf.type == HASP_GPIO_RELAY) || (conf.type == HASP_GPIO_RELAY_INVERTED);
        httpMessage += getOption(HASP_GPIO_RELAY, F("Relay"), selected);

        if(digitalPinHasPWM(webServer.arg(0).toInt())) {
            selected = (conf.type == HASP_GPIO_PWM) || (conf.type == HASP_GPIO_PWM_INVERTED);
            httpMessage += getOption(HASP_GPIO_PWM, F("PWM"), selected);
        }
        httpMessage += F("</select></p>");

        httpMessage += F("<p><b>Group</b> <select id='group' name='group'>");
        httpMessage += getOption(0, F("None"), conf.group == 0);
        String group((char*)0);
        group.reserve(10);
        for(int i = 1; i < 15; i++) {
            group = F("Group ");
            group += i;
            httpMessage += getOption(i, group, conf.group == i);
        }
        httpMessage += F("</select></p>");

        httpMessage += F("<p><b>Default State</b> <select id='state' name='state'>");
        bool inverted = (conf.type == HASP_GPIO_BUTTON_INVERTED) || (conf.type == HASP_GPIO_SWITCH_INVERTED) ||
                        (conf.type == HASP_GPIO_LED_INVERTED) || (conf.type == HASP_GPIO_RELAY_INVERTED) ||
                        (conf.type == HASP_GPIO_PWM_INVERTED);
        httpMessage += getOption(1, F("High"), inverted);
        httpMessage += getOption(0, F("Low"), !inverted);
        httpMessage += F("</select></p>");

        httpMessage +=
            F("<p><button type='submit' name='save' value='gpio'>" D_HTTP_SAVE_SETTINGS "</button></p></form>");

        httpMessage += PSTR("<p><form method='get' action='/config/gpio'><button type='submit'>&#8617; " D_HTTP_BACK
                            "</button></form></p>");

        webSendPage(haspDevice.get_hostname(), httpMessage.length(), false);
        webServer.sendContent(httpMessage);
    }
    webSendFooter();

    if(webServer.hasArg(F("action"))) dispatch_text_line(webServer.arg(F("action")).c_str()); // Security check
}
#endif // HASP_USE_GPIO

////////////////////////////////////////////////////////////////////////////////////////////////////
void webHandleDebugConfig()
{ // http://plate01/config/debug
    if(!httpIsAuthenticated(F("config/debug"))) return;

    StaticJsonDocument<256> settings;
    debugGetConfig(settings.to<JsonObject>());

    {
        String httpMessage((char*)0);
        httpMessage.reserve(HTTP_PAGE_SIZE);
        httpMessage += F("<h1>");
        httpMessage += haspDevice.get_hostname();
        httpMessage += F("</h1><hr>");

        httpMessage += F("<form method='POST' action='/config'>");

        uint16_t baudrate = settings[FPSTR(FP_CONFIG_BAUD)].as<uint16_t>();
        httpMessage += F("<p><b>Serial Port</b> <select id='baud' name='baud'>");
        httpMessage += getOption(1, F("Disabled"), baudrate == 1); // Don't use 0 here which is default 115200
        httpMessage += getOption(960, F("9600"), baudrate == 960);
        httpMessage += getOption(1920, F("19200"), baudrate == 1920);
        httpMessage += getOption(3840, F("38400"), baudrate == 3840);
        httpMessage += getOption(5760, F("57600"), baudrate == 5760);
        httpMessage += getOption(7488, F("74880"), baudrate == 7488);
        httpMessage += getOption(11520, F("115200"), baudrate == 11520);
        httpMessage += F("</select></p><p><b>Telemetry Period</b> <i><small>(Seconds, 0=disable)</small></i> "
                         "<input id='teleperiod' required name='teleperiod' type='number' min='0' max='65535' value='");
        httpMessage += settings[FPSTR(FP_DEBUG_TELEPERIOD)].as<String>();
        httpMessage += F("'></p>");

#if HASP_USE_SYSLOG > 0
        httpMessage += F("<b>Syslog Hostame</b> <i><small>(optional)</small></i><input id='host' "
                         "name='host' maxlength=31 placeholder='logserver' value='");
        httpMessage += settings[FPSTR(FP_CONFIG_HOST)].as<String>();
        httpMessage += F("'><br/><b>Syslog Port</b> <i><small>(optional)</small></i> <input id='port' required "
                         "name='port' type='number' min='0' max='65535' value='");
        httpMessage += settings[FPSTR(FP_CONFIG_PORT)].as<String>();

        httpMessage += F("'><b>Syslog Facility</b> <select id='log' name='log'>");
        uint8_t logid = settings[FPSTR(FP_CONFIG_LOG)].as<uint8_t>();
        for(int i = 0; i < 8; i++) {
            httpMessage += getOption(i, String(F("Local")) + i, i == logid);
        }

        httpMessage += F("</select></br><b>Syslog Protocol</b> <input id='proto' name='proto' type='radio' value='0'");
        if(settings[FPSTR(FP_CONFIG_PROTOCOL)].as<uint8_t>() == 0) httpMessage += F(" checked");
        httpMessage += F(">IETF (RFC 5424) &nbsp; <input id='proto' name='proto' type='radio' value='1'");
        if(settings[FPSTR(FP_CONFIG_PROTOCOL)].as<uint8_t>() == 1) httpMessage += F(" checked");
        httpMessage += F(">BSD (RFC 3164)");
#endif

        httpMessage +=
            F("</p><p><button type='submit' name='save' value='debug'>" D_HTTP_SAVE_SETTINGS "</button></p></form>");

        add_form_button(httpMessage, F("&#8617; " D_HTTP_CONFIGURATION), F("/config"), F(""));
        // httpMessage += PSTR("<p><form method='get' action='/config'><button type='submit'>&#8617; "
        // D_HTTP_CONFIGURATION
        //                     "</button></form></p>");

        webSendPage(haspDevice.get_hostname(), httpMessage.length(), false);
        webServer.sendContent(httpMessage);
    }
    // httpMessage.clear();
    webSendFooter();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void webHandleHaspConfig()
{ // http://plate01/config/http
    if(!httpIsAuthenticated(F("config/hasp"))) return;

    StaticJsonDocument<256> settings;
    haspGetConfig(settings.to<JsonObject>());

    {
        String httpMessage((char*)0);
        httpMessage.reserve(HTTP_PAGE_SIZE);
        httpMessage += F("<h1>");
        httpMessage += haspDevice.get_hostname();
        httpMessage += F("</h1><hr>");

        httpMessage += F("<p><form action='/edit' method='post' enctype='multipart/form-data'><input type='file' "
                         "name='filename' accept='.jsonl,.zi'>");
        httpMessage += F("<button type='submit'>" D_HTTP_UPLOAD_FILE "</button></form></p><hr>");

        // httpMessage += F("<form method='POST' action='/config'>");
        httpMessage += F("<form method='POST' action='/'>");
        httpMessage += F("<p><b>UI Theme</b> <i><small>(required)</small></i><select id='theme' name='theme'>");

        uint8_t themeid = settings[FPSTR(FP_CONFIG_THEME)].as<uint8_t>();
        // httpMessage += getOption(0, F("Built-in"), themeid == 0);
#if LV_USE_THEME_HASP == 1
        httpMessage += getOption(2, F("Hasp Dark"), themeid == 2);
        httpMessage += getOption(1, F("Hasp Light"), themeid == 1);
#endif
#if LV_USE_THEME_EMPTY == 1
        httpMessage += getOption(0, F("Empty"), themeid == 0);
#endif
#if LV_USE_THEME_MONO == 1
        httpMessage += getOption(3, F("Mono"), themeid == 3);
#endif
#if LV_USE_THEME_MATERIAL == 1
        httpMessage += getOption(5, F("Material Dark"), themeid == 5);
        httpMessage += getOption(4, F("Material Light"), themeid == 4);
#endif
#if LV_USE_THEME_TEMPLATE == 1
        httpMessage += getOption(7, F("Template"), themeid == 7);
#endif
        httpMessage += F("</select></br>");
        httpMessage +=
            F("<b>Hue</b><div style='width:100%;background-image:linear-gradient(to "
              "right,red,orange,yellow,green,blue,indigo,violet);'><input style='align:center;padding:0px;width:100%;' "
              "name='hue' type='range' min='0' max='360' value='");
        httpMessage += settings[FPSTR(FP_CONFIG_HUE)].as<String>();
        httpMessage += F("'></div></p>");
        httpMessage += F("<p><b>Default Font</b><select id='font' name='font'><option value=''>None</option>");

#if defined(ARDUINO_ARCH_ESP32)
        File root = HASP_FS.open("/");
        File file = root.openNextFile();

        while(file) {
            String filename = file.name();
            if(filename.endsWith(".zi"))
                httpMessage +=
                    getOption(file.name(), file.name(), filename == settings[FPSTR(FP_CONFIG_ZIFONT)].as<String>());
            file = root.openNextFile();
        }
#elif defined(ARDUINO_ARCH_ESP8266)
        Dir dir = HASP_FS.openDir("/");
        while(dir.next()) {
            File file = dir.openFile("r");
            String filename = file.name();
            if(filename.endsWith(".zi"))
                httpMessage +=
                    getOption(file.name(), file.name(), filename == settings[FPSTR(FP_CONFIG_ZIFONT)].as<String>());
            file.close();
        }
#endif
        httpMessage += F("</select></p>");

        httpMessage += F("<p><b>Startup Layout</b> <i><small>(optional)</small></i><input id='pages' "
                         "name='pages' maxlength=31 placeholder='/pages.jsonl' value='");

        httpMessage += settings[FPSTR(FP_CONFIG_PAGES)].as<String>();
        httpMessage += F("'></br><b>Startup Page</b> <i><small>(required)</small></i><input id='startpage' required "
                         "name='startpage' type='number' min='1' max='4' value='");
        httpMessage += settings[FPSTR(FP_CONFIG_STARTPAGE)].as<String>();
        httpMessage +=
            F("'></p><p><b>Startup Brightness</b> <i><small>(required)</small></i><input id='startpage' required "
              "name='startdim' type='number' min='0' max='100' value='");
        httpMessage += settings[FPSTR(FP_CONFIG_STARTDIM)].as<String>();
        httpMessage += F("'></p>");

        httpMessage +=
            F("<p><button type='submit' name='save' value='hasp'>" D_HTTP_SAVE_SETTINGS "</button></form></p>");

        // httpMessage +=
        //     F("<p><form method='get' action='/config'><button
        //     type='submit'>"D_HTTP_CONFIGURATION"</button></form></p>");
        httpMessage += FPSTR(MAIN_MENU_BUTTON);

        webSendPage(haspDevice.get_hostname(), httpMessage.length(), false);
        webServer.sendContent(httpMessage);
    }
    // httpMessage.clear();
    webSendFooter();
}
#endif // HASP_USE_CONFIG

////////////////////////////////////////////////////////////////////////////////////////////////////
void httpHandleNotFound()
{ // webServer 404
#if HASP_USE_SPIFFS > 0 || HASP_USE_LITTLEFS > 0
    if(handleFileRead(webServer.uri())) return;
#endif

#if defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_ESP8266)
    LOG_TRACE(TAG_HTTP, F("Sending 404 to client connected from: %s"),
              webServer.client().remoteIP().toString().c_str());
#else
  // LOG_TRACE(TAG_HTTP,F("Sending 404 to client connected from: %s"), String(webServer.client().remoteIP()).c_str());
#endif

    String httpMessage((char*)0);
    httpMessage.reserve(HTTP_PAGE_SIZE);

    httpMessage += F("File Not Found\n\nURI: ");
    httpMessage += webServer.uri();
    httpMessage += F("\nMethod: ");
    httpMessage += (webServer.method() == HTTP_GET) ? F("GET") : F("POST");
    httpMessage += F("\nArguments: ");
    httpMessage += webServer.args();
    httpMessage += "\n";
    for(int i = 0; i < webServer.args(); i++) {
        httpMessage += " " + webServer.argName(i) + ": " + webServer.arg(i) + "\n";
    }
    webServer.send(404, PSTR("text/plain"), httpMessage.c_str());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void webHandleFirmware()
{
    if(!httpIsAuthenticated(F("firmware"))) return;

    {
        String httpMessage((char*)0);
        httpMessage.reserve(HTTP_PAGE_SIZE);
        httpMessage += F("<h1>");
        httpMessage += haspDevice.get_hostname();
        httpMessage += F("</h1><hr>");

        httpMessage += F("<p><form action='/update' method='post' enctype='multipart/form-data'><input type='file' "
                         "name='filename' accept='.bin'>");
        httpMessage += F("<button type='submit'>" D_HTTP_UPDATE_FIRMWARE "</button></form></p>");

        // httpMessage += F("<p><form action='/update' method='post' enctype='multipart/form-data'><input type='file' "
        //                  "name='filename' accept='.spiffs'>");
        // httpMessage += F("<button type='submit'>Replace Filesystem Image</button></form></p>");

        httpMessage += F("<form method='get' action='/espfirmware'>");
        httpMessage += F("<br/><b>Update ESP from URL</b>");
        httpMessage += F("<br/><input id='url' name='url' value='");
        httpMessage += "";
        httpMessage += F("'><br/><br/><button type='submit'>Update ESP from URL</button></form>");

        httpMessage += FPSTR(MAIN_MENU_BUTTON);

        webSendPage(haspDevice.get_hostname(), httpMessage.length(), false);
        webServer.sendContent(httpMessage);
    }
    // httpMessage.clear();
    webSendFooter();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void httpHandleEspFirmware()
{ // http://plate01/espfirmware
    char url[4];
    memcpy_P(url, PSTR("url"), 4);

    if(!httpIsAuthenticated(F("espfirmware"))) return;

    {
        String httpMessage((char*)0);
        httpMessage.reserve(HTTP_PAGE_SIZE);
        httpMessage += F("<h1>");
        httpMessage += haspDevice.get_hostname();
        httpMessage += F("</h1><hr>");

        httpMessage += F("<p><b>ESP update</b></p>Updating ESP firmware from: ");
        httpMessage += webServer.arg(url);

        webSendPage(haspDevice.get_hostname(), httpMessage.length(), true);
        webServer.sendContent(httpMessage);
        // httpMessage.clear();
    }
    webSendFooter();

    LOG_TRACE(TAG_HTTP, F("Attempting ESP firmware update from: %s"), webServer.arg(url).c_str());
    dispatch_web_update(NULL, webServer.arg(url).c_str());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
#if HASP_USE_CONFIG > 0
void webHandleSaveConfig()
{
    if(!httpIsAuthenticated(F("saveConfig"))) return;

    configWrite();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void httpHandleResetConfig()
{ // http://plate01/resetConfig
    if(!httpIsAuthenticated(F("resetConfig"))) return;

    bool resetConfirmed = webServer.arg(F("confirm")) == F("yes");

    {
        String httpMessage((char*)0);
        httpMessage.reserve(HTTP_PAGE_SIZE);
        httpMessage += F("<h1>");
        httpMessage += haspDevice.get_hostname();
        httpMessage += F("</h1><hr>");

        if(resetConfirmed) { // User has confirmed, so reset everything
            bool formatted = configClearEeprom();
            if(formatted) {
                httpMessage += F("<b>Resetting all saved settings and restarting device</b>");
            } else {
                httpMessage += F("<b>Failed to format the internal flash partition</b>");
                resetConfirmed = false;
            }
        } else {
            httpMessage +=
                F("<h2>Warning</h2><b>This process will reset all settings to the default values. The internal flash "
                  "will "
                  "be erased and the device is restarted. You may need to connect to the WiFi AP displayed on the "
                  "panel to "
                  "re-configure the device before accessing it again. ALL FILES WILL BE LOST!"
                  "<br/><hr><br/><form method='get' action='resetConfig'>"
                  "<br/><br/><button type='submit' name='confirm' value='yes'>" D_HTTP_ERASE_DEVICE "</button></form>"
                  "<br/><hr><br/>");

            add_form_button(httpMessage, F("&#8617; " D_HTTP_CONFIGURATION), F("/config"), F(""));
            // httpMessage +=
            //     PSTR("<p><form method='get' action='/config'><button type='submit'>&#8617; " D_HTTP_CONFIGURATION
            //          "</button></form></p>");
        }

        webSendPage(haspDevice.get_hostname(), httpMessage.length(), resetConfirmed);
        webServer.sendContent(httpMessage);
    }
    // httpMessage.clear();
    webSendFooter();

    if(resetConfirmed) {
        delay(250);
        // configClearSaved();
        dispatch_reboot(false); // Do not save the current config
    }
}
#endif // HASP_USE_CONFIG

void httpStart()
{
    webServer.begin();
    webServerStarted = true;
#if HASP_USE_WIFI > 0
#if defined(STM32F4xx)
    IPAddress ip;
    ip = WiFi.localIP();
    LOG_INFO(TAG_HTTP, F(D_SERVICE_STARTED " @ http://%d.%d.%d.%d"), ip[0], ip[1], ip[2], ip[3]);
#else
    LOG_INFO(TAG_HTTP, F(D_SERVICE_STARTED " @ http://%s"),
             (WiFi.getMode() != WIFI_STA ? WiFi.softAPIP().toString().c_str() : WiFi.localIP().toString().c_str()));
#endif
#else
    IPAddress ip;
#if defined(ARDUINO_ARCH_ESP32)
    ip = ETH.localIP();
#else
    ip = Ethernet.localIP();
#endif
    LOG_INFO(TAG_HTTP, F(D_SERVICE_STARTED " @ http://%d.%d.%d.%d"), ip[0], ip[1], ip[2], ip[3]);
#endif
}

void httpStop()
{
    webServer.stop();
    webServerStarted = false;
    LOG_WARNING(TAG_HTTP, F(D_SERVICE_STOPPED));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void httpSetup()
{
    // httpSetConfig(settings);

    // ask server to track these headers
    const char* headerkeys[] = {"Content-Length"}; // "Authentication"
    size_t headerkeyssize    = sizeof(headerkeys) / sizeof(char*);
    webServer.collectHeaders(headerkeys, headerkeyssize);

    // Shared pages
    webServer.on(F("/about"), webHandleAbout);
    webServer.onNotFound(httpHandleNotFound);

#if HASP_USE_WIFI > 0

    // These two endpoints are needed in STA and AP mode
    webServer.on(F("/config"), webHandleConfig);

#if !defined(STM32F4xx)

#if HASP_USE_CONFIG > 0
    if(WiFi.getMode() != WIFI_STA) {
        webServer.on(F("/"), webHandleWifiConfig);
        LOG_TRACE(TAG_HTTP, F("Wifi access point"));
        return;
    }

#endif // HASP_USE_CONFIG
#endif // !STM32F4xx
#endif // HASP_USE_WIFI

    // The following endpoints are only needed in STA mode
    webServer.on(F("/page/"), []() {
        String pageid = webServer.arg(F("page"));
        webServer.send(200, PSTR("text/plain"), "Page: '" + pageid + "'");
        haspSetPage(pageid.toInt());
    });

#if HASP_USE_SPIFFS > 0 || HASP_USE_LITTLEFS > 0
    webServer.on(F("/list"), HTTP_GET, handleFileList);
    // load editor
    webServer.on(F("/edit"), HTTP_GET, []() {
        if(!handleFileRead("/edit.htm")) {
            char mimetype[16];
            snprintf_P(mimetype, sizeof(mimetype), PSTR("text/plain"));
            webServer.send_P(404, mimetype, PSTR("FileNotFound"));
        }
    });
    webServer.on(F("/edit"), HTTP_PUT, handleFileCreate);
    webServer.on(F("/edit"), HTTP_DELETE, handleFileDelete);
    // first callback is called after the request has ended with all parsed arguments
    // second callback handles file uploads at that location
    webServer.on(
        F("/edit"), HTTP_POST,
        []() {
            webServer.send(200, "text/plain", "");
            LOG_VERBOSE(TAG_HTTP, F("Headers: %d"), webServer.headers());
        },
        handleFileUpload);
#endif

    webServer.on(F("/"), webHandleRoot);
    webServer.on(F("/info"), webHandleInfo);
    webServer.on(F("/screenshot"), webHandleScreenshot);
    webServer.on(F("/firmware"), webHandleFirmware);
    webServer.on(F("/reboot"), httpHandleReboot);

#if HASP_USE_CONFIG > 0
    webServer.on(F("/config/hasp"), webHandleHaspConfig);
    webServer.on(F("/config/http"), webHandleHttpConfig);
    webServer.on(F("/config/gui"), webHandleGuiConfig);
    webServer.on(F("/config/debug"), webHandleDebugConfig);
#if HASP_USE_MQTT > 0
    webServer.on(F("/config/mqtt"), webHandleMqttConfig);
#endif
#if HASP_USE_WIFI > 0
    webServer.on(F("/config/wifi"), webHandleWifiConfig);
#endif
#if HASP_USE_GPIO > 0
    webServer.on(F("/config/gpio"), webHandleGpioConfig);
    webServer.on(F("/config/gpio/options"), webHandleGpioOptions);
#endif
    webServer.on(F("/saveConfig"), webHandleSaveConfig);
    webServer.on(F("/resetConfig"), httpHandleResetConfig);
#endif // HASP_USE_CONFIG

#if defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_ESP8266)
    webServer.on(
        F("/update"), HTTP_POST,
        []() {
            webServer.send(200, "text/plain", "");
            LOG_VERBOSE(TAG_HTTP, F("Total size: %s"), webServer.hostHeader().c_str());
        },
        webHandleFirmwareUpload);
    webServer.on(F("/espfirmware"), httpHandleEspFirmware);
#endif

    LOG_INFO(TAG_HTTP, F(D_SERVICE_STARTED));
    // webStart();  Wait for network connection
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void httpReconnect()
{
    if(!http_config.enable) return;

    if(webServerStarted) {
        httpStop();
    } else
#if HASP_USE_WIFI > 0 && !defined(STM32F4xx)
        if(WiFi.status() == WL_CONNECTED || WiFi.getMode() != WIFI_STA)
#endif
    {
        httpStart();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void httpLoop(void)
{
    if(http_config.enable) webServer.handleClient();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void httpEvery5Seconds()
{
    //  if(httpEnable && !webServerStarted) httpReconnect();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
#if HASP_USE_CONFIG > 0
bool httpGetConfig(const JsonObject& settings)
{
    bool changed = false;

    settings[FPSTR(FP_CONFIG_ENABLE)] = http_config.enable;

    if(http_config.port != settings[FPSTR(FP_CONFIG_PORT)].as<uint16_t>()) changed = true;
    settings[FPSTR(FP_CONFIG_PORT)] = http_config.port;

    if(strcmp(http_config.user, settings[FPSTR(FP_CONFIG_USER)].as<String>().c_str()) != 0) changed = true;
    settings[FPSTR(FP_CONFIG_USER)] = http_config.user;

    if(strcmp(http_config.password, settings[FPSTR(FP_CONFIG_PASS)].as<String>().c_str()) != 0) changed = true;
    settings[FPSTR(FP_CONFIG_PASS)] = http_config.password;

    if(changed) configOutput(settings, TAG_HTTP);
    return changed;
}

/** Set HTTP Configuration.
 *
 * Read the settings from json and sets the application variables.
 *
 * @note: data pixel should be formated to uint32_t RGBA. Imagemagick requirements.
 *
 * @param[in] settings    JsonObject with the config settings.
 **/
bool httpSetConfig(const JsonObject& settings)
{
    configOutput(settings, TAG_HTTP);
    bool changed = false;

    changed |= configSet(http_config.port, settings[FPSTR(FP_CONFIG_PORT)], F("httpPort"));

    if(!settings[FPSTR(FP_CONFIG_USER)].isNull()) {
        changed |= strcmp(http_config.user, settings[FPSTR(FP_CONFIG_USER)]) != 0;
        strncpy(http_config.user, settings[FPSTR(FP_CONFIG_USER)], sizeof(http_config.user));
    }

    if(!settings[FPSTR(FP_CONFIG_PASS)].isNull()) {
        changed |= strcmp(http_config.password, settings[FPSTR(FP_CONFIG_PASS)]) != 0;
        strncpy(http_config.password, settings[FPSTR(FP_CONFIG_PASS)], sizeof(http_config.password));
    }

    return changed;
}
#endif // HASP_USE_CONFIG

size_t httpClientWrite(const uint8_t* buf, size_t size)
{
    /***** Sending 16Kb at once freezes on STM32 EthernetClient *****/
    size_t bytes_sent = 0;
    while(bytes_sent < size) {
        if(!webServer.client()) return bytes_sent;
        if(size - bytes_sent >= 2048) {
            bytes_sent += webServer.client().write(buf + bytes_sent, 2048);
        } else {
            bytes_sent += webServer.client().write(buf + bytes_sent, size - bytes_sent);
        }
        // Serial.println(bytes_sent);

        // stm32_eth_scheduler(); // already in write
        // webServer.client().flush();
        delay(1); // Fixes the freeze
    }
    return bytes_sent;
}

#endif