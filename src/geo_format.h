#ifndef GEO_FORMAT_H
#define GEO_FORMAT_H

#include <Arduino.h>

bool geoIsValidCoordinate(float lat, float lon);
void geoFormatCoordinateDecimal(char* out, size_t outSize, float value, bool isLatitude);
void geoFormatCoordinateDms(char* out, size_t outSize, float value, bool isLatitude);

#endif // GEO_FORMAT_H