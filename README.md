# MDG Ledclock model 1
![MDG Ledclock](https://github.com/degraafm76/mdg-ledclock/blob/main/media/ledclock0.jpg)
![MDG Ledclock](https://github.com/degraafm76/mdg-ledclock/blob/main/media/ledclock1.jpg)
![MDG Ledclock](https://github.com/degraafm76/mdg-ledclock/blob/main/media/ledclock2.jpg)
![MDG Ledclock](https://github.com/degraafm76/mdg-ledclock/blob/main/media/ledclock3.jpg)

An ESP8266 based LED clock which can be controlled by HTTP and MQTT.
Tested with Home Assistant but other domotica systems with MQTT support should work fine.

# Ok cool, I want one!
What are your options:

1. Buy a ready to use MDG ledclock [Etsy: MDG Led clock model 1](https://www.etsy.com/nl/listing/1155302644/mdg-led-clock-model-1-3d-printed-parts?ref=listings_manager_grid).
2. Source your own electronics and buy MDG ledclock 3D printed parts see [Etsy: MDG Led clock (model 1) - 3D printed parts](https://www.etsy.com/your/shops/MDGdesignNL/tools/listings/1155302644)
3. Source your own electronics, 3D print the parts and build it yourself.

If you like my work please consider donating to support my work.

[![Donate](https://www.paypalobjects.com/en_US/NL/i/btn/btn_donateCC_LG.gif)]( https://www.paypal.com/donate/?hosted_button_id=ZDERFEHERXURW)


# Electronic parts
* 1 NodeMCU ESP8266 V2
* 1 TEMT6000 Ambient light sensor
* 1 1000uF Capacitor 24V
* 1 330R 0.5W Resistor 
* 1 SK6812 LED ring 60 (https://www.tinytronics.nl/shop/nl/verlichting/ringen-en-modules/sk6812-digitale-5050-rgb-led-ring-60-leds-wit) or (https://nl.aliexpress.com/item/1005002090813354.html?gatewayAdapt=glo2nld&spm=a2g0o.9042311.0.0.27424c4dx34xlJ)

![Schematic](https://github.com/degraafm76/mdg-ledclock/blob/main/media/Led%20clock%20Schematic.png)

# Printing parts
The parts are designed with PLA in mind. Nozzle width must be 0.4mm and layer hight 0.2mm. For the front ring and case supports on build plate are needed.

The front ring must be printed in muliple colors. The first 3 layers (0.6mm) white and the rest of the layers black. If you don't know how to print multicolor objects with your printer I suggest you take a look at: https://www.youtube.com/watch?v=1nBnVtOEAiY

The [models](https://github.com/degraafm76/mdg-ledclock/tree/main/STL) of this clock are licenced [Attribution-NonCommercial-ShareAlike 4.0 International (CC BY-NC-SA 4.0)](https://creativecommons.org/licenses/by-nc-sa/4.0/legalcode) This license lets others remix, adapt, and build upon my work non-commercially, as long as you credit me and license your new creations under the identical terms.


# Setup the clock
See https://github.com/degraafm76/mdg-ledclock/blob/main/DOC/MDG%20-%20Ledclock%20(model%201).pdf

# Serial configuration
All configuration settings (wifi/mqtt/timezone) can be set with a serial connection. Use 115200 baud

![Serial Connection](https://github.com/degraafm76/mdg-ledclock/blob/main/media/Serial_connection.png)

# Update software
There are 2 options to update the clock.

1. Install Visual Studio code with platform.io and compile/upload the software to the clock. You can find the latest release here https://github.com/degraafm76/mdg-ledclock/releases
2. Download the bin file from the latest release and flash the file to the clock with ESP home flasher https://github.com/esphome/esphome-flasher/releases
