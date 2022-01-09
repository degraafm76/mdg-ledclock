#include <Arduino.h>
#include <helperfunctions.h>
#include <ezTime.h> // using modified library in /lib (changed host to timezoned.mdg-design.nl)
#include <ESP8266httpUpdate.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <FastLED.h>
#include <AsyncMqttClient.h>
#include <ESPAsyncTCP.h> //	using modified library in /lib to correct SSL error when compiling with build_flags = -DASYNC_TCP_SSL_ENABLED=1  see (https://github.com/mhightower83/ESPAsyncTCP#correct-ssl-_recv) when this pull request is complete we can continue to use the original library.
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <Ticker.h>
#include <mdg_ledr_js.h>
#include <mdg_ledr_css.h>
#include <index_html.h>
#include <fonts.h>

WiFiClient espClient;

#define SOFTWARE_VERSION "1.2.0"			// Software version
#define CLOCK_MODEL "MDG Ledclock model 1"	// Clock Model
#define NUM_LEDS 60							// How many leds are in the clock?
#define ROTATE_LEDS 30						// Rotate leds by this number. Led ring is connected upside down
#define MAX_BRIGHTNESS 255					// Max brightness
#define MIN_BRIGHTNESS 8					// Min brightness (at very low brightness levels interpolating doesn't work well and the led's will flicker and not display the correct color)
#define DATA_PIN 3							// Neopixel data pin
#define NEOPIXEL_VOLTAGE 5					// Neopixel voltage
#define NEOPIXEL_MILLIAMPS 1000				// Neopixel maximum current usage in milliamps, if you have a powersource higher then 1A you can change this value (at your own risk!) to have brighter leds.
#define LIGHTSENSORPIN A0					// Ambient light sensor pin
#define EXE_INTERVAL_AUTO_BRIGHTNESS 1000	// Interval (ms) to check light sensor value
#define EXE_INTERVAL_LIGHTSENSOR_MQTT 60000 // Interval (ms) to send light sensor value to MQTT topic
#define CLOCK_DISPLAYS 8					// Nr of user defined clock displays
#define SCHEDULES 12						// Nr of user defined schedules
#define AP_NAME "MDG-Ledclock1"				// Name of the AP when WIFI is not setup or connected
//#define MQTT_DEBUG						// MQTT debug enabled
#define COMPILE_DATE __DATE__ " " __TIME__  // Compile date/time

//mqtt async
AsyncMqttClient mqttClient;
Ticker mqttReconnectTimer;

const uint8_t MSG_BUFFER_SIZE = 20;
char m_msg_buffer[MSG_BUFFER_SIZE];

const uint8_t TOPIC_BUFFER_SIZE = 64;
char m_topic_buffer[TOPIC_BUFFER_SIZE];

WiFiEventHandler wifiConnectHandler;
WiFiEventHandler wifiDisconnectHandler;
Ticker wifiReconnectTimer;

long lastReconnectAttempt = 0;
unsigned long lastExecutedMillis_brightness = 0;	  // Variable to save the last executed time
unsigned long lastExecutedMillis_lightsensorMQTT = 0; // Variable to save the last executed time
unsigned long currentMillis;
boolean wifiConnected = false; // Variable to store wifi connected state
boolean MQTTConnected = false; // Variable to store MQTT connected state
unsigned char bssid[6];
byte channel;
int rssi = -999;
float lightReading; // Variable to store the ambient light value
float lux;			//LUX

// Define the number of samples to keep track of. The higher the number, the
// more the readings will be smoothed, but the slower the output will respond to
// the input. Using a constant rather than a normal variable lets us use this
// value to determine the size of the readings array.
const int numReadings = 20;

int readings[numReadings]; // the readings from the analog input
int readIndex = 0;		   // the index of the current reading
int total = 0;			   // the running total
int average = 0;		   // the average

CRGB leds[NUM_LEDS]; // This is an array of leds, one item for each led in the clock

int sliderBrightnessValue = 128; // Variable to store brightness slidervalue in webserver

// Variables used to store the mqtt state, the brightness and the color of the light
boolean hour_state = true;
uint8_t hour_brightness = 100;
uint8_t hour_rgb_red = 255;
uint8_t hour_rgb_green = 255;
uint8_t hour_rgb_blue = 255;
boolean minute_state = true;
uint8_t minute_brightness = 100;
uint8_t minute_rgb_red = 255;
uint8_t minute_rgb_green = 255;
uint8_t minute_rgb_blue = 255;
boolean second_state = true;
uint8_t second_brightness = 100;
uint8_t second_rgb_red = 255;
uint8_t second_rgb_green = 255;
uint8_t second_rgb_blue = 255;
boolean hourmarks_state = true;
uint8_t hourmarks_brightness = 100;
uint8_t hourmarks_rgb_red = 255;
uint8_t hourmarks_rgb_green = 255;
uint8_t hourmarks_rgb_blue = 255;
boolean background_state = true;
uint8_t background_brightness = 100;
uint8_t background_rgb_red = 255;
uint8_t background_rgb_green = 255;
uint8_t background_rgb_blue = 255;

// MQTT: topics

// Hour state
const PROGMEM char *MQTT_HOUR_STATE_TOPIC = "/hour/status";
const PROGMEM char *MQTT_HOUR_COMMAND_TOPIC = "/hour/switch";
// Hour brightness
const PROGMEM char *MQTT_HOUR_BRIGHTNESS_STATE_TOPIC = "/hour/brightness/status";
const PROGMEM char *MQTT_HOUR_BRIGHTNESS_COMMAND_TOPIC = "/hour/brightness/set";
// Hour colors (rgb)
const PROGMEM char *MQTT_HOUR_RGB_STATE_TOPIC = "/hour/rgb/status";
const PROGMEM char *MQTT_HOUR_RGB_COMMAND_TOPIC = "/hour/rgb/set";

// Minute state
const PROGMEM char *MQTT_MINUTE_STATE_TOPIC = "/minute/status";
const PROGMEM char *MQTT_MINUTE_COMMAND_TOPIC = "/minute/switch";
// Minute brightness
const PROGMEM char *MQTT_MINUTE_BRIGHTNESS_STATE_TOPIC = "/minute/brightness/status";
const PROGMEM char *MQTT_MINUTE_BRIGHTNESS_COMMAND_TOPIC = "/minute/brightness/set";
// Minute colors (rgb)
const PROGMEM char *MQTT_MINUTE_RGB_STATE_TOPIC = "/minute/rgb/status";
const PROGMEM char *MQTT_MINUTE_RGB_COMMAND_TOPIC = "/minute/rgb/set";

// Second state
const PROGMEM char *MQTT_SECOND_STATE_TOPIC = "/second/status";
const PROGMEM char *MQTT_SECOND_COMMAND_TOPIC = "/second/switch";
// Second brightness
const PROGMEM char *MQTT_SECOND_BRIGHTNESS_STATE_TOPIC = "/second/brightness/status";
const PROGMEM char *MQTT_SECOND_BRIGHTNESS_COMMAND_TOPIC = "/second/brightness/set";
// Second colors (rgb)
const PROGMEM char *MQTT_SECOND_RGB_STATE_TOPIC = "/second/rgb/status";
const PROGMEM char *MQTT_SECOND_RGB_COMMAND_TOPIC = "/second/rgb/set";

// Background state
const PROGMEM char *MQTT_BACKGROUND_STATE_TOPIC = "/background/status";
const PROGMEM char *MQTT_BACKGROUND_COMMAND_TOPIC = "/background/switch";
// Background brightness
const PROGMEM char *MQTT_BACKGROUND_BRIGHTNESS_STATE_TOPIC = "/background/brightness/status";
const PROGMEM char *MQTT_BACKGROUND_BRIGHTNESS_COMMAND_TOPIC = "/background/brightness/set";
// Background colors (rgb)
const PROGMEM char *MQTT_BACKGROUND_RGB_STATE_TOPIC = "/background/rgb/status";
const PROGMEM char *MQTT_BACKGROUND_RGB_COMMAND_TOPIC = "/background/rgb/set";

// Hourmarks state
const PROGMEM char *MQTT_HOURMARKS_STATE_TOPIC = "/hourmarks/status";
const PROGMEM char *MQTT_HOURMARKS_COMMAND_TOPIC = "/hourmarks/switch";
// Hourmarks brightness
const PROGMEM char *MQTT_HOURMARKS_BRIGHTNESS_STATE_TOPIC = "/hourmarks/brightness/status";
const PROGMEM char *MQTT_HOURMARKS_BRIGHTNESS_COMMAND_TOPIC = "/hourmarks/brightness/set";
// Hourmarks colors (rgb)
const PROGMEM char *MQTT_HOURMARKS_RGB_STATE_TOPIC = "/hourmarks/rgb/status";
const PROGMEM char *MQTT_HOURMARKS_RGB_COMMAND_TOPIC = "/hourmarks/rgb/set";
//Light sensor
const PROGMEM char *MQTT_LIGHT_SENSOR_STATE_TOPIC = "/sensor/lux/status";

// payloads by default (on/off)
const PROGMEM char *LIGHT_ON = "ON";
const PROGMEM char *LIGHT_OFF = "OFF";

// Config
String jsonConfigfile;
String jsonSchedulefile;
String jsonClkdisplaysfile;

struct Config
{
	byte activeclockdisplay;
	char tz[64];
	char ssid[33];
	char wifipassword[65];
	char hostname[17];
	char mqttserver[64];
	int mqttport;
	char mqttuser[32];
	char mqttpassword[32];
	byte mqtttls;
};

