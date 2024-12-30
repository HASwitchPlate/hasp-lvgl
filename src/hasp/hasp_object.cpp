/* MIT License - Copyright (c) 2019-2024 Francis Van Roie
   For full license information read the LICENSE file in the project folder */

/* ********************************************************************************************
 *
 *  HASP Object Handlers
 *     - Object Finders      : Convert from object pointers to and from p[x].b[y] IDs
 *     - Value Dispatchers   : Forward output data to the dispatcher
 *     - Attribute processor : Decide if an attribute needs updating or querying and forward
 *     - Object creator      : Creates an object from a line of jsonl
 *
 ******************************************************************************************** */

#include "hasplib.h"

const char** btnmatrix_default_map;            // memory pointer to lvgl default btnmatrix map
const char* msgbox_default_map[] = {"OK", ""}; // memory pointer to hasp default msgbox map

// ##################### Object Finders ########################################################

// Return a child object from a parent with a specific objid
lv_obj_t* hasp_find_obj_from_parent_id(lv_obj_t* parent, uint8_t objid)
{
    if(objid == 0 || parent == nullptr) return parent;

    lv_obj_t* child;
    child = lv_obj_get_child(parent, NULL);
    while(child) {
        /* child found, return it */
        if(objid == child->user_data.id) return child;

        /* check grandchildren */
        lv_obj_t* grandchild = hasp_find_obj_from_parent_id(child, objid);
        if(grandchild) return grandchild; /* grandchild found, return it */

        /* check tabs */
        if(obj_check_type(child, LV_HASP_TABVIEW)) {
            uint16_t tabcount = lv_tabview_get_tab_count(child);
            for(uint16_t i = 0; i < tabcount; i++) {
                lv_obj_t* tab = lv_tabview_get_tab(child, i);
                // LOG_DEBUG(TAG_HASP, "Found tab %i", i);
                if(tab->user_data.id && objid == tab->user_data.id) return tab; /* tab found, return it */

                /* check grandchildren */
                grandchild = hasp_find_obj_from_parent_id(tab, objid);
                if(grandchild) return grandchild; /* grandchild found, return it */
            }
        }

        /* try next sibling */
        child = lv_obj_get_child(parent, child);
    }
    return NULL;
}

// Return the object with a specific pageid and objid
lv_obj_t* hasp_find_obj_from_page_id(uint8_t pageid, uint8_t objid)
{
    return hasp_find_obj_from_parent_id(haspPages.get_obj(pageid), objid);
}

// Return the pageid and objid of an object
bool hasp_find_id_from_obj(const lv_obj_t* obj, uint8_t* pageid, uint8_t* objid)
{
    if(!obj || !haspPages.get_id(obj, pageid)) return false;
    if(obj->user_data.id == 0 && obj != haspPages.get_obj(*pageid)) return false;
    *objid = obj->user_data.id;
    return true;
}

#if USE_OBJ_ALIAS > 0
/**
 * Recursive function that find object with given alias and process payload, 
 * return after all objects checked
 * 
 * @param obj Pointer to perent object
 * @param alias Hash to be searched for
 * @param toreturn If true, the function returns in case of a match
 * @param data Data for hasp_process_obj_attribute()
 * @return pointer to last founded object or NULL if nothing was found
 */
