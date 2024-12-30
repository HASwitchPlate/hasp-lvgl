/* MIT License - Copyright (c) 2019-2024 Francis Van Roie
   For full license information read the LICENSE file in the project folder */

#include "hasplib.h"
#include "hasp_attribute_helper.h"

/*** Image Improvement ***/
#if defined(ARDUINO_ARCH_ESP32)
#include <HTTPClient.h>
#endif

#if HASP_USE_PNGDECODE > 0
#include "lv_png.h"
#include "lodepng.h"
#endif
/*** Image Improvement ***/

#if HASP_USE_QRCODE > 0
#include "lv_qrcode.h"
#endif

LV_FONT_DECLARE(unscii_8_icon);
extern const char** btnmatrix_default_map; // memory pointer to lvgl default btnmatrix map
extern const char* msgbox_default_map[];   // memory pointer to lvgl default btnmatrix map

// extern const uint8_t rootca_crt_bundle_start[] asm("_binary_data_cert_x509_crt_bundle_bin_start");
// extern const uint8_t rootca_crt_bundle_end[] asm("_binary_data_cert_x509_crt_bundle_bin_end");

void my_image_release_resources(lv_obj_t* obj)
{
    if(!obj) return;

    const void* src       = lv_img_get_src(obj);
    lv_img_src_t src_type = lv_img_src_get_type(src);

    switch(src_type) {
        case LV_IMG_SRC_VARIABLE: {
            lv_img_set_src(obj, LV_SYMBOL_DUMMY); // empty symbol to clear the image
            lv_img_cache_invalidate_src(src);     // remove src from image cache

            lv_img_dsc_t* img_dsc = (lv_img_dsc_t*)src;
            hasp_free((uint8_t*)img_dsc->data); // free image data
            lv_mem_free(img_dsc);               // free image descriptor
            break;
        }

        case LV_IMG_SRC_FILE:
            lv_img_cache_invalidate_src(src); // remove src from image cache
            break;

        default:
            break;
    }
}

void my_btnmatrix_map_clear(lv_obj_t* obj)
{
    lv_btnmatrix_ext_t* ext = (lv_btnmatrix_ext_t*)lv_obj_get_ext_attr(obj);
    const char** map_p_tmp  = ext->map_p; // store current pointer

    LOG_DEBUG(TAG_ATTR, "%s %d %x   btn_cnt: %d", __FILE__, __LINE__, map_p_tmp, ext->btn_cnt);

    if(ext->map_p && (ext->btn_cnt > 0)) {

        // The map exists and is not the default lvgl map anymore
        if((map_p_tmp == NULL) || (btnmatrix_default_map == NULL) || (map_p_tmp == btnmatrix_default_map)) return;

        LOG_DEBUG(TAG_ATTR, "%s %d %x", __FILE__, __LINE__,
                  *map_p_tmp);                                          // label buffer reserved as a contiguous block
        LOG_DEBUG(TAG_ATTR, "%s %d %x", __FILE__, __LINE__, map_p_tmp); // label pointer array block
        lv_btnmatrix_set_map(obj, btnmatrix_default_map);               // reset to default btnmap pointer

        LOG_DEBUG(TAG_ATTR, F("%s %d"), __FILE__, __LINE__);
        lv_mem_free(*map_p_tmp); // free label buffer reserved as a contiguous block
        LOG_DEBUG(TAG_ATTR, F("%s %d"), __FILE__, __LINE__);
        lv_mem_free(map_p_tmp); // free label pointer array block
        LOG_DEBUG(TAG_ATTR, F("%s %d"), __FILE__, __LINE__);
    }
}

void my_msgbox_map_clear(lv_obj_t* obj)
{
    lv_msgbox_ext_t* ext_msgbox = (lv_msgbox_ext_t*)lv_obj_get_ext_attr(obj);
    if(!ext_msgbox) return;

    lv_obj_t* btnmatrix = ext_msgbox->btnm; // Get buttonmatrix object
    if(!btnmatrix) return;

    lv_btnmatrix_ext_t* ext_btnmatrix = (lv_btnmatrix_ext_t*)lv_obj_get_ext_attr(btnmatrix);
    if(!ext_btnmatrix) return;

    if(ext_btnmatrix->map_p != msgbox_default_map) // Don't clear the default btnmap
        my_btnmatrix_map_clear(btnmatrix);         // Clear the custom button map if it exists
}

// Create new btnmatrix button map from json array
const char** my_map_create(const char* payload)
{
    // Reserve memory for JsonDocument
    // StaticJsonDocument<1024> map_doc;
    size_t maxsize = (128u * ((strlen(payload) / 128) + 1)) + 1024;
    DynamicJsonDocument map_doc(maxsize);
    DeserializationError jsonError = deserializeJson(map_doc, payload);

    if(jsonError) { // Couldn't parse incoming JSON payload
        dispatch_json_error(TAG_ATTR, jsonError);
        return NULL;
    }

    JsonArray arr = map_doc.as<JsonArray>(); // Parse payload

    size_t tot_len            = sizeof(char*) * (arr.size() + 1);
    const char** map_data_str = (const char**)lv_mem_alloc(tot_len);
    if(map_data_str == NULL) {
        LOG_ERROR(TAG_ATTR, F("Out of memory while creating button map"));
        return NULL;
    }
    memset(map_data_str, 0, tot_len);

    // Create buffer
    tot_len = 0;
    for(JsonVariant btn : arr) {
        tot_len += strlen(btn.as<const char*>()) + 1;
    }
    tot_len++; // trailing '\0'
    LOG_VERBOSE(TAG_ATTR, F("Array Size = %d, Map Length = %d"), arr.size(), tot_len);

    char* buffer_addr = (char*)lv_mem_alloc(tot_len);
    if(buffer_addr == NULL) {
        lv_mem_free(map_data_str);
        LOG_ERROR(TAG_ATTR, F("Out of memory while creating button map"));
        return NULL;
    }
    memset(buffer_addr, 0, tot_len); // Important, last index needs to be 0 => empty string ""

    /* Point of no return, destroy & free the previous map */
    LOG_DEBUG(TAG_ATTR, F("%s %d   map addr:  %x"), __FILE__, __LINE__, map_data_str);
    // my_btnmatrix_map_clear(obj); // Free previous map

    // Fill buffer
    size_t index = 0;
    size_t pos   = 0;
    LOG_DEBUG(TAG_ATTR, F("%s %d   lbl addr:  %x"), __FILE__, __LINE__, buffer_addr);
    for(JsonVariant btn : arr) {
        // size_t len = btn.as<String>().length() + 1;
        size_t len = strlen(btn.as<const char*>()) + 1;
        LOG_VERBOSE(TAG_ATTR, F(D_BULLET "Adding button: %s (%d bytes) %x"), btn.as<const char*>(), len,
                    buffer_addr + pos);
        // LOG_VERBOSE(TAG_ATTR, F(D_BULLET "Adding button: %s (%d bytes) %x"), btn.as<String>().c_str(), len,
        // buffer_addr + pos);
        memccpy(buffer_addr + pos, btn.as<const char*>(), 0, len); // Copy the label text into the buffer
        // memccpy(buffer_addr + pos, btn.as<String>().c_str(), 0, len); // Copy the label text into the buffer
        map_data_str[index++] = buffer_addr + pos; // save pointer to the label in the array
        pos += len;
    }
    map_data_str[index] = buffer_addr + pos; // save pointer to the last \0 byte

    LOG_DEBUG(TAG_ATTR, F("%s %d"), __FILE__, __LINE__);
    return map_data_str;
}

static void my_btnmatrix_set_map(lv_obj_t* obj, const char* payload)
{
    LOG_DEBUG(TAG_ATTR, F("%s %d"), __FILE__, __LINE__);
    const char** map = my_map_create(payload);
    if(!map) return;

    LOG_DEBUG(TAG_ATTR, F("%s %d"), __FILE__, __LINE__);
    my_btnmatrix_map_clear(obj); // Free previous map
    LOG_DEBUG(TAG_ATTR, F("%s %d"), __FILE__, __LINE__);
    lv_btnmatrix_set_map(obj, map);
    LOG_DEBUG(TAG_ATTR, F("%s %d"), __FILE__, __LINE__);
}

static void my_msgbox_set_map(lv_obj_t* obj, const char* payload)
{
    LOG_DEBUG(TAG_ATTR, F("%s %d"), __FILE__, __LINE__);
    const char** map = my_map_create(payload);
    if(!map) return;

    LOG_DEBUG(TAG_ATTR, F("%s %d"), __FILE__, __LINE__);
    my_msgbox_map_clear(obj); // Free previous map
    LOG_DEBUG(TAG_ATTR, F("%s %d"), __FILE__, __LINE__);
    lv_msgbox_add_btns(obj, map);
    LOG_DEBUG(TAG_ATTR, F("%s %d"), __FILE__, __LINE__);
}

void my_line_clear_points(lv_obj_t* obj)
{
    lv_line_ext_t* ext    = (lv_line_ext_t*)lv_obj_get_ext_attr(obj);
    const lv_point_t* ptr = ext->point_array;
    lv_line_set_points(obj, NULL, 0);
    lv_mem_free(ptr);
}

static bool my_line_set_points(lv_obj_t* obj, const char* payload)
{
    my_line_clear_points(obj); // delete pointmap

    // Create new points
    // Reserve memory for JsonDocument
    // StaticJsonDocument<1024> doc;
    size_t maxsize = (128u * ((strlen(payload) / 128) + 1)) + 256;
    DynamicJsonDocument doc(maxsize);
    DeserializationError jsonError = deserializeJson(doc, payload);

    if(jsonError) { // Couldn't parse incoming JSON payload
        dispatch_json_error(TAG_ATTR, jsonError);
        return false;
    }

    JsonArray arr  = doc.as<JsonArray>(); // Parse payload
    size_t tot_len = sizeof(lv_point_t*) * (arr.size());
    if(tot_len == 0) return false; // bad input

    lv_point_t* point_arr = (lv_point_t*)lv_mem_alloc(tot_len);
    if(point_arr == NULL) {
        LOG_ERROR(TAG_ATTR, F("Out of memory while creating line points"));
        return false;
    }
    memset(point_arr, 0, tot_len);

    size_t index = 0;
    for(JsonVariant v : arr) {
        JsonArray point = v.as<JsonArray>(); // Parse point
        if(point.size() == 2) {
            point_arr[index].x = point[0].as<int16_t>();
            point_arr[index].y = point[1].as<int16_t>();
            LOG_VERBOSE(TAG_ATTR, F(D_BULLET "Adding point %d: %d,%d"), index, point_arr[index].x, point_arr[index].y);
            index++;
        }
    }

    my_line_clear_points(obj);                 // free previous pointlist
    lv_line_set_points(obj, point_arr, index); // arr.size());
    return true;
}

static lv_font_t* haspPayloadToFont(const char* payload)
{
    if(Parser::is_only_digits(payload)) {
        uint8_t var = atoi(payload);

        if(var >= 0 && var < 8)
            return hasp_get_font(var);
        else if(var == 8)
            return &unscii_8_icon;

#if !defined(ARDUINO_ARCH_ESP8266) // && (HASP_USE_FREETYPE == 0)

#if defined(HASP_FONT_1) && defined(HASP_FONT_1)
        else if(var == HASP_FONT_SIZE_1)
            return &HASP_FONT_1;
#endif
#if defined(HASP_FONT_2) && defined(HASP_FONT_2)
        else if(var == HASP_FONT_SIZE_2)
            return &HASP_FONT_2;
#endif
#if defined(HASP_FONT_3) && defined(HASP_FONT_3)
        else if(var == HASP_FONT_SIZE_3)
            return &HASP_FONT_3;
#endif
#if defined(HASP_FONT_4) && defined(HASP_FONT_4)
        else if(var == HASP_FONT_SIZE_4)
            return &HASP_FONT_4;
#endif
#if defined(HASP_FONT_5) && defined(HASP_FONT_5)
        else if(var == HASP_FONT_SIZE_5)
            return &HASP_FONT_5;
#endif

#endif
    }

    return get_font(payload);
}

static hasp_attribute_type_t hasp_process_label_long_mode(lv_obj_t* obj, const char* payload, char** text, bool update)
{
    const char* arr[] = {PSTR("expand"), PSTR("break"), PSTR("dots"), PSTR("scroll"), PSTR("loop"), PSTR("crop")};
    uint8_t count     = sizeof(arr) / sizeof(arr[0]);
    uint8_t i         = 0;

    if(update) {
        for(i = 0; i < count; i++) {
            if(!strcasecmp_P(payload, arr[i])) {
                lv_label_set_long_mode(obj, (lv_label_long_mode_t)i);
                break;
            }
        }
    } else {
        i = lv_label_get_long_mode(obj);
    }

    if(i < count) {
        strcpy_P(*text, arr[i]);
        return HASP_ATTR_TYPE_STR;
    }

    return HASP_ATTR_TYPE_NOT_FOUND;
}

static hasp_attribute_type_t hasp_process_cpicker_mode(lv_obj_t* obj, const char* payload, char** text, bool update)
{
    const char* arr[] = {PSTR("hue"), PSTR("saturation"), PSTR("value")};
    uint8_t count     = sizeof(arr) / sizeof(arr[0]);
    uint8_t i         = 0;

    if(update) {
        for(i = 0; i < count; i++) {
            if(!strcasecmp_P(payload, arr[i])) {
                lv_cpicker_set_color_mode(obj, (lv_cpicker_color_mode_t)i);
                break;
            }
        }
    } else {
        i = lv_cpicker_get_color_mode(obj);
    }

    if(i < count) {
        strcpy_P(*text, arr[i]);
        return HASP_ATTR_TYPE_STR;
    }

    return HASP_ATTR_TYPE_NOT_FOUND;
}

size_t hasp_attribute_split_payload(const char* payload)
{
    size_t pos = 0;
    while(*(payload + pos) != '\0') {
        if(Parser::is_only_digits(payload + pos)) return pos;
        pos++;
    }
    return pos;
}

