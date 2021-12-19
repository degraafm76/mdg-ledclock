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

int ltor(long color)
{ // long to Red
    int r = color >> 16;
    return r;
}
int ltog(long color)
{ // long to Green
    int g = color >> 8 & 0xFF;
    return g;
}
int ltob(long color)
{ // long to Blue
    int b = color & 0xFF;
    return b;
}

// Convert HEX string to Long
long hstol(String recv)
{
    char c[recv.length() + 1];
    recv.toCharArray(c, recv.length() + 1);
    return strtol(c, NULL, 16);
}

String encryptionTypeStr(uint8_t authmode)
{
    switch (authmode)
    {
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