#ifndef CONFIG_H
#define CONFIG_H

#define SOFTWARE_VERSION "1.2.0"            // Software version
#define CLOCK_MODEL "MDG Ledclock model 1"  // Clock Model
#define NUM_LEDS 60                         // How many leds are in the clock?
#define ROTATE_LEDS 30                      // Rotate leds by this number. Led ring is connected upside down
#define MAX_BRIGHTNESS 255                  // Max brightness
#define MIN_BRIGHTNESS 8                    // Min brightness (at very low brightness levels interpolating doesn't work well and the led's will flicker and not display the correct color)
#define DATA_PIN 4                          // Neopixel data pin (use pin 4 for WLED compatibility)
#define NEOPIXEL_VOLTAGE 5                  // Neopixel voltage
#define NEOPIXEL_MILLIAMPS 1000             // Neopixel maximum current usage in milliamps, if you have a powersource higher then 1A you can change this value (at your own risk!) to have brighter leds.
#define LIGHTSENSORPIN A0                   // Ambient light sensor pin
#define EXE_INTERVAL_AUTO_BRIGHTNESS 500    // Interval (ms) to check light sensor value
#define EXE_INTERVAL_LIGHTSENSOR_MQTT 60000 // Interval (ms) to send light sensor value to MQTT topic
#define CLOCK_DISPLAYS 8                    // Nr of user defined clock displays
#define SCHEDULES 12                        // Nr of user defined schedules
#define AP_NAME "MDG-Ledclock1"             // Name of the AP when WIFI is not setup or connected
//#define MQTT_DEBUG						// MQTT debug enabled
#define COMPILE_DATE __DATE__ " " __TIME__ // Compile date/time

#endif