typedef struct Clockdisplay
{
	int hourColor;
	int minuteColor;
	int secondColor;
	int backgroundColor;
	int hourMarkColor;
	byte showms;
	byte showseconds;
	byte autobrightness;
	int brightness;
	byte backgroud_effect;
} clockdisplay;

typedef struct Schedule
{
	int hour;
	int minute;
	byte activeclockdisplay;
	byte active;

} schedule;

schedule schedules[SCHEDULES];

int scheduleId = 0;
int currentMinute;

clockdisplay clockdisplays[CLOCK_DISPLAYS]; //Array of clock displays

Config config; // <- global configuration object

tmElements_t tm;

Timezone tz;

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

//PubSubClient mqttclient(espClient);

// function called to adapt the brightness and the color of the led
void setHourColor(uint8_t p_red, uint8_t p_green, uint8_t p_blue)
{
	// convert the brightness in % (0-100%) into a digital value (0-255)
	uint8_t brightness = map(hour_brightness, 0, 100, 0, 255);

	p_red = map(p_red, 0, 255, 0, brightness);
	p_green = map(p_green, 0, 255, 0, brightness);
	p_blue = map(p_blue, 0, 255, 0, brightness);

	long RGB = ((long)p_red << 16L) | ((long)p_green << 8L) | (long)p_blue;
	clockdisplays[config.activeclockdisplay].hourColor = RGB;
}
void setMinuteColor(uint8_t p_red, uint8_t p_green, uint8_t p_blue)
{
	// convert the brightness in % (0-100%) into a digital value (0-255)
	uint8_t brightness = map(minute_brightness, 0, 100, 0, 255);

	p_red = map(p_red, 0, 255, 0, brightness);
	p_green = map(p_green, 0, 255, 0, brightness);
	p_blue = map(p_blue, 0, 255, 0, brightness);

	long RGB = ((long)p_red << 16L) | ((long)p_green << 8L) | (long)p_blue;
	clockdisplays[config.activeclockdisplay].minuteColor = RGB;
}
void setSecondColor(uint8_t p_red, uint8_t p_green, uint8_t p_blue)
{
	// convert the brightness in % (0-100%) into a digital value (0-255)
	uint8_t brightness = map(second_brightness, 0, 100, 0, 255);

	p_red = map(p_red, 0, 255, 0, brightness);
	p_green = map(p_green, 0, 255, 0, brightness);
	p_blue = map(p_blue, 0, 255, 0, brightness);

	long RGB = ((long)p_red << 16L) | ((long)p_green << 8L) | (long)p_blue;
	clockdisplays[config.activeclockdisplay].secondColor = RGB;
}
void setBackgroundColor(uint8_t p_red, uint8_t p_green, uint8_t p_blue)
{
	// convert the brightness in % (0-100%) into a digital value (0-255)
	uint8_t brightness = map(background_brightness, 0, 100, 0, 255);

	p_red = map(p_red, 0, 255, 0, brightness);
	p_green = map(p_green, 0, 255, 0, brightness);
	p_blue = map(p_blue, 0, 255, 0, brightness);

	long RGB = ((long)p_red << 16L) | ((long)p_green << 8L) | (long)p_blue;
	clockdisplays[config.activeclockdisplay].backgroundColor = RGB;
}
void setHourmarksColor(uint8_t p_red, uint8_t p_green, uint8_t p_blue)
{
	// convert the brightness in % (0-100%) into a digital value (0-255)
	uint8_t brightness = map(hourmarks_brightness, 0, 100, 0, 255);

	p_red = map(p_red, 0, 255, 0, brightness);
	p_green = map(p_green, 0, 255, 0, brightness);
	p_blue = map(p_blue, 0, 255, 0, brightness);

	long RGB = ((long)p_red << 16L) | ((long)p_green << 8L) | (long)p_blue;
	clockdisplays[config.activeclockdisplay].hourMarkColor = RGB;
}

void publishRGBminuteColor(uint8_t rgb_red, uint8_t rgb_green, uint8_t rgb_blue)
{

	snprintf(m_msg_buffer, MSG_BUFFER_SIZE, "%d,%d,%d", rgb_red, rgb_green, rgb_blue);
	snprintf(m_topic_buffer, sizeof(m_topic_buffer), "%s%s", config.hostname, MQTT_MINUTE_RGB_STATE_TOPIC);

	mqttClient.publish(m_topic_buffer, 0, true, m_msg_buffer);
}
void publishRGBminuteBrightness()
{
	snprintf(m_msg_buffer, MSG_BUFFER_SIZE, "%d", minute_brightness);
	snprintf(m_topic_buffer, sizeof(m_topic_buffer), "%s%s", config.hostname, MQTT_MINUTE_BRIGHTNESS_STATE_TOPIC);
	mqttClient.publish(m_topic_buffer, 0, true, m_msg_buffer);
}
void publishRGBminuteState()

{
	if (minute_state)
	{
		snprintf(m_topic_buffer, sizeof(m_topic_buffer), "%s%s", config.hostname, MQTT_MINUTE_STATE_TOPIC);
		mqttClient.publish(m_topic_buffer, 0, true, LIGHT_ON);
	}
	else
	{
		snprintf(m_topic_buffer, sizeof(m_topic_buffer), "%s%s", config.hostname, MQTT_MINUTE_STATE_TOPIC);
		mqttClient.publish(m_topic_buffer, 0, true, LIGHT_OFF);
	}
}

void publishRGBsecondBrightness()
{
	snprintf(m_msg_buffer, MSG_BUFFER_SIZE, "%d", second_brightness);
	snprintf(m_topic_buffer, sizeof(m_topic_buffer), "%s%s", config.hostname, MQTT_SECOND_BRIGHTNESS_STATE_TOPIC);
	mqttClient.publish(m_topic_buffer, 0, true, m_msg_buffer);
}
void publishRGBsecondColor(uint8_t rgb_red, uint8_t rgb_green, uint8_t rgb_blue)
{

	//Serial.print ("send rgb");

	snprintf(m_msg_buffer, MSG_BUFFER_SIZE, "%d,%d,%d", rgb_red, rgb_green, rgb_blue);
	snprintf(m_topic_buffer, sizeof(m_topic_buffer), "%s%s", config.hostname, MQTT_SECOND_RGB_STATE_TOPIC);
	mqttClient.publish(m_topic_buffer, 0, true, m_msg_buffer);
}
void publishRGBsecondState()

{

	if (second_state)
	{

		snprintf(m_topic_buffer, sizeof(m_topic_buffer), "%s%s", config.hostname, MQTT_SECOND_STATE_TOPIC);
		mqttClient.publish(m_topic_buffer, 0, true, LIGHT_ON);
	}
	else
	{
		snprintf(m_topic_buffer, sizeof(m_topic_buffer), "%s%s", config.hostname, MQTT_SECOND_STATE_TOPIC);
		mqttClient.publish(m_topic_buffer, 0, true, LIGHT_OFF);
	}
}

void publishRGBhourBrightness()
{
	snprintf(m_msg_buffer, MSG_BUFFER_SIZE, "%d", hour_brightness);
	snprintf(m_topic_buffer, sizeof(m_topic_buffer), "%s%s", config.hostname, MQTT_HOUR_BRIGHTNESS_STATE_TOPIC);
	mqttClient.publish(m_topic_buffer, 0, true, m_msg_buffer);
}
void publishRGBhourColor(uint8_t rgb_red, uint8_t rgb_green, uint8_t rgb_blue)
{

	snprintf(m_msg_buffer, MSG_BUFFER_SIZE, "%d,%d,%d", rgb_red, rgb_green, rgb_blue);
	snprintf(m_topic_buffer, sizeof(m_topic_buffer), "%s%s", config.hostname, MQTT_HOUR_RGB_STATE_TOPIC);

	mqttClient.publish(m_topic_buffer, 0, true, m_msg_buffer);
}
void publishRGBhourState()

{
	if (hour_state)
	{
		snprintf(m_topic_buffer, sizeof(m_topic_buffer), "%s%s", config.hostname, MQTT_HOUR_STATE_TOPIC);
		mqttClient.publish(m_topic_buffer, 0, true, LIGHT_ON);
	}
	else
	{
		snprintf(m_topic_buffer, sizeof(m_topic_buffer), "%s%s", config.hostname, MQTT_HOUR_STATE_TOPIC);
		mqttClient.publish(m_topic_buffer, 0, true, LIGHT_OFF);
	}
}

void publishRGBbackgroundBrightness()
{
	snprintf(m_msg_buffer, MSG_BUFFER_SIZE, "%d", background_brightness);
	snprintf(m_topic_buffer, sizeof(m_topic_buffer), "%s%s", config.hostname, MQTT_BACKGROUND_BRIGHTNESS_STATE_TOPIC);
	mqttClient.publish(m_topic_buffer, 0, true, m_msg_buffer);
}
void publishRGBbackgroundColor(uint8_t rgb_red, uint8_t rgb_green, uint8_t rgb_blue)
{

	snprintf(m_msg_buffer, MSG_BUFFER_SIZE, "%d,%d,%d", rgb_red, rgb_green, rgb_blue);
	snprintf(m_topic_buffer, sizeof(m_topic_buffer), "%s%s", config.hostname, MQTT_BACKGROUND_RGB_STATE_TOPIC);
	mqttClient.publish(m_topic_buffer, 0, true, m_msg_buffer);
}
void publishRGBbackgroundState()
{
	if (background_state)
	{
		snprintf(m_topic_buffer, sizeof(m_topic_buffer), "%s%s", config.hostname, MQTT_BACKGROUND_STATE_TOPIC);
		mqttClient.publish(m_topic_buffer, 0, true, LIGHT_ON);
	}
	else
	{
		snprintf(m_topic_buffer, sizeof(m_topic_buffer), "%s%s", config.hostname, MQTT_BACKGROUND_STATE_TOPIC);
		mqttClient.publish(m_topic_buffer, 0, true, LIGHT_OFF);
	}
}

