/* MIT License - Copyright (c) 2019-2024 Francis Van Roie
   For full license information read the LICENSE file in the project folder */

#include "hasplib.h"

lv_task_t* my_obj_get_task(const lv_obj_t* obj)
{
    lv_task_t* task = lv_task_get_next(NULL);
    while(task) {
        if(task->user_data) {
            hasp_task_user_data_t* data = (hasp_task_user_data_t*)task->user_data;
            if(data->obj == obj) return task;
        }
        task = lv_task_get_next(task);
    }
    return NULL;
}

lv_task_t* my_obj_new_task(lv_obj_t* obj)
{
    if(!obj) return NULL;

    lv_task_t* task                  = NULL;
    hasp_task_user_data_t* user_data = (hasp_task_user_data_t*)lv_mem_alloc(sizeof(hasp_task_user_data_t));
    if(user_data) {
        user_data->obj      = obj;
        user_data->templ    = (char*)D_TIMESTAMP;
        user_data->interval = 1000;
        task = lv_task_create(event_timer_clock, user_data->interval, LV_TASK_PRIO_LOWEST, (void*)user_data);
        if(task) {
            lv_task_set_repeat_count(task, -1); // Infinite
        } else {
            lv_mem_free(user_data);
        }

        // lv_task_ready(task);                // trigger it
        // (void)task; // unused
    }
    return task;
}

void my_obj_del_task(const lv_obj_t* obj)
{
    lv_task_t* task = my_obj_get_task(obj);
    if(!task) return;

    hasp_task_user_data_t* data = (hasp_task_user_data_t*)task->user_data;
    if(data) {
        if(data->templ != D_TIMESTAMP) hasp_free(data->templ);
        lv_mem_free(data);
    }
    lv_task_del(task);
}

const char* my_obj_get_template(const lv_obj_t* obj)
{
    lv_task_t* task = my_obj_get_task(obj);
    if(!task || !task->user_data) return NULL;

    hasp_task_user_data_t* data = (hasp_task_user_data_t*)task->user_data;
    return data->templ;
}

void my_obj_set_template(lv_obj_t* obj, const char* text)
{
    lv_task_t* task = my_obj_get_task(obj);
    if(!task || !task->user_data) {
        task = my_obj_new_task(obj);
    }

    hasp_task_user_data_t* data = (hasp_task_user_data_t*)task->user_data;
    if(data->templ != D_TIMESTAMP) hasp_free(data->templ);
    data->templ = NULL;
    if(!text) return;

    size_t size = strlen(text);
    if(size == 0) return;

    size++; // terminating \0 character
    data->templ = (char*)hasp_calloc(sizeof(char), size);
    if(data->templ)
        strncpy(data->templ, text, size);
    else
        LOG_WARNING(TAG_ATTR, "Failed to allocate memory!");
}

// free the extended user_data when all properties are NULL
static void my_prune_ext_tags(lv_obj_t* obj)
{
    if(!obj || !obj->user_data.ext) return;

    hasp_ext_user_data_t* ext = (hasp_ext_user_data_t*)obj->user_data.ext;
    if(!ext->action && !ext->swipe && !ext->tag) {
        hasp_free(ext);
        obj->user_data.ext = NULL;
    }
}

// create extended user_data properties object
static hasp_ext_user_data_t* my_create_ext_tags(lv_obj_t* obj)
{
    void* ext          = hasp_calloc(1, sizeof(hasp_ext_user_data_t));
    obj->user_data.ext = ext;
    return (hasp_ext_user_data_t*)ext;
}

void my_obj_set_tag(lv_obj_t* obj, const char* payload)
{
    hasp_ext_user_data_t* ext = (hasp_ext_user_data_t*)obj->user_data.ext;

    // extended tag exists, free old tag
    if(ext && ext->tag) {
        hasp_free(ext->tag);
        ext->tag = NULL;
    }

    // new tag is blank
    if(payload == NULL || payload[0] == '\0') goto prune;

    // create new extended tags
    if(!ext) ext = my_create_ext_tags(obj);

    if(ext) {
        // create new tag
        StaticJsonDocument<512> doc;
        size_t len = payload ? strlen(payload) : 0;

        // check if it is a proper JSON object
        DeserializationError res = deserializeJson(doc, payload, len);
        if(res != DeserializationError::Ok) doc.set(payload); // use tag as-is

        const size_t size = measureJson(doc) + 1;
        if(char* str = (char*)hasp_malloc(size)) {
            len      = serializeJson(doc, str, size); // tidy-up the json object
            ext->tag = str;
            LOG_VERBOSE(TAG_ATTR, "new json: %s", str);
            return; // no error & no prune
        }
    }

error:
    LOG_WARNING(TAG_ATTR, D_ERROR_OUT_OF_MEMORY); // ext or str was NULL

prune:
    my_prune_ext_tags(obj); // delete extended data if all extended properties are NULL
}

// the tag data is stored as SERIALIZED JSON data
const char* my_obj_get_tag(lv_obj_t* obj)
{
    if(!obj) return NULL;
    hasp_ext_user_data_t* ext = (hasp_ext_user_data_t*)obj->user_data.ext;
    return ext ? ext->tag : NULL;
}

