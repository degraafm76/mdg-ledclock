#ifndef GLOBAL_VARS_H
#define GLOBAL_VARS_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <structs.h>
#include <config.h>
#include <ezTime.h>

extern WiFiClient espClient;
extern BearSSL::WiFiClientSecure TLSClient;
extern PubSubClient mqttClient; //uninitialised pubsub client instance. The client is initialised as TLS or espClient in setup()
extern Config config;           // <- global configuration object
extern Timezone tz;

extern clockdisplay clockdisplays[CLOCK_DISPLAYS]; //Array of clock displays
extern schedule schedules[SCHEDULES];

extern const PROGMEM char *JSON_CONFIG_FILE;
extern const PROGMEM char *JSON_SCHEDULES_FILE;
extern const PROGMEM char *JSON_CLOCK_DISPLAYS_FILE;

//MQTT
extern long lastReconnectAttempt;
extern boolean MQTTConnected; // Variable to store MQTT connected state

#define MSG_BUFFER_SIZE 16   //MQTT Payload buffer
#define TOPIC_BUFFER_SIZE 64 //MQTT Topic buffer

//MQTT Payload buffer
extern char m_msg_buffer[MSG_BUFFER_SIZE];
//MQTT Topic buffer
extern char m_topic_buffer[TOPIC_BUFFER_SIZE];

//WiFi
extern boolean wifiConnected; // Variable to store wifi connected state
extern unsigned char bssid[6];
extern byte channel;
extern int rssi;
extern String apPassword;

//Serial
extern String rxString;
#define LINE_BUF_SIZE 128 //Maximum serial input string length
#define ARG_BUF_SIZE 128  //Maximum argument input string length
#define MAX_NUM_ARGS 8    //Maximum arguments
extern char line[LINE_BUF_SIZE];
extern char args[MAX_NUM_ARGS][ARG_BUF_SIZE];
extern boolean serial_input_error_flag;
extern boolean reset_flag;

//Lightsensor
extern unsigned long lastExecutedMillis_brightness;      // Variable to save the last executed time
extern unsigned long lastExecutedMillis_lightsensorMQTT; // Variable to save the last executed time
extern float lightReading;                               // Variable to store the ambient light value
extern float lux;                                        //LUX

// Define the number of samples to keep track of. The higher the number, the
// more the readings will be smoothed, but the slower the output will respond to
// the input. Using a constant rather than a normal variable lets us use this
// value to determine the size of the readings array.
#define NUMREADINGS 100

extern int readings[NUMREADINGS]; // the readings from the analog input
extern int readIndex;             // the index of the current reading
extern int total;                 // the running total
extern int average;               // the average

//Keep track of the total runtime in mili's
extern unsigned long currentMillis;

#endif
