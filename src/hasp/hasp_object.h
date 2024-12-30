/* MIT License - Copyright (c) 2019-2024 Francis Van Roie
   For full license information read the LICENSE file in the project folder */

#ifndef HASP_OBJECT_H
#define HASP_OBJECT_H

#include "hasplib.h"

const char FP_SKIP[] PROGMEM     = "skip";
const char FP_PAGE[] PROGMEM     = "page";
const char FP_ID[] PROGMEM       = "id";
const char FP_OBJ[] PROGMEM      = "obj";
// const char FP_OBJID[] PROGMEM    = "objid"; // obsolete
const char FP_PARENTID[] PROGMEM = "parentid";
const char FP_GROUPID[] PROGMEM  = "groupid";

typedef struct
{
    char* action;
    char* tag;
    const char* swipe;
} hasp_ext_user_data_t;

typedef struct
{
    lv_obj_t* obj;
    char* templ;
    uint16_t interval;
} hasp_task_user_data_t;

typedef struct
{
    lv_obj_t* obj;
    uint8_t group;
    int32_t min;
    int32_t max;
    int32_t val;
    bool power;
#if USE_OBJ_ALIAS > 0
    uint16_t alias;
#endif
} hasp_update_value_t;

#if  USE_OBJ_ALIAS > 0
typedef struct
{
    const char* topic_p;
    const char* payload;
    bool update;
} hasp_cmd_process_data_t;
#endif

enum lv_hasp_obj_type_t {
    /* Containers */
    LV_HASP_SCREEN    = 1,
    LV_HASP_CONTAINER = 2,
    LV_HASP_WINDOW    = 3, // placeholder
    LV_HASP_MSGBOX    = 4, // placeholder
    LV_HASP_TILEVIEW  = 5, // placeholder
    LV_HASP_TABVIEW   = 6, // placeholder
    LV_HASP_TAB       = 7, // placeholder
    LV_HASP_PAGE      = 8, // Obsolete in v8
    LV_HASP_SPAN      = 9, // placeholder

    /* Controls */
    LV_HASP_KEYBOARD  = 10, // placeholder
    LV_HASP_OBJECT    = 11,
    LV_HASP_BUTTON    = 12,
    LV_HASP_BTNMATRIX = 13,
    LV_HASP_IMGBTN    = 14, // placeholder
    LV_HASP_CHECKBOX  = 15,
    LV_HASP_SWITCH    = 16,
    LV_HASP_SLIDER    = 17,
    LV_HASP_TEXTAREA  = 18, // placeholder
    LV_HASP_SPINBOX   = 19, // placeholder
    LV_HASP_CPICKER   = 20,

    /* Visualizers */
    LV_HASP_LABEL     = 21,
    LV_HASP_GAUGE     = 22,
    LV_HASP_BAR       = 23,
    LV_HASP_LINEMETER = 24,
    LV_HASP_LED       = 25,
    LV_HASP_ARC       = 26,
    LV_HASP_SPINNER   = 27,
    LV_HASP_CHART     = 28,
    LV_HASP_DATETIME  = 40,

    /* Selectors */
    LV_HASP_DROPDOWN = 29,
    LV_HASP_ROLLER   = 30,
    LV_HASP_LIST     = 31, // placeholder
    LV_HASP_TABLE    = 32,
    LV_HASP_CALENDER = 33,
    LV_HASP_MENU     = 34, // placeholder

    /* Graphics */
    LV_HASP_LINE      = 36,
    LV_HASP_IMAGE     = 37, // placeholder
    LV_HASP_ANIMIMAGE = 38, // placeholder
    LV_HASP_CANVAS    = 39, // placeholder
    LV_HASP_MASK      = 40, // placeholder
    LV_HASP_QRCODE    = 41, // placeholder

    /* Custom */
    LV_HASP_ALARM = 60,
};

void hasp_new_object(const JsonObject& config, uint8_t& saved_page_id);

lv_obj_t* hasp_find_obj_from_parent_id(lv_obj_t* parent, uint8_t objid);
lv_obj_t* hasp_find_obj_from_page_id(uint8_t pageid, uint8_t objid);
bool hasp_find_id_from_obj(const lv_obj_t* obj, uint8_t* pageid, uint8_t* objid);

