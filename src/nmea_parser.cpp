// nmea_parser.cpp - NMEA 0183 sentence parsing

#include "nmea_parser.h"
#include "web_ui.h"

// LITE: Helper functions (from wind_calculations.h)
inline int wrap360(int deg) {
  while (deg < 0) deg += 360;
  while (deg >= 360) deg -= 360;
  return deg;
}

// External references to global state (LITE Plus: GPS restored)
extern SemaphoreHandle_t dataMutex;
extern SpeedPulseConfig speedPulses[3];
extern bool hasMwvR;
extern bool hasMwvT;
extern bool hasVwr;
extern bool hasVwt;

// Apparent Wind data
extern float apparent_speed_kn;
extern float apparent_angle_deg;
extern bool apparent_hasData;
extern uint32_t apparent_lastUpdate_ms;
extern char apparent_source[16];

// True Wind data
extern float true_speed_kn;
extern float true_angle_deg;
extern bool true_hasData;
extern uint32_t true_lastUpdate_ms;
extern char true_source[16];

// GPS data
extern float gps_sog_kn;
extern float gps_cog_deg;
extern float gps_heading_deg;
extern bool gps_hasSOG;
extern bool gps_hasCOG;
extern bool gps_hasHeading;
extern uint32_t gps_lastUpdate_ms;

/* ========= Validation Functions ========= */

bool nmeaChecksumOK(const char* s){
  const char* star = strrchr(s, '*');
  if(!star) return true;
  uint8_t cs=0; const char* p = s+1;
  while(p && *p && p<star){ cs ^= (uint8_t)(*p++); }
  if(*(star+1)==0 || *(star+2)==0) return true;
  char hex[3]={star[1], star[2], 0};
  uint8_t want = (uint8_t)strtoul(hex, nullptr, 16);
  return cs==want;
}

int splitCSV(char* line, char* fields[], int maxf){
  int n=0; for(char* p=line; *p && n<maxf; ){
    fields[n++]=p; char* c=strchr(p, ','); if(!c) break; *c=0; p=c+1;
  } return n;
}

bool hasFormatter(const char* s, const char* fmt3){
  const char* p=s; if(*p=='$') p++;
  if(strlen(p)<5) return false;
  // Check for formatter - can be at position 0-2 or 2-4 (for talkers like II, WI, etc)
  // Standard: $IIMWV (pos 2-4) or Yachta: $WIMWV (pos 3-5)
  if (p[2]==fmt3[0] && p[3]==fmt3[1] && p[4]==fmt3[2]) return true;
  if (strlen(p)>=6 && p[3]==fmt3[0] && p[4]==fmt3[1] && p[5]==fmt3[2]) return true;
  return false;
}

/* ========= Sentence Parsers ========= */

bool parseMWV(char* line){
  char* f[12]; int n = splitCSV(line, f, 12);
  if(n<3) return false;
  if(!hasFormatter(line,"MWV")) return false;
  if(!nmeaChecksumOK(line)) return false;
  float ang = atof(f[1]); char ref = toupper((unsigned char)f[2][0]);
  if(ref!='R' && ref!='T') return false;
  if(!(ang>=0 && ang<=360)) return false;
  
  int newAngle = wrap360((int)lroundf(ang));
  float newSpeed = 0.0f;
  bool hasSpeed = false;
  
  if(n>=4) {
    float spd = atof(f[3]);
    if(spd>=0 && spd<200) {
      newSpeed = spd;
      hasSpeed = true;
    }
  }
  
  // LITE Plus: Process both Apparent (R) and True (T) Wind
  if(ref == 'R') {
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    apparent_angle_deg = (float)newAngle;
    if (hasSpeed) {
      apparent_speed_kn = newSpeed;
    }
    apparent_hasData = true;
    apparent_lastUpdate_ms = millis();
    strncpy(apparent_source, "MWV(R)", sizeof(apparent_source) - 1);
    xSemaphoreGive(dataMutex);
    
    Serial.printf("MWV(R): angle=%d°, speed=%.1f kn, hasSpeed=%d\n", newAngle, newSpeed, hasSpeed);
    
    hasMwvR = true;
    return true;
  } else if(ref == 'T') {
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    true_angle_deg = (float)newAngle;
    if (hasSpeed) {
      true_speed_kn = newSpeed;
    }
    true_hasData = true;
    true_lastUpdate_ms = millis();
    strncpy(true_source, "MWV(T)", sizeof(true_source) - 1);
    xSemaphoreGive(dataMutex);
    
    Serial.printf("MWV(T): angle=%d°, speed=%.1f kn, hasSpeed=%d\n", newAngle, newSpeed, hasSpeed);
    
    hasMwvT = true;
    return true;
  }
  return false;
}

