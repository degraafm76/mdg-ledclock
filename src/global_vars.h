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
extern Config config; // <- global configuration object
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
#define MAX_NUM_ARGS 8	  //Maximum arguments
extern char line[LINE_BUF_SIZE];
extern char args[MAX_NUM_ARGS][ARG_BUF_SIZE];
extern boolean serial_input_error_flag ;
extern boolean reset_flag;

#endif