// the action data is stored as SERIALIZED JSON data
void my_obj_set_action(lv_obj_t* obj, const char* payload)
{
    hasp_ext_user_data_t* ext = (hasp_ext_user_data_t*)obj->user_data.ext;

    // extended tag exists, free old tag
    if(ext && ext->action) {
        hasp_free(ext->action);
        ext->action = NULL;
    }

    // new tag is blank
    if(payload == NULL || payload[0] == '\0') goto prune;

    // create new extended tags
    if(!ext) ext = my_create_ext_tags(obj);

    // create new action
    if(ext) {
        StaticJsonDocument<512> doc;
        size_t len  = payload ? strlen(payload) : 0;
        ext->action = NULL;

        // Backwards compatibility
        if(uint8_t page = Parser::get_action_id(payload)) {
            char json[64] = "{\"up\":\"page ";
            switch(page) {
                case 0:
                    LOG_WARNING(TAG_ATTR, "Invalid parameter");
                    goto prune;
                case HASP_NUM_PAGE_PREV:
                    strcat(json, "prev");
                    break;
                case HASP_NUM_PAGE_NEXT:
                    strcat(json, "next");
                    break;
                case HASP_NUM_PAGE_BACK:
                    strcat(json, "back");
                    break;
                default: {
                    char str[64];
                    itoa(page, str, DEC);
                    strcat(json, str);
                }
            }
            strcat(json, "\"}");
            deserializeJson(doc, json);
        } else {
            // Check for new json action format
            DeserializationError res = deserializeJson(doc, payload, len);
            if(res != DeserializationError::Ok) {
                LOG_WARNING(TAG_ATTR, "Invalid parameter");
                goto prune;
            }
        }

        const size_t size = measureJson(doc) + 1;
        if(char* str = (char*)hasp_malloc(size)) {
            size_t len  = serializeJson(doc, str, size); // tidy-up the json object
            ext->action = str;
            LOG_VERBOSE(TAG_ATTR, "new json: %s", str);
            return; // no error & no prune
        }
    }

error:
    LOG_WARNING(TAG_ATTR, D_ERROR_OUT_OF_MEMORY); // ext or str was NULL

prune:
    my_prune_ext_tags(obj); // delete extended data if all extended properties are NULL
}

// the tag data is stored as SERIALIZED JSON data
const char* my_obj_get_action(lv_obj_t* obj)
{
    if(!obj) return NULL;
    hasp_ext_user_data_t* ext = (hasp_ext_user_data_t*)obj->user_data.ext;
    return ext ? ext->action : NULL;
}

// the swipe data is stored as SERIALIZED JSON data
void my_obj_set_swipe(lv_obj_t* obj, const char* payload)
{
    hasp_ext_user_data_t* ext     = (hasp_ext_user_data_t*)obj->user_data.ext;
    static const char* _swipejson = R"({"down":"page back","left":"page next","right":"page prev","up":"page back"})";

    // extended tag exists, free old tag if it's not the const _swipejson
    if(ext) {
        if(ext->swipe != _swipejson) hasp_free((void*)ext->swipe);
        ext->swipe = NULL;
    }

    // new tag is blank
    if(payload == NULL || payload[0] == '\0') goto prune;

    // create new extended tags
    if(!ext) ext = my_create_ext_tags(obj);

    if(ext) {
        if(Parser::is_true(payload)) {
            ext->swipe = _swipejson; // backwards compatibility: use static action
            return;                  // no error & no prune
        }

        // create new action
        StaticJsonDocument<512> doc;
        size_t len = payload ? strlen(payload) : 0;
        ext->swipe = NULL;

        // check if it is a proper JSON object
        DeserializationError res = deserializeJson(doc, payload, len);
        if(res != DeserializationError::Ok) {
            LOG_WARNING(TAG_ATTR, "Invalid parameter");
            goto prune;
        }
        if(doc.isNull()) goto prune;
        if(doc.is<bool>() || doc.is<uint8_t>()) {
            if(doc.as<bool>()) { // backwards compatibility: use static action
                ext->swipe = _swipejson;
                return; // no error & no prune
            } else {
                goto prune;
            }
        }

        const size_t size = measureJson(doc) + 1;
        if(char* str = (char*)hasp_malloc(size)) {
            size_t len = serializeJson(doc, str, size); // tidy-up the json object
            ext->swipe = str;
            LOG_VERBOSE(TAG_ATTR, "new json: %s", str);
            return; // no error & no prune
        }
    }

error:
    LOG_WARNING(TAG_ATTR, D_ERROR_OUT_OF_MEMORY); // ext or str was NULL

prune:
    my_prune_ext_tags(obj); // delete extended data if all extended properties are NULL
}

// the tag data is stored as SERIALIZED JSON data
const char* my_obj_get_swipe(lv_obj_t* obj)
{
    if(!obj) return NULL;
    hasp_ext_user_data_t* ext = (hasp_ext_user_data_t*)obj->user_data.ext;
    return ext ? ext->swipe : NULL;
}

lv_label_align_t my_textarea_get_text_align(lv_obj_t* ta)
{
    lv_textarea_ext_t* ext = (lv_textarea_ext_t*)lv_obj_get_ext_attr(ta);
    lv_obj_t* label        = lv_textarea_get_label(ta);
    return lv_label_get_align(label);
}

const char* my_tabview_get_tab_name(const lv_obj_t* tabview, uint16_t id)
{
    if(id >= lv_tabview_get_tab_count(tabview)) return NULL;

    lv_tabview_ext_t* ext = (lv_tabview_ext_t*)lv_obj_get_ext_attr(tabview);
    return ext->tab_name_ptr[id];
}

static void my_img_set_pivot_x(lv_obj_t* obj, lv_coord_t x)
{
    lv_point_t point;
    lv_img_get_pivot(obj, &point);
    lv_img_set_pivot(obj, x, point.y);
}

static void my_img_set_pivot_y(lv_obj_t* obj, lv_coord_t y)
{
    lv_point_t point;
    lv_img_get_pivot(obj, &point);
    lv_img_set_pivot(obj, point.x, y);
}

static lv_coord_t my_img_get_pivot_x(lv_obj_t* obj)
{
    lv_point_t point;
    lv_img_get_pivot(obj, &point);
    return point.x;
}

static lv_coord_t my_img_get_pivot_y(lv_obj_t* obj)
{
    lv_point_t point;
    lv_img_get_pivot(obj, &point);
    return point.y;
}

