
#ifndef STRUCTS_H
#define STRUCTS_H

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
    byte showseconds;
    byte showminutes;
    byte showhours;
    byte showhourmarks;
    byte showbackground;
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



#endif