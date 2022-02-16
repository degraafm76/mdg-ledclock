#include <serial.h>

//Serial
String rxString = "";
#define LINE_BUF_SIZE 128 //Maximum serial input string length
#define ARG_BUF_SIZE 128  //Maximum argument input string length
#define MAX_NUM_ARGS 8	  //Maximum arguments
char line[LINE_BUF_SIZE];
char args[MAX_NUM_ARGS][ARG_BUF_SIZE];
boolean serial_input_error_flag = false;
boolean reset_flag = false;

void printInfo()
{
	Serial.println();
	Serial.println("----------------- [Information] ------------------");
	Serial.print("Clock Model: ");
	Serial.println(CLOCK_MODEL);
	Serial.print("Software version: ");
	Serial.println(SOFTWARE_VERSION);
	Serial.print("MAC address: ");
	Serial.println(WiFi.macAddress());
	Serial.println("AP Password: " + apPassword);
	Serial.print("Hostname: ");
	Serial.println(config.hostname);
	Serial.println("SSID: " + WiFi.SSID());
	Serial.print("Rssi: ");
	Serial.println(WiFi.RSSI());
	Serial.print("Channel: ");
	Serial.println(channel);
	Serial.println("IP Address: " + WiFi.localIP().toString());
	Serial.print("MQTT Server: ");
	Serial.println(config.mqttserver);
	Serial.print("MQTT Port: ");
	Serial.println(config.mqttport);
	Serial.print("MQTT Connected: ");
	Serial.println(MQTTConnected);
	Serial.print("MQTT TLS: ");
	Serial.println(config.mqtttls);
	Serial.print("Time zone: ");
	Serial.println(config.tz);
	Serial.print("ESP Free heap: ");
	Serial.println(ESP.getFreeHeap());
	Serial.print("ESP Chip id: ");
	Serial.println(ESP.getChipId());
	Serial.println("---------------------------------------------------");
}

//Function declarations
int cmd_mqtt();
int cmd_wifi();
int cmd_reboot();
int cmd_help();
int cmd_information();
int cmd_reset();
int cmd_timezone();
int cmd_hostname();

int (*commands_func[])(){
	//&cmd_help,
	&cmd_mqtt,
	&cmd_wifi,
	&cmd_reboot,
	&cmd_help,
	&cmd_information,
	&cmd_reset,
	&cmd_timezone,
	&cmd_hostname};

//List of command names
const char *commands_str[] = {
	"mqtt",
	"wifi",
	"reboot",
	"help",
	"information",
	"reset",
	"timezone",
	"hostname"};

int num_commands = sizeof(commands_str) / sizeof(char *);

//List of MQTT sub command names
const char *mqtt_args[] = {
	"server",
	"port",
	"tls",
	"user",
	"password",
	"settings"};

int num_mqtt_args = sizeof(mqtt_args) / sizeof(char *);

//List of WIFI sub command names
const char *wifi_args[] = {
	"ssid",
	"password",
	"settings"};
int num_wifi_args = sizeof(wifi_args) / sizeof(char *);

int cmd_reset()
{
	if (reset_flag)
	{
		Serial.println("Reset clock to default");
		LittleFS.remove(JSON_CONFIG_FILE);
		LittleFS.remove(JSON_SCHEDULES_FILE);
		LittleFS.remove(JSON_CLOCK_DISPLAYS_FILE);
		WiFi.disconnect(true); //clear wifi settings
		cmd_reboot();
	}
	else
	{
		Serial.println("Please type \"reset\" again if you want to reset the clock to default settings.");
		reset_flag = true;
	}
	return 1;
}

int cmd_information()
{
	printInfo();
	return 1;
}

int cmd_reboot()
{
	delay(500);
	ESP.restart();
	return 1;
}

int cmd_hostname()
{
	Serial.print("Hostname: ");
	if (strlen(args[1]) == 0)
	{ // show current value

		Serial.println(config.hostname);
	}
	else
	{ // set value
		strcpy(config.hostname, args[1]);
		Serial.println(config.hostname);
		saveConfiguration(JSON_CONFIG_FILE);
	}
	return 1;
}

int cmd_timezone()
{
	Serial.print("Time zone: ");
	if (strlen(args[1]) == 0)
	{ // show current value

		Serial.println(config.tz);
	}
	else
	{ // set value
		strcpy(config.tz, args[1]);
		Serial.println(config.tz);
		tz.setLocation(config.tz);
		saveConfiguration(JSON_CONFIG_FILE);
	}
	return 1;
}

