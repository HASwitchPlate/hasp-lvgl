/* MIT License - Copyright (c) 2019-2024 Francis Van Roie
   For full license information read the LICENSE file in the project folder */

#ifdef ARDUINO
#include "pgmspace.h"
#include <Arduino.h>
#endif

#include "hasplib.h"

void Parser::ColorToHaspPayload(lv_color_t color, char* payload, size_t size)
{
    lv_color32_t c32;
    c32.full = lv_color_to32(color);
    snprintf_P(payload, size, PSTR("#%02x%02x%02x"), c32.ch.red, c32.ch.green, c32.ch.blue);
}

bool Parser::haspPayloadToColor(const char* payload, lv_color32_t& color)
{
    if(!payload) return false;

    /* HEX format #rrggbb or #rgb */
    if(*payload == '#') {
        if(strlen(payload) >= 8) return false;

        char* pEnd;
        long color_int = strtol(payload + 1, &pEnd, HEX);

        if(pEnd - payload == 7) { // #rrbbgg
            color.ch.red   = color_int >> 16 & 0xff;
            color.ch.green = color_int >> 8 & 0xff;
            color.ch.blue  = color_int & 0xff;

        } else if(pEnd - payload == 4) { // #rgb
            color.ch.red   = color_int >> 8 & 0xf;
            color.ch.green = color_int >> 4 & 0xf;
            color.ch.blue  = color_int & 0xf;

            color.ch.red += color.ch.red * HEX;
            color.ch.green += color.ch.green * HEX;
            color.ch.blue += color.ch.blue * HEX;

        } else {
            return false; /* Invalid hex length */
        }

        return true; /* Color found */
    }

    /* 16-bit RGB565 Color Scheme*/
    if(Parser::is_only_digits(payload)) {
        uint16_t c = atoi(payload);

        /* Initial colors */
        uint8_t R5 = ((c >> 11) & 0b11111);
        uint8_t G6 = ((c >> 5) & 0b111111);
        uint8_t B5 = (c & 0b11111);

        /* Remapped colors */
        color.ch.red   = (R5 * 527 + 23) >> 6;
        color.ch.green = (G6 * 259 + 33) >> 6;
        color.ch.blue  = (B5 * 527 + 23) >> 6;

        return true; /* Color found */
    }

    /* Named colors */
    size_t numColors = sizeof(haspNamedColors) / sizeof(haspNamedColors[0]);
    uint16_t sdbm    = Parser::get_sdbm(payload);

#ifdef ARDUINO
    for(size_t i = 0; i < numColors; i++) {
        if(sdbm == (uint16_t)pgm_read_word_near(&(haspNamedColors[i].hash))) {
            color.ch.red   = (uint16_t)pgm_read_byte_near(&(haspNamedColors[i].r));
            color.ch.green = (uint16_t)pgm_read_byte_near(&(haspNamedColors[i].g));
            color.ch.blue  = (uint16_t)pgm_read_byte_near(&(haspNamedColors[i].b));

            return true; /* Color found */
        }
    }
#else
    for(size_t i = 0; i < numColors; i++) {
        if(sdbm == haspNamedColors[i].hash) {
            color.ch.red   = haspNamedColors[i].r;
            color.ch.green = haspNamedColors[i].g;
            color.ch.blue  = haspNamedColors[i].b;

            return true; /* Color found */
        }
    }
#endif

    return false; /* Color not found */
}

uint8_t Parser::haspPayloadToPageid(const char* payload)
{
    if(!payload || strlen(payload) == 0) return 0;

    uint8_t pageid       = 0;
    uint8_t current_page = haspPages.get();

    if(!strcasecmp_P(payload, PSTR("prev"))) {
        pageid = haspPages.get_prev(current_page);
    } else if(!strcasecmp_P(payload, PSTR("next"))) {
        pageid = haspPages.get_next(current_page);
    } else if(!strcasecmp_P(payload, PSTR("back"))) {
        pageid = haspPages.get_back(current_page);
    } else if(is_only_digits(payload)) {
        pageid = atoi(payload);
    }
    return pageid;
}

// Map events to either ON or OFF (UP or DOWN)
bool Parser::get_event_state(uint8_t eventid)
{
    switch(eventid) {
        case HASP_EVENT_ON:
        case HASP_EVENT_DOWN:
        case HASP_EVENT_LONG:
        case HASP_EVENT_HOLD:
            return true;
        // case HASP_EVENT_OFF:
        // case HASP_EVENT_UP:
        // case HASP_EVENT_SHORT:
        // case HASP_EVENT_DOUBLE:
        // case HASP_EVENT_LOST:
        default:
            return false;
    }
}

