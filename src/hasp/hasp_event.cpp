/* MIT License - Copyright (c) 2019-2024 Francis Van Roie
   For full license information read the LICENSE file in the project folder */

/* ********************************************************************************************
 *
 *  HASP Event Handlers
 *     - Value Senders       : Convert values and events into topic/payload before forwarding
 *     - Event Handlers      : Callbacks for object event processing
 *
 * To avoid race conditions:
 *  - Objects need to send consistent events encapsulated between `up`and `down` events
 *  - Where appropriate include the current value of the object
 *
 *  TODO:
 *  - Swiping an open dropdown list activates a gesture (lvgl bug)
 *  - Rolling a roller object sends the events out-of-order (down > up > changed)
 *  - Long pressing a colorpicker (to change the mode) does not send an up event:
 * static void indev_proc_release(lv_indev_proc_t* proc)
 * {
 *    if(proc->wait_until_release != 0) {
 *        lv_event_send(proc->types.pointer.act_obj, LV_EVENT_RELEASED, NULL);  // Add this line for HASP
 *
 ******************************************************************************************** */

#include <time.h>
#include <sys/time.h>

#include "hasplib.h"

#include "lv_core/lv_obj.h" // for tabview ext

static lv_style_int_t last_value_sent;
static lv_obj_t* last_obj_sent = NULL;
static lv_color_t last_color_sent;

void swipe_event_handler(lv_obj_t* obj, lv_event_t event);

// resets the last_value_sent
void event_reset_last_value_sent()
{
    last_obj_sent   = NULL;
    last_value_sent = INT16_MIN;
}

void script_event_handler(const char* eventname, const char* json)
{
    StaticJsonDocument<256> doc;
    StaticJsonDocument<64> filter;

    filter[eventname]              = true;
    DeserializationError jsonError = deserializeJson(doc, json, DeserializationOption::Filter(filter));

    if(!jsonError) {
        JsonVariant json  = doc[eventname].as<JsonVariant>();
        uint8_t savedPage = haspPages.get();
        if(!dispatch_json_variant(json, savedPage, TAG_EVENT)) {
            LOG_WARNING(TAG_EVENT, F(D_DISPATCH_COMMAND_NOT_FOUND), eventname);
        }
    } else {
        dispatch_json_error(TAG_EVENT, jsonError);
    }
}

/**
 * Clean-up allocated memory before an object is deleted
 * @param obj pointer to an object to clean-up
 */
void delete_event_handler(lv_obj_t* obj, lv_event_t event)
{
    if(event != LV_EVENT_DELETE) return;

    switch(obj_get_type(obj)) {
        case LV_HASP_LINE:
            my_line_clear_points(obj);
            break;

        case LV_HASP_BTNMATRIX:
            my_btnmatrix_map_clear(obj);
            break;

        case LV_HASP_MSGBOX:
            my_msgbox_map_clear(obj);
            break;

        case LV_HASP_IMAGE:
            my_image_release_resources(obj);
            break;

#if HASP_USE_QRCODE > 0
        case LV_HASP_QRCODE:
            lv_qrcode_delete(obj);
            break;
#endif

        case LV_HASP_GAUGE:
            break;

        case LV_HASP_LABEL:
            my_obj_del_task(obj);
            break;

        case LV_HASP_DROPDOWN:
            lv_mem_free(lv_dropdown_get_text(obj));
            break;

        default:
            break;
    }

    // TODO: delete value_str data for ALL parts
    uint8_t part_cnt = LV_OBJ_PART_MAIN;
    for(uint8_t part = 0; part <= part_cnt; part++) {
        my_obj_set_value_str_text(obj, part, LV_STATE_DEFAULT, NULL);
        my_obj_set_value_str_text(obj, part, LV_STATE_CHECKED, NULL);
        my_obj_set_value_str_text(obj, part, LV_STATE_PRESSED + LV_STATE_DEFAULT, NULL);
        my_obj_set_value_str_text(obj, part, LV_STATE_PRESSED + LV_STATE_CHECKED, NULL);
        my_obj_set_value_str_text(obj, part, LV_STATE_DISABLED + LV_STATE_DEFAULT, NULL);
        my_obj_set_value_str_text(obj, part, LV_STATE_DISABLED + LV_STATE_CHECKED, NULL);
    }
    my_obj_set_tag(obj, (char*)NULL);
    my_obj_set_action(obj, (char*)NULL);
    my_obj_set_swipe(obj, (char*)NULL);

#if USE_OBJ_ALIAS > 0
    my_obj_set_alias(obj, (char*)NULL);
#endif  // #if USE_OBJ_ALIAS > 0
}