void publishRGBhourmarksBrightness()
{
	snprintf(m_msg_buffer, MSG_BUFFER_SIZE, "%d", hourmarks_brightness);
	snprintf(m_topic_buffer, sizeof(m_topic_buffer), "%s%s", config.hostname, MQTT_HOURMARKS_BRIGHTNESS_STATE_TOPIC);
	mqttClient.publish(m_topic_buffer, 0, true, m_msg_buffer);
}
void publishRGBhourmarksColor(uint8_t rgb_red, uint8_t rgb_green, uint8_t rgb_blue)
{

	snprintf(m_msg_buffer, MSG_BUFFER_SIZE, "%d,%d,%d", rgb_red, rgb_green, rgb_blue);
	snprintf(m_topic_buffer, sizeof(m_topic_buffer), "%s%s", config.hostname, MQTT_HOURMARKS_RGB_STATE_TOPIC);
	mqttClient.publish(m_topic_buffer, 0, true, m_msg_buffer);
}
void publishRGBhourmarksState()
{
	if (hourmarks_state)
	{
		snprintf(m_topic_buffer, sizeof(m_topic_buffer), "%s%s", config.hostname, MQTT_HOURMARKS_STATE_TOPIC);
		mqttClient.publish(m_topic_buffer, 0, true, LIGHT_ON);
	}
	else
	{
		snprintf(m_topic_buffer, sizeof(m_topic_buffer), "%s%s", config.hostname, MQTT_HOURMARKS_STATE_TOPIC);
		mqttClient.publish(m_topic_buffer, 0, true, LIGHT_OFF);
	}
}

void connectToMqtt()
{
	//Serial.println("Connecting to MQTT...");
	mqttClient.connect();
}

void onWifiConnect(const WiFiEventStationModeGotIP &event)
{
#ifdef MQTT_DEBUG
	Serial.println("Connected to Wi-Fi.");
#endif
	if (strlen(config.mqttserver))
	{
		connectToMqtt();
	}
}

void onWifiDisconnect(const WiFiEventStationModeDisconnected &event)
{
	//Serial.println("Disconnected from Wi-Fi.");
	mqttReconnectTimer.detach(); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
								 //wifiReconnectTimer.once(2, connectToWifi);
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason)
{
	MQTTConnected = false;
#ifdef MQTT_DEBUG
	Serial.println("Disconnected from MQTT.");
#endif

	if (WiFi.isConnected())
	{
		mqttReconnectTimer.once(2, connectToMqtt);
	}
}

void onMqttPublish(uint16_t packetId)
{
#ifdef MQTT_DEBUG
	Serial.println("Publish acknowledged.");
	Serial.print("  packetId: ");
	Serial.println(packetId);
#endif
}

void onMqttConnect(bool sessionPresent)
{
	MQTTConnected = true;
#ifdef MQTT_DEBUG
	Serial.println("Connected to MQTT.");
	Serial.print("Session present: ");
	Serial.println(sessionPresent);
#endif
	snprintf(m_topic_buffer, sizeof(m_topic_buffer), "%s%s", config.hostname, "/#");
	uint16_t packetIdSub = mqttClient.subscribe(m_topic_buffer, 0);
#ifdef MQTT_DEBUG
	Serial.print("Subscribing at QoS 0, packetId: ");
	Serial.println(packetIdSub);
#endif
	snprintf(m_topic_buffer, sizeof(m_topic_buffer), "%s%s", config.hostname, "/status/");
	mqttClient.publish(m_topic_buffer, 0, true, "Online");
	/* Serial.println("Publishing at QoS 0");
  uint16_t packetIdPub1 = mqttClient.publish("mdg/test/lol", 1, true, "test 2");
  Serial.print("Publishing at QoS 1, packetId: ");
  Serial.println(packetIdPub1);
  uint16_t packetIdPub2 = mqttClient.publish("mdg/test/lol", 2, true, "test 3");
  Serial.print("Publishing at QoS 2, packetId: ");
  Serial.println(packetIdPub2); */
}

void onMqttSubscribe(uint16_t packetId, uint8_t qos)
{
#ifdef MQTT_DEBUG
	Serial.println("Subscribe acknowledged.");
	Serial.print("  packetId: ");
	Serial.println(packetId);
	Serial.print("  qos: ");
	Serial.println(qos);
#endif
}

