#include <Arduino.h>

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