/* ============================== Timer Event  ============================ */
#if LV_USE_CALENDAR > 0
void event_timer_calendar(lv_task_t* task)
{
    hasp_task_user_data_t* data = (hasp_task_user_data_t*)task->user_data;
    lv_obj_t* obj               = NULL;

    if(data) obj = hasp_find_obj_from_page_id(data->pageid, data->objid);
    if(!obj || !data || !obj_check_type(obj, LV_HASP_CALENDER)) {
        if(data) lv_mem_free(data); // the object that the user_data points to is gone
        lv_task_del(task);          // the calendar object for this task was deleted
        LOG_WARNING(TAG_EVENT, "event_timer_calendar could not find the linked object");
        return;
    }

    lv_calendar_date_t date;

    timeval curTime;
    int rslt     = gettimeofday(&curTime, NULL);
    time_t t     = curTime.tv_sec;
    tm* timeinfo = localtime(&t);
    (void)rslt; // unused

    if(timeinfo->tm_year < 120) {
        lv_task_set_period(task, 60000); // try again in a minute
        LOG_WARNING(TAG_EVENT, "event_timer_calendar could not sync the clock");
        return;
    } else {
        uint32_t next_hour = (3600 - (t % 3600)) * 1000; // ms to next top of hour
        // lv_task_set_period(task, next_hour + 128);       // small offset so all tasks don't run at once
        lv_task_set_period(task, data->interval);
    }

    date.day   = timeinfo->tm_mday;
    date.month = timeinfo->tm_mon + 1;     // months since January 0-11
    date.year  = timeinfo->tm_year + 1900; // years since 1900

    LOG_VERBOSE(TAG_EVENT, "event_timer_calendar called with user %d:%d:%d", timeinfo->tm_hour, timeinfo->tm_min,
                timeinfo->tm_sec);

    lv_calendar_set_today_date(obj, &date);
}
#endif

void event_timer_clock(lv_task_t* task)
{
    hasp_task_user_data_t* data = (hasp_task_user_data_t*)task->user_data;

    if(!data || !data->obj || !lv_debug_check_obj_valid(data->obj)) {
        if(data) {
            if(data->templ != D_TIMESTAMP) hasp_free(data->templ);
            lv_mem_free(data); // the object that the user_data points to is gone}
        }
        lv_task_del(task); // the calendar object for this task was deleted
        LOG_WARNING(TAG_EVENT, "event_timer_clock could not find the linked object");
        return;

    } else if(!data->templ) {
        return; // nothing to do
    }

    timeval curTime;
    int rslt = gettimeofday(&curTime, NULL);
    (void)rslt; // unused
    time_t seconds = curTime.tv_sec;
    auto tv_msec   = curTime.tv_usec / 1000;
    tm* timeinfo   = localtime(&seconds);
    lv_task_set_period(task, data->interval - tv_msec);

    char buffer[128] = {0};
    /* if(timeinfo->tm_year < 120) {
         snprintf_P(buffer, sizeof(buffer), PSTR("%ld"), seconds);
     } else */
    strftime(buffer, sizeof(buffer), data->templ, timeinfo);

    // LOG_VERBOSE(TAG_EVENT, "event_timer_clock called with user %d:%d:%d", timeinfo->tm_hour, timeinfo->tm_min,
    //             timeinfo->tm_sec);

    char* cur_text = lv_label_get_text(data->obj);
    if(!cur_text || !strcmp(buffer, cur_text)) return; // No change
    lv_label_set_text(data->obj, buffer);
}

/* ============================== Timer Event  ============================ */
void event_timer_refresh(lv_task_t* task)
{
    lv_obj_t* obj = (lv_obj_t*)task->user_data;
    printf("event_timer_refresh called with user data\n");
    if(!obj) return;

    lv_obj_invalidate(obj);
}

/* ============================== Event Senders ============================ */

/* Takes and lv_obj and finds the pageid and objid
   Then sends the data out on the state/pxby topic
*/

/**
 * Get the hasp eventid for LV_EVENT_PRESSED, LV_EVENT_VALUE_CHANGED, LV_EVENT_LONG_PRESSED_REPEAT and
 * LV_EVENT_RELEASED Also updates the sleep state and handles LV_EVENT_DELETE events
 * @param obj pointer to a color picker
 * @param event type of event that occurred
 * @param eventid returns the hasp eventid
 */
static bool translate_event(lv_obj_t* obj, lv_event_t event, uint8_t& eventid)
{
    switch(event) {
        case LV_EVENT_GESTURE:
            swipe_event_handler(obj, event);
            break;

        case LV_EVENT_DELETE:
            LOG_VERBOSE(TAG_EVENT, F(D_OBJECT_DELETED));
            delete_event_handler(obj, event);
            break;

        case LV_EVENT_PRESSED:
            hasp_update_sleep_state(); // wakeup on press down?
            eventid = HASP_EVENT_DOWN;
            return true;

        case LV_EVENT_LONG_PRESSED_REPEAT:
        case LV_EVENT_LONG_PRESSED:
            eventid = HASP_EVENT_CHANGED;
            return true;

        case LV_EVENT_RELEASED:
            eventid = HASP_EVENT_UP;
            return true;

        case LV_EVENT_VALUE_CHANGED:
            eventid = HASP_EVENT_CHANGED;
            return true;
    }

    return false; // event not translated
}

// ##################### Value Senders ########################################################

static void event_send_object_data(lv_obj_t* obj, const char* data)
{
    uint8_t pageid;
    uint8_t objid;

    if(hasp_find_id_from_obj(obj, &pageid, &objid)) {
        if(!data) return;
        object_dispatch_state(pageid, objid, data);
    } else {
        LOG_ERROR(TAG_EVENT, F(D_OBJECT_UNKNOWN));
    }
}

