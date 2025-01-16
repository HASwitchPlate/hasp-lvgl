/* MIT License - Copyright (c) 2019-2024 Francis Van Roie
   For full license information read the LICENSE file in the project folder */

#include <time.h>
#include <sys/time.h>

// #include "ArduinoLog.h"
#include "hasplib.h"

#include "dev/device.h"
#include "drv/tft/tft_driver.h"

// #include "hasp_gui.h"

#if HASP_USE_DEBUG > 0
#include "../hasp_debug.h"
#include "hasp_gui.h" // for screenshot

#if HASP_TARGET_PC
#include <iostream>
#include <fstream>
#include <sstream>
#include "../mqtt/hasp_mqtt.h"
#else
#include "StringStream.h"
#include "StreamUtils.h" // for exec ReadBufferingStream
#include "CharStream.h"

#include "hasp_oobe.h"
#include "sys/gpio/hasp_gpio.h"
#include "hal/hasp_hal.h"

#include "sys/svc/hasp_ota.h"
#include "mqtt/hasp_mqtt.h"
#include "sys/net/hasp_network.h" // for network_get_status()
#include "sys/net/hasp_time.h"
#endif
#endif

dispatch_conf_t dispatch_setings = {.teleperiod = 300};

uint16_t dispatchSecondsToNextTeleperiod = 0;
uint16_t dispatchSecondsToNextSensordata = 0;
uint16_t dispatchSecondsToNextDiscovery  = 0;
uint8_t nCommands                        = 0;
haspCommand_t commands[29];

moodlight_t moodlight    = {.brightness = 255};
uint8_t saved_jsonl_page = 0;

/* Sends the payload out on the state/subtopic
 */
void dispatch_state_subtopic(const char* subtopic, const char* payload)
{
#if HASP_USE_MQTT == 0 && defined(HASP_USE_TASMOTA_CLIENT) && HASP_USE_TASMOTA_CLIENT > 0
    LOG_TRACE(TAG_MSGR, F("%s => %s"), subtopic, payload);
#else

#if HASP_USE_MQTT > 0
    switch(mqtt_send_state(subtopic, payload)) {
        case MQTT_ERR_OK:
            LOG_TRACE(TAG_MQTT_PUB, F("%s => %s"), subtopic, payload);
            break;
        case MQTT_ERR_PUB_FAIL:
            LOG_ERROR(TAG_MQTT_PUB, F(D_MQTT_FAILED " %s => %s"), subtopic, payload);
            break;
        case MQTT_ERR_NO_CONN:
            LOG_ERROR(TAG_MQTT, F(D_MQTT_NOT_CONNECTED " %s => %s"), subtopic, payload);
            break;
        case MQTT_ERR_DISABLED:
            break;
        default:
            LOG_ERROR(TAG_MQTT, F(D_ERROR_UNKNOWN " %s => %s"), subtopic, payload);
    }
#endif

#if defined(HASP_USE_TASMOTA_CLIENT) && HASP_USE_TASMOTA_CLIENT > 0
    slave_send_state(subtopic, payload);
#endif

#endif

#if defined(HASP_USE_CUSTOM) && HASP_USE_CUSTOM > 0
    custom_state_subtopic(subtopic, payload);
#endif
}

void dispatch_state_eventid(const char* topic, hasp_event_t eventid)
{
    char payload[32];
    char eventname[8];

    Parser::get_event_name(eventid, eventname, sizeof(eventname));
    if(eventid == HASP_EVENT_ON || eventid == HASP_EVENT_OFF) {
        snprintf_P(payload, sizeof(payload), PSTR("{\"state\":\"%s\"}"), eventname);
    } else {
        snprintf_P(payload, sizeof(payload), PSTR("{\"event\":\"%s\"}"), eventname);
    }
    dispatch_state_subtopic(topic, payload);
}

void dispatch_state_brightness(const char* topic, hasp_event_t eventid, int32_t val)
{
    char payload[64];
    char eventname[8];

    Parser::get_event_name(eventid, eventname, sizeof(eventname));
    snprintf_P(payload, sizeof(payload), PSTR("{\"state\":\"%s\",\"brightness\":%d}"), eventname, val);
    dispatch_state_subtopic(topic, payload);
}

void dispatch_state_antiburn(hasp_event_t eventid)
{
    char topic[9];
    char payload[64];
    char eventname[8];

    Parser::get_event_name(eventid, eventname, sizeof(eventname));
    snprintf_P(topic, sizeof(topic), PSTR("antiburn"));
    snprintf_P(payload, sizeof(payload), PSTR("{\"state\":\"%s\"}"), eventname);
    dispatch_state_subtopic(topic, payload);
}

void dispatch_state_val(const char* topic, hasp_event_t eventid, int32_t val)
{
    char payload[64];
    char eventname[8];

    Parser::get_event_name(eventid, eventname, sizeof(eventname));
    snprintf_P(payload, sizeof(payload), PSTR("{\"state\":\"%s\",\"val\":%d}"), eventname, val);
    dispatch_state_subtopic(topic, payload);
}

void dispatch_json_error(uint8_t tag, DeserializationError& jsonError)
{
    LOG_ERROR(tag, F(D_JSON_FAILED " %d"), jsonError);
    // const char * error = jsonError.c_str();
    // LOG_ERROR(tag, F(D_JSON_FAILED " %s"), error);
}

// p[x].b[y].attr=value
static inline bool dispatch_parse_button_attribute(const char* topic_p, const char* payload, bool update)
{
    long num;
    char* pEnd;
    uint8_t pageid, objid;

    if(*topic_p != 'p' && *topic_p != 'P') return false; // obligated p
    topic_p++;

    if(*topic_p == '[') { // optional brackets, TODO: remove
        topic_p++;
        num = strtol(topic_p, &pEnd, DEC);
        if(*pEnd != ']') return false; // obligated closing bracket
        pEnd++;

    } else {
        num = strtol(topic_p, &pEnd, DEC);
    }

    if(num < 0 || num > HASP_NUM_PAGES) return false; // page number must be valid

    pageid  = (uint8_t)num;
    topic_p = pEnd;

    if(*topic_p == '.') topic_p++; // optional separator

    if(*topic_p != 'b' && *topic_p != 'B') return false; // obligated b
    topic_p++;

    if(*topic_p == '[') { // optional brackets, TODO: remove
        topic_p++;
        num = strtol(topic_p, &pEnd, DEC);
        if(*pEnd != ']') return false; // obligated closing bracket
        pEnd++;
    } else {
        num = strtol(topic_p, &pEnd, DEC);
    }

    if(num < 0 || num > 255) return false; // id must be valid
    objid   = (uint8_t)num;
    topic_p = pEnd;

    if(*topic_p != '.') return false; // obligated separator
    topic_p++;

    hasp_process_attribute(pageid, objid, topic_p, payload, update);
    return true;
}

static void dispatch_input(const char* topic, const char* payload)
{
#if HASP_USE_GPIO > 0

    if(!Parser::is_only_digits(topic)) {
        LOG_WARNING(TAG_MSGR, F("Invalid pin %s"), topic);
        return;
    }

    // just output the pin state
    uint8_t pin = atoi(topic);
    if(gpio_input_pin_state(pin)) return;

    LOG_WARNING(TAG_GPIO, F(D_BULLET "Pin %d is not configured"), pin);

#endif
}

