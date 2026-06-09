/**
 * fn_gps_dashboard.cpp — GPS仪表盘（整合 Dashboard + Fix Info + Coords）
 * 显示：GPS定位数据 + 卫星统计 + 天空图 + PDOP + 坐标
 */
#include "fn_gps_dashboard.h"
#include "../display_manager.h"
#include "../gps_manager.h"
#include "../ui_helpers.h"
#include "../imu_manager.h"

static const int CELL_W = 90;

void FnGpsDashboard::onEnter() { _tab = 0; }

void FnGpsDashboard::onUpdate(bool force) {
  M5Canvas& cv = DisplayManager::instance().canvas();
  GPSManager& gps = GPSManager::instance();

  static const char* tabTitle[] = {"DASH","Fix Info","Coords"};
  cv.setTextSize(1);
  cv.setTextColor(TFT_DARKGREY);
  cv.setCursor(4, 1);
  cv.print(tabTitle[_tab]);
  cv.setCursor(SCREEN_W - 40, 1);
  cv.print("[Tab]");

  switch (_tab) {
    case 0: _drawTabMain(); break;
    case 1: _drawTabFix();  break;
    case 2: _drawTabCoord(); break;
  }
}

bool FnGpsDashboard::onKeyEvent(const KeyEvent& event) {
  if (event.pressed && event.key == 0x09) { _tab = (_tab + 1) % 3; return true; }
  return false;
}

void FnGpsDashboard::_drawTabMain() {
  M5Canvas& cv = DisplayManager::instance().canvas();
  GPSManager& gps = GPSManager::instance();

  int startY = 10;
  int availableH = SCREEN_H - startY;
  int cellH = availableH / 8;
  int x = 1, y = startY;
  const char* c1labels[] = {"Lat","Lng","Alt","Spd","Crs","Date","Time","Fix"};
  const char* c2labels[] = {"Vis","Usd","PD","HD","VD","Gp/Gl","Ga/Bd","Qz"};
  char c1values[8][20];
  char c2values[8][12];

  if (gps.hasFix()) {snprintf(c1values[0],20,"%.6f",gps.latitude());} else {snprintf(c1values[0],20,"NoFix");}
  if (gps.hasFix()) {snprintf(c1values[1],20,"%.6f",gps.longitude());} else {snprintf(c1values[1],20,"NoFix");}
  snprintf(c1values[2],20,"%.1fm", gps.altitude());
  snprintf(c1values[3],20,"%.1f", gps.speedKmph());
  snprintf(c1values[4],20,"%.1f", gps.courseDeg());
  if (gps.dateValid()) {snprintf(c1values[5],20,"%02d/%02d/%02d",gps.utcDay(),gps.utcMonth(),gps.utcYear()%100);}
    else {snprintf(c1values[5],20,"NoData");}
  if (gps.timeValid()) {snprintf(c1values[6],20,"%02d:%02d:%02d",gps.utcHour(),gps.utcMinute(),gps.utcSecond());}
    else {snprintf(c1values[6],20,"NoData");}
  {
    const char* fixQ="None";
    if (gps.ggaFixQuality()==1) fixQ="GPS";
    else if (gps.ggaFixQuality()==2) fixQ="DGPS";
    else if (gps.ggaFixQuality()==4) fixQ="RTK";
    else if (gps.ggaFixQuality()==5) fixQ="FRTK";
    const char* fixM="";
    if (gps.gsaFixMode()==2) fixM=" 2D";
    else if (gps.gsaFixMode()==3) fixM=" 3D";
    snprintf(c1values[7],20,"%s%s",fixQ,fixM);
  }

  int totVis=0,totUsed=0,gpV=0,glV=0,gaV=0,bdV=0,qzV=0;
  const auto& sats=gps.satellites();
  for (auto& sat:sats) {
    if (sat.used) totUsed++;
    if (sat.visible) {totVis++; if (sat.system=="GPS") gpV++; else if (sat.system=="GLONASS") glV++; else if (sat.system=="Galileo") gaV++; else if (sat.system=="BeiDou") bdV++; else if (sat.system=="QZSS") qzV++;}
  }
  snprintf(c2values[0],12,"%d/%d",totVis,(int)sats.size());
  snprintf(c2values[1],12,"%d",gps.satellitesUsed());
  snprintf(c2values[2],12,"%.1f",gps.pdop());
  snprintf(c2values[3],12,"%.1f",gps.hdop());
  snprintf(c2values[4],12,"%.1f",gps.vdop());
  snprintf(c2values[5],12,"%d/%d",gpV,glV);
  snprintf(c2values[6],12,"%d/%d",gaV,bdV);
  snprintf(c2values[7],12,"%d",qzV);

  for (int i=0;i<8;i++) {
    cv.fillRect(x,y,CELL_W,cellH,TFT_BLACK);
    cv.drawRect(x,y,CELL_W,cellH,TFT_DARKGREY);
    cv.setTextColor(TFT_WHITE);
    cv.setCursor(x+4,y+(cellH-8)/2+1);
    cv.setTextSize(1);
    cv.printf("%s: %s",c1labels[i],c1values[i]);
    y+=cellH;
  }

  y = startY; x = 91;
  int cellW2 = 63;
  for (int i=0;i<8;i++) {
    cv.fillRect(x,y,cellW2,cellH,TFT_BLACK);
    cv.drawRect(x,y,cellW2,cellH,TFT_DARKGREY);
    cv.setTextColor(TFT_WHITE);
    cv.setCursor(x+2,y+(cellH-8)/2+1);
    cv.setTextSize(1);
    cv.printf("%s:%s",c2labels[i],c2values[i]);
    y+=cellH;
  }

  int skyX = 155, skyY = startY, skyW = SCREEN_W - skyX - 1, skyH = availableH;
  _drawSkyPlot(skyX, skyY, skyW, skyH);
}