int cmd_mqtt()
{
	if (strcmp(args[1], mqtt_args[0]) == 0)
	{
		Serial.print("Mqtt server: ");
		if (strlen(args[2]) == 0)
		{ // show current value

			Serial.print(config.mqttserver);
		}
		else
		{ // set value
			strcpy(config.mqttserver, args[2]);
			Serial.print(config.mqttserver);
			saveConfiguration(JSON_CONFIG_FILE);
		}
	}
	else if (strcmp(args[1], mqtt_args[1]) == 0)
	{
		Serial.print("Mqtt port: ");
		if (strlen(args[2]) == 0)
		{ // show current value
			Serial.print(config.mqttport);
		}
		else
		{						   // set value
			if (atoi(args[2]) > 0) //check if port number is numeric
			{

				config.mqttport = atoi(args[2]);
				Serial.print(config.mqttport);
				saveConfiguration(JSON_CONFIG_FILE);
			}
			else
			{
				Serial.print("Invalid port number");
			}
		}
	}
	else if (strcmp(args[1], mqtt_args[2]) == 0)
	{
		Serial.print("Mqtt tls: ");
		if (strlen(args[2]) == 0)
		{ // show current value
			Serial.print(config.mqtttls);
		}
		else
		{ // set value
			//check if input is integer and 0 or 1
			if (atoi(args[2]) == 0 || atoi(args[2]) == 1)
			{
				config.mqtttls = atoi(args[2]);
				Serial.print(config.mqtttls);
				//saveConfiguration(JSON_CONFIG_FILE);
			}
			else
			{
				Serial.print("Only values \"1\" for enabled and \"0\" for disabled are allowed");
			}
		}
	}
	else if (strcmp(args[1], mqtt_args[3]) == 0)
	{
		Serial.print("Mqtt user: ");
		if (strlen(args[2]) == 0)
		{ // show current value
			Serial.print(config.mqttuser);
		}
		else
		{ // set value
			strcpy(config.mqttuser, args[2]);
			Serial.print(config.mqttuser);
			saveConfiguration(JSON_CONFIG_FILE);
		}
	}
	else if (strcmp(args[1], mqtt_args[4]) == 0)
	{
		Serial.print("Mqtt password: ");
		if (strlen(args[2]) == 0)
		{ // show current value
			Serial.print("********");
		}
		else
		{ // set value
			strcpy(config.mqttpassword, args[2]);
			Serial.print(config.mqttpassword);
			saveConfiguration(JSON_CONFIG_FILE);
		}
	}
	else if (strcmp(args[1], mqtt_args[5]) == 0)
	{
		Serial.println("-------- Mqtt settings --------");
		Serial.print("Server: ");
		Serial.println(config.mqttserver);
		Serial.print("Port: ");
		Serial.println(config.mqttport);
		Serial.print("TLS: ");
		Serial.println(config.mqtttls);
		Serial.print("Username: ");
		Serial.println(config.mqttuser);
		Serial.print("Password: ");
		Serial.println("********");
		Serial.print("-------------------------------");
	}
	else
	{
		Serial.println("Invalid command. Type \"help mqtt\" to see how to use the mqtt command.");
		Serial.println();

		return 0;
	}
	Serial.println();

	return 1;
}

int cmd_wifi()
{
	if (strcmp(args[1], wifi_args[0]) == 0)
	{
		Serial.print("WiFi ssid: ");
		if (strlen(args[2]) == 0)
		{ // show current value

			Serial.print(config.ssid);
		}
		else
		{ // set value

			strcpy(config.ssid, args[2]);

			for (int i = 3; i < MAX_NUM_ARGS; i++)
			{
				if (strlen(args[i]) != 0) //when there are spaces in ssid
				{
					strcat(config.ssid, " ");
					strcat(config.ssid, args[i]);
				}
			}

			Serial.print(config.ssid);
			saveConfiguration(JSON_CONFIG_FILE);
		}
	}
	else if (strcmp(args[1], wifi_args[1]) == 0)
	{
		Serial.print("WiFi password: ");
		if (strlen(args[2]) == 0)
		{ // show current value

			Serial.print(config.wifipassword);
		}
		else
		{ // set value		
			
			strcpy(config.wifipassword, args[2]);
			for (int i = 3; i < MAX_NUM_ARGS; i++)
			{
				if (strlen(args[i]) != 0) //when there are spaces in ssid
				{
					strcat(config.wifipassword, " ");
					strcat(config.wifipassword, args[i]);
				}
			}


			
			Serial.print("********");
			saveConfiguration(JSON_CONFIG_FILE);
		}
	}
	else if (strcmp(args[1], wifi_args[2]) == 0)
	{
		Serial.println("-------- WiFi settings --------");
		Serial.print("SSID: ");
		Serial.println(config.ssid);
		Serial.print("Password: ");
		Serial.println("********");
		Serial.print("-------------------------------");
	}
	else
	{
		Serial.println("Invalid command. Type \"help mqtt\" to see how to use the mqtt command.");
		Serial.println();

		return 0;
	}
	Serial.println();

	return 1;
}