static void dispatch_output(const char* topic, const char* payload)
{
#if HASP_USE_GPIO > 0

    if(!Parser::is_only_digits(topic)) {
        LOG_WARNING(TAG_MSGR, F("Invalid pin %s"), topic);
        return;
    }

    uint8_t pin = atoi(topic);

    if(strlen(payload) > 0) {
        StaticJsonDocument<128> json;

        // Note: Deserialization needs to be (const char *) so the objects WILL be copied
        // this uses more memory but otherwise the mqtt receive buffer can get overwritten by the send buffer !!
        DeserializationError jsonError = deserializeJson(json, payload);

        if(jsonError) { // Couldn't parse incoming JSON command
            dispatch_json_error(TAG_MSGR, jsonError);
        } else {

            // Save the current state
            int32_t state_value;
            bool power_state;
            bool updated = false;

            if(!gpio_get_pin_state(pin, power_state, state_value)) {
                LOG_WARNING(TAG_GPIO, F(D_BULLET "Pin %d can not be set"), pin);
                return;
            }

            JsonVariant state      = json[F("state")];
            JsonVariant value      = json[F("val")];
            JsonVariant brightness = json[F("brightness")];

            // Check if the state needs to change
            if(!state.isNull() && power_state != Parser::is_true(state)) {
                power_state = Parser::is_true(state);
                updated     = true;
            }

            if(!value.isNull() && state_value != value.as<int32_t>()) {
                state_value = value.as<int32_t>();
                updated     = true;
            } else if(!brightness.isNull() && state_value != brightness.as<int32_t>()) {
                state_value = brightness.as<int32_t>();
                updated     = true;
            }

            // Set new state
            if(updated && gpio_set_pin_state(pin, power_state, state_value)) {
                return; // value was set and state output already in gpio_set_pin_state
            } else {
                // output the new state to the log
            }
        }
    }

    // just output this pin
    if(!gpio_output_pin_state(pin)) {
        LOG_WARNING(TAG_GPIO, F(D_BULLET "Pin %d is not configured"), pin);
    }

#endif
}

// static inline size_t dispatch_msg_length(size_t len)
// {
//     return (len / 64) * 64 + 64;
// }

// void dispatch_enqueue_message(const char* topic, const char* payload, size_t payload_len, uint8_t source)
// {
//     // Add new message to the queue
//     dispatch_message_t data;

//     size_t topic_len = strlen(topic);
//     data.topic       = (char*)hasp_calloc(sizeof(char), dispatch_msg_length(topic_len + 1));
//     data.payload     = (char*)hasp_calloc(sizeof(char), dispatch_msg_length(payload_len + 1));
//     data.source      = source;

//     if(!data.topic || !data.payload) {
//         LOG_ERROR(TAG_MQTT_RCV, D_ERROR_OUT_OF_MEMORY);
//         hasp_free(data.topic);
//         hasp_free(data.payload);
//         return;
//     }
//     memcpy(data.topic, topic, topic_len);
//     memcpy(data.payload, payload, payload_len);

//     {
//         size_t attempt = 0;
//         while(xQueueSend(message_queue, &data, (TickType_t)0) == errQUEUE_FULL && attempt < 100) {
//             delay(5);
//             attempt++;
//         };
//         if(attempt >= 100) {
//             LOG_ERROR(TAG_MSGR, D_ERROR_OUT_OF_MEMORY);
//         }
//     }
// }

// objectattribute=value
static void dispatch_command(const char* topic, const char* payload, bool update, uint8_t source)
{
    /* ================================= Standard payload commands ======================================= */

    if(dispatch_parse_button_attribute(topic, payload, update)) return; // matched pxby.attr, first for speed

    // check and execute commands from commands array
    for(int i = 0; i < nCommands; i++) {
        if(!strcasecmp_P(topic, commands[i].p_cmdstr)) {
            // LOG_DEBUG(TAG_MSGR, F("Command %d found in array !!!"), i);
            commands[i].func(topic, payload, source); /* execute command */
            return;
        }
    }

    /* =============================== Not standard payload commands ===================================== */

    if(topic == strstr_P(topic, PSTR("output"))) {
        dispatch_output(topic + 6, payload);

    } else if(topic == strstr_P(topic, PSTR("input"))) {
        dispatch_input(topic + 5, payload);

        // } else if(strcasecmp_P(topic, PSTR("screenshot")) == 0) {
        //     guiTakeScreenshot("/screenshot.bmp"); // Literal String

#if HASP_USE_CONFIG > 0

#if HASP_USE_WIFI > 0
    } else if(!strcmp_P(topic, FP_CONFIG_SSID) || !strcmp_P(topic, FP_CONFIG_PASS)) {
        StaticJsonDocument<64> settings;
        settings[topic] = payload;
        wifiSetConfig(settings.as<JsonObject>());
#endif // HASP_USE_WIFI

#if HASP_USE_MQTT > 0
    } else if(!strcmp_P(topic, PSTR("mqtthost")) || !strcmp_P(topic, PSTR("mqttport")) ||
              !strcmp_P(topic, PSTR("mqttuser")) || !strcmp_P(topic, PSTR("mqttpass")) ||
              !strcmp_P(topic, PSTR("hostname"))) {
        // char item[5];
        // memset(item, 0, sizeof(item));
        // strncpy(item, topic + 4, 4);

        StaticJsonDocument<64> settings;
        settings[topic + 4] = payload;
        mqttSetConfig(settings.as<JsonObject>());
#endif // HASP_USE_MQTT

#endif // HASP_USE_CONFIG
    } else {
        if(strlen(payload) == 0) {
            //    dispatch_simple_text_command(topic); // Could cause an infinite loop!
        }
        LOG_WARNING(TAG_MSGR, F(D_DISPATCH_COMMAND_NOT_FOUND " => %s"), topic, payload);
    }
}

// Parse one line of text and execute the command
static void dispatch_simple_text_command(const char* cmnd, uint8_t source)
{
    while(cmnd[0] == ' ' || cmnd[0] == '\t') cmnd++; // skip leading spaces
    if(cmnd[0] == '/' && cmnd[1] == '/') return;     // comment

    switch(cmnd[0]) {
        case '#':  // comment
        case '\0': // empty line
            break;

        case '{':
            dispatch_command("jsonl", cmnd, false, source);
            break;

        case '[':
            dispatch_command("json", cmnd, false, source);
            break; // comment

            // case ' ':
            //     dispatch_simple_text_command(cmnd, source);
            //     break;

        default: {
            size_t pos1 = std::string(cmnd).find("=");
            size_t pos2 = std::string(cmnd).find(" ");
            size_t pos  = 0;
            bool update = false;

            // Find what comes first, ' ' or '='
            if(pos1 != std::string::npos) {     // '=' found
                if(pos2 != std::string::npos) { // ' ' found
                    pos = (pos1 < pos2 ? pos1 : pos2);
                } else {
                    pos = pos1;
                }
                update = pos == pos1; // equal sign wins

            } else {
                pos = (pos2 != std::string::npos) ? pos2 : 0;
            }

            if(pos > 0) { // ' ' or '=' found
                char topic[64];
                memset(topic, 0, sizeof(topic));
                if(pos < sizeof(topic))
                    memcpy(topic, cmnd, pos);
                else
                    memcpy(topic, cmnd, sizeof(topic) - 1);

                // topic is before '=', payload is after '=' position
                update |= strlen(cmnd + pos + 1) > 0; // equal sign OR space with payload
                LOG_TRACE(TAG_MSGR, update ? F("%s=%s") : F("%s%s"), topic, cmnd + pos + 1);
                dispatch_topic_payload(topic, cmnd + pos + 1, update, source);
            } else {
                char empty_payload[1] = {0};
                LOG_TRACE(TAG_MSGR, cmnd);
                dispatch_topic_payload(cmnd, empty_payload, false, source);
            }
        }
    }
}

