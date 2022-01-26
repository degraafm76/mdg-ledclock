#ifndef MQTT_H
#define MQTT_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <structs.h>
#include <global_vars.h>

// Variable used to store the mqtt state of the whole clock display
extern boolean display_state;

// Variables used to store the mqtt state, the brightness and the color of the specific clock items
extern boolean hour_state;
extern uint8_t hour_brightness;
extern uint8_t hour_rgb_red;
extern uint8_t hour_rgb_green;
extern uint8_t hour_rgb_blue;
extern boolean minute_state;
extern uint8_t minute_brightness;
extern uint8_t minute_rgb_red;
extern uint8_t minute_rgb_green;
extern uint8_t minute_rgb_blue;
extern boolean second_state;
extern uint8_t second_brightness;
extern uint8_t second_rgb_red;
extern uint8_t second_rgb_green;
extern uint8_t second_rgb_blue;
extern boolean hourmarks_state;
extern uint8_t hourmarks_brightness;
extern uint8_t hourmarks_rgb_red;
extern uint8_t hourmarks_rgb_green;
extern uint8_t hourmarks_rgb_blue;
extern boolean background_state;
extern uint8_t background_brightness;
extern uint8_t background_rgb_red;
extern uint8_t background_rgb_green;
extern uint8_t background_rgb_blue;

// Arrays with above variables
extern boolean array_state[];
extern uint8_t array_brightness[];
extern uint8_t array_rgb_red[];
extern uint8_t array_rgb_green[];
extern uint8_t array_rgb_blue[];
extern int array_index;
extern boolean topic_match;
extern const char *state_topic;

// MQTT: topics
// Ledclock all state
extern const PROGMEM char *MQTT_DISPLAY_STATE_TOPIC;
extern const PROGMEM char *MQTT_DISPLAY_COMMAND_TOPIC;
// Hour state
extern const PROGMEM char *MQTT_HOUR_STATE_TOPIC;
extern const PROGMEM char *MQTT_HOUR_COMMAND_TOPIC;
// Minute state
extern const PROGMEM char *MQTT_MINUTE_STATE_TOPIC;
extern const PROGMEM char *MQTT_MINUTE_COMMAND_TOPIC;
// Second state
extern const PROGMEM char *MQTT_SECOND_STATE_TOPIC;
extern const PROGMEM char *MQTT_SECOND_COMMAND_TOPIC;
// Background state
extern const PROGMEM char *MQTT_BACKGROUND_STATE_TOPIC;
extern const PROGMEM char *MQTT_BACKGROUND_COMMAND_TOPIC;
// Hourmarks state
extern const PROGMEM char *MQTT_HOURMARKS_STATE_TOPIC;
extern const PROGMEM char *MQTT_HOURMARKS_COMMAND_TOPIC;
//Light sensor
extern const PROGMEM char *MQTT_LIGHT_SENSOR_STATE_TOPIC;

// payloads by default (on/off)
extern const PROGMEM char *LIGHT_ON;
extern const PROGMEM char *LIGHT_OFF;

void setColor(uint8_t, uint8_t, uint8_t, int);

void publishSensorState();

void publishState(int, const char*);

boolean MQTTconnect();

void callback(char*, byte*, unsigned int);



#endif
