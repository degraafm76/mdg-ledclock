#include <Arduino.h>
#include <ESP8266WiFi.h>

//Default HEX conversion strips leading zero's which we like to keep for use with the js colorpicker.
String inttoHex(int num, int precision)
{
	char tmp[16];
	char format[128];

	sprintf(format, "#%%.%dX", precision);

	sprintf(tmp, format, num);
	String hexstring = String(tmp);
	return hexstring;
}

// Convert HEX string to Long
long hstol(String recv)
{
	char c[recv.length() + 1];
	recv.toCharArray(c, recv.length() + 1);
	return strtol(c, NULL, 16);
}


String encryptionTypeStr(uint8_t authmode) {
    switch(authmode) {
        case ENC_TYPE_NONE:
            return "NONE";
        case ENC_TYPE_WEP:
            return "WEP";
        case ENC_TYPE_TKIP:
            return "TKIP";
        case ENC_TYPE_CCMP:
            return "CCMP";
        case ENC_TYPE_AUTO:
            return "AUTO";
        default:
            return "?";
    }
}