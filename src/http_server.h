#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <Arduino.h>
#include <helperfunctions.h>
#include <ArduinoJson.h>
#include <filesystem.h>
#include <mqtt.h>
#include <serial.h>
#include <global_vars.h>
#include <config.h>
#include <web/mdg_ledr_js.h>
#include <web/mdg_ledr_css.h>
#include <web/index_html.h>
#include <web/fonts.h>

String processor(const String &var);

void handle_webpages();

#endif