static void hasp_attribute_get_part_state_new(lv_obj_t* obj, const char* attr_in, char* attr_out, uint8_t& part,
                                              uint8_t& state)
{
    state = LV_STATE_DEFAULT;
    part  = LV_OBJ_PART_MAIN;

    size_t pos = hasp_attribute_split_payload(attr_in);
    if(pos <= 0 || pos >= 32) {
        attr_out[0] = 0; // empty string
        return;
    }

    strncpy(attr_out, attr_in, pos);
    attr_out[pos] = 0;

    int index         = atoi(attr_in + pos);
    uint8_t state_num = index % 10;
    uint8_t part_num  = index - state_num;

    LOG_DEBUG(TAG_ATTR, F("Parsed %s to %s with part %d and state %d"), attr_in, attr_out, part_num, state_num);

#if(LV_SLIDER_PART_INDIC != LV_SWITCH_PART_INDIC) || (LV_SLIDER_PART_KNOB != LV_SWITCH_PART_KNOB) ||                   \
    (LV_SLIDER_PART_BG != LV_SWITCH_PART_BG) || (LV_SLIDER_PART_INDIC != LV_ARC_PART_INDIC) ||                         \
    (LV_SLIDER_PART_KNOB != LV_ARC_PART_KNOB) || (LV_SLIDER_PART_BG != LV_ARC_PART_BG) ||                              \
    (LV_SLIDER_PART_INDIC != LV_SPINNER_PART_INDIC) || (LV_SLIDER_PART_BG != LV_SPINNER_PART_BG) ||                    \
    (LV_SLIDER_PART_INDIC != LV_BAR_PART_INDIC) || (LV_SLIDER_PART_BG != LV_BAR_PART_BG) ||                            \
    (LV_SLIDER_PART_KNOB != LV_GAUGE_PART_NEEDLE) || (LV_SLIDER_PART_INDIC != LV_GAUGE_PART_MAJOR) ||                  \
    (LV_SLIDER_PART_BG != LV_GAUGE_PART_MAIN)
#error "LV_SLIDER, LV_BAR, LV_ARC, LV_SPINNER, LV_SWITCH, LV_GAUGE parts should match!"
#endif

    /* States */
    switch(state_num) {
        case 1:
            state = LV_STATE_CHECKED;
            break;
        case 2:
            state = LV_STATE_PRESSED + LV_STATE_DEFAULT;
            break;
        case 3:
            state = LV_STATE_PRESSED + LV_STATE_CHECKED;
            break;
        case 4:
            state = LV_STATE_DISABLED + LV_STATE_DEFAULT;
            break;
        case 5:
            state = LV_STATE_DISABLED + LV_STATE_CHECKED;
            break;
        default: // 0 or 6-9
            state = LV_STATE_DEFAULT;
    }

    /* Parts */
    switch(obj_get_type(obj)) {
        case LV_HASP_BUTTON:
        case LV_HASP_LABEL:
        case LV_HASP_LED:
        case LV_HASP_LINE:
        case LV_HASP_LINEMETER:
        case LV_HASP_IMAGE:
        case LV_HASP_IMGBTN:
        case LV_HASP_OBJECT:
        case LV_HASP_TAB:
        case LV_HASP_QRCODE:
            part = LV_BTN_PART_MAIN;
            break;

        case LV_HASP_BTNMATRIX:
            switch(part_num) {
                case LV_HASP_PART_ITEMS:
                    part = LV_BTNMATRIX_PART_BTN;
                    break;
                default:
                    part = LV_BTNMATRIX_PART_BG;
            }
            break;

        case LV_HASP_SLIDER:
        case LV_HASP_SWITCH:
        case LV_HASP_ARC:
            switch(part_num) {
                case LV_HASP_PART_INDICATOR:
                    part = LV_SLIDER_PART_INDIC;
                    break;
                case LV_HASP_PART_KNOB:
                    part = LV_SLIDER_PART_KNOB;
                    break;
                default:
                    part = LV_SLIDER_PART_BG;
            }
            break;

        case LV_HASP_BAR:
        case LV_HASP_SPINNER:
            if(part_num == LV_HASP_PART_INDICATOR) {
                part = LV_SLIDER_PART_INDIC;
            } else {
                part = LV_SLIDER_PART_BG;
            }
            break;

        case LV_HASP_CHECKBOX:
            part = part_num == LV_HASP_PART_INDICATOR ? LV_CHECKBOX_PART_BULLET : LV_CHECKBOX_PART_BG;
            break;

        case LV_HASP_CPICKER:
            part = part_num == LV_HASP_PART_KNOB ? LV_CPICKER_PART_KNOB : LV_CPICKER_PART_MAIN;
            break;

        case LV_HASP_ROLLER:
            switch(part_num) {
                case LV_HASP_PART_SELECTED:
                    part = LV_ROLLER_PART_SELECTED;
                    break;
                default:
                    part = LV_ROLLER_PART_BG;
            }
            break;

        case LV_HASP_DROPDOWN:
            switch(part_num) {
                case LV_HASP_PART_ITEMS:
                    part = LV_DROPDOWN_PART_LIST;
                    break;
                case LV_HASP_PART_SELECTED:
                    part = LV_DROPDOWN_PART_SELECTED;
                    break;
                case LV_HASP_PART_SCROLLBAR:
                    part = LV_DROPDOWN_PART_SCROLLBAR;
                    break;
                default:
                    part = LV_DROPDOWN_PART_MAIN;
            }
            break;

        case LV_HASP_GAUGE:
            switch(part_num) {
                case LV_HASP_PART_INDICATOR:
                    part = LV_GAUGE_PART_NEEDLE;
                    break;
                case LV_HASP_PART_TICKS:
                    part = LV_GAUGE_PART_MAJOR;
                    break;
                default:
                    part = LV_GAUGE_PART_MAIN;
            }
            break;

        case LV_HASP_MSGBOX:
            switch(part_num) {
                case LV_HASP_PART_ITEMS_BG:
                    part = LV_MSGBOX_PART_BTN_BG; // Button Matrix Background
                    break;
                case LV_HASP_PART_ITEMS:
                    part = LV_MSGBOX_PART_BTN; // Button Matrix Buttons
                    break;
                default:
                    part = LV_MSGBOX_PART_BG;
            }
            break;

        case LV_HASP_TABVIEW:
            switch(part_num) {
                case LV_HASP_PART_INDICATOR:
                    part = LV_TABVIEW_PART_INDIC; // Rectangle-like object under the currently selected tab
                    break;
                case LV_HASP_PART_ITEMS_BG:
                    part = LV_TABVIEW_PART_TAB_BG; // Button Matrix background
                    break;
                case LV_HASP_PART_ITEMS:
                    part = LV_TABVIEW_PART_TAB_BTN; // Button Matrix Button
                    break;
                case LV_HASP_PART_SELECTED:
                    part = LV_TABVIEW_PART_BG_SCROLLABLE; // It holds the content of the tabs next to each other
                    break;
                default:
                    part = LV_TABVIEW_PART_BG;
            }
            break;

        default:; // nothing to do
    }
}

static void hasp_attribute_get_part_state_old(lv_obj_t* obj, const char* attr_in, char* attr_out, uint8_t& part,
                                              uint8_t& state)
{
    int len = strlen(attr_in);
    if(len <= 0 || len >= 32) {
        attr_out[0] = 0; // empty string
        part        = LV_OBJ_PART_MAIN;
        state       = LV_STATE_DEFAULT;
        return;
    }
    int index = atoi(&attr_in[len - 1]);

    if(attr_in[len - 1] == '0') {
        len--; // Drop Trailing partnumber
    } else if(index > 0) {
        len--; // Drop Trailing partnumber
    } else {
        index = -1; // force default state when no trailing number is found
    }
    strncpy(attr_out, attr_in, len);
    attr_out[len] = 0;

#if(LV_SLIDER_PART_INDIC != LV_SWITCH_PART_INDIC) || (LV_SLIDER_PART_KNOB != LV_SWITCH_PART_KNOB) ||                   \
    (LV_SLIDER_PART_BG != LV_SWITCH_PART_BG) || (LV_SLIDER_PART_INDIC != LV_ARC_PART_INDIC) ||                         \
    (LV_SLIDER_PART_KNOB != LV_ARC_PART_KNOB) || (LV_SLIDER_PART_BG != LV_ARC_PART_BG) ||                              \
    (LV_SLIDER_PART_INDIC != LV_SPINNER_PART_INDIC) || (LV_SLIDER_PART_BG != LV_SPINNER_PART_BG) ||                    \
    (LV_SLIDER_PART_INDIC != LV_BAR_PART_INDIC) || (LV_SLIDER_PART_BG != LV_BAR_PART_BG) ||                            \
    (LV_SLIDER_PART_KNOB != LV_GAUGE_PART_NEEDLE) || (LV_SLIDER_PART_INDIC != LV_GAUGE_PART_MAJOR) ||                  \
    (LV_SLIDER_PART_BG != LV_GAUGE_PART_MAIN)
#error "LV_SLIDER, LV_BAR, LV_ARC, LV_SPINNER, LV_SWITCH, LV_GAUGE parts should match!"
#endif

    /* Attributes depending on objecttype */
    state = LV_STATE_DEFAULT;
    part  = LV_BTN_PART_MAIN;

    switch(obj_get_type(obj)) {
        case LV_HASP_BUTTON:
            switch(index) {
                case 1:
                    state = LV_STATE_CHECKED;
                    break;
                case 2:
                    state = LV_STATE_PRESSED + LV_STATE_DEFAULT;
                    break;
                case 3:
                    state = LV_STATE_PRESSED + LV_STATE_CHECKED;
                    break;
                case 4:
                    state = LV_STATE_DISABLED + LV_STATE_DEFAULT;
                    break;
                case 5:
                    state = LV_STATE_DISABLED + LV_STATE_CHECKED;
                    break;
                default: // 0 or -1
                    state = LV_STATE_DEFAULT;
            }
            break;

        case LV_HASP_BTNMATRIX:
            switch(index) {
                case 0:
                    part  = LV_BTNMATRIX_PART_BTN;
                    state = LV_STATE_DEFAULT;
                    break;
                case 1:
                    part  = LV_BTNMATRIX_PART_BTN;
                    state = LV_STATE_CHECKED;
                    break;
                case 2:
                    part  = LV_BTNMATRIX_PART_BTN;
                    state = LV_STATE_PRESSED + LV_STATE_DEFAULT;
                    break;
                case 3:
                    part  = LV_BTNMATRIX_PART_BTN;
                    state = LV_STATE_PRESSED + LV_STATE_CHECKED;
                    break;
                case 4:
                    part  = LV_BTNMATRIX_PART_BTN;
                    state = LV_STATE_DISABLED + LV_STATE_DEFAULT;
                    break;
                case 5:
                    part  = LV_BTNMATRIX_PART_BTN;
                    state = LV_STATE_DISABLED + LV_STATE_CHECKED;
                    break;
                default: // -1
                    state = LV_STATE_DEFAULT;
                    part  = LV_BTNMATRIX_PART_BG;
            }
            break;

        case LV_HASP_SLIDER:
        case LV_HASP_SWITCH:
        case LV_HASP_ARC:
        case LV_HASP_BAR:
        case LV_HASP_SPINNER:
            if(index == 1) {
                part = LV_SLIDER_PART_INDIC;
            } else if(index == 2) {
                if(!obj_check_type(obj, LV_HASP_BAR) && !obj_check_type(obj, LV_HASP_SPINNER))
                    part = LV_SLIDER_PART_KNOB;
            } else { // index = 0 or -1
                part = LV_SLIDER_PART_BG;
            }
            break;

        case LV_HASP_CHECKBOX:
            part = index == 1 ? LV_CHECKBOX_PART_BULLET : LV_CHECKBOX_PART_BG;
            break;

        case LV_HASP_CPICKER:
            part = index == 1 ? LV_CPICKER_PART_KNOB : LV_CPICKER_PART_MAIN;
            break;

        case LV_HASP_ROLLER:
            part = index == 1 ? LV_ROLLER_PART_SELECTED : LV_ROLLER_PART_BG;
            break;

        case LV_HASP_GAUGE:
            part = (index > 0) && (index <= LV_GAUGE_PART_NEEDLE) ? index : LV_GAUGE_PART_MAIN;
            break;

        case LV_HASP_TABVIEW:
            switch(index) {
                case 0:
                    part  = LV_TABVIEW_PART_TAB_BTN; // Button Matrix
                    state = LV_STATE_DEFAULT;
                    break;
                case 1:
                    part  = LV_TABVIEW_PART_TAB_BTN; // Button Matrix
                    state = LV_STATE_CHECKED;
                    break;
                case 2:
                    part  = LV_TABVIEW_PART_TAB_BTN; // Button Matrix
                    state = LV_STATE_PRESSED + LV_STATE_DEFAULT;
                    break;
                case 3:
                    part  = LV_TABVIEW_PART_TAB_BTN; // Button Matrix
                    state = LV_STATE_PRESSED + LV_STATE_CHECKED;
                    break;
                case 4:
                    part  = LV_TABVIEW_PART_TAB_BTN; // Button Matrix
                    state = LV_STATE_DISABLED + LV_STATE_DEFAULT;
                    break;
                case 5:
                    part  = LV_TABVIEW_PART_TAB_BTN; // Button Matrix
                    state = LV_STATE_DISABLED + LV_STATE_CHECKED;
                    break;
                case 6:
                    part = LV_TABVIEW_PART_TAB_BG; // Matrix background
                                                   // state = LV_STATE_DEFAULT;
                    break;
                case 7:
                    part = LV_TABVIEW_PART_INDIC; // Rectangle-like object under the currently selected tab
                                                  // state = LV_STATE_DEFAULT;
                    break;
                case 8:
                    part = LV_TABVIEW_PART_BG_SCROLLABLE; // It holds the content of the tabs next to each other
                                                          // state = LV_STATE_DEFAULT;
                    break;

                default: // 9 or -1
                    part = LV_TABVIEW_PART_BG;
                    // state = LV_STATE_DEFAULT;
            }
            break;

        default:; // nothing to do
    }
}

static void hasp_attribute_get_part_state(lv_obj_t* obj, const char* attr_in, char* attr_out, uint8_t& part,
                                          uint8_t& state)
{
    size_t pos = hasp_attribute_split_payload(attr_in);
    if(strlen(attr_in + pos) == 2)
        hasp_attribute_get_part_state_new(obj, attr_in, attr_out, part, state);
    else
        hasp_attribute_get_part_state_old(obj, attr_in, attr_out, part, state);
}

/**
 * Change or Retrieve the value of a local attribute of an object PART
 * @param obj lv_obj_t*: the object to get/set the attribute
 * @param attr_p char*: the attribute name (with or without leading ".")
 * @param attr_hash uint16_t: the sbdm hash of the attribute name without leading "."
 * @param payload char*: the new value of the attribute
 * @param update  bool: change/set the value if true, dispatch/get value if false
 * @note setting a value won't return anything, getting will dispatch the value
 */