// Send out events with a val attribute
static void event_object_val_event(lv_obj_t* obj, uint8_t eventid, int16_t val)
{
    char data[512];
    {
    char eventname[8];
    Parser::get_event_name(eventid, eventname, sizeof(eventname));

/*        
    if(const char* tag = my_obj_get_tag(obj))
        snprintf_P(data, sizeof(data), PSTR("{\"event\":\"%s\",\"val\":%d,\"tag\":%s}"), eventname, val, tag);
    else
        snprintf_P(data, sizeof(data), PSTR("{\"event\":\"%s\",\"val\":%d}"), eventname, val);
*/

    snprintf_P(data, sizeof(data), PSTR("{\"event\":\"%s\",\"val\":%d"), eventname, val);

    if (const char* tag = my_obj_get_tag(obj)) {
        snprintf_P(&data[strlen(data)], sizeof(data)-strlen(data), PSTR(",\"tag\":%s"), tag);
    }

#if USE_OBJ_ALIAS > 0
    if (const char* alias = my_obj_get_alias(obj)) {
        snprintf_P(&data[strlen(data)], sizeof(data)-strlen(data), PSTR(",\"alias\":\"%s\""), alias);
    }
#endif  // #if USE_OBJ_ALIAS > 0

    snprintf_P(&data[strlen(data)], sizeof(data)-strlen(data), PSTR("}"));
    }
    event_send_object_data(obj, data);
}

// Send out events with a val and text attribute
static void event_object_selection_changed(lv_obj_t* obj, uint8_t eventid, int16_t val, const char* text)
{
    char data[512];
    {
        StaticJsonDocument<256> doc;
        size_t len = text ? strlen(text) : 0;
        doc.set(text); // use text as-is
        char serialized_text[256];
        len = serializeJson(doc, serialized_text, sizeof(serialized_text));

        char eventname[8];
        Parser::get_event_name(eventid, eventname, sizeof(eventname));

/*        
        if(const char* tag = my_obj_get_tag(obj))
            snprintf_P(data, sizeof(data), PSTR("{\"event\":\"%s\",\"val\":%d,\"text\":%s,\"tag\":%s}"), eventname, val,
                       serialized_text, tag);
        else
            snprintf_P(data, sizeof(data), PSTR("{\"event\":\"%s\",\"val\":%d,\"text\":%s}"), eventname, val,
                       serialized_text);
*/

        snprintf_P(data, sizeof(data), PSTR("{\"event\":\"%s\",\"val\":%d,\"text\":%s"), eventname, val, serialized_text);

        if (const char* tag = my_obj_get_tag(obj)) {
            snprintf_P(&data[strlen(data)], sizeof(data)-strlen(data), PSTR(",\"tag\":%s"), tag);
        }

#if USE_OBJ_ALIAS > 0
        if (const char* alias = my_obj_get_alias(obj)) {
            snprintf_P(&data[strlen(data)], sizeof(data)-strlen(data), PSTR(",\"alias\":\"%s\""), alias);
        }
#endif  // #if USE_OBJ_ALIAS > 0

        snprintf_P(&data[strlen(data)], sizeof(data)-strlen(data), PSTR("}"));
    }
    event_send_object_data(obj, data);
}

// ##################### Event Handlers ########################################################

static inline void event_update_group(uint8_t group, lv_obj_t* obj, bool power, int32_t val, int32_t min, int32_t max)
{
    hasp_update_value_t value = {
                      .obj = obj
                    , .group = group
                    , .min = min
                    , .max = max
                    , .val = val
                    , .power = power 
#if  USE_OBJ_ALIAS > 0
                    , .alias = obj->user_data.aliashash
#endif                    
                    };

    dispatch_normalized_group_values(value);
}

static void log_event(const char* name, lv_event_t event)
{
    return;

    switch(event) {
        case LV_EVENT_PRESSED:
            LOG_TRACE(TAG_EVENT, "%s Changed", name);
            break;

        case LV_EVENT_PRESS_LOST:
            LOG_TRACE(TAG_EVENT, "%s Press lost", name);
            break;

        case LV_EVENT_SHORT_CLICKED:
            LOG_TRACE(TAG_EVENT, "%s Short clicked", name);
            break;

        case LV_EVENT_CLICKED:
            LOG_TRACE(TAG_EVENT, "%s Clicked", name);
            break;

        case LV_EVENT_LONG_PRESSED:
            LOG_TRACE(TAG_EVENT, "%S Long press", name);
            break;

        case LV_EVENT_LONG_PRESSED_REPEAT:
            LOG_TRACE(TAG_EVENT, "%s Long press repeat", name);
            break;

        case LV_EVENT_RELEASED:
            LOG_TRACE(TAG_EVENT, "%s Released", name);
            break;

        case LV_EVENT_VALUE_CHANGED:
            LOG_TRACE(TAG_EVENT, "%s Changed", name);
            break;

        case LV_EVENT_GESTURE:
            LOG_TRACE(TAG_EVENT, "%s Gesture", name);
            break;

        case LV_EVENT_FOCUSED:
            LOG_TRACE(TAG_EVENT, "%s Focussed", name);
            break;

        case LV_EVENT_DEFOCUSED:
            LOG_TRACE(TAG_EVENT, "%s Defocussed", name);
            break;

        case LV_EVENT_PRESSING:
            break;

        default:
            LOG_TRACE(TAG_EVENT, "%s Other %d", name, event);
    }
}