lv_obj_t* hasp_find_obj_from_alias(const lv_obj_t* obj, uint16_t alias, bool toreturn, hasp_cmd_process_data_t *data) 
{
    lv_obj_t* result = NULL;

    if(!obj || !alias) return NULL;

    // check of page self of match alias
    if (alias == obj->user_data.aliashash) {
        // alias match page self
        LOG_DEBUG(TAG_HASP, "page alias match");

        if (data) {
            hasp_process_obj_attribute((lv_obj_t*)obj, data->topic_p, data->payload, data->update);
        }
        result = (lv_obj_t*)obj;
        if (toreturn) return result;
    }

    lv_obj_t* child = lv_obj_get_child_back(obj, NULL);
    while(child) {
        LOG_DEBUG(TAG_HASP, "search alias : objid[%d] objtype[%s] ahash[%d]"
                    , child->user_data.id, obj_get_type_name(child), child->user_data.aliashash);

        if (alias == child->user_data.aliashash) {
            LOG_DEBUG(TAG_HASP, "alias match");

            if (data) {
                hasp_process_obj_attribute((lv_obj_t*)child, data->topic_p, data->payload, data->update);
            }
            result = child;
            if (toreturn) return result;
        }
        
        if (obj_check_type(child, LV_HASP_TABVIEW)) {
            lv_tabview_ext_t * tv_ext = (lv_tabview_ext_t *)lv_obj_get_ext_attr(child);
            lv_obj_t * tabs = lv_page_get_scrollable(tv_ext->content);
            // Recursive call for tabs
            result = hasp_find_obj_from_alias(tabs, alias, toreturn, data);
        } else if (obj_check_type(child, LV_HASP_TAB)) {
            lv_obj_t* tab_content_area = lv_obj_get_child_back(child, NULL);
            // Recursive call for tab content
            result = hasp_find_obj_from_alias(tab_content_area, alias, toreturn, data);
        }

        if (toreturn) return result;

        child = lv_obj_get_child_back(obj, child);
    }

    return result;
}
#endif  // #if USE_OBJ_ALIAS > 0

void hasp_object_tree(const lv_obj_t* parent, uint8_t pageid, uint16_t level)
{
    if(parent == nullptr) return;

    /* Output parent info */
    char indent[31];
    memset(indent, 32, 31);
    if(level < 15) indent[level * 2] = 0;
    indent[30] = 0;

    LOG_VERBOSE(TAG_HASP, F("%s- " HASP_OBJECT_NOTATION ": %s"), indent, pageid, parent->user_data.id,
                obj_get_type_name(parent));

    lv_obj_t* child;
    child = lv_obj_get_child(parent, NULL);
    while(child) {
        /* child found, process it */
        hasp_object_tree(child, pageid, level + 1);

        /* try next sibling */
        child = lv_obj_get_child(parent, child);
    }

    /* check tabs */
    if(obj_check_type(parent, LV_HASP_TABVIEW)) {
#if 1
        uint16_t tabcount = lv_tabview_get_tab_count(parent);
        for(uint16_t i = 0; i < tabcount; i++) {
            lv_obj_t* tab = lv_tabview_get_tab(parent, i);
            LOG_VERBOSE(TAG_HASP, "Found tab %i", i);
            if(tab->user_data.objid) hasp_object_tree(tab, pageid, level + 1);
        }
#endif
    }
}

// ##################### Value Dispatchers ########################################################

/* Sends the data out on the state/pxby topic */
void object_dispatch_state(uint8_t pageid, uint8_t btnid, const char* payload)
{
    char topic[64];
    char* pagename = haspPages.get_name(pageid);
    if(pagename)
        snprintf_P(topic, sizeof(topic), PSTR("%s.b%u"), pagename, btnid);
    else
        snprintf_P(topic, sizeof(topic), PSTR(HASP_OBJECT_NOTATION), pageid, btnid);
    dispatch_state_subtopic(topic, payload);
}

// ##################### State Changers ########################################################

// Recursive function that goes over all objects only ONCE
void object_set_group_values(lv_obj_t* parent, hasp_update_value_t& value)
{
    if(parent == nullptr) return;

    // Update object if it's in the same group
#if USE_OBJ_ALIAS == 0
    if(value.group == parent->user_data.groupid && value.obj != parent)
#else
    if(value.obj != parent &&
       ((value.group > 0 && value.group == parent->user_data.groupid) || 
        (value.alias > 0 && value.alias == parent->user_data.aliashash)))
#endif
    {
        attribute_set_normalized_value(parent, value);
    }

    /* check tabs */
    if(obj_get_type(parent) == LV_HASP_TABVIEW) {
        uint16_t tabcount = lv_tabview_get_tab_count(parent);
        for(uint16_t i = 0; i < tabcount; i++) {
            lv_obj_t* tab = lv_tabview_get_tab(parent, i);
            object_set_group_values(tab, value);
        }
    } else {
        lv_obj_t* child;
        child = lv_obj_get_child(parent, NULL);
        while(child) {
            object_set_group_values(child, value);
            child = lv_obj_get_child(parent, child);
        }
    }
}

