/*  MDG Lightclock
    Copyright (C) 2022  M. de Graaf

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <Arduino.h>
#include <helperfunctions.h>
#include <ezTime.h> // using modified library in /lib (changed host to timezoned.mdg-design.nl)
#include <ESP8266httpUpdate.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <FastLED.h>
#include <PubSubClient.h>
#include <ESPAsyncTCP.h> //	using modified library in /lib to correct SSL error when compiling with build_flags = -DASYNC_TCP_SSL_ENABLED=1  see (https://github.com/mhightower83/ESPAsyncTCP#correct-ssl-_recv) when this pull request is complete we can continue to use the original library.
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <web/mdg_ledr_js.h>
#include <web/mdg_ledr_css.h>
#include <web/index_html.h>
#include <web/fonts.h>

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

//MQTT
long lastReconnectAttempt = 0;
boolean MQTTConnected = false; // Variable to store MQTT connected state
BearSSL::WiFiClientSecure TLSClient;
WiFiClient espClient;
PubSubClient mqttClient; //uninitialised pubsub client instance. The client is initialised as TLS or espClient in setup()

//MQTT Payload buffer
const uint8_t MSG_BUFFER_SIZE = 16;
char m_msg_buffer[MSG_BUFFER_SIZE];
//MQTT Topic buffer
const uint8_t TOPIC_BUFFER_SIZE = 64;
char m_topic_buffer[TOPIC_BUFFER_SIZE];

//WiFi
boolean wifiConnected = false; // Variable to store wifi connected state
unsigned char bssid[6];
byte channel;
int rssi = -999;

//Lightsensor
unsigned long lastExecutedMillis_brightness = 0;	  // Variable to save the last executed time
unsigned long lastExecutedMillis_lightsensorMQTT = 0; // Variable to save the last executed time
float lightReading;									  // Variable to store the ambient light value
float lux;											  //LUX

// Define the number of samples to keep track of. The higher the number, the
// more the readings will be smoothed, but the slower the output will respond to
// the input. Using a constant rather than a normal variable lets us use this
// value to determine the size of the readings array.
const int numReadings = 20;

int readings[numReadings]; // the readings from the analog input
int readIndex = 0;		   // the index of the current reading
int total = 0;			   // the running total
int average = 0;		   // the average

//Keep track of the total runtime in mili's
unsigned long currentMillis;

CRGB leds[NUM_LEDS]; // This is an array of leds, one item for each led in the clock

int sliderBrightnessValue = 128; // Variable to store brightness slidervalue in webserver

// Variable used to store the mqtt state of the whole clock display
boolean display_state = true;

// Variables used to store the mqtt state, the brightness and the color of the specific clock items
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

// Arrays with above variables
boolean array_state[] = {hour_state, minute_state, second_state, hourmarks_state, background_state};
uint8_t array_brightness[] = {hour_brightness, minute_brightness, second_brightness, hourmarks_brightness, background_brightness};
uint8_t array_rgb_red[] = {hour_rgb_red, minute_rgb_red, second_rgb_red, hourmarks_rgb_red, background_rgb_red};
uint8_t array_rgb_green[] = {hour_rgb_green, minute_rgb_green, second_rgb_green, hourmarks_rgb_green, background_rgb_green};
uint8_t array_rgb_blue[] = {hour_rgb_blue, minute_rgb_blue, second_rgb_blue, hourmarks_rgb_blue, background_rgb_blue};
int array_index;
boolean topic_match = false;
const char *state_topic;

// MQTT: topics
// Ledclock all state
const PROGMEM char *MQTT_DISPLAY_STATE_TOPIC = "/display/state";
const PROGMEM char *MQTT_DISPLAY_COMMAND_TOPIC = "/display/set";
// Hour state
const PROGMEM char *MQTT_HOUR_STATE_TOPIC = "/hour/state";
const PROGMEM char *MQTT_HOUR_COMMAND_TOPIC = "/hour/set";
// Minute state
const PROGMEM char *MQTT_MINUTE_STATE_TOPIC = "/minute/state";
const PROGMEM char *MQTT_MINUTE_COMMAND_TOPIC = "/minute/set";
// Second state
const PROGMEM char *MQTT_SECOND_STATE_TOPIC = "/second/state";
const PROGMEM char *MQTT_SECOND_COMMAND_TOPIC = "/second/set";
// Background state
const PROGMEM char *MQTT_BACKGROUND_STATE_TOPIC = "/background/state";
const PROGMEM char *MQTT_BACKGROUND_COMMAND_TOPIC = "/background/set";
// Hourmarks state
const PROGMEM char *MQTT_HOURMARKS_STATE_TOPIC = "/hourmarks/state";
const PROGMEM char *MQTT_HOURMARKS_COMMAND_TOPIC = "/hourmarks/set";
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

// function called to adapt the brightness and the color of the led
void setColor(uint8_t p_red, uint8_t p_green, uint8_t p_blue, int p_index)
{
	// convert the brightness in % (0-100%) into a digital value (0-255)
	uint8_t brightness = map(array_brightness[p_index], 0, 100, 0, 255);

	p_red = map(p_red, 0, 255, 0, brightness);
	p_green = map(p_green, 0, 255, 0, brightness);
	p_blue = map(p_blue, 0, 255, 0, brightness);

	long RGB = ((long)p_red << 16L) | ((long)p_green << 8L) | (long)p_blue;

	switch (p_index)
	{
	case 0:
		clockdisplays[config.activeclockdisplay].hourColor = RGB;
		break;
	case 1:
		clockdisplays[config.activeclockdisplay].minuteColor = RGB;
		break;
	case 2:
		clockdisplays[config.activeclockdisplay].secondColor = RGB;
		break;
	case 3:
		clockdisplays[config.activeclockdisplay].hourMarkColor = RGB;
		break;
	case 4:
		clockdisplays[config.activeclockdisplay].backgroundColor = RGB;
		break;
	}
}

void publishState(int index, const char *TOPIC)
{

	DynamicJsonDocument mqtt_payload(256);
	if (!(String(MQTT_DISPLAY_STATE_TOPIC)).equals(TOPIC) && index > -1)
	{
		mqtt_payload["state"] = array_state[index] ? LIGHT_ON : LIGHT_OFF;
		mqtt_payload["brightness"] = array_brightness[index];

		JsonObject color = mqtt_payload.createNestedObject("color");
		color["r"] = array_rgb_red[index];
		color["g"] = array_rgb_green[index];
		color["b"] = array_rgb_blue[index];
		mqtt_payload["color_mode"] = "rgb";
	}
	else //only state and brightness is needed for complete display
	{
		mqtt_payload["state"] = display_state ? LIGHT_ON : LIGHT_OFF;
		mqtt_payload["brightness"] = map(clockdisplays[config.activeclockdisplay].brightness, 0, 255, 0, 100);
	}

	char mqtt_payload_buffer[measureJson(mqtt_payload) + 1];
	serializeJson(mqtt_payload, mqtt_payload_buffer, sizeof(mqtt_payload_buffer));

	snprintf(m_topic_buffer, sizeof(m_topic_buffer), "%s%s", config.hostname, TOPIC);

#ifdef MQTT_DEBUG
	Serial.println(m_topic_buffer);
	Serial.println(mqtt_payload_buffer);
#endif
	mqttClient.publish(m_topic_buffer, mqtt_payload_buffer, true);
}

boolean MQTTconnect()
{
#ifdef MQTT_DEBUG
	Serial.println("INFO: Attempting MQTT connection...");
#endif
	// Attempt to connect

	if (mqttClient.connect(config.hostname, config.mqttuser, config.mqttpassword))
	{
#ifdef MQTT_DEBUG
		Serial.println("INFO: connected");
#endif
		snprintf(m_topic_buffer, sizeof(m_topic_buffer), "%s%s", config.hostname, "/+/set");
#ifdef MQTT_DEBUG
		Serial.println("Subscribe to: " + String(m_topic_buffer));
#endif
		mqttClient.subscribe(m_topic_buffer);

		MQTTConnected = true;
	}
	else
	{
#ifdef MQTT_DEBUG
		Serial.print("ERROR: failed, rc=");
		Serial.print(mqttClient.state());
		Serial.println("DEBUG: try again in 5 seconds");
#endif
		MQTTConnected = false;
	}
	return mqttClient.connected();
}

void callback(char *p_topic, byte *p_payload, unsigned int p_length)
{

	String payload;
	for (uint8_t i = 0; i < p_length; i++)
	{
		payload.concat((char)p_payload[i]);
	}

	DynamicJsonDocument mqttbuffer(128);

	// Deserialize the JSON document
	DeserializationError error_config = deserializeJson(mqttbuffer, payload);

	if (error_config)
	{
#ifdef MQTT_DEBUG
		Serial.println("parseObject() failed");
#endif
	}

	if ((String(config.hostname) + String(MQTT_DISPLAY_COMMAND_TOPIC)).equals(p_topic)) //complete clock display on/off + complete clock overall brightness
	{
		if (mqttbuffer.containsKey("state"))
		{
			if (strcmp(mqttbuffer["state"], LIGHT_ON) == 0)
			{
				if (display_state != true)
				{
					display_state = true;
				}
			}
			else if (strcmp(mqttbuffer["state"], LIGHT_OFF) == 0)
			{
				if (display_state != false)
				{
					display_state = false;
				}
			}
		}
		if (mqttbuffer.containsKey("brightness"))
		{
			uint8_t brightness = uint8_t(mqttbuffer["brightness"]);
			if (brightness < 0 || brightness > 100)
			{
				// do nothing...
				return;
			}
			else
			{
				clockdisplays[config.activeclockdisplay].brightness = map(brightness, 0, 100, 0, 255);
				int NumtToBrightness = map(clockdisplays[config.activeclockdisplay].brightness, 0, 255, MIN_BRIGHTNESS, MAX_BRIGHTNESS);
				FastLED.setBrightness(NumtToBrightness);
			}
		}
		publishState(-1, MQTT_DISPLAY_STATE_TOPIC); //publish MQTT state topic
	}

	// Clock specific items (hour, minute, second, hour marks, background)
	if ((String(config.hostname) + String(MQTT_HOUR_COMMAND_TOPIC)).equals(p_topic))
	{
		state_topic = MQTT_HOUR_STATE_TOPIC;
		array_index = 0;
		topic_match = true;
	}
	else if ((String(config.hostname) + String(MQTT_MINUTE_COMMAND_TOPIC)).equals(p_topic))
	{
		state_topic = MQTT_MINUTE_STATE_TOPIC;
		array_index = 1;
		topic_match = true;
	}
	else if ((String(config.hostname) + String(MQTT_SECOND_COMMAND_TOPIC)).equals(p_topic))
	{
		state_topic = MQTT_SECOND_STATE_TOPIC;
		array_index = 2;
		topic_match = true;
	}
	else if ((String(config.hostname) + String(MQTT_HOURMARKS_COMMAND_TOPIC)).equals(p_topic))
	{
		state_topic = MQTT_HOURMARKS_STATE_TOPIC;
		array_index = 3;
		topic_match = true;
	}
	else if ((String(config.hostname) + String(MQTT_BACKGROUND_COMMAND_TOPIC)).equals(p_topic))
	{
		state_topic = MQTT_BACKGROUND_STATE_TOPIC;
		array_index = 4;
		topic_match = true;
	}
	else
	{
		topic_match = false;
	}

	if (topic_match) //only run when there is a mqtt topic match
	{
		if (mqttbuffer.containsKey("state"))
		{
			if (strcmp(mqttbuffer["state"], LIGHT_ON) == 0)
			{
				if (array_state[array_index] != true)
				{
					array_state[array_index] = true;
				}
				display_state = true; //turn on display when one of the clock objects is changed
			}
			else if (strcmp(mqttbuffer["state"], LIGHT_OFF) == 0)
			{
				if (array_state[array_index] != false)
				{
					array_state[array_index] = false;
				}
			}
		}
		if (mqttbuffer.containsKey("brightness"))
		{
			uint8_t brightness = uint8_t(mqttbuffer["brightness"]);
			if (brightness < 0 || brightness > 100)
			{
				//do nothing...
				return;
			}
			else
			{
				array_brightness[array_index] = brightness;
				setColor(array_rgb_red[array_index], array_rgb_green[array_index], array_rgb_blue[array_index], array_index);
				display_state = true; //turn on display when one of the clock objects is changed
			}
		}
		if (mqttbuffer.containsKey("color"))
		{
			uint8_t rgb_red = uint8_t(mqttbuffer["color"]["r"]);
			if (rgb_red < 0 || rgb_red > 255)
			{
				return;
			}
			else
			{
				array_rgb_red[array_index] = rgb_red;
			}

			uint8_t rgb_green = uint8_t(mqttbuffer["color"]["g"]);
			if (rgb_green < 0 || rgb_green > 255)
			{
				return;
			}
			else
			{
				array_rgb_green[array_index] = rgb_green;
			}

			uint8_t rgb_blue = uint8_t(mqttbuffer["color"]["b"]);
			if (rgb_blue < 0 || rgb_blue > 255)
			{
				return;
			}
			else
			{
				array_rgb_blue[array_index] = rgb_blue;
			}
			setColor(rgb_red, rgb_green, rgb_blue, array_index);
		}
		publishState(array_index, state_topic); //publish MQTT state topic
	}
}

// Replaces placeholder %xxx% with section in web page
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
														   //													   .setCorrection(TypicalLEDStrip);
		.setCorrection(0xFFFFFF);						   //No correction
	FastLED.setMaxRefreshRate(0);

	//MQTT

	//
	if (config.mqtttls == 1) //TLS enabled
	{

		TLSClient.setInsecure();
		mqttClient.setClient(TLSClient);
		mqttClient.setSocketTimeout(1);
	}
	else //TLS disabled
	{
		mqttClient.setClient(espClient);
	}

	mqttClient.setServer(config.mqttserver, config.mqttport);
	mqttClient.setCallback(callback);
	mqttClient.setSocketTimeout(1);

	//WIFI
	WiFi.mode(WIFI_STA);

	WiFi.hostname(config.hostname);

	scanWifi(config.ssid);

	if (rssi != -999) //connect to strongest ap
	{
		WiFi.begin(config.ssid, config.wifipassword, channel);
	}
	else
	{
		WiFi.begin(config.ssid, config.wifipassword);
	}

	if (checkConnection())
	{
		wifiConnected = true;
		waitForSync();
		MQTTconnect();

		//WiFiClient httpclient;
		//ESPhttpUpdate.update(httpclient, "192.168.178.100", 90, "/bin/firmware.bin");
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

	// Start web server
	server.begin();
}

void loop()
{

	if (WiFi.isConnected())
	{

		if (strlen(config.mqttserver) != 0)
		{
			if (!mqttClient.connected())
			{
				long now = millis();

				if (now - lastReconnectAttempt > 5000)
				{
					#ifdef MQTT_DEBUG
					Serial.println("Try to reconnect MQTT");
					#endif
					lastReconnectAttempt = now;

					// Attempt to MQTTconnect
					if (MQTTconnect())
					{
						#ifdef MQTT_DEBUG
						Serial.println("Connected to MQTT");
						#endif
						lastReconnectAttempt = 0;
					}
				}
			}
			else
			{
				// Client connected
				mqttClient.loop();
			}
		}
	}

	currentMillis = millis();

	events();

	MDNS.update();

	//Process Schedule every new minute
	if (currentMinute != tz.minute()) //check if a minute has passed
	{

		currentMinute = tz.minute(); //set current minute
		
		//	breakTime(tz.now(), tm);
		for (int i = 0; i <= SCHEDULES - 1; i++)
		{
			if (tz.hour() == schedules[i].hour && tz.minute() == schedules[i].minute && schedules[i].active == 1)
			{
				config.activeclockdisplay = schedules[i].activeclockdisplay;
			
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
			if (array_state[4] && display_state) //if background is on
			{
				leds[rotate(Led)] = clockdisplays[config.activeclockdisplay].backgroundColor;
			}
			else //if background is off
			{
				leds[rotate(Led)] = 0x000000;
			}
		}
	}
	if (array_state[3] & display_state)
	{
		//Set hour marks
		for (int Led = 0; Led <= 55; Led = Led + 5)
		{
			leds[rotate(Led)] = clockdisplays[config.activeclockdisplay].hourMarkColor;
			//leds[rotate(Led)].fadeToBlackBy(230);
		}
	}

	if (array_state[0] && display_state)
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

	if (array_state[1] && display_state)
	{
		//Minute hand
		leds[rotate(tz.minute())] = clockdisplays[config.activeclockdisplay].minuteColor;
	}

	if (array_state[2] && display_state) //mqtt seconds on
	{
		clockdisplays[config.activeclockdisplay].showseconds = 1;
	}
	else //mqtt seconds off
	{
		clockdisplays[config.activeclockdisplay].showseconds = 0;
	}

	//Second hand
	if (clockdisplays[config.activeclockdisplay].showseconds == 1 && display_state)
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

			mqttClient.publish(m_topic_buffer, m_msg_buffer, true);
		}
	}

	//FastLED.show();
	FastLED.delay(1000 / 400);
}