void onMqttMessage(char *p_topic, char *p_payload, AsyncMqttClientMessageProperties properties, size_t p_length, size_t index, size_t total)
{
#ifdef MQTT_DEBUG
	Serial.println("Publish received.");
	Serial.print("  topic: ");
	Serial.println(p_topic);
	Serial.print("  qos: ");
	Serial.println(properties.qos);
	Serial.print("  dup: ");
	Serial.println(properties.dup);
	Serial.print("  retain: ");
	Serial.println(properties.retain);
	Serial.print("  len: ");
	Serial.println(p_length);
	Serial.print("  index: ");
	Serial.println(index);
	Serial.print("  total: ");
	Serial.println(total);
#endif

	String payload;
	for (uint8_t i = 0; i < p_length; i++)
	{
		payload.concat((char)p_payload[i]);
	}

	if ((String(config.hostname) + String(MQTT_HOUR_COMMAND_TOPIC)).equals(p_topic))
	{
		// test if the payload is equal to "ON" or "OFF"
		if (payload.equals(String(LIGHT_ON)))
		{
			if (hour_state != true)
			{
				hour_state = true;
			}
			publishRGBhourState();
		}
		else if (payload.equals(String(LIGHT_OFF)))
		{
			if (hour_state != false)
			{
				hour_state = false;
			}
			publishRGBhourState();
		}
	}
	else if ((String(config.hostname) + String(MQTT_HOUR_BRIGHTNESS_COMMAND_TOPIC)).equals(p_topic))
	{
		uint8_t brightness = payload.toInt();
		if (brightness < 0 || brightness > 100)
		{
			// do nothing...
			return;
		}
		else
		{
			hour_brightness = brightness;
			setHourColor(hour_rgb_red, hour_rgb_green, hour_rgb_blue);
			publishRGBhourBrightness();
		}
	}
	else if ((String(config.hostname) + String(MQTT_HOUR_RGB_COMMAND_TOPIC)).equals(p_topic))
	{
		// get the position of the first and second commas
		uint8_t firstIndex = payload.indexOf(',');
		uint8_t lastIndex = payload.lastIndexOf(',');

		uint8_t rgb_red = payload.substring(0, firstIndex).toInt();
		if (rgb_red < 0 || rgb_red > 255)
		{
			return;
		}
		else
		{
			hour_rgb_red = rgb_red;
		}

		uint8_t rgb_green = payload.substring(firstIndex + 1, lastIndex).toInt();
		if (rgb_green < 0 || rgb_green > 255)
		{
			return;
		}
		else
		{
			hour_rgb_green = rgb_green;
		}

		uint8_t rgb_blue = payload.substring(lastIndex + 1).toInt();
		if (rgb_blue < 0 || rgb_blue > 255)
		{
			return;
		}
		else
		{
			hour_rgb_blue = rgb_blue;
		}
		setHourColor(rgb_red, rgb_green, rgb_blue);
		publishRGBhourColor(rgb_red, rgb_green, rgb_blue);
	}

	else if ((String(config.hostname) + String(MQTT_MINUTE_COMMAND_TOPIC)).equals(p_topic))
	{

		// test if the payload is equal to "ON" or "OFF"
		if (payload.equals(String(LIGHT_ON)))
		{
			if (minute_state != true)
			{
				minute_state = true;
			}
			publishRGBminuteState();
		}
		else if (payload.equals(String(LIGHT_OFF)))
		{
			if (minute_state != false)
			{
				minute_state = false;
			}
			publishRGBminuteState();
		}
	}
	else if ((String(config.hostname) + String(MQTT_MINUTE_BRIGHTNESS_COMMAND_TOPIC)).equals(p_topic))
	{
		uint8_t brightness = payload.toInt();
		if (brightness < 0 || brightness > 100)
		{
			// do nothing...
			return;
		}
		else
		{
			minute_brightness = brightness;
			setMinuteColor(minute_rgb_red, minute_rgb_green, minute_rgb_blue);
			publishRGBminuteBrightness();
		}
	}
	else if ((String(config.hostname) + String(MQTT_MINUTE_RGB_COMMAND_TOPIC)).equals(p_topic))
	{
		// get the position of the first and second commas
		uint8_t firstIndex = payload.indexOf(',');
		uint8_t lastIndex = payload.lastIndexOf(',');

		uint8_t rgb_red = payload.substring(0, firstIndex).toInt();
		if (rgb_red < 0 || rgb_red > 255)
		{
			return;
		}
		else
		{
			minute_rgb_red = rgb_red;
		}

		uint8_t rgb_green = payload.substring(firstIndex + 1, lastIndex).toInt();
		if (rgb_green < 0 || rgb_green > 255)
		{
			return;
		}
		else
		{
			minute_rgb_green = rgb_green;
		}

		uint8_t rgb_blue = payload.substring(lastIndex + 1).toInt();
		if (rgb_blue < 0 || rgb_blue > 255)
		{
			return;
		}
		else
		{
			minute_rgb_blue = rgb_blue;
		}
		setMinuteColor(rgb_red, rgb_green, rgb_blue);
		publishRGBminuteColor(rgb_red, rgb_green, rgb_blue);
	}

	else if ((String(config.hostname) + String(MQTT_SECOND_RGB_COMMAND_TOPIC)).equals(p_topic))
	{
		// get the position of the first and second commas
		uint8_t firstIndex = payload.indexOf(',');
		uint8_t lastIndex = payload.lastIndexOf(',');

		uint8_t rgb_red = payload.substring(0, firstIndex).toInt();
		if (rgb_red < 0 || rgb_red > 255)
		{
			return;
		}
		else
		{
			second_rgb_red = rgb_red;
		}

		uint8_t rgb_green = payload.substring(firstIndex + 1, lastIndex).toInt();
		if (rgb_green < 0 || rgb_green > 255)
		{
			return;
		}
		else
		{
			second_rgb_green = rgb_green;
		}

		uint8_t rgb_blue = payload.substring(lastIndex + 1).toInt();
		if (rgb_blue < 0 || rgb_blue > 255)
		{
			return;
		}
		else
		{
			second_rgb_blue = rgb_blue;
		}
		setSecondColor(rgb_red, rgb_green, rgb_blue);
		publishRGBsecondColor(rgb_red, rgb_green, rgb_blue);
	}
	else if ((String(config.hostname) + String(MQTT_BACKGROUND_RGB_COMMAND_TOPIC)).equals(p_topic))
	{
		// get the position of the first and second commas
		uint8_t firstIndex = payload.indexOf(',');
		uint8_t lastIndex = payload.lastIndexOf(',');

		uint8_t rgb_red = payload.substring(0, firstIndex).toInt();
		if (rgb_red < 0 || rgb_red > 255)
		{
			return;
		}
		else
		{
			background_rgb_red = rgb_red;
		}

		uint8_t rgb_green = payload.substring(firstIndex + 1, lastIndex).toInt();
		if (rgb_green < 0 || rgb_green > 255)
		{
			return;
		}
		else
		{
			background_rgb_green = rgb_green;
		}

		uint8_t rgb_blue = payload.substring(lastIndex + 1).toInt();
		if (rgb_blue < 0 || rgb_blue > 255)
		{
			return;
		}
		else
		{
			background_rgb_blue = rgb_blue;
		}
		setBackgroundColor(rgb_red, rgb_green, rgb_blue);
		publishRGBbackgroundColor(rgb_red, rgb_green, rgb_blue);
	}
	else if ((String(config.hostname) + String(MQTT_HOURMARKS_RGB_COMMAND_TOPIC)).equals(p_topic))
	{
		// get the position of the first and second commas
		uint8_t firstIndex = payload.indexOf(',');
		uint8_t lastIndex = payload.lastIndexOf(',');

		uint8_t rgb_red = payload.substring(0, firstIndex).toInt();
		if (rgb_red < 0 || rgb_red > 255)
		{
			return;
		}
		else
		{
			hourmarks_rgb_red = rgb_red;
		}

		uint8_t rgb_green = payload.substring(firstIndex + 1, lastIndex).toInt();
		if (rgb_green < 0 || rgb_green > 255)
		{
			return;
		}
		else
		{
			hourmarks_rgb_green = rgb_green;
		}

		uint8_t rgb_blue = payload.substring(lastIndex + 1).toInt();
		if (rgb_blue < 0 || rgb_blue > 255)
		{
			return;
		}
		else
		{
			hourmarks_rgb_blue = rgb_blue;
		}
		setHourmarksColor(rgb_red, rgb_green, rgb_blue);
		publishRGBhourmarksColor(rgb_red, rgb_green, rgb_blue);
	}

	else if ((String(config.hostname) + String(MQTT_SECOND_BRIGHTNESS_COMMAND_TOPIC)).equals(p_topic))
	{
		uint8_t brightness = payload.toInt();
		if (brightness < 0 || brightness > 100)
		{
			// do nothing...
			return;
		}
		else
		{
			second_brightness = brightness;
			setSecondColor(minute_rgb_red, minute_rgb_green, minute_rgb_blue);
			publishRGBsecondBrightness();
		}
	}
	else if ((String(config.hostname) + String(MQTT_BACKGROUND_BRIGHTNESS_COMMAND_TOPIC)).equals(p_topic))
	{
		uint8_t brightness = payload.toInt();
		if (brightness < 0 || brightness > 100)
		{
			// do nothing...
			return;
		}
		else
		{
			background_brightness = brightness;
			setBackgroundColor(background_rgb_red, background_rgb_green, background_rgb_blue);
			publishRGBbackgroundBrightness();
		}
	}
	else if ((String(config.hostname) + String(MQTT_HOURMARKS_BRIGHTNESS_COMMAND_TOPIC)).equals(p_topic))
	{
		uint8_t brightness = payload.toInt();
		if (brightness < 0 || brightness > 100)
		{
			// do nothing...
			return;
		}
		else
		{
			hourmarks_brightness = brightness;
			setHourmarksColor(hourmarks_rgb_red, hourmarks_rgb_green, hourmarks_rgb_blue);
			publishRGBhourmarksBrightness();
		}
	}

	else if ((String(config.hostname) + String(MQTT_SECOND_COMMAND_TOPIC)).equals(p_topic))
	{
		// test if the payload is equal to "ON" or "OFF"
		if (payload.equals(String(LIGHT_ON)))
		{

			if (second_state != true)
			{
				second_state = true;
			}
			publishRGBsecondState();
			clockdisplays[config.activeclockdisplay].showseconds = 1;
		}
		else if (payload.equals(String(LIGHT_OFF)))
		{

			if (second_state != false)
			{
				second_state = false;
			}
			publishRGBsecondState();
			clockdisplays[config.activeclockdisplay].showseconds = 0;
		}
	}

	else if ((String(config.hostname) + String(MQTT_BACKGROUND_COMMAND_TOPIC)).equals(p_topic))
	{
		// test if the payload is equal to "ON" or "OFF"
		if (payload.equals(String(LIGHT_ON)))
		{
			if (background_state != true)
			{
				background_state = true;
			}
			setBackgroundColor(background_rgb_red, background_rgb_green, background_rgb_blue);
			publishRGBbackgroundState();
		}
		else if (payload.equals(String(LIGHT_OFF)))
		{
			if (background_state != false)
			{
				background_state = false;
			}
			setBackgroundColor(0, 0, 0);
			publishRGBbackgroundState();
		}
	}
	else if ((String(config.hostname) + String(MQTT_HOURMARKS_COMMAND_TOPIC)).equals(p_topic))
	{
		// test if the payload is equal to "ON" or "OFF"
		if (payload.equals(String(LIGHT_ON)))
		{
			if (hourmarks_state != true)
			{
				hourmarks_state = true;
			}
			publishRGBhourmarksState();
		}
		else if (payload.equals(String(LIGHT_OFF)))
		{
			if (hourmarks_state != false)
			{
				hourmarks_state = false;
			}
			publishRGBhourmarksState();
		}
	}
}

// Replaces placeholder with section in your web page
String processor(const String &var)
{

	if (var == "sliderBrightnessValue")
	{
		return String(sliderBrightnessValue);
	}
	if (var == "LDRValue")
	{
		return String(lightReading);
	}
	if (var == "backGroundColor")
	{
		return String(inttoHex(clockdisplays[config.activeclockdisplay].backgroundColor, 6));
	}
	if (var == "hourMarkColor")
	{
		return String(inttoHex(clockdisplays[config.activeclockdisplay].hourMarkColor, 6));
	}
	if (var == "hourColor")
	{
		return String(inttoHex(clockdisplays[config.activeclockdisplay].hourColor, 6));
	}
	if (var == "minuteColor")
	{
		return String(inttoHex(clockdisplays[config.activeclockdisplay].minuteColor, 6));
	}
	if (var == "secondColor")
	{

		return String(inttoHex(clockdisplays[config.activeclockdisplay].secondColor, 6));
	}
	if (var == "showms")
	{
		if (clockdisplays[config.activeclockdisplay].showms != 0)
		{
			return String("checked");
		}
	}
	if (var == "showseconds")
	{
		if (clockdisplays[config.activeclockdisplay].showseconds != 0)
		{
			return String("checked");
		}
	}
	if (var == "autobrightness")
	{
		if (clockdisplays[config.activeclockdisplay].autobrightness != 0)
		{
			return String("checked");
		}
	}
	if (var == "activeclockdisplay")
	{
		return String(config.activeclockdisplay);
	}

	if (var == "compile_date_time")
	{
		return String(COMPILE_DATE);
	}
	if (var == "software_version")
	{
		return String(SOFTWARE_VERSION);
	}
	if (var == "wifi_rssi")
	{
		return String(rssi);
	}
	if (var == "wifi_channel")
	{
		return String(channel);
	}
	if (var == "wifi_mac")
	{
		return String(WiFi.macAddress());
	}
	if (var == "wifi_ip")
	{
		return String(WiFi.localIP().toString());
	}
	if (var == "lux")
	{
		return String(lux);
	}
	if (var == "clock_model")
	{
		return String(CLOCK_MODEL);
	}

	if (var == "mqtt_connected")
	{
		if (MQTTConnected)
		{
			return String("Yes");
		}
		else
		{
			return String("No");
		}
	}

	return String();
}

