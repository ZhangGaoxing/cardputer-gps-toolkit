#include "ui_helpers.h"
#include "geo_math.h"
#include <math.h>

uint16_t systemColor(const String& sys) {
  if (sys == "GPS") return TFT_GREEN;
  if (sys == "GLONASS") return TFT_RED;
  if (sys == "Galileo") return TFT_CYAN;
  if (sys == "BeiDou") return TFT_YELLOW;
  if (sys == "QZSS") return TFT_MAGENTA;
  return TFT_WHITE;
}

const char* cardinalFromHeading(float heading) {
  const char* dirs[] = {"N","NE","E","SE","S","SW","W","NW"};
  int idx = (int)((heading+22.5f)/45.0f) % 8;
  return dirs[idx];
}

float bearingTo(float lat1,float lon1,float lat2,float lon2) {
  return geoBearingDeg(lat1, lon1, lat2, lon2);
}

float haversineKm(float lat1,float lon1,float lat2,float lon2) {
  return geoDistanceKm(lat1, lon1, lat2, lon2);
}

const char* dopQuality(float dop) {
  if(dop<2.0) return "Ideal";
  if(dop<5.0) return "Good";
  if(dop<10.0) return "Fair";
  return "Poor";
}