// OK - this function is missing in lvgl
static uint8_t my_roller_get_visible_row_count(const lv_obj_t* roller)
{
    const lv_font_t* font     = lv_obj_get_style_text_font(roller, LV_ROLLER_PART_BG);
    lv_style_int_t line_space = lv_obj_get_style_text_line_space(roller, LV_ROLLER_PART_BG);
    lv_coord_t h              = lv_obj_get_height(roller);

    if((lv_font_get_line_height(font) + line_space) != 0)
        return (uint8_t)(h / (lv_font_get_line_height(font) + line_space));
    else
        return 0;
}

// OK - this function is not const in lvgl and doesn't return 0
static uint16_t my_msgbox_stop_auto_close(const lv_obj_t* obj)
{
    lv_msgbox_stop_auto_close((lv_obj_t*)obj);
    return 0;
}

// OK - this function is not const in lvgl
static bool my_arc_get_adjustable(const lv_obj_t* arc)
{
    return lv_arc_get_adjustable((lv_obj_t*)arc);
}

// OK - we need to change the event handler too
static void my_arc_set_adjustable(lv_obj_t* arc, bool toggle)
{
    lv_arc_set_adjustable(arc, toggle);
    lv_obj_set_event_cb(arc, toggle ? slider_event_handler : generic_event_handler);
}

// OK - this function is missing in lvgl
static inline uint16_t my_arc_get_rotation(lv_obj_t* arc)
{
    lv_arc_ext_t* ext = (lv_arc_ext_t*)lv_obj_get_ext_attr(arc);
    return ext->rotation_angle;
}

#if LV_USE_CHART > 0
// OK - this function is missing in lvgl
static inline int16_t my_chart_get_min_value(lv_obj_t* chart)
{
    lv_chart_ext_t* ext = (lv_chart_ext_t*)lv_obj_get_ext_attr(chart);
    return ext->ymin[LV_CHART_AXIS_PRIMARY_Y];
}

// OK - this function is missing in lvgl
static inline int16_t my_chart_get_max_value(lv_obj_t* chart)
{
    lv_chart_ext_t* ext = (lv_chart_ext_t*)lv_obj_get_ext_attr(chart);
    return ext->ymax[LV_CHART_AXIS_PRIMARY_Y];
}

lv_chart_series_t* my_chart_get_series(lv_obj_t* chart, uint8_t ser_num)
{
    lv_chart_ext_t* ext    = (lv_chart_ext_t*)lv_obj_get_ext_attr(chart);
    lv_chart_series_t* ser = (lv_chart_series_t*)_lv_ll_get_tail(&ext->series_ll);
    while(ser_num > 0 && ser) {
        ser = (lv_chart_series_t*)_lv_ll_get_prev(&ext->series_ll, ser);
        ser_num--;
    }
    return ser;
}
#endif

#if LV_USE_SPINBOX > 0
// OK - this function is missing in lvgl
static inline int16_t my_spinbox_get_min_value(lv_obj_t* chart)
{
    lv_spinbox_ext_t* ext = (lv_spinbox_ext_t*)lv_obj_get_ext_attr(chart);
    return ext->range_min;
}

// OK - this function is missing in lvgl
static inline int16_t my_spinbox_get_max_value(lv_obj_t* chart)
{
    lv_spinbox_ext_t* ext = (lv_spinbox_ext_t*)lv_obj_get_ext_attr(chart);
    return ext->range_max;
}
#endif

// OK
static inline lv_color_t haspLogColor(lv_color_t color)
{
    // uint8_t r = (LV_COLOR_GET_R(color) * 263 + 7) >> 5;
    // uint8_t g = (LV_COLOR_GET_G(color) * 259 + 3) >> 6;
    // uint8_t b = (LV_COLOR_GET_B(color) * 263 + 7) >> 5;
    // LOG_VERBOSE(TAG_ATTR,F("Color: R%u G%u B%u"), r, g, b);
    return color;
}

void my_bar_set_start_value(lv_obj_t* bar, int16_t start_value)
{
    lv_bar_set_start_value(bar, start_value, true);
}

void my_slider_set_left_value(lv_obj_t* bar, int16_t start_value)
{
    lv_slider_set_left_value(bar, start_value, true);
}

lv_coord_t my_dropdown_get_max_height(lv_obj_t* obj)
{
    return lv_dropdown_get_max_height(obj);
}

// OK
lv_obj_t* FindButtonLabel(lv_obj_t* btn)
{
    if(btn) {
        lv_obj_t* label = lv_obj_get_child_back(btn, NULL);
        if(label) {
            if(obj_check_type(label, LV_HASP_LABEL)) {
                return label;
            }

        } else {
            LOG_ERROR(TAG_ATTR, F("FindButtonLabel NULL Pointer encountered"));
        }
    } else {
        LOG_WARNING(TAG_ATTR, F("Button not defined"));
    }
    return NULL;
}

// OK - lvgl does not return a const char *
static const char* my_dropdown_get_text(const lv_obj_t* dd)
{
    const char* str_p = lv_dropdown_get_text((lv_obj_t*)dd);
    return str_p ? str_p : "";
}

// OK - lvgl does not return a const char *
static void my_dropdown_set_text(lv_obj_t* dd, const char* text)
{
    size_t len = 0;
    if(text) len = strlen(text) + 1;

    // release previous text
    char* str_p = (char*)lv_dropdown_get_text(dd);
    if(str_p) lv_mem_free(str_p);

    // reserve and copy new text
    str_p = (char*)lv_mem_alloc(len);
    if(str_p != NULL) strncpy(str_p, text, len);

    lv_dropdown_set_text((lv_obj_t*)dd, str_p); // library does not return const
    lv_obj_invalidate(dd); // Needed if old ptr is equal to new ptr
}

// OK - lvgl does not return a const char *
static const char* my_label_get_text(const lv_obj_t* label)
{
    return lv_label_get_text(label); // library does not return const
}