// Strip command/config prefix from the topic and process the payload
void dispatch_topic_payload(const char* topic, const char* payload, bool update, uint8_t source)
{
    if(!strcmp_P(topic, PSTR(MQTT_TOPIC_COMMAND)) || topic[0] == '\0') {
        dispatch_simple_text_command((char*)payload, source);
        return;
    }

    if(topic == strstr_P(topic, PSTR(MQTT_TOPIC_COMMAND "/"))) { // startsWith command/
        topic += 8u;
        dispatch_command(topic, (char*)payload, update, source);
        return;
    }

#if HASP_USE_CONFIG > 0
    if(topic == strstr_P(topic, PSTR("config/"))) { // startsWith config/
        topic += 7u;
        dispatch_config(topic, (char*)payload, source);
        return;
    }
#endif

#if defined(HASP_USE_CUSTOM) && HASP_USE_CUSTOM > 0
    if(topic == strstr_P(topic, PSTR(MQTT_TOPIC_CUSTOM "/"))) { // startsWith custom
        topic += 7u;
        custom_topic_payload(topic, (char*)payload, source);
        return;
    }
#endif

    dispatch_command(topic, (char*)payload, update, source); // dispatch as is
}

// void dispatch_output_group_state(uint8_t groupid, uint16_t state)
// {
//     char payload[64];
//     char number[16]; // Return the current state
//     char topic[8];
//     itoa(state, number, DEC);
//     snprintf_P(payload, sizeof(payload), PSTR("{\"group\":%d,\"state\":\"%s\"}"), groupid, number);

//     memcpy_P(topic, PSTR("output"), 7);
//     dispatch_state_subtopic(topic, payload);
// }

#if HASP_USE_CONFIG > 0
// Get or Set a part of the config.json file
void dispatch_config(const char* topic, const char* payload, uint8_t source)
{
    DynamicJsonDocument doc(512);
    char buffer[512];
    JsonObject settings;
    bool update;

    if(strlen(payload) == 0) {
        // Make sure we have a valid JsonObject to start from
        settings = doc.to<JsonObject>().createNestedObject(topic);
        update   = false;

    } else {
        DeserializationError jsonError = deserializeJson(doc, payload);
        if(jsonError) { // Couldn't parse incoming JSON command
            dispatch_json_error(TAG_MSGR, jsonError);
            return;
        }
        settings = doc.as<JsonObject>();
        update   = true;
    }

    if(strcasecmp_P(topic, PSTR("debug")) == 0) {
#if HASP_TARGET_ARDUINO
        if(update)
            debugSetConfig(settings);
        else
            debugGetConfig(settings);
#endif
    }

    else if(strcasecmp_P(topic, PSTR("gui")) == 0) {
        if(update)
            guiSetConfig(settings);
        else
            guiGetConfig(settings);
    }

    else if(strcasecmp_P(topic, PSTR("hasp")) == 0) {
        if(update)
            haspSetConfig(settings);
        else
            haspGetConfig(settings);
    }

#if HASP_USE_GPIO > 0
    else if(strcasecmp_P(topic, PSTR("gpio")) == 0) {
        if(update)
            gpioSetConfig(settings);
        else
            gpioGetConfig(settings);
    }
#endif

#if HASP_USE_WIFI > 0
    else if(strcasecmp_P(topic, PSTR("wifi")) == 0) {
        if(update)
            wifiSetConfig(settings);
        else
            wifiGetConfig(settings);
    }

    else if(strcasecmp_P(topic, PSTR("time")) == 0) {
        if(update)
            timeSetConfig(settings);
        else
            timeGetConfig(settings);
    }
#if HASP_USE_WIREGUARD > 0
    else if(strcasecmp_P(topic, PSTR(FP_WG)) == 0) {
        if(update)
            wgSetConfig(settings);
        else
            wgGetConfig(settings);
    }
#endif
#if HASP_USE_MQTT > 0
    else if(strcasecmp_P(topic, PSTR(FP_MQTT)) == 0) {
        if(update)
            mqttSetConfig(settings);
        else
            mqttGetConfig(settings);
    }
#endif
#if HASP_USE_TELNET > 0
    //   else if(strcasecmp_P(topic, PSTR("telnet")) == 0)
    //       telnetGetConfig(settings[F("telnet")]);
#endif
#if HASP_USE_MDNS > 0
    else if(strcasecmp_P(topic, PSTR("mdns")) == 0) {
        if(update)
            mdnsSetConfig(settings);
        else
            mdnsGetConfig(settings);
    }
#endif
#if HASP_USE_HTTP > 0 || HASP_USE_HTTP_ASYNC > 0
    else if(strcasecmp_P(topic, PSTR(FP_HTTP)) == 0) {
        if(update)
            httpSetConfig(settings);
        else
            httpGetConfig(settings);
    }
#endif
#if HASP_USE_FTP > 0
    else if(strcasecmp_P(topic, PSTR(FP_FTP)) == 0) {
        if(update)
            ftpSetConfig(settings);
        else
            ftpGetConfig(settings);
    }
#endif
#if HASP_USE_ARDUINOOTA > 0 || HASP_USE_HTTP_UPDATE > 0
    else if(strcasecmp_P(topic, PSTR(FP_OTA)) == 0) {
        if(update)
            otaSetConfig(settings);
        else
            otaGetConfig(settings);
    }
#endif
#endif

    // Send output
    if(!update) {
        char subtopic[8];
        settings.remove(FP_CONFIG_PASS); // hide password in output

        /* size_t size = */ serializeJson(doc, buffer, sizeof(buffer));
        memcpy_P(subtopic, PSTR("config"), 7);
        dispatch_state_subtopic(subtopic, buffer);
    }
}
#endif // HASP_USE_CONFIG

/********************************************** Output States ******************************************/

void dispatch_normalized_group_values(hasp_update_value_t& value)
{
    if(value.group == 0) return;

#if HASP_USE_GPIO > 0
    gpio_set_normalized_group_values(value); // Update GPIO states first
#endif
    object_set_normalized_group_values(value); // Update onsreen objects except originating obj

    LOG_VERBOSE(TAG_MSGR, F("GROUP %d value %d (%d-%d)"), value.group, value.val, value.min, value.max);
#if HASP_USE_GPIO > 0
    gpio_output_group_values(value.group); // Output new gpio values
#endif
}

/********************************************** Native Commands ****************************************/

void dispatch_screenshot(const char*, const char* filename, uint8_t source)
{
#if HASP_USE_SPIFFS > 0 || HASP_USE_LITTLEFS > 0

    if(strlen(filename) == 0) { // no filename given
        char tempfile[32];
        memcpy_P(tempfile, PSTR("/screenshot.bmp"), sizeof(tempfile));
        guiTakeScreenshot(tempfile);
    } else if(strlen(filename) > 31 || filename[0] != '/') { // Invalid filename
        LOG_WARNING(TAG_MSGR, F("D_FILE_SAVE_FAILED"), filename);
    } else { // Valid filename
        guiTakeScreenshot(filename);
    }

#else
    LOG_WARNING(TAG_MSGR, D_FILE_SAVE_FAILED, filename);
#endif
}

bool dispatch_json_variant(JsonVariant& json, uint8_t& savedPage, uint8_t source)
{
    if(json.is<JsonArray>()) { // handle json as an array of commands
        LOG_DEBUG(TAG_MSGR, "Json ARRAY");
        for(JsonVariant command : json.as<JsonArray>()) {
            dispatch_json_variant(command, savedPage, source);
        }

    } else if(json.is<JsonObject>()) { // handle json as a jsonl
        LOG_DEBUG(TAG_MSGR, "Json OBJECT");
        hasp_new_object(json.as<JsonObject>(), savedPage);

    } else if(json.is<std::string>()) { // handle json as a single command
        LOG_DEBUG(TAG_MSGR, "Json text = %s", json.as<std::string>().c_str());
        dispatch_simple_text_command(json.as<std::string>().c_str(), source);

    } else if(json.is<const char*>()) { // handle json as a single command
        LOG_DEBUG(TAG_MSGR, "Json text = %s", json.as<const char*>());
        dispatch_simple_text_command(json.as<const char*>(), source);

    } else if(json.isNull()) { // event handler not found
                               // nothing to do

    } else {
        LOG_WARNING(TAG_MSGR, "Json has unknown type");
        return false;
    }
    return true;
}

