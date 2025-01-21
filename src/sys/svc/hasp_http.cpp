/* MIT License - Copyright (c) 2019-2024 Francis Van Roie
   For full license information read the LICENSE file in the project folder */

#include "hasplib.h"

#if HASP_USE_HTTP > 0

#include "ArduinoLog.h"

#define HTTP_LEGACY

#if defined(ARDUINO_ARCH_ESP32)
#include "Update.h"
#include "Preferences.h"
#include "sdkconfig.h" // for CONFIG_IDF_TARGET_ESP32* defines
#include <uri/UriBraces.h>
#include <uri/UriRegex.h>
#endif

#include "hasp_conf.h"
#include "dev/device.h"
#include "hal/hasp_hal.h"

#include "hasp_gui.h"
#include "hasp_debug.h"

#include "sys/net/hasp_network.h"
#include "sys/net/hasp_time.h"

#if(HASP_USE_CAPTIVE_PORTAL > 0) && (HASP_USE_WIFI > 0)
#include <DNSServer.h>
#endif

#if defined(ARDUINO_ARCH_ESP8266) || defined(ARDUINO_ARCH_ESP32)
File fsUploadFile;
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
bool webServerStarted = false;

#if(HASP_USE_CAPTIVE_PORTAL > 0) && (HASP_USE_WIFI > 0)
DNSServer dnsServer;
IPAddress apIP(192, 168, 4, 1);
#ifndef DNS_PORT
#define DNS_PORT 53
#endif // DNS_PORT
#endif // HASP_USE_CAPTIVE_PORTAL

hasp_http_config_t http_config;

#define HTTP_PAGE_SIZE (6 * 256)

#if(defined(STM32F4xx) || defined(STM32F7xx)) && HASP_USE_ETHERNET > 0
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

#if defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C3)
extern const uint8_t EDIT_HTM_GZ_START[] asm("_binary_data_static_edit_htm_gz_start");
extern const uint8_t EDIT_HTM_GZ_END[] asm("_binary_data_static_edit_htm_gz_end");
extern const uint8_t STYLE_CSS_GZ_START[] asm("_binary_data_static_style_css_gz_start");
extern const uint8_t STYLE_CSS_GZ_END[] asm("_binary_data_static_style_css_gz_end");
extern const uint8_t SCRIPT_JS_GZ_START[] asm("_binary_data_static_script_js_gz_start");
extern const uint8_t SCRIPT_JS_GZ_END[] asm("_binary_data_static_script_js_gz_end");
extern const uint8_t LOGO_SVG_GZ_START[] asm("_binary_data_static_logo_svg_gz_start");
extern const uint8_t LOGO_SVG_GZ_END[] asm("_binary_data_static_logo_svg_gz_end");
extern const uint8_t PETITE_VUE_HASP_JS_GZ_START[] asm("_binary_data_static_petite_vue_hasp_js_gz_start");
extern const uint8_t PETITE_VUE_HASP_JS_GZ_END[] asm("_binary_data_static_petite_vue_hasp_js_gz_end");
extern const uint8_t MAIN_JS_GZ_START[] asm("_binary_data_static_main_js_gz_start");
extern const uint8_t MAIN_JS_GZ_END[] asm("_binary_data_static_main_js_gz_end");
extern const uint8_t EN_JSON_GZ_START[] asm("_binary_data_static_en_json_gz_start");
extern const uint8_t EN_JSON_GZ_END[] asm("_binary_data_static_en_json_gz_end");
// extern const uint8_t HASP_HTM_GZ_START[] asm("_binary_data_static_hasp_htm_gz_start");
// extern const uint8_t HASP_HTM_GZ_END[] asm("_binary_data_static_hasp_htm_gz_end");
// extern const uint8_t ACE_JS_GZ_START[] asm("_binary_data_static_ace_1_9_6_min_js_gz_start");
// extern const uint8_t ACE_JS_GZ_END[] asm("_binary_data_static_ace_1_9_6_min_js_gz_end");

#endif // CONFIG_IDF_TARGET_ESP32

#endif // ESP32

HTTPUpload* upload;

const char MAIN_MENU_BUTTON[] PROGMEM = "<a href='/'>" D_HTTP_MAIN_MENU "</a>";

const char HTTP_DOCTYPE[] PROGMEM =
    "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"utf-8\">"
    //  "<meta http-equiv=\"Content-Security-Policy\" content=\"default-src 'self';img-src "
    //  "'self' data:;style-src 'self' data:;\">"
    "<meta charset='utf-8'><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"/>";
const char HTTP_META_GO_BACK[] PROGMEM = "<meta http-equiv='refresh' content='%d;url=/'/>";
const char HTTP_STYLESHEET[] PROGMEM   = "<link rel=\"stylesheet\" href=\"/%s.css?" COMMIT_HASH "\">";
const char HTTP_HEADER[] PROGMEM       = "<title>%s</title>";
const char HTTP_HEADER_END[] PROGMEM =
    "<link rel=\"stylesheet\" href=\"/static/style.css?" COMMIT_HASH "\"></head>"
    "<link rel=\"icon\" href=\"/static/logo.svg?" COMMIT_HASH "\" type=\"image/svg+xml\">"
    "<script type=\"module\" src=\"/static/main.js?" COMMIT_HASH "\"></script>"
    R"(<style>[v-cloak]{display:none}</style><body><div v-cloak v-scope @vue:mounted="mounted" id='doc'>)";
const char HTTP_FOOTER[] PROGMEM      = "<div class='clear'><hr/><a class='foot' href='/about'>" D_MANUFACTURER " ";
const char HTTP_END[] PROGMEM         = " " D_HTTP_FOOTER "</div></body></html>";
const uint8_t HTTP_VARS_CSS[] PROGMEM = ":root{"
                                        "--txt:" D_HTTP_COLOR_TEXT ";"
                                        "--bg:" D_HTTP_COLOR_BACKGROUND ";"
                                        "--btnfg:" D_HTTP_COLOR_BUTTON_TEXT ";"
                                        "--btnbg:" D_HTTP_COLOR_BUTTON ";"
                                        "--btnbghi:" D_HTTP_COLOR_BUTTON_HOVER ";"
                                        "--btnred:" D_HTTP_COLOR_BUTTON_RESET ";"
                                        "--btnredhi:" D_HTTP_COLOR_BUTTON_RESET_HOVER ";"
                                        "--btnbrd: transparent;"
                                        "--grpfg:" D_HTTP_COLOR_GROUP_TEXT ";"
                                        "--grpbg:" D_HTTP_COLOR_GROUP ";"
                                        "--fldbg:" D_HTTP_COLOR_INPUT ";"
                                        "--fldfg:" D_HTTP_COLOR_INPUT_TEXT ";"
                                        "--fldred:" D_HTTP_COLOR_INPUT_WARNING ";"
                                        "--footfg:" D_HTTP_COLOR_FOOTER_TEXT ";"
                                        "--toolbg:" D_HTTP_COLOR_TOOLBAR ";"
                                        "--treebg:" D_HTTP_COLOR_TREE ";"
                                        "--preeviewbg:" D_HTTP_COLOR_PREVIEW ";"
                                        "}";

////////////////////////////////////////////////////////////////////////////////////////////////////

// URL for auto-update "version.json"
// const char UPDATE_URL[] PROGMEM = "http://haswitchplate.com/update/version.json";
// // Default link to compiled Arduino firmware image
// String espFirmwareUrl = "http://haswitchplate.com/update/HASwitchPlate.ino.d1_mini.bin";
// // Default link to compiled Nextion firmware images
// String lcdFirmwareUrl = "http://haswitchplate.com/update/HASwitchPlate.tft";

////////////////////////////////////////////////////////////////////////////////////////////////////
String getOption(int value, String label, int current_value = INT_MIN)
{
    char buffer[128];
    snprintf_P(buffer, sizeof(buffer), PSTR("<option value='%d'%s>%s</option>"), value,
               (value == current_value ? PSTR(" selected") : ""), label.c_str());
    return buffer;
}

String getOption(String& value, String& label, String& current_value)
{
    char buffer[128];
    snprintf_P(buffer, sizeof(buffer), PSTR("<option value='%s'%s>%s</option>"), value.c_str(),
               (value == current_value ? PSTR(" selected") : ""), label.c_str());
    return buffer;
}

static void add_form_button(String& str, const char* label, const char* action)
{
    str += "<a href='";
    str += action;
    str += "'>";
    str += label;
    str += "</a>";
}

static void add_form_button(String& str, const char* label, const __FlashStringHelper* action)
{
    str += "<a href='";
    str += action;
    str += "'>";
    str += label;
    str += "</a>";
}