static hasp_attribute_type_t hasp_local_style_attr(lv_obj_t* obj, const char* attr_p, uint16_t attr_hash,
                                                   const char* payload, bool update, int32_t& val)
{
    char attr[32];
    uint8_t part  = LV_OBJ_PART_MAIN;
    uint8_t state = LV_STATE_DEFAULT;
    int16_t var   = atoi(payload);

    // test_prop(attr_hash);

    hasp_attribute_get_part_state(obj, attr_p, attr, part, state);
    attr_hash = Parser::get_sdbm(attr); // attribute name without the index number

    /* ***** WARNING ****************************************************
     * when using hasp_out use attr_p for the original attribute name
     * *************************************************************** */

    switch(attr_hash) {

/* 1: Use other blend modes than normal (`LV_BLEND_MODE_...`)*/
#if LV_USE_BLEND_MODES
        case ATTR_BG_BLEND_MODE:
            return attribute_bg_blend_mode(obj, part, state, update, (lv_blend_mode_t)var, val);
            // case ATTR_TEXT_BLEND_MODE:
            //     return lv_obj_set_style_local_text_blend_mode(obj, part, state, (lv_blend_mode_t)var);
            // case ATTR_BORDER_BLEND_MODE:
            //     return lv_obj_set_style_local_border_blend_mode(obj, part, state, (lv_blend_mode_t)var);
            // case ATTR_OUTLINE_BLEND_MODE:
            //     return lv_obj_set_style_local_outline_blend_mode(obj, part, state, (lv_blend_mode_t)var);
            // case ATTR_SHADOW_BLEND_MODE:
            //     return lv_obj_set_style_local_shadow_blend_mode(obj, part, state, (lv_blend_mode_t)var);
            // case ATTR_LINE_BLEND_MODE:
            //     return lv_obj_set_style_local_line_blend_mode(obj, part, state, (lv_blend_mode_t)var);
            // case ATTR_VALUE_BLEND_MODE:
            //     return lv_obj_set_style_local_value_blend_mode(obj, part, state, (lv_blend_mode_t)var);
            // case ATTR_PATTERN_BLEND_MODE:
            //     return lv_obj_set_style_local_pattern_blend_mode(obj, part, state, (lv_blend_mode_t)var);
#endif

        case ATTR_SIZE:
            return attribute_size(obj, part, state, update, var, val);
        case ATTR_RADIUS:
            return attribute_radius(obj, part, state, update, var, val);
        case ATTR_CLIP_CORNER:
            return attribute_clip_corner(obj, part, state, update, var, val);

        case ATTR_OPA_SCALE:
            return attribute_opa_scale(obj, part, state, update, (lv_opa_t)var, val);
        case ATTR_TRANSFORM_WIDTH:
            return attribute_transform_width(obj, part, state, update, (lv_style_int_t)var, val);
        case ATTR_TRANSFORM_HEIGHT:
            return attribute_transform_height(obj, part, state, update, (lv_style_int_t)var, val);

            /* Background attributes */
        case ATTR_BG_MAIN_STOP:
            return attribute_bg_main_stop(obj, part, state, update, (lv_style_int_t)var, val);
        case ATTR_BG_GRAD_STOP:
            return attribute_bg_grad_stop(obj, part, state, update, (lv_style_int_t)var, val);
        case ATTR_BG_GRAD_DIR:
            return attribute_bg_grad_dir(obj, part, state, update, (lv_grad_dir_t)var, val);
        case ATTR_BG_COLOR: {
            if(update) {
                lv_color32_t c;
                if(Parser::haspPayloadToColor(payload, c) && part != 64)
                    lv_obj_set_style_local_bg_color(obj, part, state, lv_color_make(c.ch.red, c.ch.green, c.ch.blue));
            } else {
                attr_out_color(obj, attr, lv_obj_get_style_bg_color(obj, part));
            }
            return HASP_ATTR_TYPE_METHOD_OK;
        }
        case ATTR_BG_GRAD_COLOR:
            if(update) {
                lv_color32_t c;
                if(Parser::haspPayloadToColor(payload, c))
                    lv_obj_set_style_local_bg_grad_color(obj, part, state,
                                                         lv_color_make(c.ch.red, c.ch.green, c.ch.blue));
            } else {
                attr_out_color(obj, attr, lv_obj_get_style_bg_grad_color(obj, part));
            }
            return HASP_ATTR_TYPE_METHOD_OK;

        case ATTR_BG_OPA:
            return attribute_bg_opa(obj, part, state, update, (lv_opa_t)var, val);

        /* Margin attributes */
        case ATTR_MARGIN_TOP:
            return attribute_margin_top(obj, part, state, update, (lv_style_int_t)var, val);
        case ATTR_MARGIN_BOTTOM:
            return attribute_margin_bottom(obj, part, state, update, (lv_style_int_t)var, val);
        case ATTR_MARGIN_LEFT:
            return attribute_margin_left(obj, part, state, update, (lv_style_int_t)var, val);
        case ATTR_MARGIN_RIGHT:
            return attribute_margin_right(obj, part, state, update, (lv_style_int_t)var, val);

        /* Padding attributes */
        case ATTR_PAD_TOP:
            return attribute_pad_top(obj, part, state, update, (lv_style_int_t)var, val);
        case ATTR_PAD_BOTTOM:
            return attribute_pad_bottom(obj, part, state, update, (lv_style_int_t)var, val);
        case ATTR_PAD_LEFT:
            return attribute_pad_left(obj, part, state, update, (lv_style_int_t)var, val);
        case ATTR_PAD_RIGHT:
            return attribute_pad_right(obj, part, state, update, (lv_style_int_t)var, val);
#if LVGL_VERSION_MAJOR == 7
        case ATTR_PAD_INNER:
            return attribute_pad_inner(obj, part, state, update, (lv_style_int_t)var, val);
#endif

        /* Scale attributes */
        case ATTR_SCALE_GRAD_COLOR: {
            if(update) {
                lv_color32_t c;
                if(Parser::haspPayloadToColor(payload, c) && part != 64)
                    lv_obj_set_style_local_scale_grad_color(obj, part, state,
                                                            lv_color_make(c.ch.red, c.ch.green, c.ch.blue));
            } else {
                attr_out_color(obj, attr, lv_obj_get_style_scale_grad_color(obj, part));
            }
            return HASP_ATTR_TYPE_METHOD_OK;
        }
        case ATTR_SCALE_END_COLOR:
            if(update) {
                lv_color32_t c;
                if(Parser::haspPayloadToColor(payload, c))
                    lv_obj_set_style_local_scale_end_color(obj, part, state,
                                                           lv_color_make(c.ch.red, c.ch.green, c.ch.blue));
            } else {
                attr_out_color(obj, attr, lv_obj_get_style_scale_end_color(obj, part));
            }
            return HASP_ATTR_TYPE_METHOD_OK;
        case ATTR_SCALE_END_LINE_WIDTH:
            return attribute_scale_end_line_width(obj, part, state, update, (lv_style_int_t)var, val);
        case ATTR_SCALE_END_BORDER_WIDTH:
            return attribute_scale_end_border_width(obj, part, state, update, (lv_style_int_t)var, val);
        case ATTR_SCALE_BORDER_WIDTH:
            return attribute_scale_border_width(obj, part, state, update, (lv_style_int_t)var, val);
        case ATTR_SCALE_WIDTH:
            return attribute_scale_width(obj, part, state, update, (lv_style_int_t)var, val);

        /* Text attributes */
        case ATTR_TEXT_LETTER_SPACE:
            return attribute_text_letter_space(obj, part, state, update, (lv_style_int_t)var, val);
        case ATTR_TEXT_LINE_SPACE:
            return attribute_text_line_space(obj, part, state, update, (lv_style_int_t)var, val);
        case ATTR_TEXT_DECOR:
            return attribute_text_decor(obj, part, state, update, (lv_text_decor_t)var, val);
        case ATTR_TEXT_OPA:
            return attribute_text_opa(obj, part, state, update, (lv_opa_t)var, val);
        case ATTR_TEXT_COLOR: {
            if(update) {
                lv_color32_t c;
                if(Parser::haspPayloadToColor(payload, c))
                    lv_obj_set_style_local_text_color(obj, part, state, lv_color_make(c.ch.red, c.ch.green, c.ch.blue));
            } else {
                attr_out_color(obj, attr, lv_obj_get_style_text_color(obj, part));
            }
            return HASP_ATTR_TYPE_METHOD_OK;
        }
        case ATTR_TEXT_SEL_COLOR: {
            if(update) {
                lv_color32_t c;
                if(Parser::haspPayloadToColor(payload, c))
                    lv_obj_set_style_local_text_sel_color(obj, part, state,
                                                          lv_color_make(c.ch.red, c.ch.green, c.ch.blue));
            } else {
                attr_out_color(obj, attr, lv_obj_get_style_text_sel_color(obj, part));
            }
            return HASP_ATTR_TYPE_METHOD_OK;
        }
        case ATTR_TEXT_FONT: {
            lv_font_t* font = haspPayloadToFont(payload);
            if(font) {
                LOG_DEBUG(TAG_ATTR, "%s %d %x", __FILE__, __LINE__, font);
                uint8_t count = 3;
                if(obj_check_type(obj, LV_HASP_ROLLER)) count = my_roller_get_visible_row_count(obj);
                lv_obj_set_style_local_text_font(obj, part, state, font);
                if(obj_check_type(obj, LV_HASP_ROLLER)) lv_roller_set_visible_row_count(obj, count);
                lv_obj_set_style_local_text_font(obj, part, state, font); // again, for roller

                if(obj_check_type(obj, LV_HASP_DROPDOWN)) { // issue #43
                    lv_obj_set_style_local_text_font(obj, LV_DROPDOWN_PART_MAIN, state, font);
                    lv_obj_set_style_local_text_font(obj, LV_DROPDOWN_PART_LIST, state, font);
                    lv_obj_set_style_local_text_font(obj, LV_DROPDOWN_PART_SELECTED, state, font);
                };

            } else {
                LOG_WARNING(TAG_ATTR, F("Unknown Font ID %s"), payload);
            }
            return HASP_ATTR_TYPE_METHOD_OK;
        }

        /* Border attributes */
        case ATTR_BORDER_WIDTH:
            return attribute_border_width(obj, part, state, update, (lv_style_int_t)var, val);
        case ATTR_BORDER_SIDE:
            return attribute_border_side(obj, part, state, update, (lv_border_side_t)var, val);
        case ATTR_BORDER_POST:
            attribute_border_post(obj, part, state, update, Parser::is_true(payload), val);
            return HASP_ATTR_TYPE_BOOL;
        case ATTR_BORDER_OPA:
            return attribute_border_opa(obj, part, state, update, (lv_opa_t)var, val);
        case ATTR_BORDER_COLOR: {
            if(update) {
                lv_color32_t c;
                if(Parser::haspPayloadToColor(payload, c))
                    lv_obj_set_style_local_border_color(obj, part, state,
                                                        lv_color_make(c.ch.red, c.ch.green, c.ch.blue));
            } else {
                attr_out_color(obj, attr, lv_obj_get_style_border_color(obj, part));
            }
            return HASP_ATTR_TYPE_METHOD_OK;
        }

        /* Outline attributes */
        case ATTR_OUTLINE_WIDTH:
            return attribute_outline_width(obj, part, state, update, (lv_style_int_t)var, val);
        case ATTR_OUTLINE_PAD:
            return attribute_outline_pad(obj, part, state, update, (lv_style_int_t)var, val);
        case ATTR_OUTLINE_OPA:
            return attribute_outline_opa(obj, part, state, update, (lv_opa_t)var, val);
        case ATTR_OUTLINE_COLOR: {
            if(update) {
                lv_color32_t c;
                if(Parser::haspPayloadToColor(payload, c))
                    lv_obj_set_style_local_outline_color(obj, part, state,
                                                         lv_color_make(c.ch.red, c.ch.green, c.ch.blue));
            } else {
                attr_out_color(obj, attr, lv_obj_get_style_outline_color(obj, part));
            }
            return HASP_ATTR_TYPE_METHOD_OK;
        }

        /* Shadow attributes */
#if LV_USE_SHADOW
        case ATTR_SHADOW_WIDTH:
            return attribute_shadow_width(obj, part, state, update, (lv_style_int_t)var, val);
        case ATTR_SHADOW_OFS_X:
            return attribute_shadow_ofs_x(obj, part, state, update, (lv_style_int_t)var, val);
        case ATTR_SHADOW_OFS_Y:
            return attribute_shadow_ofs_y(obj, part, state, update, (lv_style_int_t)var, val);
        case ATTR_SHADOW_SPREAD:
            return attribute_shadow_spread(obj, part, state, update, (lv_style_int_t)var, val);
        case ATTR_SHADOW_OPA:
            return attribute_shadow_opa(obj, part, state, update, (lv_opa_t)var, val);
        case ATTR_SHADOW_COLOR: {
            if(update) {
                lv_color32_t c;
                if(Parser::haspPayloadToColor(payload, c))
                    lv_obj_set_style_local_shadow_color(obj, part, state,
                                                        lv_color_make(c.ch.red, c.ch.green, c.ch.blue));
            } else {
                attr_out_color(obj, attr, lv_obj_get_style_shadow_color(obj, part));
            }
            return HASP_ATTR_TYPE_METHOD_OK;
        }
#endif

        /* Line attributes */
        case ATTR_LINE_WIDTH:
            return attribute_line_width(obj, part, state, update, (lv_style_int_t)var, val);
        case ATTR_LINE_DASH_WIDTH:
            return attribute_line_dash_width(obj, part, state, update, (lv_style_int_t)var, val);
        case ATTR_LINE_DASH_GAP:
            return attribute_line_dash_gap(obj, part, state, update, (lv_style_int_t)var, val);
        case ATTR_LINE_ROUNDED:
            attribute_line_rounded(obj, part, state, update, Parser::is_true(payload), val);
            return HASP_ATTR_TYPE_BOOL;
        case ATTR_LINE_OPA:
            return attribute_line_opa(obj, part, state, update, (lv_opa_t)var, val);
        case ATTR_LINE_COLOR: {
            if(update) {
                lv_color32_t c;
                if(Parser::haspPayloadToColor(payload, c))
                    lv_obj_set_style_local_line_color(obj, part, state, lv_color_make(c.ch.red, c.ch.green, c.ch.blue));
            } else {
                attr_out_color(obj, attr, lv_obj_get_style_line_color(obj, part));
            }
            return HASP_ATTR_TYPE_METHOD_OK;
        }

        /* Value attributes */
        case ATTR_VALUE_LETTER_SPACE:
            return attribute_value_letter_space(obj, part, state, update, (lv_style_int_t)var, val);
        case ATTR_VALUE_LINE_SPACE:
            return attribute_value_line_space(obj, part, state, update, (lv_style_int_t)var, val);
        case ATTR_VALUE_OFS_X:
            return attribute_value_ofs_x(obj, part, state, update, (lv_style_int_t)var, val);
        case ATTR_VALUE_OFS_Y:
            return attribute_value_ofs_y(obj, part, state, update, (lv_style_int_t)var, val);
        case ATTR_VALUE_ALIGN:
            return attribute_value_align(obj, part, state, update, (lv_align_t)var, val);
        case ATTR_VALUE_OPA:
            return attribute_value_opa(obj, part, state, update, (lv_opa_t)var, val);
        case ATTR_VALUE_STR: {
            if(update) {
                my_obj_set_value_str_text(obj, part, state, payload);
            } else {
                attr_out_str(obj, attr, my_obj_get_value_str_text(obj, part, state));
            }
            return HASP_ATTR_TYPE_METHOD_OK;
        }
        case ATTR_VALUE_COLOR: {
            if(update) {
                lv_color32_t c;
                if(Parser::haspPayloadToColor(payload, c))
                    lv_obj_set_style_local_value_color(obj, part, state,
                                                       lv_color_make(c.ch.red, c.ch.green, c.ch.blue));
            } else {
                attr_out_color(obj, attr, lv_obj_get_style_value_color(obj, part));
            }
            return HASP_ATTR_TYPE_METHOD_OK;
        }
        case ATTR_VALUE_FONT: {
            lv_font_t* font = haspPayloadToFont(payload);
            if(font) {
                lv_obj_set_style_local_value_font(obj, part, state, font);
            } else {
                LOG_WARNING(TAG_ATTR, F("Unknown Font ID %s"), attr_p);
            }
            return HASP_ATTR_TYPE_METHOD_OK;
        }

        /* Pattern attributes */
        case ATTR_PATTERN_REPEAT:
            attribute_pattern_repeat(obj, part, state, update, Parser::is_true(payload), val);
            return HASP_ATTR_TYPE_BOOL;
        case ATTR_PATTERN_OPA:
            return attribute_pattern_opa(obj, part, state, update, (lv_opa_t)var, val);
        case ATTR_PATTERN_RECOLOR_OPA:
            return attribute_pattern_recolor_opa(obj, part, state, update, (lv_opa_t)var, val);
        case ATTR_PATTERN_RECOLOR: {
            if(update) {
                lv_color32_t c;
                if(Parser::haspPayloadToColor(payload, c))
                    lv_obj_set_style_local_pattern_recolor(obj, part, state,
                                                           lv_color_make(c.ch.red, c.ch.green, c.ch.blue));
            } else {
                attr_out_color(obj, attr, lv_obj_get_style_pattern_recolor(obj, part));
            }
            return HASP_ATTR_TYPE_METHOD_OK;
        }

        case ATTR_PATTERN_IMAGE:
            //   return lv_obj_set_style_local_pattern_image(obj, part, state, (constvoid *)var);
            // return;

            /* Image attributes */
        case ATTR_IMAGE_RECOLOR_OPA:
            return attribute_image_recolor_opa(obj, part, state, update, (lv_opa_t)var, val);
        case ATTR_IMAGE_OPA:
            return attribute_image_opa(obj, part, state, update, (lv_opa_t)var, val);
        case ATTR_IMAGE_RECOLOR: {
            if(update) {
                lv_color32_t c;
                if(Parser::haspPayloadToColor(payload, c))
                    lv_obj_set_style_local_image_recolor(obj, part, state,
                                                         lv_color_make(c.ch.red, c.ch.green, c.ch.blue));
            } else {
                attr_out_color(obj, attr, lv_obj_get_style_image_recolor(obj, part));
            }
            return HASP_ATTR_TYPE_METHOD_OK;
        }

            /* Transition attributes */
            // Todo

        default:
            break;
    }

    // LOG_WARNING(TAG_ATTR, F(D_ATTRIBUTE_UNKNOWN " (%d)"), attr_p, attr_hash);
    // result = false;
    return HASP_ATTR_TYPE_NOT_FOUND;
}