/**
 * Called when a press on the system layer is detected
 * @param obj pointer to a button matrix
 * @param event type of event that occurred
 */
void first_touch_event_handler(lv_obj_t* obj, lv_event_t event)
{
    //  log_event("wakeup", event);
    if(obj != lv_disp_get_layer_sys(NULL)) return;

    if(event == LV_EVENT_RELEASED) {
        bool changed = hasp_stop_antiburn(); // Disable antiburn task

        /*  if(!haspDevice.get_backlight_power()) { */

        dispatch_backlight(NULL, "on", TAG_EVENT); // backlight on and also disable wakeup touch
        hasp_set_wakeup_touch(false);              // only disable wakeup touch

        /*  } else {
              hasp_set_wakeup_touch(false); // only disable wakeup touch
          }*/

        hasp_update_sleep_state();                                // wakeup, send Idle off
        if(changed) dispatch_state_antiburn(hasp_get_antiburn()); // publish the new state

    } else if(event == LV_EVENT_PRESSED) {
        haspDevice.set_backlight_power(true);
    }
}

void swipe_event_handler(lv_obj_t* obj, lv_event_t event)
{
    if(event != LV_EVENT_GESTURE) return;

    if(const char* swipe = my_obj_get_swipe(obj)) {
        lv_gesture_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
        switch(dir) {
            case LV_GESTURE_DIR_LEFT:
                script_event_handler("left", swipe);
                break;
            case LV_GESTURE_DIR_RIGHT:
                script_event_handler("right", swipe);
                break;
            case LV_GESTURE_DIR_BOTTOM:
                script_event_handler("down", swipe);
                break;
            default:
                script_event_handler("up", swipe);
        }
    }
}

/**
 * Called when a textarea is clicked
 * @param obj pointer to a textarea object
 * @param event type of event that occurred
 */
void textarea_event_handler(lv_obj_t* obj, lv_event_t event)
{
    log_event("textarea", event);

    if(event == LV_EVENT_VALUE_CHANGED) {
        LOG_TRACE(TAG_EVENT, "Changed to: %s", lv_textarea_get_text(obj));

        uint8_t hasp_event_id;
        if(!translate_event(obj, event, hasp_event_id)) return;

        char data[1024];
        {
            char eventname[8];
            Parser::get_event_name(hasp_event_id, eventname, sizeof(eventname));
/*            
            if(const char* tag = my_obj_get_tag(obj))
                snprintf_P(data, sizeof(data), PSTR("{\"event\":\"%s\",\"text\":\"%s\",\"tag\":%s}"), eventname,
                           lv_textarea_get_text(obj), tag);
            else
                snprintf_P(data, sizeof(data), PSTR("{\"event\":\"%s\",\"text\":\"%s\"}"), eventname,
                           lv_textarea_get_text(obj));
*/

            snprintf_P(data, sizeof(data), PSTR("{\"event\":\"%s\",\"text\":\"%s\""), eventname, lv_textarea_get_text(obj));

            if (const char* tag = my_obj_get_tag(obj)) {
                snprintf_P(&data[strlen(data)], sizeof(data)-strlen(data), PSTR(",\"tag\":%s"), tag);
            }

#if USE_OBJ_ALIAS > 0
            if (const char* alias = my_obj_get_alias(obj)) {
                snprintf_P(&data[strlen(data)], sizeof(data)-strlen(data), PSTR(",\"alias\":\"%s\""), alias);
            }
#endif  // #if USE_OBJ_ALIAS > 0

            snprintf_P(&data[strlen(data)], sizeof(data)-strlen(data), PSTR("}"));
        }

        event_send_object_data(obj, data);
    } else if(event == LV_EVENT_FOCUSED) {
        lv_textarea_set_cursor_hidden(obj, false);
    } else if(event == LV_EVENT_DEFOCUSED) {
        lv_textarea_set_cursor_hidden(obj, true);
    }
}

/**
 * Called when a button-style object is clicked
 * @param obj pointer to a button object
 * @param event type of event that occurred
 */