bool parseVWR(char* line){
  char* f[12]; int n = splitCSV(line, f, 12);
  if(n<3) return false;
  if(!hasFormatter(line,"VWR")) return false;
  if(!nmeaChecksumOK(line)) return false;
  float ang = atof(f[1]); char side = toupper((unsigned char)f[2][0]);
  if(!(ang>=0 && ang<=180)) return false;
  int awa = (int)lroundf(ang);
  int newAngle = (side=='L') ? wrap360(360-awa) : awa;
  float newSpeed = 0.0f;
  bool hasSpeed = false;
  
  // VWR format: $IIVWR,angle,L/R,speed_kn,N,speed_ms,M,speed_kmh,K*checksum
  // We need to check which speed field has valid data and correct unit
  if(n>=5) {
    float spd = atof(f[3]);
    char unit = toupper((unsigned char)f[4][0]);
    if(spd>=0 && spd<200 && unit=='N') {  // N = knots
      newSpeed = spd;
      hasSpeed = true;
    }
  }
  if(!hasSpeed && n>=7) {  // Try m/s field
    float spd = atof(f[5]);
    char unit = toupper((unsigned char)f[6][0]);
    if(spd>=0 && spd<200 && unit=='M') {  // M = m/s
      newSpeed = spd * 1.94384;  // Convert m/s to knots
      hasSpeed = true;
    }
  }
  if(!hasSpeed && n>=9) {  // Try km/h field
    float spd = atof(f[7]);
    char unit = toupper((unsigned char)f[8][0]);
    if(spd>=0 && spd<200 && unit=='K') {  // K = km/h
      newSpeed = spd * 0.539957;  // Convert km/h to knots
      hasSpeed = true;
    }
  }
  
  // VWR is Apparent Wind
  xSemaphoreTake(dataMutex, portMAX_DELAY);
  apparent_angle_deg = (float)newAngle;
  if (hasSpeed) {
    apparent_speed_kn = newSpeed;
  }
  apparent_hasData = true;
  apparent_lastUpdate_ms = millis();
  strncpy(apparent_source, "VWR", sizeof(apparent_source) - 1);
  xSemaphoreGive(dataMutex);
  
  Serial.printf("VWR: angle=%d°, speed=%.1f kn, hasSpeed=%d\n", newAngle, newSpeed, hasSpeed);
  
  hasVwr = true;
  return true;
}

// Parse RMC: $GPRMC,time,status,lat,N,lon,E,speed,course,date,magvar,E*checksum
bool parseRMC(char* line){
  char* f[15]; int n = splitCSV(line, f, 15);
  if(n<9) return false;
  if(!hasFormatter(line,"RMC")) return false;
  if(!nmeaChecksumOK(line)) return false;
  
  // f[2] = status (A=valid, V=invalid)
  if(f[2][0] != 'A') return false;
  
  // f[7] = speed over ground (knots)
  // f[8] = course over ground (degrees)
  float sog = atof(f[7]);
  float cog = atof(f[8]);
  
  if(sog >= 0 && sog < 100 && cog >= 0 && cog <= 360) {
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    gps_sog_kn = sog;
    gps_cog_deg = cog;
    gps_hasSOG = true;
    gps_hasCOG = true;
    gps_lastUpdate_ms = millis();
    xSemaphoreGive(dataMutex);
    
    Serial.printf("GPS RMC: SOG=%.1f kn, COG=%.1f°\n", sog, cog);
    return true;
  }
  return false;
}

// Parse VTG: $GPVTG,cogt,T,cogm,M,sog,N,sogk,K*checksum
bool parseVTG(char* line){
  char* f[12]; int n = splitCSV(line, f, 12);
  if(n<8) return false;
  if(!hasFormatter(line,"VTG")) return false;
  if(!nmeaChecksumOK(line)) return false;
  
  // f[1] = course over ground (true)
  // f[5] = speed over ground (knots)
  float cog = atof(f[1]);
  float sog = atof(f[5]);
  
  if(sog >= 0 && sog < 100 && cog >= 0 && cog <= 360) {
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    gps_sog_kn = sog;
    gps_cog_deg = cog;
    gps_hasSOG = true;
    gps_hasCOG = true;
    gps_lastUpdate_ms = millis();
    xSemaphoreGive(dataMutex);
    
    Serial.printf("GPS VTG: SOG=%.1f kn, COG=%.1f°\n", sog, cog);
    return true;
  }
  return false;
}

