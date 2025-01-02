/* MIT License - Copyright (c) 2019-2024 Francis Van Roie
   For full license information read the LICENSE file in the project folder */

#ifndef HASP_CONF_H
#define HASP_CONF_H

#if HASP_TARGET_ARDUINO
#include "Arduino.h"
#endif

#ifdef USE_CONFIG_OVERRIDE
#include "user_config_override.h"
#endif

// language specific defines
#include "lang/lang.h"

// Lengths
#ifndef MAX_PASSWORD_LENGTH
#define MAX_PASSWORD_LENGTH 64
#endif
#ifndef MAX_USERNAME_LENGTH
#define MAX_USERNAME_LENGTH 32
#endif
#ifndef MAX_HOSTNAME_LENGTH
#define MAX_HOSTNAME_LENGTH 128
#endif

// TFT defines
#ifndef TFT_BACKLIGHT_ON
#define TFT_BACKLIGHT_ON HIGH
#endif

#ifndef TFT_BCKL
#define TFT_BCKL -1
#endif

#define HASP_USE_APP 1

/* Validate that build target was specified */
#if HASP_TARGET_ARDUINO + HASP_TARGET_PC != 1
#error "Build target invalid! Set *one* of: HASP_TARGET_ARDUINO, HASP_TARGET_PC"
#endif

#ifndef HASP_USE_DEBUG
#define HASP_USE_DEBUG 1
#endif

/* Network Services */
#ifndef HASP_USE_ETHERNET
#define HASP_USE_ETHERNET 0
#endif

#ifndef HASP_USE_WIFI
#define HASP_USE_WIFI (ARDUINO_ARCH_ESP32 > 0 || ARDUINO_ARCH_ESP8266 > 0)
#endif

#ifndef HASP_USE_CAPTIVE_PORTAL
#define HASP_USE_CAPTIVE_PORTAL (ARDUINO_ARCH_ESP32 > 0) && (HASP_USE_WIFI > 0)
#endif

#define HASP_HAS_NETWORK                                                                                               \
    (ARDUINO_ARCH_ESP32 > 0 || ARDUINO_ARCH_ESP8266 > 0 || HASP_USE_ETHERNET > 0 || HASP_USE_WIFI > 0)

#ifndef HASP_USE_ARDUINOOTA
#define HASP_USE_ARDUINOOTA 0 //(HASP_HAS_NETWORK)
#endif

#ifndef HASP_USE_HTTP_UPDATE
#define HASP_USE_HTTP_UPDATE (HASP_HAS_NETWORK) // Adds 10kB
#endif

#ifndef HASP_USE_MQTT
#define HASP_USE_MQTT (HASP_HAS_NETWORK)
#endif

#ifndef HASP_USE_MQTT_ASYNC
#define HASP_USE_MQTT_ASYNC (HASP_TARGET_PC)
#endif

#ifndef HASP_USE_WIREGUARD
#define HASP_USE_WIREGUARD 0
#endif

#ifndef HASP_USE_BROADCAST
#define HASP_USE_BROADCAST 1
#endif

#ifndef MQTT_NODENAME
#define MQTT_NODENAME "plate"
#endif

#ifndef HASP_START_CONSOLE
#define HASP_START_CONSOLE 1
#endif

#ifndef HASP_USE_HTTP
#define HASP_USE_HTTP (HASP_HAS_NETWORK)
#endif

#ifndef HASP_USE_HTTP_ASYNC
#define HASP_USE_HTTP_ASYNC 0 //(HASP_HAS_NETWORK)
#endif

#ifndef HASP_START_HTTP
#define HASP_START_HTTP 1
#endif

#ifndef HASP_START_FTP
#define HASP_START_FTP 1
#endif

#ifndef HASP_USE_MDNS
#define HASP_USE_MDNS (HASP_HAS_NETWORK)
#endif

#ifndef HASP_USE_SYSLOG
#define HASP_USE_SYSLOG (HASP_HAS_NETWORK)
#endif

#ifndef HASP_USE_FTP
#define HASP_USE_FTP 0
#endif

#ifndef HASP_USE_TELNET
#define HASP_USE_TELNET 0
#endif