#if USE_OBJ_ALIAS > 0
lv_obj_t* hasp_find_obj_from_alias(const lv_obj_t* obj, uint16_t alias, bool toreturn, hasp_cmd_process_data_t *data);
#endif  // #if USE_OBJ_ALIAS

void hasp_object_tree(const lv_obj_t* parent, uint8_t pageid, uint16_t level);

void object_dispatch_state(uint8_t pageid, uint8_t btnid, const char* payload);

void hasp_process_attribute(uint8_t pageid, uint8_t objid, const char* attr, const char* payload, bool update);
int hasp_parse_json_attributes(lv_obj_t* obj, const JsonObject& doc);

void object_set_normalized_group_values(hasp_update_value_t& value);

/**
 * Get the hasp object type of a given LVGL object
 * @param obj an lv_obj_t* of the object to check its type
 * @return lv_hasp_obj_type_t
 * @note
 */
inline lv_hasp_obj_type_t obj_get_type(const lv_obj_t* obj)
{
    return (lv_hasp_obj_type_t)obj->user_data.objid;
}
/**
 * Get the object type name of an object
 * @param obj an lv_obj_t* of the object to check its type
 * @return name of the object type
 * @note
 */
inline const char* obj_get_type_name(const lv_obj_t* obj)
{
    if(obj_get_type(obj) == LV_HASP_TAB) return "tab"; // LVGL reports tab objects as "lv_page"

    lv_obj_type_t list;
    lv_obj_get_type(obj, &list);
    const char* objtype = list.type[0];
    return objtype + 3; // skip lv_
}
/**
 * Check if an lvgl objecttype name corresponds to a given HASP object ID
 * @param obj an lv_obj_t* of the object to check its type
 * @param haspobjtype the HASP object ID to check against
 * @return true or false whether the types match
 * @note
 */
inline bool obj_check_type(const lv_obj_t* obj, lv_hasp_obj_type_t haspobjtype)
{
#if 1
    if(!obj) return false;
    return obj->user_data.objid == (uint8_t)haspobjtype;
#else
    lv_obj_type_t list;
    lv_obj_get_type(obj, &list);
    const char* objtype = list.type[0];
    return obj_check_type(objtype, haspobjtype);
#endif
}

#define HASP_OBJ_BAR 1971
#define HASP_OBJ_BTN 3164
#define HASP_OBJ_CPICKER 3313
#define HASP_OBJ_CHECKBOX 1923
#define HASP_OBJ_SPINNER 7097
#define HASP_OBJ_MSGBOX 7498
#define HASP_OBJ_TABLE 12078
#define HASP_OBJ_ROLLER 13258
#define HASP_OBJ_LABEL 13684
#define HASP_OBJ_KEYBOARD 14343
#define HASP_OBJ_PAGE 19759
#define HASP_OBJ_WIN 20284
#define HASP_OBJ_TEXTAREA 24186
#define HASP_OBJ_IMGBTN 24441
#define HASP_OBJ_SPINBOX 25641
#define HASP_OBJ_CALENDAR 30334
#define HASP_OBJ_IMG 30499
#define HASP_OBJ_QRCODE 50958
#define HASP_OBJ_GAUGE 33145
#define HASP_OBJ_CHART 34654
#define HASP_OBJ_LINE 34804
#define HASP_OBJ_LIST 35134
#define HASP_OBJ_SLIDER 35265
#define HASP_OBJ_CANVAS 35480
#define HASP_OBJ_TILEVIEW 36019
#define HASP_OBJ_CONT 36434
#define HASP_OBJ_SWITCH 38484
#define HASP_OBJ_LED 41899
#define HASP_OBJ_DROPDOWN 49169
#define HASP_OBJ_BTNMATRIX 49629
#define HASP_OBJ_OBJ 53623
#define HASP_OBJ_OBJMASK 55395
#define HASP_OBJ_LMETER 62749
#define HASP_OBJ_LINEMETER 55189
#define HASP_OBJ_TABVIEW 63226
#define HASP_OBJ_TAB 7861
#define HASP_OBJ_ARC 64594
#define HASP_OBJ_ALARM 3153

#endif