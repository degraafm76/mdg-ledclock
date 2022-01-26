#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <LittleFS.h>
#include <structs.h>
#include <ArduinoJson.h>
#include <global_vars.h>


void saveConfiguration(const char*);
void saveSchedules(const char*);
void saveClockDisplays(const char*);


#endif