void dispatch_text_line(const char* payload, uint8_t source)
{

    {
        // StaticJsonDocument<1024> doc;
        size_t maxsize = (128u * ((strlen(payload) / 128) + 1)) + 512;
        DynamicJsonDocument doc(maxsize);

        // Note: Deserialization needs to be (const char *) so the objects WILL be copied
        // this uses more memory but otherwise the mqtt receive buffer can get overwritten by the send buffer !!
        DeserializationError jsonError = deserializeJson(doc, payload);
        doc.shrinkToFit();

        if(jsonError) {
            // dispatch_json_error(TAG_MSGR, jsonError);

        } else {
            JsonVariant json  = doc.as<JsonVariant>();
            uint8_t savedPage = haspPages.get();
            if(!dispatch_json_variant(json, savedPage, source)) {
                LOG_WARNING(TAG_MSGR, F(D_DISPATCH_COMMAND_NOT_FOUND), payload);
                // dispatch_simple_text_command(payload, source);
            }

            return;
        }
    }

    // Could not parse as json
    dispatch_simple_text_command(payload, source);
}

void dispatch_parse_json(const char*, const char* payload, uint8_t source)
{ // Parse an incoming JSON array into individual commands
  // StaticJsonDocument<2048> doc;
    size_t maxsize = (128u * ((strlen(payload) / 128) + 1)) + 512;
    DynamicJsonDocument doc(maxsize);
    DeserializationError jsonError = deserializeJson(doc, payload);
    doc.shrinkToFit();

    if(jsonError) {
        dispatch_json_error(TAG_MSGR, jsonError);
        return;
    }

    JsonVariant json  = doc.as<JsonVariant>();
    uint8_t savedPage = haspPages.get();
    if(!dispatch_json_variant(json, savedPage, TAG_EVENT)) {
        LOG_WARNING(TAG_MSGR, F(D_DISPATCH_COMMAND_NOT_FOUND), "");
    }
}

#ifdef ARDUINO
void dispatch_parse_jsonl(Stream& stream, uint8_t& saved_page_id)
#else
void dispatch_parse_jsonl(std::istream& stream, uint8_t& saved_page_id)
#endif
{
#ifdef ARDUINO
    stream.setTimeout(25);
#endif

    // StaticJsonDocument<1024> jsonl;
    DynamicJsonDocument jsonl(MQTT_MAX_PACKET_SIZE / 2 + 128);
    DeserializationError jsonError; // = deserializeJson(jsonl, stream);
    uint16_t line = 1;

    // while(jsonError == DeserializationError::Ok) {
    //     hasp_new_object(jsonl.as<JsonObject>(), saved_page_id);
    //     jsonError = deserializeJson(jsonl, stream);
    //     line++;
    // }

    while(1) {
        jsonError = deserializeJson(jsonl, stream);
        if(jsonError == DeserializationError::Ok) {
            hasp_new_object(jsonl.as<JsonObject>(), saved_page_id);
            line++;
        } else {
            break;
        }
    };

    /* For debugging purposes */
    if(jsonError == DeserializationError::EmptyInput) {
        LOG_DEBUG(TAG_MSGR, F(D_JSONL_SUCCEEDED));

    } else {
        LOG_ERROR(TAG_MSGR, F(D_JSONL_FAILED ": %s"), line, jsonError.c_str());
    }

    saved_jsonl_page = saved_page_id;
}

void dispatch_parse_jsonl(const char*, const char* payload, uint8_t source)
{
    if(source != TAG_MQTT) saved_jsonl_page = haspPages.get();
#if HASP_USE_CONFIG > 0 && HASP_TARGET_ARDUINO
    CharStream stream((char*)payload);
    // stream.setTimeout(10);
    dispatch_parse_jsonl(stream, saved_jsonl_page);
#else
    std::istringstream stream((char*)payload);
    dispatch_parse_jsonl(stream, saved_jsonl_page);
#endif
}

void dispatch_run_script(const char*, const char* payload, uint8_t source)
{
    const char* filename = payload;
    if(filename[0] == 'L' && filename[1] == ':') filename += 2; // strip littlefs drive letter

#if ARDUINO
#if HASP_USE_SPIFFS > 0 || HASP_USE_LITTLEFS > 0 || HASP_USE_SDCARD

    if(!HASP_FS.exists(filename)) {
        LOG_WARNING(TAG_MSGR, F(D_FILE_NOT_FOUND ": %s"), payload);
        return;
    }

    LOG_TRACE(TAG_MSGR, F(D_FILE_LOADING), payload);
    
    File cmdfile = HASP_FS.open(filename, "r");
    if(!cmdfile) {
        LOG_ERROR(TAG_MSGR, F(D_FILE_LOAD_FAILED), payload);
        return;
    }

    // if(!gui_acquire(pdMS_TO_TICKS(500))) {
    //     LOG_ERROR(TAG_MSGR, F(D_FILE_LOAD_FAILED), payload);
    //     return;
    // }

    // char buffer[512]; // use stack
    String buffer((char*)0); // use heap
    buffer.reserve(512);

    ReadBufferingStream bufferedFile{cmdfile, 512};
    cmdfile.seek(0);

    while(bufferedFile.available()) {
        size_t index = 0;
        buffer       = "";
        while(index < MQTT_MAX_PACKET_SIZE) {
            int c = bufferedFile.read();
            if(c < 0 || c == '\n' || c == '\r') { // CR or LF
                break;
            }
            buffer += (char)c;
            index++;
        }
        if(index > 0 && buffer.charAt(0) != '#') { // Check for comments
            dispatch_simple_text_command(buffer.c_str(), TAG_FILE);
        }
    }

    // gui_release();
    cmdfile.close();
    LOG_INFO(TAG_MSGR, F(D_FILE_LOADED), payload);
#else
    LOG_ERROR(TAG_MSGR, F(D_FILE_LOAD_FAILED), payload);
#endif
#else
    char path[strlen(filename) + 4];
    path[0] = '.';
    path[1] = '\0';
    strcat(path, filename);
#if defined(WINDOWS)
    path[1] = '\\';
#elif defined(POSIX)
    path[1] = '/';
#endif

    LOG_TRACE(TAG_HASP, F("Loading %s from disk..."), path);
    std::ifstream f(path); // taking file as inputstream
    if(f) {
        std::string line;
        while(std::getline(f, line)) {
            LOG_VERBOSE(TAG_HASP, line.c_str());
            if(!line.empty() && line[0] != '#') dispatch_simple_text_command(line.c_str(), TAG_FILE); // # for comments
        }
    } else {
        LOG_ERROR(TAG_MSGR, F(D_FILE_LOAD_FAILED), payload);
    }
    f.close();
    LOG_INFO(TAG_HASP, F("Loaded %s from disk"), path);
#endif
}

/*
void dispatch_fs(const char*, const char* payload, uint8_t source)
{
    StaticJsonDocument<512> json;

    // Note: Deserialization needs to be (const char *) so the objects WILL be copied
    // this uses more memory but otherwise the mqtt receive buffer can get overwritten by the send buffer !!
    DeserializationError jsonError = deserializeJson(json, payload);
    // json.shrinkToFit();

    if(!jsonError && json.is<JsonObject>()) { // Only JsonObject is valid
        JsonVariant action;

        const char* cmd = json["cmd"].as<const char*>();
        const char* src = json["src"].as<const char*>();
        const char* dst = json["dst"].as<const char*>();
        int res = 0;

        if(String(cmd) == "stat") {
            res = filesystem_vfs_file_exists(src);
        }
        if(String(cmd) == "rm") {
            res = filesystem_vfs_delete_file(src);
        }
        if(String(cmd) == "cp") {
            res = filesystem_vfs_copy_file(src, dst);
        }
        if(String(cmd) == "ls") {
            filesystem_list_path(src);
        }

        if(res) {
            LOG_WARNING(TAG_MSGR, "Succes");
        } else {
            LOG_WARNING(TAG_MSGR, "Failed");
        }
    }
}
*/

