/*  MDG Ledclock
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
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <FastLED.h>
#include <PubSubClient.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <mqtt.h>
#include <http_server.h>
#include <filesystem.h>
#include <config.h>
#include <structs.h>
#include <global_vars.h>
#include <serial.h>
#include <effects.h>

void scanWifi(String ssid)
{

	byte numSsid = WiFi.scanNetworks();
	rssi = -999;
	for (int thisNet = 0; thisNet < numSsid; thisNet++)
	{
		if (WiFi.SSID(thisNet) == ssid)
		{		
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
	Serial.print("Info: Waiting for Wi-Fi connection");
	while (count < 30)
	{
		leds[rotate(count)] = CRGB::White;
		FastLED.show();

		if (WiFi.status() == WL_CONNECTED)
		{
			Serial.println();
			Serial.println("Info: Connected!");
			leds[rotate(count)] = CRGB::Green;
			FastLED.show();
			return (true);
		}
		delay(500);
		Serial.print(".");
		count++;
	}
	Serial.println("Info: Timed out.");
	leds[rotate(count)] = CRGB::Red;
	FastLED.show();
	delay(5000);
	return false;
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
	File configfile = LittleFS.open(JSON_CONFIG_FILE, "r");
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
	File schedulefile = LittleFS.open(JSON_SCHEDULES_FILE, "r");
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
	File clkdisplaysfile = LittleFS.open(JSON_CLOCK_DISPLAYS_FILE, "r");

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
		clockdisplays[i].showseconds = clkdisplays_doc[i]["s"] | 1;
		clockdisplays[i].showminutes = clkdisplays_doc[i]["m"] | 1;
		clockdisplays[i].showhours = clkdisplays_doc[i]["h"] | 1;
		clockdisplays[i].showhourmarks = clkdisplays_doc[i]["hm"] | 1;
		clockdisplays[i].showbackground = clkdisplays_doc[i]["bg"] | 1;
		clockdisplays[i].autobrightness = clkdisplays_doc[i]["ab"] | 1;
		clockdisplays[i].brightness = clkdisplays_doc[i]["bn"] | 128;
		clockdisplays[i].backgroud_effect = clkdisplays_doc[i]["be"] | 0;
	}

	config.activeclockdisplay = config_doc["acd"] | 0;

	if (clockdisplays[config.activeclockdisplay].showhours == 0)
	{
		array_state[0] = false; //set hours off
	}
	if (clockdisplays[config.activeclockdisplay].showminutes == 0)
	{
		array_state[1] = false; //set minutes off
	}
	if (clockdisplays[config.activeclockdisplay].showseconds == 0)
	{
		array_state[2] = false; //set seconds off
	}
	if (clockdisplays[config.activeclockdisplay].showhourmarks == 0)
	{
		array_state[3] = false; //set hourmarks off
	}
	if (clockdisplays[config.activeclockdisplay].showbackground == 0)
	{
		array_state[4] = false; //set background off
	}

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
	//  .setCorrection(TypicalLEDStrip);
		.setCorrection(0xFFFFFF); //No correction
	FastLED.setMaxRefreshRate(0);

	//MQTT
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

	if (rssi != -999) //connect to strongest ap if multiple ap's with same ssid are found
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
	}
	else
	{
		wifiConnected = false;
	}

	byte mac[6];

	WiFi.macAddress(mac);

	apPassword = String(mac[5], HEX) + String(mac[4], HEX) + String(mac[3], HEX) + String(mac[2] + mac[5], HEX);

	if (wifiConnected == false)
	{

		WiFi.mode(WIFI_AP); //Accespoint mode
		WiFi.softAP(AP_NAME, apPassword);
	}

	// Provide official timezone names
	// https://en.wikipedia.org/wiki/List_of_tz_database_time_zones
	if (!tz.setCache(0))
		tz.setLocation(config.tz);

	// Route for root / web pages
	handle_webpages();

	if (config.mqtttls == 1 && MQTTConnected)
	{
		//do not start webserver when MQTT is connected with TLS because of low memory and crashes
		Serial.println("Info: Webserver is disabled because MQTT TLS is enabled");
	}
	else
	{
		// Start web server
		server.begin();
	}

	//Multicast DNS
	if (!MDNS.begin(config.hostname))
	{ // Start the mDNS responder for esp8266.local
		Serial.println("Error setting up MDNS responder!");
	}

	MDNS.addService("http", "tcp", 80);

	//printInfo();
	Serial.println("Type help for information");
	Serial.print(">");
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

		for (int i = 0; i <= SCHEDULES - 1; i++)
		{
			if (tz.hour() == schedules[i].hour && tz.minute() == schedules[i].minute && schedules[i].active == 1)
			{
				config.activeclockdisplay = schedules[i].activeclockdisplay;
			}
		}
	}

	if (array_state[4] && display_state) //Background on
	{
		clockdisplays[config.activeclockdisplay].showbackground = 1;
	}
	else //Background off
	{
		clockdisplays[config.activeclockdisplay].showbackground = 0;
	}

	if (clockdisplays[config.activeclockdisplay].showbackground == 1) //if background is on
	{

		if (clockdisplays[config.activeclockdisplay].backgroud_effect > 0) //show effect
		{
			EVERY_N_MILLISECONDS(20) { gHue++; }

			switch (clockdisplays[config.activeclockdisplay].backgroud_effect)
			{
			case 1:
				rainbow();
				break;
			case 2:
				bpm();
				break;
			default:
				break;
			}

			uint8_t brightness = map(array_brightness[4], 0, 100, MAX_BRIGHTNESS, MIN_BRIGHTNESS);
			for (int Led = 0; Led < NUM_LEDS; Led = Led + 1)
			{
				leds[rotate(Led)].fadeLightBy(brightness);
			}
		}
		else //Set background color
		{
			
			for (int Led = 0; Led < NUM_LEDS; Led = Led + 1)
			{
				leds[rotate(Led)] = clockdisplays[config.activeclockdisplay].backgroundColor;
			}
		}
	}
	else //if background is off
	{
		for (int Led = 0; Led < 60; Led = Led + 1)
		{
			leds[rotate(Led)] = 0x000000;
		}
	}

	if (array_state[3] & display_state)
	{
		clockdisplays[config.activeclockdisplay].showhourmarks = 1;
	}
	else
	{
		clockdisplays[config.activeclockdisplay].showhourmarks = 0;
	}

	if (clockdisplays[config.activeclockdisplay].showhourmarks == 1) // Hour marks
	{
		for (int Led = 0; Led <= 55; Led = Led + 5)
		{
			leds[rotate(Led)] = clockdisplays[config.activeclockdisplay].hourMarkColor;
		}
	}

	if (array_state[0] & display_state)
	{
		clockdisplays[config.activeclockdisplay].showhours = 1;
	}
	else
	{
		clockdisplays[config.activeclockdisplay].showhours = 0;
	}

	if (clockdisplays[config.activeclockdisplay].showhours == 1) //Hour hand
	{	
		int houroffset = map(tz.minute(), 0, 60, 0, 5); //move the hour hand when the minutes pass

		uint8_t hrs12 = (tz.hour() % 12);
		leds[rotate((hrs12 * 5) + houroffset)] = clockdisplays[config.activeclockdisplay].hourColor;
	}

	if (array_state[1] && display_state) //Minute Hand
	{
		leds[rotate(tz.minute())] = clockdisplays[config.activeclockdisplay].minuteColor;
	}

	if (array_state[2] && display_state) //MQTT seconds on
	{
		clockdisplays[config.activeclockdisplay].showseconds = 1;
		
	}
	else //MQTT seconds off
	{
		clockdisplays[config.activeclockdisplay].showseconds = 0;
	}

	//Second hand
	if (clockdisplays[config.activeclockdisplay].showseconds == 1)
	{
		leds[rotate(tz.second())] = clockdisplays[config.activeclockdisplay].secondColor;
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
			if (readIndex >= NUMREADINGS)
			{
				// ...wrap around to the beginning:
				readIndex = 0;
			}

			// calculate the average:
			average = total / NUMREADINGS;

			lux = average * 0.9765625; // 1000/1024

			int brightnessMap = map(average, 3, 40, MIN_BRIGHTNESS, MAX_BRIGHTNESS);

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
			publishSensorState();
		}
	}

	FastLED.delay(1000 / 400);

	handleSerial();
}