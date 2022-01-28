#include <effects.h>

uint8_t gHue = 0; // rotating "base color" used by many of the patterns

void bpm()
{
	// colored stripes pulsing at a defined Beats-Per-Minute (BPM)
	uint8_t BeatsPerMinute = 60;
	CRGBPalette16 palette = PartyColors_p;
	uint8_t beat = beatsin8(BeatsPerMinute, 60, 255);
	for (int i = 0; i < NUM_LEDS; i++)
	{
		leds[i] = ColorFromPalette(palette, gHue + (i * 2), beat - gHue + (i * 10));
	}
}

void rainbow()
{
	// FastLED's built-in rainbow generator
	//fill_rainbow(leds, NUM_LEDS, gHue, 7);
	fill_rainbow(leds, NUM_LEDS, gHue, 255 / NUM_LEDS);
}