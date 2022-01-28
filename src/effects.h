#ifndef EFFECTS_H
#define EFFECTS_H

#include <FastLED.h>
#include <global_vars.h>

extern uint8_t gHue; // rotating "base color" used by many of the patterns

void bpm();
void rainbow();

#endif