#if HASP_TARGET_PC
static void shell_command_thread(char* cmdline)
{
    // run the command
    FILE* pipe = popen(cmdline, "r");
    // free the string duplicated previously
    free(cmdline);
    if(!pipe) {
        LOG_ERROR(TAG_MSGR, F("Couldn't execute system command"));
        return;
    }
    // read each line, up to 1023 chars long
    char command[1024];
    while(fgets(command, sizeof(command), pipe) != NULL) {
        // strip newline character
        char* temp = command;
        while(*temp) {
            if(*temp == '\r' || *temp == '\n') {
                *temp = '\0';
                break;
            }
            temp++;
        }
        // run the command
        LOG_INFO(TAG_MSGR, F("Running '%s'"), command);
        dispatch_text_line(command, TAG_MSGR);
    }
    // close the pipe, check return code
    int status_code = pclose(pipe);
    if(status_code) {
        LOG_ERROR(TAG_MSGR, F("Process exited with non-zero return code %d"), status_code);
    }
}

void dispatch_shell_execute(const char*, const char* payload, uint8_t source)
{
    // must duplicate the string for thread's own usage
    char* command = strdup(payload);
    haspDevice.run_thread((void (*)(void*))shell_command_thread, (void*)command);
}
#endif

void dispatch_current_page()
{
    char topic[8];
    char payload[8];

    memcpy_P(topic, PSTR("page"), 5);
    snprintf_P(payload, sizeof(payload), PSTR("%d"), haspPages.get());
    dispatch_state_subtopic(topic, payload);
}

// Dispatch Page Get or Set
void dispatch_page_next(lv_scr_load_anim_t animation)
{
    haspPages.next(animation, 500, 0);
    dispatch_current_page();
}

void dispatch_page_prev(lv_scr_load_anim_t animation)
{
    haspPages.prev(animation, 500, 0);
    dispatch_current_page();
}

void dispatch_page_back(lv_scr_load_anim_t animation)
{
    haspPages.back(animation, 500, 0);
    dispatch_current_page();
}

void dispatch_set_page(uint8_t pageid)
{
    dispatch_set_page(pageid, LV_SCR_LOAD_ANIM_NONE, 500, 0);
}

void dispatch_set_page(uint8_t pageid, lv_scr_load_anim_t animation, uint32_t time, uint32_t delay)
{
    haspPages.set(pageid, animation, time, delay);
}

void dispatch_page(const char*, const char* payload, uint8_t source)
{
    if(!payload || strlen(payload) == 0) {
        dispatch_current_page(); // No payload, send current page
        return;
    }

    lv_scr_load_anim_t animation = LV_SCR_LOAD_ANIM_NONE;
    uint32_t time                = 500;
    uint32_t delay               = 0;
    uint8_t pageid               = Parser::haspPayloadToPageid(payload);

    if(pageid == 0) {
        StaticJsonDocument<128> json;

        // Note: Deserialization needs to be (const char *) so the objects WILL be copied
        // this uses more memory but otherwise the mqtt receive buffer can get overwritten by the send buffer !!
        DeserializationError jsonError = deserializeJson(json, payload);
        // json.shrinkToFit();

        if(!jsonError && json.is<JsonObject>()) { // Only JsonObject is valid
            JsonVariant prop;

            prop = json[F("page")];
            if(!prop.isNull()) pageid = Parser::haspPayloadToPageid(prop.as<const char*>());

            prop = json[F("transition")];
            if(!prop.isNull()) animation = (lv_scr_load_anim_t)prop.as<uint8_t>();

            prop = json[F("time")];
            if(!prop.isNull()) time = prop.as<uint32_t>();

            prop = json[F("delay")];
            if(!prop.isNull()) delay = prop.as<uint32_t>();
        }
    }

    dispatch_set_page(pageid, animation, time, delay);
}

// Clears a page id or the current page if empty
void dispatch_clear_page(const char*, const char* page, uint8_t source)
{
    if(!strcasecmp(page, "all")) {
#if !HASP_TARGET_PC
        hasp_init();
#else
        // workaround for "clearpage all" deadlocking or crashing on PC build (when called from non-LVGL thread)
        for(uint8_t pageid = 0; pageid <= HASP_NUM_PAGES; pageid++) {
            haspPages.clear(pageid);
        }
#endif
        return;
    }

    uint8_t pageid;
    if(strlen(page) == 0) {
        pageid = haspPages.get();
    } else {
        pageid = atoi(page);
    }
    haspPages.clear(pageid);
}

// Clears all fonts
void dispatch_clear_font(const char*, const char* payload, uint8_t source)
{
    dispatch_clear_page(NULL, "all", source);
    hasp_init();
    font_clear_list(payload);
}

void dispatch_dim(const char*, const char* level)
{
    // Set the current state
    if(strlen(level) != 0) haspDevice.set_backlight_level(atoi(level));

    char topic[8];
    char payload[8];

    memcpy_P(topic, PSTR("dim"), 4);
    snprintf_P(payload, sizeof(payload), PSTR("%d"), haspDevice.get_backlight_level());
    dispatch_state_subtopic(topic, payload);
}

void dispatch_moodlight(const char* topic, const char* payload, uint8_t source)
{
    // Set the current state
    if(strlen(payload) != 0) {
        StaticJsonDocument<128> json;

        // Note: Deserialization needs to be (const char *) so the objects WILL be copied
        // this uses more memory but otherwise the mqtt receive buffer can get overwritten by the send buffer !!
        DeserializationError jsonError = deserializeJson(json, payload);
        // json.shrinkToFit();

        if(jsonError) { // Couldn't parse incoming JSON command
            dispatch_json_error(TAG_MSGR, jsonError);
        } else {
            JsonVariant state = json[F("state")];
            if(!state.isNull()) moodlight.power = Parser::is_true(state);

            if(!json["r"].isNull()) moodlight.rgbww[0] = json["r"].as<uint8_t>();
            if(!json["g"].isNull()) moodlight.rgbww[1] = json["g"].as<uint8_t>();
            if(!json["b"].isNull()) moodlight.rgbww[2] = json["b"].as<uint8_t>();
            if(!json["brightness"].isNull()) moodlight.brightness = json["brightness"].as<uint8_t>();

            if(!json[F("color")].isNull()) {
                lv_color32_t color;
                if(Parser::haspPayloadToColor(json[F("color")].as<const char*>(), color)) {
                    moodlight.rgbww[0] = color.ch.red;
                    moodlight.rgbww[1] = color.ch.green;
                    moodlight.rgbww[2] = color.ch.blue;
                }
            }

#if HASP_USE_GPIO > 0
            gpio_set_moodlight(moodlight);
#endif
        }
    }

    // Return the current state
    char buffer[128];
    char out_topic[16];
    memcpy_P(out_topic, PSTR("moodlight"), 10);
    snprintf_P(buffer, sizeof(buffer),
               PSTR("{\"state\":\"%s\",\"brightness\":%u,\"color\":\"#%02x%02x%02x\",\"r\":%u,\"g\":%u,\"b\":%u}"),
               moodlight.power ? "on" : "off", moodlight.brightness, moodlight.rgbww[0], moodlight.rgbww[1],
               moodlight.rgbww[2], moodlight.rgbww[0], moodlight.rgbww[1], moodlight.rgbww[2]);
    dispatch_state_subtopic(out_topic, buffer);
}