void saveSchedules(const char *filename)
{
	// Delete existing file, otherwise the configuration is appended to the file
	LittleFS.remove(filename);

	File schedulefile = LittleFS.open(filename, "w");

	if (!schedulefile)
	{
		//Serial.println(F("Failed to create file"));
		return;
	}

	// Allocate a temporary JsonDocument
	// Don't forget to change the capacity to match your requirements.
	// Use arduinojson.org/assistant to compute the capacity.

	DynamicJsonDocument data(128 * SCHEDULES);
	// Set the values in the document
	for (int i = 0; i <= SCHEDULES - 1; i++)
	{
		JsonObject obj = data.createNestedObject();
		obj["i"] = i;
		obj["a"] = schedules[i].active;
		obj["acd"] = schedules[i].activeclockdisplay;
		obj["h"] = schedules[i].hour;
		obj["m"] = schedules[i].minute;
		/* JsonArray levels = obj.createNestedArray("weekdays");
		levels.add(1);
		levels.add(2); */
	}

	// Serialize JSON to file
	if (serializeJson(data, schedulefile) == 0)
	{
		//Serial.println(F("Failed to write to file"));
	}

	// Close the file
	schedulefile.close();
}

void saveConfiguration(const char *filename)
{
	// Delete existing file, otherwise the configuration is appended to the file
	LittleFS.remove(filename);

	// Open file for writing
	File configfile = LittleFS.open(filename, "w");

	if (!configfile)
	{
		//Serial.println(F("Failed to create file"));
		return;
	}

	// Allocate a temporary JsonDocument
	// Don't forget to change the capacity to match your requirements.
	// Use arduinojson.org/assistant to compute the capacity.

	DynamicJsonDocument doc(2048);

	doc["acd"] = config.activeclockdisplay;
	doc["tz"] = config.tz;
	doc["ssid"] = config.ssid;
	doc["wp"] = config.wifipassword;
	doc["hn"] = config.hostname;
	doc["ms"] = config.mqttserver;
	doc["mp"] = config.mqttport;
	doc["mu"] = config.mqttuser;
	doc["mpw"] = config.mqttpassword;
	doc["mt"] = config.mqtttls;

	//Serial.print(config.tz);

	// Serialize JSON to file
	if (serializeJson(doc, configfile) == 0)
	{
		//Serial.println(F("Failed to write to file"));
	}

	// Close the file
	configfile.close();
}

void saveClockDisplays(const char *filename)
{
	// Delete existing file, otherwise the configuration is appended to the file
	LittleFS.remove(filename);

	// Open file for writing
	File configfile = LittleFS.open(filename, "w");

	if (!configfile)
	{
		//Serial.println(F("Failed to create file"));
		return;
	}

	// Allocate a temporary JsonDocument
	// Don't forget to change the capacity to match your requirements.
	// Use arduinojson.org/assistant to compute the capacity.

	DynamicJsonDocument data(256 * CLOCK_DISPLAYS);

	// Set the values in the document
	for (int i = 0; i <= CLOCK_DISPLAYS - 1; i++)

	{
		JsonObject obj = data.createNestedObject();
		obj["i"] = i;
		obj["bgc"] = clockdisplays[i].backgroundColor;
		obj["hmc"] = clockdisplays[i].hourMarkColor;
		obj["hc"] = clockdisplays[i].hourColor;
		obj["mc"] = clockdisplays[i].minuteColor;
		obj["sc"] = clockdisplays[i].secondColor;
		obj["ms"] = clockdisplays[i].showms;
		obj["s"] = clockdisplays[i].showseconds;
		obj["ab"] = clockdisplays[i].autobrightness;
		obj["bn"] = clockdisplays[i].brightness;
		obj["be"] = clockdisplays[i].backgroud_effect;
	}

	// Serialize JSON to file
	if (serializeJson(data, configfile) == 0)
	{
		//Serial.println(F("Failed to write to file"));
	}

	// Close the file
	configfile.close();
}

//Rotate clock display
int rotate(int nr)
{
	if ((nr + ROTATE_LEDS) >= 60)
	{
		return (nr + ROTATE_LEDS) - NUM_LEDS;
	}
	else
	{
		return (nr + ROTATE_LEDS);
	}
}

void scanWifi(String ssid)
{

	byte numSsid = WiFi.scanNetworks();
	rssi = -999;
	for (int thisNet = 0; thisNet < numSsid; thisNet++)
	{
		//Serial.println(WiFi.SSID(thisNet));
		if (WiFi.SSID(thisNet) == ssid)
		{
			//Serial.println("found SSID: " + WiFi.SSID(thisNet));
			//Serial.println("rssi: " + String(WiFi.RSSI(thisNet)));
			//Serial.println("channel: " + String(WiFi.channel(thisNet)));
			if (WiFi.RSSI(thisNet) > rssi)
			{
				memcpy(bssid, WiFi.BSSID(thisNet), 6);
				channel = WiFi.channel(thisNet);
				rssi = WiFi.RSSI(thisNet);
			}
		}
	}
}

boolean checkConnection()
{
	int count = 0;
	Serial.print("Waiting for Wi-Fi connection");
	while (count < 30)
	{
		leds[rotate(count)] = CRGB::White;
		FastLED.show();

		if (WiFi.status() == WL_CONNECTED)
		{
			Serial.println();
			Serial.println("Connected!");
			leds[rotate(count)] = CRGB::Green;
			FastLED.show();
			return (true);
		}
		delay(500);
		Serial.print(".");
		count++;
	}
	Serial.println("Timed out.");
	leds[rotate(count)] = CRGB::Red;
	FastLED.show();
	delay(5000);
	return false;
}

uint8_t gHue = 0; // rotating "base color" used by many of the patterns

void addGlitter(fract8 chanceOfGlitter)
{
	if (random8() < chanceOfGlitter)
	{
		leds[random16(NUM_LEDS)] += CRGB::White;
	}
}
void confetti()
{
	// random colored speckles that blink in and fade smoothly
	fadeToBlackBy(leds, NUM_LEDS, 10);
	int pos = random16(NUM_LEDS);
	leds[pos] += CHSV(gHue + random8(64), 200, 255);
}

void juggle()
{
	// eight colored dots, weaving in and out of sync with each other
	fadeToBlackBy(leds, NUM_LEDS, 20);
	byte dothue = 0;
	for (int i = 0; i < 8; i++)
	{
		leds[beatsin16(i + 7, 0, NUM_LEDS - 1)] |= CHSV(dothue, 200, 255);
		dothue += 32;
	}
}

void bpm()
{
	// colored stripes pulsing at a defined Beats-Per-Minute (BPM)
	uint8_t BeatsPerMinute = 62;
	CRGBPalette16 palette = PartyColors_p;
	uint8_t beat = beatsin8(BeatsPerMinute, 64, 255);
	for (int i = 0; i < NUM_LEDS; i++)
	{ //9948
		leds[i] = ColorFromPalette(palette, gHue + (i * 2), beat - gHue + (i * 10));
	}
}

void sinelon()
{
	// a colored dot sweeping back and forth, with fading trails
	fadeToBlackBy(leds, NUM_LEDS, 20);
	int pos = beatsin16(13, 0, NUM_LEDS - 1);
	leds[pos] += CHSV(gHue, 255, 192);
}

void rainbow()
{
	// FastLED's built-in rainbow generator
	fill_rainbow(leds, NUM_LEDS, gHue, 7);
}

void rainbowWithGlitter()
{
	// built-in FastLED rainbow, plus some random sparkly glitter
	rainbow();
	addGlitter(80);
}