static void my_label_set_text(lv_obj_t* label, const char* text)
{
    if(text[0] == '%') {
        uint16_t hash           = Parser::get_sdbm(text);
        size_t len              = strlen(text);
        const char* static_text = NULL;

        switch(hash) {
            case ATTR_TEXT_MAC:
                if(len == 5) static_text = haspDevice.get_hardware_id();
                break;

#if HASP_USE_WIFI > 0
            case ATTR_TEXT_SSID:
                if(len == 6) static_text = wifi_get_ssid();
                break;

            case ATTR_TEXT_IP:
                if(len == 4) static_text = wifi_get_ip_address();
                break;
#endif
            case ATTR_TEXT_HOSTNAME:
                if(len == 10) static_text = haspDevice.get_hostname();
                break;

            case ATTR_TEXT_MODEL:
                if(len == 7) static_text = haspDevice.get_model();
                break;

            case ATTR_TEXT_VERSION:
                if(len == 9) static_text = haspDevice.get_version();
                break;
        }

        if(static_text) {
            lv_label_set_text_static(label, static_text);
            return;
        }
    }

    lv_label_set_text(label, text);
}

/**
 * Resize the button holder to fit
 * @param mbox pointer to message box object
 */
static void my_mbox_realign(lv_obj_t* mbox)
{
    lv_msgbox_ext_t* ext = (lv_msgbox_ext_t*)lv_obj_get_ext_attr(mbox);

    lv_coord_t w = lv_obj_get_width_fit(mbox);

    if(ext->text) {
        lv_obj_set_width(ext->text, w);
    }

    if(ext->btnm) {
        lv_style_int_t bg_top     = lv_obj_get_style_pad_top(mbox, LV_MSGBOX_PART_BTN_BG);
        lv_style_int_t bg_bottom  = lv_obj_get_style_pad_bottom(mbox, LV_MSGBOX_PART_BTN_BG);
        lv_style_int_t bg_inner   = lv_obj_get_style_pad_inner(mbox, LV_MSGBOX_PART_BTN_BG);
        lv_style_int_t btn_top    = lv_obj_get_style_pad_top(mbox, LV_MSGBOX_PART_BTN);
        lv_style_int_t btn_bottom = lv_obj_get_style_pad_bottom(mbox, LV_MSGBOX_PART_BTN);
        const lv_font_t* font     = lv_obj_get_style_text_font(mbox, LV_MSGBOX_PART_BTN);

        uint16_t btnm_lines   = 1;
        const char** btnm_map = lv_btnmatrix_get_map_array(ext->btnm);
        uint16_t i;
        for(i = 0; btnm_map[i][0] != '\0'; i++) {
            if(btnm_map[i][0] == '\n') btnm_lines++;
        }

        lv_coord_t font_h = lv_font_get_line_height(font);
        lv_coord_t btn_h  = font_h + btn_top + btn_bottom;
        lv_obj_set_size(ext->btnm, w, btn_h * btnm_lines + bg_inner * (btnm_lines - 1) + bg_top + bg_bottom);
    }
}

/* allows for static text label */
void my_msgbox_set_text(lv_obj_t* mbox, const char* txt)
{
    LV_ASSERT_OBJ(mbox, LV_OBJX_NAME);
    LV_ASSERT_STR(txt);

    lv_msgbox_ext_t* ext = (lv_msgbox_ext_t*)lv_obj_get_ext_attr(mbox);
    my_label_set_text(ext->text, txt);

    my_mbox_realign(mbox);
}

// OK
static const char* my_btn_get_text(const lv_obj_t* obj)
{
    if(!obj) {
        LOG_WARNING(TAG_ATTR, F("Button not defined"));
        return NULL;
    }

    lv_obj_t* label = lv_obj_get_child_back(obj, NULL);
    if(label) {
#if 1
        if(obj_check_type(label, LV_HASP_LABEL)) return lv_label_get_text(label);

#else
        lv_obj_type_t list;
        lv_obj_get_type(label, &list);

        if(obj_check_type(list.type[0], LV_HASP_LABEL)) {
            text = lv_label_get_text(label);
            return true;
        }
#endif

    } else {
        LOG_WARNING(TAG_ATTR, F("my_btn_get_text NULL Pointer encountered"));
    }

    return NULL;
}

// OK
static inline void my_btn_set_text(lv_obj_t* obj, const char* value)
{
    lv_obj_t* label = FindButtonLabel(obj);
    if(label) {
        my_label_set_text(label, value);
    }
}

// OK - lvgl does not return a const char *
#if HASP_USE_QRCODE > 0
static const char* my_qrcode_get_text(const lv_obj_t* obj)
{
    if(!obj) {
        LOG_WARNING(TAG_ATTR, F("QR-code not defined"));
        return NULL;
    }

    if(obj) {
        if(obj_check_type(obj, LV_HASP_QRCODE)) return lv_qrcode_get_text(obj);
    } else {
        LOG_WARNING(TAG_ATTR, F("my_qrcode_get_text NULL Pointer encountered"));
    }

    return NULL;
}

static void my_qrcode_set_text(lv_obj_t* obj, const char* text)
{
    lv_qrcode_set_text(obj, text);
}
#endif

/**
 * Get the value_str for an object part and state.
 * @param obj pointer to a object
 * @param result text '\0' terminated character string.
 */
const char* my_obj_get_value_str_text(lv_obj_t* obj, uint8_t part, lv_state_t state)
{
    lv_state_t old_state = lv_obj_get_state(obj, part);
    lv_obj_set_state(obj, state);
    lv_obj_refresh_style(obj, part, LV_STYLE_VALUE_STR);
    const char* value_str_p = lv_obj_get_style_value_str(obj, part);
    lv_obj_set_state(obj, old_state);
    lv_obj_refresh_style(obj, part, LV_STYLE_VALUE_STR);
    return value_str_p;
}

