#include "ui_helpers.h"
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
  int idx = (int)((heading+22.5)/45.0) % 8;
  return dirs[idx];
}

float bearingTo(float lat1,float lon1,float lat2,float lon2) {
  float dLon=(lon2-lon1)*DEG_TO_RAD;
  float la1=lat1*DEG_TO_RAD, la2=lat2*DEG_TO_RAD;
  float y=sin(dLon)*cos(la2);
  float x=cos(la1)*sin(la2)-sin(la1)*cos(la2)*cos(dLon);
  return fmod(atan2(y,x)*RAD_TO_DEG+360.0,360.0);
}

float haversineKm(float lat1,float lon1,float lat2,float lon2) {
  float dLat=(lat2-lat1)*DEG_TO_RAD,dLon=(lon2-lon1)*DEG_TO_RAD;
  float a=sin(dLat/2)*sin(dLat/2)+cos(lat1*DEG_TO_RAD)*cos(lat2*DEG_TO_RAD)*sin(dLon/2)*sin(dLon/2);
  return 6371.0*2*atan2(sqrt(a),sqrt(1-a));
}

const char* dopQuality(float dop) {
  if(dop<2.0) return "Ideal";
  if(dop<5.0) return "Good";
  if(dop<10.0) return "Fair";
  return "Poor";
}
