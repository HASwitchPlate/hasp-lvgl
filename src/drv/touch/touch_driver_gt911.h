/* MIT License - Copyright (c) 2019-2024 Francis Van Roie
   For full license information read the LICENSE file in the project folder */

#ifndef HASP_GT911_TOUCH_DRIVER_H
#define HASP_GT911_TOUCH_DRIVER_H

#ifdef ARDUINO
#include "lvgl.h"
#include "touch_driver.h"

namespace dev {

class TouchGt911 : public BaseTouch {
  public:
    IRAM_ATTR bool read(lv_indev_drv_t* indev_driver, lv_indev_data_t* data);
    void init(int w, int h);
#ifdef NOISE_REDUCTION
    void setup_noise_reduction(uint8_t nr_level);
#endif
};

} // namespace dev

using dev::TouchGt911;
extern dev::TouchGt911 haspTouch;

#endif // ARDUINO

#endif // HASP_GT911_TOUCH_DRIVER_H
