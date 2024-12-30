/* MIT License - Copyright (c) 2019-2024 Francis Van Roie
   For full license information read the LICENSE file in the project folder */

#ifndef HASP_PARSER_H
#define HASP_PARSER_H

#include "hasplib.h"

class Parser {

  public:
    static uint8_t haspPayloadToPageid(const char* payload);
    static void ColorToHaspPayload(lv_color_t color, char* payload, size_t len);
    static bool haspPayloadToColor(const char* payload, lv_color32_t& color);
    static bool get_event_state(uint8_t eventid);
    static void get_event_name(uint8_t eventid, char* buffer, size_t size);
    static uint8_t get_action_id(const char* action);
    static uint16_t get_sdbm(const char* str, uint16_t len = UINT16_MAX, bool digits=false);
    static bool is_true(const char* s);
    static bool is_true(JsonVariant json);
    static bool is_only_digits(const char* s);
    static int format_bytes(uint64_t filesize, char* buf, size_t len);
};

#ifndef ARDUINO
long map(long x, long in_min, long in_max, long out_min, long out_max);
#endif

/* Named COLOR attributes */
#define ATTR_RED 177
#define ATTR_TAN 7873
#define ATTR_AQUA 3452
#define ATTR_BLUE 37050
#define ATTR_CYAN 9763
#define ATTR_GOLD 53440
#define ATTR_GRAY 64675
#define ATTR_GREY 64927
#define ATTR_LIME 34741
#define ATTR_NAVY 44918
#define ATTR_PERU 36344
#define ATTR_PINK 51958
#define ATTR_PLUM 64308
#define ATTR_SNOW 35587
#define ATTR_TEAL 52412
#define ATTR_AZURE 44239
#define ATTR_BEIGE 12132
#define ATTR_BLACK 26527
#define ATTR_BLUSH 41376
#define ATTR_BROWN 10774
#define ATTR_CORAL 16369
#define ATTR_GREEN 26019
#define ATTR_IVORY 1257
#define ATTR_KHAKI 32162
#define ATTR_LINEN 30074
#define ATTR_OLIVE 47963
#define ATTR_WHEAT 11591
#define ATTR_WHITE 28649
#define ATTR_BISQUE 60533
#define ATTR_INDIGO 46482
#define ATTR_MAROON 12528
#define ATTR_ORANGE 21582
#define ATTR_ORCHID 39235
#define ATTR_PURPLE 53116
#define ATTR_SALMON 29934
#define ATTR_SIENNA 50930
#define ATTR_SILVER 62989
#define ATTR_TOMATO 8234
#define ATTR_VIOLET 61695
#define ATTR_YELLOW 10484
#define ATTR_FUCHSIA 5463
#define ATTR_MAGENTA 49385

struct hasp_color_t
{
    uint16_t hash;
    uint8_t r, g, b;
};

/* Named COLOR lookup table */
const hasp_color_t haspNamedColors[] PROGMEM = {
    {ATTR_RED, 0xFF, 0x00, 0x00},    {ATTR_TAN, 0xD2, 0xB4, 0x8C},     {ATTR_AQUA, 0x00, 0xFF, 0xFF},
    {ATTR_BLUE, 0x00, 0x00, 0xFF},   {ATTR_CYAN, 0x00, 0xFF, 0xFF},    {ATTR_GOLD, 0xFF, 0xD7, 0x00},
    {ATTR_GRAY, 0x80, 0x80, 0x80},   {ATTR_GREY, 0x80, 0x80, 0x80},    {ATTR_LIME, 0x00, 0xFF, 0x00},
    {ATTR_NAVY, 0x00, 0x00, 0x80},   {ATTR_PERU, 0xCD, 0x85, 0x3F},    {ATTR_PINK, 0xFF, 0xC0, 0xCB},
    {ATTR_PLUM, 0xDD, 0xA0, 0xDD},   {ATTR_SNOW, 0xFF, 0xFA, 0xFA},    {ATTR_TEAL, 0x00, 0x80, 0x80},
    {ATTR_AZURE, 0xF0, 0xFF, 0xFF},  {ATTR_BEIGE, 0xF5, 0xF5, 0xDC},   {ATTR_BLACK, 0x00, 0x00, 0x00},
    {ATTR_BLUSH, 0xB0, 0x00, 0x00},  {ATTR_BROWN, 0xA5, 0x2A, 0x2A},   {ATTR_CORAL, 0xFF, 0x7F, 0x50},
    {ATTR_GREEN, 0x00, 0x80, 0x00},  {ATTR_IVORY, 0xFF, 0xFF, 0xF0},   {ATTR_KHAKI, 0xF0, 0xE6, 0x8C},
    {ATTR_LINEN, 0xFA, 0xF0, 0xE6},  {ATTR_OLIVE, 0x80, 0x80, 0x00},   {ATTR_WHEAT, 0xF5, 0xDE, 0xB3},
    {ATTR_WHITE, 0xFF, 0xFF, 0xFF},  {ATTR_BISQUE, 0xFF, 0xE4, 0xC4},  {ATTR_INDIGO, 0x4B, 0x00, 0x82},
    {ATTR_MAROON, 0x80, 0x00, 0x00}, {ATTR_ORANGE, 0xFF, 0xA5, 0x00},  {ATTR_ORCHID, 0xDA, 0x70, 0xD6},
    {ATTR_PURPLE, 0x80, 0x00, 0x80}, {ATTR_SALMON, 0xFA, 0x80, 0x72},  {ATTR_SIENNA, 0xA0, 0x52, 0x2D},
    {ATTR_SILVER, 0xC0, 0xC0, 0xC0}, {ATTR_TOMATO, 0xFF, 0x63, 0x47},  {ATTR_VIOLET, 0xEE, 0x82, 0xEE},
    {ATTR_YELLOW, 0xFF, 0xFF, 0x00}, {ATTR_FUCHSIA, 0xFF, 0x00, 0xFF}, {ATTR_MAGENTA, 0xFF, 0x00, 0xFF},
};

#endif