/* MIT License - Copyright (c) 2019-2024 Francis Van Roie
   For full license information read the LICENSE file in the project folder */

/***************************************************
// This file contains the default settings that are
// burned into the compiled firmware.
//
// These default settings can be changed at runtime
//
// To use: Save a copy as user_config_override.h
***************************************************/
#ifndef HASP_USER_CONFIG_OVERRIDE_H
#define HASP_USER_CONFIG_OVERRIDE_H

#define SERIAL_SPEED 115200

/***************************************************
          WiFi Settings
 **************************************************/
#define WIFI_SSID ""
#define WIFI_PASSWORD ""

/***************************************************
          Http Server Settings
 **************************************************/
#define HTTP_USERNAME ""
#define HTTP_PASSWORD ""

/***************************************************
          MQTT Client Settings
 **************************************************/
#define MQTT_HOSTNAME ""
#define MQTT_PORT 1883
#define MQTT_USERNAME ""
#define MQTT_PASSWORD ""
#define MQTT_PREFIX "hasp"
#define MQTT_NODENAME "plate01"
#define MQTT_GROUPNAME "plates"

#define MQTT_TELEPERIOD 60000
#define MQTT_STATEPERIOD 300000

/***************************************************
 *        OTA Settings
 **************************************************/
//#define HASP_USE_ARDUINOOTA 1                       // Enable the Arduino OTA service
#define ARDUINOOTA_PORT 3232
#define ARDUINOOTA_PASSWORD ""
#define OTA_URL ""

/***************************************************
 *        Syslog Settings
 **************************************************/
#define HASP_USE_SYSLOG 0
#define SYSLOG_SERVER ""
#define SYSLOG_PORT 514
#define APP_NAME "HASP"

/***************************************************
 *        Timezone Settings
 **************************************************/
#define NTPSERVER1 "pool.ntp.org"
#define NTPSERVER2 "time.nist.gov"
#define NTPSERVER3 "time.google.com"
#define MYTZ                                                                                                           \
    "CET-1CEST,M3.5.0,M10.5.0/3" // A full list with possible timezones can be found here
                                 // https://gist.github.com/alwynallan/24d96091655391107939

/***************************************************
 *        Interface Language Settings
 **************************************************/
#define HASP_LANGUAGE en_US // English
//#define HASP_LANGUAGE ru_RU                        // Russian
// #define HASP_LANGUAGE es_ES                       // Spanish
// #define HASP_LANGUAGE fr_FR                       // French
// #define HASP_LANGUAGE hu_HU                       // Hungarian
// #define HASP_LANGUAGE nl_NL                       // Dutch
// #define HASP_LANGUAGE pt_BR                       // Brazilian Portuguese
// #define HASP_LANGUAGE pt_PT                       // Portuguese
// #define HASP_LANGUAGE ro_RO                       // Romanian
// #define HASP_LANGUAGE de_DE                       // German

/***************************************************
 *        Web interface coloring
 **************************************************/
// Light theme (default)
#define D_HTTP_COLOR_TEXT "#000"               // Global text color - Black
#define D_HTTP_COLOR_BACKGROUND "#fff"         // Global background color - White
#define D_HTTP_COLOR_INPUT_TEXT "#000"         // Input text color - Black
#define D_HTTP_COLOR_INPUT "#fff"              // Input background color - White
#define D_HTTP_COLOR_INPUT_WARNING "#f00"      // Input warning border color - Red
#define D_HTTP_COLOR_BUTTON_TEXT "#fff"        // Button text color - White
#define D_HTTP_COLOR_BUTTON "#1fa3ec"          // Button color - Vivid blue
#define D_HTTP_COLOR_BUTTON_HOVER "#0083cc"    // Button color - Olympic blue
#define D_HTTP_COLOR_BUTTON_RESET "#f00"       // Restart/Reset button color - red
#define D_HTTP_COLOR_BUTTON_RESET_HOVER "#b00" // Restart/Reset button color - Dark red
#define D_HTTP_COLOR_GROUP "#f3f3f3"           // Group container background color
#define D_HTTP_COLOR_GROUP_TEXT "#000"         // Group container text color - black
#define D_HTTP_COLOR_FOOTER_TEXT "#0083cc"     // Footer text color - Olympic blue