// void dispatch_backlight_obsolete(const char* topic, const char* payload, uint8_t source)
// {
//     LOG_WARNING(TAG_MSGR, F(D_ATTRIBUTE_OBSOLETE D_ATTRIBUTE_INSTEAD), topic,
//                 "backlight"); // TODO: obsolete dim, light and brightness
//     dispatch_backlight(topic, payload, source);
// }

void dispatch_backlight(const char*, const char* payload, uint8_t source)
{
    bool power    = haspDevice.get_backlight_power();
    uint8_t level = haspDevice.get_backlight_level();

    // Set the current state
    if(strlen(payload) != 0) {
        StaticJsonDocument<128> json;

        // Note: Deserialization needs to be (const char *) so the objects WILL be copied
        // this uses more memory but otherwise the mqtt receive buffer can get overwritten by the send buffer !!
        DeserializationError jsonError = deserializeJson(json, payload);
        // json.shrinkToFit();

        if(jsonError) { // Couldn't parse incoming payload as json
            if(Parser::is_only_digits(payload)) {
                uint8_t temp_level = atoi(payload);
                if(temp_level <= 1)
                    power = temp_level == 1;
                else
                    level = temp_level;
            } else {
                power = Parser::is_true(payload);
            }

        } else {

            // plain numbers are parsed as valid json object
            if(json.is<uint8_t>()) {
                uint8_t temp_level = json.as<uint8_t>();
                if(temp_level <= 1)
                    power = temp_level == 1;
                else
                    level = temp_level;

                // true and false are parsed as valid json object
            } else if(json.is<bool>()) {
                power = json.as<bool>();

            } else {
                JsonVariant state      = json[F("state")];
                JsonVariant brightness = json[F("brightness")];

                if(!state.isNull()) power = Parser::is_true(state);
                if(!brightness.isNull()) level = brightness.as<uint8_t>();
            }
        }
    }

    // toggle power and wakeup touch if changed
    if(power) haspDevice.set_backlight_level(level); // set level before power on
    if(haspDevice.get_backlight_power() != power) {
        haspDevice.set_backlight_power(power);
        hasp_set_wakeup_touch(!power);
    }
    if(!power) haspDevice.set_backlight_level(level); // set level after power off

    // Return the current state
    char topic[10];
    memcpy_P(topic, PSTR("backlight"), 10);
    dispatch_state_brightness(topic, (hasp_event_t)haspDevice.get_backlight_power(), haspDevice.get_backlight_level());
}

void dispatch_web_update(const char*, const char* espOtaUrl, uint8_t source)
{
#if HASP_USE_HTTP_UPDATE > 0
    LOG_TRACE(TAG_MSGR, F(D_OTA_CHECK_UPDATE), espOtaUrl);
    ota_http_update(espOtaUrl);
#endif
}

void dispatch_antiburn(const char*, const char* payload, uint8_t source)
{
    if(strlen(payload) >= 0) {
        StaticJsonDocument<128> json;

        // Note: Deserialization needs to be (const char *) so the objects WILL be copied
        // this uses more memory but otherwise the mqtt receive buffer can get overwritten by the send buffer !!
        DeserializationError jsonError = deserializeJson(json, payload);
        // json.shrinkToFit();
        int32_t count   = 30;
        uint32_t period = 1000;
        bool state      = false;

        if(jsonError) { // Couldn't parse incoming payload as json
            state = Parser::is_true(payload);
        } else {
            if(json.is<uint8_t>()) { // plain numbers are parsed as valid json object
                state = json.as<uint8_t>();

            } else if(json.is<bool>()) { // true and false are parsed as valid json object
                state = json.as<bool>();

            } else { // other text
                JsonVariant key = json[F("state")];
                if(!key.isNull()) state = Parser::is_true(key);
            }
        }
        hasp_set_antiburn(state ? count : 0, period); // ON = 30 cycles of 1000 milli seconds (i.e. 30 sec)
    }

    dispatch_state_antiburn(hasp_get_antiburn()); // Always publish the current state in response
}

// restart the device
void dispatch_reboot(bool saveConfig)
{
#if HASP_USE_CONFIG > 0
    if(saveConfig) configWrite();
#endif
#if HASP_USE_MQTT > 0 && defined(ARDUINO)
    mqttStop(); // Stop the MQTT Client first
#endif
#if HASP_USE_TELNET > 0
    telnetStop();
#endif
#if HASP_USE_CONFIG > 0
    debugStop();
#endif
#if HASP_USE_WIFI > 0 || HASP_USE_ETHERNET > 0
    networkStop();
#endif
    LOG_VERBOSE(TAG_MSGR, F("-------------------------------------"));
    LOG_TRACE(TAG_MSGR, F(D_DISPATCH_REBOOT));

#if HASP_TARGET_PC
    fflush(stdout);
#else
    Serial.flush();
#endif

    // halRestartMcu();
    haspDevice.reboot();
}

/******************************************* Command Wrapper Functions *********************************/

// Periodically publish a JSON string indicating sensor status
void dispatch_send_sensordata(const char*, const char*, uint8_t source)
{
#if HASP_USE_MQTT > 0

    StaticJsonDocument<1024> doc;

    time_t rawtime;
    time(&rawtime);
    char buffer[80];
    struct tm* timeinfo = localtime(&rawtime);

    strftime(buffer, sizeof(buffer), "%FT%T", timeinfo);
    doc[F("time")] = buffer;

    long uptime         = haspDevice.get_uptime();
    doc[F("uptimeSec")] = (uint32_t)uptime;

    uint32_t seconds = uptime % 60;
    uint32_t minutes = uptime / 60;
    uint32_t hours   = minutes / 60;
    uint32_t days    = hours / 24;
    minutes          = minutes % 60;
    hours            = hours % 24;
    snprintf_P(buffer, sizeof(buffer), PSTR("%dT%02d:%02d:%02d"), days, hours, minutes, seconds);
    doc[F("uptime")] = buffer;

    haspDevice.get_sensors(doc);

#if defined(HASP_USE_CUSTOM) && HASP_USE_CUSTOM > 0
    custom_get_sensors(doc);
#endif

    //     JsonObject input = doc.createNestedObject(F("input"));
    //     JsonArray relay  = doc.createNestedArray(F("power"));
    //     JsonArray led    = doc.createNestedArray(F("light"));
    //     JsonArray dimmer = doc.createNestedArray(F("dim"));

    // #if HASP_USE_GPIO > 0
    //     gpio_discovery(input, relay, led, dimmer);
    // #endif

    char data[1024];
    size_t len = serializeJson(doc, data);
    (void)len; // unused

    switch(mqtt_send_state(MQTT_TOPIC_SENSORS, data)) {
        case MQTT_ERR_OK:
            LOG_TRACE(TAG_MQTT_PUB, F(MQTT_TOPIC_SENSORS " => %s"), data);
            break;
        case MQTT_ERR_PUB_FAIL:
            LOG_ERROR(TAG_MQTT_PUB, F(D_MQTT_FAILED " " MQTT_TOPIC_SENSORS " => %s"), data);
            break;
        case MQTT_ERR_NO_CONN:
            LOG_ERROR(TAG_MQTT, F(D_MQTT_NOT_CONNECTED));
            break;
        default:
            LOG_ERROR(TAG_MQTT, F(D_ERROR_UNKNOWN));
    }
    dispatchSecondsToNextSensordata = dispatch_setings.teleperiod;

#endif
}