// SHOULD only by called from DISPATCH
void object_set_normalized_group_values(hasp_update_value_t& value)
{
#if USE_OBJ_ALIAS == 0
    if(value.group == 0 || value.min == value.max) return;
#else
    if((value.group == 0 && value.alias == 0) || value.min == value.max) return;
#endif

    uint8_t page = haspPages.get();
    object_set_group_values(haspPages.get_obj(page), value); // Update visible objects first

    for(uint8_t i = 0; i < HASP_NUM_PAGES; i++) {
        if(i != page) {
            object_set_group_values(haspPages.get_obj(i), value);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////

// Used in the dispatcher
void hasp_process_attribute(uint8_t pageid, uint8_t objid, const char* attr, const char* payload, bool update)
{
    if(lv_obj_t* obj = hasp_find_obj_from_page_id(pageid, objid)) {
        hasp_process_obj_attribute(obj, attr, payload, update); // || strlen(payload) > 0);
    } else {
        LOG_WARNING(TAG_HASP, F(D_OBJECT_UNKNOWN " " HASP_OBJECT_NOTATION), pageid, objid);
    }
}

// ##################### Object Creator ########################################################

// Called from hasp_new_object or TAG_JSON to process all attributes
int hasp_parse_json_attributes(lv_obj_t* obj, const JsonObject& doc)
{
    int i = 0;

#if HASP_TARGET_PC || defined(ESP32)
    std::string v;
    v.reserve(64);

    for(JsonPair keyValue : doc) {
        // LOG_VERBOSE(TAG_HASP, F(D_BULLET "%s=%s"), keyValue.key().c_str(),
        // keyValue.value().as<std::string>().c_str());
        v = keyValue.value().as<std::string>();
        hasp_process_obj_attribute(obj, keyValue.key().c_str(), keyValue.value().as<std::string>().c_str(), true);
        i++;
    }
#else
    String v((char*)0);
    v.reserve(64);

    for(JsonPair keyValue : doc) {
        // LOG_DEBUG(TAG_HASP, F(D_BULLET "%s=%s"), keyValue.key().c_str(), keyValue.value().as<String>().c_str());
        v = keyValue.value().as<String>();
        hasp_process_obj_attribute(obj, keyValue.key().c_str(), keyValue.value().as<String>().c_str(), true);
        i++;
    }
#endif
    // LOG_DEBUG(TAG_HASP, F("%d keys processed"), i);
    return i;
}

static void object_add_task(lv_obj_t* obj, lv_task_cb_t task_xcb, uint16_t interval)
{
    hasp_task_user_data_t* user_data = (hasp_task_user_data_t*)lv_mem_alloc(sizeof(hasp_task_user_data_t));
    if(!user_data) return;

    user_data->obj      = obj;
    user_data->templ    = (char*)D_TIMESTAMP;
    user_data->interval = interval;
    lv_task_t* task     = lv_task_create(task_xcb, interval, LV_TASK_PRIO_LOWEST, (void*)user_data);
    lv_task_set_repeat_count(task, -1); // Infinite
    lv_task_ready(task);                // trigger it
    // (void)task; // unused
}

/**
 * Create a new object according to the json config
 * @param config Json representation for this object
 * @param saved_page_id the pageid to use when no pageid is specified in the Json, updated when it is specified so
 * following objects in the file can share the pageid
 */
void hasp_new_object(const JsonObject& config, uint8_t& saved_page_id)
{
    /* Skip line detection */
    if(!config[FPSTR(FP_SKIP)].isNull() && config[FPSTR(FP_SKIP)].as<bool>()) return;

    /* Page selection */
    uint8_t pageid = saved_page_id;
    if(!config[FPSTR(FP_PAGE)].isNull()) {
        pageid = config[FPSTR(FP_PAGE)].as<uint8_t>();
        config.remove(FPSTR(FP_PAGE));
    }

    /* Page with pageid is the default parent_obj */
    lv_obj_t* parent_obj = haspPages.get_obj(pageid);
    if(!parent_obj) {
        LOG_WARNING(TAG_HASP, F(D_OBJECT_PAGE_UNKNOWN), pageid);
        return;
    } else {
        saved_page_id = pageid; /* save the current pageid for next objects */
    }

    /* A custom parentid was set */
    if(!config[FPSTR(FP_PARENTID)].isNull()) {
        uint8_t parentid = config[FPSTR(FP_PARENTID)].as<uint8_t>();
        parent_obj       = hasp_find_obj_from_parent_id(parent_obj, parentid);
        if(!parent_obj) {
            LOG_WARNING(TAG_HASP, F("Parent ID " HASP_OBJECT_NOTATION " not found, skipping..."), pageid, parentid);
            return;
        } else {
            LOG_VERBOSE(TAG_HASP, F("Parent ID " HASP_OBJECT_NOTATION " found"), pageid, parentid);
        }
        config.remove(FPSTR(FP_PARENTID));
    }

    uint16_t sdbm = 0;
    uint8_t id    = config[FPSTR(FP_ID)].as<uint8_t>();
    config.remove(FPSTR(FP_ID));

    /* Create the object if it does not exist */
    lv_obj_t* obj = hasp_find_obj_from_parent_id(parent_obj, id);
    if(!obj) {

        /* Create the object first */

        /* Validate type */
       // if(config[FPSTR(FP_OBJID)].isNull()) { // TODO: obsolete objid
            if(config[FPSTR(FP_OBJ)].isNull()) {
                return; // comments
            } else {
                sdbm = Parser::get_sdbm(config[FPSTR(FP_OBJ)].as<const char*>());
                config.remove(FPSTR(FP_OBJ));
            }
        // } else {
        //     LOG_WARNING(TAG_HASP, F(D_ATTRIBUTE_OBSOLETE D_ATTRIBUTE_INSTEAD), "objid",
        //                 "obj"); // TODO: obsolete objid
        //     sdbm = config[FPSTR(FP_OBJID)].as<uint8_t>();
        //     config.remove(FPSTR(FP_OBJID));
        // }

        switch(sdbm) {
                /* ----- Custom Objects ------ */
            case LV_HASP_ALARM:
            case HASP_OBJ_ALARM:
                obj = lv_obj_create(parent_obj, NULL);
                if(obj) obj->user_data.objid = LV_HASP_ALARM;
                break;

            /* ----- Basic Objects ------ */
            case LV_HASP_BTNMATRIX:
            case HASP_OBJ_BTNMATRIX:
                obj = lv_btnmatrix_create(parent_obj, NULL);
                if(obj) {
                    lv_btnmatrix_set_recolor(obj, true);
                    if(obj_check_type(parent_obj, LV_HASP_ALARM))
                        lv_obj_set_event_cb(obj, alarm_event_handler);
                    else
                        lv_obj_set_event_cb(obj, btnmatrix_event_handler);

                    lv_btnmatrix_ext_t* ext = (lv_btnmatrix_ext_t*)lv_obj_get_ext_attr(obj);
                    btnmatrix_default_map   = ext->map_p; // store the static pointer to the default lvgl btnmap
                    obj->user_data.objid    = LV_HASP_BTNMATRIX;
                }
                break;

#if LV_USE_TABLE > 0
            case LV_HASP_TABLE:
            case HASP_OBJ_TABLE:
                obj = lv_table_create(parent_obj, NULL);
                if(obj) {
                    lv_obj_set_event_cb(obj, selector_event_handler);
                    obj->user_data.objid = LV_HASP_TABLE;
                }
                break;
#endif

            case LV_HASP_BUTTON:
            case HASP_OBJ_BTN:
                obj = lv_btn_create(parent_obj, NULL);
                if(obj) {
                    lv_obj_t* lbl = lv_label_create(obj, NULL);
                    if(lbl) {
                        lv_label_set_text(lbl, "");
                        lv_label_set_recolor(lbl, true);
                        lbl->user_data.objid = LV_HASP_LABEL;
                        lv_obj_align(lbl, NULL, LV_ALIGN_CENTER, 0, 0);
                    }
                    lv_obj_set_event_cb(obj, generic_event_handler);
                    obj->user_data.objid = LV_HASP_BUTTON;
                }
                break;

            case LV_HASP_CHECKBOX:
            case HASP_OBJ_CHECKBOX:
                obj = lv_checkbox_create(parent_obj, NULL);
                if(obj) {
                    lv_obj_set_event_cb(obj, toggle_event_handler);
                    obj->user_data.objid = LV_HASP_CHECKBOX;
                }
                break;

            case LV_HASP_LABEL:
            case HASP_OBJ_LABEL:
                obj = lv_label_create(parent_obj, NULL);
                if(obj) {
                    lv_label_set_long_mode(obj, LV_LABEL_LONG_CROP);
                    lv_label_set_recolor(obj, true);
                    lv_obj_set_event_cb(obj, generic_event_handler);
                    obj->user_data.objid = LV_HASP_LABEL;

                    // if(id >= 250) object_add_task(obj, event_timer_clock, 1000);
                }
                break;

            case LV_HASP_TEXTAREA:
            case HASP_OBJ_TEXTAREA:
                obj = lv_textarea_create(parent_obj, NULL);
                if(obj) {
                    lv_obj_set_event_cb(obj, textarea_event_handler);
                    lv_textarea_set_cursor_click_pos(obj, true);
                    obj->user_data.objid = LV_HASP_TEXTAREA;
                }
                break;

            case LV_HASP_IMAGE:
            case HASP_OBJ_IMG:
                obj = lv_img_create(parent_obj, NULL);
                if(obj) {
                    lv_obj_set_event_cb(obj, generic_event_handler);
                    obj->user_data.objid = LV_HASP_IMAGE;
                }
                break;

#if HASP_USE_QRCODE > 0
            case LV_HASP_QRCODE:
            case HASP_OBJ_QRCODE:
                obj = lv_qrcode_create(parent_obj, 140, LV_COLOR_BLACK, LV_COLOR_WHITE);
                if(obj) {
                    lv_obj_set_event_cb(obj, delete_event_handler);
                    obj->user_data.objid = LV_HASP_QRCODE;
                }
                break;
#endif

            case LV_HASP_ARC:
            case HASP_OBJ_ARC:
                obj = lv_arc_create(parent_obj, NULL);
                if(obj) {
                    lv_obj_set_event_cb(obj, generic_event_handler);
                    obj->user_data.objid = LV_HASP_ARC;
                }
                break;

            case LV_HASP_CONTAINER:
            case HASP_OBJ_CONT:
                obj = lv_cont_create(parent_obj, NULL);
                if(obj) {
                    lv_obj_set_event_cb(obj, generic_event_handler);
                    obj->user_data.objid = LV_HASP_CONTAINER;
                }
                break;

            case LV_HASP_OBJECT:
            case HASP_OBJ_OBJ:
                obj = lv_obj_create(parent_obj, NULL);
                if(obj) {
                    lv_obj_set_event_cb(obj, generic_event_handler);
                    obj->user_data.objid = LV_HASP_OBJECT;
                }
                break;

#if LVGL_VERSION_MAJOR == 7 && LV_USE_PAGE
            case LV_HASP_PAGE:
            case HASP_OBJ_PAGE:
                obj = lv_page_create(parent_obj, NULL);
                if(obj) obj->user_data.objid = LV_HASP_PAGE;
                // No event handler for pages
                break;
#endif

#if LV_USE_WIN && LVGL_VERSION_MAJOR == 7
            case LV_HASP_WINDOW:
            case HASP_OBJ_WIN:
                obj = lv_win_create(parent_obj, NULL);
                if(obj) obj->user_data.objid = LV_HASP_WINDOW;
                // No event handler for pages
                break;

#endif

#if LVGL_VERSION_MAJOR == 8
            case LV_HASP_LED:
            case HASP_OBJ_LED:
                obj = lv_led_create(parent_obj);
                if(obj) {
                    lv_obj_set_event_cb(obj, generic_event_handler);
                    obj->user_data.objid = LV_HASP_LED;
                }
                break;

            case LV_HASP_TILEVIEW:
            case HASP_OBJ_TILEVIEW:
                obj = lv_tileview_create(parent_obj);
                if(obj) obj->user_data.objid = LV_HASP_TILEVIEW;
                // No event handler for tileviews
                break;

            case LV_HASP_TABVIEW:
            case HASP_OBJ_TABVIEW:
                obj = lv_tabview_create(parent_obj, LV_DIR_TOP, 100);
                // No event handler for tabs
                if(obj) {
                    lv_obj_set_event_cb(obj, selector_event_handler);
                    obj->user_data.objid = LV_HASP_TABVIEW;
                }
                break;

#else
            case LV_HASP_LED:
            case HASP_OBJ_LED:
                obj = lv_led_create(parent_obj, NULL);
                if(obj) {
                    lv_obj_set_event_cb(obj, generic_event_handler);
                    obj->user_data.objid = LV_HASP_LED;
                }
                break;

#if LV_USE_TILEVIEW > 0
            case LV_HASP_TILEVIEW:
            case HASP_OBJ_TILEVIEW:
                obj = lv_tileview_create(parent_obj, NULL);
                if(obj) obj->user_data.objid = LV_HASP_TILEVIEW;

                // No event handler for tileviews
                break;
#endif

            case LV_HASP_TABVIEW:
            case HASP_OBJ_TABVIEW:
                obj = lv_tabview_create(parent_obj, NULL);
                // No event handler for tabs
                if(obj) {
                    lv_obj_set_event_cb(obj, selector_event_handler);
                    obj->user_data.objid = LV_HASP_TABVIEW;
                }
                break;

            case LV_HASP_TAB:
            case HASP_OBJ_TAB:
                if(parent_obj && parent_obj->user_data.objid == LV_HASP_TABVIEW) {
                    obj = lv_tabview_add_tab(parent_obj, "Tab");
                    if(obj) {
                        lv_obj_set_event_cb(obj, generic_event_handler);
                        obj->user_data.objid = LV_HASP_TAB;
                    }
                } else {
                    LOG_WARNING(TAG_HASP, F("Parent of a tab must be a tabview object"));
                    return;
                }
                break;

#endif
            /* ----- Color Objects ------ */
            case LV_HASP_CPICKER:
            case HASP_OBJ_CPICKER:
                obj = lv_cpicker_create(parent_obj, NULL);
                if(obj) {
                    lv_obj_set_event_cb(obj, cpicker_event_handler);
                    obj->user_data.objid = LV_HASP_CPICKER;
                }
                break;

#if LV_USE_SPINNER != 0
            case LV_HASP_SPINNER:
            case HASP_OBJ_SPINNER:
                obj = lv_spinner_create(parent_obj, NULL);
                if(obj) {
                    obj->user_data.objid = LV_HASP_SPINNER;
                    lv_obj_set_event_cb(obj, generic_event_handler);
                }
                break;
#endif

            /* ----- Range Objects ------ */
            case LV_HASP_SLIDER:
            case HASP_OBJ_SLIDER:
                obj = lv_slider_create(parent_obj, NULL);
                if(obj) {
                    lv_slider_set_range(obj, 0, 100);
                    lv_obj_set_event_cb(obj, slider_event_handler);
                    obj->user_data.objid = LV_HASP_SLIDER;
                }
                // bool knobin = config[F("knobin")].as<bool>() | true;
                // lv_slider_set_knob_in(obj, knobin);
                break;

            case LV_HASP_GAUGE:
            case HASP_OBJ_GAUGE:
                obj = lv_gauge_create(parent_obj, NULL);
                if(obj) {
                    lv_gauge_set_range(obj, 0, 100);
                    lv_obj_set_event_cb(obj, generic_event_handler);
                    obj->user_data.objid = LV_HASP_GAUGE;
                }
                break;

            case LV_HASP_LINE:
            case HASP_OBJ_LINE:
                obj = lv_line_create(parent_obj, NULL);
                if(obj) {
                    lv_obj_set_style_local_line_width(obj, LV_LINE_PART_MAIN, LV_STATE_DEFAULT, 1);
                    lv_obj_set_event_cb(obj, delete_event_handler);
                    obj->user_data.objid = LV_HASP_LINE;
                }
                break;

            case LV_HASP_BAR:
            case HASP_OBJ_BAR:
                obj = lv_bar_create(parent_obj, NULL);
                if(obj) {
                    lv_bar_set_range(obj, 0, 100);
                    lv_obj_set_event_cb(obj, generic_event_handler);
                    obj->user_data.objid = LV_HASP_BAR;
                }
                break;

            case LV_HASP_LINEMETER:
            case HASP_OBJ_LMETER: // obsolete
            case HASP_OBJ_LINEMETER:
                obj = lv_linemeter_create(parent_obj, NULL);
                if(obj) {
                    lv_linemeter_set_range(obj, 0, 100);
                    lv_obj_set_event_cb(obj, generic_event_handler);
                    obj->user_data.objid = LV_HASP_LINEMETER;
                }
                break;

#if LV_USE_SPINBOX > 0
            case LV_HASP_SPINBOX:
            case HASP_OBJ_SPINBOX:
                obj = lv_spinbox_create(parent_obj, NULL);
                if(obj) {
                    lv_spinbox_set_range(obj, 0, 100);
                    lv_obj_set_event_cb(obj, slider_event_handler);
                    obj->user_data.objid = LV_HASP_SPINBOX;
                }
                break;
#endif

            case LV_HASP_LIST:
            case HASP_OBJ_LIST:
                obj = lv_list_create(parent_obj, NULL);
                if(obj) {
                    // Callbacks are set on the individual buttons
                    obj->user_data.objid = LV_HASP_LIST;
                }
                break;

#if LV_USE_CHART > 0
            case LV_HASP_CHART:
            case HASP_OBJ_CHART:
                obj = lv_chart_create(parent_obj, NULL);
                if(obj) {
                    lv_chart_set_range(obj, 0, 100);
                    lv_obj_set_event_cb(obj, generic_event_handler);

                    lv_chart_add_series(obj, LV_COLOR_RED);
                    lv_chart_add_series(obj, LV_COLOR_GREEN);
                    lv_chart_add_series(obj, LV_COLOR_BLUE);

                    lv_chart_series_t* ser = my_chart_get_series(obj, 2);
                    lv_chart_set_next(obj, ser, 10);
                    lv_chart_set_next(obj, ser, 20);
                    lv_chart_set_next(obj, ser, 30);
                    lv_chart_set_next(obj, ser, 40);

                    obj->user_data.objid = LV_HASP_CHART;
                }
                break;
#endif

            /* ----- On/Off Objects ------ */
            case LV_HASP_SWITCH:
            case HASP_OBJ_SWITCH:
                obj = lv_switch_create(parent_obj, NULL);
                if(obj) {
                    lv_obj_set_event_cb(obj, toggle_event_handler);
                    obj->user_data.objid = LV_HASP_SWITCH;
                }
                break;

            /* ----- List Object ------- */
            case LV_HASP_DROPDOWN:
            case HASP_OBJ_DROPDOWN:
                obj = lv_dropdown_create(parent_obj, NULL);
                if(obj) {
                    lv_dropdown_set_text(obj, NULL); // Clear default text
                    lv_dropdown_set_draw_arrow(obj, true);
                    // lv_dropdown_set_anim_time(obj, 200);
                    lv_obj_set_top(obj, true);
                    // lv_obj_align(obj, NULL, LV_ALIGN_IN_TOP_MID, 0, 20);
                    lv_obj_set_event_cb(obj, selector_event_handler);
                    obj->user_data.objid = LV_HASP_DROPDOWN;
                }
                break;

            case LV_HASP_ROLLER:
            case HASP_OBJ_ROLLER:
                obj = lv_roller_create(parent_obj, NULL);
                // lv_obj_align(obj, NULL, LV_ALIGN_IN_TOP_MID, 0, 20);
                if(obj) {
                    lv_roller_set_auto_fit(obj, false);
                    lv_obj_set_event_cb(obj, selector_event_handler);
                    obj->user_data.objid = LV_HASP_ROLLER;
                }
                break;

            case LV_HASP_MSGBOX:
            case HASP_OBJ_MSGBOX:
                obj = lv_msgbox_create(parent_obj, NULL);
                if(obj) {
                    /* Assign default OK btnmap and enable recolor */
                    if(msgbox_default_map) lv_msgbox_add_btns(obj, msgbox_default_map);
                    lv_msgbox_ext_t* ext = (lv_msgbox_ext_t*)lv_obj_get_ext_attr(obj);
                    if(ext && ext->btnm) lv_btnmatrix_set_recolor(ext->btnm, true);

                    /* msgbox parameters */
                    lv_obj_align(obj, NULL, LV_ALIGN_CENTER, 0, 0);
                    lv_obj_set_auto_realign(obj, true);
                    lv_obj_set_event_cb(obj, msgbox_event_handler);
                    obj->user_data.objid = LV_HASP_MSGBOX;
                }
                break;

#if LV_USE_CALENDAR > 0
            case LV_HASP_CALENDER:
            case HASP_OBJ_CALENDAR:
                obj = lv_calendar_create(parent_obj, NULL);
                // lv_obj_align(obj, NULL, LV_ALIGN_IN_TOP_MID, 0, 20);
                if(obj) {
                    lv_obj_set_event_cb(obj, calendar_event_handler);
                    obj->user_data.objid = LV_HASP_CALENDER;

                    object_add_task(obj, pageid, id, event_timer_calendar, 5000);
                }
                break;
#endif

                /* ----- Other Object ------ */
                // default:
                //    return LOG_WARNING(TAG_HASP, F("Unsupported Object ID %u"), objid);
        }

        /* No object was actually created */
        if(!obj) {
            LOG_ERROR(TAG_HASP, F(D_OBJECT_CREATE_FAILED), id);
            return;
        }

        // Prevent losing press when the press is slid out of the objects.
        // (E.g. a Button can be released out of it if it was being pressed)
        lv_obj_add_protect(obj, LV_PROTECT_PRESS_LOST);
        lv_obj_set_gesture_parent(obj, false);
        lv_obj_set_click(obj, true);

        /* id tag the object */
        obj->user_data.id = id;

#ifdef HASP_DEBUG
        uint8_t temp; // needed for debug tests
        (void)temp;
        /** testing start **/
        if(!hasp_find_id_from_obj(obj, &pageid, &temp)) {
            LOG_ERROR(TAG_HASP, F(D_OBJECT_LOST));
            return;
        }
#endif

        /** verbose reporting **/
        LOG_VERBOSE(TAG_HASP, F(D_BULLET HASP_OBJECT_NOTATION " = %s"), pageid, id, obj_get_type_name(obj));

#ifdef HASP_DEBUG
        /* test double-check */
        lv_obj_t* test = hasp_find_obj_from_page_id(pageid, (uint8_t)temp);
        if(test != obj || temp != id) {
            LOG_ERROR(TAG_HASP, F(D_OBJECT_MISMATCH));
            return;
        } else {
            // object created successfully
        }
#endif

    } else {
        // object already exists
    }

    hasp_parse_json_attributes(obj, config);
}
