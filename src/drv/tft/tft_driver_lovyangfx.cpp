/* MIT License - Copyright (c) 2019-2024 Francis Van Roie
   For full license information read the LICENSE file in the project folder */

#if defined(ARDUINO) && defined(LGFX_USE_V1)
#include <Arduino.h>
#include <Preferences.h>
#include <sdkconfig.h>

#include "LovyanGFX.hpp"
// #if defined(ESP32S3)
#include "lgfx/v1/platforms/esp32s3/Panel_RGB.hpp"
#include "lgfx/v1/platforms/esp32s3/Bus_RGB.hpp"
// #endif

#include "tft_driver.h"
#include "tft_driver_lovyangfx.h"
#include "Panel_ILI9481b.hpp"
#include "M5Stack.hpp"

namespace dev {

void tftPinInfo(const __FlashStringHelper* pinfunction, int8_t pin)
{
    if(pin != -1) {
        char buffer[64];
        snprintf_P(buffer, sizeof(buffer), PSTR("%-12s: %s (GPIO %02d)"), String(pinfunction).c_str(),
                   haspDevice.gpio_name(pin).c_str(), pin);
        LOG_VERBOSE(TAG_TFT, buffer);
    }
}

static void _pin_level(int_fast16_t pin, bool level)
{
    lgfx::pinMode(pin, lgfx::pin_mode_t::output);
    if(level)
        lgfx::gpio_hi(pin);
    else
        lgfx::gpio_lo(pin);
}

static uint32_t _read_panel_id(lgfx::Bus_SPI* bus, int32_t pin_cs, uint32_t cmd = 0x04,
                               uint8_t dummy_read_bit = 1) // 0x04 = RDDID command
{
    bus->beginTransaction();
    _pin_level(pin_cs, true);
    bus->writeCommand(0, 8);
    bus->wait();
    _pin_level(pin_cs, false);
    bus->writeCommand(cmd, 8);
    if(dummy_read_bit) bus->writeData(0, dummy_read_bit); // dummy read bit
    bus->beginRead();
    uint32_t res = bus->readData(32);
    bus->endTransaction();
    _pin_level(pin_cs, true);

    LOG_VERBOSE(TAG_TFT, "[Autodetect] read cmd:%u = %u", cmd, res);
    return res;
}

#if defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3)
static lgfx::Bus_Parallel16* init_parallel_16_bus(Preferences* prefs, int8_t data_pins[], uint8_t num)
{
    lgfx::Bus_Parallel16* bus = new lgfx::v1::Bus_Parallel16();
    auto cfg                  = bus->config();
    cfg.pin_rd                = prefs->getInt("rd", TFT_RD);
    cfg.pin_wr                = prefs->getInt("wr", TFT_WR);
    cfg.pin_rs                = prefs->getInt("rs", TFT_DC);
    cfg.freq_write            = prefs->getUInt("write_freq", SPI_FREQUENCY);
#if !defined(CONFIG_IDF_TARGET_ESP32S3)
    uint8_t port = prefs->getUInt("i2s_port", 0);
    switch(port) {
#if SOC_I2S_NUM > 1
        case 1:
            cfg.i2s_port = I2S_NUM_1;
            break;
#endif
        default:
            cfg.i2s_port = I2S_NUM_0;
    }
#endif
    for(uint8_t i = 0; i < num; i++) {
        cfg.pin_data[i] = data_pins[i];
    }
    bus->config(cfg); // The set value is reflected on the bus.
    LOG_DEBUG(TAG_TFT, F("%s - %d"), __FILE__, __LINE__);
    return bus;
}
#endif // ESP32S2/S3

static lgfx::Bus_Parallel8* init_parallel_8_bus(Preferences* prefs, int8_t data_pins[], uint8_t num)
{
    lgfx::Bus_Parallel8* bus = new lgfx::v1::Bus_Parallel8();
    auto cfg                 = bus->config();
    cfg.pin_rd               = prefs->getInt("rd", TFT_RD);
    cfg.pin_wr               = prefs->getInt("wr", TFT_WR);
    cfg.pin_rs               = prefs->getInt("rs", TFT_DC);
#ifndef CONFIG_IDF_TARGET_ESP32C3
    cfg.freq_write = prefs->getUInt("write_freq", SPI_FREQUENCY);
#endif

#if !defined(CONFIG_IDF_TARGET_ESP32S3) && !defined(CONFIG_IDF_TARGET_ESP32C3)
    uint8_t port = prefs->getUInt("i2s_port", 0);
    switch(port) {
#if SOC_I2S_NUM > 1
        case 1:
            cfg.i2s_port = I2S_NUM_1;
            break;
#endif
        default:
            cfg.i2s_port = I2S_NUM_0;
    }
#endif
    for(uint8_t i = 0; i < num; i++) {
        cfg.pin_data[i] = data_pins[i];
    }
    bus->config(cfg); // The set value is reflected on the bus.
    LOG_DEBUG(TAG_TFT, F("%s - %d"), __FILE__, __LINE__);
    return bus;
}

static lgfx::Bus_SPI* init_spi_bus(Preferences* prefs)
{
    lgfx::Bus_SPI* bus = new lgfx::v1::Bus_SPI();
    auto cfg           = bus->config();
    cfg.pin_miso       = prefs->getInt("miso", TFT_MISO);
    cfg.pin_mosi       = prefs->getInt("mosi", TFT_MOSI);
    cfg.pin_sclk       = prefs->getInt("sclk", TFT_SCLK);
    cfg.pin_dc         = prefs->getInt("dc", TFT_DC);
    cfg.spi_3wire      = prefs->getBool("3wire", false);
    cfg.use_lock       = prefs->getBool("use_lock", true);
    cfg.freq_write     = prefs->getUInt("write_freq", SPI_FREQUENCY);
    cfg.freq_read      = prefs->getUInt("read_freq", SPI_READ_FREQUENCY);
    cfg.dma_channel    = prefs->getUInt("dma_channel", TFT_DMA_CHANNEL);
    cfg.spi_mode       = prefs->getUInt("spi_mode", TFT_SPI_MODE);
    uint8_t host       = prefs->getUInt("host", TFT_SPI_HOST);
    LOG_DEBUG(TAG_TFT, F("%s - %d"), __FILE__, __LINE__);

    // #if CONFIG_IDF_TARGET_ESP32C3
    // #define FSPI 0
    // #define HSPI 1
    // #else
    // #define FSPI 1 // SPI bus attached to the flash (can use the same data lines but different SS)
    // #define HSPI 2 // SPI bus normally mapped to pins 12 - 15, but can be matrixed to any pins
    // #if CONFIG_IDF_TARGET_ESP32
    // #define VSPI 3 // SPI bus normally attached to pins 5, 18, 19 and 23, but can be matrixed to any pins
    // #endif
    // #endif

    // //alias for different chips, deprecated for the chips after esp32s2
    // #ifdef CONFIG_IDF_TARGET_ESP32
    // #define SPI_HOST    SPI1_HOST
    // #define HSPI_HOST   SPI2_HOST
    // #define VSPI_HOST   SPI3_HOST
    // #elif CONFIG_IDF_TARGET_ESP32S2
    // // SPI_HOST (SPI1_HOST) is not supported by the SPI Master and SPI Slave driver on ESP32-S2 and later
    // #define SPI_HOST    SPI1_HOST
    // #define FSPI_HOST   SPI2_HOST
    // #define HSPI_HOST   SPI3_HOST
    // #endif

    switch(host) {
#ifdef CONFIG_IDF_TARGET_ESP32
        case 1:
            // SPI_HOST (SPI1_HOST) is not supported by the SPI Master and SPI Slave driver on ESP32-S2 and later
            LOG_DEBUG(TAG_TFT, F("%s - %d"), __FILE__, __LINE__);
            cfg.spi_host = SPI_HOST;
            break;
#endif
        case 2: // HSPI on ESP32 and FSPI on ESP32-S2
            LOG_DEBUG(TAG_TFT, F("%s - %d"), __FILE__, __LINE__);
#ifdef CONFIG_IDF_TARGET_ESP32
            cfg.spi_host = HSPI_HOST;
#elif CONFIG_IDF_TARGET_ESP32S2
            cfg.spi_host = FSPI_HOST;
#endif
            break;
        case 3:
        default: // VSPI on ESP32 and HSPI on ESP32-S2
            LOG_DEBUG(TAG_TFT, F("%s - %d"), __FILE__, __LINE__);
#ifdef CONFIG_IDF_TARGET_ESP32
            cfg.spi_host = VSPI_HOST;
#elif CONFIG_IDF_TARGET_ESP32S2
            cfg.spi_host = HSPI_HOST;
#endif
    }
    bus->config(cfg); // The set value is reflected on the bus.
    bus->init();
    return bus;
}

static void configure_panel(lgfx::Panel_Device* panel, Preferences* prefs)
{
    auto cfg = panel->config(); // Get the structure for display panel settings.

    cfg.pin_cs   = prefs->getInt("cs", TFT_CS);     // CS required
    cfg.pin_rst  = prefs->getInt("rst", TFT_RST);   // RST sum development board RST linkage
    cfg.pin_busy = prefs->getInt("busy", TFT_BUSY); // Pin number to which BUSY is connected (-1 = disable)

    // The following setting values are set to general initial values for each panel, so please comment out any unknown
    // items and try them.

    cfg.panel_width   = prefs->getUInt("panel_width", TFT_WIDTH);          // Actually displayable width
    cfg.panel_height  = prefs->getUInt("panel_height", TFT_HEIGHT);        // Height that can actually be displayed
    cfg.memory_width  = prefs->getUInt("memory_width", cfg.panel_width);   // Maximum width supported by driver IC
    cfg.memory_height = prefs->getUInt("memory_height", cfg.panel_height); // Maximum height supported by driver IC

    cfg.offset_x = prefs->getUInt("offset_x", TFT_OFFSET_X); // Amount of offset in the X direction of the panel
    cfg.offset_y = prefs->getUInt("offset_y", TFT_OFFSET_Y); // Amount of offset in the Y direction of the panel
    cfg.offset_rotation =
        prefs->getUInt("offset_rotation", TFT_OFFSET_ROTATION); // Offset of the rotation 0 ~ 7 (4 ~ 7 is upside down)

    cfg.dummy_read_pixel = prefs->getUInt("dummy_read_pixel", 8); // Number of dummy read bits before pixel read
    cfg.dummy_read_bits =
        prefs->getUInt("dummy_read_bits", 1);         // bits of dummy read before reading data other than pixels
    cfg.readable = prefs->getBool("readable", false); // true if data can be read

#ifdef INVERT_COLORS // This is configurable un Web UI
    cfg.invert =
        prefs->getBool("invert", INVERT_COLORS != 0); // true if the light and darkness of the panel is reversed
#else
    cfg.invert = prefs->getBool("invert", false); // true if the light and darkness of the panel is reversed
#endif
#ifdef TFT_RGB_ORDER
    cfg.rgb_order =
        prefs->getBool("rgb_order", TFT_RGB_ORDER != 0); // true if the red and blue of the panel are swapped
#else
    cfg.rgb_order = prefs->getBool("rgb_order", false); // true if the red and blue of the panel are swapped
#endif

    bool dlen_16bit = false;
#if defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3)
    if(panel->getBus()) {
        lgfx::v1::bus_type_t bus_type = panel->getBus()->busType();
        if(bus_type == lgfx::v1::bus_parallel16) dlen_16bit = true;
    }
#endif
    cfg.dlen_16bit = prefs->getBool("dlen_16bit", dlen_16bit); // true for panels that send data length in 16-bit units
    cfg.bus_shared = prefs->getBool("bus_shared", true);       // true if the bus is shared with the SD card
                                                               // (bus control is performed with drawJpgFile etc.)
    panel->config(cfg);
}

// Initialize the bus
lgfx::IBus* _init_bus(Preferences* preferences)
{
    if(!preferences) return nullptr;

    char key[8];
    int8_t data_pins[16] = {TFT_D0, TFT_D1, TFT_D2,  TFT_D3,  TFT_D4,  TFT_D5,  TFT_D6,  TFT_D7,
                            TFT_D8, TFT_D9, TFT_D10, TFT_D11, TFT_D12, TFT_D13, TFT_D14, TFT_D15};
    for(uint8_t i = 0; i < 16; i++) {
        snprintf(key, sizeof(key), "d%d", i + 1);
        data_pins[i] = preferences->getInt(key, data_pins[i]);
        LOG_DEBUG(TAG_TFT, F("D%d: %d"), i, data_pins[i]);
    }

    LOG_DEBUG(TAG_TFT, F("%s - %d"), __FILE__, __LINE__);
    bool is_8bit  = true;
    bool is_16bit = true;
    for(uint8_t i = 0; i < 16; i++) {
        if(i < 8) is_8bit = is_8bit && (data_pins[i] >= 0);
        is_16bit = is_16bit && (data_pins[i] >= 0);
    }

    LOG_DEBUG(TAG_TFT, F("%s - %d"), __FILE__, __LINE__);
#if defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3)
    if(is_16bit) {
        is_8bit = false;
        LOG_VERBOSE(TAG_TFT, F("16-bit TFT bus"));
        return init_parallel_16_bus(preferences, data_pins, 16);
    } else
#endif // ESP32S2/S3
        if(is_8bit) {
            is_16bit = false;
            LOG_VERBOSE(TAG_TFT, F("8-bit TFT bus"));
            return init_parallel_8_bus(preferences, data_pins, 8);
        } else {
            LOG_VERBOSE(TAG_TFT, F("SPI TFT bus"));
            return init_spi_bus(preferences);
        }
    return nullptr;
}

lgfx::Panel_Device* LovyanGfx::_init_panel(lgfx::IBus* bus)
{
    lgfx::Panel_Device* panel = nullptr;
    switch(tft_driver) {
        case TFT_PANEL_ILI9341: {
            panel = new lgfx::Panel_ILI9341();
            LOG_VERBOSE(TAG_TFT, F("Panel_ILI9341"));
            break;
        }
        case TFT_PANEL_ILI9342: {
            panel = new lgfx::Panel_ILI9342();
            LOG_VERBOSE(TAG_TFT, F("Panel_ILI9342"));
            break;
        }
        case TFT_PANEL_R61529:
        case TFT_PANEL_ILI9481: {
            panel = new lgfx::Panel_ILI9481_b();
            LOG_VERBOSE(TAG_TFT, F("Panel_ILI9481_b"));
            break;
        }
        case TFT_PANEL_ILI9486: {
            panel = new lgfx::Panel_ILI9486();
            LOG_VERBOSE(TAG_TFT, F("Panel_ILI9486"));
            break;
        }
        case TFT_PANEL_ILI9488: {
            panel = new lgfx::Panel_ILI9488();
            LOG_VERBOSE(TAG_TFT, F("Panel_ILI9488"));
            break;
        }
        case TFT_PANEL_ST7789: {
            panel = new lgfx::Panel_ST7789();
            LOG_VERBOSE(TAG_TFT, F("Panel_ST7789"));
            break;
        }
        case TFT_PANEL_ST7796: {
            panel = new lgfx::Panel_ST7796();
            LOG_VERBOSE(TAG_TFT, F("Panel_ST7796"));
            break;
        }
        case TFT_PANEL_HX8357D: {
            panel = new lgfx::Panel_HX8357D();
            LOG_VERBOSE(TAG_TFT, F("Panel_HX8357D"));
            break;
        }
        case TFT_PANEL_RGB: {
            // panel = new lgfx::Panel_RGB();
            LOG_VERBOSE(TAG_TFT, F("Panel_RGB"));
            break;
        }
        case TFT_PANEL_GC9A01: {
            panel = new lgfx::Panel_GC9A01();
            LOG_VERBOSE(TAG_TFT, F("Panel_GC9A01"));
            break;
        }
        default: { // Needs to be in curly braces
            LOG_FATAL(TAG_TFT, F(D_SERVICE_START_FAILED ": %s line %d"), __FILE__, __LINE__);
        }
    }

    return panel;
}

lgfx::ITouch* _init_touch(Preferences* preferences)
{
    // lgfx::ITouch* touch = nullptr;
    // switch(tft_driver) {
    //     default:
    // case 0x9341: {
    // break;
    //       }

#if defined(HASP_USE_LGFX_TOUCH)

#if TOUCH_DRIVER == 0x0911
    { // タッチスクリーン制御の設定を行います。（必要なければ削除）
        auto touch = new lgfx::Touch_GT911();
        auto cfg   = touch->config();

        cfg.x_min      = 0;              // タッチスクリーンから得られる最小のX値(生の値)
        cfg.x_max      = TFT_WIDTH - 1;  // タッチスクリーンから得られる最大のX値(生の値)
        cfg.y_min      = 0;              // タッチスクリーンから得られる最小のY値(生の値)
        cfg.y_max      = TFT_HEIGHT - 1; // タッチスクリーンから得られる最大のY値(生の値)
        cfg.bus_shared = false;          // 画面と共通のバスを使用している場合 trueを設定
        cfg.offset_rotation = TOUCH_OFFSET_ROTATION; // 表示とタッチの向きのが一致しない場合の調整 0~7の値で設定

        // I2C接続の場合
        cfg.i2c_port = I2C_TOUCH_PORT;      // 使用するI2Cを選択 (0 or 1)
        cfg.i2c_addr = I2C_TOUCH_ADDRESS;   // I2Cデバイスアドレス番号
        cfg.pin_sda  = TOUCH_SDA;           // SDAが接続されているピン番号
        cfg.pin_scl  = TOUCH_SCL;           // SCLが接続されているピン番号
        cfg.pin_int  = TOUCH_IRQ;           // INTが接続されているピン番号
        cfg.freq     = I2C_TOUCH_FREQUENCY; // I2Cクロックを設定

        touch->config(cfg);
        return touch;
    }
#endif

#if TOUCH_DRIVER == 0x0820

    { // タッチスクリーン制御の設定を行います。（必要なければ削除）
        auto touch = new lgfx::Touch_FT5x06();
        auto cfg   = touch->config();

        cfg.x_min      = 0;              // The minimum X value obtained from the touchscreen (raw value).
        cfg.x_max      = TFT_WIDTH - 1;  // The maximum X value obtained from the touchscreen (raw value).
        cfg.y_min      = 0;              // The minimum Y value obtained from the touchscreen (raw value).
        cfg.y_max      = TFT_HEIGHT - 1; // The maximum Y value obtained from the touchscreen (raw value).
        cfg.bus_shared = true;           // If using a common bus with the screen, set it to true.
        cfg.offset_rotation = TOUCH_OFFSET_ROTATION; // Adjustment when the display orientation does not match the touch orientation. Set in values from 0 to 7.

        // I2C接続の場合
        cfg.i2c_port = I2C_TOUCH_PORT;      // 使用するI2Cを選択 (0 or 1)
        cfg.i2c_addr = I2C_TOUCH_ADDRESS;   // I2Cデバイスアドレス番号
        cfg.pin_sda  = TOUCH_SDA;           // SDAが接続されているピン番号
        cfg.pin_scl  = TOUCH_SCL;           // SCLが接続されているピン番号
        cfg.pin_int  = TOUCH_IRQ;           // INTが接続されているピン番号
        cfg.freq     = I2C_TOUCH_FREQUENCY; // I2Cクロックを設定

        touch->config(cfg);
        return touch;
    }
#endif

#if TOUCH_DRIVER == 0x6336
    {
        auto touch = new lgfx::Touch_FT5x06();
        auto cfg   = touch->config();

        cfg.x_min           = 0;
        cfg.x_max           = TFT_WIDTH - 1;
        cfg.y_min           = 0;
        cfg.y_max           = TFT_HEIGHT - 1;
        cfg.pin_int         = TOUCH_IRQ;
        cfg.bus_shared      = true;
        cfg.offset_rotation = TOUCH_OFFSET_ROTATION;

        // SPI接続の場合
        // cfg.spi_host = VSPI_HOST; // 使用するSPIを選択 (HSPI_HOST or VSPI_HOST)
        // cfg.freq     = 1000000;   // SPIクロックを設定
        // cfg.pin_sclk = 18;        // SCLKが接続されているピン番号
        // cfg.pin_mosi = 23;        // MOSIが接続されているピン番号
        // cfg.pin_miso = 19;        // MISOが接続されているピン番号
        // cfg.pin_cs   = 5;         //   CSが接続されているピン番号

        // I2C接続の場合
        cfg.i2c_port = I2C_TOUCH_PORT;
        cfg.i2c_addr = I2C_TOUCH_ADDRESS;
        cfg.pin_sda  = TOUCH_SDA;
        cfg.pin_scl  = TOUCH_SCL;
        cfg.freq     = I2C_TOUCH_FREQUENCY;

        touch->config(cfg);
        return touch;
    }
#endif

#if TOUCH_DRIVER == 0x2046
    {
        auto touch = new lgfx::Touch_XPT2046();
        auto cfg   = touch->config();
        LOG_DEBUG(TAG_TFT, F("%s - %d"), __FILE__, __LINE__);

        cfg.bus_shared      = true;
        cfg.offset_rotation = 0;

        cfg.spi_host = SPI3_HOST;
        // cfg.pin_sclk = 18;
        // cfg.pin_mosi = 23;
        // cfg.pin_miso = 19;
        // cfg.pin_cs   = 21;
        // cfg.pin_int  = GPIO_NUM_NC;
        cfg.pin_sclk = TOUCH_SCLK;
        cfg.pin_miso = TOUCH_MISO;
        cfg.pin_mosi = TOUCH_MOSI;
        cfg.pin_cs   = TOUCH_CS;
        cfg.pin_int  = TOUCH_IRQ;
        touch->config(cfg);
        return touch;
    }
#endif

#if TOUCH_DRIVER == 0x21100
    {
        auto touch = new lgfx::Touch_TT21xxx();
        auto cfg   = touch->config();

        cfg.x_min           = 0;
        cfg.x_max           = TFT_WIDTH - 1;
        cfg.y_min           = 0;
        cfg.y_max           = TFT_HEIGHT - 1;
        cfg.pin_int         = TOUCH_IRQ;
        cfg.bus_shared      = true;
        cfg.offset_rotation = TOUCH_OFFSET_ROTATION;

        // I2C接続の場合
        cfg.i2c_port = I2C_TOUCH_PORT;
        cfg.i2c_addr = I2C_TOUCH_ADDRESS;
        cfg.pin_sda  = TOUCH_SDA;
        cfg.pin_scl  = TOUCH_SCL;
        cfg.freq     = I2C_TOUCH_FREQUENCY;

        touch->config(cfg);
        return touch;
    }
#endif

#if TOUCH_DRIVER == 0x816
    {
        auto touch = new lgfx::Touch_CST816S();
        auto cfg   = touch->config();

        cfg.x_min           = 0;
        cfg.x_max           = TFT_WIDTH - 1;
        cfg.y_min           = 0;
        cfg.y_max           = TFT_HEIGHT - 1;
        cfg.pin_int         = TOUCH_IRQ;
        cfg.bus_shared      = true;
        cfg.offset_rotation = TOUCH_OFFSET_ROTATION;

        // I2C接続の場合
        cfg.i2c_port = I2C_TOUCH_PORT;
        cfg.i2c_addr = I2C_TOUCH_ADDRESS;
        cfg.pin_sda  = TOUCH_SDA;
        cfg.pin_scl  = TOUCH_SCL;
        cfg.freq     = I2C_TOUCH_FREQUENCY;

        touch->config(cfg);
        return touch;
    }
#endif

#endif // HASP_USE_LGFX_TOUCH

    return nullptr;
}

void LovyanGfx::init(int w, int h)
{
    LOG_TRACE(TAG_TFT, F(D_SERVICE_STARTING));

#if defined(WTSC01PLUS)
    auto _panel_instance = new lgfx::Panel_ST7796();
    auto _bus_instance   = new lgfx::v1::Bus_Parallel8();
    auto _touch_instance = new lgfx::Touch_FT5x06();
    {
        auto cfg = _bus_instance->config();

        cfg.freq_write = 20000000;
        cfg.pin_wr     = 47;
        cfg.pin_rd     = -1;
        cfg.pin_rs     = 0;
        cfg.pin_d0     = 9;
        cfg.pin_d1     = 46;
        cfg.pin_d2     = 3;
        cfg.pin_d3     = 8;
        cfg.pin_d4     = 18;
        cfg.pin_d5     = 17;
        cfg.pin_d6     = 16;
        cfg.pin_d7     = 15;
        _bus_instance->config(cfg);
        _panel_instance->setBus(_bus_instance);
    }

    {
        auto cfg = _panel_instance->config();

        cfg.pin_cs   = -1;
        cfg.pin_rst  = 4;
        cfg.pin_busy = -1;

        cfg.panel_width      = 320;
        cfg.panel_height     = 480;
        cfg.offset_x         = 0;
        cfg.offset_y         = 0;
        cfg.offset_rotation  = 0;
        cfg.dummy_read_pixel = 8;
        cfg.dummy_read_bits  = 1;
        cfg.readable         = true;
        cfg.invert           = true;
        cfg.rgb_order        = false;
        cfg.dlen_16bit       = false;
        cfg.bus_shared       = true;

        _panel_instance->config(cfg);
    }

    {
        auto cfg = _touch_instance->config();

        cfg.x_min           = 0;
        cfg.x_max           = 319;
        cfg.y_min           = 0;
        cfg.y_max           = 479;
        cfg.pin_int         = 7;
        cfg.bus_shared      = true;
        cfg.offset_rotation = 0;

        cfg.i2c_port = 1;
        cfg.i2c_addr = 0x38;
        cfg.pin_sda  = 6;
        cfg.pin_scl  = 5;
        cfg.freq     = 400000;

        _touch_instance->config(cfg);
        _panel_instance->setTouch(_touch_instance);
    }

#elif 0 && defined(LILYGOPI)
    auto _panel_instance = new lgfx::Panel_ST7796();
    auto _bus_instance   = new lgfx::Bus_SPI();
    auto _touch_instance = new lgfx::Touch_FT5x06();
    {
        auto cfg        = _bus_instance->config();
        cfg.spi_host    = VSPI_HOST;
        cfg.spi_mode    = 0;
        cfg.freq_write  = 40000000;
        cfg.freq_read   = 16000000;
        cfg.spi_3wire   = false;
        cfg.use_lock    = true;
        cfg.dma_channel = 1;
        cfg.pin_sclk    = 18;
        cfg.pin_mosi    = 19;
        cfg.pin_miso    = 23;
        cfg.pin_dc      = 27;
        _bus_instance->config(cfg);
        _panel_instance->setBus(_bus_instance);
    }

    {
        auto cfg             = _panel_instance->config();
        cfg.pin_cs           = 5;
        cfg.pin_rst          = -1;
        cfg.pin_busy         = -1;
        cfg.memory_width     = 320;
        cfg.memory_height    = 480;
        cfg.panel_width      = 320;
        cfg.panel_height     = 480;
        cfg.offset_x         = 0;
        cfg.offset_y         = 0;
        cfg.offset_rotation  = 0;
        cfg.dummy_read_pixel = 8;
        cfg.dummy_read_bits  = 1;
        cfg.readable         = true;
        cfg.invert           = false;
        cfg.rgb_order        = false;
        cfg.dlen_16bit       = false;
        cfg.bus_shared       = true;

        _panel_instance->config(cfg);
    }
#elif defined(M5STACKLGFX)

    using namespace m5stack;

    auto _panel_instance = new Panel_M5StackCore2();
    auto _bus_instance   = new lgfx::Bus_SPI();
    // auto _touch_instance = new lgfx::Touch_FT5x06();
    auto _touch_instance = new Touch_M5Tough();

    // AXP192_LDO2 = LCD PWR
    // AXP192_IO4  = LCD RST
    // AXP192_DC3  = LCD BL (Core2)
    // AXP192_LDO3 = LCD BL (Tough)
    // AXP192_IO1  = TP RST (Tough)
    lgfx::i2c::writeRegister8(axp_i2c_port, axp_i2c_addr, 0x28, 0xF0, ~0, axp_i2c_freq); // set LDO2 3300mv // LCD PWR
    lgfx::i2c::writeRegister8(axp_i2c_port, axp_i2c_addr, 0x12, 0x04, ~0, axp_i2c_freq); // LDO2 enable
    lgfx::i2c::writeRegister8(axp_i2c_port, axp_i2c_addr, 0x92, 0x00, 0xF8,
                              axp_i2c_freq); // GPIO1 OpenDrain (M5Tough TOUCH)
    lgfx::i2c::writeRegister8(axp_i2c_port, axp_i2c_addr, 0x95, 0x84, 0x72, axp_i2c_freq); // GPIO4 enable
    if(/*use_reset*/ true) {
        lgfx::i2c::writeRegister8(axp_i2c_port, axp_i2c_addr, 0x96, 0, ~0x02, axp_i2c_freq); // GPIO4 LOW (LCD RST)
        lgfx::i2c::writeRegister8(axp_i2c_port, axp_i2c_addr, 0x94, 0, ~0x02,
                                  axp_i2c_freq); // GPIO1 LOW (M5Tough TOUCH RST)
        lgfx::delay(1);
    }
    lgfx::i2c::writeRegister8(axp_i2c_port, axp_i2c_addr, 0x96, 0x02, ~0, axp_i2c_freq); // GPIO4 HIGH (LCD RST)
    lgfx::i2c::writeRegister8(axp_i2c_port, axp_i2c_addr, 0x94, 0x02, ~0,
                              axp_i2c_freq); // GPIO1 HIGH (M5Tough TOUCH RST)

    {
        auto bus_cfg       = _bus_instance->config();
        bus_cfg.pin_mosi   = GPIO_NUM_23;
        bus_cfg.pin_miso   = GPIO_NUM_38;
        bus_cfg.pin_sclk   = GPIO_NUM_18;
        bus_cfg.pin_dc     = GPIO_NUM_15;
        bus_cfg.spi_3wire  = false;
        bus_cfg.freq_write = 40000000;
        bus_cfg.freq_read  = 16000000;
        _bus_instance->config(bus_cfg);
        _bus_instance->init();
    }
    _panel_instance->bus(_bus_instance);

    {
        auto cfg       = _touch_instance->config();
        cfg.pin_int    = GPIO_NUM_39;       // INT pin number
        cfg.pin_sda    = GPIO_NUM_21;       // I2C SDA pin number
        cfg.pin_scl    = GPIO_NUM_22;       // I2C SCL pin number
        cfg.i2c_addr   = I2C_TOUCH_ADDRESS; // I2C device addr
        cfg.i2c_port   = I2C_NUM_1;         // I2C port number
        cfg.freq       = 400000;            // I2C freq
        cfg.x_min      = 0;
        cfg.x_max      = 319;
        cfg.y_min      = 0;
        cfg.y_max      = 239;
        cfg.bus_shared = false;
        _touch_instance->config(cfg);
    }
    _panel_instance->touch(_touch_instance);
    // float affine[6] = {1, 0, 0, 0, 1, 0};
    //_panel_instance->setCalibrateAffine(affine);
    //_panel_instance->setLight(new Light_M5StackCore2());
    _panel_instance->setLight(new Light_M5Tough());

#elif defined(ESP32_2432S028R)

#if defined(ILI9341_DRIVER)
    auto _panel_instance = new lgfx::Panel_ILI9341();
#elif defined(ILI9342_DRIVER)
    auto _panel_instance = new lgfx::Panel_ILI9342();
#elif defined(ST7789_DRIVER)
    auto _panel_instance = new lgfx::Panel_ST7789();
#endif
    auto _bus_instance   = new lgfx::Bus_SPI();
    auto _touch_instance = new lgfx::Touch_XPT2046();

    {
        auto cfg        = _bus_instance->config();
        cfg.spi_host    = SPI2_HOST;
        cfg.spi_mode    = 0;
        cfg.freq_write  = SPI_FREQUENCY;
        cfg.freq_read   = SPI_READ_FREQUENCY;
        cfg.spi_3wire   = false;
        cfg.use_lock    = true;
        cfg.dma_channel = 1;
        cfg.pin_sclk    = TFT_SCLK;
        cfg.pin_mosi    = TFT_MOSI;
        cfg.pin_miso    = TFT_MISO;
        cfg.pin_dc      = TFT_DC;
        _bus_instance->config(cfg);
        _panel_instance->setBus(_bus_instance);
    }

    {
        auto cfg             = _panel_instance->config();
        cfg.pin_cs           = TFT_CS;
        cfg.pin_rst          = TFT_RST;
        cfg.pin_busy         = TFT_BUSY;
        cfg.memory_width     = TFT_WIDTH;
        cfg.memory_height    = TFT_HEIGHT;
        cfg.panel_width      = TFT_WIDTH;
        cfg.panel_height     = TFT_HEIGHT;
        cfg.offset_x         = 0;
        cfg.offset_y         = 0;
        cfg.offset_rotation  = TFT_ROTATION;
        cfg.dummy_read_pixel = 8;
        cfg.dummy_read_bits  = 1;
        cfg.readable         = true;
        cfg.invert           = INVERT_COLORS;
        cfg.rgb_order        = (TFT_RGB_ORDER != 0); // true if the red and blue of the panel are swapped
        cfg.dlen_16bit       = false;
        cfg.bus_shared       = false;
        _panel_instance->config(cfg);
    }

    {
        auto cfg            = _touch_instance->config();
        cfg.x_min           = 0;
        cfg.x_max           = 4095;
        cfg.y_min           = 4095;
        cfg.y_max           = 0;
        cfg.pin_int         = TOUCH_IRQ;
        cfg.bus_shared      = false;
        cfg.offset_rotation = TOUCH_OFFSET_ROTATION;
        cfg.spi_host        = VSPI_HOST;
        cfg.freq            = SPI_TOUCH_FREQUENCY;
        cfg.pin_sclk        = TOUCH_SCLK;
        cfg.pin_mosi        = TOUCH_MOSI;
        cfg.pin_miso        = TOUCH_MISO;
        cfg.pin_cs          = TOUCH_CS;
        _touch_instance->config(cfg);
        _panel_instance->setTouch(_touch_instance);
    }

#elif defined(TTGO_T_HMI)
    pinMode(PWR_EN, OUTPUT);
    digitalWrite(PWR_EN, HIGH);

    auto _panel_instance = new lgfx::Panel_ST7789();
    auto _bus_instance   = new lgfx::Bus_Parallel8();
    auto _touch_instance = new lgfx::Touch_XPT2046();

    {
        auto cfg       = _bus_instance->config();
        cfg.freq_write = 16000000;
        cfg.pin_wr     = TFT_WR;
        cfg.pin_rd     = TFT_RD;
        cfg.pin_rs     = TFT_DC; // D/C
        cfg.pin_d0     = TFT_D0;
        cfg.pin_d1     = TFT_D1;
        cfg.pin_d2     = TFT_D2;
        cfg.pin_d3     = TFT_D3;
        cfg.pin_d4     = TFT_D4;
        cfg.pin_d5     = TFT_D5;
        cfg.pin_d6     = TFT_D6;
        cfg.pin_d7     = TFT_D7;
        _bus_instance->config(cfg);
        _panel_instance->setBus(_bus_instance);
    }

    {
        auto cfg             = _panel_instance->config();
        cfg.pin_cs           = TFT_CS;
        cfg.pin_rst          = TFT_RST;
        cfg.pin_busy         = TFT_BUSY;
        cfg.memory_width     = TFT_WIDTH;
        cfg.memory_height    = TFT_HEIGHT;
        cfg.panel_width      = TFT_WIDTH;
        cfg.panel_height     = TFT_HEIGHT;
        cfg.offset_x         = 0;
        cfg.offset_y         = 0;
        cfg.offset_rotation  = TFT_ROTATION;
        cfg.dummy_read_pixel = 8;
        cfg.dummy_read_bits  = 1;
        cfg.readable         = true;
        cfg.invert           = false;
        cfg.rgb_order        = false;
        cfg.dlen_16bit       = false;
        cfg.bus_shared       = false;
        _panel_instance->config(cfg);
    }

    {
        auto cfg            = _touch_instance->config();
        cfg.x_min           = 0;
        cfg.x_max           = 4095;
        cfg.y_min           = 4095;
        cfg.y_max           = 0;
        cfg.pin_int         = TOUCH_IRQ;
        cfg.bus_shared      = false;
        cfg.offset_rotation = TOUCH_OFFSET_ROTATION;
        cfg.spi_host        = SPI3_HOST;
        cfg.freq            = SPI_TOUCH_FREQUENCY;
        cfg.pin_sclk        = TOUCH_SCLK;
        cfg.pin_mosi        = TOUCH_MOSI;
        cfg.pin_miso        = TOUCH_MISO;
        cfg.pin_cs          = TOUCH_CS;
        _touch_instance->config(cfg);
        _panel_instance->setTouch(_touch_instance);
    }
#elif defined(ESP32_2432S022C)
    //pinMode(PWR_EN, OUTPUT);
    //digitalWrite(PWR_EN, HIGH);

    auto _panel_instance = new lgfx::Panel_ST7789();
    auto _bus_instance   = new lgfx::Bus_Parallel8();
    auto _touch_instance = new lgfx::Touch_CST816S();    
    {
        auto cfg       = _bus_instance->config();
        cfg.freq_write = 16000000;
        cfg.pin_wr     = TFT_WR;
        cfg.pin_rd     = TFT_RD;
        cfg.pin_rs     = TFT_DC; // D/C
        cfg.pin_d0     = TFT_D0;
        cfg.pin_d1     = TFT_D1;
        cfg.pin_d2     = TFT_D2;
        cfg.pin_d3     = TFT_D3;
        cfg.pin_d4     = TFT_D4;
        cfg.pin_d5     = TFT_D5;
        cfg.pin_d6     = TFT_D6;
        cfg.pin_d7     = TFT_D7;
        _bus_instance->config(cfg);
        _panel_instance->setBus(_bus_instance);
    }

    {
        auto cfg             = _panel_instance->config();
        cfg.pin_cs           = TFT_CS;
        cfg.pin_rst          = TFT_RST;
        cfg.pin_busy         = TFT_BUSY;
        cfg.memory_width     = TFT_WIDTH;
        cfg.memory_height    = TFT_HEIGHT;
        cfg.panel_width      = TFT_WIDTH;
        cfg.panel_height     = TFT_HEIGHT;
        cfg.offset_x         = 0;
        cfg.offset_y         = 0;
        cfg.offset_rotation  = TFT_ROTATION;
        cfg.dummy_read_pixel = 8;
        cfg.dummy_read_bits  = 1;
        cfg.readable         = true;
        cfg.invert           = false;
        cfg.rgb_order        = false;
        cfg.dlen_16bit       = false;
        cfg.bus_shared       = false;
        _panel_instance->config(cfg);
    }

    {
        auto cfg = _touch_instance->config();

        cfg.x_min           = 0;
        cfg.x_max           = TFT_WIDTH;
        cfg.y_min           = 0;
        cfg.y_max           = TFT_HEIGHT;
        cfg.pin_int         = TOUCH_IRQ;
        cfg.bus_shared      = true;
        cfg.offset_rotation = 0;

        cfg.i2c_port = I2C_TOUCH_PORT;
        cfg.i2c_addr = I2C_TOUCH_ADDRESS;
        cfg.pin_sda  = TOUCH_SDA;
        cfg.pin_scl  = TOUCH_SCL;
        cfg.freq     = I2C_TOUCH_FREQUENCY;

        _touch_instance->config(cfg);
        _panel_instance->setTouch(_touch_instance);
    }
#elif 0
    auto _panel_instance = new lgfx::Panel_ST7796();
    auto _bus_instance   = new lgfx::Bus_SPI();
    auto _touch_instance = new lgfx::Touch_FT5x06();
    {
        auto cfg        = _bus_instance->config();
        cfg.spi_host    = VSPI_HOST;
        cfg.spi_mode    = 0;
        cfg.freq_write  = 40000000;
        cfg.freq_read   = 16000000;
        cfg.spi_3wire   = false;
        cfg.use_lock    = true;
        cfg.dma_channel = 1;
        cfg.pin_sclk    = 18;
        cfg.pin_mosi    = 19;
        cfg.pin_miso    = 23;
        cfg.pin_dc      = 27;
        _bus_instance->config(cfg);
        _panel_instance->setBus(_bus_instance);
    }

    {
        auto cfg             = _panel_instance->config();
        cfg.pin_cs           = 5;
        cfg.pin_rst          = -1;
        cfg.pin_busy         = -1;
        cfg.memory_width     = 320;
        cfg.memory_height    = 480;
        cfg.panel_width      = 320;
        cfg.panel_height     = 480;
        cfg.offset_x         = 0;
        cfg.offset_y         = 0;
        cfg.offset_rotation  = 0;
        cfg.dummy_read_pixel = 8;
        cfg.dummy_read_bits  = 1;
        cfg.readable         = true;
        cfg.invert           = false;
        cfg.rgb_order        = false;
        cfg.dlen_16bit       = false;
        cfg.bus_shared       = true;

        _panel_instance->config(cfg);
    }
#elif defined(MKFBS_TFT_S3_40_RGB)
    auto _panel_instance = new lgfx::Panel_RGB();
    auto _bus_instance   = new lgfx::Bus_RGB();
    auto _touch_instance = new lgfx::Touch_GT911();

#elif defined(MKFBS_TFT_S3_43_RGB)
    auto _panel_instance = new lgfx::Panel_RGB();
    auto _bus_instance   = new lgfx::Bus_RGB();
    auto _touch_instance = new lgfx::Touch_GT911();
    auto _light_instance = new lgfx::Light_PWM();

    {
        auto cfg          = _panel_instance->config();
        cfg.memory_width  = TFT_WIDTH;
        cfg.memory_height = TFT_HEIGHT;
        cfg.panel_width   = TFT_WIDTH;
        cfg.panel_height  = TFT_HEIGHT;

        cfg.offset_x = 0;
        cfg.offset_y = 0;
        _panel_instance->config(cfg);
    }

    {
        auto cfg      = _panel_instance->config_detail();
        cfg.use_psram = 1;
        _panel_instance->config_detail(cfg);
    }

    {
        auto cfg    = _bus_instance->config();
        cfg.panel   = _panel_instance;
        cfg.pin_d0  = GPIO_NUM_8;  // B0
        cfg.pin_d1  = GPIO_NUM_3;  // B1
        cfg.pin_d2  = GPIO_NUM_46; // B2
        cfg.pin_d3  = GPIO_NUM_9;  // B3
        cfg.pin_d4  = GPIO_NUM_1;  // B4
        cfg.pin_d5  = GPIO_NUM_5;  // G0
        cfg.pin_d6  = GPIO_NUM_6;  // G1
        cfg.pin_d7  = GPIO_NUM_7;  // G2
        cfg.pin_d8  = GPIO_NUM_15; // G3
        cfg.pin_d9  = GPIO_NUM_16; // G4
        cfg.pin_d10 = GPIO_NUM_4;  // G5
        cfg.pin_d11 = GPIO_NUM_45; // R0
        cfg.pin_d12 = GPIO_NUM_48; // R1
        cfg.pin_d13 = GPIO_NUM_47; // R2
        cfg.pin_d14 = GPIO_NUM_21; // R3
        cfg.pin_d15 = GPIO_NUM_14; // R4

        cfg.pin_henable = GPIO_NUM_40;
        cfg.pin_vsync   = GPIO_NUM_41;
        cfg.pin_hsync   = GPIO_NUM_39;
        cfg.pin_pclk    = GPIO_NUM_42;
        cfg.freq_write  = 14000000;

        cfg.hsync_polarity    = 0;
        cfg.hsync_front_porch = 8;
        cfg.hsync_pulse_width = 4;
        cfg.hsync_back_porch  = 16;
        cfg.vsync_polarity    = 0;
        cfg.vsync_front_porch = 4;
        cfg.vsync_pulse_width = 4;
        cfg.vsync_back_porch  = 4;
        cfg.pclk_idle_high    = 1;
        _bus_instance->config(cfg);
        _panel_instance->setBus(_bus_instance);
    }

    {
        auto cfg            = _touch_instance->config();
        cfg.x_min           = 0;
        cfg.y_min           = 0;
        cfg.bus_shared      = false;
        cfg.offset_rotation = 0;
        // I2C接続
        cfg.i2c_port = I2C_NUM_1;
        cfg.pin_sda  = GPIO_NUM_17;
        cfg.pin_scl  = GPIO_NUM_18;
        cfg.freq     = 400000;

        // for Board v1.3
        cfg.pin_int = GPIO_NUM_NC;
        cfg.pin_rst = GPIO_NUM_38;
        cfg.x_max   = 800;
        cfg.y_max   = 480;

        _touch_instance->config(cfg);
        _panel_instance->setTouch(_touch_instance);
    }

    {
        auto cfg   = _light_instance->config();
        cfg.pin_bl = GPIO_NUM_2;
        _light_instance->config(cfg);
        _panel_instance->setLight(_light_instance);
    }

#elif defined(ESP32_8040S070C) || defined(RGB_DRIVER)
    auto _panel_instance = new lgfx::Panel_RGB();
    auto _bus_instance   = new lgfx::Bus_RGB();
    auto _touch_instance = new lgfx::Touch_GT911();

    {
        auto cfg          = _panel_instance->config();
        cfg.memory_width  = TFT_WIDTH;
        cfg.memory_height = TFT_HEIGHT;
        cfg.panel_width   = TFT_WIDTH;
        cfg.panel_height  = TFT_HEIGHT;

        cfg.offset_x = 0;
        cfg.offset_y = 0;
        _panel_instance->config(cfg);
    }

    {
        auto cfg      = _panel_instance->config_detail();
        cfg.use_psram = 1;
        _panel_instance->config_detail(cfg);
    }

    {
        auto cfg    = _bus_instance->config();
        cfg.panel   = _panel_instance;
        cfg.pin_d0  = TFT_B0; // B0
        cfg.pin_d1  = TFT_B1; // B1
        cfg.pin_d2  = TFT_B2; // B2
        cfg.pin_d3  = TFT_B3; // B3
        cfg.pin_d4  = TFT_B4; // B4
        cfg.pin_d5  = TFT_G0; // G0
        cfg.pin_d6  = TFT_G1; // G1
        cfg.pin_d7  = TFT_G2; // G2
        cfg.pin_d8  = TFT_G3; // G3
        cfg.pin_d9  = TFT_G4; // G4
        cfg.pin_d10 = TFT_G5; // G5
        cfg.pin_d11 = TFT_R0; // R0
        cfg.pin_d12 = TFT_R1; // R1
        cfg.pin_d13 = TFT_R2; // R2
        cfg.pin_d14 = TFT_R3; // R3
        cfg.pin_d15 = TFT_R4; // R4

        cfg.pin_henable = TFT_DE;
        cfg.pin_vsync   = TFT_VSYNC;
        cfg.pin_hsync   = TFT_HSYNC;
        cfg.pin_pclk    = TFT_PCLK;
        cfg.freq_write  = TFT_PREFER_SPEED;

        cfg.hsync_polarity    = TFT_HSYNC_POLARITY;
        cfg.hsync_front_porch = TFT_HSYNC_FRONT_PORCH;
        cfg.hsync_pulse_width = TFT_HSYNC_PULSE_WIDTH;
        cfg.hsync_back_porch  = TFT_HSYNC_BACK_PORCH;
        cfg.vsync_polarity    = TFT_VSYNC_POLARITY;
        cfg.vsync_front_porch = TFT_VSYNC_FRONT_PORCH;
        cfg.vsync_pulse_width = TFT_VSYNC_PULSE_WIDTH;
        cfg.vsync_back_porch  = TFT_VSYNC_BACK_PORCH;
        cfg.pclk_idle_high    = 1;
        _bus_instance->config(cfg);
        _panel_instance->setBus(_bus_instance);
    }

    {
        auto cfg            = _touch_instance->config();
        cfg.x_min           = 0;
        cfg.x_max           = TFT_WIDTH;
        cfg.y_min           = 0;
        cfg.y_max           = TFT_HEIGHT;
        cfg.pin_int         = TOUCH_IRQ;
        cfg.bus_shared      = false;
        cfg.offset_rotation = TOUCH_OFFSET_ROTATION;
        cfg.i2c_port        = I2C_TOUCH_PORT;
        cfg.pin_sda         = TOUCH_SDA;
        cfg.pin_scl         = TOUCH_SCL;
        cfg.freq            = 400000;
        cfg.i2c_addr        = 0x14; // 0x5D , 0x14
        _touch_instance->config(cfg);
        _panel_instance->setTouch(_touch_instance);
    }
#else

    Preferences preferences;
    nvs_user_begin(preferences, "tft", false);
    this->tft_driver = preferences.getUInt("DRIVER", get_tft_driver());

    lgfx::IBus* _bus_instance           = _init_bus(&preferences);
    lgfx::Panel_Device* _panel_instance = _init_panel(_bus_instance);
    lgfx::ITouch* _touch_instance       = _init_touch(&preferences);

    if(_panel_instance != nullptr) {
        _panel_instance->setBus(_bus_instance);
        configure_panel(_panel_instance, &preferences);
        _panel_instance->setTouch(_touch_instance);
    }
    preferences.end();
#endif

    tft.setPanel(_panel_instance);

    if(_touch_instance != nullptr) {
        LOG_INFO(TAG_TFT, F("Touch " D_SERVICE_STARTED));
    } else {
        LOG_WARNING(TAG_TFT, F("Touch " D_SERVICE_START_FAILED));
    }

    /* TFT init */
    LOG_DEBUG(TAG_TFT, F("%s - %d"), __FILE__, __LINE__);
    tft.begin();
    LOG_DEBUG(TAG_TFT, F("%s - %d"), __FILE__, __LINE__);
    tft.setSwapBytes(true); /* set endianness */

    LOG_INFO(TAG_TFT, F(D_SERVICE_STARTED));
}

void LovyanGfx::show_info()
{
    splashscreen();

    LOG_VERBOSE(TAG_TFT, F("LovyanGFX   : v%d.%d.%d"), LGFX_VERSION_MAJOR, LGFX_VERSION_MINOR, LGFX_VERSION_PATCH);
    auto panel = tft.getPanel();
    if(!panel) return;

    if(!panel->getBus()) return;
    lgfx::v1::bus_type_t bus_type = panel->getBus()->busType();

    if(bus_type == lgfx::v1::bus_parallel8) {
        LOG_VERBOSE(TAG_TFT, F("Interface  : Parallel 8bit"));
        auto bus = (lgfx::Bus_Parallel8*)panel->getBus();
        auto cfg = bus->config(); // Get the structure for bus configuration.
        tftPinInfo(F("TFT_WR"), cfg.pin_wr);
        tftPinInfo(F("TFT_RD"), cfg.pin_rd);
        tftPinInfo(F("TFT_RS"), cfg.pin_rs);

        tftPinInfo(F("TFT_D0"), cfg.pin_d0);
        tftPinInfo(F("TFT_D1"), cfg.pin_d1);
        tftPinInfo(F("TFT_D2"), cfg.pin_d2);
        tftPinInfo(F("TFT_D3"), cfg.pin_d3);
        tftPinInfo(F("TFT_D4"), cfg.pin_d4);
        tftPinInfo(F("TFT_D5"), cfg.pin_d5);
        tftPinInfo(F("TFT_D6"), cfg.pin_d6);
        tftPinInfo(F("TFT_D7"), cfg.pin_d7);
    }

#if defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3)
    if(bus_type == lgfx::v1::bus_parallel16) {
        LOG_VERBOSE(TAG_TFT, F("Interface  : Parallel 16bit"));
        auto bus = (lgfx::v1::Bus_Parallel16*)panel->getBus();
        auto cfg = bus->config(); // Get the structure for bus configuration.
        tftPinInfo(F("TFT_WR"), cfg.pin_wr);
        tftPinInfo(F("TFT_RD"), cfg.pin_rd);
        tftPinInfo(F("TFT_RS"), cfg.pin_rs);

        tftPinInfo(F("TFT_D0"), cfg.pin_d0);
        tftPinInfo(F("TFT_D1"), cfg.pin_d1);
        tftPinInfo(F("TFT_D2"), cfg.pin_d2);
        tftPinInfo(F("TFT_D3"), cfg.pin_d3);
        tftPinInfo(F("TFT_D4"), cfg.pin_d4);
        tftPinInfo(F("TFT_D5"), cfg.pin_d5);
        tftPinInfo(F("TFT_D6"), cfg.pin_d6);
        tftPinInfo(F("TFT_D7"), cfg.pin_d7);
        tftPinInfo(F("TFT_D8"), cfg.pin_d8);
        tftPinInfo(F("TFT_D9"), cfg.pin_d9);
        tftPinInfo(F("TFT_D01"), cfg.pin_d10);
        tftPinInfo(F("TFT_D11"), cfg.pin_d11);
        tftPinInfo(F("TFT_D12"), cfg.pin_d12);
        tftPinInfo(F("TFT_D13"), cfg.pin_d13);
        tftPinInfo(F("TFT_D14"), cfg.pin_d14);
        tftPinInfo(F("TFT_D15"), cfg.pin_d15);
    }
#endif

    if(bus_type == lgfx::v1::bus_spi) {
        LOG_VERBOSE(TAG_TFT, F("Interface   : Serial"));
        auto bus = (lgfx::Bus_SPI*)panel->getBus();
        auto cfg = bus->config(); // Get the structure for bus configuration.
        tftPinInfo(F("TFT_MOSI"), cfg.pin_mosi);
        tftPinInfo(F("TFT_MISO"), cfg.pin_miso);
        tftPinInfo(F("TFT_SCLK"), cfg.pin_sclk);
        tftPinInfo(F("TFT_DC"), cfg.pin_dc);
    }

    {
        auto cfg = panel->config(); // Get the structure for panel configuration.
        tftPinInfo(F("TFT_CS"), cfg.pin_cs);
        tftPinInfo(F("TFT_RST"), cfg.pin_rst);
        tftPinInfo(F("TFT_BUSY"), cfg.pin_busy);
    }

    if(bus_type == lgfx::v1::bus_spi) {
        auto bus      = (lgfx::Bus_SPI*)panel->getBus();
        auto cfg      = bus->config(); // Get the structure for bus configuration.
        uint32_t freq = cfg.freq_write / 100000;
        LOG_VERBOSE(TAG_TFT, F("TFT SPI freq: %d.%d MHz"), freq / 10, freq % 10);
    }

    lgfx::v1::ITouch* touch = panel->getTouch();
    if(touch) {
        auto cfg      = touch->config(); // Get the structure for bus configuration.
        uint32_t freq = cfg.freq / 100000;
        tftPinInfo(F("TOUCH_INT"), cfg.pin_int);
        tftPinInfo(F("TOUCH_RST"), cfg.pin_rst);
        if(touch->isSPI()) {
            tftPinInfo(F("TOUCH_MISO"), cfg.pin_miso);
            tftPinInfo(F("TOUCH_MOSI"), cfg.pin_mosi);
            tftPinInfo(F("TOUCH_SCLK"), cfg.pin_sclk);
            tftPinInfo(F("TOUCH_CS"), cfg.pin_cs);
            LOG_VERBOSE(TAG_TFT, F("Touch SPI freq.   : %d.%d MHz"), freq / 10, freq % 10);
        } else {
            tftPinInfo(F("TOUCH_SDA"), cfg.pin_sda);
            tftPinInfo(F("TOUCH_SCL"), cfg.pin_scl);
            tftPinInfo(F("TOUCH_ADDR"), cfg.i2c_addr);
            LOG_VERBOSE(TAG_TFT, F("Touch I2C freq.   : %d.%d MHz"), freq / 10, freq % 10);
        }
    }
}

void LovyanGfx::splashscreen()
{
    uint8_t fg[]       = logoFgColor;
    uint8_t bg[]       = logoBgColor;
    lv_color_t fgColor = lv_color_make(fg[0], fg[1], fg[2]);
    lv_color_t bgColor = lv_color_make(bg[0], bg[1], bg[2]);

    tft.fillScreen(bgColor.full);
    int x = (tft.width() - logoWidth) / 2;
    int y = (tft.height() - logoHeight) / 2;
    tft.drawXBitmap(x, y, logoImage, logoWidth, logoHeight, fgColor.full);
    // tft.fillSmoothRoundRect(x, y, logoWidth, logoWidth, 15, fgColor.full);
}

void LovyanGfx::set_rotation(uint8_t rotation)
{
    LOG_VERBOSE(TAG_TFT, F("Rotation    : %d"), rotation);
    tft.setRotation(rotation);
}

void LovyanGfx::set_invert(bool invert)
{
    char buffer[4];
    memcpy_P(buffer, invert ? PSTR(D_YES) : PSTR(D_NO), sizeof(buffer));

    LOG_VERBOSE(TAG_TFT, F("Invert Color: %s"), buffer);
    tft.invertDisplay(invert);
}

/* Update TFT */
void IRAM_ATTR LovyanGfx::flush_pixels(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* color_p)
{
    uint32_t w   = (area->x2 - area->x1 + 1);
    uint32_t h   = (area->y2 - area->y1 + 1);
    uint32_t len = w * h;

    tft.startWrite();                                        /* Start new TFT transaction */
    tft.setAddrWindow(area->x1, area->y1, w, h);             /* set the working window */
    tft.writePixels((lgfx::rgb565_t*)&color_p->full, w * h); /* Write words at once */
    tft.endWrite();                                          /* terminate TFT transaction */

    /* Tell lvgl that flushing is done */
    lv_disp_flush_ready(disp);
}

bool LovyanGfx::is_driver_pin(uint8_t pin)
{
    auto panel = tft.getPanel();
    if(panel && panel->getBus()) {
        lgfx::v1::bus_type_t bus_type = panel->getBus()->busType();

        if(bus_type == lgfx::v1::bus_parallel8) {
            auto bus = (lgfx::Bus_Parallel8*)panel->getBus();
            auto cfg = bus->config(); // Get the structure for bus configuration.
            if(pin == cfg.pin_wr || pin == cfg.pin_rd || pin == cfg.pin_rs || pin == cfg.pin_d0 || pin == cfg.pin_d1 ||
               pin == cfg.pin_d2 || pin == cfg.pin_d3 || pin == cfg.pin_d4 || pin == cfg.pin_d5 || pin == cfg.pin_d6 ||
               pin == cfg.pin_d7)
                return true;

#if defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3)
        } else if(bus_type == lgfx::v1::bus_parallel16) {
            auto bus = (lgfx::v1::Bus_Parallel16*)panel->getBus();
            auto cfg = bus->config(); // Get the structure for bus configuration.
            if(cfg.pin_wr || pin == cfg.pin_rd || pin == cfg.pin_rs || pin == cfg.pin_d0 || pin == cfg.pin_d1 ||
               pin == cfg.pin_d2 || pin == cfg.pin_d3 || pin == cfg.pin_d4 || pin == cfg.pin_d5 || pin == cfg.pin_d6 ||
               pin == cfg.pin_d7 || pin == cfg.pin_d8 || pin == cfg.pin_d9 || pin == cfg.pin_d10 ||
               pin == cfg.pin_d11 || pin == cfg.pin_d12 || pin == cfg.pin_d13 || pin == cfg.pin_d14 ||
               pin == cfg.pin_d15)
                return true;
#endif

        } else if(bus_type == lgfx::v1::bus_spi) {
            auto bus = (lgfx::Bus_SPI*)panel->getBus();
            auto cfg = bus->config(); // Get the structure for bus configuration.
            if(pin == cfg.pin_mosi || pin == cfg.pin_miso || pin == cfg.pin_sclk || pin == cfg.pin_dc) return true;
        }

        {
            auto cfg = panel->config(); // Get the structure for panel configuration.
            if(pin == cfg.pin_cs || pin == cfg.pin_rst || pin == cfg.pin_busy) return true;
        }

        lgfx::v1::ITouch* touch = panel->getTouch();
        if(touch) {
            auto cfg = touch->config(); // Get the structure for bus configuration.
            if(pin == cfg.pin_int || pin == cfg.pin_rst) return true;
            if(touch->isSPI()) {
                if(pin == cfg.pin_miso || pin == cfg.pin_mosi || pin == cfg.pin_sclk || pin == cfg.pin_cs) return true;
            } else {
                if(pin == cfg.pin_sda || pin == cfg.pin_scl) return true;
            }
        }
    }

    return false;
}

const char* LovyanGfx::get_tft_model()
{
#if defined(ILI9341_DRIVER)
    return "ILI9341";
#elif defined(ILI9342_DRIVER)
    return "ILI9342";
#elif defined(ST7735_DRIVER)
    return "ST7735";
#elif defined(ILI9163_DRIVER)
    return "ILI9163";
#elif defined(S6D02A1_DRIVER)
    return "S6D02A1";
#elif defined(ST7796_DRIVER)
    return "ST7796";
#elif defined(ILI9486_DRIVER)
    return "ILI9486";
#elif defined(ILI9481_DRIVER)
    return "ILI9481";
#elif defined(ILI9488_DRIVER)
    return "ILI9488";
#elif defined(HX8357D_DRIVER)
    return "HX8357D";
#elif defined(EPD_DRIVER)
    return "EPD";
#elif defined(ST7789_DRIVER)
    return "ST7789";
#elif defined(R61581_DRIVER)
    return "R61581";
#elif defined(ST7789_2_DRIVER)
    return "ST7789_2";
#elif defined(R61529_DRIVER)
    return "R61529";
#elif defined(RM68140_DRIVER)
    return "RM68140";
#elif defined(GC9A01_DRIVER)
    return "GC9A01";
#else
    return "Other";
#endif
}

uint32_t LovyanGfx::get_tft_driver()
{
#if defined(ILI9341_DRIVER)
    return TFT_PANEL_ILI9341;
#elif defined(ILI9342_DRIVER)
    return TFT_PANEL_ILI9342;
#elif defined(ILI9163_DRIVER)
    return TFT_PANEL_ILI9163;
#elif defined(ILI9486_DRIVER)
    return TFT_PANEL_ILI9486;
#elif defined(ILI9481_DRIVER)
    return TFT_PANEL_ILI9481;
#elif defined(ILI9488_DRIVER)
    return TFT_PANEL_ILI9488;
#elif defined(HX8357D_DRIVER)
    return TFT_PANEL_HX8357D;
#elif defined(ST7735_DRIVER)
    return TFT_PANEL_ST7735;
#elif defined(ST7789_DRIVER)
    return TFT_PANEL_ST7789;
#elif defined(ST7789_2_DRIVER)
    return TFT_PANEL_ST7789B;
#elif defined(ST7796_DRIVER)
    return TFT_PANEL_ST7796;
#elif defined(S6D02A1_DRIVER)
    return TFT_PANEL_S6D02A1;
#elif defined(R61581_DRIVER)
    return TFT_PANEL_R61581;
#elif defined(R61529_DRIVER)
    return TFT_PANEL_R61529;
#elif defined(RM68140_DRIVER)
    return TFT_PANEL_RM68140;
#elif defined(EPD_DRIVER)
    return TFT_PANEL_EPD;
#elif defined(RGB_DRIVER)
    return TFT_PANEL_RGB;
#elif defined(GC9A01_DRIVER)
    return TFT_PANEL_GC9A01;
#else
    return TFT_PANEL_UNKNOWN;
#endif
}

uint32_t LovyanGfx::get_touch_driver()
{
#ifdef TOUCH_DRIVER
    return TOUCH_DRIVER > 0 ? TOUCH_DRIVER : 0;
#else
    return 0;
#endif
}

} // namespace dev

dev::LovyanGfx haspTft;
#endif