void generic_event_handler(lv_obj_t* obj, lv_event_t event)
{
    log_event("generic", event);
    last_obj_sent = obj; // updated but not used in this function

    switch(event) {
        case LV_EVENT_GESTURE:
            swipe_event_handler(obj, event);
            return;

        case LV_EVENT_PRESSED:
            hasp_update_sleep_state(); // wakeup?
            last_value_sent = HASP_EVENT_DOWN;
            break;

        case LV_EVENT_CLICKED:
            // UP = the same object was release then was pressed and press was not lost!
            // eventid = HASP_EVENT_UP;
            if(last_value_sent != HASP_EVENT_LOST && last_value_sent != HASP_EVENT_UP)
                last_value_sent = HASP_EVENT_LONG;
            return;

        case LV_EVENT_SHORT_CLICKED:
            if(last_value_sent != HASP_EVENT_LOST) last_value_sent = HASP_EVENT_UP; // Avoid SHORT + UP double events
            break;

        case LV_EVENT_LONG_PRESSED:
            if(last_value_sent != HASP_EVENT_LOST) last_value_sent = HASP_EVENT_LONG;
            break;

        case LV_EVENT_LONG_PRESSED_REPEAT:
            if(last_value_sent != HASP_EVENT_LOST) last_value_sent = HASP_EVENT_HOLD;
            break; // we do care about hold

        case LV_EVENT_PRESS_LOST:
            last_value_sent = HASP_EVENT_LOST;
            break;

        case LV_EVENT_RELEASED:
            if(last_value_sent == HASP_EVENT_UP) return;
            last_value_sent = HASP_EVENT_RELEASE;
            break;

        case LV_EVENT_DELETE:
            LOG_VERBOSE(TAG_EVENT, F(D_OBJECT_DELETED));
            delete_event_handler(obj, event); // free and destroy persistent memory allocated for certain objects
            return;

        case LV_EVENT_PRESSING:
        case LV_EVENT_FOCUSED:
        case LV_EVENT_DEFOCUSED:
            return; // Don't care about these

        case LV_EVENT_VALUE_CHANGED: // Should not occur in this event handler
        default:
            LOG_WARNING(TAG_EVENT, F(D_OBJECT_EVENT_UNKNOWN), event);
            return;
    }

    if(last_value_sent == HASP_EVENT_LOST) return;

    if(const char* action = my_obj_get_action(obj)) {
        char eventname[8];
        Parser::get_event_name(last_value_sent, eventname, sizeof(eventname));
        script_event_handler(eventname, action);
    } else {
        char data[512];
        {
            char eventname[8];
            Parser::get_event_name(last_value_sent, eventname, sizeof(eventname));
/*            
            if(const char* tag = my_obj_get_tag(obj))
                snprintf_P(data, sizeof(data), PSTR("{\"event\":\"%s\",\"tag\":%s}"), eventname, tag);
            else
                snprintf_P(data, sizeof(data), PSTR("{\"event\":\"%s\"}"), eventname);
*/

            snprintf_P(data, sizeof(data), PSTR("{\"event\":\"%s\""), eventname);

            if (const char* tag = my_obj_get_tag(obj)) {
                snprintf_P(&data[strlen(data)], sizeof(data)-strlen(data), PSTR(",\"tag\":%s"), tag);
            }

#if USE_OBJ_ALIAS > 0
            if (const char* alias = my_obj_get_alias(obj)) {
                snprintf_P(&data[strlen(data)], sizeof(data)-strlen(data), PSTR(",\"alias\":\"%s\""), alias);
            }
#endif  // #if USE_OBJ_ALIAS > 0

            snprintf_P(&data[strlen(data)], sizeof(data)-strlen(data), PSTR("}"));
        }
        event_send_object_data(obj, data);
    }

    // Update group objects and gpios on release
    if(last_value_sent != LV_EVENT_LONG_PRESSED && last_value_sent != LV_EVENT_LONG_PRESSED_REPEAT) {
        bool state = Parser::get_event_state(last_value_sent);
        event_update_group(obj->user_data.groupid, obj, state, state, HASP_EVENT_OFF, HASP_EVENT_ON);
    }
}

/**
 * Called when a object state is toggled on/off
 * @param obj pointer to a switch object
 * @param event type of event that occurred
 */
void toggle_event_handler(lv_obj_t* obj, lv_event_t event)
{
    log_event("toggle", event);
    last_obj_sent = obj; // updated but not used in this function

    uint8_t hasp_event_id;
    if(!translate_event(obj, event, hasp_event_id)) return;
    if(hasp_event_id != HASP_EVENT_DOWN && hasp_event_id != HASP_EVENT_UP) return; // only up or down wanted

    /* Get the new value */
    switch(obj->user_data.objid) {
        case LV_HASP_SWITCH:
            last_value_sent = lv_switch_get_state(obj);
            break;

            /* case LV_HASP_CHECKBOX:
                // This lvgl value is incorrect, it returns pressed instead of checked
                // last_value_sent = lv_checkbox_is_checked(obj);
                last_value_sent = lv_obj_get_state(obj, LV_BTN_PART_MAIN) & LV_STATE_CHECKED;
                break; */

        case LV_HASP_CHECKBOX:
        case LV_HASP_BUTTON: {
            last_value_sent = lv_obj_get_state(obj, LV_BTN_PART_MAIN) & LV_STATE_CHECKED;
            break;
        }

        default:
            return; // invalid object
    }

    event_object_val_event(obj, hasp_event_id, last_value_sent);

    // Update group objects and gpios on release
#if  USE_OBJ_ALIAS == 0
    if(obj->user_data.groupid && hasp_event_id == HASP_EVENT_UP) 
#else
    if((obj->user_data.groupid || obj->user_data.aliashash) && hasp_event_id == HASP_EVENT_UP) 
#endif
    {
        event_update_group(obj->user_data.groupid, obj, last_value_sent, last_value_sent, HASP_EVENT_OFF,
                           HASP_EVENT_ON);
    }
}

