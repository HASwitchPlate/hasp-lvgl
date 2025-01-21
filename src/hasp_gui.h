/* MIT License - Copyright (c) 2019-2024 Francis Van Roie
   For full license information read the LICENSE file in the project folder */

#ifndef HASP_GUI_H
#define HASP_GUI_H

#include "hasplib.h"

struct bmp_header_t
{
    uint32_t bfSize;
    uint32_t bfReserved;
    uint32_t bfOffBits;

    uint32_t biSize;
    int32_t biWidth;
    int32_t biHeight;
    uint16_t biPlanes;
    uint16_t biBitCount;
    uint32_t biCompression;
    uint32_t biSizeImage;
    int32_t biXPelsPerMeter;
    int32_t biYPelsPerMeter;
    uint32_t biClrUsed;
    uint32_t biClrImportant;

    uint32_t bdMask[3]; // RGB
};

struct gui_conf_t
{
    bool show_pointer;
    int8_t backlight_pin;
    uint8_t rotation;
    uint8_t invert_display;
    uint8_t noise_reduction;
#if defined(USER_SETUP_LOADED)
    uint16_t cal_data[5];
#else
    uint16_t cal_data[8];
#endif
};

/* ===== Default Event Processors ===== */
void guiTftInit(void);
void guiSetup(void);
IRAM_ATTR void guiLoop(void);
void guiEverySecond(void);
void guiStart(void);
void guiStop(void);
void gui_hide_pointer(bool hidden);

/* ===== Special Event Processors ===== */
void guiCalibrate(void);
void guiTakeScreenshot(const char* pFileName); // to file
void guiTakeScreenshot(void);                  // webclient
bool guiScreenshotIsDirty();
uint32_t guiScreenshotEtag();

/* ===== Callbacks ===== */
void gui_flush_cb(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* color_p);
void gui_antiburn_cb(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* color_p);

/* ===== Main LVGL Task ===== */
#if HASP_USE_LVGL_TASK == 1
void gui_task(void* args);
#endif

/* ===== Locks ===== */
#ifdef ESP32
IRAM_ATTR bool gui_acquire(TickType_t timeout);
IRAM_ATTR void gui_release(void);
esp_err_t gui_setup_lvgl_task(void);
#endif

/* ===== Read/Write Configuration ===== */
#if HASP_USE_CONFIG > 0
bool guiGetConfig(const JsonObject& settings);
bool guiSetConfig(const JsonObject& settings);
#endif // HASP_USE_CONFIG

#endif // HASP_GUI_H
