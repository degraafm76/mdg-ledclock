#include <http_server.h>

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

void handle_webpages()
{
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

        saveConfiguration(JSON_CONFIG_FILE);
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
        saveClockDisplays(JSON_CLOCK_DISPLAYS_FILE);
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
                  saveSchedules(JSON_SCHEDULES_FILE);

                  request->send(200, "text/plain", "OK");
              });
    server.on("/reboot", HTTP_GET, [](AsyncWebServerRequest *request) { //reboot ESP
        request->send(200, "text/plain", "OK");
        cmd_reboot();
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
}