// Map events to their description string
void Parser::get_event_name(uint8_t eventid, char* buffer, size_t size)
{
    switch(eventid) {
        case HASP_EVENT_ON:
            memcpy_P(buffer, PSTR("on"), 3);
            break;
        case HASP_EVENT_OFF:
            memcpy_P(buffer, PSTR("off"), 4);
            break;
        case HASP_EVENT_UP:
            memcpy_P(buffer, PSTR("up"), 3);
            break;
        case HASP_EVENT_DOWN:
            memcpy_P(buffer, PSTR("down"), 5);
            break;
        case HASP_EVENT_RELEASE:
            memcpy_P(buffer, PSTR("release"), 8);
            break;
        case HASP_EVENT_LONG:
            memcpy_P(buffer, PSTR("long"), 5);
            break;
        case HASP_EVENT_HOLD:
            memcpy_P(buffer, PSTR("hold"), 5);
            break;
        case HASP_EVENT_LOST:
            memcpy_P(buffer, PSTR("lost"), 5);
            break;
        case HASP_EVENT_CHANGED:
            memcpy_P(buffer, PSTR("changed"), 8);
            break;
        default:
            memcpy_P(buffer, PSTR("unknown"), 8);
    }
}

/** ***************************************************************************
 * @brief 16-bit hashing function http://www.cse.yorku.ca/~oz/hash.html
 * all possible attributes are hashed and checked if they are unique
 *
 * To calculate the attribute hashes, digits must be "false", otherwise
 * attributes such as "bg_color01" will not work. I think for aliases
 * digits is better "true", so that lamp1 and lamp2 provide a different hash.
 * 
 * @param str : pointer to text to be hashed
 * @param len : maximum number of characters on which the hash is calculated
 * @param digis : false=digits are not included; true=digits are included
 * @return hash of given text
 ************************************************************************** **/
uint16_t Parser::get_sdbm(const char* str, uint16_t len, bool digits)
{
    uint16_t hash = 0;
    char c;
    while( (c = tolower(*str++) ) && len) {
        if(c > 57 || c < 48 || digits) {
            hash = c + (hash << 6) - hash; // exclude numbers which can cause collisions
        }
        len--;
    }
    return hash;
}

bool Parser::is_true(const char* s)
{
    return (!strcasecmp_P(s, PSTR("true")) || !strcasecmp_P(s, PSTR("on")) || !strcasecmp_P(s, PSTR("yes")) ||
            !strcmp_P(s, PSTR("1")));
}

bool Parser::is_true(JsonVariant json)
{
    return is_true(json.as<std::string>().c_str());
}

bool Parser::is_only_digits(const char* s)
{
    size_t digits = 0;
    while(*(s + digits) != '\0' && isdigit(*(s + digits))) {
        digits++;
    }
    return strlen(s) == digits;
}

int Parser::format_bytes(uint64_t filesize, char* buf, size_t len)
{
    const char* suffix[] = {D_FILE_SIZE_BYTES, D_FILE_SIZE_KILOBYTES, D_FILE_SIZE_MEGABYTES, D_FILE_SIZE_GIGABYTES,
                            D_FILE_SIZE_TERABYTES};
    uint32_t factor;
    uint16_t remainder = 0;
    uint8_t i          = 0;
    uint8_t last_index = (sizeof(suffix) / sizeof(suffix[0])) - 1;

    while(filesize >= D_FILE_SIZE_DIVIDER && i < last_index) {
        i += 1;
        remainder = filesize % D_FILE_SIZE_DIVIDER;
        filesize /= D_FILE_SIZE_DIVIDER;
    }

    factor = (uint32_t)filesize;
    if(i == 0) return snprintf_P(buf, len, PSTR("%u %s"), factor, suffix[i]);

    remainder = remainder * 100 / D_FILE_SIZE_DIVIDER;
    return snprintf_P(buf, len, PSTR("%u" D_DECIMAL_POINT "%02u %s"), factor, remainder, suffix[i]);
}

uint8_t Parser::get_action_id(const char* action)
{
    if(!strcasecmp_P(action, PSTR("prev"))) {
        return HASP_NUM_PAGE_PREV;
    } else if(!strcasecmp_P(action, PSTR("next"))) {
        return HASP_NUM_PAGE_NEXT;
    } else if(!strcasecmp_P(action, PSTR("back"))) {
        return HASP_NUM_PAGE_BACK;
    } else if(action[0] == 'p') {
        action++;
        if(is_only_digits(action)) {
            return atoi(action);
        }
    }
    return 0;
}

#ifndef ARDUINO
long map(long x, long in_min, long in_max, long out_min, long out_max)
{
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
#endif
