/* MIT License - Copyright (c) 2019-2024 Francis Van Roie
   For full license information read the LICENSE file in the project folder */

/*
#ifdef CORE_DEBUG_LEVEL
#undef CORE_DEBUG_LEVEL
#endif
#define CORE_DEBUG_LEVEL 3
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

#include "esp_log.h"
*/

#include "hasplib.h"
#include "hasp_oobe.h"
#include "sys/net/hasp_network.h"
#include "sys/net/hasp_time.h"
#include "dev/device.h"

#if HASP_USE_CONFIG > 0
#include "hasp_debug.h"
#include "hasp_macro.h"
#endif

#if HASP_USE_CONFIG > 0
#include "hasp_gui.h"
#endif

static bool isConnected;
static uint8_t mainLoopCounter        = 0;
static unsigned long mainLastLoopTime = 0;

#ifdef HASP_USE_STAT_COUNTER
uint16_t statLoopCounter = 0; // measures the average looptime
#endif

void setup()
{
    //   hal_setup();
#if HASP_USE_SDCARD > 0
    sdcardSetup();
#endif

#if HASP_TARGET_ARDUINO
    esp_log_level_set("*", ESP_LOG_NONE); // set all components to ERROR level
    // esp_log_level_set("wifi", ESP_LOG_NONE);              // enable WARN logs from WiFi stack
    // esp_log_level_set("dhcpc", ESP_LOG_INFO);             // enable INFO logs from DHCP client
    // esp_log_level_set("esp_crt_bundle", ESP_LOG_VERBOSE); // enable WARN logs from WiFi stack
    // esp_log_level_set("esp_tls", ESP_LOG_VERBOSE);        // enable WARN logs from WiFi stack
#elif HASP_TARGET_PC
    // Initialize lvgl environment
    lv_init();
    lv_log_register_print_cb(debugLvglLogEvent);
#if HASP_USE_CONFIG
    // initialize FS before running configSetup()
    // normally, it's initialized in guiSetup(), but Arduino doesn't need FS in configSetup()
    lv_fs_if_init();
#endif
#endif

    haspDevice.init();

    /****************************
     * Storage initializations
     ***************************/
    // nvs_setup();
#if HASP_USE_EEPROM > 0
    eepromSetup(); // Don't start at boot, only at write
#endif

    // #if HASP_USE_SPIFFS > 0 || HASP_USE_LITTLEFS > 0
    //     filesystemSetup();  // FS mount is done in configSetup()
    // #endif

    // #if HASP_USE_SDCARD > 0
    //     sdcardSetup();
    // #endif

    /****************************
     * Read & Apply User Configuration
     ***************************/
#if HASP_USE_CONFIG > 0
    configSetup(); // also runs  debugSetup(), debugStart() and consoleSetup()
#endif

#if HASP_USE_WIFI > 0 || HASP_USE_ETHERNET > 0
    networkSetup();
    timeSetup();
#endif

    dispatchSetup(); // before hasp and oobe, asap after logging starts
    guiSetup();

    bool oobe = false;
#if HASP_USE_CONFIG > 0
    oobe = oobeSetup();
#endif
    if(!oobe) {
        haspSetup();
    }

    /****************************
     * Apply User Configuration
     ***************************/

    // #if HASP_USE_MQTT > 0
    //     mqttSetup(); // Load Hostname before starting WiFi
    // #endif

#if HASP_USE_GPIO > 0
    gpioSetup();
#endif

#if HASP_USE_MDNS > 0
    mdnsSetup();
#endif

#if HASP_USE_ARDUINOOTA > 0 || HASP_USE_HTTP_UPDATE > 0
    otaSetup();
#endif

#if HASP_USE_HTTP > 0 || HASP_USE_HTTP_ASYNC > 0
    httpSetup();
#endif

    // #if HASP_USE_CONSOLE > 0
    //     consoleSetup(); // the consoleSetup is called in debugSetup
    // #endif

#if HASP_USE_TELNET > 0
    telnetSetup();
#endif

#if HASP_USE_FTP > 0
    ftpSetup();
#endif

#if HASP_USE_TASMOTA_CLIENT > 0
    slaveSetup();
#endif

#if defined(HASP_USE_CUSTOM) && HASP_USE_CUSTOM > 0
    custom_setup();
#endif

    // guiStart();

    delay(20);

    if(!oobe) {
        dispatch_run_script(NULL, "L:/boot.cmd", TAG_HASP);
#if HASP_USE_WIFI > 0 || HASP_USE_ETHERNET > 0
        network_run_scripts();
#endif
    }

#if HASP_USE_MQTT > 0
    mqttSetup();
#endif

#if HASP_USE_LVGL_TASK && defined(ESP32)
    gui_setup_lvgl_task();
#endif // HASP_USE_LVGL_TASK

    mainLastLoopTime = 0; // reset loop counter
}