/**
 * Set a new value_str for an object. Memory will be allocated to store the text by the object.
 * @param obj pointer to a object
 * @param text '\0' terminated character string. NULL to refresh with the current text.
 */
void my_obj_set_value_str_text(lv_obj_t* obj, uint8_t part, lv_state_t state, const char* text)
{
    //  LOG_VERBOSE(TAG_ATTR, F("%s %d"), __FILE__, __LINE__);
    lv_state_t old_state = lv_obj_get_state(obj, part);

    // the lower priority state to check inheritance of the value_str against
    lv_state_t prev_state;
    switch(state) {
        case LV_STATE_CHECKED:
        case(LV_STATE_PRESSED + LV_STATE_DEFAULT):
        case(LV_STATE_DISABLED + LV_STATE_DEFAULT):
            prev_state = LV_STATE_DEFAULT;
            break;
        case(LV_STATE_DISABLED + LV_STATE_CHECKED):
        case(LV_STATE_PRESSED + LV_STATE_CHECKED):
            prev_state = LV_STATE_CHECKED;
            break;
    }

    const char* prev_value_str_p = NULL;
    if(state != LV_STATE_DEFAULT) {
        prev_value_str_p = my_obj_get_value_str_text(obj, part, prev_state);
    }

    // The value_str pointer of the current state to check inheritance
    lv_obj_set_state(obj, state);
    lv_obj_refresh_style(obj, part, LV_STYLE_VALUE_STR);
    const char* curr_value_str_p = lv_obj_get_style_value_str(obj, part);

    // Only free if there is no value_str state inheritance
    if(prev_value_str_p != curr_value_str_p && curr_value_str_p != NULL) {
        LOG_DEBUG(TAG_ATTR, "Releasing %s", curr_value_str_p);
        lv_mem_free(curr_value_str_p);
    }

    /*Get the size of the new text*/
    size_t len = 0;
    if(text != NULL && text[0] != '\0') len = strlen(text) + 1;

    /*Allocate space for the new text*/
    char* str_p = NULL;
    if(len > 0) str_p = (char*)lv_mem_alloc(len);
    if(str_p != NULL) strncpy(str_p, text, len);
    lv_obj_set_style_local_value_str(obj, part, state, str_p);

    lv_obj_set_state(obj, old_state);
    lv_obj_refresh_style(obj, part, LV_STYLE_VALUE_STR);
}

void my_list_set_options(lv_obj_t* obj, const char* payload)
{ // Reserve memory for JsonDocument
    size_t maxsize = (128u * ((strlen(payload) / 128) + 1)) + 256;
    DynamicJsonDocument doc(maxsize);
    DeserializationError jsonError = deserializeJson(doc, payload);

    if(jsonError) { // Couldn't parse incoming JSON payload
        dispatch_json_error(TAG_ATTR, jsonError);
        return;
    }

    doc.shrinkToFit();
    lv_list_clean(obj);

    JsonArray arr = doc.as<JsonArray>(); // Parse payload
    lv_obj_t* btn = NULL;
    for(JsonVariant v : arr) {
        const char* c = v.as<const char*>();
        if(*c == 0xee || *c == 0xef) {
            char icon[4];
            memcpy(icon, c, 3);
            icon[3] = '\0';
            btn     = lv_list_add_btn(obj, icon, c + 3);

        } else
            btn = lv_list_add_btn(obj, NULL, c);
    }

    if(btn) lv_obj_set_event_cb(btn, selector_event_handler);
}

void my_tabview_set_text(lv_obj_t* obj, const char* payload)
{
    uint16_t id = lv_tabview_get_tab_act(obj);

    if(id < lv_tabview_get_tab_count(obj)) {
        lv_tabview_set_tab_name(obj, id, (char*)payload);
    }
}

const char* my_tabview_get_text(const lv_obj_t* obj)
{
    uint16_t id = lv_tabview_get_tab_act(obj);

    if(id < lv_tabview_get_tab_count(obj)) {
        return my_tabview_get_tab_name(obj, id);
    } else {
        return NULL;
    }
}

void my_tab_set_text(lv_obj_t* obj, const char* payload)
{
    lv_obj_t* content = lv_obj_get_parent(obj->parent); // 2 levels up
    if(!content) return LOG_WARNING(TAG_ATTR, F("content not found"));

    lv_obj_t* tabview = lv_obj_get_parent(content); // 3rd level up
    if(!tabview) return LOG_WARNING(TAG_ATTR, F("Tabview not found"));

    if(!obj_check_type(tabview, LV_HASP_TABVIEW))
        return LOG_WARNING(TAG_ATTR, F("LV_HASP_TABVIEW not found %d"), obj_get_type(tabview));

    for(uint16_t id = 0; id < lv_tabview_get_tab_count(tabview); id++) {
        if(obj == lv_tabview_get_tab(tabview, id)) {
            lv_tabview_set_tab_name(tabview, id, (char*)payload);
            return;
        }
    }
    LOG_WARNING(TAG_ATTR, F("Tab not found"));
}

const char* my_tab_get_text(const lv_obj_t* obj)
{
    lv_obj_t* content = lv_obj_get_parent(obj->parent); // 2 levels up
    if(!content) {
        LOG_WARNING(TAG_ATTR, F("content not found"));
        return NULL;
    }

    lv_obj_t* tabview = lv_obj_get_parent(content); // 3rd level up
    if(!tabview) {
        LOG_WARNING(TAG_ATTR, F("Tabview not found"));
        return NULL;
    }

    if(!obj_check_type(tabview, LV_HASP_TABVIEW)) {
        LOG_WARNING(TAG_ATTR, F("LV_HASP_TABVIEW not found %d"), obj_get_type(tabview));
        return NULL;
    }

    for(uint16_t id = 0; id < lv_tabview_get_tab_count(tabview); id++) {
        if(obj == lv_tabview_get_tab(tabview, id)) {
            return my_tabview_get_tab_name(tabview, id);
        }
    }
    LOG_WARNING(TAG_ATTR, F("Tab not found"));
    return NULL;
}