void dispatch_queue_discovery(const char*, const char*, uint8_t source)
{
    long seconds = HASP_RANDOM(10);
    if(dispatchSecondsToNextTeleperiod == seconds) seconds++;
    if(dispatchSecondsToNextSensordata == seconds) seconds++;
    LOG_VERBOSE(TAG_MSGR, F("Discovery queued in %d seconds"), seconds);
    dispatchSecondsToNextDiscovery = seconds;
}

void dispatch_get_discovery_data(JsonDocument& doc)
{
    char buffer[64];

    doc[F("node")]   = haspDevice.get_hostname();
    doc[F("mdl")]    = haspDevice.get_model();
    doc[F("mf")]     = F(D_MANUFACTURER);
    doc[F("hwid")]   = haspDevice.get_hardware_id();
    doc[F("pages")]  = haspPages.count();
    doc[F("sw")]     = haspDevice.get_version();
    doc[F("node_t")] = String("hasp/") + haspDevice.get_hostname() + "/";

#if HASP_USE_HTTP > 0
    network_get_ipaddress(buffer, sizeof(buffer));
    doc[F("uri")] = String(F("http://")) + String(buffer);
#elif HASP_TARGET_PC
    doc[F("uri")] = "http://google.pt";
#endif

    JsonObject input = doc.createNestedObject(F("input"));
    JsonArray relay  = doc.createNestedArray(F("power"));
    JsonArray led    = doc.createNestedArray(F("light"));
    JsonArray dimmer = doc.createNestedArray(F("dim"));
    JsonArray event  = doc.createNestedArray(F("event"));

#if HASP_USE_GPIO > 0
    gpio_discovery(input, relay, led, dimmer, event);
#endif
}

// Periodically publish a JSON string facilitating plate discovery
void dispatch_send_discovery(const char*, const char*, uint8_t source)
{
#if HASP_USE_MQTT > 0
    StaticJsonDocument<1024> doc;
    char data[1024];
    char buffer[64];

    dispatch_get_discovery_data(doc);
    size_t len = serializeJson(doc, data);

    switch(mqtt_send_discovery(data, len)) {
        case MQTT_ERR_OK:
            LOG_TRACE(TAG_MQTT_PUB, F(MQTT_TOPIC_DISCOVERY " => %s"), data);
            break;
        case MQTT_ERR_PUB_FAIL:
            LOG_ERROR(TAG_MQTT_PUB, F(D_MQTT_FAILED " " MQTT_TOPIC_DISCOVERY " => %s"), data);
            break;
        case MQTT_ERR_NO_CONN:
            LOG_ERROR(TAG_MQTT, F(D_MQTT_NOT_CONNECTED));
            break;
        default:
            LOG_ERROR(TAG_MQTT, F(D_ERROR_UNKNOWN));
    }
    dispatchSecondsToNextDiscovery = dispatch_setings.teleperiod * 2 + HASP_RANDOM(10);

#endif
}

// Periodically publish a JSON string indicating system status
void dispatch_statusupdate(const char*, const char*, uint8_t source)
{
#if HASP_USE_MQTT > 0

    char data[400];
    char topic[16];
    {
        char buffer[128];

        hasp_get_sleep_payload(hasp_get_sleep_state(), topic);
        snprintf_P(data, sizeof(data), PSTR("{\"node\":\"%s\",\"idle\":\"%s\",\"version\":\"%s\",\"uptime\":%lu,"),
                   haspDevice.get_hostname(), topic, haspDevice.get_version(),
                   long(millis() / 1000)); // \"status\":\"available\",

#if HASP_USE_WIFI > 0 || HASP_USE_ETHERNET > 0
        network_get_statusupdate(buffer, sizeof(buffer));
        strcat(data, buffer);

        // snprintf_P(buffer, sizeof(buffer), PSTR("\"mac\":\"%s\","), halGetMacAddress(0, ":").c_str());
        // strcat(data, buffer);
#endif

        snprintf_P(buffer, sizeof(buffer), PSTR("\"heapFree\":%u,\"heapFrag\":%u,\"core\":\"%s\","),
                   haspDevice.get_free_heap(), haspDevice.get_heap_fragmentation(), haspDevice.get_core_version());
        strcat(data, buffer);

        snprintf_P(buffer, sizeof(buffer), PSTR("\"canUpdate\":\"false\",\"page\":%u,\"numPages\":%u,"),
                   haspPages.get(), haspPages.count());
        strcat(data, buffer);

        // #if defined(ARDUINO_ARCH_ESP8266)
        //         snprintf_P(buffer, sizeof(buffer), PSTR("\"espVcc\":%.2f,"), (float)ESP.getVcc() / 1000);
        //         strcat(data, buffer);
        // #endif

        snprintf_P(buffer, sizeof(buffer), PSTR("\"tftDriver\":\"%s\",\"tftWidth\":%u,\"tftHeight\":%u}"),
                   haspTft.get_tft_model(), haspTft.width(), haspTft.height());
        strcat(data, buffer);
    }

    memcpy_P(topic, PSTR("statusupdate"), 13);
    dispatch_state_subtopic(topic, data);
    dispatchSecondsToNextTeleperiod = dispatch_setings.teleperiod;

    /* if(updateEspAvailable) {
            mqttStatusPayload += F("\"updateEspAvailable\":true,");
        } else {
            mqttStatusPayload += F("\"updateEspAvailable\":false,");
        }
    */

#endif
}

void dispatch_current_state(uint8_t source)
{
    dispatch_current_page();
    dispatch_idle_state(hasp_get_sleep_state());
    dispatch_state_antiburn(hasp_get_antiburn());

    // delayed published topic
    dispatchSecondsToNextTeleperiod = 0;
    dispatchSecondsToNextSensordata = 1;
    dispatchSecondsToNextDiscovery  = 2;
}

// Format filesystem and erase EEPROM
bool dispatch_factory_reset()
{
    bool formatted = true;
    bool erased    = true;
    bool cleared   = true;

#if ESP32
    erased = nvs_clear_user_config();
#endif

#if HASP_USE_SPIFFS > 0 || HASP_USE_LITTLEFS > 0
    formatted = HASP_FS.format();
    if(formatted) filesystemSetupFiles();
#endif

#if HASP_USE_EEPROM > 0
    erased = configClearEeprom();
#endif

    return formatted && erased && cleared;
}

void dispatch_calibrate(const char*, const char*, uint8_t source)
{
    guiCalibrate();
}

void dispatch_sleep(const char*, const char*, uint8_t source)
{
    hasp_set_wakeup_touch(false);
}

void dispatch_idle_state(uint8_t state)
{
    char topic[8];
    char buffer[8];
    memcpy_P(topic, PSTR("idle"), 5);
    hasp_get_sleep_payload(state, buffer);
    dispatch_state_subtopic(topic, buffer);
}

void dispatch_idle(const char*, const char* payload, uint8_t source)
{
    if(payload && strlen(payload)) {
        uint8_t state = HASP_SLEEP_LAST;
        if(!strcmp_P(payload, "off")) {
            hasp_set_sleep_state(HASP_SLEEP_OFF);
            dispatch_run_script(NULL, "L:/idle_off.cmd", source);
        } else if(!strcmp_P(payload, "short")) {
            hasp_set_sleep_state(HASP_SLEEP_SHORT);
            dispatch_run_script(NULL, "L:/idle_short.cmd", source);
        } else if(!strcmp_P(payload, "long")) {
            hasp_set_sleep_state(HASP_SLEEP_LONG);
            dispatch_run_script(NULL, "L:/idle_long.cmd", source);
        } else {
            LOG_WARNING(TAG_MSGR, F("Invalid idle value %s"), payload);
            return;
        }
    }

    dispatch_idle_state(hasp_get_sleep_state()); // always send the current state
}

void dispatch_wakeup(uint8_t source)
{
    //  if(hasp_get_sleep_state() == HASP_SLEEP_OFF) return;
    dispatch_idle(NULL, "off", source);
}