#ifndef HASP_START_TELNET
#define HASP_START_TELNET 1
#endif

#ifndef HASP_USE_CONSOLE
#define HASP_USE_CONSOLE 1
#endif

/* Filesystem */
#define HASP_HAS_FILESYSTEM (ARDUINO_ARCH_ESP32 > 0 || ARDUINO_ARCH_ESP8266 > 0)

#ifndef HASP_USE_SPIFFS
#ifndef HASP_USE_LITTLEFS
#define HASP_USE_SPIFFS (HASP_HAS_FILESYSTEM)
#else
#define HASP_USE_SPIFFS (HASP_USE_LITTLEFS <= 0)
#endif
#endif

#ifndef HASP_USE_LITTLEFS
#define HASP_USE_LITTLEFS (HASP_USE_SPIFFS <= 0)
#endif

#ifndef HASP_USE_EEPROM
#define HASP_USE_EEPROM 1
#endif

#ifndef HASP_USE_SDCARD
#define HASP_USE_SDCARD 1
#endif

#ifndef HASP_USE_GPIO
#define HASP_USE_GPIO 1
#endif

#ifndef HASP_USE_QRCODE
#define HASP_USE_QRCODE 1
#endif

#ifndef HASP_USE_PNGDECODE
#define HASP_USE_PNGDECODE 0
#endif

#ifndef HASP_USE_BMPDECODE
#define HASP_USE_BMPDECODE 0
#endif

#ifndef HASP_USE_GIFDECODE
#define HASP_USE_GIFDECODE 0
#endif

#ifndef HASP_USE_JPGDECODE
#define HASP_USE_JPGDECODE 0
#endif

#ifndef HASP_NUM_GPIO_CONFIG
#define HASP_NUM_GPIO_CONFIG 8
#endif

// #ifndef HASP_USE_CUSTOM
// #define HASP_USE_CUSTOM 0
// #endif

// #ifndef HASP_NUM_OUTPUTS
// #define HASP_NUM_OUTPUTS 3
// #endif

#ifndef HASP_NUM_PAGES
#if defined(ARDUINO_ARCH_ESP8266)
#define HASP_NUM_PAGES 4
#else
#define HASP_NUM_PAGES 12
#endif
#endif

#define HASP_OBJECT_NOTATION "p%ub%u"

#ifndef HASP_ATTRIBUTE_FAST_MEM
#define HASP_ATTRIBUTE_FAST_MEM
#endif

#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif

#if !defined(FPSTR)
#define FPSTR(pstr_pointer) (reinterpret_cast<const __FlashStringHelper*>(pstr_pointer))
#endif

#if !defined(PGM_P)
#define PGM_P const char*
#endif

/* Workarounds for PC build */
#if HASP_TARGET_PC
#ifndef __FlashStringHelper
typedef char __FlashStringHelper;
#endif

#if defined(__cplusplus) && !defined(String)
#include <iostream>
using String = std::string;
#endif

#ifndef F
#define F(x) (x)
#endif

#ifndef PSTR
#define PSTR(x) x
#endif

#ifndef PROGMEM
#define PROGMEM
#endif
#endif

/* Includes */
#ifdef WINDOWS
#include "winsock2.h"
#include "Windows.h"
#elif defined(POSIX)
#else
#include <Arduino.h>
#endif

#if HASP_USE_SPIFFS > 0
// #if defined(ARDUINO_ARCH_ESP32)
// #include "SPIFFS.h"
// #endif
// #include <FS.h> // Include the SPIFFS library
#include "hasp_filesystem.h"
#endif

#if HASP_USE_LITTLEFS > 0
// #if defined(ARDUINO_ARCH_ESP32)
// #include "LITTLEFS.h"
// #elif defined(ARDUINO_ARCH_ESP8266)
// #include <FS.h> // Include the FS library
// #include <LittleFS.h>
// #endif
#include "hasp_filesystem.h"
#endif

#if HASP_USE_SPIFFS > 0 || HASP_USE_LITTLEFS > 0
#if defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_ESP8266)
// #include "lv_zifont.h"
#endif
#endif

#if HASP_USE_EEPROM > 0
#include "hasp_eeprom.h"
#endif