static void gauge_format_10(lv_obj_t* gauge, char* buf, int bufsize, int32_t value)
{
    snprintf(buf, bufsize, PSTR("%d"), value / 10);
}

static void gauge_format_100(lv_obj_t* gauge, char* buf, int bufsize, int32_t value)
{
    snprintf(buf, bufsize, PSTR("%d"), value / 100);
}

static void gauge_format_1k(lv_obj_t* gauge, char* buf, int bufsize, int32_t value)
{
    snprintf(buf, bufsize, PSTR("%d"), value / 1000);
}

static void gauge_format_10k(lv_obj_t* gauge, char* buf, int bufsize, int32_t value)
{
    snprintf(buf, bufsize, PSTR("%d"), value / 10000);
}

// OK - this function is missing in lvgl
static uint16_t my_btnmatrix_get_count(const lv_obj_t* btnm)
{
    lv_btnmatrix_ext_t* ext = (lv_btnmatrix_ext_t*)lv_obj_get_ext_attr(btnm);
    return ext->btn_cnt;
}

#if USE_OBJ_ALIAS > 0
/**
 * Get the text of alias
 * @param obj pointer to perent object
 * @return alias text
 */
const char* my_obj_get_alias(lv_obj_t* obj)
{
    if(!obj) return NULL;

    return obj->user_data.alias;
}

/**
 * Set the alias of an object
 * @param obj pointer to object
 * @param text alias text
 */
void my_obj_set_alias(lv_obj_t* obj, const char* text)
{
    // If exist old alias string, free up memory
    if(obj->user_data.alias) {
        hasp_free(obj->user_data.alias);
        obj->user_data.aliashash = 0; 
        obj->user_data.alias = NULL;
    }

    // new alias is blank
    if(text == NULL || text[0] == '\0') {
        obj->user_data.aliashash = 0; 
        obj->user_data.alias = NULL;
        return; 
    }

    // calculate hash
    uint16_t hash = Parser::get_sdbm(text, UINT16_MAX, true);

    // store alias hash in object
    obj->user_data.aliashash = hash; 

    // allocate mem for store alias text and save the pointer in object
    const size_t size = strlen(text);
    if(char* str = (char*)hasp_malloc(size + 1)) {
        strncpy(str, text, size + 1);   // copy include 0 termination
        str[size] = '\0';   // safety 0 termination
        obj->user_data.alias = str;
    } 

    return;
}
#endif // USE_OBJ_ALIAS