static hasp_attribute_type_t hasp_process_arc_attribute(lv_obj_t* obj, uint16_t attr_hash, int32_t& val, bool update)
{
    // We already know it's a arc object
    switch(attr_hash) {
        case ATTR_TYPE:
            if(update)
                lv_arc_set_type(obj, val % 3);
            else
                val = lv_arc_get_type(obj);
            break;

        default:
            return HASP_ATTR_TYPE_NOT_FOUND;
    }

    return HASP_ATTR_TYPE_INT;
}

static hasp_attribute_type_t hasp_process_spinner_attribute(lv_obj_t* obj, uint16_t attr_hash, int32_t& val,
                                                            bool update)
{
    // We already know it's a spnner object
    switch(attr_hash) {
        case ATTR_TYPE:
            if(update)
                lv_spinner_set_type(obj, val % 3);
            else
                val = lv_spinner_get_type(obj);
            break;

        default:
            return HASP_ATTR_TYPE_NOT_FOUND;
    }

    return HASP_ATTR_TYPE_INT;
}

static hasp_attribute_type_t hasp_process_slider_attribute(lv_obj_t* obj, uint16_t attr_hash, int32_t& val, bool update)
{
    // We already know it's a slider object
    switch(attr_hash) {
        case ATTR_TYPE:
            if(update)
                lv_slider_set_type(obj, val % 3);
            else
                val = lv_slider_get_type(obj);
            break;

        default:
            return HASP_ATTR_TYPE_NOT_FOUND;
    }

    return HASP_ATTR_TYPE_INT;
}

static hasp_attribute_type_t hasp_process_lmeter_attribute(lv_obj_t* obj, uint16_t attr_hash, int32_t& val, bool update)
{
    // We already know it's a linemeter object
    uint16_t line_count = lv_linemeter_get_line_count(obj);
    uint16_t angle      = lv_linemeter_get_scale_angle(obj);

    switch(attr_hash) {
        case ATTR_TYPE:
            if(update)
                lv_linemeter_set_mirror(obj, !!val);
            else
                val = lv_linemeter_get_mirror(obj);
            break;

        case ATTR_LINE_COUNT:
            if(update)
                lv_linemeter_set_scale(obj, angle, val);
            else
                val = line_count;
            break;

        case ATTR_ANGLE:
            if(update)
                lv_linemeter_set_scale(obj, val, line_count);
            else
                val = angle;
            break;

        default:
            return HASP_ATTR_TYPE_NOT_FOUND;
    }

    return HASP_ATTR_TYPE_INT;
}

static hasp_attribute_type_t special_attribute_direction(lv_obj_t* obj, uint16_t attr_hash, int32_t& val, bool update)
{
    switch(obj_get_type(obj)) {

        case LV_HASP_DROPDOWN:
            if(update)
                lv_dropdown_set_dir(obj, (lv_dropdown_dir_t)val);
            else
                val = lv_dropdown_get_dir(obj);
            break;

        case LV_HASP_SPINNER:
            if(update)
                lv_spinner_set_dir(obj, (lv_spinner_dir_t)val);
            else
                val = lv_spinner_get_dir(obj);
            break;

        default:
            return HASP_ATTR_TYPE_NOT_FOUND;
    }

    return HASP_ATTR_TYPE_INT;
}

static hasp_attribute_type_t hasp_process_gauge_attribute(lv_obj_t* obj, uint16_t attr_hash, int32_t& val, bool update)
{
    // We already know it's a gauge object

    uint8_t label_count = lv_gauge_get_label_count(obj);
    uint16_t line_count = lv_gauge_get_line_count(obj);
    uint16_t angle      = lv_gauge_get_scale_angle(obj);

    switch(attr_hash) {
        case ATTR_CRITICAL_VALUE:
            if(update)
                lv_gauge_set_critical_value(obj, val);
            else
                val = lv_gauge_get_critical_value(obj);
            break;

        case ATTR_ANGLE:
            if(update)
                lv_gauge_set_scale(obj, val, line_count, label_count);
            else
                val = angle;
            break;

        case ATTR_LINE_COUNT:
            if(update)
                lv_gauge_set_scale(obj, angle, val, label_count);
            else
                val = line_count;
            break;

        case ATTR_LABEL_COUNT:
            if(update)
                lv_gauge_set_scale(obj, angle, line_count, val);
            else
                val = label_count;
            break;

        case ATTR_FORMAT:
            if(update) {
                // TODO : getter needed
                switch(val) {
                    case 1:
                        lv_gauge_set_formatter_cb(obj, gauge_format_10);
                        break;
                    case 2:
                        lv_gauge_set_formatter_cb(obj, gauge_format_100);
                        break;
                    case 3:
                        lv_gauge_set_formatter_cb(obj, gauge_format_1k);
                        break;
                    case 4:
                        lv_gauge_set_formatter_cb(obj, gauge_format_10k);
                        break;
                    default:
                        lv_gauge_set_formatter_cb(obj, NULL);
                }
            }
            break;

        default:
            return HASP_ATTR_TYPE_NOT_FOUND;
    }

    return HASP_ATTR_TYPE_INT;
}

// ##################### Common Attributes ########################################################

static hasp_attribute_type_t specific_page_attribute(lv_obj_t* obj, uint16_t attr_hash, int32_t& val, bool update)
{
    if(!obj_check_type(obj, LV_HASP_SCREEN)) return HASP_ATTR_TYPE_NOT_FOUND;

    uint8_t pageid;

    if(haspPages.get_id(obj, &pageid)) {
        switch(attr_hash) {
            case ATTR_NEXT:
                if(update)
                    haspPages.set_next(pageid, (uint8_t)val);
                else
                    val = haspPages.get_next(pageid);
                break;

            case ATTR_PREV:
                if(update)
                    haspPages.set_prev(pageid, (uint8_t)val);
                else
                    val = haspPages.get_prev(pageid);
                break;

            case ATTR_BACK:
                if(update)
                    haspPages.set_back(pageid, (uint8_t)val);
                else
                    val = haspPages.get_back(pageid);
                break;

            default:
                return HASP_ATTR_TYPE_NOT_FOUND;
        }
    }

    return HASP_ATTR_TYPE_INT;
}

template <typename T>
static inline bool do_attribute(T& list, lv_obj_t* obj, uint16_t attr_hash, int32_t& val, bool update)
{
    uint8_t obj_type = obj_get_type(obj);
    int count        = sizeof(list) / sizeof(list[0]);

    for(int i = 0; i < count; i++) {
        if(obj_type == list[i].obj_type && attr_hash == list[i].hash) {
            if(update)
                list[i].set(obj, val);
            else
                val = list[i].get(obj);
            return true;
        }
    }

    return false;
}

