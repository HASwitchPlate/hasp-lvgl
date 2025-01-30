/* MIT License - Copyright (c) 2019-2024 Francis Van Roie
   For full license information read the LICENSE file in the project folder */

#include "hasplib.h"

#include "lv_drv_conf.h"
#include "lv_fs_if.h"

// Device Drivers
#include "dev/device.h"
#include "drv/tft/tft_driver.h"
#include "drv/touch/touch_driver.h"

#include "hasp_debug.h"
#include "hasp_gui.h"
#include "hasp_oobe.h"

// #include "tpcal.h"

#define BACKLIGHT_CHANNEL 0 // pwm channel 0-15

#if HASP_USE_SPIFFS > 0 || HASP_USE_LITTLEFS > 0
File pFileOut;
#endif

#if ESP32
static SemaphoreHandle_t xGuiSemaphore = NULL;
static TaskHandle_t g_lvgl_task_handle;
#endif

#define LVGL_TICK_PERIOD 20

#ifndef TFT_BCKL
#define TFT_BCKL -1 // No Backlight Control
#endif
#ifndef TFT_ROTATION
#define TFT_ROTATION 0
#endif
#ifndef INVERT_COLORS
#define INVERT_COLORS 0
#endif

// HASP_ATTRIBUTE_FAST_MEM static void  lv_tick_handler(void);

gui_conf_t gui_settings = {.show_pointer   = false,
                           .backlight_pin  = TFT_BCKL,
                           .rotation       = TFT_ROTATION,
                           .invert_display = INVERT_COLORS,
                           .cal_data       = {0, 65535, 0, 65535, 0}};
lv_obj_t* cursor;

uint16_t tft_width  = TFT_WIDTH;
uint16_t tft_height = TFT_HEIGHT;

bool screenshotIsDirty  = true;
uint32_t screenshotEtag = 0;
void (*drv_display_flush_cb)(struct _disp_drv_t* disp_drv, const lv_area_t* area, lv_color_t* color_p);

static lv_disp_buf_t disp_buf;