void help_reset()
{
	Serial.println("Reset clock to default settings");
}
void help_information()
{
	Serial.println("Display clock settings and information");
}

void help_help()
{
	Serial.println("The following commands are available:");

	for (int i = 0; i < num_commands; i++)
	{
		Serial.print("  ");
		Serial.println(commands_str[i]);
	}
	Serial.println("");
	Serial.println("You can for example type \"help mqtt\" for more info on the mqtt command.");
}

void help_reboot()
{
	Serial.println("This will restart the program.");
}

void help_timezone()
{
	Serial.println("Set the timezone, please use \"Olson\" format like Europe/Amsterdam. You can find a list at: https://en.wikipedia.org/wiki/List_of_tz_database_time_zones");
	Serial.println("For example: \"timezone Europe/Amsterdam\". If you type \"timezone\" the current timezone is shown.");
}

void help_hostname()
{
	Serial.println("Set the hostname. The clock is reachable for clients that support MDNS at [hostname].local");
}

void help_mqtt()
{
	Serial.println("The following mqtt arguments are available:");

	for (int i = 0; i < num_mqtt_args; i++)
	{
		Serial.print("  ");
		Serial.println(mqtt_args[i]);
	}

	Serial.println();
	Serial.println("You can for example type \"mqtt server 192.168.1.100\" to set the mqtt server.");
	Serial.print("If you leave the third argument empty for example \"mqtt server\" you get the current value, ");
	Serial.println("\"mqtt settings\" shows all mqtt settings");
}

void help_wifi()
{
	Serial.println("The following WiFi arguments are available:");

	for (int i = 0; i < num_wifi_args; i++)
	{
		Serial.print("  ");
		Serial.println(wifi_args[i]);
	}

	Serial.println();
	Serial.println("You can for example type \"wifi ssid yourwifissid\" to set the WiFi ssid.");
	Serial.print("If you leave the third argument empty for example \"wifi ssid\" you get the current value, ");
	Serial.println("\"wifi settings\" shows all wifi settings");
}

int cmd_help()
{
	if (args[1] == NULL)
	{
		help_help();
	}
	else if (strcmp(args[1], commands_str[0]) == 0)
	{
		help_mqtt();
	}
	else if (strcmp(args[1], commands_str[1]) == 0)
	{
		help_wifi();
	}
	else if (strcmp(args[1], commands_str[2]) == 0)
	{
		help_reboot();
	}
	else if (strcmp(args[1], commands_str[4]) == 0)
	{
		help_information();
	}
	else if (strcmp(args[1], commands_str[5]) == 0)
	{
		help_reset();
	}
	else if (strcmp(args[1], commands_str[6]) == 0)
	{
		help_timezone();
	}
	else if (strcmp(args[1], commands_str[7]) == 0)
	{
		help_hostname();
	}
	else
	{
		help_help();
	}
	return 1;
}

int execute()
{
	for (int i = 0; i < num_commands; i++)
	{
		if (strcmp(args[0], commands_str[i]) == 0)
		{
			return (*commands_func[i])();
		}
	}

	if (strcmp(args[0], "") == 0)
	{ //user just pressed enter
		//do nothing
		return 1;
	}
	else
	{
		Serial.println("Invalid command. Type \"help\" for more.");
		return 0;
	}
	return 1;
}

void handleSerial()
{
	while (Serial.available())
	{

		// get the new byte:
		char inChar = (char)Serial.read();
		// add it to the rxString:
		rxString += inChar;
		// if the incoming character is a line feed, command is complete
		if (inChar == '\r')
		{

			Serial.println();
			rxString.trim(); //remove CRLF from received string

			char *argument;
			int counter = 0;
			rxString.toCharArray(line, LINE_BUF_SIZE);

			argument = strtok(line, " ");

			while ((argument != NULL))
			{
				if (counter < MAX_NUM_ARGS)
				{
					if (strlen(argument) < ARG_BUF_SIZE)
					{
						strcpy(args[counter], argument);
						argument = strtok(NULL, " ");
						counter++;
					}
					else
					{
						Serial.println("Error: Input string is too long.");
						serial_input_error_flag = true; // set error flag to true
						break;
					}
				}
				else
				{
					Serial.println("Error: To much arguments");
					serial_input_error_flag = true; // set error flag to true
					break;
				}
			}

			if (!serial_input_error_flag) //only proceed when error flag is false
			{
				execute();
			}
			else
			{
				serial_input_error_flag = false; //reset error flag
			}

			rxString = "";

			//reset the line string and the args list to zero.
			memset(line, 0, LINE_BUF_SIZE);
			memset(args, 0, sizeof(args[0][0]) * MAX_NUM_ARGS * ARG_BUF_SIZE);

			Serial.print(">"); //command prompt
		}
	}
}