void FnGpsDashboard::_drawTabFix() {
  M5Canvas& cv = DisplayManager::instance().canvas();
  GPSManager& gps = GPSManager::instance();
  IMUManager& imu = IMUManager::instance();

  cv.setTextSize(1); int y = CONTENT_TOP + 12; int lh = 14; char buf[40];

  const char* fixQ = "None";
  if (gps.ggaFixQuality()==1) fixQ="GPS"; else if (gps.ggaFixQuality()==2) fixQ="DGPS"; else if (gps.ggaFixQuality()==4) fixQ="RTK"; else if (gps.ggaFixQuality()==5) fixQ="Float RTK";
  cv.setTextColor(TFT_GREEN); cv.setCursor(4,y); snprintf(buf,sizeof(buf),"Fix Quality: %s",fixQ); cv.print(buf); y+=lh;

  const char* fixM="No Fix"; if (gps.gsaFixMode()==2) fixM="2D"; else if (gps.gsaFixMode()==3) fixM="3D";
  cv.setTextColor(TFT_WHITE); cv.setCursor(4,y); snprintf(buf,sizeof(buf),"Fix Mode:    %s",fixM); cv.print(buf); y+=lh;

  cv.setCursor(4,y); snprintf(buf,sizeof(buf),"PDOP: %.1f (%s)",gps.pdop(),dopQuality(gps.pdop())); cv.print(buf); y+=lh;
  cv.setCursor(4,y); snprintf(buf,sizeof(buf),"HDOP: %.1f (%s)",gps.hdop(),dopQuality(gps.hdop())); cv.print(buf); y+=lh;
  cv.setCursor(4,y); snprintf(buf,sizeof(buf),"VDOP: %.1f (%s)",gps.vdop(),dopQuality(gps.vdop())); cv.print(buf); y+=lh;

  const auto& sats=gps.satellites(); int totVis=0,totUsed=0,gpV=0,glV=0,gaV=0,bdV=0,qzV=0;
  for (auto& sat:sats){if(sat.used)totUsed++;if(sat.visible){totVis++;if(sat.system=="GPS")gpV++;else if(sat.system=="GLONASS")glV++;else if(sat.system=="Galileo")gaV++;else if(sat.system=="BeiDou")bdV++;else if(sat.system=="QZSS")qzV++;}}
  cv.setTextColor(TFT_LIGHTGREY); cv.setCursor(4,y); snprintf(buf,sizeof(buf),"Used:%d Vis:%d Tot:%d",totUsed,totVis,(int)sats.size()); cv.print(buf); y+=lh;
  cv.setCursor(4,y); snprintf(buf,sizeof(buf),"GP:%d GL:%d GA:%d BD:%d QZ:%d",gpV,glV,gaV,bdV,qzV); cv.print(buf); y+=lh;
  cv.setCursor(4,y); if(gps.geoidValid()) snprintf(buf,sizeof(buf),"Geoid:%.1fm",gps.geoidHeight()); else snprintf(buf,sizeof(buf),"Geoid:N/A"); cv.print(buf); y+=lh;
  if(imu.isAvailable()){cv.setCursor(4,y);snprintf(buf,sizeof(buf),"IMU P:%+.0f R:%+.0f G:%.1f T:%.0fC",imu.pitch(),imu.roll(),imu.gForce(),imu.temperature());cv.print(buf);}
}