static hasp_attribute_type_t special_attribute_src(lv_obj_t* obj, const char* payload, char** text, bool update)
{
    if(!obj_check_type(obj, LV_HASP_IMAGE)) return HASP_ATTR_TYPE_NOT_FOUND;

    if(update) {

        if(payload != strstr_P(payload, PSTR("http://")) &&  // not start with http
           payload != strstr_P(payload, PSTR("https://"))) { // not start with https

            if(payload == strstr_P(payload, PSTR("L:"))) { // startsWith command/
                my_image_release_resources(obj);
                lv_img_set_src(obj, payload);

            } else if(payload == strstr_P(payload, PSTR("/littlefs/"))) { // startsWith command/
                char tempsrc[64] = "L:";
                strncpy(tempsrc + 2, payload + 10, sizeof(tempsrc) - 2);
                my_image_release_resources(obj);
                lv_img_set_src(obj, tempsrc);

            } else {
                char tempsrc[64] = LV_SYMBOL_DUMMY;
                strncpy(tempsrc + 3, payload, sizeof(tempsrc) - 3);
                my_image_release_resources(obj);
                lv_img_set_src(obj, tempsrc);
            }

        } else {
#if defined(ARDUINO) && defined(ARDUINO_ARCH_ESP32)
#if HASP_USE_WIFI > 0 || HASP_USE_ETHERNET > 0
            HTTPClient http;
            // http.begin(payload, (const char*)rootca_crt_bundle_start);
            http.begin(payload);
            http.setTimeout(5000);
            http.setConnectTimeout(5000);

            // const char* hdrs[] = {"Content-Type"};
            // size_t numhdrs     = sizeof(hdrs) / sizeof(char*);
            // http.collectHeaders(hdrs, numhdrs);

            int httpCode = http.GET();
            if(httpCode == HTTP_CODE_OK) {
                int total   = http.getSize();
                int url_len = strlen(payload) + 1;
                int buf_len = total;
                int dsc_len = sizeof(lv_img_dsc_t) + url_len;

                if(buf_len <= 8) { // header could not fit
                    LOG_ERROR(TAG_ATTR, "img data size is too small %d", buf_len);
                    return HASP_ATTR_TYPE_STR;
                }

                Stream* stream = http.getStreamPtr();
                if(!stream) {
                    LOG_ERROR(TAG_ATTR, "failed to get http data stream");
                    return HASP_ATTR_TYPE_STR;
                }

                lv_img_dsc_t* img_dsc = (lv_img_dsc_t*)lv_mem_alloc(dsc_len);
                if(!img_dsc) {
                    LOG_ERROR(TAG_ATTR, "img header creation failed %d", dsc_len);
                    return HASP_ATTR_TYPE_STR;
                }
                char* url = ((char*)img_dsc) + sizeof(lv_img_dsc_t);

                uint8_t* img_buf_start = (uint8_t*)(buf_len > 0 ? hasp_malloc(buf_len) : NULL);
                uint8_t* img_buf_pos   = img_buf_start;
                if(!img_buf_start) {
                    lv_mem_free(img_dsc); // destroy header too
                    LOG_ERROR(TAG_ATTR, "img buffer creation failed %d", buf_len);
                    return HASP_ATTR_TYPE_STR;
                }

                // LOG_VERBOSE(TAG_ATTR, "img buffers created of %d and %d bytes", dsc_len, buf_len);
                LOG_VERBOSE(TAG_ATTR, "img Content-Type: %s", http.header((size_t)0).c_str());

                // Initialize the buffers
                memset(img_buf_start, 0, buf_len);           // empty data buffer
                memset(img_dsc, 0, dsc_len);                 // empty img descriptor + url
                strncpy(url, payload, url_len);              // store the url behind the img_dsc data
                img_dsc->data               = img_buf_start; // store pointer to the start of the data buffer
                img_dsc->header.always_zero = 0;

                // LOG_DEBUG(TAG_ATTR, "%s %d %x == %x", __FILE__, __LINE__, img_dsc->data, img_buf);

                int read = 0;
                while(http.connected() && (stream->available() < 8) && read < 250) {
                    delay(1); // wait for header
                    read++;   // time-out check
                }

                // Read image header
                read                      = 0;
                const uint8_t png_magic[] = {0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a};
                if(stream->available() >= 8) {
                    int c = stream->readBytes(img_buf_pos, 8); // don't read too far

                    if(!memcmp(png_magic, img_buf_pos, sizeof(png_magic))) {
                        // PNG image, keep all data and advance buffer
                        LOG_VERBOSE(TAG_ATTR, D_BULLET "PNG HEADER: %d bytes read=%d buf_len=%d", c, read, buf_len);
                        img_buf_pos += c;
                        buf_len -= c;
                        read += c;
                    } else {
                        // BIN format, copy the header
                        lv_img_header_t* header     = (lv_img_header_t*)img_buf_pos;
                        img_dsc->header.always_zero = 0;
                        img_dsc->header.w           = header->w;
                        img_dsc->header.h           = header->h;
                        img_dsc->header.cf          = header->cf;

                        LOG_VERBOSE(TAG_ATTR, D_BULLET "BIN image: w=%d h=%d cf=%d len=%d", img_dsc->header.w,
                                    img_dsc->header.h, img_dsc->header.cf, img_dsc->data_size);
                        img_buf_pos += sizeof(lv_img_header_t);
                        // shift remainder of 8 data-bytes to the start of the buffer
                        memcpy(img_buf_start, img_buf_pos, 8U - sizeof(lv_img_header_t));
                        buf_len -= c;
                        read += c;
                    }
                } else {
                    // disconnected
                    hasp_free(img_buf_start);
                    lv_mem_free(img_dsc); // destroy header too
                    LOG_ERROR(TAG_ATTR, "img header read failed %d", buf_len);
                    return HASP_ATTR_TYPE_STR;
                }

                // Read image data
                while(http.connected() && (buf_len > 0)) {
                    if(size_t size = stream->available()) {
                        int c = stream->readBytes(img_buf_pos, size > buf_len ? buf_len : size); // don't read too far
                        // LOG_DEBUG(TAG_ATTR, "%s %d %x -> %x", __FILE__, __LINE__, img_dsc->data, img_buf);
                        img_buf_pos += c;
                        buf_len -= c;
                        // LOG_DEBUG(TAG_ATTR, D_BULLET "IMG DATA: %d bytes read=%d buf_len=%d", c, read, buf_len);
                        read += c;
                    } else {
                        delay(1); // wait for data
                    }
                }

                // disconnected
                if(buf_len > 0) {
                    hasp_free(img_buf_start);
                    lv_mem_free(img_dsc); // destroy header too
                    LOG_ERROR(TAG_ATTR, "img data read failed %d", buf_len);
                    return HASP_ATTR_TYPE_STR;
                }

                LOG_VERBOSE(TAG_ATTR, D_BULLET "HTTP TOTAL READ: %d bytes, %d buffered", read,
                            img_buf_pos - img_buf_start);

                if(total > 24 && !memcmp(png_magic, img_dsc->data, sizeof(png_magic))) {
                    // PNG format, get image size from header
                    img_dsc->header.always_zero = 0;
                    img_dsc->header.w           = img_buf_start[19] + (img_buf_start[18] << 8);
                    img_dsc->header.h           = img_buf_start[23] + (img_buf_start[22] << 8);
                    img_dsc->data_size          = img_buf_pos - img_buf_start; // end of buffer - start of buffer
                    img_dsc->header.cf          = LV_IMG_CF_RAW_ALPHA;
                    LOG_VERBOSE(TAG_ATTR, D_BULLET "PNG image: w=%d h=%d cf=%d len=%d", img_dsc->header.w,
                                img_dsc->header.h, img_dsc->header.cf, img_dsc->data_size);
                } else {
                    img_dsc->data_size = img_buf_pos - img_buf_start - sizeof(lv_img_header_t); // end buf - start buf
                    LOG_VERBOSE(TAG_ATTR, D_BULLET "BIN image: w=%d h=%d cf=%d len=%d", img_dsc->header.w,
                                img_dsc->header.h, img_dsc->header.cf, img_dsc->data_size);
                }

                my_image_release_resources(obj);
                lv_img_set_src(obj, img_dsc);
                // LOG_DEBUG(TAG_ATTR, "%s %d %x -> %x", __FILE__, __LINE__, img_buf_start, img_buf_start_pos);

            } else {
                LOG_WARNING(TAG_ATTR, "HTTP result %d", httpCode);
            }
            http.end();
#endif // HASP_USE_NETWORK
#endif // ESP32
        }
    } else {
        const void* src = lv_img_get_src(obj);
        switch(lv_img_src_get_type(src)) {
            case LV_IMG_SRC_FILE:
                LOG_DEBUG(TAG_ATTR, F("%s %d"), __FILE__, __LINE__);
                *text = (char*)lv_img_get_file_name(obj);
                break;
            case LV_IMG_SRC_SYMBOL:
                LOG_DEBUG(TAG_ATTR, F("%s %d"), __FILE__, __LINE__);
                *text = (char*)src + strlen(LV_SYMBOL_DUMMY);
                break;
            case LV_IMG_SRC_VARIABLE:
                LOG_DEBUG(TAG_ATTR, F("%s %d"), __FILE__, __LINE__);
                *text = (char*)src + sizeof(lv_img_dsc_t);
                break;
            default:
                LOG_DEBUG(TAG_ATTR, F("%s %d"), __FILE__, __LINE__);
        }
    }
    return HASP_ATTR_TYPE_STR;
}

static hasp_attribute_type_t attribute_common_align(lv_obj_t* obj, const char* attr, const char* payload, char** text,
                                                    bool update)
{
    lv_label_align_t val = 0;
    lv_align_t pos       = LV_ALIGN_CENTER;

    if(update) {
        if(!strcasecmp_P(payload, PSTR("left"))) {
            val = LV_LABEL_ALIGN_LEFT;
            //  pos = LV_ALIGN_IN_LEFT_MID;
        } else if(!strcasecmp_P(payload, PSTR("right"))) {
            val = LV_LABEL_ALIGN_RIGHT;
            // pos = LV_ALIGN_IN_RIGHT_MID;
        } else if(!strcasecmp_P(payload, PSTR("center"))) {
            val = LV_LABEL_ALIGN_CENTER;
        } else if(!strcasecmp_P(payload, PSTR("auto"))) {
            val = LV_LABEL_ALIGN_AUTO;
        } else {
            val = atoi(payload);
            if(val > LV_LABEL_ALIGN_AUTO) return HASP_ATTR_TYPE_ALIGN_INVALID;
        }
    }

    switch(obj_get_type(obj)) {
        case LV_HASP_BUTTON: {
            lv_obj_t* label = FindButtonLabel(obj);
            if(label) {
                if(update) {
                    //  lv_obj_align(label, NULL, pos, 0, 0);
                    lv_label_set_align(label, val);
                } else
                    val = lv_label_get_align(label);
            } else {
                return HASP_ATTR_TYPE_NOT_FOUND; // not found
            }
            break;
        }
        case LV_HASP_BTNMATRIX:
            if(update)
                lv_btnmatrix_set_align(obj, val);
            else
                val = lv_btnmatrix_get_align(obj);
            break;

        case LV_HASP_LABEL:
            if(update)
                lv_label_set_align(obj, val);
            else
                val = lv_label_get_align(obj);
            break;

        case LV_HASP_TEXTAREA:
            if(update)
                lv_textarea_set_text_align(obj, val);
            else
                val = my_textarea_get_text_align(obj);
            break;

        case LV_HASP_ROLLER:
            if(update)
                lv_roller_set_align(obj, val);
            else
                val = lv_roller_get_align(obj);
            break;

        default:
            return HASP_ATTR_TYPE_NOT_FOUND; // not found
    }

    // return values
    switch(val) {
        case LV_LABEL_ALIGN_AUTO:
            strcpy_P(*text, PSTR("auto"));
            break;
        case LV_LABEL_ALIGN_CENTER:
            strcpy_P(*text, PSTR("center"));
            break;
        case LV_LABEL_ALIGN_RIGHT:
            strcpy_P(*text, PSTR("right"));
            break;
        default:
            strcpy_P(*text, PSTR("left"));
    }

    return HASP_ATTR_TYPE_STR;
}

static hasp_attribute_type_t attribute_common_mode(lv_obj_t* obj, const char* payload, char** text, int32_t& val,
                                                   bool update)
{
    switch(obj_get_type(obj)) {
        case LV_HASP_BUTTON: {
            lv_obj_t* label = FindButtonLabel(obj);
            if(label) {
                hasp_attribute_type_t ret = hasp_process_label_long_mode(label, payload, text, update);
                lv_obj_set_width(label, lv_obj_get_width(obj));
                return ret;
            }
            break; // not found
        }

        case LV_HASP_LABEL:
            return hasp_process_label_long_mode(obj, payload, text, update);

        case LV_HASP_ROLLER:
            if(update) {
                val = Parser::is_true(payload);
                lv_roller_set_options(obj, lv_roller_get_options(obj), (lv_roller_mode_t)val);
            } else {
                lv_roller_ext_t* ext = (lv_roller_ext_t*)lv_obj_get_ext_attr(obj);
                val                  = ext->mode;
            }
            return HASP_ATTR_TYPE_INT;

        case LV_HASP_CPICKER:
            return hasp_process_cpicker_mode(obj, payload, text, update);

        default:
            break; // not found
    }

    return HASP_ATTR_TYPE_NOT_FOUND;
}

static hasp_attribute_type_t attribute_common_tag(lv_obj_t* obj, uint16_t attr_hash, const char* payload, char** text,
                                                  bool update)
{
    switch(attr_hash) {
        case ATTR_TAG:
            if(update) {
                my_obj_set_tag(obj, payload);
            } else {
                if(my_obj_get_tag(obj)) {
                    *text = (char*)my_obj_get_tag(obj);
                } else {
                    strcpy_P(*text, "null"); // TODO : Literal String
                }
            }
            break; // attribute_found

        case ATTR_ACTION:
            if(update) {
                my_obj_set_action(obj, payload);
            } else {
                if(my_obj_get_action(obj)) {
                    *text = (char*)my_obj_get_action(obj);
                } else {
                    strcpy_P(*text, "null"); // TODO : Literal String
                }
            }
            break; // attribute_found

        case ATTR_SWIPE:
            if(update) {
                my_obj_set_swipe(obj, payload);
            } else {
                if(my_obj_get_swipe(obj)) {
                    *text = (char*)my_obj_get_swipe(obj);
                } else {
                    strcpy_P(*text, "null"); // TODO : Literal String
                }
            }
            break; // attribute_found

        default:
            return HASP_ATTR_TYPE_NOT_FOUND;
    }

    return HASP_ATTR_TYPE_JSON;
}
static hasp_attribute_type_t attribute_common_json(lv_obj_t* obj, uint16_t attr_hash, const char* payload, char** text,
                                                   bool update)
{
    switch(attr_hash) {
        case ATTR_JSONL: {
            DeserializationError jsonError = DeserializationError::Ok;

            if(update) {

                // StaticJsonDocument<1024> json;
                size_t maxsize = (512u + JSON_OBJECT_SIZE(25));
                DynamicJsonDocument json(maxsize);

                // Note: Deserialization can to be (char *) so the objects WILL NOT be copied
                // this uses less memory since the data is already copied from the mqtt receive buffer and cannot
                // get overwritten by the send buffer !!
                DeserializationError jsonError = deserializeJson(json, (char*)payload);
                json.shrinkToFit();

                if(jsonError == DeserializationError::Ok) {
                    // Make sure we have a valid JsonObject to start from
                    if(JsonObject keys = json.as<JsonObject>()) {
                        hasp_parse_json_attributes(obj, keys); // json is valid object, cast as a JsonObject
                    } else {
                        LOG_DEBUG(TAG_ATTR, "%s %d", __FILE__, __LINE__);
                        jsonError = DeserializationError::InvalidInput;
                    }
                } else {
                    LOG_DEBUG(TAG_ATTR, "%s %d", __FILE__, __LINE__);
                    jsonError = DeserializationError::IncompleteInput;
                }
            }
            LOG_DEBUG(TAG_ATTR, "%s %d", __FILE__, __LINE__);

            if(jsonError) { // Couldn't parse incoming JSON object
                dispatch_json_error(TAG_ATTR, jsonError);
                return HASP_ATTR_TYPE_JSON_INVALID;
            }

            break; // attribute_found
        }

        default:
            return HASP_ATTR_TYPE_NOT_FOUND;
    }

    return HASP_ATTR_TYPE_METHOD_OK;
}

static hasp_attribute_type_t attribute_common_text(lv_obj_t* obj, uint16_t attr_hash, const char* payload, char** text,
                                                   bool update)
{
    uint8_t obj_type = obj_get_type(obj);

    hasp_attr_update_char_const_t list[] = {
        {LV_HASP_BUTTON, ATTR_TEXT, my_btn_set_text, my_btn_get_text},
        {LV_HASP_LABEL, ATTR_TEXT, my_label_set_text, my_label_get_text},
        {LV_HASP_LABEL, ATTR_TEMPLATE, my_obj_set_template, my_obj_get_template},
        {LV_HASP_CHECKBOX, ATTR_TEXT, lv_checkbox_set_text, lv_checkbox_get_text},
        {LV_HASP_DROPDOWN, ATTR_TEXT, my_dropdown_set_text, my_dropdown_get_text},
        {LV_HASP_TABVIEW, ATTR_TEXT, my_tabview_set_text, my_tabview_get_text},
        {LV_HASP_TEXTAREA, ATTR_TEXT, lv_textarea_set_text, lv_textarea_get_text},
        {LV_HASP_TAB, ATTR_TEXT, my_tab_set_text, my_tab_get_text},
#if LV_USE_WIN != 0
        {LV_HASP_WINDOW, ATTR_TEXT, lv_win_set_title, lv_win_get_title},
#endif
#if HASP_USE_QRCODE > 0
        {LV_HASP_QRCODE, ATTR_TEXT, my_qrcode_set_text, my_qrcode_get_text},
#endif
        {LV_HASP_MSGBOX, ATTR_TEXT, my_msgbox_set_text, lv_msgbox_get_text}
    };

    for(int i = 0; i < sizeof(list) / sizeof(list[0]); i++) {
        if(obj_type == list[i].obj_type && attr_hash == list[i].hash) {
            if(update)
                list[i].set(obj, payload);
            else
                *text = (char*)list[i].get(obj);

            return HASP_ATTR_TYPE_STR;
        }
    }

    if(attr_hash == ATTR_TEXT || attr_hash == ATTR_TXT) {
        // Special cases
        // {LV_HASP_DROPDOWN, set_text_not_implemented, my_dropdown_get_text},
        // {LV_HASP_ROLLER, set_text_not_implemented, my_roller_get_text},
        switch(obj_get_type(obj)) {
            case LV_HASP_DROPDOWN: {
                lv_dropdown_get_selected_str(obj, *text, 128);
                if(update) return HASP_ATTR_TYPE_STR_READONLY;
                break;
            }

            case LV_HASP_ROLLER: {
                lv_roller_get_selected_str(obj, *text, 128);
                if(update) return HASP_ATTR_TYPE_STR_READONLY;
                break;
            }

            default:
                return HASP_ATTR_TYPE_NOT_FOUND;
        }
    }

#if USE_OBJ_ALIAS > 0
    if(attr_hash == ATTR_ALIAS) {
        if(update) {
            my_obj_set_alias(obj, payload);
        } else {
            *text = (char*)my_obj_get_alias(obj);
        }
    }
#endif  // #if USE_OBJ_ALIAS > 0

    return HASP_ATTR_TYPE_STR;
}

