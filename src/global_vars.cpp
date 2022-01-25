#include <global_vars.h>

WiFiClient espClient;
BearSSL::WiFiClientSecure TLSClient;
PubSubClient mqttClient; //uninitialised pubsub client instance. The client is initialised as TLS or espClient in setup()
Config config; // <- global configuration object
Timezone tz;

clockdisplay clockdisplays[CLOCK_DISPLAYS]; //Array of clock displays
schedule schedules[SCHEDULES];

const PROGMEM char *JSON_CONFIG_FILE = "/config.json";
const PROGMEM char *JSON_SCHEDULES_FILE = "/schedules.json";
const PROGMEM char *JSON_CLOCK_DISPLAYS_FILE = "/clkdisplays.json";

//MQTT
long lastReconnectAttempt = 0;
boolean MQTTConnected = false; // Variable to store MQTT connected state
char m_msg_buffer[MSG_BUFFER_SIZE];
char m_topic_buffer[TOPIC_BUFFER_SIZE];

//WiFi
boolean wifiConnected = false; // Variable to store wifi connected state
unsigned char bssid[6];
byte channel;
int rssi = -999;
String apPassword;

//Serial
String rxString = "";
#define LINE_BUF_SIZE 128 //Maximum serial input string length
#define ARG_BUF_SIZE 128  //Maximum argument input string length
#define MAX_NUM_ARGS 8    //Maximum arguments
char line[LINE_BUF_SIZE];
char args[MAX_NUM_ARGS][ARG_BUF_SIZE];
boolean serial_input_error_flag = false;
boolean reset_flag = false;