static String http_get_content_type(const String& path)
{
    char buffer[sizeof(mime::mimeTable[0].mimeType)];
    int len = sizeof(mime::mimeTable) / sizeof(mime::mimeTable[0]) - 1;

    // Check all entries except the last one for match, return if found
    for(size_t i = 0; i < len; i++) {
        strcpy_P(buffer, mime::mimeTable[i].endsWith);
        if(path.endsWith(buffer)) {
            strcpy_P(buffer, mime::mimeTable[i].mimeType);
            return String(buffer);
        }
    }

    // Fall-through and return default (=last) mime-type
    strcpy_P(buffer, mime::mimeTable[len].mimeType);
    return String(buffer);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

static void webHandleHaspConfig();

////////////////////////////////////////////////////////////////////////////////////////////////////

// Check authentication but only create Log entry if it failed
bool http_is_authenticated()
{
    if(http_config.password[0] != '\0') { // Request HTTP auth if httpPassword is set
        if(!webServer.authenticate(http_config.username, http_config.password)) {
            webServer.requestAuthentication();
            LOG_WARNING(TAG_HTTP, D_TELNET_INCORRECT_LOGIN_ATTEMPT,
                        webServer.client().remoteIP().toString().c_str());
            return false;
        }
    }
    return true;
}

// Check authentication and create Log entry
bool http_is_authenticated(const __FlashStringHelper* notused)
{
    if(!http_is_authenticated()) return false;

#if defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_ESP8266)
    LOG_VERBOSE(TAG_HTTP, D_HTTP_SENDING_PAGE, webServer.uri().c_str(),
                webServer.client().remoteIP().toString().c_str());
#else
        // LOG_INFO(TAG_HTTP,D_HTTP_SENDING_PAGE, page,
        //             String(webServer.client().remoteIP()).c_str());
#endif

    return true;
}

// Check authentication and create Log entry
bool http_is_authenticated(const char* notused)
{
    if(!http_is_authenticated()) return false;

#if defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_ESP8266)
    LOG_VERBOSE(TAG_HTTP, D_HTTP_SENDING_PAGE, webServer.uri().c_str(),
                webServer.client().remoteIP().toString().c_str());
#else
        // LOG_INFO(TAG_HTTP,D_HTTP_SENDING_PAGE, page,
        //             String(webServer.client().remoteIP()).c_str());
#endif

    return true;
}

static void webSendFooter()
{
#if defined(STM32F4xx)
    webServer.sendContent(HTTP_FOOTER);
    webServer.sendContent(haspDevice.get_version());
    webServer.sendContent(HTTP_END);
#else
    webServer.sendContent_P(HTTP_FOOTER);
    webServer.sendContent(haspDevice.get_version());
    webServer.sendContent_P(HTTP_END);
#endif
}

static void http_send_cache_header(int age = 3600)
{
    webServer.sendHeader("Cache-Control", (String)(F("public, max-age=")) + (String)(age));
}

static int http_send_cached(int statuscode, const char* contenttype, const char* data, size_t size, int age = 3600)
{
    http_send_cache_header(age);
#if defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_ESP8266)
    webServer.send_P(statuscode, contenttype, data, size);
#else
    webServer.sendHeader("Content-Length", (String)(size));
    webServer.send(statuscode, contenttype, data);
#endif
    return statuscode;
}

static int http_send_static_file(const uint8_t* start, const uint8_t* end, String& contentType)
{
    size_t size = end > start ? end - start : 0;
    int age     = 365 * 24 * 60 * 60;
    return http_send_cached(200, contentType.c_str(), (const char*)start, size, age);
}

static int http_send_static_gzip_file(const uint8_t* start, const uint8_t* end, String& contentType)
{
    webServer.sendHeader("Content-Encoding", "gzip");
    return http_send_static_file(start, end, contentType);
}

static void webSendHtmlHeader(const char* title, uint32_t httpdatalength, uint8_t gohome = 0)
{
    char buffer[64];

    /* Calculate Content Length upfront */
    uint32_t contentLength = strlen(haspDevice.get_version()); // version length
    contentLength += sizeof(HTTP_DOCTYPE) - 1;
    contentLength += sizeof(HTTP_HEADER) - 1 - 2 + strlen(title);             // -2 for %s
    contentLength += sizeof(HTTP_STYLESHEET) - 1 - 2 + strlen("static/vars"); // -2 for %s
    if(gohome > 0) {
        snprintf_P(buffer, sizeof(buffer), HTTP_META_GO_BACK, gohome);
        contentLength += strlen(buffer); // gohome
    } else {
        buffer[0] = '\0';
    }
    contentLength += sizeof(HTTP_HEADER_END) - 1;
    contentLength += sizeof(HTTP_FOOTER) - 1;
    contentLength += sizeof(HTTP_END) - 1;

    if(httpdatalength > HTTP_PAGE_SIZE) {
        LOG_WARNING(TAG_HTTP, F("Sending page with %u static and %u dynamic bytes"), contentLength, httpdatalength);
    }

    webServer.setContentLength(contentLength + httpdatalength);
    // webServer.sendHeader("Set-Cookie", "lang=nl; SameSite=None; path=/");
#if defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_ESP8266)
    webServer.send_P(200, PSTR("text/html"), HTTP_DOCTYPE); // 122
#else
    webServer.send(200, ("text/html"), HTTP_DOCTYPE); // 122
#endif
    webServer.sendContent(buffer); // gohome

    snprintf_P(buffer, sizeof(buffer), HTTP_STYLESHEET, "static/vars");
    webServer.sendContent(buffer); // stylesheet

    snprintf_P(buffer, sizeof(buffer), HTTP_HEADER, title);
    webServer.sendContent(buffer); // 17-2+len

#if defined(STM32F4xx)
    webServer.sendContent(HTTP_HEADER_END); // 80
#else
    webServer.sendContent_P(HTTP_HEADER_END);         // 80
#endif
}

static void http_send_content(const char* form[], int count, uint8_t gohome = 0)
{
    size_t total = 0;
    size_t len[count];
    for(int i = 0; i < count; i++) {
        if(form[i]) {
            len[i] = strlen(form[i]);
            total += len[i];
        }
    }
    webSendHtmlHeader(haspDevice.get_hostname(), total, gohome);
    for(int i = 0; i < count; i++) {
        if(form[i]) webServer.sendContent(form[i], len[i]);
    }
    webSendFooter();
}

// Allows caching of this file, BUT browser must validate Etag before using cached versions
static void http_send_etag(String& etag)
{
    String newTag((char*)0);
    newTag.reserve(64);
    newTag = "\"";
    newTag += etag;
    newTag += "\"";
    webServer.sendHeader("Cache-Control", F("no-cache, must-revalidate, public"));
    webServer.sendHeader("Expires", F("Fri, 30 Oct 1998 14:19:41 GMT"));
    webServer.sendHeader("ETag", newTag);
}

bool http_save_config()
{
    bool updated = false;

    if(webServer.method() == HTTP_POST && webServer.hasArg("save")) {
        String save = webServer.arg("save");

        StaticJsonDocument<256> settings;
        for(int i = 0; i < webServer.args(); i++) settings[webServer.argName(i)] = webServer.arg(i);

        if(save == FP_HASP) {
            updated = haspSetConfig(settings.as<JsonObject>());

#if HASP_USE_MQTT > 0
        } else if(save == String(PSTR(FP_MQTT))) {
            updated = mqttSetConfig(settings.as<JsonObject>());
#endif
#if HASP_USE_FTP > 0
        } else if(save == String(PSTR(FP_FTP))) {
            updated = ftpSetConfig(settings.as<JsonObject>());
#endif

        } else if(save == FP_GUI) {
            settings[FPSTR(FP_GUI_POINTER)]         = webServer.hasArg("cursor");
            settings[FPSTR(FP_GUI_INVERT)]          = webServer.hasArg("invert");
            settings[FPSTR(FP_GUI_BACKLIGHTINVERT)] = webServer.hasArg("bcklinv");
            updated                                 = guiSetConfig(settings.as<JsonObject>());

        } else if(save == FP_DEBUG) {
            settings[FPSTR(FP_DEBUG_ANSI)] = webServer.hasArg("ansi");
            updated                        = debugSetConfig(settings.as<JsonObject>());

        } else if(save == FP_HTTP) {
            updated = httpSetConfig(settings.as<JsonObject>());

            // Password might have changed
            if(!http_is_authenticated("config")) return updated;

#if HASP_USE_WIFI > 0
        } else if(save == FP_WIFI) {
            updated = wifiSetConfig(settings.as<JsonObject>());
#endif
#if HASP_USE_WIREGUARD > 0
        } else if(save == FP_WG) {
            updated = wgSetConfig(settings.as<JsonObject>());
#endif
        }
    }

    return updated;
}

static void http_handle_root()
{
    if(!http_is_authenticated("root")) return;
    bool updated = http_save_config();

    const char* html[20];
    int i   = 0;
    int len = (sizeof(html) / sizeof(html[0])) - 1;

    html[min(i++, len)] = "<h1>";
    html[min(i++, len)] = haspDevice.get_hostname();
    html[min(i++, len)] = "</h1><hr>";
    if(updated) {
        html[min(i++, len)] = R"(<p class='info'>" D_HTTP_CONFIG_CHANGED "</p>)";
    }
    html[min(i++, len)] = R"(<a href="/config/hasp" v-t="'hasp.btn'"></a>)";
    html[min(i++, len)] = R"(<a href="/screenshot" v-t="'screenshot.btn'"></a>)";
    html[min(i++, len)] = R"(<a href="/info" v-t="'info.btn'"></a>)";
    html[min(i++, len)] = R"(<a href="/config" v-t="'config.btn'"></a>)";
    html[min(i++, len)] = R"(<a href="/firmware" v-t="'ota.btn'"></a>)";
#ifdef ARDUINO_ARCH_ESP32
    html[min(i++, len)] = R"(<a href="/edit" v-t="'editor.btn'"></a>)";
#endif
    html[min(i++, len)] = R"(<a href="/reboot" class="red" v-t="'reboot.btn'"></a>)";
    http_send_content(html, min(i, len));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
static void http_handle_reboot()
{ // http://plate01/reboot
    if(!http_is_authenticated("reboot")) return;

    const char* html[20];
    int i   = 0;
    int len = (sizeof(html) / sizeof(html[0])) - 1;

    html[min(i++, len)] = "<h1>";
    html[min(i++, len)] = haspDevice.get_hostname();
    html[min(i++, len)] = "</h1><hr>";
    html[min(i++, len)] = R"(<h2 v-t="'reboot.title'"></h2><div v-t="'reboot.message'"></div>)";
    http_send_content(html, min(i, len), 10);

    { // Execute Actions
      // delay(200);
        dispatch_reboot(true);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
static void http_handle_screenshot()
{ // http://plate01/screenshot
    if(!http_is_authenticated("screenshot")) return;

    { // Execute actions
        if(webServer.hasArg("a")) {
            if(webServer.arg("a") == "next") {
                dispatch_page_next(LV_SCR_LOAD_ANIM_NONE);
            } else if(webServer.arg("a") == "prev") {
                dispatch_page_prev(LV_SCR_LOAD_ANIM_NONE);
            } else if(webServer.arg("a") == "back") {
                dispatch_page_back(LV_SCR_LOAD_ANIM_NONE);
            }
        }

        // Check if screenshot bitmap is dirty
        if(webServer.hasArg("d")) {
            if(guiScreenshotIsDirty())
                webServer.send(200, F("text/text"), "1");
            else
                webServer.send(304, F("text/text"), "0");
            return;
        }

        uint32_t modified = guiScreenshotEtag();
        String etag((char*)0);
        etag.reserve(64);

        if(webServer.hasHeader("If-None-Match")) {
            etag = webServer.header("If-None-Match");
            etag.replace("\"", "");
            LOG_DEBUG(TAG_HTTP, F("If-None-Match: %s"), etag.c_str());
            if(modified > 0 && modified == atol(etag.c_str())) { // Not Changed
                http_send_etag(etag);                            // Reuse same ETag
                webServer.send(304, "image/bmp", "");            // Use correct mimetype
                return;                                          // 304 not Modified
            }
        }

        // Send actual bitmap
        if(webServer.hasArg("q")) {
            lv_disp_t* disp = lv_disp_get_default();
            etag            = (String)(modified);
            http_send_etag(etag); // Send new tag with modification version
            webServer.setContentLength(66 + disp->driver.hor_res * disp->driver.ver_res * sizeof(lv_color_t));
            webServer.send(200, "image/bmp", "");
            guiTakeScreenshot();
            webServer.client().stop();
            return;
        }
    }

    const char* html[20];
    int i   = 0;
    int len = (sizeof(html) / sizeof(html[0])) - 1;

    html[min(i++, len)] = "<h1>";
    html[min(i++, len)] = haspDevice.get_hostname();
    html[min(i++, len)] = "</h1><hr>";
    html[min(i++, len)] = R"(
<p class="c"><img loading="lazy" id="bmp" src="/screenshot?q=0"></p>
<div class="dist">
<a href="#" @click.prevent="upd('prev') " v-t="'screenshot.prev'"></a>
<a href="#" @click.prevent="upd('') " v-t="'screenshot.refresh'"></a>
<a href="#" @click.prevent="upd('next') " v-t="'screenshot.next'"></a>
</div>)";
    html[min(i++, len)] = R"(<a v-t="'home.btn'" href="/"></a>)";
    http_send_content(html, min(i, len));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
static void add_json(String& data, JsonDocument& doc)
{
    char buffer[800];
    size_t len = serializeJson(doc, buffer, sizeof(buffer));
    if(doc.isNull()) return; // empty document

    buffer[len - 1] = ',';
    char* start     = buffer + 1;
    data += String(start);
    doc.clear();
}

static void add_license(JsonObject& obj, const char* title, const char* year, const char* author, const char* license,
                        uint8_t allrightsreserved = 0)
{
    obj["t"] = title;
    obj["y"] = year;
    obj["a"] = author;
    obj["l"] = license;
    if(allrightsreserved) obj["r"] = allrightsreserved;
}

static void webHandleApi()
{ // http://plate01/api
    if(!http_is_authenticated("api")) return;

    DynamicJsonDocument doc(max(MAX_CONFIG_JSON_ALLOC_SIZE, 2048));
    String contentType = http_get_content_type(F(".json"));
    String endpoint((char*)0);
    endpoint = webServer.pathArg(0);

    if(!strcasecmp(endpoint.c_str(), "files")) {
        String path = webServer.arg("dir");
        webServer.send(200, contentType.c_str(), filesystem_list(HASP_FS, path.c_str(), 5).c_str());

    } else if(!strcasecmp(endpoint.c_str(), "info")) {
        String jsondata((char*)0);
        jsondata.reserve(HTTP_PAGE_SIZE);
        jsondata = "{";

        hasp_get_info(doc);
        add_json(jsondata, doc);

#if HASP_USE_MQTT > 0
        mqtt_get_info(doc);
        add_json(jsondata, doc);
#endif

#if HASP_USE_WIFI > 0 || HASP_USE_EHTERNET > 0
        network_get_info(doc);
        add_json(jsondata, doc);
#endif

        haspDevice.get_info(doc);
        add_json(jsondata, doc);

        jsondata[jsondata.length() - 1] = '}'; // Replace last comma with a bracket

        webServer.send(200, contentType, jsondata);
        return;

    } else if(!strcasecmp(endpoint.c_str(), "credits")) {

        {
            JsonObject obj;
            obj = doc.createNestedObject();
            add_license(obj, "HASwitchPlate", "2019", "Allen Derusha allen@derusha.org", "mit");
            obj = doc.createNestedObject();
            add_license(obj, "Tasmota Core", "2016", "Tasmota", "apache2");
            obj = doc.createNestedObject();
            add_license(obj, "LVGL", "2021", "LVGL Kft", "mit");
#if defined(LGFX_USE_V1)
            obj = doc.createNestedObject();
            add_license(obj, "LovyanGFX", "2020", "lovyan03 (https://github.com/lovyan03)", "freebsd", 1);
#endif
            obj = doc.createNestedObject();
            add_license(obj, "TFT_eSPI", "2020", "Bodmer (https://github.com/Bodmer)", "freebsd", 1);
            obj = doc.createNestedObject();
            add_license(obj, "Adafruit_GFX", "2021", "Adafruit Industries.", "bsd", 1);
#if defined(HASP_USE_ARDUINOGFX)
            obj = doc.createNestedObject();
            add_license(obj, "Arduino_GFX", "", "moononournation", "", 0);
#endif
#if HASP_USE_MQTT > 0 && defined(HASP_USE_PUBSUBCLIENT)
            obj = doc.createNestedObject();
            add_license(obj, "PubSubClient", "2008-2015", "Nicholas O'Leary", "mit");
#endif
            obj = doc.createNestedObject();
            add_license(obj, "ArduinoJson", "2014-2022", "Benoit BLANCHON", "mit");
            obj = doc.createNestedObject();
            add_license(obj, "ArduinoLog", "2017-2018",
                        "Thijs Elenbaas, MrRobot62, rahuldeo2047, NOX73, dhylands, Josha blemasle, mfalkvidd", "mit");
#if HASP_USE_FTP > 0
            obj = doc.createNestedObject();
            add_license(obj, "SimpleFTPServer", "2017", "Renzo Mischianti www.mischianti.org", "mit", 1);
#endif
            obj = doc.createNestedObject();
            add_license(obj, "AceButton", "2018", "Brian T. Park", "mit");
            obj = doc.createNestedObject();
            add_license(obj, "QR Code generator", "", "Project Nayuki", "mit");
#if HASP_USE_WIREGUARD > 0
            obj = doc.createNestedObject();
            add_license(obj, "WireGuard", "2021", "Kenta Ida fugafuga.org, Daniel Hope www.floorsense.nz", "bsd", 1);
#endif
        }
        {
            char output[HTTP_PAGE_SIZE];
            serializeJson(doc, output, sizeof(output));
            webServer.send(200, contentType.c_str(), output);
        }

    } else if(!strcasecmp(endpoint.c_str(), "config")) {

        JsonObject settings;
        String postBody((char*)0);
        postBody = webServer.arg("plain");

        if(webServer.method() == HTTP_GET) {
            // Make sure we have a valid JsonObject to start from
            settings = doc.to<JsonObject>();

        } else if(webServer.method() == HTTP_POST || webServer.method() == HTTP_PUT) {
            DeserializationError jsonError = deserializeJson(doc, postBody);
            if(jsonError) { // Couldn't parse incoming JSON command
                dispatch_json_error(TAG_HTTP, jsonError);
                return;
            }
            settings = doc.as<JsonObject>();
        } else {
            webServer.send(400, contentType, "Bad Request");
            return;
        }

        settings = doc.to<JsonObject>();
        const __FlashStringHelper* module;

        module = FPSTR(FP_HASP);
        settings.createNestedObject(module);
        haspGetConfig(settings[module]);

        module = FPSTR(FP_GUI);
        settings.createNestedObject(module);
        guiGetConfig(settings[module]);

        module = FPSTR(FP_DEBUG);
        settings.createNestedObject(module);
        debugGetConfig(settings[module]);

#if HASP_USE_WIFI > 0
        module = FPSTR(FP_WIFI);
        settings.createNestedObject(module);
        wifiGetConfig(settings[module]);

        module = FPSTR(FP_TIME);
        settings.createNestedObject(module);
        timeGetConfig(settings[module]);
#endif
#if HASP_USE_WIREGUARD > 0
        module = FPSTR(FP_WG);
        settings.createNestedObject(module);
        wgGetConfig(settings[module]);
#endif
#if HASP_USE_MQTT > 0
        module = FPSTR(FP_MQTT);
        settings.createNestedObject(module);
        mqttGetConfig(settings[module]);
#endif
#if HASP_USE_FTP > 0
        module = FPSTR(FP_FTP);
        settings.createNestedObject(module);
        ftpGetConfig(settings[module]);
#endif
#if HASP_USE_HTTP > 0
        module = FPSTR(FP_HTTP);
        settings.createNestedObject(module);
        httpGetConfig(settings[module]);
#endif
#if HASP_USE_ARDUINOOTA > 0 || HASP_USE_HTTP_UPDATE > 0
        module = FPSTR(FP_OTA);
        settings.createNestedObject(module);
        otaGetConfig(settings[module]);
#endif
#if HASP_USE_GPIO > 0
        module = FPSTR(FP_GPIO);
        settings.createNestedObject(module);
        gpioGetConfig(settings[module]);
#endif
        configOutput(settings, TAG_HTTP); // Log current JSON config

        LOG_DEBUG(TAG_HTTP, "%s - %d", __FILE__, __LINE__);
        // Mask non-blank passwords
        // if(!settings[FPSTR(FP_CONFIG_PASS)].isNull() && settings[FPSTR(FP_CONFIG_PASS)].as<String>().length() != 0) {
        //     settings[FPSTR(FP_CONFIG_PASS)] = D_PASSWORD_MASK;
        // }

        LOG_DEBUG(TAG_HTTP, "%s - %d", __FILE__, __LINE__);
        doc.shrinkToFit();
        LOG_DEBUG(TAG_HTTP, "%s - %d", __FILE__, __LINE__);
        const size_t size = measureJson(doc) + 1;
        LOG_DEBUG(TAG_HTTP, "%s - %d", __FILE__, __LINE__);
        char jsondata[size];
        LOG_DEBUG(TAG_HTTP, "%s - %d", __FILE__, __LINE__);
        memset(jsondata, 0, size);
        LOG_DEBUG(TAG_HTTP, "%s - %d", __FILE__, __LINE__);
        serializeJson(doc, jsondata, size);
        LOG_DEBUG(TAG_HTTP, "%s - %d", __FILE__, __LINE__);
        webServer.send(200, contentType, jsondata);
        LOG_DEBUG(TAG_HTTP, "%s - %d", __FILE__, __LINE__);

    } else {
        webServer.send(400, contentType, "Bad Request");
    }
}

static void webHandleApiConfig()
{ // http://plate01/about
    if(!http_is_authenticated("api")) return;

    if(webServer.method() != HTTP_GET && webServer.method() != HTTP_POST) {
        return;
    }

    StaticJsonDocument<1024> doc;
    JsonObject settings;
    // String contentType = http_get_content_type(F(".json"));
    String endpoint((char*)0);
    endpoint                 = webServer.pathArg(0);
    const char* endpoint_key = endpoint.c_str();

    String postBody = webServer.arg("plain");

    if(webServer.method() == HTTP_GET) {
        // Make sure we have a valid JsonObject to start from
        settings = doc.to<JsonObject>();

    } else if(webServer.method() == HTTP_POST || webServer.method() == HTTP_PUT) {
        DeserializationError jsonError = deserializeJson(doc, postBody);
        if(jsonError) { // Couldn't parse incoming JSON command
            dispatch_json_error(TAG_HTTP, jsonError);
            return;
        }
        settings = doc.as<JsonObject>();
    } else {
        webServer.send(400, "application/json", "Bad Request");
        return;
    }

    if(webServer.method() == HTTP_POST || webServer.method() == HTTP_PUT) {
        configOutput(settings, TAG_HTTP); // Log input JSON config

        if(!strcasecmp(endpoint_key, FP_HASP)) {
            haspSetConfig(settings);
        } else if(!strcasecmp(endpoint_key, FP_GUI)) {
            guiSetConfig(settings);
        } else if(!strcasecmp(endpoint_key, FP_DEBUG)) {
            debugSetConfig(settings);
        } else
#if HASP_USE_WIFI > 0
            if(!strcasecmp(endpoint_key, FP_WIFI)) {
            wifiSetConfig(settings);
        } else if(!strcasecmp(endpoint_key, FP_TIME)) {
            timeSetConfig(settings);
        } else
#endif
#if HASP_USE_WIREGUARD > 0
            if(!strcasecmp(endpoint_key, FP_WG)) {
            wgSetConfig(settings);
        } else
#endif
#if HASP_USE_MQTT > 0
            if(!strcasecmp(endpoint_key, FP_MQTT)) {
            mqttSetConfig(settings);
        } else
#endif
#if HASP_USE_FTP > 0
            if(!strcasecmp(endpoint_key, FP_FTP)) {
            ftpSetConfig(settings);
        } else
#endif
#if HASP_USE_HTTP > 0
            if(!strcasecmp(endpoint_key, FP_HTTP)) {
            httpSetConfig(settings);
        } else
#endif
#if HASP_USE_ARDUINOOTA > 0 || HASP_USE_HTTP_UPDATE > 0
            if(!strcasecmp(endpoint_key, FP_OTA)) {
            otaSetConfig(settings);
        } else
#endif
        {
            LOG_WARNING(TAG_HTTP, F("Invalid module %s"), endpoint_key);
            return;
        }
    }

    settings = doc.to<JsonObject>();
    if(!strcasecmp(endpoint_key, FP_HASP)) {
        haspGetConfig(settings);
    } else if(!strcasecmp(endpoint_key, FP_GUI)) {
        guiGetConfig(settings);
    } else if(!strcasecmp(endpoint_key, FP_DEBUG)) {
        debugGetConfig(settings);
    } else
#if HASP_USE_WIFI > 0
        if(!strcasecmp(endpoint_key, FP_WIFI)) {
        wifiGetConfig(settings);
    } else if(!strcasecmp(endpoint_key, FP_TIME)) {
        timeGetConfig(settings);
    } else
#endif
#if HASP_USE_WIREGUARD > 0
        if(!strcasecmp(endpoint_key, FP_WG)) {
        wgGetConfig(settings);
    } else
#endif
#if HASP_USE_MQTT > 0
        if(!strcasecmp(endpoint_key, FP_MQTT)) {
        mqttGetConfig(settings);
    } else
#endif
#if HASP_USE_FTP > 0
        if(!strcasecmp(endpoint_key, FP_FTP)) {
        ftpGetConfig(settings);
    } else
#endif
#if HASP_USE_HTTP > 0
        if(!strcasecmp(endpoint_key, FP_HTTP)) {
        httpGetConfig(settings);
    } else
#endif
#if HASP_USE_ARDUINOOTA > 0 || HASP_USE_HTTP_UPDATE > 0
        if(!strcasecmp(endpoint_key, FP_OTA)) {
        otaGetConfig(settings);
    } else
#endif
    {
        webServer.send(400, "application/json", "Bad Request");
        return;
    }
    configOutput(settings, TAG_HTTP); // Log current JSON config

    LOG_DEBUG(TAG_HTTP, "%s - %d", __FILE__, __LINE__);
    // Mask non-blank passwords
    if(!settings[FPSTR(FP_CONFIG_PASS)].isNull() && settings[FPSTR(FP_CONFIG_PASS)].as<String>().length() != 0) {
        settings[FPSTR(FP_CONFIG_PASS)] = D_PASSWORD_MASK;
    }

    LOG_DEBUG(TAG_HTTP, "%s - %d", __FILE__, __LINE__);
    // doc.shrinkToFit();
    LOG_DEBUG(TAG_HTTP, "%s - %d", __FILE__, __LINE__);
    const size_t size = measureJson(doc);
    LOG_DEBUG(TAG_HTTP, "%s - %d", __FILE__, __LINE__);
    char jsondata[size];
    LOG_DEBUG(TAG_HTTP, "%s - %d", __FILE__, __LINE__);
    memset(jsondata, 0, size);
    LOG_DEBUG(TAG_HTTP, "%s - %d", __FILE__, __LINE__);
    serializeJson(doc, jsondata, size);
    LOG_DEBUG(TAG_HTTP, "%s - %d", __FILE__, __LINE__);
    //  webServer.send(200, contentType.c_str(), jsondata);
    webServer.setContentLength(size);
    LOG_DEBUG(TAG_HTTP, "%s - %d", __FILE__, __LINE__);
    webServer.send(200, "application/json", "");
    LOG_DEBUG(TAG_HTTP, "%s - %d", __FILE__, __LINE__);
    webServer.sendContent(jsondata, size);
    LOG_DEBUG(TAG_HTTP, "%s - %d", __FILE__, __LINE__);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
static void http_handle_about()
{ // http://plate01/about
    if(!http_is_authenticated("about")) return;

    const char* form = R"(
<template id="credit-template">
<h3>{{model.t}}</h3>
<p><span v-t="'about.copyright'"></span>
{{model.y}} {{model.a}}
<span v-if="model.r && !!model.r" v-t="'about.rights'"></span><br>
<span v-if="model.l && !!model.l" v-t="'about.' + model.l"></span></p>
</template>

<h3>openHASP</h3><p>Copyright 2019-2024 Francis Van Roie</br>MIT License</p>
<p v-t="'about.clause1'"></p>
<p v-t="'about.clause2'"></p>
<p v-t="'about.clause3'"></p>
<a v-t="'home.btn'" href="/"></a>
<hr>
<p v-t="'about.credits'"></p>
<div v-cloak v-if="licenseData">
<p v-for="license in licenseData" v-scope="Credits(license) "></p>
</div><div>
<p v-for="license in licenseApp" v-scope="Credits(license) "></p>
</div>  
)";
    http_send_content(&form, 1);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
static void http_handle_info()
{ // http://plate01/
    if(!http_is_authenticated("info")) return;

    const char* html[20];
    int i   = 0;
    int len = (sizeof(html) / sizeof(html[0])) - 1;

    html[min(i++, len)] = "<h1>";
    html[min(i++, len)] = haspDevice.get_hostname();
    html[min(i++, len)] = "</h1><hr>";
    html[min(i++, len)] = R"(
<table v-cloak v-if="info">
<th v-if="info.openHASP" colspan="2">openHASP</th>
<tr v-for="(item, key) in info.openHASP"><td v-t="key"><td v-if="item">{{ item }}</td></tr>
<th v-if="info['Device Memory']" colspan="2">Device Memory</th>
<tr v-for="(item, key) in info['Device Memory']"><td v-t="key"></td><td v-if="item">{{ item }}</td></tr>
<th v-if="info['LVGL Memory']" colspan="2">LVGL Memory</th>
<tr v-for="(item, key) in info['LVGL Memory']"><td v-t="key"></td><td v-if="item">{{ item }}</td></tr>
<th v-if="info.MQTT" colspan="2">MQTT</th>
<tr v-for="(item, key) in info.MQTT"><td v-t="key"></td><td v-if="item">{{ item }}</td></tr>
<th v-if="info.Wifi" colspan="2">Wifi</th>
<tr v-for="(item, key) in info.Wifi"><td v-t="key"></td><td v-if="item">{{ item }}</td></tr>
<th v-if="info.WireGuard" colspan="2">WireGuard</th>
<tr v-for="(item, key) in info.WireGuard"><td v-t="key"></td><td v-if="item">{{ item }}</td></tr>
<th v-if="info.Module" colspan="2">Module</th>
<tr v-for="(item, key) in info.Module"><td v-t="key"></td><td v-if="item">{{ item }}</td></tr>
</table>)";
    html[min(i++, len)] = R"(<a v-t="'home.btn'" href="/"></a>)";
    http_send_content(html, min(i, len));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
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
static void http_upload_progress()
{
    if(millis() - htppLastLoopTime >= 1250) {
        long t = webServer.header("Content-Length").toInt();
        LOG_VERBOSE(TAG_HTTP, F(D_BULLET "Uploaded %u / %d bytes"), upload->totalSize + upload->currentSize, t);
        htppLastLoopTime = millis();
        if(t > 0) t = (upload->totalSize + upload->currentSize) * 100 / t;
        haspProgressVal(t);
    }
    // if(t > 0) t = (upload->totalSize + upload->currentSize) * 100 / t;
    // haspProgressVal(t);
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
    Update.abort();
    Update.end(false);
#endif
}

static void webUpdateReboot()
{
    LOG_INFO(TAG_HTTP, F("Update Success: %u bytes received. Rebooting..."), upload->totalSize);

    { // Send Content
        String httpMessage((char*)0);
        httpMessage.reserve(256);
        httpMessage += "<h1>";
        httpMessage += haspDevice.get_hostname();
        httpMessage += "</h1><hr>";
        httpMessage += F("<b>Upload complete. Rebooting device, please wait...</b>");

        webSendHtmlHeader(haspDevice.get_hostname(), httpMessage.length(), 10);
        webServer.sendContent(httpMessage);
    }
    webSendFooter();
    dispatch_reboot(true); // Save the current config
}

static void webHandleFirmwareUpload()
{
    upload = &webServer.upload();

    switch(upload->status) {

        case UPLOAD_FILE_START: {
            if(!http_is_authenticated("update")) return;

            // WiFiUDP::stopAll();

            int command = webServer.arg("cmd").toInt();
            size_t size = 0;
            if(command == U_FLASH) {
                LOG_TRACE(TAG_HTTP, F("Update flash: %s"), upload->filename.c_str());
                size = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
#ifdef ESP32
            } else if(command == U_SPIFFS) {
                LOG_TRACE(TAG_HTTP, F("Update filesystem: %s"), upload->filename.c_str());
                size = UPDATE_SIZE_UNKNOWN;
#endif
            }
            haspProgressMsg(upload->filename.c_str());
            htppLastLoopTime = millis();

            // if(!Update.begin(UPDATE_SIZE_UNKNOWN)) { // start with max available size
            //  const char label[] = "spiffs";
            if(!Update.begin(size, command, -1, 0U)) { // start with max available size
                webUpdatePrintError();
            }
            break;
        }

        case UPLOAD_FILE_WRITE: // flashing firmware to ESP
            if(!Update.isFinished()) {
                if(Update.write(upload->buf, upload->currentSize) != upload->currentSize) {
                    webUpdatePrintError();
                } else {
                    http_upload_progress();
                }
            }
            break;

        case UPLOAD_FILE_END:
            haspProgressVal(100);
            if(Update.end(true)) { // true to set the size to the current progress
                haspProgressMsg(D_OTA_UPDATE_APPLY);
                webUpdateReboot();
            } else {
                webUpdatePrintError();
            }
            break;

        default:;
    }
}
#endif

static inline int handleFilesystemFile(String path)
{
    if(!http_is_authenticated()) return false;

#if HASP_USE_SPIFFS > 0 || HASP_USE_LITTLEFS > 0
    // path = webServer.urlDecode(path).substring(0, 31);
    if(path.endsWith("/")) {
        path += F("index.html");
    }
    String pathWithGz = path + F(".gz");
    // String pathWithBr = path + F(".br");
    String contentType((char*)0);
    contentType = http_get_content_type(path);

    if(HASP_FS.exists(pathWithGz) /* || HASP_FS.exists(pathWithBr) */ || HASP_FS.exists(path)) {

        if(webServer.hasArg("download")) contentType = F("application/octet-stream");

        if(!HASP_FS.exists(path) && HASP_FS.exists(pathWithGz))
            path = pathWithGz; // Only use .gz if normal file doesn't exist
        // if(!HASP_FS.exists(path) && HASP_FS.exists(pathWithBr))
        //     path = pathWithBr; // Only use .gz if normal file doesn't exist

        LOG_TRACE(TAG_HTTP, D_HTTP_SENDING_PAGE, path.c_str(), webServer.client().remoteIP().toString().c_str());

        String configFile((char*)0); // Verify if the file is config.json
        configFile = FPSTR(FP_HASP_CONFIG_FILE);

        if(path.endsWith(configFile.c_str())) { // "//config.json" is also a valid path!
            DynamicJsonDocument settings(MAX_CONFIG_JSON_ALLOC_SIZE);
            DeserializationError error = configParseFile(configFile, settings);

            if(error) return 500; // Internal Server Error

            configMaskPasswords(settings); // Output settings to the client with masked passwords!
            char buffer[1024];
            size_t len = serializeJson(settings, buffer, sizeof(buffer));
            webServer.setContentLength(len);
            webServer.send(200, contentType, buffer);

        } else {
            File file       = HASP_FS.open(path, "r");
            time_t modified = file.getLastWrite();
            String etag((char*)0);
            etag.reserve(64);

            if(webServer.hasHeader("If-None-Match")) {
                etag = webServer.header("If-None-Match");
                etag.replace("\"", "");
                LOG_DEBUG(TAG_HTTP, F("If-None-Match: %s"), etag.c_str());
                if(modified > 0 && modified == atol(etag.c_str())) { // Not Changed
                    file.close();                                    // Skip reading the file contents
                    http_send_etag(etag);                            // Reuse same ETag
                    webServer.send(304, contentType, "");            // Use correct mimetype
                    return 304;                                      // Not Modified
                }
            }

            etag = (String)(modified);
            http_send_etag(etag); // Send new tag with modification datetime

            /* Only needed for brotli encoding. Gzip is handled automatically in streamfile() */
            /* Brotli is not supported over HTTP/1.1 */
            // if(path.endsWith(".br") && contentType != String(FPSTR(mime::mimeTable[mime::type::none].mimeType))) {
            //     webServer.sendHeader("Content-Encoding", "br");
            //     webServer.streamFile(file, contentType);
            //     LOG_DEBUG(TAG_HTTP, F("Headers: OK"));

            // } else {
            // Stream other files directly from filesystem
            webServer.streamFile(file, contentType);
            LOG_DEBUG(TAG_HTTP, F("If-None-Match: %s"), etag.c_str());
            //}
            file.close();
        }

        return 200; // OK
    }

#endif // HASP_USE_SPIFFS || HASP_USE_LITTLEFS

    return 404; // File Not found on Flash
}

#if HASP_USE_SPIFFS > 0 || HASP_USE_LITTLEFS > 0
static void handleFileUpload()
{
    // if(!http_is_authenticated()) return;   // Moved to UPLOAD_FILE_START

    if(webServer.uri() != "/edit") {
        return;
    }
    upload = &webServer.upload();
    switch(upload->status) {
        case UPLOAD_FILE_START: {
            if(!http_is_authenticated("fileupload")) return;
            String filename((char*)0);
            filename.reserve(64);
            filename = upload->filename;
            if(!filename.startsWith("/")) {
                filename = "/";
                filename += upload->filename;
            }
            fsUploadFile = HASP_FS.open(filename, "w");
            if(fsUploadFile) {
                if(!fsUploadFile || fsUploadFile.isDirectory()) {
                    // Clear upload filesize, fix Response Content-Length
                    webServer.setContentLength(CONTENT_LENGTH_NOT_SET);
                    webServer.send_P(400, PSTR("text/plain"), PSTR("Invalid filename"));
                    fsUploadFile.close();
                    fsUploadFile = File();
                    LOG_WARNING(TAG_HTTP, D_FILE_SAVE_FAILED, filename.c_str());
                } else {
                    LOG_TRACE(TAG_HTTP, F("handleFileUpload Name: %s"), filename.c_str());
                    haspProgressMsg(fsUploadFile.name());
                    htppLastLoopTime = millis();
                }
            } else {
                // Clear upload filesize, fix Response Content-Length
                webServer.setContentLength(CONTENT_LENGTH_NOT_SET);
                webServer.send_P(400, PSTR("text/plain"), PSTR("Could not open file for writing"));
                LOG_ERROR(TAG_HTTP, F("Could not open file %s for writing"), filename.c_str());
            }
            break;
        }
        case UPLOAD_FILE_WRITE: {
            if(fsUploadFile) {
                if(fsUploadFile.write(upload->buf, upload->currentSize) != upload->currentSize) {
                    // Clear upload filesize, fix Response Content-Length
                    webServer.setContentLength(CONTENT_LENGTH_NOT_SET);
                    webServer.send_P(400, PSTR("text/plain"), PSTR("Failed to write received data to file"));
                    fsUploadFile.close();
                    fsUploadFile = File();
                    LOG_ERROR(TAG_HTTP, "Failed to write received data to file");
                } else {
                    http_upload_progress(); // Moved to httpEverySecond Loop
                }
            }
            break;
        }
        case UPLOAD_FILE_END: {
            if(fsUploadFile) {
                LOG_INFO(TAG_HTTP, F("Uploaded %s (%u bytes)"), fsUploadFile.name(), upload->totalSize);
                fsUploadFile.close();

                // Redirect to /config/hasp page. This flushes the web buffer and frees the memory
                // webServer.sendHeader(String("Location"), String(F("/config/hasp")), true);

                // Clear upload filesize, fix Response Content-Length
                webServer.setContentLength(CONTENT_LENGTH_NOT_SET);
                webServer.send_P(200, PSTR("text/plain"), PSTR("Upload OK"));
            }
            haspProgressVal(255);
            break;
        }
        default:
            LOG_WARNING(TAG_HTTP, "File upload aborted");
            webServer.send_P(400, PSTR("text/plain"), PSTR("File upload aborted"));
            fsUploadFile.close();
            fsUploadFile = File();
    }
}

static void handleFileDelete()
{
    if(!http_is_authenticated("filedelete")) return;

    const char mimetype[] = "text/plain";

    if(!webServer.hasArg("path")) {
        return webServer.send(500, mimetype, "BAD ARGS");
    }
    String path = webServer.arg("path");
    LOG_TRACE(TAG_HTTP, F("handleFileDelete: %s"), path.c_str());
    if(path == "/") {
        return webServer.send(500, mimetype, "BAD PATH");
    }
    if(!HASP_FS.exists(path)) {
        return webServer.send(404, mimetype, "FileNotFound");
    }
    bool result;
    if(path.endsWith("/")) {
        path.remove(path.length() - 1);
        result = HASP_FS.rmdir(path);
    } else {
        result = HASP_FS.remove(path);
    }
    if(result) {
        webServer.send(200, mimetype, String(""));
    } else {
        webServer.send(405, mimetype, "RemoveFailed");
    }
}

static void handleFileCreate()
{
    if(!http_is_authenticated("filecreate")) return;

    if(webServer.args() == 0) {
        return webServer.send(500, PSTR("text/plain"), PSTR("BAD ARGS"));
    }

    if(webServer.hasArg("path")) {
        String path = webServer.arg("path");
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
    }
    if(webServer.hasArg("init")) {
        dispatch_wakeup(TAG_HTTP);
        hasp_init();
    }
    if(webServer.hasArg("load")) {
        dispatch_wakeup(TAG_HTTP);
        hasp_load_json();
    }
    if(webServer.hasArg("page")) {
        dispatch_wakeup(TAG_HTTP);
        dispatch_page(NULL, webServer.arg("page").c_str(), TAG_HTTP);
        // uint8_t pageid = atoi(webServer.arg("page").c_str());
        // dispatch_set_page(pageid, LV_SCR_LOAD_ANIM_NONE);
    }
    webServer.send(200, PSTR("text/plain"), "");
}

static void handleFileList()
{
    if(!http_is_authenticated("filelist")) return;

    if(!webServer.hasArg("dir")) {
        webServer.send(500, PSTR("text/plain"), PSTR("BAD ARGS"));
        return;
    }

    String path = webServer.arg("dir");
    // LOG_TRACE(TAG_HTTP, F("handleFileList: %s"), path.c_str());
    // path.clear();

#if defined(ARDUINO_ARCH_ESP32)
    File root = HASP_FS.open(path.c_str(), FILE_READ);
    File file = root.openNextFile();
    String output((char*)0);
    output.reserve(HTTP_PAGE_SIZE);
    output = "[";

    while(file) {
        if(output != "[") {
            output += ',';
        }
        bool isDir = file.isDirectory();
        output += F("{\"type\":\"");
        output += (isDir) ? "dir" : "file";
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
    String output((char*)0);
    output.reserve(HTTP_PAGE_SIZE);
    output = "[";

    while(dir.next()) {
        File entry = dir.openFile("r");
        if(output != "[") {
            output += ',';
        }
        bool isDir = false;
        output += F("{\"type\":\"");
        output += (isDir) ? "dir" : "file";
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

#ifdef HTTP_LEGACY

////////////////////////////////////////////////////////////////////////////////////////////////////
#if HASP_USE_CONFIG > 0
static void http_handle_config()
{ // http://plate01/config
    if(!http_is_authenticated("config")) return;

    bool updated = http_save_config();

// Reboot after saving wifi config in AP mode
#if HASP_USE_WIFI > 0 && !defined(STM32F4xx)
    if(WiFi.getMode() != WIFI_STA) {
        http_handle_reboot();
    }
#endif

    const char* html[20];
    int i   = 0;
    int len = (sizeof(html) / sizeof(html[0])) - 1;

    html[min(i++, len)] = "<h1>";
    html[min(i++, len)] = haspDevice.get_hostname();
    html[min(i++, len)] = "</h1><hr>";
    if(updated) {
        html[min(i++, len)] = R"(<p class='info'>)" D_HTTP_CONFIG_CHANGED "</p>";
    }

#if HASP_USE_WIFI > 0
    html[min(i++, len)] = R"(<a href="/config/wifi" v-t="'wifi.btn'"></a>)";
#endif
#if HASP_USE_WIREGUARD > 0
    html[min(i++, len)] = R"(<a href="/config/wireguard" v-t="'wg.btn'"></a>)";
#endif
#if HASP_USE_MQTT > 0
    html[min(i++, len)] = R"(<a href="/config/mqtt" v-t="'mqtt.btn'"></a>)";
#endif
    html[min(i++, len)] = R"(<a href="/config/http" v-t="'http.btn'"></a>)";
#if HASP_USE_FTP > 0
    html[min(i++, len)] = R"(<a href="/config/ftp" v-t="'ftp.btn'"></a>)";
#endif
    html[min(i++, len)] = R"(<a href="/config/gui" v-t="'gui.btn'"></a>)";
#if HASP_USE_GPIO > 0
    html[min(i++, len)] = R"(<a href="/config/gpio" v-t="'gpio.btn'"></a>)";
#endif
    html[min(i++, len)] = R"(<a href="/config/time" v-t="'time.btn'"></a>)";
    html[min(i++, len)] = R"(<a href="/config/debug" v-t="'debug.btn'"></a>)";
    html[min(i++, len)] = R"(<a href="/config/reset" class="red" v-t="'reset.btn'"></a>)";
    html[min(i++, len)] = R"(<a v-t="'home.btn'" href="/"></a>)";
    http_send_content(html, min(i, len));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
#if HASP_USE_MQTT > 0
static void http_handle_mqtt()
{ // http://plate01/config/mqtt
    if(!http_is_authenticated(F("config/mqtt"))) return;

    const char* html[20];
    int i   = 0;
    int len = (sizeof(html) / sizeof(html[0])) - 1;

    html[min(i++, len)] = "<h1>";
    html[min(i++, len)] = haspDevice.get_hostname();
    html[min(i++, len)] = "</h1><hr>";

    html[min(i++, len)] = R"(
<h2 v-t="'mqtt.title'"></h2>
<div class="container" v-cloak v-if="config.mqtt">
<form @submit.prevent="submitOldConfig('mqtt') ">
<div class="row gap">
<div class="col-25"><label class="required" for="name" v-t="'mqtt.name'"></label></div>
<div class="col-75"><input required="" type="text" id="name" maxlength="63" pattern="[a-z0-9_]*" placeholder="Plate Name" v-model="config.mqtt.name"></div>
</div>
<div class="row">
<div class="col-25"><label for="host" v-t="'mqtt.host'"></label></div>
<div class="col-75"><input type="text" id="host" maxlength="127" placeholder="Server Name" v-model="config.mqtt.host"></div>
</div>
<div class="row gap">
<div class="col-25"><label for="port" v-t="'mqtt.port'"></label></div>
<div class="col-75"><input type="number" id="port" min="0" max="65535" placeholder="1883" v-model="config.mqtt.port"></div>
</div><div class="row">
<div class="col-25"><label for="user" v-t="'user'"></label></div>
<div class="col-75"><input type="text" id="user" maxlength="31" :placeholder="t('user') " v-model="config.mqtt.user"></div>
</div>
<div class="row gap">
<div class="col-25"><label for="pass" v-t="'pass'"></label></div>
<div class="col-75"><input type="password" id="pass" maxlength="63" :placeholder="t('pass') " v-model="config.mqtt.pass"></div>
</div>

<div class="row">
<div class="col-25"><label for="node_t" v-t="'mqtt.node_t'"></label></div>
<div class="col-75"><input type="text" id="node_t" maxlength="128" placeholder="Node Topic" v-model="config.mqtt.topic.node"></div>
</div>
<div class="row">
<div class="col-25"><label for="group_t" v-t="'mqtt.group_t'"></label></div>
<div class="col-75"><input type="text" id="group_t" maxlength="128" placeholder="Group Topic" v-model="config.mqtt.topic.group"></div>
</div>
<div class="row">
<div class="col-25"><label for="broadcast_t" v-t="'mqtt.broadcast_t'"></label></div>
<div class="col-75"><input type="text" id="broadcast_t" maxlength="128" placeholder="Broadcast Topic" v-model="config.mqtt.topic.broadcast"></div>
</div>
<div class="row">
<div class="col-25"><label for="hass_t" v-t="'mqtt.hass_t'"></label></div>
<div class="col-75"><input type="text" id="hass_t" maxlength="128" placeholder="Home Automation LWT Topic" v-model="config.mqtt.topic.hass"></div>
</div>)";

    html[min(i++, len)] = R"(<button type="submit" v-t="'save'"></button></form></div>)";
    html[min(i++, len)] = R"(<a v-t="'config.btn'" href="/config"></a>)";
    http_send_content(html, min(i, len));
}
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
static void http_handle_gui()
{ // http://plate01/config/wifi
    if(!http_is_authenticated(F("config/gui"))) return;

    const char* html[20];
    int i   = 0;
    int len = (sizeof(html) / sizeof(html[0])) - 1;

    html[min(i++, len)] = "<h1>";
    html[min(i++, len)] = haspDevice.get_hostname();
    html[min(i++, len)] = "</h1><hr>";
    html[min(i++, len)] = R"(
<h2 v-t="'gui.title'"></h2>
<div class="container" v-cloak v-if="config.gui">
<form @submit.prevent="submitOldConfig('gui') ">
<div class="row">
<div class="col-25"><label for="idle1">Short Idle</label></div>
<div class="col-75"><input type="number" id="idle1" min="0" max="32400" v-model="config.gui.idle1"></div>
</div>
<div class="row gap">
<div class="col-25"><label for="idle2">Long Idle</label></div>
<div class="col-75"><input type="number" id="idle2" min="0" max="32400" v-model="config.gui.idle2"></div>
</div>
<div class="row">
<div class="col-25"><label for="rotate">Orientation</label></div>
<div class="col-75"><select id="rotate" v-model="config.gui.rotate">
<option value="0">0 degrees</option>
<option value="1">90 degrees</option>
<option value="2">180 degrees</option>
<option value="3">270 degrees</option>
<option value="6">0 degrees - mirrored</option>
<option value="7">90 degrees - mirrored</option>
<option value="4">180 degrees - mirrored</option>
<option value="5">270 degrees - mirrored</option>
</select></div>
</div>)";
#ifdef NOISE_REDUCTION
    html[min(i++, len)] = R"(<div class="row">
<div class="col-25"><label for="noise_reduction">Noise Reduction</label></div>
<div class="col-75"><input type="number" id="noise_reduction" min="0" max="15" v-model="config.gui.noise_reduction"></div>
</div>)";
#endif
    html[min(i++, len)] = R"(<div class="row">
<div class="col-25"></div>
<div class="col-75"><input type="checkbox" id="invert" @vue:mounted="config.gui.invert = !!config.gui.invert" v-model="config.gui.invert">
<label for="invert">Invert Colors</label></div>
</div>
<div class="row gap">
<div class="col-25"></div>
<div class="col-75"><input type="checkbox" id="cursor" @vue:mounted="config.gui.cursor = !!config.gui.cursor" v-model="config.gui.cursor">
<label for="cursor">Show Pointer</label></div>
</div>
<div class="row">
<div class="col-25"><label for="group">Backlight Pin</label></div>
<div class="col-75"><select id="bckl" v-model="config.gui.bckl">)";

    String httpGpio((char*)0);
    httpGpio.reserve(256);
    httpGpio += getOption(-1, "None");
#if defined(ARDUINO_ARCH_ESP32)
    char buffer[10];
    for(uint8_t gpio = 0; gpio < NUM_DIGITAL_PINS; gpio++) {
        if(!gpioIsSystemPin(gpio)) {
            snprintf_P(buffer, sizeof(buffer), PSTR("GPIO %d"), gpio);
            httpGpio += getOption(gpio, buffer);
        } else {
            LOG_WARNING(TAG_HTTP, F("pin %d"), gpio);
        }
    }
#endif
    html[min(i++, len)] = httpGpio.c_str();
    html[min(i++, len)] = R"(
</select></div>
</div>
<div class="row">
<div class="col-25"></div>
<div class="col-75"><input type="checkbox" id="bcklinv" @vue:mounted="config.gui.bcklinv=!!config.gui.bcklinv" v-model="config.gui.bcklinv">
    <label for="bcklinv">Invert Backlight</label></div>
</div>)";
    html[min(i++, len)] = R"(<button type="submit" v-t="'save'"></button></form></div>)";
#if TOUCH_DRIVER == 0x2046 && defined(TOUCH_CS)
    html[min(i++, len)] = R"(<a v-t="'gui.calibrate'" href="/config/gui?cal=1"></a>)";
#endif
    html[min(i++, len)] = R"(<a v-t="'gui.antiburn'" href="/config/gui?brn=1"></a>)";
    html[min(i++, len)] = R"(<a v-t="'config.btn'" href="/config"></a>)";
    http_send_content(html, min(i, len));

    { // Execute Actions
        if(webServer.hasArg("cal")) dispatch_calibrate(NULL, NULL, TAG_HTTP);
        if(webServer.hasArg("brn")) dispatch_antiburn(NULL, "on", TAG_HTTP);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
static void webHandleHttpConfig()
{ // http://plate01/config/http
    if(!http_is_authenticated(F("config/http"))) return;

    const char* html[20];
    int i   = 0;
    int len = (sizeof(html) / sizeof(html[0])) - 1;

    html[min(i++, len)] = "<h1>";
    html[min(i++, len)] = haspDevice.get_hostname();
    html[min(i++, len)] = "</h1><hr>";
    html[min(i++, len)] = R"(
<h2 v-t="'http.title'"></h2>
<div class="container" v-cloak v-if="config.http">
<form @submit.prevent="submitOldConfig('http') ">
<div class="row">
<div class="col-25"><label for="user" v-t="'user'"></label></div>
<div class="col-75"><input type="text" id="user" maxlength="31" :placeholder="t('user') " v-model="config.http.user"></div>
</div>
<div class="row">
<div class="col-25"><label for="user" v-t="'pass'"></label></div>
<div class="col-75"><input type="password" id="pass" maxlength="63" :placeholder="t('pass') " v-model="config.http.pass"></div>
</div>)";
    html[min(i++, len)] = R"(<button type="submit" v-t="'save'"></button></form></div>)";
    html[min(i++, len)] = R"(<a v-t="'config.btn'" href="/config"></a>)";
    http_send_content(html, min(i, len));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
static void http_handle_ftp()
{ // http://plate01/config/http
    if(!http_is_authenticated(F("config/ftp"))) return;

    const char* html[20];
    int i   = 0;
    int len = (sizeof(html) / sizeof(html[0])) - 1;

    html[min(i++, len)] = "<h1>";
    html[min(i++, len)] = haspDevice.get_hostname();
    html[min(i++, len)] = "</h1><hr>";
    html[min(i++, len)] = R"(
<h2 v-t="'ftp.title'"></h2>
<div class="container" v-if="config.ftp">
<form @submit.prevent="submitOldConfig('ftp') ">
<div class='row'>
<div class="col-25"><label for="user" v-t="'user'"></label></div>
<div class='col-75'><input type='text' id='user' maxlength=31 :placeholder="t('user') " v-model="config.ftp.user"></div>
</div>
<div class='row gap'>
<div class="col-25"><label for="pass" v-t="'pass'"></label></div>
<div class='col-75'><input type='password' id='pass' maxlength=63 :placeholder="t('pass') " v-model="config.ftp.pass"></div>
</div>
<div class='row'>
<div class="col-25"><label for="port" v-t="'ftp.port'"></label></div>
<div class='col-75'><input type='number' id='port' min='0' max='65535' v-model="config.ftp.port"></div>
</div>
<div class='row'>
<div class="col-25"><label for="pasv" v-t="'ftp.pasv'"></label></div>
<div class='col-75'><input type='number' id='pasv' min='0' max='65535' v-model="config.ftp.pasv"></div>
</div>)";
    html[min(i++, len)] = R"(<button type="submit" v-t="'save'"></button></form></div>)";
    html[min(i++, len)] = R"(<a v-t="'config.btn'" href="/config"></a>)";
    http_send_content(html, min(i, len));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
static void http_handle_time()
{ // http://plate01/config/time
    if(!http_is_authenticated(F("config/time"))) return;

    const char* html[20];
    int i   = 0;
    int len = (sizeof(html) / sizeof(html[0])) - 1;

    html[min(i++, len)] = "<h1>";
    html[min(i++, len)] = haspDevice.get_hostname();
    html[min(i++, len)] = "</h1><hr>";
    html[min(i++, len)] = R"(
<template id="region-template">
<div class="row">
<div class="col-25"><label for="region" v-t="'time.region'"></label></div>
<div class="col-75">
<select id="region" v-model="config.time.region" @change='setTimeout(function myFunction() { document.getElementById("zone").dispatchEvent(new Event("change")) }, 0)'>
<option value="etc" v-t="'region.etc'"></option>
<optgroup :label="t('region.continents') ">
<option value="af" v-t="'region.af'"></option>
<option value="na" v-t="'region.na'"></option>
<option value="sa" v-t="'region.sa'"></option>
<option value="as" v-t="'region.as'"></option>
<option value="au" v-t="'region.au'"></option>
<option value="eu" v-t="'region.eu'"></option>
<option value="aq" v-t="'region.aq'"></option>
</optgroup>
<optgroup :label="t('region.islands') ">
<option value="at" v-t="'region.at'"></option>
<option value="in" v-t="'region.in'"></option>
<option value="pa" v-t="'region.pa'"></option>
</optgroup>
</select>
</div>
</div>
<div class="row gap">
<div class="col-25"><label for="zone" v-t="'time.zone'"></label></div>
<div class="col-75">
<select id="zone" v-model="config.time.zone">
<!-- option v-for="city in list(config.time.region) " :value="city" v-ts="config.time.region+'/'+city"></option -->
<optgroup :label="t('region.etc') " v-if="config.time.region=='etc'"><option v-for="tz in list('etc') " :value="tz" v-ts="tz"></option></optgroup>
<optgroup :label="t('region.af') " v-if="config.time.region=='af'"><option v-for="tz in list('af') " :value="tz" v-ts="tz"></option></optgroup>
<optgroup :label="t('region.na') " v-if="config.time.region=='na'"><option v-for="tz in list('na') " :value="tz" v-ts="tz"></option></optgroup>
<optgroup :label="t('region.sa') " v-if="config.time.region=='sa'"><option v-for="tz in list('sa') " :value="tz" v-ts="tz"></option></optgroup>
<optgroup :label="t('region.as') " v-if="config.time.region=='as'"><option v-for="tz in list('as') " :value="tz" v-ts="tz"></option></optgroup>
<optgroup :label="t('region.au') " v-if="config.time.region=='au'"><option v-for="tz in list('au') " :value="tz" v-ts="tz"></option></optgroup>
<optgroup :label="t('region.eu') " v-if="config.time.region=='eu'"><option v-for="tz in list('eu') " :value="tz" v-ts="tz"></option></optgroup>
<optgroup :label="t('region.aq') " v-if="config.time.region=='aq'"><option v-for="tz in list('aq') " :value="tz" v-ts="tz"></option></optgroup>
<optgroup :label="t('region.at') " v-if="config.time.region=='at'"><option v-for="tz in list('at') " :value="tz" v-ts="tz"></option></optgroup>
<optgroup :label="t('region.in') " v-if="config.time.region=='in'"><option v-for="tz in list('in') " :value="tz" v-ts="tz"></option></optgroup>
<optgroup :label="t('region.pa') " v-if="config.time.region=='pa'"><option v-for="tz in list('pa') " :value="tz" v-ts="tz"></option></optgroup>
</select>
</div>
</div>

<div class="row" v-for="(ntp, id) in config.time.ntp" :key="id">
<div class="col-25"><label for="ntp{{id}}" v-t="'time.ntp'" v-if="!id"></label></div>
<div class="col-75"><input type="text" id="ntp{{id}}" maxlength="128" :placeholder="t('time.ntp'+id) " v-model="config.time.ntp[id]"></div>
</div>
<button type="submit" v-t="'save'"></button>
</template>

<h2 v-t="'time.title'"></h2>
<div class="container" v-if="config.time">
<form @submit.prevent="submitOldConfig('time') " v-scope="RegionItem(locations, regions, i18n) ">
</form>
</div>)";
    html[min(i++, len)] = R"(<a v-t="'config.btn'" href="/config"></a>)";
    http_send_content(html, min(i, len));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
#if HASP_USE_GPIO > 0
static void webHandleGpioConfig()
{ // http://plate01/config/gpio
    if(!http_is_authenticated(F("config/gpio"))) return;
    uint8_t configCount = 0;

    { // Execute Actions
        uint8_t id  = webServer.arg("id").toInt();
        uint8_t pin = webServer.arg("pin").toInt();

        if(webServer.hasArg("save")) {
            uint8_t type    = webServer.arg("type").toInt();
            uint8_t group   = webServer.arg("group").toInt();
            uint8_t pinfunc = webServer.arg("func").toInt();
            bool inverted   = webServer.arg("state").toInt();
            gpioSavePinConfig(id, pin, type, group, pinfunc, inverted);
        }

        if(webServer.hasArg("del")) {
            gpioSavePinConfig(id, pin, hasp_gpio_type_t::FREE, 0, 0, false);
        }
    }

    { // Send Content
        String httpMessage((char*)0);
        httpMessage.reserve(HTTP_PAGE_SIZE);
        httpMessage += "<h1>";
        httpMessage += haspDevice.get_hostname();
        httpMessage += "</h1><hr>";
        httpMessage += F("<h2>" D_HTTP_GPIO_SETTINGS "</h2>");

        httpMessage += F("<form method='POST' action='/config'>");

        httpMessage += F("<table><tr><th>" D_GPIO_PIN "</th><th>Type</th><th>" D_GPIO_GROUP
                         "</th><th>Default</th><th>Action</th></tr>");

        for(uint8_t gpio = 0; gpio < NUM_DIGITAL_PINS; gpio++) {
            for(uint8_t id = 0; id < HASP_NUM_GPIO_CONFIG; id++) {
                hasp_gpio_config_t conf = gpioGetPinConfig(id);
                if((conf.pin == gpio) && gpioConfigInUse(id) && !gpioIsSystemPin(gpio)) {
                    httpMessage += F("<tr><td>");
                    // httpMessage += halGpioName(gpio);
                    httpMessage += haspDevice.gpio_name(gpio).c_str();
                    if(conf.type >= 0x80) {
                        httpMessage += F("</td><td><a href='/config/gpio/input?id=");
                    } else {
                        httpMessage += F("</td><td><a href='/config/gpio/options?id=");
                    }
                    httpMessage += id;
                    httpMessage += F("'>");

                    switch(conf.type) {

                        case hasp_gpio_type_t::BUTTON_TYPE:
                            httpMessage += D_GPIO_BUTTON;
                            break;
                        case hasp_gpio_type_t::SWITCH:
                            httpMessage += D_GPIO_SWITCH;
                            break;
                        case hasp_gpio_type_t::DOOR:
                            httpMessage += "door";
                            break;
                        case hasp_gpio_type_t::GARAGE_DOOR:
                            httpMessage += "garage_door";
                            break;
                        case hasp_gpio_type_t::GAS:
                            httpMessage += "gas";
                            break;
                        case hasp_gpio_type_t::LIGHT:
                            httpMessage += "light";
                            break;
                        case hasp_gpio_type_t::LOCK:
                            httpMessage += "lock";
                            break;
                        case hasp_gpio_type_t::MOISTURE:
                            httpMessage += "moisture";
                            break;
                        case hasp_gpio_type_t::MOTION:
                            httpMessage += "motion";
                            break;
                        case hasp_gpio_type_t::OCCUPANCY:
                            httpMessage += "occupancy";
                            break;
                        case hasp_gpio_type_t::OPENING:
                            httpMessage += "opening";
                            break;
                        case hasp_gpio_type_t::PRESENCE:
                            httpMessage += "presence";
                            break;
                        case hasp_gpio_type_t::PROBLEM:
                            httpMessage += "problem";
                            break;
                        case hasp_gpio_type_t::SAFETY:
                            httpMessage += "Safety";
                            break;
                        case hasp_gpio_type_t::SMOKE:
                            httpMessage += "Smoke";
                            break;
                        case hasp_gpio_type_t::VIBRATION:
                            httpMessage += "Vibration";
                            break;
                        case hasp_gpio_type_t::WINDOW:
                            httpMessage += "Window";
                            break;

                        case hasp_gpio_type_t::TOUCH:
                            httpMessage += D_GPIO_TOUCH;
                            break;
                        case hasp_gpio_type_t::LED:
                            httpMessage += D_GPIO_LED;
                            break;
                        case hasp_gpio_type_t::LED_R:
                            httpMessage += D_GPIO_LED_R;
                            break;
                        case hasp_gpio_type_t::LED_G:
                            httpMessage += D_GPIO_LED_G;
                            break;
                        case hasp_gpio_type_t::LED_B:
                            httpMessage += D_GPIO_LED_B;
                            break;
                        case hasp_gpio_type_t::LIGHT_RELAY:
                            httpMessage += D_GPIO_LIGHT_RELAY;
                            break;
                        case hasp_gpio_type_t::POWER_RELAY:
                            httpMessage += D_GPIO_POWER_RELAY;
                            break;
                        case hasp_gpio_type_t::SHUTTER_RELAY:
                            httpMessage += "SHUTTER_RELAY";
                            break;
                        case hasp_gpio_type_t::PWM:
                            httpMessage += D_GPIO_PWM;
                            break;
                        case hasp_gpio_type_t::HASP_DAC:
                            httpMessage += D_GPIO_DAC;
                            break;

#if defined(LANBONL8)
                            // case hasp_gpio_type_t::SERIAL_DIMMER:
                            //     httpMessage += D_GPIO_SERIAL_DIMMER;
                            //     break;
                        case hasp_gpio_type_t::SERIAL_DIMMER_L8_HD_INVERTED:
                            httpMessage += F("L8-HD (inv.)");
                            break;
                        case hasp_gpio_type_t::SERIAL_DIMMER_L8_HD:
                            httpMessage += F("L8-HD");
                            break;
#endif
                        default:
                            httpMessage += D_GPIO_UNKNOWN;
                    }

                    httpMessage += F("</a></td><td>");
                    httpMessage += conf.group;
                    httpMessage += F("</td><td>");
                    httpMessage += (conf.inverted) ? D_GPIO_STATE_INVERTED : D_GPIO_STATE_NORMAL;

                    httpMessage += ("</td><td><a href='/config/gpio?del=&id=");
                    httpMessage += id;
                    httpMessage += ("&pin=");
                    httpMessage += conf.pin;
                    httpMessage += ("' class='icon trash'></a></td><tr>");
                    configCount++;
                }
            }
        }

        httpMessage += F("</table></form>");

        if(configCount < HASP_NUM_GPIO_CONFIG) {
            httpMessage += F("<a href='gpio/input?id=");
            httpMessage += gpioGetFreeConfigId();
            httpMessage += F("'>" D_HTTP_ADD_GPIO " Input</a>");

            httpMessage += F("<a href='gpio/options?id=");
            httpMessage += gpioGetFreeConfigId();
            httpMessage += F("'>" D_HTTP_ADD_GPIO " Output</a>");
        }

        add_form_button(httpMessage, D_BACK_ICON D_HTTP_CONFIGURATION, F("/config"));

        webSendHtmlHeader(haspDevice.get_hostname(), httpMessage.length(), 0);
        webServer.sendContent(httpMessage);
    }
    webSendFooter();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
static void webHandleGpioOutput()
{ // http://plate01/config/gpio/options
    if(!http_is_authenticated(F("config/gpio/options"))) return;

    { // Send Content
        uint8_t config_id = webServer.arg("id").toInt();

        String httpMessage((char*)0);
        httpMessage.reserve(HTTP_PAGE_SIZE);
        httpMessage += "<h1>";
        httpMessage += haspDevice.get_hostname();
        httpMessage += "</h1><hr>";

        httpMessage += F("<form method='GET' action='/config/gpio'>");
        httpMessage += F("<input type='hidden' name='id' value='");
        httpMessage += config_id;
        httpMessage += F("'>");

        httpMessage += F("<p><b>GPIO Output</b></p>");

        httpMessage += F("<p><b>" D_GPIO_PIN "</b> <select id='pin' name='pin'>");
        hasp_gpio_config_t conf = gpioGetPinConfig(config_id);

        for(uint8_t io = 0; io < NUM_DIGITAL_PINS; io++) {
            if(((conf.pin == io) || !gpioInUse(io)) && !gpioIsSystemPin(io)) {
                httpMessage += getOption(io, haspDevice.gpio_name(io).c_str(), conf.pin);
            }
        }
        httpMessage += F("</select></p>");

        httpMessage += F("<p><b>Type</b> <select id='type' name='type'>");
        httpMessage += getOption(hasp_gpio_type_t::LED, D_GPIO_LED, conf.type);
        httpMessage += getOption(hasp_gpio_type_t::LED_R, D_GPIO_LED_R, conf.type);
        httpMessage += getOption(hasp_gpio_type_t::LED_G, D_GPIO_LED_G, conf.type);
        httpMessage += getOption(hasp_gpio_type_t::LED_B, D_GPIO_LED_B, conf.type);
        httpMessage += getOption(hasp_gpio_type_t::LIGHT_RELAY, D_GPIO_LIGHT_RELAY, conf.type);
        httpMessage += getOption(hasp_gpio_type_t::POWER_RELAY, D_GPIO_POWER_RELAY, conf.type);
        httpMessage += getOption(hasp_gpio_type_t::SHUTTER_RELAY, "Shutter Relay", conf.type);
        httpMessage += getOption(hasp_gpio_type_t::HASP_DAC, D_GPIO_DAC, conf.type);
        // httpMessage += getOption(hasp_gpio_type_t::SERIAL_DIMMER, D_GPIO_SERIAL_DIMMER, conf.type);
#if defined(LANBONL8)
        httpMessage += getOption(hasp_gpio_type_t::SERIAL_DIMMER_L8_HD, F("L8-HD"), conf.type);
        httpMessage += getOption(hasp_gpio_type_t::SERIAL_DIMMER_L8_HD_INVERTED, F("L8-HD (inv.)"), conf.type);
#endif
        if(digitalPinHasPWM(webServer.arg(0).toInt())) {
            httpMessage += getOption(hasp_gpio_type_t::PWM, D_GPIO_PWM, conf.type);
        }
        httpMessage += F("</select></p>");

        httpMessage += F("<p><b>" D_GPIO_GROUP "</b> <select id='group' name='group'>");
        httpMessage += getOption(0, D_GPIO_GROUP_NONE, conf.group);
        String group((char*)0);
        group.reserve(10);
        for(int i = 1; i < 15; i++) {
            group = D_GPIO_GROUP " ";
            group += i;
            httpMessage += getOption(i, group, conf.group);
        }
        httpMessage += F("</select></p>");

        httpMessage += F("<p><b>Value</b> <select id='state' name='state'>");
        httpMessage += getOption(0, D_GPIO_STATE_NORMAL, conf.inverted);
        httpMessage += getOption(1, D_GPIO_STATE_INVERTED, conf.inverted);
        httpMessage += F("</select></p>");

        httpMessage +=
            F("<p><button type='submit' name='save' value='gpio'>" D_HTTP_SAVE_SETTINGS "</button></p></form>");

        httpMessage += PSTR("<p><form method='GET' action='/config/gpio'><button type='submit'>&#8617; " D_HTTP_BACK
                            "</button></form></p>");

        webSendHtmlHeader(haspDevice.get_hostname(), httpMessage.length(), 0);
        webServer.sendContent(httpMessage);
    }
    webSendFooter();

    // if(webServer.hasArg("action")) dispatch_text_line(webServer.arg("action").c_str()); // Security check
}

////////////////////////////////////////////////////////////////////////////////////////////////////
static void webHandleGpioInput()
{ // http://plate01/config/gpio/options
    if(!http_is_authenticated(F("config/gpio/input"))) return;

    { // Send Content
        uint8_t config_id = webServer.arg("id").toInt();

        String httpMessage((char*)0);
        httpMessage.reserve(HTTP_PAGE_SIZE);
        httpMessage += "<h1>";
        httpMessage += haspDevice.get_hostname();
        httpMessage += "</h1><hr>";

        httpMessage += F("<form method='GET' action='/config/gpio'>");
        httpMessage += F("<input type='hidden' name='id' value='");
        httpMessage += config_id;
        httpMessage += F("'>");

        httpMessage += F("<p><b>GPIO Input</b></p>");

        httpMessage += F("<p><b>" D_GPIO_PIN "</b> <select id='pin' name='pin'>");
        hasp_gpio_config_t conf = gpioGetPinConfig(config_id);

        for(uint8_t io = 0; io < NUM_DIGITAL_PINS; io++) {
            if(((conf.pin == io) || !gpioInUse(io)) && !gpioIsSystemPin(io)) {
                httpMessage += getOption(io, haspDevice.gpio_name(io).c_str(), conf.pin);
            }
        }
        httpMessage += F("</select></p>");

        httpMessage += F("<p><b>Type</b> <select id='type' name='type'>");
        httpMessage += getOption(hasp_gpio_type_t::BUTTON_TYPE, D_GPIO_BUTTON, conf.type);
        httpMessage += getOption(hasp_gpio_type_t::SWITCH, D_GPIO_SWITCH, conf.type);
        httpMessage += getOption(hasp_gpio_type_t::DOOR, "door", conf.type);
        httpMessage += getOption(hasp_gpio_type_t::GARAGE_DOOR, "garage_door", conf.type);
        httpMessage += getOption(hasp_gpio_type_t::GAS, "gas", conf.type);
        httpMessage += getOption(hasp_gpio_type_t::LIGHT, "light", conf.type);
        httpMessage += getOption(hasp_gpio_type_t::LOCK, "lock", conf.type);
        httpMessage += getOption(hasp_gpio_type_t::MOISTURE, "moisture", conf.type);
        httpMessage += getOption(hasp_gpio_type_t::MOTION, "motion", conf.type);
        httpMessage += getOption(hasp_gpio_type_t::OCCUPANCY, "occupancy", conf.type);
        httpMessage += getOption(hasp_gpio_type_t::OPENING, "opening", conf.type);
        httpMessage += getOption(hasp_gpio_type_t::PRESENCE, "presence", conf.type);
        httpMessage += getOption(hasp_gpio_type_t::PROBLEM, "problem", conf.type);
        httpMessage += getOption(hasp_gpio_type_t::SAFETY, "Safety", conf.type);
        httpMessage += getOption(hasp_gpio_type_t::SMOKE, "Smoke", conf.type);
        httpMessage += getOption(hasp_gpio_type_t::VIBRATION, "Vibration", conf.type);
        httpMessage += getOption(hasp_gpio_type_t::WINDOW, "Window", conf.type);
        httpMessage += F("</select></p>");

        httpMessage += F("<p><b>" D_GPIO_GROUP "</b> <select id='group' name='group'>");
        httpMessage += getOption(0, D_GPIO_GROUP_NONE, conf.group);
        String group((char*)0);
        group.reserve(10);
        for(int i = 1; i < 15; i++) {
            group = D_GPIO_GROUP " ";
            group += i;
            httpMessage += getOption(i, group, conf.group);
        }
        httpMessage += F("</select></p>");

        httpMessage += F("<p><b>Default State</b> <select id='state' name='state'>");
        httpMessage += getOption(0, "Normally Open", conf.inverted);
        httpMessage += getOption(1, "Normally Closed", conf.inverted);
        httpMessage += F("</select></p>");

        httpMessage += F("<p><b>Resistor</b> <select id='func' name='func'>");
        httpMessage += getOption(hasp_gpio_function_t::INTERNAL_PULLUP, "Internal Pullup", conf.gpio_function);
        httpMessage += getOption(hasp_gpio_function_t::INTERNAL_PULLDOWN, "Internal Pulldown", conf.gpio_function);
        httpMessage += getOption(hasp_gpio_function_t::EXTERNAL_PULLUP, "External Pullup", conf.gpio_function);
        httpMessage += getOption(hasp_gpio_function_t::EXTERNAL_PULLDOWN, "External Pulldown", conf.gpio_function);
        httpMessage += F("</select></p>");

        httpMessage +=
            F("<p><button type='submit' name='save' value='gpio'>" D_HTTP_SAVE_SETTINGS "</button></p></form>");

        httpMessage += PSTR("<p><form method='GET' action='/config/gpio'><button type='submit'>&#8617; " D_HTTP_BACK
                            "</button></form></p>");

        webSendHtmlHeader(haspDevice.get_hostname(), httpMessage.length(), 0);
        webServer.sendContent(httpMessage);
    }
    webSendFooter();

    // if(webServer.hasArg("action")) dispatch_text_line(webServer.arg("action").c_str()); // Security check
}
#endif // HASP_USE_GPIO

////////////////////////////////////////////////////////////////////////////////////////////////////
static void http_handle_debug()
{ // http://plate01/config/debug
    if(!http_is_authenticated(F("config/debug"))) return;

    const char* html[20];
    int i   = 0;
    int len = (sizeof(html) / sizeof(html[0])) - 1;

    html[min(i++, len)] = "<h1>";
    html[min(i++, len)] = haspDevice.get_hostname();
    html[min(i++, len)] = "</h1><hr>";
    html[min(i++, len)] = R"(
<h2 v-t="'debug.title'"></h2>
<div class="container" v-cloak v-if="config.debug">
<form @submit.prevent="submitOldConfig('debug') ">
<div class="row">
<div class="col-25"><label for="baud" v-t="'debug.baud'"></label></div>
<div class="col-75"><select id="baud" v-model="config.debug.baud">
<option value="-1" v-t="'debug.disabled'"></option>
<option value="0" v-t="'debug.default'"></option>
<option value="9600">9600</option>
<option value="19200">19200</option>
<option value="38400">38400</option>
<option value="57600">57600</option>
<option value="74880">74880</option>
<option value="115200">115200</option>
<option value="230400">230400</option>
<option value="460800">460800</option>
<option value="921600">921600</option>
</select></div>
</div>
<div class="row">
<div class="col-25"><label for="tele" v-t="'debug.tele'"></label></div>
<div class="col-75"><input type="number" id="tele" min="0" max="65535" v-model="config.debug.tele"></div>
</div>
<div class="row gap">
<div class="col-25"></div>
<div class="col-75"><input type="checkbox" id="ansi" @vue:mounted="config.debug.ansi=!!config.debug.ansi" v-model="config.debug.ansi">
<label for="ansi" v-t="'debug.ansi'"></label></div>
</div>
)";

#if HASP_USE_SYSLOG > 0
    html[min(i++, len)] = R"(
<div class="row">
<div class="col-25"><label for="host">Syslog Server</label></div>
<div class="col-75"><input type="text" id="host" name="host" maxlength="127" v-model="config.debug.host"></div>
</div>
<div class="row">
<div class="col-25"><label for="port" v-t="'debug.port'"></label></div>
<div class="col-75"><input type="number" id="port" min="0" max="65535" v-model="config.debug.port"></div>
</div>
<div class="row">
<div class="col-25"><label for="log" v-t="'debug.log'"></label></div>
<div class="col-75"><select id="log" v-model="config.debug.log">
<option value="0">Local0</option>
<option value="1">Local1</option>
<option value="2">Local2</option>
<option value="3">Local3</option>
<option value="4">Local4</option>
<option value="5">Local5</option>
<option value="6">Local6</option>
<option value="7">Local7</option>
</select></div>
</div>
<div class="row">
<div class="col-25"></div>
<div class="col-75">
<input id="ietf" type="radio" value="0" v-model="config.debug.proto"><label for="ietf" v-t="'debug.ietf'"></label>
<input id="bsd" type="radio" value="1" v-model="config.debug.proto"><label for="bsd" v-t="'debug.bsd'"></label>
</div>
</div>)";
#endif

    html[min(i++, len)] = R"(<button type="submit" v-t="'save'"></button></form></div>)";
    html[min(i++, len)] = R"(<a v-t="'config.btn'" href="/config"></a>)";
    http_send_content(html, min(i, len));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
static void webHandleHaspConfig()
{ // http://plate01/config/http
    if(!http_is_authenticated(F("config/hasp"))) return;

    const char* html[20];
    int i   = 0;
    int len = (sizeof(html) / sizeof(html[0])) - 1;

    html[min(i++, len)] = "<h1>";
    html[min(i++, len)] = haspDevice.get_hostname();
    html[min(i++, len)] = "</h1><hr>";

    /*
    #if LV_USE_THEME_HASP == 1
        httpMessage += getOption(2, "Hasp Dark");
        httpMessage += getOption(1, "Hasp Light");
    #endif
    #if LV_USE_THEME_EMPTY == 1
        httpMessage += getOption(0, "Empty");
    #endif
    #if LV_USE_THEME_MONO == 1
        httpMessage += getOption(3, "Mono");
    #endif
    #if LV_USE_THEME_MATERIAL == 1
        httpMessage += getOption(5, "Material Dark");
        httpMessage += getOption(4, "Material Light");
    #endif
    #if LV_USE_THEME_TEMPLATE == 1
        httpMessage += getOption(7, "Template");
    #endif

    #if defined(ARDUINO_ARCH_ESP32)
        File root = HASP_FS.open("/");
        File file = root.openNextFile();

        while(file) {
            String filename = file.name();
            file            = root.openNextFile();
        }
    #endif
    */

    html[min(i++, len)] = R"(
<h2 v-t="'hasp.title'"></h2>
<div class="container" v-cloak v-if="config.hasp">
<form @submit.prevent="submitOldConfig('hasp') ">
<div class="row">
<div class="col-25"><label for="theme" v-t="'hasp.theme'"></label></div>
<div class="col-75">
<select id="theme" v-model="config.hasp.theme">
<option value="2">Hasp Dark</option>
<option value="1">Hasp Light</option>
<option value="3">Mono</option>
<option value="5">Material Dark</option>
<option value="4">Material Light</option>
</select>
</div>
</div>
<div class="row">
<div class="col-25"><label for="color1" v-t="'hasp.color1'"></label></div>
<div class="col-75"><input id="color1" type="color" v-model="config.hasp.color1"></div>
</div>
<div class="row">
<div class="col-25"><label for="color2" v-t="'hasp.color2'"></label></div>
<div class="col-75"><input id="color2" type="color" v-model="config.hasp.color2"></div>
</div>
<div class="row gap">
<div class="col-25"><label for="font" v-t="'hasp.font'"></label></div>
<div class="col-75">
<select id="font" v-model="config.hasp.font">
<option value="">None</option>
</select>
</div>
</div>
<div class="row">
<div class="col-25"><label for="pages" v-t="'hasp.pages'"></label></div>
<div class="col-75"><input v-model="config.hasp.pages" id="pages" maxlength="31" placeholder="/pages.jsonl"></div>
</div>
<div class="row">
<div class="col-25"><label for="startpage" v-t="'hasp.startpage'"></label></div>
<div class="col-75"><input v-model="config.hasp.startpage" id="startpage" type="number" min="1" max="12"></div>
</div>
<div class="row">
<div class="col-25"><label for="startdim" v-t="'hasp.startdim'"></label></div>
<div class="col-75"><input v-model="config.hasp.startdim" id="startdim" type="number" min="0" max="255"></div>
</div>)";
    html[min(i++, len)] = R"(<button type="submit" v-t="'save'"></button></form></div>)";
    html[min(i++, len)] = R"(<a v-t="'home.btn'" href="/"></a>)";
    // html[min(i++, len)] = "<div class=\"clear\"><pre>{{ config }}</pre><pre v-t=\" 'about' \">/</pre></div>";
    http_send_content(html, min(i, len));
}
#endif // HASP_USE_CONFIG

#endif // HTTP_LEGACY

////////////////////////////////////////////////////////////////////////////////////////////////////
#if HASP_USE_WIFI > 0
static void http_handle_wifi()
{ // http://plate01/config/wifi
    if(!http_is_authenticated(F("config/wifi"))) return;

    const char* html[20];
    int i   = 0;
    int len = (sizeof(html) / sizeof(html[0])) - 1;

    html[min(i++, len)] = "<h1>";
    html[min(i++, len)] = haspDevice.get_hostname();
    html[min(i++, len)] = "</h1><hr>";
    html[min(i++, len)] = R"(
<h2 v-t="'wifi.title'" @vue:mounted="showConfig('wifi');"></h2>
<div class="container" v-cloak v-if="config.wifi">
<form @submit.prevent="submitOldConfig('wifi') ">
<div class="row">
<div class="col-25"><label for="ssid" v-t="'wifi.ssid'"></label></div>
<div class="col-75"><input type="text" id="ssid" maxlength="31" placeholder="SSID" v-model="config.wifi.ssid"></div>
</div>
<div class="row gap">
<div class="col-25"><label for="pass" v-t="'pass'"></label></div>
<div class="col-75"><input type="password" id="pass" maxlength="63" placeholder="Password" v-model="config.wifi.pass"></div>
</div>
<!--div class="row">
<div class="col-25"><label class="required" for="ip" v-t="'wifi.ip'"></label></div>
<div class="col-75"><input type="text" id="ip" minlength="7" maxlength="15" size="15" pattern="^((\d{1,2}|1\d\d|2[0-4]\d|25[0-5])\.){3}(\d{1,2}|1\d\d|2[0-4]\d|25[0-5])$" placeholder="IP Address" v-model="config.wifi.ip"></div>
</div>
<div class="row">
<div class="col-25"><label class="required" for="gw" v-t="'wifi.gw'"></label></div>
<div class="col-75"><input type="text" id="gw" minlength="7" maxlength="15" size="15" pattern="^((\d{1,2}|1\d\d|2[0-4]\d|25[0-5])\.){3}(\d{1,2}|1\d\d|2[0-4]\d|25[0-5])$" placeholder="Gateway IP" v-model="config.wifi.gw"></div>
</div-->)";
    html[min(i++, len)] = R"(<button type="submit" v-t="'save'"></button></form></div>)";

#if HASP_USE_WIFI > 0 && !defined(STM32F4xx)
    if(WiFi.getMode() == WIFI_STA) {
        html[min(i++, len)] = R"(<a v-t="'config.btn'" href="/config"></a>)";
#if defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_ESP8266)
    } else {
        html[min(i++, len)] = R"(<a v-t="'ota.btn'" href="/firmware"></a>)";
#endif // ARDUINO_ARCH_ESP
    }
#endif // HASP_USE_WIFI

    http_send_content(html, min(i, len));
}

#endif // HASP_USE_WIFI

////////////////////////////////////////////////////////////////////////////////////////////////////
#if HASP_USE_WIREGUARD > 0
static void http_handle_wireguard()
{ // http://plate01/config/wireguard
    if(!http_is_authenticated(F("config/wireguard"))) return;

    const char* html[20];
    int i   = 0;
    int len = (sizeof(html) / sizeof(html[0])) - 1;

    html[min(i++, len)] = "<h1>";
    html[min(i++, len)] = haspDevice.get_hostname();
    html[min(i++, len)] = "</h1><hr>";
    html[min(i++, len)] = R"(
<h2 v-t="'wg.title'" @vue:mounted="showConfig('wg');"></h2>
<div class="container" v-cloak v-if="config.wg">
<form @submit.prevent="submitOldConfig('wg') ">
<div class="row">
<div class="col-25"><label for="vpnip" v-t="'wg.vpnip'"></label></div>
<div class="col-75"><input type="text" id="vpnip" maxlength="15" placeholder="VPN IP" v-model="config.wg.vpnip" pattern="^((\d{1,2}|1\d\d|2[0-4]\d|25[0-5])\.){3}(\d{1,2}|1\d\d|2[0-4]\d|25[0-5])$"></div>
</div>
<div class="row gap">
<div class="col-25"><label for="privkey" v-t="'wg.privkey'"></label></div>
<div class="col-75"><input type="password" id="privkey" maxlength="44" placeholder="Private Key" v-model="config.wg.privkey" pattern="^((?:[A-Za-z0-9+\/]{4})*(?:[A-Za-z0-9+\/]{4}|[A-Za-z0-9+\/]{3}=|[A-Za-z0-9+\/]{2}={2})|(\*\*\*\*\*\*\*\*))$"></div>
</div>
<div class="row">
<div class="col-25"><label for="host" v-t="'wg.host'"></label></div>
<div class="col-75"><input type="text" id="host" maxlength="40" placeholder="Remote IP" v-model="config.wg.host"></div>
</div>
<div class="row">
<div class="col-25"><label for="port" v-t="'wg.port'"></label></div>
<div class="col-75"><input id="port" type="number" min="0" max="65535" placeholder="Remote Port" v-model="config.wg.port"></div>
</div>
<div class="row">
<div class="col-25"><label for="pubkey" v-t="'wg.pubkey'"></label></div>
<div class="col-75"><input type="text" id="pubkey" maxlength="44" placeholder="Remote Public Key" v-model="config.wg.pubkey" pattern="^(?:[A-Za-z0-9+\/]{4})*(?:[A-Za-z0-9+\/]{4}|[A-Za-z0-9+\/]{3}=|[A-Za-z0-9+\/]{2}={2})$"></div>
</div>)";
    html[min(i++, len)] = R"(<button type="submit" v-t="'save'"></button></form></div>)";
    html[min(i++, len)] = R"(<a v-t="'home.btn'" href="/"></a>)";
    http_send_content(html, min(i, len));
}

#endif // HASP_USE_WIREGUARD

static inline int handleFirmwareFile(String path)
{
    String contentType((char*)0);
    contentType = http_get_content_type(path);
    if(path.startsWith("/static/")) {
        path = path.substring(7);
    }

#if defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C3)
    if(path == F("/edit.htm")) {
        return http_send_static_gzip_file(EDIT_HTM_GZ_START, EDIT_HTM_GZ_END, contentType);
        // } else if(path == F("/hasp.htm")) { // 39 kB
        //     return http_send_static_gzip_file(HASP_HTM_GZ_START, HASP_HTM_GZ_END, contentType);
    } else if(path == F("/logo.svg")) { // 300 bytes
        return http_send_static_gzip_file(LOGO_SVG_GZ_START, LOGO_SVG_GZ_END, contentType);
    } else if(path == F("/style.css")) { // 11 kB
        return http_send_static_gzip_file(STYLE_CSS_GZ_START, STYLE_CSS_GZ_END, contentType);
    } else if(path == F("/vars.css")) {
        return http_send_static_file(HTTP_VARS_CSS, HTTP_VARS_CSS + sizeof(HTTP_VARS_CSS) - 1, contentType);
    } else if(path == F("/script.js")) { // 3 kB
        return http_send_static_gzip_file(SCRIPT_JS_GZ_START, SCRIPT_JS_GZ_END, contentType);
    } else if(path == F("/en.json")) { // 2 kB
        return http_send_static_gzip_file(EN_JSON_GZ_START, EN_JSON_GZ_END, contentType);
    } else if(path == F("/main.js")) { // 9 kB
        return http_send_static_gzip_file(MAIN_JS_GZ_START, MAIN_JS_GZ_END, contentType);
    } else if(path == F("/petite-vue.hasp.js")) { // 9 kB
        return http_send_static_gzip_file(PETITE_VUE_HASP_JS_GZ_START, PETITE_VUE_HASP_JS_GZ_END, contentType);
#if ESP_FLASH_SIZE > 4
        // } else if(path == F("/ace.js")) { // 96 kB
        //     return http_send_static_gzip_file(ACE_JS_GZ_START, ACE_JS_GZ_END, contentType);
#endif
    }
#endif // ARDUINO_ARCH_ESP32

    if(path == F("/favicon.ico")) {
        return http_send_cached(204, contentType.c_str(), "", 0, 365 * 24 * 60 * 60); // No content
    }

    return 404;
}

static inline void httpHandleInvalidRequest(int statuscode, String& path)
{
    String httpMessage((char*)0);
    httpMessage.reserve(HTTP_PAGE_SIZE);

    if(statuscode == 500)
        httpMessage += "Internal Server Error";
    else
        httpMessage += D_FILE_NOT_FOUND;

    httpMessage += F("\n\nURI: ");
    httpMessage += path;
    httpMessage += F("\nMethod: ");
    httpMessage += http_method_str(webServer.method());
    httpMessage += F("\nArguments: ");
    httpMessage += webServer.args();
    httpMessage += "\n";
    for(int i = 0; i < webServer.args(); i++) {
        httpMessage += " " + webServer.argName(i) + ": " + webServer.arg(i) + "\n";
    }
    webServer.send(statuscode, PSTR("text/plain"), httpMessage.c_str());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
static void httpHandleFile(String path)
{ // webServer 404
    int statuscode = 404;

#if HASP_USE_SPIFFS > 0 || HASP_USE_LITTLEFS > 0
    if(statuscode == 404) {
        statuscode = handleFilesystemFile(path);
    }
#endif

    if(statuscode == 404) {
        statuscode = handleFirmwareFile(path);
    }

#if defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_ESP8266)
    LOG_TRACE(TAG_HTTP, F("Sending %d %s to client connected from: %s"), statuscode, path.c_str(),
              webServer.client().remoteIP().toString().c_str());
#else
    // LOG_TRACE(TAG_HTTP,F("Sending 404 to client connected from: %s"),
    // String(webServer.client().remoteIP()).c_str());
#endif

    if(statuscode < 300 || statuscode == 304) return; // OK or Not Modified

    httpHandleInvalidRequest(statuscode, path);
}

static void httpHandleFileUri()
{
    httpHandleFile(webServer.uri());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
static void webHandleFirmware()
{
    if(!http_is_authenticated("firmware")) return;

    const char* html[20];
    int i   = 0;
    int len = (sizeof(html) / sizeof(html[0])) - 1;

    html[min(i++, len)] = "<h1>";
    html[min(i++, len)] = haspDevice.get_hostname();
    html[min(i++, len)] = "</h1><hr>";

    if(webServer.method() == HTTP_POST && webServer.hasArg("url")) {
        StaticJsonDocument<512> settings;
        for(int i = 0; i < webServer.args(); i++) settings[webServer.argName(i)] = webServer.arg(i);
        bool updated = otaSetConfig(settings.as<JsonObject>());
        String url   = webServer.arg("url");

        html[min(i++, len)] = R"(<h2 v-t="'ota.title'"></h2><p>Updating firmware from: )";
        html[min(i++, len)] = url.c_str();
        html[min(i++, len)] = R"(</p><p>Please wait...</p>)";
        html[min(i++, len)] = R"(<a v-t="'home.btn'" href="/"></a>)";
        http_send_content(html, min(i, len));

        dispatch_web_update(NULL, url.c_str(), TAG_HTTP);

    } else {

        html[min(i++, len)] = R"(
<h2 v-t="'ota.title'"></h2>
<div class="container">
<form method="POST">
<div class="row">
<div class="col-25"><label class="required" for="url" v-t="'ota.url'"></label></div>
<div class="col-75"><input required="" id="url" name="url" v-model="config.ota.url"></div>
</div>
<div class="row">
<div class="col-25"><label for="redirect">Follow Redirects</label></div>
<div class="col-75"><select id="redirect" name="redirect" v-model="config.ota.redirect">
<option value="0" v-t="'ota.never'"></option>
<option value="1" v-t="'ota.strict'"></option>
<option value="2" v-t="'ota.always'"></option>
</select></div>
</div>
<button type="submit" v-t="'ota.submit'"></button>
</form>
</div>
<div class="container">
<form method="POST" action="/update" enctype="multipart/form-data" id="ota">
<div class="row">
<div class="col-25"><label class="required" for="filename" v-t="'ota.file'"></label></div>
<div class="col-75">
<input required="" type="file" name="filename" accept=".bin">
<input type="hidden" name="cmd" value="0">
</div>
</div>
<button type="submit" value="ota" v-t="'ota.submit'"></button>
</form>
</div>
)";
        html[min(i++, len)] = R"(<a v-t="'home.btn'" href="/"></a>)";
        http_send_content(html, min(i, len));
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
#if HASP_USE_CONFIG > 0

static void httpHandleResetConfig()
{ // http://plate01/config/reset
    if(!http_is_authenticated("reset")) return;

    bool resetConfirmed = webServer.arg("confirm") == "yes";

    { // Send Content
        String httpMessage((char*)0);
        httpMessage.reserve(HTTP_PAGE_SIZE);
        httpMessage += "<h1>";
        httpMessage += haspDevice.get_hostname();
        httpMessage += "</h1><hr>";
        httpMessage += F("<h2>" D_HTTP_FACTORY_RESET "</h2>");

        if(resetConfirmed) {                           // User has confirmed, so reset everything
            bool formatted = dispatch_factory_reset(); // configClearEeprom();
            if(formatted) {
                httpMessage += F("<div class=\"success\">Reset all saved settings. Restarting device...</div>");
            } else {
                httpMessage +=
                    F("<div class=\"error\">Failed to reset the internal storage to factory settings!</div>");
                resetConfirmed = false;
            }
        } else {
            // Form
            httpMessage += F("<form method='POST' action='/config/reset'>");
            httpMessage +=
                F("<div class=\"warning\"><b>Warning</b><p>This process will reset all settings to the "
                  "default values. The internal flash will be erased and the device is restarted. You may need to "
                  "connect to the WiFi AP displayed on the panel to reconfigure the device before accessing it "
                  "again.</p>"
                  "<p>ALL FILES WILL BE LOST!</p></div>");
            httpMessage += F("<p><button class='red' type='submit' name='confirm' value='yes'>" D_HTTP_ERASE_DEVICE
                             "</button></p></form>");

            add_form_button(httpMessage, D_BACK_ICON D_HTTP_CONFIGURATION, F("/config"));
        }

        webSendHtmlHeader(haspDevice.get_hostname(), httpMessage.length(), resetConfirmed ? 10 : 0);
        webServer.sendContent(httpMessage);
    }
    webSendFooter();

    { // Execute Actions
        if(resetConfirmed) {
            // delay(250);
            dispatch_reboot(false); // Do NOT save the current config
        }
    }
}
#endif // HASP_USE_CONFIG

void httpStart()
{
    webServer.begin(80);
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
#endif
#if HASP_USE_ETHERNET > 0
    IPAddress ip;
#if defined(ARDUINO_ARCH_ESP32)
#if HASP_USE_ETHSPI > 0
    ip = ETHSPI.localIP();
#else
    ip = ETH.localIP();
#endif
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
    LOG_WARNING(TAG_HTTP, D_SERVICE_STOPPED);
}

// Do not keep CSS in memory because it is cached in the browser
/*
static void webSendCssVars()
{
    if(!http_is_authenticated("cssvars")) return;

    char filename[32];
    strncpy(filename, webServer.uri().c_str(), sizeof(filename));

    if(HASP_FS.exists(filename)) {
        String contentType((char*)0);
        contentType = http_get_content_type(filename);
        File file   = HASP_FS.open(filename, "r");
        http_send_cache_header(file.size(), 3600);
        webServer.streamFile(file, contentType);
        file.close();
        return;
    }

    String HTTP_CSS = F(":root{"
                        "--txt:" D_HTTP_COLOR_TEXT ";"
                        "--bg:" D_HTTP_COLOR_BACKGROUND ";"
                        "--btnfg:" D_HTTP_COLOR_BUTTON_TEXT ";"
                        "--btnbg:" D_HTTP_COLOR_BUTTON ";"
                        "--btnbghi:" D_HTTP_COLOR_BUTTON_HOVER ";"
                        "--btnred:" D_HTTP_COLOR_BUTTON_RESET ";"
                        "--btnredhi:" D_HTTP_COLOR_BUTTON_RESET_HOVER ";"
                        "--btnbrd: transparent;"
                        "--grpfg:" D_HTTP_COLOR_GROUP_TEXT ";"
                        "--grpbg:" D_HTTP_COLOR_GROUP ";"
                        "--fldbg:" D_HTTP_COLOR_INPUT ";"
                        "--fldfg:" D_HTTP_COLOR_INPUT_TEXT ";"
                        "--fldred:" D_HTTP_COLOR_INPUT_WARNING ";"
                        "--footfg:" D_HTTP_COLOR_FOOTER_TEXT ";"
                        "}");
    http_send_cached(200, PSTR("text/css"), HTTP_CSS.c_str(), HTTP_CSS.length());
}
*/

////////////////////////////////////////////////////////////////////////////////////////////////////
static inline void webStartConfigPortal()
{
#if HASP_USE_CAPTIVE_PORTAL > 0
    // if DNSServer is started with "*" for domain name, it will reply with
    // provided IP to all DNS request
    dnsServer.start(DNS_PORT, "*", apIP);
#endif // HASP_USE_CAPTIVE_PORTAL

    webServer.on("/vars.css", httpHandleFileUri);
    webServer.on(UriBraces("/static/{}"), httpHandleFileUri);
    // webServer.on("/script.js", httpHandleFileUri);
// reply to all requests with same HTML
#if HASP_USE_WIFI > 0
    webServer.onNotFound(http_handle_wifi);
#endif
    LOG_TRACE(TAG_HTTP, "Wifi access point");
}

void httpSetup()
{
    Preferences preferences;
    nvs_user_begin(preferences, FP_HTTP, true);
    String password = preferences.getString(FP_CONFIG_PASS, HTTP_PASSWORD);
    strncpy(http_config.password, password.c_str(), sizeof(http_config.password));
    LOG_DEBUG(TAG_HTTP, F(D_BULLET "Read %s => %s (%d bytes)"), FP_CONFIG_PASS, password.c_str(), password.length());

    // ask server to track these headers
    const char* headerkeys[] = {"Content-Length", "If-None-Match",
                                "Cookie"}; // "Authentication" is automatically checked
    size_t headerkeyssize    = sizeof(headerkeys) / sizeof(char*);
    webServer.collectHeaders(headerkeys, headerkeyssize);

    // Shared pages between STA and AP
    webServer.on("/about", http_handle_about);
    // webServer.on("/vars.css", webSendCssVars);
    // webServer.on("/js", webSendJavascript);
    webServer.on(UriBraces("/api/config/{}/"), webHandleApiConfig);
    webServer.on(UriBraces("/api/{}/"), webHandleApi);

    webServer.on(UriBraces("/config/{}/"), HTTP_GET, []() { httpHandleFile(F("/hasp.htm")); }); // SPA Route
    webServer.on(UriBraces("/{}/"), HTTP_GET, []() { httpHandleFile(F("/hasp.htm")); });        // SPA Route

#if defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_ESP8266)
    webServer.on("/firmware", webHandleFirmware);
    webServer.on(
        F("/update"), HTTP_POST,
        []() {
            webServer.send(200, "text/plain", "");
            LOG_VERBOSE(TAG_HTTP, F("Total size: %s"), webServer.hostHeader().c_str());
        },
        webHandleFirmwareUpload);
#endif

#ifdef HTTP_LEGACY
    webServer.on("/config", http_handle_config);
#endif

#if HASP_USE_WIFI > 0
#if !defined(STM32F4xx)

#if HASP_USE_CONFIG > 0
    if(WiFi.getMode() != WIFI_STA) {
        webStartConfigPortal();
        return;
    }

#endif // HASP_USE_CONFIG
#endif // !STM32F4xx
#endif // HASP_USE_WIFI

    // The following endpoints are only needed in STA mode
    webServer.on("/page/", []() {
        String pageid = webServer.arg("page");
        webServer.send(200, PSTR("text/plain"), "Page: '" + pageid + "'");
        dispatch_page(NULL, webServer.arg("page").c_str(), TAG_HTTP);
        // dispatch_set_page(pageid.toInt(), LV_SCR_LOAD_ANIM_NONE);
    });

#if HASP_USE_SPIFFS > 0 || HASP_USE_LITTLEFS > 0
    webServer.on("/list", HTTP_GET, handleFileList);
    // load editor
    webServer.on("/edit", HTTP_GET, []() { httpHandleFile(F("/edit.htm")); });
    webServer.on("/edit", HTTP_PUT, handleFileCreate);
    webServer.on("/edit", HTTP_DELETE, handleFileDelete);
    // first callback is called after the request has ended with all parsed arguments
    // second callback handles file uploads at that location
    webServer.on(
        F("/edit"), HTTP_POST,
        []() {
            webServer.setContentLength(CONTENT_LENGTH_NOT_SET);
            webServer.send(200, "text/plain", "OK");
            LOG_VERBOSE(TAG_HTTP, F("Headers: %d"), webServer.headers());
        },
        handleFileUpload);
#endif

    webServer.on("/", http_handle_root);
    webServer.on("/screenshot", http_handle_screenshot);
#ifdef HTTP_LEGACY
    webServer.on("/info", http_handle_info);
    webServer.on("/reboot", http_handle_reboot);
#endif

#if HASP_USE_CONFIG > 0
#ifdef HTTP_LEGACY
    webServer.on("/config/hasp", webHandleHaspConfig);
    webServer.on("/config/http", webHandleHttpConfig);
    webServer.on("/config/gui", http_handle_gui);
    webServer.on("/config/time", http_handle_time);
    webServer.on("/config/debug", http_handle_debug);
#if HASP_USE_MQTT > 0
    webServer.on("/config/mqtt", http_handle_mqtt);
#endif
#if HASP_USE_FTP > 0
    webServer.on("/config/ftp", http_handle_ftp);
#endif
#if HASP_USE_WIFI > 0
    webServer.on("/config/wifi", http_handle_wifi);
#endif
#if HASP_USE_WIREGUARD > 0
    webServer.on("/config/wireguard", http_handle_wireguard);
#endif
#if HASP_USE_GPIO > 0
    webServer.on("/config/gpio", webHandleGpioConfig);
    webServer.on("/config/gpio/options", webHandleGpioOutput);
    webServer.on("/config/gpio/input", webHandleGpioInput);
#endif
#endif // HTTP_LEGACY
    webServer.on("/config/reset", httpHandleResetConfig);
#endif // HASP_USE_CONFIG
    webServer.onNotFound(httpHandleFileUri);

    LOG_INFO(TAG_HTTP, D_SERVICE_STARTED);
    // webStart();  Wait for network connection
}

////////////////////////////////////////////////////////////////////////////////////////////////////
IRAM_ATTR void httpLoop(void)
{
#if(HASP_USE_CAPTIVE_PORTAL > 0) && (HASP_USE_WIFI > 0)
    dnsServer.processNextRequest();
#endif
    webServer.handleClient();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
#if HASP_USE_CONFIG > 0
bool httpGetConfig(const JsonObject& settings)
{
    bool changed = false;

    settings[FPSTR(FP_CONFIG_ENABLE)] = http_config.enable;

    if(http_config.port != settings[FPSTR(FP_CONFIG_PORT)].as<uint16_t>()) changed = true;
    settings[FPSTR(FP_CONFIG_PORT)] = http_config.port;

    if(strcmp(http_config.username, settings[FPSTR(FP_CONFIG_USER)].as<String>().c_str()) != 0) changed = true;
    settings[FPSTR(FP_CONFIG_USER)] = http_config.username;

    // if(strcmp(http_config.password, settings[FPSTR(FP_CONFIG_PASS)].as<String>().c_str()) != 0) changed = true;
    // settings[FPSTR(FP_CONFIG_PASS)] = http_config.password;
    if(strcmp(D_PASSWORD_MASK, settings[FPSTR(FP_CONFIG_PASS)].as<String>().c_str()) != 0) changed = true;
    settings[FPSTR(FP_CONFIG_PASS)] = D_PASSWORD_MASK;

    if(changed) configOutput(settings, TAG_HTTP);
    return changed;
}

/** Set HTTP Configuration.
 *
 * Read the settings from json and sets the application variables.
 *
 * @note: data pixel should be formatted to uint32_t RGBA. Imagemagick requirements.
 *
 * @param[in] settings    JsonObject with the config settings.
 **/
bool httpSetConfig(const JsonObject& settings)
{
    Preferences preferences;
    nvs_user_begin(preferences, FP_HTTP, false);

    configOutput(settings, TAG_HTTP);
    bool changed = false;

    changed |= configSet(http_config.port, settings[FPSTR(FP_CONFIG_PORT)], "httpPort");

    if(!settings[FPSTR(FP_CONFIG_USER)].isNull()) {
        changed |= strcmp(http_config.username, settings[FPSTR(FP_CONFIG_USER)]) != 0;
        strncpy(http_config.username, settings[FPSTR(FP_CONFIG_USER)], sizeof(http_config.username));
    }

    if(!settings[FPSTR(FP_CONFIG_PASS)].isNull() &&
       settings[FPSTR(FP_CONFIG_PASS)].as<String>() != String(FPSTR(D_PASSWORD_MASK))) {
        changed |= strcmp(FP_CONFIG_PASS, settings[FPSTR(FP_CONFIG_PASS)]) != 0;
        strncpy(http_config.password, settings[FPSTR(FP_CONFIG_PASS)], sizeof(http_config.password));
        nvsUpdateString(preferences, FP_CONFIG_PASS, settings[FPSTR(FP_CONFIG_PASS)]);
    }

    return changed;
}
#endif // HASP_USE_CONFIG

size_t httpClientWrite(const uint8_t* buf, size_t size)
{
    /***** Sending 16Kb at once freezes on STM32 EthernetClient *****/
    size_t bytes_sent = 0;
    while(bytes_sent < size) {
        if(!webServer.client() || !webServer.client().connected()) return bytes_sent;
        if(size - bytes_sent >= 20480) {
            bytes_sent += webServer.client().write(buf + bytes_sent, 20480); // 2048
            delay(1);                                                        // Fixes the freeze
        } else {
            bytes_sent += webServer.client().write(buf + bytes_sent, size - bytes_sent);
            return bytes_sent;
        }
        // Serial.println(bytes_sent);

        // stm32_eth_scheduler(); // already in write
        // webServer.client().flush();
    }
    return bytes_sent;
}

#endif
