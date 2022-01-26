#ifndef SERIAL_H
#define SERIAL_H

#include <Arduino.h>
#include <LittleFS.h>
#include <structs.h>
#include <filesystem.h>
#include <config.h>
#include <global_vars.h>


//Serial
extern String rxString;
#define LINE_BUF_SIZE 128 //Maximum serial input string length
#define ARG_BUF_SIZE 128  //Maximum argument input string length
#define MAX_NUM_ARGS 8    //Maximum arguments
extern char line[LINE_BUF_SIZE];
extern char args[MAX_NUM_ARGS][ARG_BUF_SIZE];
extern boolean serial_input_error_flag;
extern boolean reset_flag;

void printInfo();

//Function declarations
int cmd_mqtt();
int cmd_wifi();
int cmd_reboot();
int cmd_help();
int cmd_information();
int cmd_reset();
int cmd_timezone();
int cmd_hostname();

extern int (*commands_func[])();


//List of command names
extern const char *commands_str[];
extern int num_commands;

//List of MQTT sub command names
extern const char *mqtt_args[];
extern int num_mqtt_args;

//List of WIFI sub command names
extern const char *wifi_args[];
extern int num_wifi_args;

int cmd_reset();
int cmd_information();
int cmd_reboot();
int cmd_hostname();
int cmd_timezone();
int cmd_mqtt();
int cmd_wifi();


void help_reset();
void help_information();
void help_help();
void help_reboot();
void help_timezone();
void help_hostname();
void help_mqtt();
void help_wifi();
int cmd_help();

int execute();

void handleSerial();




#endif