static hasp_attribute_type_t specific_options_attribute(lv_obj_t* obj, const char* payload, char** text, bool update)
{
    switch(obj_get_type(obj)) {
        case LV_HASP_DROPDOWN:
            if(update) {
                lv_dropdown_set_options(obj, payload);
                lv_obj_invalidate(obj); // otherwise it won't refresh
            } else {
                *text = (char*)lv_dropdown_get_options(obj);
            }
            return hasp_attribute_type_t::HASP_ATTR_TYPE_STR;
        case LV_HASP_ROLLER:
            if(update) {
                lv_roller_ext_t* ext = (lv_roller_ext_t*)lv_obj_get_ext_attr(obj);
                lv_roller_set_options(obj, payload, ext->mode);
            } else {
                *text = (char*)lv_roller_get_options(obj);
            }
            return hasp_attribute_type_t::HASP_ATTR_TYPE_STR;
        case LV_HASP_BTNMATRIX:
            if(update) {
                my_btnmatrix_set_map(obj, payload);
            } else {
                strcpy_P(*text, "Not implemented"); // TODO : Literal String
            }
            return hasp_attribute_type_t::HASP_ATTR_TYPE_STR;
        case LV_HASP_MSGBOX:
            if(update) {
                my_msgbox_set_map(obj, payload);
            } else {
                strcpy_P(*text, "Not implemented"); // TODO : Literal String
            }
            return hasp_attribute_type_t::HASP_ATTR_TYPE_STR;
        case LV_HASP_LIST:
            if(update) {
                my_list_set_options(obj, payload);
            } else {
                strcpy_P(*text, "Not implemented"); // TODO : Literal String
            }
            return hasp_attribute_type_t::HASP_ATTR_TYPE_STR;
        default:
            break; // not found
    }

    return hasp_attribute_type_t::HASP_ATTR_TYPE_NOT_FOUND;
}

static hasp_attribute_type_t specific_bool_attribute(lv_obj_t* obj, uint16_t attr_hash, int32_t& val, bool update)
{
    { // bool
        hasp_attr_update_bool_const_t list[] = {
            {LV_HASP_ARC, ATTR_ADJUSTABLE, my_arc_set_adjustable, my_arc_get_adjustable},
            {LV_HASP_BTNMATRIX, ATTR_ONE_CHECK, lv_btnmatrix_set_one_check, lv_btnmatrix_get_one_check},
            {LV_HASP_LINE, ATTR_Y_INVERT, lv_line_set_y_invert, lv_line_get_y_invert},
            {LV_HASP_LINE, ATTR_AUTO_SIZE, lv_line_set_auto_size, lv_line_get_auto_size},
            {LV_HASP_IMAGE, ATTR_AUTO_SIZE, lv_img_set_auto_size, lv_img_get_auto_size}};
        if(do_attribute(list, obj, attr_hash, val, update)) return HASP_ATTR_TYPE_BOOL;
    }

    { // bool but obj is not const
        hasp_attr_update_bool_t list[] = {
            {LV_HASP_DROPDOWN, ATTR_SHOW_SELECTED, lv_dropdown_set_show_selected, lv_dropdown_get_show_selected},
            {LV_HASP_IMAGE, ATTR_ANTIALIAS, lv_img_set_antialias, lv_img_get_antialias},
            {LV_HASP_CPICKER, ATTR_MODE_FIXED, lv_cpicker_set_color_mode_fixed, lv_cpicker_get_color_mode_fixed}};
        if(do_attribute(list, obj, attr_hash, val, update)) return HASP_ATTR_TYPE_BOOL;
    }

    return HASP_ATTR_TYPE_NOT_FOUND;
}

static hasp_attribute_type_t specific_coord_attribute(lv_obj_t* obj, uint16_t attr_hash, int32_t& val, bool update)
{
    { // lv_coord_t
        hasp_attr_update_lv_coord_t list[] = {
            {LV_HASP_IMAGE, ATTR_OFFSET_X, lv_img_set_offset_x, lv_img_get_offset_x},
            {LV_HASP_IMAGE, ATTR_OFFSET_Y, lv_img_set_offset_y, lv_img_get_offset_y},
            {LV_HASP_IMAGE, ATTR_PIVOT_X, my_img_set_pivot_x, my_img_get_pivot_x},
            {LV_HASP_IMAGE, ATTR_PIVOT_Y, my_img_set_pivot_y, my_img_get_pivot_y},
            {LV_HASP_DROPDOWN, ATTR_MAX_HEIGHT, lv_dropdown_set_max_height, my_dropdown_get_max_height}};
        if(do_attribute(list, obj, attr_hash, val, update)) return HASP_ATTR_TYPE_INT;
    }

    return HASP_ATTR_TYPE_NOT_FOUND;
}

static hasp_attribute_type_t specific_int_attribute(lv_obj_t* obj, uint16_t attr_hash, int32_t& val, bool update)
{
    { // uint16_t
        hasp_attr_update_uint16_const_t list[] = {
            {LV_HASP_MSGBOX, ATTR_AUTO_CLOSE, lv_msgbox_start_auto_close, my_msgbox_stop_auto_close},
            {LV_HASP_SPINNER, ATTR_SPEED, lv_spinner_set_spin_time, lv_spinner_get_spin_time},
            {LV_HASP_BAR, ATTR_ANIM_TIME, lv_bar_set_anim_time, lv_bar_get_anim_time},
            {LV_HASP_SWITCH, ATTR_ANIM_TIME, lv_switch_set_anim_time, lv_switch_get_anim_time},
            {LV_HASP_LIST, ATTR_ANIM_TIME, lv_list_set_anim_time, lv_list_get_anim_time},
            {LV_HASP_MSGBOX, ATTR_ANIM_TIME, lv_msgbox_set_anim_time, lv_msgbox_get_anim_time},
            {LV_HASP_ROLLER, ATTR_ANIM_TIME, lv_roller_set_anim_time, lv_roller_get_anim_time},
            {LV_HASP_TABVIEW, ATTR_ANIM_TIME, lv_tabview_set_anim_time, lv_tabview_get_anim_time},
#if LVGL_VERSION_MAJOR == 7 && LV_USE_PAGE
            {LV_HASP_PAGE, ATTR_ANIM_TIME, lv_page_set_anim_time, lv_page_get_anim_time},
#endif
#if LV_USE_WINDOW > 0
            {LV_HASP_WINDOW, ATTR_ANIM_TIME, lv_win_set_anim_time, lv_win_get_anim_time},
#endif
            {LV_HASP_LABEL, ATTR_ANIM_SPEED, lv_label_set_anim_speed, lv_label_get_anim_speed}
        };
        if(do_attribute(list, obj, attr_hash, val, update)) return HASP_ATTR_TYPE_INT;
    }

    { // uint16_t, but getter is not const
        hasp_attr_update_uint16_t list[] = {
            {LV_HASP_ARC, ATTR_ROTATION, lv_arc_set_rotation, my_arc_get_rotation},
            {LV_HASP_ARC, ATTR_START_ANGLE, lv_arc_set_bg_start_angle, lv_arc_get_bg_angle_start},
            {LV_HASP_ARC, ATTR_END_ANGLE, lv_arc_set_bg_end_angle, lv_arc_get_bg_angle_end},
            {LV_HASP_ARC, ATTR_START_ANGLE1, lv_arc_set_start_angle, lv_arc_get_angle_start},
            {LV_HASP_ARC, ATTR_END_ANGLE1, lv_arc_set_end_angle, lv_arc_get_angle_end},
            {LV_HASP_LINEMETER, ATTR_ROTATION, lv_linemeter_set_angle_offset, lv_linemeter_get_angle_offset},
            {LV_HASP_IMAGE, ATTR_ZOOM, lv_img_set_zoom, lv_img_get_zoom},
            {LV_HASP_GAUGE, ATTR_ROTATION, lv_gauge_set_angle_offset, lv_gauge_get_angle_offset},
#if LV_USE_TABLE > 0
            {LV_HASP_TABLE, ATTR_COLS, lv_table_set_col_cnt, lv_table_get_col_cnt},
            {LV_HASP_TABLE, ATTR_ROWS, lv_table_set_row_cnt, lv_table_get_row_cnt},
#endif
#if LV_USE_TILEVIEW > 0
            {LV_HASP_TILEVIEW, ATTR_ANIM_TIME, lv_tileview_set_anim_time, lv_tileview_get_anim_time}
#endif
            {LV_HASP_SLIDER, ATTR_ANIM_TIME, lv_slider_set_anim_time, lv_slider_get_anim_time},
        };
        if(do_attribute(list, obj, attr_hash, val, update)) return HASP_ATTR_TYPE_INT;
    }

    { // lv_anim_value_t
        hasp_attr_update_lv_anim_value_const_t list[] = {
            {LV_HASP_SPINNER, ATTR_ANGLE, lv_spinner_set_arc_length, lv_spinner_get_arc_length}};
        if(do_attribute(list, obj, attr_hash, val, update)) return HASP_ATTR_TYPE_INT;
    }

    { // int16_t
        hasp_attr_update_int16_const_t list[] = {
            {LV_HASP_BAR, ATTR_START_VALUE, my_bar_set_start_value, lv_bar_get_start_value},
            {LV_HASP_SLIDER, ATTR_START_VALUE, my_slider_set_left_value, lv_slider_get_left_value}};
        if(do_attribute(list, obj, attr_hash, val, update)) return HASP_ATTR_TYPE_INT;
    }

    { // uint8_t
        hasp_attr_update_uint8_const_t list[] = {
            {LV_HASP_ROLLER, ATTR_ROWS, lv_roller_set_visible_row_count, my_roller_get_visible_row_count}};
        if(do_attribute(list, obj, attr_hash, val, update)) return HASP_ATTR_TYPE_INT;
    }

    if(obj_check_type(obj, LV_HASP_TABVIEW)) {
        switch(attr_hash) {
            case ATTR_COUNT:
                val = lv_tabview_get_tab_count(obj);
                if(update)
                    return HASP_ATTR_TYPE_INT_READONLY;
                else
                    return HASP_ATTR_TYPE_INT;

            case ATTR_BTN_POS:
                if(update) {
                    lv_tabview_set_btns_pos(obj, val);
                } else {
                    val = lv_tabview_get_btns_pos(obj);
                }
                return HASP_ATTR_TYPE_INT;
        }
    }

    if(obj_check_type(obj, LV_HASP_IMAGE)) {
        switch(attr_hash) {
            case ATTR_ANGLE:
                if(update) {
                    lv_img_set_angle(obj, val);
                } else {
                    val = lv_img_get_angle(obj);
                }
                return HASP_ATTR_TYPE_INT;
        }
    }

    return HASP_ATTR_TYPE_NOT_FOUND;
}

static bool my_obj_get_range(lv_obj_t* obj, int32_t& min, int32_t& max)
{
    min = 0;
    max = 1;

    switch(obj_get_type(obj)) {
        case LV_HASP_BUTTON:
            if(!lv_btn_get_checkable(obj)) {
                return false; // not checkable
            }
        case LV_HASP_CHECKBOX:
        case LV_HASP_SWITCH:
            // default min=0 and max=1
            break;

        case LV_HASP_LED:
            min = 0;
            max = 255;
            break;

        case LV_HASP_LINEMETER:
            min = lv_linemeter_get_min_value(obj);
            max = lv_linemeter_get_max_value(obj);
            break;

        case LV_HASP_SLIDER:
            min = lv_slider_get_min_value(obj);
            max = lv_slider_get_max_value(obj);
            break;

        case LV_HASP_GAUGE:
            min = lv_gauge_get_min_value(obj);
            max = lv_gauge_get_max_value(obj);
            break;

        case LV_HASP_ARC:
            min = lv_arc_get_min_value(obj);
            max = lv_arc_get_max_value(obj);
            break;

        case LV_HASP_BAR:
            min = lv_bar_get_min_value(obj);
            max = lv_bar_get_max_value(obj);
            break;

        case LV_HASP_TABVIEW:
            min = 0;
            max = lv_tabview_get_tab_count(obj) - 1;
            if(max == 0) return false; // only one tab available
            break;

#if LV_USE_CHART > 0
        case LV_HASP_CHART:
            min = my_chart_get_min_value(obj);
            max = my_chart_get_max_value(obj);
            break;
#endif

#if LV_USE_SPINBOX > 0
        case LV_HASP_SPINBOX:
            min = my_spinbox_get_min_value(obj);
            max = my_spinbox_get_max_value(obj);
            break;
#endif

        case LV_HASP_DROPDOWN:
        case LV_HASP_ROLLER:
            return false; // not supported yet

        default:
            return false;
    }
    return true;
}