#if HASP_USE_WIFI > 0
#include "sys/net/hasp_wifi.h"

#if defined(STM32F4xx)
#include "WiFiSpi.h"
static WiFiSpiClass WiFi;
#endif
#endif // HASP_USE_WIFI

#if HASP_USE_WIREGUARD > 0
#include "sys/net/hasp_wireguard.h"
#endif

#if HASP_USE_ETHERNET > 0
#if defined(ARDUINO_ARCH_ESP32)
#include "sys/net/hasp_ethernet_esp32.h"
#if HASP_USE_ETHSPI > 0
#include <ETHSPI.h>
#warning Using ESP32 Ethernet SPI W5500
#define HASP_ETHERNET ETHSPI

#else
#define ETH_ADDR 0
#define ETH_POWER_PIN -1
#define ETH_MDC_PIN 23
#define ETH_MDIO_PIN 18
#define NRST 5
#define ETH_TYPE ETH_PHY_LAN8720
#define ETH_CLKMODE ETH_CLOCK_GPIO17_OUT
#include <ETH.h>
#warning Using ESP32 Ethernet LAN8720
#define HASP_ETHERNET ETH
#endif // HASP_USE_ETHSPI

#else
#if USE_BUILTIN_ETHERNET > 0
#include <LwIP.h>
#include <STM32Ethernet.h>
#warning Use built-in STM32 Ethernet
#elif USE_UIP_ETHERNET
#include <UIPEthernet.h>
#include <utility/logging.h>
#warning Use ENC28J60 Ethernet shield
#else
#include "Ethernet.h"
#warning Use W5x00 Ethernet shield
#endif
#include "sys/net/hasp_ethernet_stm32.h"
#endif
#endif

#if HASP_USE_MQTT > 0
#include "mqtt/hasp_mqtt.h"

#if HASP_TARGET_PC
#define HASP_USE_PAHO
#else
#define HASP_USE_ESP_MQTT
//#define HASP_USE_PUBSUBCLIENT
#endif

#endif

#if HASP_USE_GPIO > 0
#include "sys/gpio/hasp_gpio.h"
#endif

#if HASP_USE_HTTP > 0
#include "sys/svc/hasp_http.h"
#endif

#if HASP_USE_HTTP_ASYNC > 0
#include "sys/svc/hasp_http.h"
#endif

#if HASP_USE_CONSOLE > 0
#include "sys/svc/hasp_console.h"
#endif

#if HASP_USE_FTP > 0
#include "sys/svc/hasp_ftp.h"
#endif

#if HASP_USE_TELNET > 0
#include "sys/svc/hasp_telnet.h"
#endif

#if HASP_USE_MDNS > 0
#include "sys/svc/hasp_mdns.h"
#endif

#if HASP_USE_ARDUINOOTA > 0 || HASP_USE_HTTP_UPDATE > 0
#include "sys/svc/hasp_ota.h"
#endif

#if HASP_USE_TASMOTA_CLIENT > 0
#include "sys/svc/hasp_slave.h"
#endif

#if defined(WINDOWS)
#include <Windows.h>
#define delay Sleep
#endif

#if defined(POSIX)
#ifdef USE_MONITOR
#define delay SDL_Delay
#else
#define delay msleep
#endif
#endif

#if HASP_TARGET_PC
#include <string.h>
#include <strings.h>
#include <stdio.h>

#if USE_MONITOR
#include <SDL2/SDL.h>
#endif

#define snprintf_P snprintf
#define memcpy_P memcpy
#define strcasecmp_P strcasecmp
#define strcmp_P strcmp
#define strcpy_P strcpy
#define strstr_P strstr
#define halRestartMcu()
#if USE_MONITOR
#define millis SDL_GetTicks
#elif defined(WINDOWS)
#define millis Win32Millis
#elif defined(POSIX)
#define millis PosixMillis
#endif

#define DEC 10
#define HEX 16
#define BIN 2

#define guiGetDim() 255
#define guiSetDim(x)
#define guiGetBacklight() 1
#define guiSetBacklight(x)
//#define guiCalibrate()
#endif

#endif // HASP_CONF_H
