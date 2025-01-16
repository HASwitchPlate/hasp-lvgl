/* MIT License - Copyright (c) 2019-2024 Francis Van Roie
   For full license information read the LICENSE file in the project folder */

#ifndef HASP_FILESYSTEM_H
#define HASP_FILESYSTEM_H

#include <Arduino.h>
#include <FS.h>

bool filesystemSetup(void);
bool sdcardSetup(void);

void filesystemList();
void filesystemInfo();
void filesystemSetupFiles();

enum { ZIP_NO_COMPRESSION = 0, ZIP_DEFLTATE = 8 };
typedef uint16_t zip_compression_method_t;

typedef struct
{
    uint16_t dummy_bytes; // total struct needs to be a multiple of 4 bytes
    uint16_t min_version;
    uint16_t flags;
    zip_compression_method_t compression_method;
    uint16_t time_modified;
    uint16_t date_modified;
    uint32_t crc;
    uint32_t compressed_size;
    uint32_t uncompressed_size;
    uint16_t filename_length; // OK
    uint16_t extra_length;
} zip_file_header_t;

#if defined(ARDUINO_ARCH_ESP32)
#if HASP_USE_SPIFFS > 0
#include "SPIFFS.h"
#define HASP_FS SPIFFS
#elif HASP_USE_LITTLEFS > 0

#ifndef ESP_ARDUINO_VERSION_VAL
#define ESP_ARDUINO_VERSION_VAL(major, minor, patch) ((major << 16) | (minor << 8) | (patch))
#endif

#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(2, 0, 0)
#include <LittleFS.h>
#define HASP_FS LittleFS
#else
#include "LITTLEFS.h"
#include "esp_littlefs.h"
#define HASP_FS LITTLEFS
#endif // ESP_ARDUINO_VERSION

#if HASP_USE_SDCARD > 0
#include "SPI.h"
#include "SD.h"
#include "FS.h"
#define HASP_SD_FS SD

#endif

#endif

#elif defined(ARDUINO_ARCH_ESP8266)
// included by default
#include <LittleFS.h>
#define HASP_FS LittleFS
#endif // ARDUINO_ARCH

#if defined(ARDUINO_ARCH_ESP8266) || defined(ARDUINO_ARCH_ESP32)
#include <FS.h>
#include <Esp.h>
#endif // ARDUINO_ARCH

#if defined(ARDUINO_ARCH_ESP32)
void filesystemUnzip(const char*, const char* filename, uint8_t source);
String filesystem_list(fs::FS& fs, const char* dirname, uint8_t levels);
void listDir_SD(fs::FS &fs, const char *dirname, uint8_t levels); 
#endif

#endif // HASP_FILESYSTEM_H