static hasp_attribute_type_t attribute_common_val(lv_obj_t* obj, int32_t& val, bool update)
{
    switch(obj_get_type(obj)) {

        case LV_HASP_BUTTON:
            if(lv_btn_get_checkable(obj)) {
                if(update) {
                    if(val)
                        lv_obj_add_state(obj, LV_STATE_CHECKED);
                    else
                        lv_obj_clear_state(obj, LV_STATE_CHECKED);
                } else {
                    val = lv_obj_get_state(obj, LV_BTN_PART_MAIN) & LV_STATE_CHECKED;
                }
            } else {
                return HASP_ATTR_TYPE_NOT_FOUND; // not checkable
            }
            break;

        case LV_HASP_BTNMATRIX:
            if(!lv_btnmatrix_get_one_check(obj)) return HASP_ATTR_TYPE_NOT_FOUND;

            if(update) {
                if(val < -1 || val >= my_btnmatrix_get_count(obj)) {
                    LOG_WARNING(TAG_ATTR, F("Invalid index %d"), val);
                } else {
                    lv_btnmatrix_clear_btn_ctrl_all(obj, LV_BTNMATRIX_CTRL_CHECK_STATE);
                    lv_btnmatrix_ext_t* ext = (lv_btnmatrix_ext_t*)lv_obj_get_ext_attr(obj);
                    if(val == -1) {
                        ext->btn_id_act = LV_BTNMATRIX_BTN_NONE;
                    } else {
                        lv_btnmatrix_set_btn_ctrl(obj, val, LV_BTNMATRIX_CTRL_CHECK_STATE);
                        ext->btn_id_act = val;
                    }
                }

            } else {
                uint16_t btn_id_act = lv_btnmatrix_get_active_btn(obj);
                val                 = (btn_id_act == LV_BTNMATRIX_BTN_NONE) ? -1 : btn_id_act;
            }
            break;

        case LV_HASP_CHECKBOX:
            if(update)
                lv_checkbox_set_checked(obj, !!val);
            else
                val = lv_checkbox_is_checked(obj);
            break;

        case LV_HASP_SWITCH:
            if(update)
                !val ? lv_switch_off(obj, LV_ANIM_ON) : lv_switch_on(obj, LV_ANIM_ON);
            else
                val = lv_switch_get_state(obj);
            break;

        case LV_HASP_DROPDOWN:
            if(update)
                lv_dropdown_set_selected(obj, (uint16_t)val);
            else
                val = lv_dropdown_get_selected(obj);
            break;

        case LV_HASP_LINEMETER:
            if(update)
                lv_linemeter_set_value(obj, val);
            else
                val = lv_linemeter_get_value(obj);
            break;

        case LV_HASP_SLIDER:
            if(update)
                lv_slider_set_value(obj, val, LV_ANIM_ON);
            else
                val = lv_slider_get_value(obj);
            break;

        case LV_HASP_LED:
            if(update)
                lv_led_set_bright(obj, (uint8_t)val);
            else
                val = lv_led_get_bright(obj);
            break;

        case LV_HASP_ARC:
            if(update)
                lv_arc_set_value(obj, val);
            else
                val = lv_arc_get_value(obj);
            break;

        case LV_HASP_GAUGE:
            if(update)
                lv_gauge_set_value(obj, 0, val);
            else
                val = lv_gauge_get_value(obj, 0);
            break;

        case LV_HASP_ROLLER:
            if(update)
                lv_roller_set_selected(obj, (uint16_t)val, LV_ANIM_ON);
            else
                val = lv_roller_get_selected(obj);
            break;

        case LV_HASP_BAR:
            if(update)
                lv_bar_set_value(obj, val, LV_ANIM_ON);
            else
                val = lv_bar_get_value(obj);
            break;

#if LV_USE_SPINBOX > 0
        case LV_HASP_SPINBOX:
            if(update)
                lv_spinbox_set_value(obj, val);
            else
                val = lv_spinbox_get_value(obj);
            break;
#endif

        case LV_HASP_TABVIEW:
            if(update)
                lv_tabview_set_tab_act(obj, val, LV_ANIM_ON);
            else
                val = lv_tabview_get_tab_act(obj);
            break;

        case LV_HASP_CPICKER:
            if(update) {
                lv_cpicker_set_color(obj, lv_color_hex(val));
            } else {
                lv_color_t color = lv_cpicker_get_color(obj);
                val = lv_color_to32(color);                
            }
            break;

        default:
            return HASP_ATTR_TYPE_NOT_FOUND; // not found
    }

    return HASP_ATTR_TYPE_INT; // found
}

bool attribute_set_normalized_value(lv_obj_t* obj, hasp_update_value_t& value)
{
    if(value.min == value.max) return false; // would cause divide by zero error

    int32_t val;

    if (obj_get_type(obj) != LV_HASP_CPICKER) {
        int32_t min;
        int32_t max;
        if(!my_obj_get_range(obj, min, max)) return false; // range could not be determined

        // Limit the value between min and max, adjust if power = 0
        if(value.power == 0 || value.val <= value.min) {
            val = value.min;
        } else if(value.val >= value.max) {
            val = value.max;
        } else {
            val = value.val;
        }
    
        if(min == 0 && max == 1) {
            val = val != value.min; // Toggles are set to 0 when val = min, otherwise 1
        } else {
            val = map(val, value.min, value.max, min, max);
        }
    } else {
        val = value.val;
    }

    attribute_common_val(obj, val, true);
    return true;
}

static hasp_attribute_type_t attribute_common_range(lv_obj_t* obj, int32_t& val, bool update, bool set_min,
                                                    bool set_max)
{
    int32_t min;
    int32_t max;
    if(!my_obj_get_range(obj, min, max)) return HASP_ATTR_TYPE_RANGE_ERROR;

    switch(obj_get_type(obj)) {
        case LV_HASP_SLIDER:
            if(update && (set_min ? val : min) == (set_max ? val : max))
                return HASP_ATTR_TYPE_RANGE_ERROR; // prevent setting min=max
            if(update)
                lv_slider_set_range(obj, set_min ? val : min, set_max ? val : max);
            else
                val = set_min ? min : max;
            break;

        case LV_HASP_GAUGE:
            if(update && (set_min ? val : min) == (set_max ? val : max))
                return HASP_ATTR_TYPE_RANGE_ERROR; // prevent setting min=max
            if(update)
                lv_gauge_set_range(obj, set_min ? val : min, set_max ? val : max);
            else
                val = set_min ? min : max;
            break;

        case LV_HASP_ARC:
            if(update && (set_min ? val : min) == (set_max ? val : max))
                return HASP_ATTR_TYPE_RANGE_ERROR; // prevent setting min=max
            if(update)
                lv_arc_set_range(obj, set_min ? val : min, set_max ? val : max);
            else
                val = set_min ? min : max;
            break;

        case LV_HASP_BAR:
            if(update && (set_min ? val : min) == (set_max ? val : max))
                return HASP_ATTR_TYPE_RANGE_ERROR; // prevent setting min=max
            if(update)
                lv_bar_set_range(obj, set_min ? val : min, set_max ? val : max);
            else
                val = set_min ? min : max;
            break;

        case LV_HASP_LINEMETER:
            if(update && (set_min ? val : min) == (set_max ? val : max))
                return HASP_ATTR_TYPE_RANGE_ERROR; // prevent setting min=max
            if(update)
                lv_linemeter_set_range(obj, set_min ? val : min, set_max ? val : max);
            else
                val = set_min ? min : max;
            break;

#if LV_USE_CHART > 0
        case LV_HASP_CHART:
            if(update && (set_min ? val : min) == (set_max ? val : max))
                return HASP_ATTR_TYPE_RANGE_ERROR; // prevent setting  min=max
            if(update)
                lv_chart_set_range(obj, set_min ? val : min, set_max ? val : max);
            else
                val = set_min ? min : max;
            break;
#endif

#if LV_USE_SPINBOX > 0
        case LV_HASP_SPINBOX:
            if(update && (set_min ? val : min) == (set_max ? val : max))
                return HASP_ATTR_TYPE_RANGE_ERROR; // prevent setting min=max
            if(update)
                lv_spinbox_set_range(obj, set_min ? val : min, set_max ? val : max);
            else
                val = set_min ? min : max;
            break;
#endif

        default:
            return HASP_ATTR_TYPE_NOT_FOUND;
    }

    return HASP_ATTR_TYPE_INT;
}

// ##################### Default Attributes ########################################################

/**
 * Execute the method of an object
 * @param obj lv_obj_t*: the object to get/set the attribute
 * @param attr_hash uint16_t: the hashed attribute value
 * @param attr char*: the attribute name (without leading ".")
 * @param payload char*: the parameters for the method
 * @note setting a value won't return anything, getting will dispatch the value
 */
static hasp_attribute_type_t attribute_common_method(lv_obj_t* obj, uint16_t attr_hash, const char* attr,
                                                     const char* payload)
{
    switch(attr_hash) {
        case ATTR_DELETE:
            if(!lv_obj_get_parent(obj)) return HASP_ATTR_TYPE_METHOD_INVALID_FOR_PAGE;
            lv_obj_del_async(obj);
            break;

        case ATTR_CLEAR:
            lv_obj_clean(obj);
            break;

        case ATTR_TO_FRONT:
            if(!lv_obj_get_parent(obj)) return HASP_ATTR_TYPE_METHOD_INVALID_FOR_PAGE;
            lv_obj_move_foreground(obj);
            break;

        case ATTR_TO_BACK:
            if(!lv_obj_get_parent(obj)) return HASP_ATTR_TYPE_METHOD_INVALID_FOR_PAGE;
            lv_obj_move_background(obj);
            break;

        case ATTR_OPEN:
        case ATTR_CLOSE:
            if(!obj_check_type(obj, LV_HASP_DROPDOWN)) return HASP_ATTR_TYPE_NOT_FOUND;
            event_reset_last_value_sent(); // Prevents manual selection bug because no manual 'down' occurred
            if(attr_hash == ATTR_OPEN)
                lv_dropdown_open(obj);
            else
                lv_dropdown_close(obj);
            break;

        default:
            return HASP_ATTR_TYPE_NOT_FOUND; // attribute_not found
    }

    return HASP_ATTR_TYPE_METHOD_OK;
}

/**
 * Change or Retrieve the value of the attribute of an object
 * @param obj lv_obj_t*: the object to get/set the attribute
 * @param attr_hash uint16_t: the hashed attribute to get/set
 * @param val uint32_t: the new value of the attribute
 * @param update  bool: change/set the value if true, dispatch/get value if false
 * @note setting a value won't return anything, getting will dispatch the value
 */
static hasp_attribute_type_t attribute_common_int(lv_obj_t* obj, uint16_t attr_hash, int32_t& val, bool update)
{
    switch(attr_hash) {
        case ATTR_ID:
            if(update)
                obj->user_data.id = (uint8_t)val;
            else
                val = obj->user_data.id;
            break; // attribute_found

        case ATTR_GROUPID:
            if(update)
                obj->user_data.groupid = (uint8_t)val;
            else
                val = obj->user_data.groupid;
            break; // attribute_found

            // case ATTR_TRANSITION:
            //     if(update)
            //         obj->user_data.transitionid = (uint8_t)val;
            //     else
            //         val = obj->user_data.transitionid;
            //     break; // attribute_found

        case ATTR_OBJID:
            if(update && val != obj->user_data.objid) return HASP_ATTR_TYPE_INT_READONLY;
            val = obj->user_data.objid;
            break; // attribute_found

        case ATTR_X:
            if(update)
                lv_obj_set_x(obj, val);
            else
                val = lv_obj_get_x(obj);
            break; // attribute_found

        case ATTR_Y:
            if(update)
                lv_obj_set_y(obj, val);
            else
                val = lv_obj_get_y(obj);
            break; // attribute_found

        case ATTR_W:
            if(update) {
                lv_obj_set_width(obj, val);
                if(obj_check_type(obj, LV_HASP_CPICKER)) {
#if LVGL_VERSION_MAJOR == 7
                    lv_cpicker_set_type(obj, lv_obj_get_width(obj) == lv_obj_get_height(obj) ? LV_CPICKER_TYPE_DISC
                                                                                             : LV_CPICKER_TYPE_RECT);
#endif
                }
            } else {
                val = lv_obj_get_width(obj);
            }
            break; // attribute_found

        case ATTR_H:
            if(update) {
                lv_obj_set_height(obj, val);
                if(obj_check_type(obj, LV_HASP_CPICKER)) {
#if LVGL_VERSION_MAJOR == 7
                    lv_cpicker_set_type(obj, lv_obj_get_width(obj) == lv_obj_get_height(obj) ? LV_CPICKER_TYPE_DISC
                                                                                             : LV_CPICKER_TYPE_RECT);
#endif
                }
            } else {
                val = lv_obj_get_height(obj);
            }
            break; // attribute_found

        case ATTR_OPACITY:
            if(update)
                lv_obj_set_style_local_opa_scale(obj, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, val);
            else
                val = lv_obj_get_style_opa_scale(obj, LV_OBJ_PART_MAIN);
            break; // attribute_found

        case ATTR_EXT_CLICK_H:
            if(update)
                lv_obj_set_ext_click_area(obj, val, val, lv_obj_get_ext_click_pad_top(obj),
                                          lv_obj_get_ext_click_pad_bottom(obj));
            else
                val = lv_obj_get_ext_click_pad_left(obj);
            break; // attribute_found

        case ATTR_EXT_CLICK_V:
            if(update)
                lv_obj_set_ext_click_area(obj, lv_obj_get_ext_click_pad_left(obj), lv_obj_get_ext_click_pad_right(obj),
                                          val, val);
            else
                val = lv_obj_get_ext_click_pad_top(obj);
            break; // attribute_found

#if HASP_USE_QRCODE > 0
        case ATTR_SIZE:
            if(obj_check_type(obj, LV_HASP_QRCODE)) {
                if(update) {
                    lv_qrcode_set_size(obj, val);
                } else {
                    val = lv_obj_get_width(obj);
                }
            }
            break;
#endif

        default:
            return HASP_ATTR_TYPE_NOT_FOUND; // attribute_not found
    }

    return HASP_ATTR_TYPE_INT;
}

/**
 * Change or Retrieve the value of the attribute of an object
 * @param obj lv_obj_t*: the object to get/set the attribute
 * @param attr_hash uint16_t: the hashed attribute to get/set
 * @param val uint32_t: the new value of the attribute
 * @param update  bool: change/set the value if true, dispatch/get value if false
 * @note setting a value won't return anything, getting will dispatch the value
 */
static hasp_attribute_type_t attribute_common_bool(lv_obj_t* obj, uint16_t attr_hash, int32_t& val, bool update)
{
    switch(attr_hash) {
        case ATTR_VIS:
            if(update)
                lv_obj_set_hidden(obj, !val);
            else
                val = !lv_obj_get_hidden(obj);
            break; // attribute_found

        case ATTR_HIDDEN:
            if(update)
                lv_obj_set_hidden(obj, !!val);
            else
                val = lv_obj_get_hidden(obj);
            break; // attribute_found

        case ATTR_CLICK:
            if(update)
                lv_obj_set_click(obj, !!val);
            else
                val = lv_obj_get_click(obj);
            break; // attribute_found

        case ATTR_ENABLED:
            if(update)
                if(!!val)
                    lv_obj_clear_state(obj, LV_STATE_DISABLED);
                else
                    lv_obj_add_state(obj, LV_STATE_DISABLED);
            else
                val = !(lv_obj_get_state(obj, LV_BTN_PART_MAIN) & LV_STATE_DISABLED);
            break; // attribute_found

            // case ATTR_SWIPE:
            //     if(update)
            //         obj->user_data.swipeid = (!!val) % 16;
            //     else
            //         val = obj->user_data.swipeid;
            //     break; // attribute_found

        case ATTR_TOGGLE:
            switch(obj_get_type(obj)) {
                case LV_HASP_BUTTON:
                    if(update) {
                        lv_btn_set_checkable(obj, !!val);
                        lv_obj_set_event_cb(obj, !!val ? toggle_event_handler : generic_event_handler);
                    } else {
                        val = lv_btn_get_checkable(obj);
                    }
                    break; // attribute_found
                case LV_HASP_BTNMATRIX:
                    if(update) {
                        if(val) {
                            lv_btnmatrix_set_btn_ctrl_all(obj, LV_BTNMATRIX_CTRL_CHECKABLE);
                        } else {
                            lv_btnmatrix_clear_btn_ctrl_all(obj, LV_BTNMATRIX_CTRL_CHECKABLE);
                            lv_btnmatrix_clear_btn_ctrl_all(obj, LV_BTNMATRIX_CTRL_CHECK_STATE);
                        }
                    } else {
                        val = lv_btn_get_checkable(obj);
                    }
                    break; // attribute_found
                default:
                    return HASP_ATTR_TYPE_NOT_FOUND; // attribute_not found
            }
            break; // attribute_found

        default:
            return HASP_ATTR_TYPE_NOT_FOUND; // attribute_not found
    }

    return HASP_ATTR_TYPE_BOOL;
}

