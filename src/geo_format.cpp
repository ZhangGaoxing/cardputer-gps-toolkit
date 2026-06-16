#include "geo_format.h"

#include <math.h>
#include <stdio.h>

bool geoIsValidCoordinate(float lat, float lon) {
  return isfinite(lat) && isfinite(lon) &&
         lat >= -90.0f && lat <= 90.0f &&
         lon >= -180.0f && lon <= 180.0f;
}

void geoFormatCoordinateDecimal(char* out, size_t outSize, float value, bool isLatitude) {
  if (!out || outSize == 0) return;
  float minValue = isLatitude ? -90.0f : -180.0f;
  float maxValue = isLatitude ? 90.0f : 180.0f;
  if (!isfinite(value) || value < minValue || value > maxValue) {
    snprintf(out, outSize, "--");
    return;
  }
  snprintf(out, outSize, "%.6f", value);
}

void geoFormatCoordinateDms(char* out, size_t outSize, float value, bool isLatitude) {
  if (!out || outSize == 0) return;

  float minValue = isLatitude ? -90.0f : -180.0f;
  float maxValue = isLatitude ? 90.0f : 180.0f;
  if (!isfinite(value) || value < minValue || value > maxValue) {
    snprintf(out, outSize, "--");
    return;
  }

  char hemi;
  if (isLatitude) hemi = value >= 0.0f ? 'N' : 'S';
  else hemi = value >= 0.0f ? 'E' : 'W';

  float absValue = fabsf(value);
  int deg = (int)absValue;
  float minutesFloat = (absValue - deg) * 60.0f;
  int minutes = (int)minutesFloat;
  int seconds = (int)roundf((minutesFloat - minutes) * 60.0f);

  if (seconds >= 60) {
    seconds = 0;
    minutes++;
  }
  if (minutes >= 60) {
    minutes = 0;
    deg++;
  }

  snprintf(out, outSize, "%c%d%c%02d'%02d\"", hemi, deg, 'd', minutes, seconds);
}