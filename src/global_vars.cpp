#include <global_vars.h>

WiFiClient espClient;
BearSSL::WiFiClientSecure TLSClient;
PubSubClient mqttClient; //uninitialised pubsub client instance. The client is initialised as TLS or espClient in setup()
Config config; // <- global configuration object

//ezTime
Timezone tz;

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

CRGB leds[NUM_LEDS]; // This is an array of leds, one item for each led in the clock

clockdisplay clockdisplays[CLOCK_DISPLAYS]; //Array of clock displays
schedule schedules[SCHEDULES];

int sliderBrightnessValue = 128; // Variable to store brightness slidervalue in webserver

// Config
String jsonConfigfile;
String jsonSchedulefile;
String jsonClkdisplaysfile;

int scheduleId = 0;
int currentMinute;

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



//Lightsensor
unsigned long lastExecutedMillis_brightness = 0;	  // Variable to save the last executed time
unsigned long lastExecutedMillis_lightsensorMQTT = 0; // Variable to save the last executed time
float lightReading;									  // Variable to store the ambient light value
float lux;											  //LUX

// Define the number of samples to keep track of. The higher the number, the
// more the readings will be smoothed, but the slower the output will respond to
// the input. Using a constant rather than a normal variable lets us use this
// value to determine the size of the readings array.

int readings[NUMREADINGS]; // the readings from the analog input
int readIndex = 0;		   // the index of the current reading
int total = 0;			   // the running total
int average = 0;		   // the average

//Keep track of the total runtime in mili's
unsigned long currentMillis;
