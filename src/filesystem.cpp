#include <filesystem.h>

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
		obj["s"] = clockdisplays[i].showseconds;
		obj["m"] = clockdisplays[i].showminutes;
		obj["h"] = clockdisplays[i].showhours;
		obj["hm"] = clockdisplays[i].showhourmarks;
		obj["bg"] = clockdisplays[i].showbackground;
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