/**
 * Called when a range value has changed
 * @param obj pointer to a dropdown list or roller
 * @param event type of event that occurred
 */
void selector_event_handler(lv_obj_t* obj, lv_event_t event)
{
    log_event("selector", event);

    uint8_t hasp_event_id;
    if(!translate_event(obj, event, hasp_event_id)) return; // Use LV_EVENT_VALUE_CHANGED

    /* Get the new value */
    char buffer[128];
#if LV_USE_TABLE > 0
    char property[36];
#endif
    uint16_t val = 0;
    uint16_t max = 0;

    /* Get the text, val and max properties */
    switch(obj->user_data.objid) {
        case LV_HASP_DROPDOWN:
            val = lv_dropdown_get_selected(obj);
            max = lv_dropdown_get_option_cnt(obj) - 1;
            lv_dropdown_get_selected_str(obj, buffer, sizeof(buffer));
            break;

        case LV_HASP_ROLLER:
            val = lv_roller_get_selected(obj);
            max = lv_roller_get_option_cnt(obj) - 1;
            lv_roller_get_selected_str(obj, buffer, sizeof(buffer));
            break;

        case LV_HASP_TABVIEW: {
            val = lv_tabview_get_tab_act(obj);
            max = lv_tabview_get_tab_count(obj) - 1;

            lv_tabview_ext_t* ext = (lv_tabview_ext_t*)lv_obj_get_ext_attr(obj);
            strcpy(buffer, ext->tab_name_ptr[val]);
            break;
        }

#if LV_USE_TABLE > 0
        case LV_HASP_TABLE: {
            uint16_t row;
            uint16_t col;
            if(lv_table_get_pressed_cell(obj, &row, &col) != LV_RES_OK) return; // outside any cell

            const char* txt = lv_table_get_cell_value(obj, row, col);
            strncpy(buffer, txt, sizeof(buffer));

            snprintf_P(property, sizeof(property), PSTR("row\":%d,\"col\":%d,\"text"), row, col);
            attr_out_str(obj, property, buffer);
            return; // done sending
        }
#endif

        default:
            return; // Invalid selector type
    }

    if(hasp_event_id == HASP_EVENT_CHANGED && last_value_sent == val && last_obj_sent == obj)
        return; // same object and value as before

    last_value_sent = val;
    last_obj_sent   = obj;
    event_object_selection_changed(obj, hasp_event_id, val, buffer);

    if(obj->user_data.groupid && max > 0) // max a cannot be 0, its the divider
        if(hasp_event_id == HASP_EVENT_UP || hasp_event_id == HASP_EVENT_CHANGED) {
            event_update_group(obj->user_data.groupid, obj, !!last_value_sent, last_value_sent, 0, max);
        }

    // set the property
    // snprintf_P(property, sizeof(property), PSTR("val\":%d,\"text"), val);
    // attr_out_str(obj, property, buffer);
}

/**
 * Called when a btnmatrix value has changed
 * @param obj pointer to a dropdown list or roller
 * @param event type of event that occurred
 */
void alarm_event_handler(lv_obj_t* obj, lv_event_t event)
{
    log_event("alarm", event);

    uint8_t hasp_event_id;
    if(!translate_event(obj, event, hasp_event_id)) return; // Use LV_EVENT_VALUE_CHANGED

    /* Get the new value */
    // char buffer[128] = "";
    uint16_t val = 0;

    val = lv_btnmatrix_get_active_btn(obj);
    if(val != LV_BTNMATRIX_BTN_NONE && hasp_event_id == HASP_EVENT_UP) {
        lv_obj_t* ta    = hasp_find_obj_from_parent_id(lv_obj_get_parent(obj), 5);
        const char* txt = lv_btnmatrix_get_btn_text(obj, val);
        if(!strcmp(txt, LV_SYMBOL_BACKSPACE))
            lv_textarea_del_char(ta);
        else if(!strcmp(txt, LV_SYMBOL_CLOSE))
            lv_textarea_set_text(ta, "");
        else if(strlen(txt) == 1)
            lv_textarea_add_text(ta, txt);
        else
            ;
        // strncpy(buffer, txt, sizeof(buffer));
    }

    if(hasp_event_id == HASP_EVENT_CHANGED && last_value_sent == val && last_obj_sent == obj)
        return; // same object and value as before

    last_value_sent = val;
    last_obj_sent   = obj;
    // event_object_selection_changed(obj, hasp_event_id, val, buffer);

    // if(max > 0) // max a cannot be 0, its the divider
    //     if(hasp_event_id == HASP_EVENT_UP || hasp_event_id == LV_EVENT_VALUE_CHANGED) {
    //         event_update_group(obj->user_data.groupid, obj, last_value_sent, 0, max);
    //     }
}

/**
 * Called when a btnmatrix value has changed
 * @param obj pointer to a dropdown list or roller
 * @param event type of event that occurred
 */