IRAM_ATTR void loop()
{
#if defined(ESP32) && defined(HASP_USE_ESP_MQTT)
    if(!gui_acquire(portMAX_DELAY)) {
        // LOG_ERROR(TAG_MAIN, F("TAKE Mutex"));
        delay(5); // ms
        return;
    }
#endif

#if HASP_USE_LVGL_TASK == 0
    guiLoop();
#endif

#if HASP_USE_WIFI > 0 || HASP_USE_ETHERNET > 0
    networkLoop();
#endif

#if HASP_USE_GPIO > 0
    gpioLoop();
#endif // GPIO

#if HASP_USE_MQTT > 0
    mqttLoop();
#endif

    // haspDevice.loop();

#if HASP_USE_CONSOLE > 0
    // debugLoop();
    consoleLoop();
#endif

#if defined(HASP_USE_CUSTOM) && HASP_USE_CUSTOM > 0
    custom_loop();
#endif

#ifdef HASP_USE_STAT_COUNTER
    statLoopCounter++; // measures the average looptime
#endif

    /* Timer Loop */
    if(millis() - mainLastLoopTime >= 1000) {
        mainLastLoopTime = millis();

        /* Runs Every Second */
        haspEverySecond(); // sleep timer & statusupdate

#if HASP_USE_MQTT > 0
        mqttEverySecond();
#endif

#if HASP_USE_FTP > 0
        ftpEverySecond();
#endif

#if HASP_USE_TELNET > 0
        telnetEverySecond();
#endif

#if defined(HASP_USE_CUSTOM) && HASP_USE_CUSTOM > 0
        custom_every_second();
#endif
        // debugEverySecond();

        switch(++mainLoopCounter) {
            case 1:
                haspDevice.loop_5s();
                break;

            case 2:
#if HASP_USE_HTTP_ASYNC > 0
                httpEvery5Seconds();
#endif
                break;

            case 3:
#if HASP_USE_GPIO > 0
                //   gpioEvery5Seconds();
#endif

#if defined(HASP_USE_CUSTOM) && HASP_USE_CUSTOM > 0
                custom_every_5seconds();
#endif
                break;

            case 4:
#if HASP_USE_WIFI > 0 || HASP_USE_ETHERNET > 0
                isConnected = networkEvery5Seconds(); // Check connection
#endif
#if HASP_USE_MQTT > 0
                mqttEvery5Seconds(isConnected);
#endif
                break;

            case 5:
                mainLoopCounter = 0;
#ifdef HASP_USE_STAT_COUNTER
                if(statLoopCounter)
                    LOG_DEBUG(TAG_MAIN, F("%d millis per loop, %d counted"), 5000 / statLoopCounter, statLoopCounter);
                statLoopCounter = 0;
#endif
                break;
        }
    }

#if defined(ESP32) && defined(HASP_USE_ESP_MQTT)
    gui_release();
#endif

// allow the cpu to switch to other tasks
#if HASP_USE_LVGL_TASK == 0
#ifdef ARDUINO_ARCH_ESP8266
    delay(2); // ms
#else
    delay(2); // ms
#endif
#else // HASP_USE_LVGL_TASK != 0
    delay(2); // ms
#endif
}