// ##################### Default Attributes ########################################################

void attr_out(lv_obj_t* obj, const char* attribute, const char* data, bool is_json)
{
    uint8_t pageid;
    uint8_t objid;

    if(!attribute || !hasp_find_id_from_obj(obj, &pageid, &objid)) return;

    size_t len = 10;
    if(data) {
        len = strlen(data); // crashes if NULL
    }

    const size_t size = 32 + strlen(attribute) + len;
    char payload[size];

    {
        StaticJsonDocument<64> doc; // Total (recommended) size for const char*
        if(data)
            if(is_json)
                doc[attribute].set(serialized(data));
            else
                doc[attribute].set(data);
        else
            doc[attribute].set(nullptr);
        serializeJson(doc, payload, size);
    }

    object_dispatch_state(pageid, objid, payload);
}

void attr_out_str(lv_obj_t* obj, const char* attribute, const char* data)
{
    attr_out(obj, attribute, data, false);
}

void attr_out_json(lv_obj_t* obj, const char* attribute, const char* data)
{
    attr_out(obj, attribute, data, true);
}

void attr_out_int(lv_obj_t* obj, const char* attribute, int32_t val)
{
    char data[16];
    itoa(val, data, DEC);
    attr_out(obj, attribute, data, true);
}

void attr_out_bool(lv_obj_t* obj, const char* attribute, bool val)
{
    attr_out(obj, attribute, val ? "true" : "false", true);
}

void attr_out_color(lv_obj_t* obj, const char* attribute, lv_color_t color)
{
    uint8_t pageid;
    uint8_t objid;

    if(!attribute || !hasp_find_id_from_obj(obj, &pageid, &objid)) return;

    const size_t size = 64 + strlen(attribute);
    char payload[size];
    {
        StaticJsonDocument<128> doc; // Total (recommended) size for const char*
        char buffer[16];
        lv_color32_t c32;

        c32.full = lv_color_to32(color);
        snprintf_P(buffer, sizeof(buffer), PSTR("#%02x%02x%02x"), c32.ch.red, c32.ch.green, c32.ch.blue);

        doc[attribute].set(buffer);
        doc["r"].set(c32.ch.red);
        doc["g"].set(c32.ch.green);
        doc["b"].set(c32.ch.blue);
        serializeJson(doc, payload, size);
    }
    object_dispatch_state(pageid, objid, payload);
}

/**
 * Change or Retrieve the value of the attribute of an object
 * @param obj lv_obj_t*: the object to get/set the attribute
 * @param attribute char*: the attribute name (with or without leading ".")
 * @param payload char*: the new value of the attribute
 * @param update  bool: change/set the value if true, dispatch/get value if false
 * @note setting a value won't return anything, getting will dispatch the value
 */
void hasp_process_obj_attribute(lv_obj_t* obj, const char* attribute, const char* payload, bool update)
{
    // unsigned long start = millis();
    if(!obj) return;

    lv_color_t color;
    int32_t val;
    char temp_buffer[128]     = "";                       // buffer to hold return strings
    char* text                = &temp_buffer[0];          // pointer to temp_buffer
    hasp_attribute_type_t ret = HASP_ATTR_TYPE_NOT_FOUND; // the return code determines the attribute return value type
    uint16_t attr_hash        = Parser::get_sdbm(attribute);

    switch(attr_hash) {
        case ATTR_GROUPID:
        case ATTR_ID:
        // case ATTR_TRANSITION:
        case ATTR_OBJID:
        case ATTR_X:
        case ATTR_Y:
        case ATTR_H:
        case ATTR_W:
        case ATTR_OPACITY:
        case ATTR_EXT_CLICK_H:
        case ATTR_EXT_CLICK_V:
        case ATTR_SIZE:
            val = strtol(payload, nullptr, DEC);
            ret = attribute_common_int(obj, attr_hash, val, update);
            break;

        case ATTR_VIS:
        case ATTR_HIDDEN:
        case ATTR_TOGGLE:
        case ATTR_CLICK:
        // case ATTR_SWIPE:
        case ATTR_ENABLED:
            val = Parser::is_true(payload);
            ret = attribute_common_bool(obj, attr_hash, val, update);
            break;

        case ATTR_MIN:
            val = strtol(payload, nullptr, DEC);
            ret = attribute_common_range(obj, val, update, true, false);
            break;

        case ATTR_MAX:
            val = strtol(payload, nullptr, DEC);
            ret = attribute_common_range(obj, val, update, false, true);
            break;

        case ATTR_VAL:
            val = strtol(payload, nullptr, DEC);
            ret = attribute_common_val(obj, val, update);
            break;

        // case ATTR_TXT: // TODO: remove
        //     LOG_WARNING(TAG_HASP, F(D_ATTRIBUTE_OBSOLETE D_ATTRIBUTE_INSTEAD), attribute, "text");
        case ATTR_TEXT:
        case ATTR_TEMPLATE:
#if USE_OBJ_ALIAS > 0
        case ATTR_ALIAS:
#endif  // #if USE_OBJ_ALIAS > 0
            ret = attribute_common_text(obj, attr_hash, payload, &text, update);
            break;

        case ATTR_ALIGN:
            ret = attribute_common_align(obj, attribute, payload, &text, update);
            break;
        case ATTR_TAG:
        case ATTR_ACTION:
        case ATTR_SWIPE:
            ret = attribute_common_tag(obj, attr_hash, payload, &text, update);
            break;
        case ATTR_JSONL:
            ret = attribute_common_json(obj, attr_hash, payload, &text, update);
            break;

        case ATTR_OBJ:
            text = (char*)obj_get_type_name(obj);
            if(update && strcasecmp(payload, text) == 0)
                ret = HASP_ATTR_TYPE_METHOD_OK; // Value is already correct
            else if(update)
                ret = HASP_ATTR_TYPE_STR_READONLY; // Can't change to the new value
            else
                ret = HASP_ATTR_TYPE_STR; // Reply the current value
            break;

        case ATTR_MODE:
            ret = attribute_common_mode(obj, payload, &text, val, update);
            break;

        case ATTR_OPTIONS:
            ret = specific_options_attribute(obj, payload, &text, update);
            break;

            // case ATTR_BTN_POS:

            /*    case ATTR_ACTION:
                    if(update)
                        obj->user_data.actionid = Parser::get_action_id(payload);
                    else
                        val = obj->user_data.actionid;
                    ret = HASP_ATTR_TYPE_INT;
                    break;*/

            // case ATTR_SYMBOL:
            //     (update) ? lv_dropdown_set_symbol(obj, payload) :
            //     attr_out_str(obj, attr, lv_dropdown_get_symbol(obj));
            //     return true;

        case ATTR_DELETE:
        case ATTR_CLEAR:
        case ATTR_TO_FRONT:
        case ATTR_TO_BACK:
        case ATTR_OPEN:
        case ATTR_CLOSE:
            ret = attribute_common_method(obj, attr_hash, attribute, payload);
            break;

        case ATTR_COMMENT: // skip this key
            ret = HASP_ATTR_TYPE_METHOD_OK;
            break;

        case ATTR_COLS:
        case ATTR_ROWS:
        case ATTR_AUTO_CLOSE:
        case ATTR_SPEED:
        case ATTR_ANIM_TIME:
        case ATTR_ANIM_SPEED:
        case ATTR_ANGLE:
        case ATTR_ROTATION:
        case ATTR_ZOOM:
        case ATTR_START_VALUE:
        case ATTR_START_ANGLE:
        case ATTR_END_ANGLE:
        case ATTR_START_ANGLE1:
        case ATTR_END_ANGLE1:
        case ATTR_COUNT:
        case ATTR_BTN_POS:
            val = strtol(payload, nullptr, DEC);
            ret = specific_int_attribute(obj, attr_hash, val, update);
            break;

        case ATTR_OFFSET_X:
        case ATTR_OFFSET_Y:
        case ATTR_PIVOT_X:
        case ATTR_PIVOT_Y:
        case ATTR_MAX_HEIGHT:
            val = strtol(payload, nullptr, DEC);
            ret = specific_coord_attribute(obj, attr_hash, val, update);
            break;

        case ATTR_ADJUSTABLE:
        case ATTR_ONE_CHECK:
        case ATTR_AUTO_SIZE:
        case ATTR_SHOW_SELECTED:
        case ATTR_Y_INVERT:
        case ATTR_ANTIALIAS:
        case ATTR_MODE_FIXED:
            val = Parser::is_true(payload);
            ret = specific_bool_attribute(obj, attr_hash, val, update);
            break;

        case ATTR_NEXT:
        case ATTR_PREV:
        case ATTR_BACK:
            val = strtol(payload, nullptr, DEC);
            ret = specific_page_attribute(obj, attr_hash, val, update);
            break;
        case ATTR_NAME: {
            uint8_t pageid = 99;
            haspPages.get_id(obj, &pageid);
            if(update) {
                haspPages.set_name(pageid, payload);
                LOG_VERBOSE(TAG_HASP, F("%s %d"), haspPages.get_name(pageid), pageid);
            } else {
                text = haspPages.get_name(pageid);
                LOG_VERBOSE(TAG_HASP, F("%s %d"), haspPages.get_name(pageid), pageid);
            }
            ret = HASP_ATTR_TYPE_STR;
            break;
        }

        case ATTR_DIRECTION:
            val = strtol(payload, nullptr, DEC);
            ret = special_attribute_direction(obj, attr_hash, val, update);
            break;

        case ATTR_SRC:
            ret = special_attribute_src(obj, payload, &text, update);
            break;

        default: {
            ret = hasp_local_style_attr(obj, attribute, attr_hash, payload, update, val);
        }
    }

    if(ret == HASP_ATTR_TYPE_NOT_FOUND) {
        switch(obj_get_type(obj)) { // Properties by object type

            case LV_HASP_ARC:
                val = strtol(payload, nullptr, DEC);
                ret = hasp_process_arc_attribute(obj, attr_hash, val, update);
                break;

            case LV_HASP_SLIDER:
                val = strtol(payload, nullptr, DEC);
                ret = hasp_process_slider_attribute(obj, attr_hash, val, update);
                break;

            case LV_HASP_SPINNER:
                val = strtol(payload, nullptr, DEC);
                ret = hasp_process_spinner_attribute(obj, attr_hash, val, update);
                break;

            case LV_HASP_GAUGE:
                val = strtol(payload, nullptr, DEC);
                ret = hasp_process_gauge_attribute(obj, attr_hash, val, update);
                break;

            case LV_HASP_LINEMETER:
                val = strtol(payload, nullptr, DEC);
                ret = hasp_process_lmeter_attribute(obj, attr_hash, val, update);
                break;

            case LV_HASP_LINE:
                if(attr_hash == ATTR_POINTS)
                    ret = my_line_set_points(obj, payload) ? HASP_ATTR_TYPE_METHOD_OK : HASP_ATTR_TYPE_RANGE_ERROR;
                break;

            case LV_HASP_CPICKER:
                if(attr_hash == ATTR_COLOR) {
                    if(update) {
                        lv_color32_t c;
                        if(Parser::haspPayloadToColor(payload, c))
                            lv_cpicker_set_color(obj, lv_color_make(c.ch.red, c.ch.green, c.ch.blue));
                    } else {
                        color = lv_cpicker_get_color(obj);
                    }
                    ret = HASP_ATTR_TYPE_COLOR;
                }
                break;

            default:
                break; // not found
        }
    }

    // Positive return codes have returned a value, negative are warnings
    if(update && ret > 0) return; // done

    // Output the returned value or warning
    switch(ret) {
        case HASP_ATTR_TYPE_NOT_FOUND:
            LOG_WARNING(TAG_ATTR, F(D_ATTRIBUTE_UNKNOWN " (%d)"), attribute, attr_hash);
            break;

        case HASP_ATTR_TYPE_INT_READONLY:
            LOG_WARNING(TAG_ATTR, F(D_ATTRIBUTE_READ_ONLY), attribute);
        case HASP_ATTR_TYPE_INT:
            attr_out_int(obj, attribute, val);
            break;

        case HASP_ATTR_TYPE_BOOL_READONLY:
            LOG_WARNING(TAG_ATTR, F(D_ATTRIBUTE_READ_ONLY), attribute);
        case HASP_ATTR_TYPE_BOOL:
            attr_out_bool(obj, attribute, val);
            break;

        case HASP_ATTR_TYPE_STR_READONLY:
            LOG_WARNING(TAG_ATTR, F(D_ATTRIBUTE_READ_ONLY), attribute);
        case HASP_ATTR_TYPE_STR:
            attr_out_str(obj, attribute, text);
            break;

        case HASP_ATTR_TYPE_JSON_READONLY:
            LOG_WARNING(TAG_ATTR, F(D_ATTRIBUTE_READ_ONLY), attribute);
        case HASP_ATTR_TYPE_JSON:
            attr_out_json(obj, attribute, text);
            break;

        case HASP_ATTR_TYPE_COLOR_INVALID:
            LOG_WARNING(TAG_ATTR, F(D_ATTRIBUTE_COLOR_INVALID), payload);
            break;

        case HASP_ATTR_TYPE_COLOR:
            attr_out_color(obj, attribute, color);
            break;

        case HASP_ATTR_TYPE_ALIGN_INVALID:
            LOG_WARNING(TAG_ATTR, F(D_ATTRIBUTE_ALIGN_INVALID), payload);
            break;

        case HASP_ATTR_TYPE_ALIGN:
        case HASP_ATTR_TYPE_DIRECTION_CLOCK:
        case HASP_ATTR_TYPE_DIRECTION_XY:
            attr_out_int(obj, attribute, val);
            break;

        case HASP_ATTR_TYPE_METHOD_INVALID_FOR_PAGE:
            LOG_ERROR(TAG_ATTR, F(D_ATTRIBUTE_PAGE_METHOD_INVALID), attribute);
        case HASP_ATTR_TYPE_METHOD_OK:
            break;

        default:
            LOG_ERROR(TAG_ATTR, F(D_ERROR_UNKNOWN " (%d)"), ret);
    }
}
