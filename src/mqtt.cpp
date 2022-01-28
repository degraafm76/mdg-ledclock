#include <mqtt.h>

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
const PROGMEM char *MQTT_LIGHT_SENSOR_STATE_TOPIC = "/sensor";

// payloads by default (on/off)
const PROGMEM char *LIGHT_ON = "ON";
const PROGMEM char *LIGHT_OFF = "OFF";

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

void publishSensorState()
{

	DynamicJsonDocument mqtt_payload(32);

	mqtt_payload["lux"] = round(lux);

	char mqtt_payload_buffer[measureJson(mqtt_payload) + 1];
	serializeJson(mqtt_payload, mqtt_payload_buffer, sizeof(mqtt_payload_buffer));

	snprintf(m_topic_buffer, sizeof(m_topic_buffer), "%s%s", config.hostname, MQTT_LIGHT_SENSOR_STATE_TOPIC);
	mqttClient.publish(m_topic_buffer, mqtt_payload_buffer, true);
}

void publishState(int index, const char *TOPIC)
{

	DynamicJsonDocument mqtt_payload(384);
	if (index > -1)
	{
		mqtt_payload["state"] = array_state[index] ? LIGHT_ON : LIGHT_OFF;
		mqtt_payload["brightness"] = array_brightness[index];

		JsonObject color = mqtt_payload.createNestedObject("color");
		color["r"] = array_rgb_red[index];
		color["g"] = array_rgb_green[index];
		color["b"] = array_rgb_blue[index];
		mqtt_payload["color_mode"] = "rgb";
	}
	else if((String(MQTT_DISPLAY_STATE_TOPIC)).equals(TOPIC))//only state and brightness is needed for complete display
	{
		mqtt_payload["state"] = display_state ? LIGHT_ON : LIGHT_OFF;
		mqtt_payload["brightness"] = map(clockdisplays[config.activeclockdisplay].brightness, 0, 255, 0, 100);
		if (clockdisplays[config.activeclockdisplay].autobrightness == 1)
		{
			mqtt_payload["effect"] = "auto_brightness";
		}
		else
		{
			mqtt_payload["effect"] = "manual_brightness";
		}
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

	DynamicJsonDocument mqttbuffer(256);

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
		if (mqttbuffer.containsKey("effect"))
		{

			if (strcmp(mqttbuffer["effect"], "auto_brightness") == 0)
			{
				clockdisplays[config.activeclockdisplay].autobrightness = 1;
			}
			else if (strcmp(mqttbuffer["effect"], "manual_brightness") == 0)
			{
				clockdisplays[config.activeclockdisplay].autobrightness = 0;
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
		if ((String(config.hostname) + String(MQTT_BACKGROUND_COMMAND_TOPIC)).equals(p_topic))
		{
			if (mqttbuffer.containsKey("effect"))
			{
				if (strcmp(mqttbuffer["effect"], "rgb") == 0)
					clockdisplays[config.activeclockdisplay].backgroud_effect = 0;
				else if (strcmp(mqttbuffer["effect"], "rainbow") == 0)
					clockdisplays[config.activeclockdisplay].backgroud_effect = 1;
				else if (strcmp(mqttbuffer["effect"], "bpm") == 0)
					clockdisplays[config.activeclockdisplay].backgroud_effect = 2;
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