void btnmatrix_event_handler(lv_obj_t* obj, lv_event_t event)
{
    log_event("btnmatrix", event);

    uint8_t hasp_event_id;
    if(!translate_event(obj, event, hasp_event_id)) return; // Use LV_EVENT_VALUE_CHANGED

    /* Get the new value */
    char buffer[128] = "";
    uint16_t val     = 0;

    val = lv_btnmatrix_get_active_btn(obj);
    if(val != LV_BTNMATRIX_BTN_NONE) {
        const char* txt = lv_btnmatrix_get_btn_text(obj, val);
        strncpy(buffer, txt, sizeof(buffer));
    }

    if(hasp_event_id == HASP_EVENT_CHANGED && last_value_sent == val && last_obj_sent == obj)
        return; // same object and value as before

    last_value_sent = val;
    last_obj_sent   = obj;
    event_object_selection_changed(obj, hasp_event_id, val, buffer);

    // if(max > 0) // max a cannot be 0, its the divider
    //     if(hasp_event_id == HASP_EVENT_UP || hasp_event_id == LV_EVENT_VALUE_CHANGED) {
    //         event_update_group(obj->user_data.groupid, obj, last_value_sent, 0, max);
    //     }
}

/**
 * Called when a msgbox value has changed
 * @param obj pointer to a dropdown list or roller
 * @param event type of event that occurred
 */
void msgbox_event_handler(lv_obj_t* obj, lv_event_t event)
{
    log_event("msgbox", event);

    uint8_t hasp_event_id;
    if(!translate_event(obj, event, hasp_event_id)) return; // Use LV_EVENT_VALUE_CHANGED

    /* Get the new value */
    char buffer[128];
    uint16_t val = 0;

    val = lv_msgbox_get_active_btn(obj);
    if(val != LV_BTNMATRIX_BTN_NONE) {
        const char* txt = lv_msgbox_get_active_btn_text(obj);
        strncpy(buffer, txt, sizeof(buffer));
        if(hasp_event_id == HASP_EVENT_UP || hasp_event_id == HASP_EVENT_RELEASE) lv_msgbox_start_auto_close(obj, 0);
    } else {
        buffer[0] = 0; // empty string
    }

    if(hasp_event_id == HASP_EVENT_CHANGED && last_value_sent == val && last_obj_sent == obj)
        return; // same object and value as before

    last_value_sent = val;
    last_obj_sent   = obj;
    event_object_selection_changed(obj, hasp_event_id, val, buffer);
    // if(max > 0) event_update_group(obj->user_data.groupid, obj, val, 0, max);
}

/**
 * Called when a slider or adjustable arc is clicked
 * @param obj pointer to a slider
 * @param event type of event that occurred
 */
void slider_event_handler(lv_obj_t* obj, lv_event_t event)
{
    log_event("slider", event);

    uint8_t hasp_event_id;
    if(!translate_event(obj, event, hasp_event_id) || event == LV_EVENT_VALUE_CHANGED) return;

    /* Get the new value */
    int16_t val;
    int16_t min;
    int16_t max;

    if(obj->user_data.objid == LV_HASP_SLIDER) {
        val = lv_slider_get_value(obj);
        min = lv_slider_get_min_value(obj);
        max = lv_slider_get_max_value(obj);

    } else if(obj->user_data.objid == LV_HASP_ARC) {
        val = lv_arc_get_value(obj);
        min = lv_arc_get_min_value(obj);
        max = lv_arc_get_max_value(obj);

    } else {
        return; // not a slider
    }

    if(hasp_event_id == HASP_EVENT_CHANGED && last_value_sent == val && last_obj_sent == obj)
        return; // same object and value as before

    last_value_sent = val;
    last_obj_sent   = obj;
    event_object_val_event(obj, hasp_event_id, val);

#if USE_OBJ_ALIAS == 0
    if(obj->user_data.groupid && (hasp_event_id == HASP_EVENT_CHANGED || hasp_event_id == HASP_EVENT_UP) && min != max)
#else
    if((obj->user_data.groupid || obj->user_data.aliashash) && (hasp_event_id == HASP_EVENT_CHANGED || hasp_event_id == HASP_EVENT_UP) && min != max)
#endif
    {
        event_update_group(obj->user_data.groupid, obj, !!val, val, min, max);
    }
}

/**
 * Called when a color picker is clicked
 * @param obj pointer to a color picker
 * @param event type of event that occurred
 */