static inline void gui_init_lvgl()
{
    LOG_VERBOSE(TAG_LVGL, F("Version    : %u.%u.%u %s"), LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR, LVGL_VERSION_PATCH,
                PSTR(LVGL_VERSION_INFO));
    lv_init();

#if LV_USE_LOG != 0
    // Register logger to capture lvgl_init output
    lv_log_register_print_cb(debugLvglLogEvent);
#endif

    /* Create the Virtual Device Buffers */
    const size_t guiVDBsize = LV_VDB_SIZE / sizeof(lv_color_t);
#ifdef ESP32
    static lv_color_t* guiVdbBuffer1 =
        (lv_color_t*)heap_caps_malloc(sizeof(lv_color_t) * guiVDBsize, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
#else
    static lv_color_t* guiVdbBuffer1 = (lv_color_t*)malloc(sizeof(lv_color_t) * guiVDBsize);
#endif

    /* Static VDB allocation */
    // static lv_color_t guiVdbBuffer1[LV_VDB_SIZE * 512u];
    // const size_t guiVDBsize = sizeof(guiVdbBuffer1) / sizeof(lv_color_t);

    /* Initialize VDB */
    if(guiVdbBuffer1 && guiVDBsize > 0) {
        lv_disp_buf_init(&disp_buf, guiVdbBuffer1, NULL, guiVDBsize);
    } else {
        LOG_FATAL(TAG_GUI, F(D_ERROR_OUT_OF_MEMORY));
    }

#ifdef LV_MEM_SIZE
    LOG_VERBOSE(TAG_LVGL, F("MEM size   : %d"), LV_MEM_SIZE);
#endif
    LOG_VERBOSE(TAG_LVGL, F("VFB size   : %d"), (size_t)sizeof(lv_color_t) * guiVDBsize);
}

void gui_hide_pointer(bool hidden)
{
    if(cursor) lv_obj_set_hidden(cursor, hidden || !gui_settings.show_pointer);
}

IRAM_ATTR void gui_flush_cb(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* color_p)
{
    haspTft.flush_pixels(disp, area, color_p);
    screenshotIsDirty = true;
}

void gui_antiburn_cb(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* color_p)
{
    /*  uint32_t w   = (area->x2 - area->x1 + 1);
        uint32_t h   = (area->y2 - area->y1 + 1);
        uint32_t len = w * h;

        lv_color_t dots[w];
        lv_area_t line = *area;

        for(lv_coord_t y = 0; y < h; y++) {
            for(lv_coord_t x = 0; x < w; x++) {
                dots[x].full = HASP_RANDOM(UINT16_MAX);
            }
            line.y1 = area->y1 + y;
            line.y2 = line.y1;
            haspTft.flush_pixels(disp, &line, dots);
        } */
    /* Tell lvgl that flushing is done */
    lv_disp_flush_ready(disp);
}

IRAM_ATTR void gui_monitor_cb(lv_disp_drv_t* disp_drv, uint32_t time, uint32_t px)
{
    // if(screenshotIsDirty) return;
    LOG_DEBUG(TAG_GUI, F("The Screen is dirty"));
    screenshotIsDirty = true;
}

IRAM_ATTR bool gui_touch_read(lv_indev_drv_t* indev_driver, lv_indev_data_t* data)
{
    return haspTouch.read(indev_driver, data);
}

void guiCalibrate(void)
{
#if TOUCH_DRIVER == 0x2046 //&& defined(USER_SETUP_LOADED)
    haspTouch.calibrate(gui_settings.cal_data);

    // size_t len = sizeof(gui_settings.cal_data) / sizeof(gui_settings.cal_data[0]);
    // for(int i = 0; i < len; i++) {
    //     Serial.print(gui_settings.cal_data[i]);
    //     if(i < len - 1) Serial.print(", ");
    // }

    lv_obj_invalidate(lv_disp_get_layer_sys(NULL));
#endif
}

// fast init
void gui_start_tft(void)
{
    /* Setup Backlight Control Pin */
    haspDevice.set_backlight_pin(gui_settings.backlight_pin);

    haspTft.init(tft_width, tft_height);
    haspTft.set_rotation(gui_settings.rotation);
    haspTft.set_invert(gui_settings.invert_display);
}

static inline void gui_init_tft(void)
{
    // Initialize TFT
    LOG_TRACE(TAG_TFT, F(D_SERVICE_STARTING));
    gui_start_tft();
    haspTft.show_info();

#ifdef USE_DMA_TO_TFT
    LOG_VERBOSE(TAG_TFT, F("DMA        : " D_SETTING_ENABLED));
#else
    LOG_VERBOSE(TAG_TFT, F("DMA        : " D_SETTING_DISABLED));
#endif
    LOG_INFO(TAG_TFT, F(D_SERVICE_STARTED));
}

// initialize the image decoders
static inline void gui_init_images()
{
#if HASP_USE_PNGDECODE > 0
    lv_png_init(); // Initialize PNG decoder
#endif

#if HASP_USE_BMPDECODE > 0
    lv_bmp_init(); // Initialize BMP decoder
#endif

#if HASP_USE_GIFDECODE > 0
    lv_gif_init(); // Initialize GIF decoder
#endif

#if HASP_USE_JPGDECODE > 0
    lv_split_jpeg_init(); // Initialize JPG decoder
#endif

#if defined(ARDUINO_ARCH_ESP32)
    if(hasp_use_psram()) lv_img_cache_set_size(LV_IMG_CACHE_DEF_SIZE_PSRAM);
#endif
}

static inline void gui_init_filesystems()
{
#if LV_USE_FS_IF != 0
    LOG_VERBOSE(TAG_LVGL, F("Filesystem : " D_SETTING_ENABLED));
    lv_fs_if_init(); // auxiliary file system drivers
#else
    LOG_VERBOSE(TAG_LVGL, F("Filesystem : " D_SETTING_DISABLED));
#endif
}

void guiSetup()
{
    // Initialize hardware drivers
    gui_init_tft();
    haspDevice.show_info(); // debug info + preload app flash size

    // Initialize LVGL
    LOG_TRACE(TAG_LVGL, F(D_SERVICE_STARTING));
    gui_init_lvgl();
    gui_init_images();
    gui_init_filesystems();
    font_setup();

    /* Initialize the LVGL display driver with correct orientation */
#if(TOUCH_DRIVER == 0x2046) ||                                                                                         \
    (defined(LGFX_USE_V1) && defined(HASP_USE_LGFX_TOUCH)) // Use native display driver to rotate display and touch
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.buffer   = &disp_buf;
    disp_drv.flush_cb = gui_flush_cb;

    if(gui_settings.rotation % 2) {
        disp_drv.hor_res = tft_height;
        disp_drv.ver_res = tft_width;
    } else {
        disp_drv.hor_res = tft_width;
        disp_drv.ver_res = tft_height;
    }

    lv_disp_t* display = lv_disp_drv_register(&disp_drv);
    lv_disp_set_rotation(display, LV_DISP_ROT_NONE);

    /*
    #elif defined(LANBONL8) // Screen is 0 deg. rotated
        static lv_disp_drv_t disp_drv;
        lv_disp_drv_init(&disp_drv);
        disp_drv.buffer   = &disp_buf;
        disp_drv.flush_cb = gui_flush_cb;

        disp_drv.hor_res = tft_width;
        disp_drv.ver_res = tft_height;

        lv_disp_rot_t rotation[] = {LV_DISP_ROT_NONE, LV_DISP_ROT_270, LV_DISP_ROT_180, LV_DISP_ROT_90};
        lv_disp_t* display       = lv_disp_drv_register(&disp_drv);
        lv_disp_set_rotation(display, rotation[(4 + gui_settings.rotation - TFT_ROTATION) % 4]);
    */

#elif defined(M5STACK) // Screen is 90 deg. rotated
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.buffer   = &disp_buf;
    disp_drv.flush_cb = gui_flush_cb;

    disp_drv.hor_res = tft_height;
    disp_drv.ver_res = tft_width;

    lv_disp_rot_t rotation[] = {LV_DISP_ROT_NONE, LV_DISP_ROT_270, LV_DISP_ROT_180, LV_DISP_ROT_90};
    lv_disp_t* display       = lv_disp_drv_register(&disp_drv);
    lv_disp_set_rotation(display, rotation[(4 + gui_settings.rotation - TFT_ROTATION) % 4]);

#else // Use lvgl transformations
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.buffer    = &disp_buf;
    disp_drv.flush_cb  = gui_flush_cb;
    disp_drv.hor_res   = tft_width;
    disp_drv.ver_res   = tft_height;

#if defined(HASP_LV_USE_SW_ROTATE)
    disp_drv.sw_rotate = 1; // enable special bit order in framebuffer bitmaps
#endif

    lv_disp_rot_t rotation[] = {LV_DISP_ROT_NONE, LV_DISP_ROT_270, LV_DISP_ROT_180, LV_DISP_ROT_90};
    lv_disp_t* display       = lv_disp_drv_register(&disp_drv);
    lv_disp_set_rotation(display, rotation[(4 + gui_settings.rotation - TFT_ROTATION) % 4]);
#endif
    disp_drv.monitor_cb = gui_monitor_cb;

    // register a touchscreen/mouse driver - only on real hardware and SDL2
    // Win32 and POSIX handles input drivers in tft_driver
#if TOUCH_DRIVER != -1 || USE_MONITOR
    /* Initialize the touch pad */
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
#if USE_MONITOR && HASP_TARGET_PC
    indev_drv.read_cb = mouse_read;
#else
    indev_drv.read_cb = gui_touch_read;
#endif
    lv_indev_t* mouse_indev  = lv_indev_drv_register(&indev_drv);
    mouse_indev->driver.type = LV_INDEV_TYPE_POINTER;
#else
    // find the first registered input device to add a cursor to
    lv_indev_t* mouse_indev = NULL;
    while((mouse_indev = lv_indev_get_next(mouse_indev))) {
        if(mouse_indev->driver.type == LV_INDEV_TYPE_POINTER) break;
    }
#endif

    /*Set a cursor for the mouse*/
    LOG_TRACE(TAG_GUI, F("Initialize Cursor"));
    lv_obj_t* mouse_layer = lv_disp_get_layer_sys(NULL); // default display

#if defined(ARDUINO_ARCH_ESP32)
    Preferences preferences;
    nvs_user_begin(preferences, "gui", true);
    // indev_drv.drag_limit           = preferences.getUChar(key, LV_INDEV_DEF_DRAG_LIMIT);
    // indev_drv.drag_throw           = preferences.getUChar(key, LV_INDEV_DEF_DRAG_THROW);
    indev_drv.long_press_time     = preferences.getUShort(FP_GUI_LONG_TIME, LV_INDEV_DEF_LONG_PRESS_TIME);
    indev_drv.long_press_rep_time = preferences.getUShort(FP_GUI_REPEAT_TIME, LV_INDEV_DEF_LONG_PRESS_REP_TIME);
    // indev_drv.gesture_limit        = preferences.getUChar(key, LV_INDEV_DEF_GESTURE_LIMIT);
    // indev_drv.gesture_min_velocity = preferences.getUChar(key, LV_INDEV_DEF_GESTURE_MIN_VELOCITY);
    preferences.end();

    LV_IMG_DECLARE(mouse_cursor_icon);          /*Declare the image file.*/
    cursor = lv_img_create(mouse_layer, NULL);  /*Create an image object for the cursor */
    lv_img_set_src(cursor, &mouse_cursor_icon); /*Set the image source*/
#else
    cursor = lv_obj_create(mouse_layer, NULL); // show cursor object on every page
    lv_obj_set_size(cursor, 9, 9);
    lv_obj_set_style_local_radius(cursor, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_RADIUS_CIRCLE);
    lv_obj_set_style_local_bg_color(cursor, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_RED);
    lv_obj_set_style_local_bg_opa(cursor, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_COVER);
#endif
    gui_hide_pointer(false);
    if(mouse_indev != NULL) {
        lv_indev_set_cursor(mouse_indev, cursor); /*Connect the image object to the driver*/
    }

#if HASP_TARGET_ARDUINO
    // drv_touch_init(gui_settings.rotation); // Touch driver
    haspTouch.init(tft_width, tft_height);
    haspTouch.set_calibration(gui_settings.cal_data);
    haspTouch.set_rotation(gui_settings.rotation);
#endif

    /* Initialize Global progress bar*/
#if USE_OBJ_ALIAS > 0
    lv_obj_user_data_t udata = (lv_obj_user_data_t){10, 0, 10, NULL, 0, NULL};
#else
    lv_obj_user_data_t udata = (lv_obj_user_data_t){10, 0, 10};
#endif  // #if USE_OBJ_ALIAS > 0

    lv_obj_t* bar            = lv_bar_create(lv_layer_sys(), NULL);
    lv_obj_set_user_data(bar, udata);
    lv_obj_set_hidden(bar, true);
    lv_bar_set_range(bar, 0, 100);
    lv_bar_set_value(bar, 10, LV_ANIM_OFF);
    lv_obj_set_size(bar, 200, 15);
    lv_obj_align(bar, lv_layer_sys(), LV_ALIGN_CENTER, 0, -10);
    lv_obj_set_style_local_value_color(bar, LV_BAR_PART_BG, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    lv_obj_set_style_local_value_align(bar, LV_BAR_PART_BG, LV_STATE_DEFAULT, LV_ALIGN_CENTER);
    lv_obj_set_style_local_value_ofs_y(bar, LV_BAR_PART_BG, LV_STATE_DEFAULT, 20);
    lv_obj_set_style_local_value_font(bar, LV_BAR_PART_BG, LV_STATE_DEFAULT, LV_FONT_DEFAULT);
    lv_obj_set_style_local_bg_color(lv_layer_sys(), LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_BLACK);
    lv_obj_set_style_local_bg_opa(lv_layer_sys(), LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_0);

#if defined(ESP32) && defined(HASP_USE_ESP_MQTT)
    xGuiSemaphore = xSemaphoreCreateMutex();
    if(!xGuiSemaphore) {
        LOG_FATAL(TAG_GUI, "Create mutex for LVGL failed");
    }
#endif // ESP32 && HASP_USE_ESP_MQTT

    LOG_INFO(TAG_LVGL, F(D_SERVICE_STARTED));
}

IRAM_ATTR void guiLoop(void)
{
    lv_task_handler(); // process animations

#if defined(STM32F4xx)
    //  tick.update();
#endif

#if HASP_TARGET_ARDUINO
    // haspTouch.loop();
#endif
}

void guiEverySecond(void)
{
    // nothing
}

#if HASP_USE_LVGL_TASK == 1
void gui_task(void* args)
{
    LOG_TRACE(TAG_GUI, "Start to run LVGL");
    while(haspDevice.pc_is_running) {
        // no idea what MQTT has to do with LVGL - the #if is copied from the code below
#if defined(ESP32) && defined(HASP_USE_ESP_MQTT)
        /* Try to take the semaphore, call lvgl related function on success */
        if(pdTRUE == xSemaphoreTake(xGuiSemaphore, portMAX_DELAY)) {
            lv_task_handler();
            xSemaphoreGive(xGuiSemaphore);
            vTaskDelay(pdMS_TO_TICKS(5));
        }
#else
        // optimize lv_task_handler() by actually using the returned delay value
        auto time_start     = millis();
        uint32_t sleep_time = lv_task_handler();
        delay(sleep_time);
        auto time_end = millis();
        lv_tick_inc(time_end - time_start);
#endif
    }
}
#endif // HASP_USE_LVGL_TASK

#if defined(ESP32) && defined(HASP_USE_ESP_MQTT)

#if HASP_USE_LVGL_TASK == 1
esp_err_t gui_setup_lvgl_task()
{
#if CONFIG_FREERTOS_UNICORE == 0
    int err = xTaskCreatePinnedToCore(gui_task, "lvglTask", 1024 * 8, NULL, 5, &g_lvgl_task_handle, 1);
#else
    int err = xTaskCreatePinnedToCore(gui_task, "lvglTask", 1024 * 8, NULL, 5, &g_lvgl_task_handle, 0);
#endif
    if(!err) {
        LOG_FATAL(TAG_GUI, "Create task for LVGL failed");
        return ESP_FAIL;
    }
    return ESP_OK;
}
#endif // HASP_USE_LVGL_TASK

IRAM_ATTR bool gui_acquire(TickType_t timeout)
{
#if ESP32
    TaskHandle_t task = xTaskGetCurrentTaskHandle();
    if(g_lvgl_task_handle == task) return true;
    if(xSemaphoreTake(xGuiSemaphore, timeout) != pdTRUE) return false;
#endif
    return true;
}

IRAM_ATTR void gui_release(void)
{
#if ESP32
    TaskHandle_t task = xTaskGetCurrentTaskHandle();
    if(g_lvgl_task_handle == task) return;
    xSemaphoreGive(xGuiSemaphore);
    // LOG_VERBOSE(TAG_TFT, F("GIVE"));
#endif
}

#endif // ESP32 && HASP_USE_ESP_MQTT

////////////////////////////////////////////////////////////////////////////////////////////////////
#if HASP_USE_CONFIG > 0
bool guiGetConfig(const JsonObject& settings)
{
    bool changed          = false;
    bool backlight_invert = haspDevice.get_backlight_invert();
    uint16_t guiSleepTime1;
    uint16_t guiSleepTime2;
    hasp_get_sleep_time(guiSleepTime1, guiSleepTime2);

    // if(guiTickPeriod != settings[FPSTR(FP_GUI_TICKPERIOD)].as<uint8_t>()) changed = true;
    // settings[FPSTR(FP_GUI_TICKPERIOD)] = guiTickPeriod;

    if(guiSleepTime1 != settings[FPSTR(FP_GUI_IDLEPERIOD1)].as<uint16_t>()) changed = true;
    settings[FPSTR(FP_GUI_IDLEPERIOD1)] = guiSleepTime1;

    if(guiSleepTime2 != settings[FPSTR(FP_GUI_IDLEPERIOD2)].as<uint16_t>()) changed = true;
    settings[FPSTR(FP_GUI_IDLEPERIOD2)] = guiSleepTime2;

    if(gui_settings.backlight_pin != settings[FPSTR(FP_GUI_BACKLIGHTPIN)].as<int8_t>()) changed = true;
    settings[FPSTR(FP_GUI_BACKLIGHTPIN)] = gui_settings.backlight_pin;

    if(backlight_invert != settings[FPSTR(FP_GUI_BACKLIGHTINVERT)].as<bool>()) changed = true;
    settings[FPSTR(FP_GUI_BACKLIGHTINVERT)] = (uint8_t)backlight_invert;

    if(gui_settings.rotation != settings[FPSTR(FP_GUI_ROTATION)].as<uint8_t>()) changed = true;
    settings[FPSTR(FP_GUI_ROTATION)] = gui_settings.rotation;

    if(gui_settings.show_pointer != settings[FPSTR(FP_GUI_POINTER)].as<bool>()) changed = true;
    settings[FPSTR(FP_GUI_POINTER)] = (uint8_t)gui_settings.show_pointer;

    if(gui_settings.invert_display != settings[FPSTR(FP_GUI_INVERT)].as<bool>()) changed = true;
    settings[FPSTR(FP_GUI_INVERT)] = (uint8_t)gui_settings.invert_display;

    /* Check CalData array has changed */
    JsonArray array = settings[FPSTR(FP_GUI_CALIBRATION)].as<JsonArray>();
    uint8_t i       = 0;
    size_t len      = sizeof(gui_settings.cal_data) / sizeof(gui_settings.cal_data[0]);
    for(JsonVariant v : array) {
        LOG_VERBOSE(TAG_GUI, F("GUI CONF: %d: %d <=> %d"), i, gui_settings.cal_data[i], v.as<uint16_t>());
        if(i < len) {
            if(gui_settings.cal_data[i] != v.as<uint16_t>()) changed = true;
            v.set(gui_settings.cal_data[i]);
        } else {
            changed = true;

#if TOUCH_DRIVER == 0x2046 && defined(USER_SETUP_LOADED) && defined(TOUCH_CS)
            // haspTft.tft.setTouch(gui_settings.cal_data);
            haspTouch.set_calibration(gui_settings.cal_data);
#elif TOUCH_DRIVER == 0x2046 && defined(HASP_USE_LGFX_TOUCH)
            // haspTft.tft.setTouchCalibrate(gui_settings.cal_data);
            haspTouch.set_calibration(gui_settings.cal_data);
#endif
        }
        i++;
    }

    /* Build new CalData array if the count is not correct */
    if(i != len) {
        array = settings[FPSTR(FP_GUI_CALIBRATION)].to<JsonArray>(); // Clear JsonArray
        for(int i = 0; i < len; i++) {
            array.add(gui_settings.cal_data[i]);
        }
        changed = true;

#if TOUCH_DRIVER == 0x2046 && defined(USER_SETUP_LOADED) && defined(TOUCH_CS)
        // haspTft.tft.setTouch(gui_settings.cal_data);
        haspTouch.set_calibration(gui_settings.cal_data);
#elif TOUCH_DRIVER == 0x2046 && defined(HASP_USE_LGFX_TOUCH)
        // haspTft.tft.setTouchCalibrate(gui_settings.cal_data);
        haspTouch.set_calibration(gui_settings.cal_data);
#endif
    }

    if(changed) configOutput(settings, TAG_GUI);
    return changed;
}

/** Set GUI Configuration.
 *
 * Read the settings from json and sets the application variables.
 *
 * @note: data pixel should be formatted to uint32_t RGBA. Imagemagick requirements.
 *
 * @param[in] settings    JsonObject with the config settings.
 **/
bool guiSetConfig(const JsonObject& settings)
{
    configOutput(settings, TAG_GUI);
    bool changed             = false;
    uint8_t backlight_invert = haspDevice.get_backlight_invert();
    uint16_t guiSleepTime1;
    uint16_t guiSleepTime2;

    hasp_get_sleep_time(guiSleepTime1, guiSleepTime2);

    // changed |= configSet(guiTickPeriod, settings[FPSTR(FP_GUI_TICKPERIOD)], F("guiTickPeriod"));
    changed |= configSet(gui_settings.backlight_pin, settings[FPSTR(FP_GUI_BACKLIGHTPIN)], F("guiBacklightPin"));
    changed |= configSet(backlight_invert, settings[FPSTR(FP_GUI_BACKLIGHTINVERT)], F("guiBacklightInvert"));
    changed |= configSet(guiSleepTime1, settings[FPSTR(FP_GUI_IDLEPERIOD1)], F("guiSleepTime1"));
    changed |= configSet(guiSleepTime2, settings[FPSTR(FP_GUI_IDLEPERIOD2)], F("guiSleepTime2"));
    changed |= configSet(gui_settings.rotation, settings[FPSTR(FP_GUI_ROTATION)], F("gui_settings.rotation"));
    changed |= configSet(gui_settings.invert_display, settings[FPSTR(FP_GUI_INVERT)], F("guiInvertDisplay"));

    hasp_set_sleep_time(guiSleepTime1, guiSleepTime2);
    haspDevice.set_backlight_invert(backlight_invert); // Update if changed

    if(!settings[FPSTR(FP_GUI_POINTER)].isNull()) {
        if(gui_settings.show_pointer != settings[FPSTR(FP_GUI_POINTER)].as<bool>()) {
            LOG_VERBOSE(TAG_GUI, F("guiShowPointer set"));
        }
        changed |= gui_settings.show_pointer != settings[FPSTR(FP_GUI_POINTER)].as<bool>();

        gui_settings.show_pointer = settings[FPSTR(FP_GUI_POINTER)].as<bool>();
        gui_hide_pointer(false);
    }

    if(!settings[FPSTR(FP_GUI_CALIBRATION)].isNull()) {
        bool status = false;
        int i       = 0;
        size_t len  = sizeof(gui_settings.cal_data) / sizeof(gui_settings.cal_data[0]);

        JsonArray array = settings[FPSTR(FP_GUI_CALIBRATION)].as<JsonArray>();
        for(JsonVariant v : array) {
            if(i < len) {
                if(gui_settings.cal_data[i] != v.as<uint16_t>()) status = true;
                gui_settings.cal_data[i] = v.as<uint16_t>();
            }
            i++;
        }

        if(gui_settings.cal_data[0] != 0 || gui_settings.cal_data[1] != 65535 || gui_settings.cal_data[2] != 0 ||
           gui_settings.cal_data[3] != 65535) {
            LOG_VERBOSE(TAG_GUI, F("calData set [%u, %u, %u, %u, %u]"), gui_settings.cal_data[0],
                        gui_settings.cal_data[1], gui_settings.cal_data[2], gui_settings.cal_data[3],
                        gui_settings.cal_data[4]);
            oobeSetAutoCalibrate(false);
        } else {
            LOG_TRACE(TAG_GUI, F("First Touch Calibration enabled"));
            oobeSetAutoCalibrate(true);
        }

#if TOUCH_DRIVER == 0x2046 && defined(USER_SETUP_LOADED) && defined(TOUCH_CS)
        // haspTft.tft.setTouch(gui_settings.cal_data);
        if(status) haspTouch.set_calibration(gui_settings.cal_data);
#elif TOUCH_DRIVER == 0x2046 && defined(HASP_USE_LGFX_TOUCH)
        // haspTft.tft.setTouchCalibrate(gui_settings.cal_data);
        if(status) haspTouch.set_calibration(gui_settings.cal_data);
#endif

        changed |= status;
    }

    return changed;
}
#endif // HASP_USE_CONFIG

/* **************************** SCREENSHOTS ************************************** */
#if HASP_USE_SPIFFS > 0 || HASP_USE_LITTLEFS > 0 || HASP_USE_HTTP > 0

/** Send Bitmap Header.
 *
 * Sends a header in BMP format for the size of the screen.
 *
 * @note: send header before refreshing the whole screen
 *
 **/
static void gui_get_bitmap_header(uint8_t* buffer, size_t bufsize)
{
    lv_obj_t* scr     = lv_disp_get_scr_act(NULL);
    lv_coord_t width  = lv_obj_get_width(scr);
    lv_coord_t height = lv_obj_get_height(scr);

    const char* bm = "BM";
    memcpy(buffer, bm, strlen(bm));
    buffer += strlen(bm);

    // Bitmap file header
    bmp_header_t* bmp = (bmp_header_t*)buffer;
    bmp->bfSize       = (uint32_t)(width * height * LV_COLOR_DEPTH / 8);
    bmp->bfReserved   = 0;
    bmp->bfOffBits    = bufsize;

    // Bitmap information header
    bmp->biSize          = 40;
    bmp->biWidth         = width;
    bmp->biHeight        = -height;
    bmp->biPlanes        = 1;
    bmp->biBitCount      = LV_COLOR_DEPTH;
    bmp->biCompression   = 3; // BI_BITFIELDS
    bmp->biSizeImage     = bmp->bfSize;
    bmp->biXPelsPerMeter = 2836;
    bmp->biYPelsPerMeter = 2836;
    bmp->biClrUsed       = 0; // zero defaults to 2^n
    bmp->biClrImportant  = 0;

    // BI_BITFIELDS
    bmp->bdMask[0] = 0xF800; // Red bitmask  : 1111 1000 | 0000 0000
    bmp->bdMask[1] = 0x07E0; // Green bitmask: 0000 0111 | 1110 0000
    bmp->bdMask[2] = 0x001F; // Blue bitmask : 0000 0000 | 0001 1111
}

void gui_flush_not_complete()
{
    LOG_WARNING(TAG_GUI, F("Pixelbuffer not completely sent"));
}
#endif // HASP_USE_SPIFFS > 0 || HASP_USE_LITTLEFS > 0 || HASP_USE_HTTP > 0

#if HASP_USE_SPIFFS > 0 || HASP_USE_LITTLEFS > 0
/* Flush VDB bytes to a file */
static void gui_screenshot_to_file(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* color_p)
{
    size_t len = (area->x2 - area->x1 + 1) * (area->y2 - area->y1 + 1); /* Number of pixels */
    len *= sizeof(lv_color_t);                                          /* Number of bytes */
    size_t res = pFileOut.write((uint8_t*)color_p, len);
    if(res != len) gui_flush_not_complete();

    // indirect callback to flush screenshot data to the screen
    drv_display_flush_cb(disp, area, color_p);
}

/** Take Screenshot.
 *
 * Flush buffer into a binary file.
 *
 * @note: data pixel should be formatted to uint16_t RGB. Set by Bitmap header.
 *
 * @param[in] pFileName   Output binary file name.
 *
 **/
void guiTakeScreenshot(const char* pFileName)
{
    uint8_t buffer[sizeof(bmp_header_t) + 2];
    gui_get_bitmap_header(buffer, sizeof(buffer));

    pFileOut = HASP_FS.open(pFileName, "w");
    if(pFileOut) {

        size_t len = pFileOut.write(buffer, sizeof(buffer));
        if(len == sizeof(buffer)) {
            LOG_VERBOSE(TAG_GUI, F("Bitmap header written"));

            /* Refresh screen to screenshot callback */
            lv_disp_t* disp       = lv_disp_get_default();
            drv_display_flush_cb  = disp->driver.flush_cb; /* store callback */
            disp->driver.flush_cb = gui_screenshot_to_file;

            lv_obj_invalidate(lv_scr_act());
            lv_refr_now(NULL);                            /* Will call our disp_drv.disp_flush function */
            disp->driver.flush_cb = drv_display_flush_cb; /* restore callback */

            LOG_VERBOSE(TAG_GUI, F("Bitmap data flushed to %s"), pFileName);

        } else {
            LOG_ERROR(TAG_GUI, F("Data written does not match header size"));
        }
        pFileOut.close();

    } else {
        LOG_WARNING(TAG_GUI, F(D_FILE_SAVE_FAILED), pFileName);
    }
}
#endif

#if HASP_USE_HTTP > 0
/* Flush VDB bytes to a webclient */
static void gui_screenshot_to_http(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* color_p)
{
    size_t len = (area->x2 - area->x1 + 1) * (area->y2 - area->y1 + 1); /* Number of pixels */
    len *= sizeof(lv_color_t);                                          /* Number of bytes */
    size_t res = httpClientWrite((uint8_t*)color_p, len);
    if(res != len) gui_flush_not_complete();

    lv_disp_flush_ready(disp);
}

static void gui_screenshot_to_both(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* color_p)
{
    gui_screenshot_to_http(disp, area, color_p);

    // indirect callback to flush screenshot data to the screen
    drv_display_flush_cb(disp, area, color_p);
}

/** Take Screenshot.
 *
 * Flush buffer into a http client.
 *
 * @note: data pixel should be formatted to uint16_t RGB. Set by Bitmap header.
 *
 **/
void guiTakeScreenshot()
{
    uint8_t buffer[sizeof(bmp_header_t) + 2];
    gui_get_bitmap_header(buffer, sizeof(buffer));

    if(httpClientWrite(buffer, sizeof(buffer)) == sizeof(buffer)) {
        LOG_VERBOSE(TAG_GUI, F("Bitmap header sent"));

        lv_disp_t* disp      = lv_disp_get_default();
        drv_display_flush_cb = disp->driver.flush_cb; /* store callback */

        if(disp->driver.sw_rotate) {
            disp->driver.flush_cb  = gui_screenshot_to_http;
            disp->driver.sw_rotate = 0;
            lv_obj_invalidate(lv_scr_act());
            lv_refr_now(NULL);                            /* Will call our disp_drv.disp_flush function */
            disp->driver.flush_cb = drv_display_flush_cb; /* restore callback */

            disp->driver.sw_rotate = 1; /* redraw to screen */
            lv_obj_invalidate(lv_scr_act());
            lv_refr_now(NULL);
        } else {
            /* Refresh screen to screenshot callback */
            disp->driver.flush_cb = gui_screenshot_to_both;
            lv_obj_invalidate(lv_scr_act());
            lv_refr_now(NULL);                            /* Will call our disp_drv.disp_flush function */
            disp->driver.flush_cb = drv_display_flush_cb; /* restore callback */
        }

        screenshotIsDirty = false;
        LOG_VERBOSE(TAG_GUI, F("Bitmap data flushed to webclient"));
    } else {
        LOG_ERROR(TAG_GUI, F("Data sent does not match header size"));
    }
}

bool guiScreenshotIsDirty()
{
    return screenshotIsDirty;
}

uint32_t guiScreenshotEtag()
{
    screenshotEtag += screenshotIsDirty;
    LOG_DEBUG(TAG_GUI, F("The ETag is %u"), screenshotEtag);
    return screenshotEtag;
}
#endif