// Parse VWT: $xxVWT,angle,L/R,speed,N,speed,M,speed,K*checksum (True Wind)
bool parseVWT(char* line){
  char* f[12]; int n = splitCSV(line, f, 12);
  if(n<3) return false;
  if(!hasFormatter(line,"VWT")) return false;
  if(!nmeaChecksumOK(line)) return false;
  float ang = atof(f[1]); char side = toupper((unsigned char)f[2][0]);
  if(!(ang>=0 && ang<=180)) return false;
  int awa = (int)lroundf(ang);
  int newAngle = (side=='L') ? wrap360(360-awa) : awa;
  float newSpeed = 0.0f;
  bool hasSpeed = false;
  
  if(n>=4) {
    float spd = atof(f[3]);
    if(spd>=0 && spd<200) {
      newSpeed = spd;
      hasSpeed = true;
    }
  }
  
  // VWT is True Wind
  xSemaphoreTake(dataMutex, portMAX_DELAY);
  true_angle_deg = (float)newAngle;
  if (hasSpeed) {
    true_speed_kn = newSpeed;
  }
  true_hasData = true;
  true_lastUpdate_ms = millis();
  strncpy(true_source, "VWT", sizeof(true_source) - 1);
  xSemaphoreGive(dataMutex);
  
  Serial.printf("VWT: angle=%d°, speed=%.1f kn, hasSpeed=%d\n", newAngle, newSpeed, hasSpeed);
  
  hasVwt = true;
  return true;
}

/* ========= Main Parser Dispatcher ========= */

// Parse HDT: $xxHDT,heading,T*checksum (True Heading)
bool parseHDT(char* line){
  char* f[5]; int n = splitCSV(line, f, 5);
  if(n<2) return false;
  if(!hasFormatter(line,"HDT")) return false;
  if(!nmeaChecksumOK(line)) return false;
  
  float heading = atof(f[1]);
  
  if(heading >= 0 && heading <= 360) {
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    gps_heading_deg = heading;
    gps_hasHeading = true;
    gps_lastUpdate_ms = millis();
    xSemaphoreGive(dataMutex);
    
    Serial.printf("HDT: Heading=%.1f° (true)\n", heading);
    return true;
  }
  return false;
}

// Parse HDM: $xxHDM,heading,M*checksum (Magnetic Heading)
bool parseHDM(char* line){
  char* f[5]; int n = splitCSV(line, f, 5);
  if(n<2) return false;
  if(!hasFormatter(line,"HDM")) return false;
  if(!nmeaChecksumOK(line)) return false;
  
  float heading = atof(f[1]);
  
  if(heading >= 0 && heading <= 360) {
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    gps_heading_deg = heading;
    gps_hasHeading = true;
    gps_lastUpdate_ms = millis();
    xSemaphoreGive(dataMutex);
    
    Serial.printf("HDM: Heading=%.1f° (magnetic)\n", heading);
    return true;
  }
  return false;
}

bool parseNMEALine(char* line){
  if(strlen(line)<6 || line[0]!='$') return false;
  static char tmp[256];
  size_t L = min(strlen(line), sizeof(tmp)-1);
  
  // LITE Multi: Wind sentences (Apparent + True)
  memcpy(tmp, line, L); tmp[L]=0;
  if(hasFormatter(tmp,"MWV") && parseMWV(tmp)) return true;
  memcpy(tmp,line,L); tmp[L]=0;
  if(hasFormatter(tmp,"VWR") && parseVWR(tmp)) return true;
  memcpy(tmp,line,L); tmp[L]=0;
  if(hasFormatter(tmp,"VWT") && parseVWT(tmp)) return true;
  
  // GPS sentences
  memcpy(tmp,line,L); tmp[L]=0;
  if(hasFormatter(tmp,"RMC") && parseRMC(tmp)) return true;
  memcpy(tmp,line,L); tmp[L]=0;
  if(hasFormatter(tmp,"VTG") && parseVTG(tmp)) return true;
  
  // Heading sentences
  memcpy(tmp,line,L); tmp[L]=0;
  if(hasFormatter(tmp,"HDT") && parseHDT(tmp)) return true;
  memcpy(tmp,line,L); tmp[L]=0;
  if(hasFormatter(tmp,"HDM") && parseHDM(tmp)) return true;
  
  return false;
}
