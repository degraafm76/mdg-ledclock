#ifndef HELPERFUNCTIONS_H
#define HELPERFUNCTIONS_H

#include <Arduino.h>
#include <ESP8266WiFi.h>

//Default HEX conversion strips leading zero's which we like to keep for use with the js colorpicker.
String inttoHex(int, int);

int ltor(long);
int ltog(long);
int ltob(long);

long hstol(String);

String encryptionTypeStr(uint8_t);

int rotate(int);


#endif