#if 0
static bool attribute_lookup_lv_property(uint16_t hash, uint8_t * prop)
{
    struct prop_hash_map
    {
        uint16_t hash;
        uint8_t  prop;
    };

    /* in order of prevalence */
    prop_hash_map props[] = {
        {ATTR_PAD_TOP, LV_STYLE_PAD_TOP & LV_STYLE_PROP_ALL},
        {ATTR_BORDER_WIDTH, LV_STYLE_BORDER_WIDTH & LV_STYLE_PROP_ALL},
        {ATTR_OUTLINE_WIDTH, LV_STYLE_OUTLINE_WIDTH & LV_STYLE_PROP_ALL},
        {ATTR_VALUE_LETTER_SPACE, LV_STYLE_VALUE_LETTER_SPACE & LV_STYLE_PROP_ALL},
        {ATTR_TEXT_LETTER_SPACE, LV_STYLE_TEXT_LETTER_SPACE & LV_STYLE_PROP_ALL},
        {ATTR_LINE_WIDTH, LV_STYLE_LINE_WIDTH & LV_STYLE_PROP_ALL},
        {ATTR_TRANSITION_TIME, LV_STYLE_TRANSITION_TIME & LV_STYLE_PROP_ALL},
        {ATTR_SCALE_WIDTH, LV_STYLE_SCALE_WIDTH & LV_STYLE_PROP_ALL},
        {ATTR_RADIUS, LV_STYLE_RADIUS & LV_STYLE_PROP_ALL},
        {ATTR_PAD_BOTTOM, LV_STYLE_PAD_BOTTOM & LV_STYLE_PROP_ALL},
        {ATTR_BG_MAIN_STOP, LV_STYLE_BG_MAIN_STOP & LV_STYLE_PROP_ALL},
        {ATTR_BORDER_SIDE, LV_STYLE_BORDER_SIDE & LV_STYLE_PROP_ALL},
        {ATTR_OUTLINE_PAD, LV_STYLE_OUTLINE_PAD & LV_STYLE_PROP_ALL},
        {ATTR_PATTERN_REPEAT, LV_STYLE_PATTERN_REPEAT & LV_STYLE_PROP_ALL},
        {ATTR_VALUE_LINE_SPACE, LV_STYLE_VALUE_LINE_SPACE & LV_STYLE_PROP_ALL},
        {ATTR_TEXT_LINE_SPACE, LV_STYLE_TEXT_LINE_SPACE & LV_STYLE_PROP_ALL},
        {ATTR_TRANSITION_DELAY, LV_STYLE_TRANSITION_DELAY & LV_STYLE_PROP_ALL},
        {ATTR_SCALE_BORDER_WIDTH, LV_STYLE_SCALE_BORDER_WIDTH & LV_STYLE_PROP_ALL},
        {ATTR_CLIP_CORNER, LV_STYLE_CLIP_CORNER & LV_STYLE_PROP_ALL},
        {ATTR_PAD_LEFT, LV_STYLE_PAD_LEFT & LV_STYLE_PROP_ALL},
        {ATTR_BG_GRAD_STOP, LV_STYLE_BG_GRAD_STOP & LV_STYLE_PROP_ALL},
        {ATTR_TEXT_DECOR, LV_STYLE_TEXT_DECOR & LV_STYLE_PROP_ALL},
        {ATTR_LINE_DASH_WIDTH, LV_STYLE_LINE_DASH_WIDTH & LV_STYLE_PROP_ALL},
        {ATTR_TRANSITION_PROP_1, LV_STYLE_TRANSITION_PROP_1 & LV_STYLE_PROP_ALL},
        {ATTR_SCALE_END_BORDER_WIDTH, LV_STYLE_SCALE_END_BORDER_WIDTH & LV_STYLE_PROP_ALL},
        {ATTR_SIZE, LV_STYLE_SIZE & LV_STYLE_PROP_ALL},
        {ATTR_PAD_RIGHT, LV_STYLE_PAD_RIGHT & LV_STYLE_PROP_ALL},
        {ATTR_BG_GRAD_DIR, LV_STYLE_BG_GRAD_DIR & LV_STYLE_PROP_ALL},
        {ATTR_BORDER_POST, LV_STYLE_BORDER_POST & LV_STYLE_PROP_ALL},
        {ATTR_VALUE_OFS_X, LV_STYLE_VALUE_OFS_X & LV_STYLE_PROP_ALL},
        {ATTR_LINE_DASH_GAP, LV_STYLE_LINE_DASH_GAP & LV_STYLE_PROP_ALL},
        {ATTR_TRANSITION_PROP_2, LV_STYLE_TRANSITION_PROP_2 & LV_STYLE_PROP_ALL},
        {ATTR_SCALE_END_LINE_WIDTH, LV_STYLE_SCALE_END_LINE_WIDTH & LV_STYLE_PROP_ALL},
        {ATTR_TRANSFORM_WIDTH, LV_STYLE_TRANSFORM_WIDTH & LV_STYLE_PROP_ALL},
        {ATTR_PAD_INNER, LV_STYLE_PAD_INNER & LV_STYLE_PROP_ALL},
        {ATTR_VALUE_OFS_Y, LV_STYLE_VALUE_OFS_Y & LV_STYLE_PROP_ALL},
        {ATTR_LINE_ROUNDED, LV_STYLE_LINE_ROUNDED & LV_STYLE_PROP_ALL},
        {ATTR_TRANSITION_PROP_3, LV_STYLE_TRANSITION_PROP_3 & LV_STYLE_PROP_ALL},
        {ATTR_TRANSFORM_HEIGHT, LV_STYLE_TRANSFORM_HEIGHT & LV_STYLE_PROP_ALL},
        // {ATTR_MARGIN_TOP, LV_STYLE_MARGIN_TOP & LV_STYLE_PROP_ALL},
        {ATTR_VALUE_ALIGN, LV_STYLE_VALUE_ALIGN & LV_STYLE_PROP_ALL},
        {ATTR_TRANSITION_PROP_4, LV_STYLE_TRANSITION_PROP_4 & LV_STYLE_PROP_ALL},
        // {ATTR_TRANSFORM_ANGLE, LV_STYLE_TRANSFORM_ANGLE & LV_STYLE_PROP_ALL},
        // {ATTR_MARGIN_BOTTOM, LV_STYLE_MARGIN_BOTTOM & LV_STYLE_PROP_ALL},
        {ATTR_TRANSITION_PROP_5, LV_STYLE_TRANSITION_PROP_5 & LV_STYLE_PROP_ALL},
        // {ATTR_TRANSFORM_ZOOM, LV_STYLE_TRANSFORM_ZOOM & LV_STYLE_PROP_ALL},
        // {ATTR_MARGIN_LEFT, LV_STYLE_MARGIN_LEFT & LV_STYLE_PROP_ALL},
        {ATTR_TRANSITION_PROP_6, LV_STYLE_TRANSITION_PROP_6 & LV_STYLE_PROP_ALL},
        // {ATTR_MARGIN_RIGHT, LV_STYLE_MARGIN_RIGHT & LV_STYLE_PROP_ALL},
        {ATTR_BG_COLOR, LV_STYLE_BG_COLOR & LV_STYLE_PROP_ALL},
        {ATTR_BORDER_COLOR, LV_STYLE_BORDER_COLOR & LV_STYLE_PROP_ALL},
        {ATTR_OUTLINE_COLOR, LV_STYLE_OUTLINE_COLOR & LV_STYLE_PROP_ALL},
        {ATTR_PATTERN_RECOLOR, LV_STYLE_PATTERN_RECOLOR & LV_STYLE_PROP_ALL},
        {ATTR_VALUE_COLOR, LV_STYLE_VALUE_COLOR & LV_STYLE_PROP_ALL},
        {ATTR_TEXT_COLOR, LV_STYLE_TEXT_COLOR & LV_STYLE_PROP_ALL},
        {ATTR_LINE_COLOR, LV_STYLE_LINE_COLOR & LV_STYLE_PROP_ALL},
        {ATTR_IMAGE_RECOLOR, LV_STYLE_IMAGE_RECOLOR & LV_STYLE_PROP_ALL},
        {ATTR_SCALE_GRAD_COLOR, LV_STYLE_SCALE_GRAD_COLOR & LV_STYLE_PROP_ALL},
        {ATTR_BG_GRAD_COLOR, LV_STYLE_BG_GRAD_COLOR & LV_STYLE_PROP_ALL},
        {ATTR_TEXT_SEL_COLOR, LV_STYLE_TEXT_SEL_COLOR & LV_STYLE_PROP_ALL},
        {ATTR_SCALE_END_COLOR, LV_STYLE_SCALE_END_COLOR & LV_STYLE_PROP_ALL},
        // {ATTR_TEXT_SEL_BG_COLOR, LV_STYLE_TEXT_SEL_BG_COLOR & LV_STYLE_PROP_ALL},
        {ATTR_OPA_SCALE, LV_STYLE_OPA_SCALE & LV_STYLE_PROP_ALL},
        {ATTR_BG_OPA, LV_STYLE_BG_OPA & LV_STYLE_PROP_ALL},
        {ATTR_BORDER_OPA, LV_STYLE_BORDER_OPA & LV_STYLE_PROP_ALL},
        {ATTR_OUTLINE_OPA, LV_STYLE_OUTLINE_OPA & LV_STYLE_PROP_ALL},
        {ATTR_PATTERN_OPA, LV_STYLE_PATTERN_OPA & LV_STYLE_PROP_ALL},
        {ATTR_VALUE_OPA, LV_STYLE_VALUE_OPA & LV_STYLE_PROP_ALL},
        {ATTR_TEXT_OPA, LV_STYLE_TEXT_OPA & LV_STYLE_PROP_ALL},
        {ATTR_LINE_OPA, LV_STYLE_LINE_OPA & LV_STYLE_PROP_ALL},
        {ATTR_IMAGE_OPA, LV_STYLE_IMAGE_OPA & LV_STYLE_PROP_ALL},
        {ATTR_PATTERN_RECOLOR_OPA, LV_STYLE_PATTERN_RECOLOR_OPA & LV_STYLE_PROP_ALL},
        {ATTR_IMAGE_RECOLOR_OPA, LV_STYLE_IMAGE_RECOLOR_OPA & LV_STYLE_PROP_ALL},
        {ATTR_PATTERN_IMAGE, LV_STYLE_PATTERN_IMAGE & LV_STYLE_PROP_ALL},
        {ATTR_VALUE_FONT, LV_STYLE_VALUE_FONT & LV_STYLE_PROP_ALL},
        {ATTR_TEXT_FONT, LV_STYLE_TEXT_FONT & LV_STYLE_PROP_ALL},
        {ATTR_TRANSITION_PATH, LV_STYLE_TRANSITION_PATH & LV_STYLE_PROP_ALL},
        {ATTR_VALUE_STR, LV_STYLE_VALUE_STR & LV_STYLE_PROP_ALL},

#if LV_USE_SHADOW
        {ATTR_SHADOW_WIDTH, LV_STYLE_SHADOW_WIDTH & LV_STYLE_PROP_ALL},
        {ATTR_SHADOW_OFS_X, LV_STYLE_SHADOW_OFS_X & LV_STYLE_PROP_ALL},
        {ATTR_SHADOW_OFS_Y, LV_STYLE_SHADOW_OFS_Y & LV_STYLE_PROP_ALL},
        {ATTR_SHADOW_SPREAD, LV_STYLE_SHADOW_SPREAD & LV_STYLE_PROP_ALL},
        {ATTR_SHADOW_COLOR, LV_STYLE_SHADOW_COLOR & LV_STYLE_PROP_ALL},
        {ATTR_SHADOW_OPA, LV_STYLE_SHADOW_OPA & LV_STYLE_PROP_ALL},
#endif

#if LV_USE_BLEND_MODES && LV_USE_SHADOW
        {ATTR_SHADOW_BLEND_MODE, LV_STYLE_SHADOW_BLEND_MODE & LV_STYLE_PROP_ALL},
#endif

#if LV_USE_BLEND_MODES
        {ATTR_BG_BLEND_MODE, LV_STYLE_BG_BLEND_MODE & LV_STYLE_PROP_ALL},
        {ATTR_PATTERN_BLEND_MODE, LV_STYLE_PATTERN_BLEND_MODE & LV_STYLE_PROP_ALL},
        {ATTR_IMAGE_BLEND_MODE, LV_STYLE_IMAGE_BLEND_MODE & LV_STYLE_PROP_ALL},
        {ATTR_LINE_BLEND_MODE, LV_STYLE_LINE_BLEND_MODE & LV_STYLE_PROP_ALL},
        {ATTR_BORDER_BLEND_MODE, LV_STYLE_BORDER_BLEND_MODE & LV_STYLE_PROP_ALL},
        {ATTR_OUTLINE_BLEND_MODE, LV_STYLE_OUTLINE_BLEND_MODE & LV_STYLE_PROP_ALL},
        {ATTR_VALUE_BLEND_MODE, LV_STYLE_VALUE_BLEND_MODE & LV_STYLE_PROP_ALL},
        {ATTR_TEXT_BLEND_MODE, LV_STYLE_TEXT_BLEND_MODE & LV_STYLE_PROP_ALL},
#endif
    };

    for(uint32_t i = 0; i < sizeof(props) / sizeof(props[0]); i++) {
        if(props[i].hash == hash) {
            *prop = props[1].prop;
            LOG_WARNING(TAG_ATTR, F("%d found and has property %d"), hash, props[i].prop);
            return true;
        }
    }
    LOG_ERROR(TAG_ATTR, F("%d has no property id"), hash);
    return false;
}

static bool attribute_get_lv_property()
{
    lv_res_t res _lv_style_get_int(const lv_style_t * style, lv_style_property_t prop, void * res);
    return res == LV_RES_OK
}

static bool attribute_set_lv_property()
{
    lv_res_t res _lv_style_get_int(const lv_style_t * style, lv_style_property_t prop, void * res);
    return res == LV_RES_OK
}

static bool attribute_update_lv_property(lv_obj_t * obj, const char * attr_p, uint16_t attr_hash, const char * payload,
                                         bool update)
{
    uint8_t prop;
    uint8_t prop_type;

    // convert sdbm hash to lv property number
    if(!attribute_lookup_lv_property(attr_hash, &prop)) return false;

    // find the parameter type for this property
    prop_type = prop & 0xF;

    if(prop_type < LV_STYLE_ID_COLOR) {
        if(update) {
            _lv_obj_set_style_local_int(obj, part, prop | (state << LV_STYLE_STATE_POS), atoi(payload))
        } else {
            attr_out_str(obj, attr_p, lv_obj_get_style_value_str(obj, part));
        }
    } else if(prop_type < LV_STYLE_ID_OPA) {
    } else if(prop_type < LV_STYLE_ID_PTR) {
    } else {
    }
}
#endif