void cpicker_event_handler(lv_obj_t* obj, lv_event_t event)
{
    log_event("cpicker", event);

    uint8_t hasp_event_id;
    if(!translate_event(obj, event, hasp_event_id) || event == LV_EVENT_VALUE_CHANGED) return;

    /* Get the new value */
    lv_color_t color             = lv_cpicker_get_color(obj);
    lv_cpicker_color_mode_t mode = lv_cpicker_get_color_mode(obj);

    if(hasp_event_id == HASP_EVENT_CHANGED && last_color_sent.full == color.full) return; // same value as before

    char data[512];
    {
        char eventname[8];
        Parser::get_event_name(hasp_event_id, eventname, sizeof(eventname));

        lv_color32_t c32;
        lv_color_hsv_t hsv;
        c32.full        = lv_color_to32(color);
        hsv             = lv_color_rgb_to_hsv(c32.ch.red, c32.ch.green, c32.ch.blue);
        last_color_sent = color;

/*
        if(const char* tag = my_obj_get_tag(obj))
            snprintf_P(data, sizeof(data),
                       PSTR("{\"event\":\"%s\",\"color\":\"#%02x%02x%02x\",\"r\":%d,\"g\":%d,\"b\":%d,\"h\":%d,\"s\":%"
                            "d,\"v\":%d,\"tag\":%s}"),
                       eventname, c32.ch.red, c32.ch.green, c32.ch.blue, c32.ch.red, c32.ch.green, c32.ch.blue, hsv.h,
                       hsv.s, hsv.v, tag);
        else
            snprintf_P(data, sizeof(data),
                       PSTR("{\"event\":\"%s\",\"color\":\"#%02x%02x%02x\",\"r\":%d,\"g\":%d,\"b\":%d,\"h\":%d,\"s\":%"
                            "d,\"v\":%d}"),
                       eventname, c32.ch.red, c32.ch.green, c32.ch.blue, c32.ch.red, c32.ch.green, c32.ch.blue, hsv.h,
                       hsv.s, hsv.v);
*/

        snprintf_P(data, sizeof(data), PSTR("{\"event\":\"%s\",\"color\":\"#%02x%02x%02x\",\"r\":%d,\"g\":%d,\"b\":%d,"
                    "\"h\":%d,\"s\":%d,\"v\":%d"), 
                    eventname, c32.ch.red, c32.ch.green, c32.ch.blue, c32.ch.red, c32.ch.green, c32.ch.blue, hsv.h,
                    hsv.s, hsv.v);

        if (const char* tag = my_obj_get_tag(obj)) {
            snprintf_P(&data[strlen(data)], sizeof(data)-strlen(data), PSTR(",\"tag\":%s"), tag);
        }

#if USE_OBJ_ALIAS > 0
        if (const char* alias = my_obj_get_alias(obj)) {
            snprintf_P(&data[strlen(data)], sizeof(data)-strlen(data), PSTR(",\"alias\":\"%s\""), alias);
        }
#endif  // #if USE_OBJ_ALIAS > 0

        snprintf_P(&data[strlen(data)], sizeof(data)-strlen(data), PSTR("}"));

    }
    event_send_object_data(obj, data);

#if USE_OBJ_ALIAS > 0
    if(obj->user_data.aliashash && hasp_event_id == HASP_EVENT_UP) {
        event_update_group(0, obj, true, lv_color_to32(color), 0x000000, 0x00ffffff);
    }
#endif
}

#if LV_USE_CALENDAR > 0
void calendar_event_handler(lv_obj_t* obj, lv_event_t event)
{
    log_event("calendar", event);

    uint8_t hasp_event_id;
    if(event != LV_EVENT_PRESSED && event != LV_EVENT_RELEASED && event != LV_EVENT_VALUE_CHANGED) return;
    if(!translate_event(obj, event, hasp_event_id)) return; // Use LV_EVENT_VALUE_CHANGED

    /* Get the new value */
    lv_calendar_date_t* date;
    if(hasp_event_id == HASP_EVENT_CHANGED)
        date = lv_calendar_get_pressed_date(obj); // pressed date
    else
        date = lv_calendar_get_showed_date(obj); // current month
    if(!date) return;

    lv_style_int_t val = date->day + date->month * 31;
    if(hasp_event_id == HASP_EVENT_CHANGED && last_value_sent == val && last_obj_sent == obj)
        return; // same object and value as before

    char data[512];
    {
        char eventname[8];
        Parser::get_event_name(hasp_event_id, eventname, sizeof(eventname));

        last_value_sent = val;
        last_obj_sent   = obj;

/*
        if(const char* tag = my_obj_get_tag(obj))
            snprintf_P(data, sizeof(data),
                       PSTR("{\"event\":\"%s\",\"val\":\"%d\",\"text\":\"%04d-%02d-%02dT00:00:00Z\",\"tag\":%s}"),
                       eventname, date->day, date->year, date->month, date->day, tag);
        else
            snprintf_P(data, sizeof(data),
                       PSTR("{\"event\":\"%s\",\"val\":\"%d\",\"text\":\"%04d-%02d-%02dT00:00:00Z\"}"), eventname,
                       date->day, date->year, date->month, date->day);
*/

        snprintf_P(data, sizeof(data), PSTR("{\"event\":\"%s\",\"val\":\"%d\",\"text\":\"%04d-%02d-%02dT00:00:00Z\""), 
                    eventname, date->day, date->year, date->month, date->day);

        if (const char* tag = my_obj_get_tag(obj)) {
            snprintf_P(&data[strlen(data)], sizeof(data)-strlen(data), PSTR(",\"tag\":%s"), tag);
        }

#if USE_OBJ_ALIAS > 0
        if (const char* alias = my_obj_get_alias(obj)) {
            snprintf_P(&data[strlen(data)], sizeof(data)-strlen(data), PSTR(",\"alias\":\"%s\""), alias);
        }
#endif  // #if USE_OBJ_ALIAS > 0

        snprintf_P(&data[strlen(data)], sizeof(data)-strlen(data), PSTR("}"));
    }
    event_send_object_data(obj, data);

    // event_update_group(obj->user_data.groupid, obj, val, min, max);
}
#endif