/*
// Dark theme
#define D_HTTP_COLOR_TEXT               "#eaeaea"    // Global text color - Very light gray
#define D_HTTP_COLOR_BACKGROUND         "#252525"    // Global background color - Very dark gray (mostly black)
#define D_HTTP_COLOR_INPUT_TEXT         "#000"       // Input text color - Black
#define D_HTTP_COLOR_INPUT              "#ddd"       // Input background color - Very light gray
#define D_HTTP_COLOR_INPUT_WARNING      "#ff5661"    // Warning text color - Brick Red
#define D_HTTP_COLOR_BUTTON_TEXT        "#faffff"    // Button text color - Very pale (mostly white) cyan
#define D_HTTP_COLOR_BUTTON             "#1fa3ec"    // Button color - Vivid blue
#define D_HTTP_COLOR_BUTTON_HOVER       "#0083cc"    // Button color - Olympic Blue
#define D_HTTP_COLOR_BUTTON_RESET       "#d43535"    // Restart/Reset/Delete button color - Strong red
#define D_HTTP_COLOR_BUTTON_RESET_HOVER "#b00"       // Restart/Reset button color - Dark red
#define D_HTTP_COLOR_GROUP              "#444"       // Group container background color - Dark gray
#define D_HTTP_COLOR_GROUP_TEXT         "#fff"       // Group container text color - white
#define D_HTTP_COLOR_FOOTER_TEXT        "#1fa3ec"    // Footer text color - Vivid blue
*/

/***************************************************
 *        Font Settings
 **************************************************/
// #define HASP_FONT_1 robotocondensed_regular_16_latin1 // Use available fonts from src/fonts directory
// #define HASP_FONT_2 robotocondensed_regular_22_latin1
// #define HASP_FONT_3 robotocondensed_regular_40_latin1
// #define HASP_FONT_4 robotocondensed_regular_48_latin1
// #define HASP_FONT_5 robotocondensed_regular_12_latin1

// #define ROBOTOCONDENSED_REGULAR_16_LATIN1 1           // Enable the selected fonts
// #define ROBOTOCONDENSED_REGULAR_22_LATIN1 1
// #define ROBOTOCONDENSED_REGULAR_40_LATIN1 1
// #define ROBOTOCONDENSED_REGULAR_48_LATIN1 1
// #define ROBOTOCONDENSED_REGULAR_12_LATIN1 1

// #define HASP_FONT_SIZE_1 16                           // Define used font sizes
// #define HASP_FONT_SIZE_2 22
// #define HASP_FONT_SIZE_3 40
// #define HASP_FONT_SIZE_4 48
// #define HASP_FONT_SIZE_5 12

// #define LV_FONT_DEFAULT &HASP_FONT_1

/***************************************************
 *        GPIO Settings
 **************************************************/
//#define HASP_GPIO_TEMPLATE "[197658,263456,329249,655628,655886,656155,0,0]"  // Lanbon L8 3-gang GPIO config
//#define HASP_GPIO_TEMPLATE "[3214348,197658,263456,329249,94699520,0,0,0]" // Lanbon L8 Dimmer GPIO config

/***************************************************
 *        Other Settings
 **************************************************/
//#define HASP_USE_MDNS 0                             // Disable MDNS
//#define HASP_USE_CUSTOM 1                           // Enable compilation of custom code from /src/custom
//#define HASP_START_CONSOLE 0                        // Disable starting of serial console at boot
//#define HASP_START_TELNET 0                         // Disable starting of telnet service at boot
//#define HASP_START_HTTP 0                           // Disable starting of web interface at boot
//#define HASP_START_FTP 0                            // Disable starting of ftp server at boot
//#define LV_MEM_SIZE (64 * 1024U)                    // 64KiB of lvgl memory (default 48)
//#define LV_VDB_SIZE (32 * 1024U)                    // 32KiB of lvgl draw buffer (default 32)
//#define HASP_DEBUG_OBJ_TREE                         // Output all objects to the log on page changes
//#define HASP_LOG_LEVEL LOG_LEVEL_VERBOSE            // LOG_LEVEL_* can be DEBUG, VERBOSE, TRACE, INFO, WARNING, ERROR, CRITICAL, ALERT, FATAL, SILENT
//#define HASP_LOG_TASKS                              // Also log the Taskname and watermark of ESP32 tasks

#endif // HASP_USER_CONFIG_OVERRIDE_H
