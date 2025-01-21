/* MIT License - Copyright (c) 2019-2024 Francis Van Roie
   For full license information read the LICENSE file in the project folder */

#if defined(ARDUINO) && (TOUCH_DRIVER == 0x0911) && !defined(HASP_USE_LGFX_TOUCH)
#include <Arduino.h>
#include "ArduinoLog.h"
#include "hasp_conf.h"
#include "touch_driver_gt911.h"

#include <Wire.h>
#include "Goodix.h"

#include "touch_driver.h" // base class
#include "touch_helper.h" // i2c scanner

#include "../../hasp/hasp.h" // for hasp_sleep_state
extern uint8_t hasp_sleep_state;

static Goodix touch = Goodix();
// static int8_t GT911_num_touches;
// static GTPoint* GT911_points;

// Store touch points into global variable
IRAM_ATTR void GT911_setXY(int8_t contacts, GTPoint* points)
{
    // GT911_num_touches = contacts;
    // GT911_points      = points;

    // LOG_VERBOSE(TAG_GUI, F("Contacts: %d"), contacts);
    // for(int i = 0; i < contacts; i++) {
    //     LOG_VERBOSE(TAG_GUI, F("C%d: #%d %d,%d s:%d"), i, points[i].trackId, points[i].x, points[i].y,
    //     points[i].area); yield();
    // }
}

// IRAM_ATTR bool touch_read(lv_indev_drv_t* indev_driver, lv_indev_data_t* data)
// {
//     //  LOG_VERBOSE(TAG_GUI, F("Contacts: %d"), GT911_num_touches);
//     static GTPoint points[5];

//     if(touch.readInput((uint8_t*)&points) > 0) {

//         if(hasp_sleep_state != HASP_SLEEP_OFF) hasp_update_sleep_state(); // update Idle

//         data->point.x = points[0].x;
//         data->point.y = points[0].y;
//         data->state   = LV_INDEV_STATE_PR;

//     } else {
//         data->state = LV_INDEV_STATE_REL;
//     }

//     touch.loop(); // reset IRQ

//     /*Return `false` because we are not buffering and no more data to read*/
//     return false;
// }

namespace dev {

IRAM_ATTR bool TouchGt911::read(lv_indev_drv_t* indev_driver, lv_indev_data_t* data)
{
    //  LOG_VERBOSE(TAG_GUI, F("Contacts: %d"), GT911_num_touches);
    static GTPoint points[5];

    if(touch.readInput((uint8_t*)&points) > 0) {

        if(hasp_sleep_state != HASP_SLEEP_OFF) hasp_update_sleep_state(); // update Idle

#ifdef TOUCH_WIDTH
        data->point.x = map(points[0].x, 0, TOUCH_WIDTH - 1, 0, TFT_WIDTH - 1);
#else
        data->point.x = points[0].x;
#endif

#ifdef TOUCH_HEIGHT
        data->point.y = map(points[0].y, 0, TOUCH_HEIGHT - 1, 0, TFT_HEIGHT - 1);
#else
        data->point.y = points[0].y;
#endif

        data->state = LV_INDEV_STATE_PR;
        hasp_set_sleep_offset(0); // Reset the offset

    } else {
        data->state = LV_INDEV_STATE_REL;
    }

    // touch.loop(); // reset IRQ (now in readInput)

    /*Return `false` because we are not buffering and no more data to read*/
    return false;
}

#ifdef NOISE_REDUCTION
void TouchGt911::setup_noise_reduction(uint8_t nr_level)
{
    uint8_t len = 0x8100 - GT_REG_CFG;
    uint8_t cfg[len];
    GTInfo* info;
    uint8_t err;

    memset(cfg, 0, len);
/*    This is the only way to read the entire config space.
    The Goodix driver provides a readConfig() function, but
    struct is not packed which leads to errors (when using
    struct members).

    Need to do a split read as the WDT will bite for reads
    of more than 128 bytes (give or take).
*/
    err = touch.read(GT_REG_CFG, cfg, 100);
    if (err != 0) goto end2;
    if (cfg[11] == nr_level) {
        LOG_INFO(TAG_DRVR, "GT911 noise reduction unchanged");
        return;
    }
    err = touch.read(GT_REG_CFG+100, cfg+100, len-100);
    if (err != 0) goto end2;

    if (cfg[len - 1] != touch.calcChecksum(cfg, len - 1)) goto end2;

    // Check noise_reduction is within limits
    if (nr_level < 0 || nr_level > 15) {
        LOG_ERROR(TAG_DRVR, "GT911 Noise Reduction value out of range (0-15)");
        return;
    }
    cfg[11] = nr_level;
    cfg[len - 1] = touch.calcChecksum(cfg, len - 1);

    err = touch.write(GT_REG_CFG, cfg, len);
    if (err != 0) goto end;
    err = touch.write(0x8100, 1);
    if (err != 0) goto end;
    LOG_INFO(TAG_DRVR, "GT911 noise reduction updated");
    return;
end:
    LOG_ERROR(TAG_DRVR, "GT911 Failed to write noise reduction byte");
    return;
end2:
    LOG_ERROR(TAG_DRVR, "GT911 Failed to read config space");
}
#endif

void TouchGt911::init(int w, int h)
{
    Wire.begin(TOUCH_SDA, TOUCH_SCL, (uint32_t)I2C_TOUCH_FREQUENCY);

    touch.setHandler(GT911_setXY);
    GTInfo* info;

    if(touch.begin(TOUCH_IRQ, TOUCH_RST, I2C_TOUCH_ADDRESS)) {
        info = touch.readInfo();
        if(info->xResolution > 0 && info->yResolution > 0) goto found;
    }

#if TOUCH_IRQ == -1
    // Probe both addresses if IRQ is not connected
    for(uint8_t i = 0; i < 4; i++)
        if(touch.begin(TOUCH_IRQ, TOUCH_RST, i < 2 ? 0x5d : 0x14)) {
            info = touch.readInfo();
            if(info->xResolution > 0 && info->yResolution > 0) goto found;
        }
#endif

found:
    if(info->xResolution != 0 && info->yResolution != 0) {
        LOG_INFO(TAG_DRVR, "GT911 %s (%dx%d)", D_SERVICE_STARTED, info->xResolution, info->yResolution);
        // uint8_t len = touch.fwResolution(480, 272);
    } else {
        LOG_WARNING(TAG_DRVR, "GT911 %s", D_SERVICE_START_FAILED);
    }

    Wire.begin(TOUCH_SDA, TOUCH_SCL, (uint32_t)I2C_TOUCH_FREQUENCY);
    touch_scan(Wire); // The address could change during begin, so scan afterwards
}

} // namespace dev

dev::TouchGt911 haspTouch;

#endif // ARDUINO