void FnGpsDashboard::_drawTabCoord() {
  M5Canvas& cv = DisplayManager::instance().canvas();
  GPSManager& gps = GPSManager::instance();
  if (!gps.hasFix()) { cv.setTextSize(2); cv.setTextColor(TFT_DARKGREY); cv.setCursor(40,SCREEN_H/2-12); cv.print("No Fix"); return; }
  float lat=gps.latitude(),lon=gps.longitude(); char buf[40]; int y=CONTENT_TOP+12;
  cv.setTextSize(1); cv.setTextColor(TFT_DARKGREY); cv.setCursor(4,y); cv.print("LAT"); y+=10;
  cv.setTextSize(2); cv.setTextColor(TFT_GREEN); snprintf(buf,sizeof(buf),"%.6f",lat); cv.setCursor(4,y); cv.print(buf); y+=18;
  cv.setTextSize(1); cv.setTextColor(TFT_WHITE);
  float absLat=fabs(lat); int latD=(int)absLat; float latRem=(absLat-latD)*60.0; int latM=(int)latRem; float latS=(latRem-latM)*60.0;
  snprintf(buf,sizeof(buf),"%d%c %d' %.1f\" %s",latD,0xF8,latM,latS,lat>=0?"N":"S"); cv.setCursor(4,y); cv.print(buf); y+=14;
  cv.setTextSize(1); cv.setTextColor(TFT_DARKGREY); cv.setCursor(4,y); cv.print("LON"); y+=10;
  cv.setTextSize(2); cv.setTextColor(TFT_GREEN); snprintf(buf,sizeof(buf),"%.6f",lon); cv.setCursor(4,y); cv.print(buf); y+=18;
  cv.setTextSize(1); cv.setTextColor(TFT_WHITE);
  float absLon=fabs(lon); int lonD=(int)absLon; float lonRem=(absLon-lonD)*60.0; int lonM=(int)lonRem; float lonS=(lonRem-lonM)*60.0;
  snprintf(buf,sizeof(buf),"%d%c %d' %.1f\" %s",lonD,0xF8,lonM,lonS,lon>=0?"E":"W"); cv.setCursor(4,y); cv.print(buf);
}

void FnGpsDashboard::_drawSkyPlot(int px, int py, int pw, int ph) {
  M5Canvas& cv = DisplayManager::instance().canvas();
  GPSManager& gps = GPSManager::instance();
  const auto& sats = gps.satellites();
  int side=(pw<ph)?pw:ph, r=side/2; int cx=px+pw/2, cy=py+ph/2;
  cv.drawRect(px-1,py-1,pw+2,ph+2,TFT_DARKGREY);
  cv.fillRect(px,py,pw,ph,TFT_BLACK);
  cv.drawCircle(cx,cy,r,TFT_WHITE);
  cv.drawCircle(cx,cy,r*0.66,TFT_DARKGREY);
  cv.drawCircle(cx,cy,r*0.33,TFT_DARKGREY);
  cv.drawLine(cx-r,cy,cx+r,cy,TFT_DARKGREY);
  cv.drawLine(cx,cy-r,cx,cy+r,TFT_DARKGREY);
  cv.setTextSize(1); cv.setTextColor(TFT_LIGHTGREY);
  cv.setCursor(cx-3,cy-r+4); cv.print("N");
  cv.setCursor(cx-3,cy+r-10); cv.print("S");
  cv.setCursor(cx+r-10,cy-3); cv.print("E");
  cv.setCursor(cx-r+4,cy-3); cv.print("W");
  int dotR=(r>50)?3:2;
  for (auto& sat:sats){float elev=constrain(sat.elevation,0,90); float az=fmod(sat.azimuth+360.0,360.0); float rad=(90.0-elev)/90.0*r; float radAz=radians(az); float sx=cx+rad*sin(radAz); float sy=cy-rad*cos(radAz); uint16_t color=sat.used?TFT_GREEN:(sat.visible?TFT_YELLOW:TFT_RED); cv.fillCircle(sx,sy,dotR,color);}
}

void FnGpsDashboard::drawIcon(int x, int y, int size, uint16_t color) {
  M5Canvas& cv = DisplayManager::instance().canvas();
  int cx = x + size / 2, cy = y + size / 2;
  int r = size / 2 - 2;
  // Outer ring
  cv.drawCircle(cx, cy, r, color);
  // Inner ring
  cv.drawCircle(cx, cy, r - 3, color);
  // Crosshair lines
  cv.drawLine(cx - r + 5, cy, cx - 2, cy, color);
  cv.drawLine(cx + 2, cy, cx + r - 5, cy, color);
  cv.drawLine(cx, cy - r + 5, cx, cy - 2, color);
  cv.drawLine(cx, cy + 2, cx, cy + r - 5, color);
  // Center dot
  cv.fillCircle(cx, cy, 2, color);
}