void dispatch_wakeup_obsolete(const char* topic, const char*, uint8_t source)
{
    LOG_WARNING(TAG_MSGR, F(D_ATTRIBUTE_OBSOLETE D_ATTRIBUTE_INSTEAD), topic,
                "idle=off"); // TODO: obsolete wakeup
    dispatch_wakeup(source);
    hasp_set_wakeup_touch(false);
}

void dispatch_reboot(const char*, const char*, uint8_t source)
{
    dispatch_reboot(true);
}

void dispatch_factory_reset(const char*, const char*, uint8_t source)
{
    dispatch_factory_reset();
    delay(500);
    dispatch_reboot(false); // don't save running config
}

void dispatch_theme(const char*, const char* themeid, uint8_t source)
{
    hasp_set_theme(atoi(themeid));
}

void dispatch_service(const char*, const char* payload, uint8_t source)
{
#if HASP_USE_TELNET > 0
    if(!strcmp_P(payload, "start telnet")) {
        telnetStart();
    } else if(!strcmp_P(payload, "stop telnet")) {
        telnetStop();
    }
#endif

#if HASP_USE_FTP > 0
    if(!strcmp_P(payload, "start ftp")) {
        ftpStart();
    } else if(!strcmp_P(payload, "stop ftp")) {
        ftpStop();
    }
#endif

#if HASP_USE_HTTP > 0 || HASP_USE_HTTP_ASYNC > 0
    if(!strcmp_P(payload, "start http")) {
        httpStart();
    } else if(!strcmp_P(payload, "stop http")) {
        httpStop();
    }
#endif

#if HASP_USE_MQTT > 0 && defined(HASP_USE_ESP_MQTT)
    if(!strcmp_P(payload, "start mqtt")) {
        mqttStart();
    } else if(!strcmp_P(payload, "stop mqtt")) {
        mqttStop();
    }
#endif

#if ARDUINO && HASP_USE_CONSOLE
    if(!strcmp_P(payload, "start console")) {
        consoleStart();
    } else if(!strcmp_P(payload, "stop console")) {
        consoleStop();
    }
#endif
}

/******************************************* Commands builder *******************************************/

static void dispatch_add_command(const char* p_cmdstr, void (*func)(const char*, const char*, uint8_t))
{
    if(nCommands >= sizeof(commands) / sizeof(haspCommand_t)) {
        LOG_FATAL(TAG_MSGR, F("CMD_OVERFLOW %d"), nCommands); // Needs to be in curly braces
    } else {
        commands[nCommands].p_cmdstr = p_cmdstr;
        commands[nCommands].func     = func;
        nCommands++;
    }
}

void dispatchSetup()
{
    // In order of importance : commands are NOT case-sensitive
    // The command.func() call will receive the full topic and payload parameters!

    LOG_TRACE(TAG_MSGR, F(D_SERVICE_STARTING));

    /* WARNING: remember to expand the commands array when adding new commands */
    dispatch_add_command(PSTR("json"), dispatch_parse_json);
    dispatch_add_command(PSTR("jsonl"), dispatch_parse_jsonl);
    dispatch_add_command(PSTR("page"), dispatch_page);
    dispatch_add_command(PSTR("backlight"), dispatch_backlight);
    dispatch_add_command(PSTR("moodlight"), dispatch_moodlight);
    dispatch_add_command(PSTR("idle"), dispatch_idle);
    dispatch_add_command(PSTR("sleep"), dispatch_sleep);
    dispatch_add_command(PSTR("statusupdate"), dispatch_statusupdate);
    dispatch_add_command(PSTR("clearpage"), dispatch_clear_page);
    dispatch_add_command(PSTR("clearfont"), dispatch_clear_font);
    dispatch_add_command(PSTR("sensors"), dispatch_send_sensordata);
    dispatch_add_command(PSTR("theme"), dispatch_theme);
    dispatch_add_command(PSTR("run"), dispatch_run_script);
    // dispatch_add_command(PSTR("fs"), dispatch_fs);
#if HASP_TARGET_PC
    dispatch_add_command(PSTR("shell"), dispatch_shell_execute);
#endif
    dispatch_add_command(PSTR("service"), dispatch_service);
    dispatch_add_command(PSTR("antiburn"), dispatch_antiburn);
    dispatch_add_command(PSTR("calibrate"), dispatch_calibrate);
    dispatch_add_command(PSTR("update"), dispatch_web_update);
    dispatch_add_command(PSTR("reboot"), dispatch_reboot);
    dispatch_add_command(PSTR("restart"), dispatch_reboot);
    dispatch_add_command(PSTR("screenshot"), dispatch_screenshot);
    dispatch_add_command(PSTR("discovery"), dispatch_queue_discovery);
    dispatch_add_command(PSTR("factoryreset"), dispatch_factory_reset);

    /* obsolete commands */
    // dispatch_add_command(PSTR("dim"), dispatch_backlight_obsolete);
    // dispatch_add_command(PSTR("brightness"), dispatch_backlight_obsolete);
    // dispatch_add_command(PSTR("light"), dispatch_backlight_obsolete);
    dispatch_add_command(PSTR("wakeup"), dispatch_wakeup_obsolete); // used in CC

#if HASP_USE_SPIFFS > 0 || HASP_USE_LITTLEFS > 0
#if defined(ARDUINO_ARCH_ESP32)
    dispatch_add_command(PSTR("unzip"), filesystemUnzip);
#endif
#endif
#if HASP_USE_CONFIG > 0 && HASP_TARGET_ARDUINO
    dispatch_add_command(PSTR("setupap"), oobeFakeSetup);
#endif
    /* WARNING: remember to expand the commands array when adding new commands */

    LOG_INFO(TAG_MSGR, F(D_SERVICE_STARTED));
}

IRAM_ATTR void dispatchLoop()
{
    // UBaseType_t msg_count = uxQueueMessagesWaiting(message_queue));
    // if(msg_count == 0) return;

    // dispatch_message_t data;
    // while(xQueueReceive(message_queue, &data, (TickType_t)0)) {
    //     LOG_WARNING(TAG_MSGR, F("[%d] QUE %s => %s"), msg_count, data.topic, data.payload);
    //     size_t length = strlen(data.payload);
    //     dispatch_topic_payload(data.topic, data.payload, length > 0, data.source);
    //     hasp_free(data.topic);
    //     hasp_free(data.payload);
    //     // delay(1);
    // }
}

#if 1 || ARDUINO
void dispatchEverySecond()
{
#if HASP_USE_MQTT > 0
    if(dispatchSecondsToNextTeleperiod > 1) {
        dispatchSecondsToNextTeleperiod--;
    } else if(dispatch_setings.teleperiod > 0 && mqttIsConnected()) {
        dispatch_statusupdate(NULL, NULL, TAG_MSGR);
    }

    if(dispatchSecondsToNextSensordata > 1) {
        dispatchSecondsToNextSensordata--;
    } else if(dispatch_setings.teleperiod > 0 && mqttIsConnected()) {
        dispatch_send_sensordata(NULL, NULL, TAG_MSGR);
    }

    if(dispatchSecondsToNextDiscovery > 1) {
        dispatchSecondsToNextDiscovery--;
    } else if(dispatch_setings.teleperiod > 0 && mqttIsConnected()) {
        dispatch_send_discovery(NULL, NULL, TAG_MSGR);
    }
#endif
}
#else
#include <chrono>
std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
void everySecond()
{
    if(dispatch_setings.teleperiod > 0) {
        std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
        std::chrono::seconds elapsed              = std::chrono::duration_cast<std::chrono::seconds>(end - begin);

        if(elapsed.count() >= dispatch_setings.teleperiod) {
            std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
            dispatch_statusupdate(NULL, NULL);
        }
    }
}
#endif