void setup()
{

	delay(2000); // sanity check delay - allows reprogramming if accidently blowing power w/leds

	Serial.begin(115200);
	while (!Serial)
	{
		;
	} // wait for Serial port to connect. Needed for native USB port only

	Serial.println("   __  ______  _____  __         __    __         __  ");
	Serial.println("  /  |/  / _ \\/ ___/ / / ___ ___/ /___/ /__  ____/ /__");
	Serial.println(" / /|_/ / // / (_ / / /_/ -_) _  / __/ / _ \\/ __/  '_/");
	Serial.println("/_/  /_/____/\\___/ /____|__/\\_,_/\\__/_/\\___/\\__/_/\\_\\ ");
	Serial.println();

	// Initialize LittleFS
	if (!LittleFS.begin())
	{
		//Serial.println("An Error has occurred while mounting LittleFS");
		return;
	}

	///// Config file /////
	File configfile = LittleFS.open("/config.json", "r");
	if (!configfile)
	{
		//Serial.println("Config file open failed");
	}
	// Allocate a temporary JsonDocument
	// Don't forget to change the capacity to match your requirements.
	// Use arduinojson.org/assistant to compute the capacity.
	DynamicJsonDocument config_doc(512);

	jsonConfigfile = (configfile.readString());

	// Deserialize the JSON document
	DeserializationError error_config = deserializeJson(config_doc, jsonConfigfile);
	if (error_config)
		//Serial.println(F("Failed to read config.json file, using default configuration"));

		configfile.close(); //close file

	///// Schedule file /////
	File schedulefile = LittleFS.open("/schedules.json", "r");
	if (!schedulefile)
	{
		//Serial.println("Schedule file open failed");
	}
	// Allocate a temporary JsonDocument
	// Don't forget to change the capacity to match your requirements.
	// Use arduinojson.org/assistant to compute the capacity.

	DynamicJsonDocument schedule_doc(128 * SCHEDULES);

	jsonSchedulefile = (schedulefile.readString());

	// Deserialize the JSON document
	DeserializationError error_schedule = deserializeJson(schedule_doc, jsonSchedulefile);
	if (error_schedule)
		//Serial.println(F("Failed to read schedule.json file, using default configuration"));

		schedulefile.close(); //close file

	for (int i = 0; i <= SCHEDULES - 1; i++)
	{
		schedules[i].active = schedule_doc[i]["a"] | 0;
		schedules[i].activeclockdisplay = schedule_doc[i]["acd"] | 0;
		schedules[i].hour = schedule_doc[i]["h"] | 0;
		schedules[i].minute = schedule_doc[i]["m"] | 0;
	}

	////// Config file //////
	File clkdisplaysfile = LittleFS.open("/clkdisplays.json", "r");

	if (!clkdisplaysfile)
	{
		//Serial.println("Clock display file open failed");
	}

	// Allocate a temporary JsonDocument
	// Don't forget to change the capacity to match your requirements.
	// Use arduinojson.org/assistant to compute the capacity.
	DynamicJsonDocument clkdisplays_doc(256 * CLOCK_DISPLAYS);

	jsonClkdisplaysfile = (clkdisplaysfile.readString());

	// Deserialize the JSON document
	DeserializationError error_clkdisplays = deserializeJson(clkdisplays_doc, jsonClkdisplaysfile);
	if (error_clkdisplays)
		//Serial.println(F("Failed to read file clkdisplays.json, using default configuration"));

		clkdisplaysfile.close(); //close file

	for (int i = 0; i <= CLOCK_DISPLAYS - 1; i++)
	{

		clockdisplays[i].backgroundColor = clkdisplays_doc[i]["bgc"] | 0x002622;
		clockdisplays[i].hourMarkColor = clkdisplays_doc[i]["hmc"] | 0xb81f00;
		clockdisplays[i].hourColor = clkdisplays_doc[i]["hc"] | 0xffe3e9;
		clockdisplays[i].minuteColor = clkdisplays_doc[i]["mc"] | 0xffe3e9;
		clockdisplays[i].secondColor = clkdisplays_doc[i]["sc"] | 0xffe3e9;
		clockdisplays[i].showms = clkdisplays_doc[i]["ms"] | 0;
		clockdisplays[i].showseconds = clkdisplays_doc[i]["s"] | 1;
		clockdisplays[i].autobrightness = clkdisplays_doc[i]["ab"] | 1;
		clockdisplays[i].brightness = clkdisplays_doc[i]["bn"] | 128;
		clockdisplays[i].backgroud_effect = clkdisplays_doc[i]["be"] | 0;
	}

	config.activeclockdisplay = config_doc["acd"] | 0;

	strlcpy(config.tz, config_doc["tz"] | "UTC", sizeof(config.tz));
	strlcpy(config.ssid, config_doc["ssid"] | "", sizeof(config.ssid));
	strlcpy(config.wifipassword, config_doc["wp"] | "", sizeof(config.wifipassword));
	strlcpy(config.hostname, config_doc["hn"] | "ledclock", sizeof(config.hostname));
	strlcpy(config.mqttserver, config_doc["ms"] | "", sizeof(config.mqttserver));
	strlcpy(config.mqttuser, config_doc["mu"] | "", sizeof(config.mqttuser));
	strlcpy(config.mqttpassword, config_doc["mpw"] | "", sizeof(config.mqttpassword));
	config.mqttport = config_doc["mp"] | 1883;
	config.mqtttls = config_doc["mt"] | 0;

	FastLED.setBrightness(128);
	FastLED.setMaxPowerInVoltsAndMilliamps(NEOPIXEL_VOLTAGE, NEOPIXEL_MILLIAMPS); //max 1 amp power usage

	FastLED.addLeds<SK6812, DATA_PIN, GRB>(leds, NUM_LEDS) // GRB ordering is typical
														   //.setCorrection(TypicalLEDStrip);
		.setCorrection(0xFFFFFF);						   //No correction
	FastLED.setMaxRefreshRate(0);

	//MQTT
	mqttClient.onConnect(onMqttConnect);
	mqttClient.onDisconnect(onMqttDisconnect);
	mqttClient.onPublish(onMqttPublish);
	mqttClient.onMessage(onMqttMessage);
	mqttClient.onSubscribe(onMqttSubscribe);
	if (strlen(config.mqttuser) != 0 && strlen(config.mqttpassword) != 0) //do not set credentials when MQTT user and MQTT password are empty
	{
		mqttClient.setCredentials(config.mqttuser, config.mqttpassword);
	}
	mqttClient.setClientId(config.hostname);
	if (config.mqtttls == 1)
	{
		//Serial.println("Secure MQTT");
		mqttClient.setSecure(true);
	}
	else
	{
		//Serial.println("Insecure MQTT");
	}
	mqttClient.setServer(config.mqttserver, config.mqttport);

	//WIFI
	wifiConnectHandler = WiFi.onStationModeGotIP(onWifiConnect);
	wifiDisconnectHandler = WiFi.onStationModeDisconnected(onWifiDisconnect);

	WiFi.mode(WIFI_STA);

	WiFi.hostname(config.hostname);

	scanWifi(config.ssid);

	if (rssi != -999) //connect to strongest ap
	{
		WiFi.begin(config.ssid, config.wifipassword, channel);
		//Serial.println("Connected to channel: " + String(channel));
	}
	else
	{
		WiFi.begin(config.ssid, config.wifipassword);
	}

	if (checkConnection())
	{
		wifiConnected = true;
		//Only when there is a wifi connection get time from NTP
		waitForSync();

		WiFiClient client;

		//ESPhttpUpdate.update(client, "192.168.178.100", 90, "/bin/firmware.bin");
	}
	else
	{
		wifiConnected = false;
	}

	byte mac[6];

	WiFi.macAddress(mac);

	Serial.println("----------------- [Information] ------------------");
	Serial.print("Clock Model: ");
	Serial.println(CLOCK_MODEL);
	Serial.print("Software version: ");
	Serial.println(SOFTWARE_VERSION);
	Serial.print("MAC address: ");
	Serial.println(WiFi.macAddress());
	String apPassword = String(mac[5], HEX) + String(mac[4], HEX) + String(mac[3], HEX) + String(mac[2] + mac[5], HEX);
	Serial.println("AP Password: " + apPassword);
	Serial.println("Hostname: " + String(config.hostname));
	Serial.println("SSID: " + WiFi.SSID());
	Serial.println("rssi: " + String(WiFi.RSSI()));
	Serial.println("Channel: " + String(channel));
	Serial.println("IP Address: " + WiFi.localIP().toString());
	Serial.println("MQTT Server: " + String(config.mqttserver));
	Serial.println("MQTT Port: " + String(config.mqttport));
	Serial.println("MQTT Connected: " + String(MQTTConnected));
	Serial.println("MQTT TLS: " + String(config.mqtttls));

	if (wifiConnected == false)
	{
		WiFi.mode(WIFI_AP); //Accespoint mode
		WiFi.softAP(AP_NAME, apPassword);
	}

	if (!MDNS.begin(config.hostname))
	{ // Start the mDNS responder for esp8266.local
		Serial.println("Error setting up MDNS responder!");
	}

	// Provide official timezone names
	// https://en.wikipedia.org/wiki/List_of_tz_database_time_zones
	if (!tz.setCache(0))
		tz.setLocation(config.tz);

	Serial.println("Time zone: " + String(config.tz));
	Serial.println("---------------------------------------------------");

	// Webserver
	// Route for root / web page

	//server.serveStatic("/mdg-ledr.js", LittleFS, "/mdg-ledr.js", "max-age=31536000");
	//server.serveStatic("/mdg-ledr.css", LittleFS, "mdg-ledr.css", "max-age=31536000");
	//server.serveStatic("/mdi-font.ttf", LittleFS, "/mdi-font.ttf", "max-age=31536000");
	//server.serveStatic("/mdi-font.woff", LittleFS, "/mdi-font.woff", "max-age=31536000");
	//server.serveStatic("/mdi-font.woff2", LittleFS, "/mdi-font.woff2", "max-age=31536000");

	// Webserver
	// Route for root / web page
	server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
			  {
				  AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", index_html_gz, index_html_gz_len, processor);
				  response->addHeader("Cache-Control", "max-age=31536000");
				  //response->addHeader("Content-Encoding", "gzip");
				  response->addHeader("ETag", String(index_html_gz_len));
				  request->send(response);
			  });

	server.on("/mdg-ledr.js", HTTP_GET, [](AsyncWebServerRequest *request)
			  {
				  AsyncWebServerResponse *response = request->beginResponse_P(200, "application/javascript", mdg_ledr_js, mdg_ledr_js_len);
				  response->addHeader("Cache-Control", "max-age=31536000");
				  response->addHeader("Content-Encoding", "gzip");
				  response->addHeader("ETag", String(mdg_ledr_js_len));
				  request->send(response);
			  });
	server.on("/mdg-ledr.css", HTTP_GET, [](AsyncWebServerRequest *request)
			  {
				  AsyncWebServerResponse *response = request->beginResponse_P(200, "text/css", mdg_ledr_css_gz, mdg_ledr_css_gz_len);
				  response->addHeader("Cache-Control", "max-age=31536000");
				  response->addHeader("Content-Encoding", "gzip");
				  response->addHeader("ETag", String(mdg_ledr_css_gz_len));
				  request->send(response);
			  });
	server.on("/mdi-font.ttf", HTTP_GET, [](AsyncWebServerRequest *request)
			  {
				  AsyncWebServerResponse *response = request->beginResponse_P(200, "font", mdi_font_ttf_gz, mdi_font_ttf_gz_len);
				  response->addHeader("Cache-Control", "max-age=31536000");
				  response->addHeader("Content-Encoding", "gzip");
				  response->addHeader("ETag", String(mdi_font_ttf_gz_len));
				  request->send(response);
			  });
	server.on("/mdi-font.woff", HTTP_GET, [](AsyncWebServerRequest *request)
			  {
				  AsyncWebServerResponse *response = request->beginResponse_P(200, "font", mdi_font_woff_gz, mdi_font_woff_gz_len);
				  response->addHeader("Cache-Control", "max-age=31536000");
				  response->addHeader("Content-Encoding", "gzip");
				  response->addHeader("ETag", String(mdi_font_woff_gz_len));
				  request->send(response);
			  });
	server.on("/mdi-font.woff2", HTTP_GET, [](AsyncWebServerRequest *request)
			  {
				  AsyncWebServerResponse *response = request->beginResponse_P(200, "font", mdi_font_woff2_gz, mdi_font_woff2_gz_len);
				  response->addHeader("Cache-Control", "max-age=31536000");
				  response->addHeader("Content-Encoding", "gzip");
				  response->addHeader("ETag", String(mdi_font_woff2_gz_len));
				  request->send(response);
			  });

	server.on("/save-config", HTTP_GET, [](AsyncWebServerRequest *request) { //save config
		//Serial.println("save");

		saveConfiguration("/config.json");
		request->send(200, "text/plain", "OK");
	});

	server.on("/scan-networks", HTTP_GET, [](AsyncWebServerRequest *request) { //scan networks
		WiFi.scanNetworksAsync([request](int numNetworks)
							   {
								   DynamicJsonDocument data(2048);
								   int o = numNetworks;
								   int loops = 0;

								   if (numNetworks > 0)

								   {
									   // sort by RSSI
									   int indices[numNetworks];
									   int skip[numNetworks];

									   String ssid;

									   for (int i = 0; i < numNetworks; i++)
									   {
										   indices[i] = i;
									   }

									   // CONFIG
									   bool sortRSSI = true;   // sort aps by RSSI
									   bool removeDups = true; // remove dup aps ( forces sort )

									   if (removeDups || sortRSSI)
									   {
										   for (int i = 0; i < numNetworks; i++)
										   {
											   for (int j = i + 1; j < numNetworks; j++)
											   {
												   if (WiFi.RSSI(indices[j]) > WiFi.RSSI(indices[i]))
												   {
													   loops++;
													   std::swap(indices[i], indices[j]);
													   std::swap(skip[i], skip[j]);
												   }
											   }
										   }
									   }

									   if (removeDups)
									   {
										   for (int i = 0; i < numNetworks; i++)
										   {
											   if (indices[i] == -1)
											   {
												   --o;
												   continue;
											   }
											   ssid = WiFi.SSID(indices[i]);
											   for (int j = i + 1; j < numNetworks; j++)
											   {
												   loops++;
												   if (ssid == WiFi.SSID(indices[j]))
												   {
													   indices[j] = -1;
												   }
											   }
										   }
									   }

									   for (int i = 0; i < numNetworks; ++i)
									   {
										   if (indices[i] != -1)
										   {
											   JsonObject obj = data.createNestedObject();
											   obj["i"] = i;
											   obj["ssid"] = WiFi.SSID(indices[i]);
										   }
									   }
								   }

								   String response;
								   serializeJson(data, response);
								   request->send(200, "application/json", response);
							   });

	});

	server.on("/save-clockdisplays", HTTP_GET, [](AsyncWebServerRequest *request) { //save clockdisplays
		saveClockDisplays("/clkdisplays.json");
		request->send(200, "text/plain", "OK");
	});
	server.on("/get-settings", HTTP_GET, [](AsyncWebServerRequest *request)
			  {
				  DynamicJsonDocument data(1024);
				  data["acd"] = config.activeclockdisplay;
				  data["tz"] = config.tz;
				  data["ssid"] = config.ssid;
				  if (String(config.wifipassword).isEmpty())
				  {
					  data["wp"] = String("");
				  }
				  else
				  {
					  data["wp"] = String("********");
				  }

				  data["hn"] = config.hostname;
				  data["ms"] = config.mqttserver;
				  data["mp"] = config.mqttport;
				  data["mu"] = config.mqttuser;
				  if (String(config.mqttpassword).isEmpty())
				  {
					  data["mpw"] = String("");
				  }
				  else
				  {
					  data["mpw"] = String("********");
				  }

				  data["mt"] = config.mqtttls;

				  String response;
				  serializeJson(data, response);
				  request->send(200, "application/json", response);
			  });
	server.on("/get-clockdisplay", HTTP_GET, [](AsyncWebServerRequest *request)
			  {
				  DynamicJsonDocument data(256);
				  if (request->hasParam("id"))
				  {
					  int i = (request->getParam("id")->value()).toInt();

					  data["bgc"] = String(inttoHex(clockdisplays[i].backgroundColor, 6));
					  data["hmc"] = String(inttoHex(clockdisplays[i].hourMarkColor, 6));
					  data["hc"] = String(inttoHex(clockdisplays[i].hourColor, 6));
					  data["mc"] = String(inttoHex(clockdisplays[i].minuteColor, 6));
					  data["sc"] = String(inttoHex(clockdisplays[i].secondColor, 6));
					  data["ms"] = clockdisplays[i].showms;
					  data["s"] = clockdisplays[i].showseconds;
					  data["ab"] = clockdisplays[i].autobrightness;
					  data["bn"] = clockdisplays[i].brightness;
					  data["be"] = clockdisplays[i].backgroud_effect;
				  }
				  else
				  {
					  data["message"] = "No clockdisplay id";
				  }
				  String response;
				  serializeJson(data, response);
				  request->send(200, "application/json", response);
			  });
	server.on("/get-schedule", HTTP_GET, [](AsyncWebServerRequest *request)
			  {
				  DynamicJsonDocument data(256);
				  if (request->hasParam("id"))
				  {

					  int i = (request->getParam("id")->value()).toInt();
					  if (i <= SCHEDULES)
					  {
						  data["a"] = schedules[i].active;
						  data["acd"] = schedules[i].activeclockdisplay;
						  data["h"] = schedules[i].hour;
						  data["m"] = schedules[i].minute;
					  }
					  else
					  {
						  data["message"] = "Schedule not found";
					  }
				  }
				  else
				  {
					  data["message"] = "No Schedule id";
				  }
				  String response;
				  serializeJson(data, response);
				  request->send(200, "application/json", response);
			  });
	server.on("/get-schedules", HTTP_GET, [](AsyncWebServerRequest *request)
			  {
				  DynamicJsonDocument data(128 * SCHEDULES);

				  for (int i = 0; i <= SCHEDULES - 1; i++)
				  {
					  JsonObject obj = data.createNestedObject();
					  obj["i"] = i;
					  obj["a"] = schedules[i].active;
					  obj["acd"] = schedules[i].activeclockdisplay;
					  obj["h"] = schedules[i].hour;
					  obj["m"] = schedules[i].minute;
				  }

				  String response;
				  serializeJson(data, response);
				  request->send(200, "application/json", response);
			  });
	server.on("/set-schedule", HTTP_GET, [](AsyncWebServerRequest *request)
			  {
				  int paramsNr = request->params();

				  for (int i = 0; i < paramsNr; i++)
				  {
					  AsyncWebParameter *p = request->getParam(i);

					  if (p->name() == "id")
					  {
						  scheduleId = p->value().toInt();
					  }
					  else if (p->name() == "hour")
					  {
						  schedules[scheduleId].hour = p->value().toInt();
					  }
					  else if (p->name() == "minute")
					  {
						  schedules[scheduleId].minute = p->value().toInt();
					  }
					  else if (p->name() == "active")
					  {
						  schedules[scheduleId].active = p->value().toInt();
					  }
					  else if (p->name() == "activeclockdisplay")
					  {
						  schedules[scheduleId].activeclockdisplay = p->value().toInt();
					  }
				  }
				  saveSchedules("/schedules.json");

				  request->send(200, "text/plain", "OK");
			  });
	server.on("/reboot", HTTP_GET, [](AsyncWebServerRequest *request) { //reboot ESP
		request->send(200, "text/plain", "OK");
		delay(500);
		ESP.restart();
	});
	server.on("/color", HTTP_GET, [](AsyncWebServerRequest *request)
			  {
				  String inputMessage;

				  if (request->hasParam("hourcolor"))
				  {
					  inputMessage = request->getParam("hourcolor")->value();
					  clockdisplays[config.activeclockdisplay].hourColor = hstol(inputMessage);
				  }
				  else if (request->hasParam("minutecolor"))
				  {
					  inputMessage = request->getParam("minutecolor")->value();
					  clockdisplays[config.activeclockdisplay].minuteColor = hstol(inputMessage);
				  }
				  else if (request->hasParam("secondcolor"))
				  {
					  inputMessage = request->getParam("secondcolor")->value();
					  clockdisplays[config.activeclockdisplay].secondColor = hstol(inputMessage);
				  }
				  else if (request->hasParam("backgroundcolor"))
				  {
					  inputMessage = request->getParam("backgroundcolor")->value();
					  clockdisplays[config.activeclockdisplay].backgroundColor = hstol(inputMessage);
				  }
				  else if (request->hasParam("hourmarkcolor"))
				  {
					  inputMessage = request->getParam("hourmarkcolor")->value();
					  clockdisplays[config.activeclockdisplay].hourMarkColor = hstol(inputMessage);
				  }
				  else
				  {
					  inputMessage = "No message sent";
				  }
				  request->send(200, "text/plain", "OK");
			  });
	server.on("/configuration", HTTP_GET, [](AsyncWebServerRequest *request)
			  {
				  int paramsNr = request->params();

				  for (int i = 0; i < paramsNr; i++)
				  {
					  AsyncWebParameter *p = request->getParam(i);

					  if (p->name() == "brightness")
					  {
						  if (clockdisplays[config.activeclockdisplay].autobrightness == 0)
						  {
							  clockdisplays[config.activeclockdisplay].brightness = p->value().toInt();

							  int NumtToBrightness = map(clockdisplays[config.activeclockdisplay].brightness, 0, 255, MIN_BRIGHTNESS, MAX_BRIGHTNESS);
							  FastLED.setBrightness(NumtToBrightness);
						  }
					  }
					  else if (p->name() == "showms")
					  {
						  clockdisplays[config.activeclockdisplay].showms = p->value().toInt();
					  }
					  else if (p->name() == "activeclockdisplay")
					  {
						  config.activeclockdisplay = p->value().toInt();
					  }
					  else if (p->name() == "showseconds")
					  {
						  clockdisplays[config.activeclockdisplay].showseconds = p->value().toInt();
					  }
					  else if (p->name() == "autobrightness")
					  {
						  clockdisplays[config.activeclockdisplay].autobrightness = p->value().toInt();
					  }
					  else if (p->name() == "bg_effect")
					  {
						  clockdisplays[config.activeclockdisplay].backgroud_effect = p->value().toInt();
					  }
					  else if (p->name() == "ssid")
					  {

						  p->value().toCharArray(config.ssid, sizeof(config.ssid));
					  }
					  else if (p->name() == "wifipassword")
					  {

						  p->value().toCharArray(config.wifipassword, sizeof(config.wifipassword));
					  }
					  else if (p->name() == "hostname")
					  {

						  if (p->value().isEmpty())
						  {
							  //Do nothing, we dont want an empty hostname
						  }
						  else
						  {
							  p->value().toCharArray(config.hostname, sizeof(config.hostname));
						  }
					  }
					  else if (p->name() == "mqtt_server")
					  {

						  p->value().toCharArray(config.mqttserver, sizeof(config.mqttserver));
					  }
					  else if (p->name() == "mqtt_port")
					  {

						  if (!p->value().isEmpty())
						  {
							  config.mqttport = p->value().toInt();
						  }
						  else
						  {
							  config.mqttport = 1883;
						  }
					  }
					  else if (p->name() == "mqtt_user")
					  {

						  p->value().toCharArray(config.mqttuser, sizeof(config.mqttuser));
					  }
					  else if (p->name() == "mqtt_password")
					  {

						  p->value().toCharArray(config.mqttpassword, sizeof(config.mqttpassword));
					  }
					  else if (p->name() == "mqtt_tls")
					  {

						  config.mqtttls = p->value().toInt();
					  }
					  else if (p->name() == "tz")
					  {
						  tz.clearCache();
						  p->value().toCharArray(config.tz, sizeof(config.tz));
					  }
				  }

				  request->send(200, "text/plain", "OK");
			  });

	MDNS.addService("http", "tcp", 80);

	// Start ElegantOTA
	//	AsyncElegantOTA.begin(&server);

	// Start server
	server.begin();
}

void loop()
{
	currentMillis = millis();

	events();

	//AsyncElegantOTA.loop();

	MDNS.update();

	//Process Schedule every new minute
	if (currentMinute != tz.minute()) //check if a minute has passed
	{

		currentMinute = tz.minute(); //set current minute
		//Serial.println("Check for event.....");
		//	breakTime(tz.now(), tm);
		for (int i = 0; i <= SCHEDULES - 1; i++)
		{
			if (tz.hour() == schedules[i].hour && tz.minute() == schedules[i].minute && schedules[i].active == 1)
			{
				config.activeclockdisplay = schedules[i].activeclockdisplay;
				//Serial.println("Schedule active..");
			}
		}
	}

	if (clockdisplays[config.activeclockdisplay].backgroud_effect > 0) //show effect
	{
		EVERY_N_MILLISECONDS(20) { gHue++; }

		switch (clockdisplays[config.activeclockdisplay].backgroud_effect)
		{
		case 1:
			rainbow();
			break;
		case 2:
			rainbowWithGlitter();
			break;
		case 3:
			juggle();
			break;
		case 4:
			confetti();
			break;
		case 5:
			bpm();
			break;
		default:
			break;
		}
	}
	else
	{
		//Set background color

		for (int Led = 0; Led < 60; Led = Led + 1)
		{
			leds[rotate(Led)] = clockdisplays[config.activeclockdisplay].backgroundColor;
		}
	}
	if (hourmarks_state)
	{
		//Set hour marks
		for (int Led = 0; Led <= 55; Led = Led + 5)
		{
			leds[rotate(Led)] = clockdisplays[config.activeclockdisplay].hourMarkColor;
			//leds[rotate(Led)].fadeToBlackBy(230);
		}
	}

	if (hour_state)
	{
		//Hour hand
		int houroffset = map(tz.minute(), 0, 60, 0, 5); //move the hour hand when the minutes pass

		uint8_t hrs12 = (tz.hour() % 12);
		leds[rotate((hrs12 * 5) + houroffset)] = clockdisplays[config.activeclockdisplay].hourColor;

		/* if (hrs12 == 0)
	{
		leds[rotate((hrs12 * 5) + houroffset) + 1] = clockdisplays[config.activeclockdisplay].hourColor;
		leds[rotate(59)] = clockdisplays[config.activeclockdisplay].hourColor;
	}
	else
	{
		leds[rotate((hrs12 * 5) + houroffset) + 1] = clockdisplays[config.activeclockdisplay].hourColor;
		leds[rotate((hrs12 * 5) + houroffset) - 1] = clockdisplays[config.activeclockdisplay].hourColor;
	} */
	}

	if (minute_state)
	{
		//Minute hand
		leds[rotate(tz.minute())] = clockdisplays[config.activeclockdisplay].minuteColor;
	}

	//Second hand
	if (clockdisplays[config.activeclockdisplay].showseconds == 1)
	{

		/*
		int msto255 = map(tz.ms(), 1, 999, 0, 255);
		CRGB pixelColor1 = blend(clockdisplays[config.activeclockdisplay].secondColor, leds[rotate(tz.second() - 1)], msto255);
		CRGB pixelColor2 = blend(leds[rotate(tz.second())], clockdisplays[config.activeclockdisplay].secondColor, msto255);

		leds[rotate(tz.second())] = pixelColor2;
		leds[rotate(tz.second() - 1)] = pixelColor1;
*/
		//normal

		leds[rotate(tz.second())] = clockdisplays[config.activeclockdisplay].secondColor;
		//Serial.println(tz.second());
	}

	if (clockdisplays[config.activeclockdisplay].showms == 1)
	{
		int ms = map(tz.ms(), 1, 1000, 0, 59);

		//leds[rotate(ms)] = config.secondColor;
		leds[rotate(ms)] = clockdisplays[config.activeclockdisplay].secondColor;
	}

	// Set Brightness
	if (clockdisplays[config.activeclockdisplay].autobrightness == 1)
	{
		//Set brightness when auto brightness is active
		if (currentMillis - lastExecutedMillis_brightness >= EXE_INTERVAL_AUTO_BRIGHTNESS)
		{
			lastExecutedMillis_brightness = currentMillis; // save the last executed time

			// subtract the last reading:
			total = total - readings[readIndex];

			// read from the sensor:
			readings[readIndex] = analogRead(LIGHTSENSORPIN);

			// add the reading to the total:
			total = total + readings[readIndex];
			// advance to the next position in the array:
			readIndex = readIndex + 1;

			// if we're at the end of the array...
			if (readIndex >= numReadings)
			{
				// ...wrap around to the beginning:
				readIndex = 0;
			}

			// calculate the average:
			average = total / numReadings;

			lux = average * 0.9765625; // 1000/1024

			int brightnessMap = map(average, 3, 45, MIN_BRIGHTNESS, MAX_BRIGHTNESS);

			brightnessMap = constrain(brightnessMap, MIN_BRIGHTNESS, MAX_BRIGHTNESS);

			sliderBrightnessValue = brightnessMap;
			FastLED.setBrightness(brightnessMap);
		}
	}
	else
	{
		sliderBrightnessValue = clockdisplays[config.activeclockdisplay].brightness;

		int brightnessMap = map(sliderBrightnessValue, 0, 255, MIN_BRIGHTNESS, MAX_BRIGHTNESS);
		brightnessMap = constrain(brightnessMap, MIN_BRIGHTNESS, MAX_BRIGHTNESS);
	}

	if (MQTTConnected)
	{
		if (currentMillis - lastExecutedMillis_lightsensorMQTT >= EXE_INTERVAL_LIGHTSENSOR_MQTT)
		{
			lastExecutedMillis_lightsensorMQTT = currentMillis; // save the last executed time
			snprintf(m_msg_buffer, MSG_BUFFER_SIZE, "%0.2f", lux);
			snprintf(m_topic_buffer, sizeof(m_topic_buffer), "%s%s", config.hostname, MQTT_LIGHT_SENSOR_STATE_TOPIC);
			mqttClient.publish(m_topic_buffer, 0, true, m_msg_buffer);
		}
	}

	//FastLED.show();
	FastLED.delay(1000 / 400);
}