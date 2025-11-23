// nmea_parser.cpp - NMEA 0183 sentence parsing

#include "nmea_parser.h"
#include "web_ui.h"

// LITE: Helper functions (from wind_calculations.h)
inline int wrap360(int deg) {
  while (deg < 0) deg += 360;
  while (deg >= 360) deg -= 360;
  return deg;
}

// External references to global state (LITE: GPS removed)
extern SemaphoreHandle_t dataMutex;
extern DisplayConfig display;
extern bool hasMwvR;
extern bool hasVwr;

// Apparent Wind data
extern float apparent_speed_kn;
extern float apparent_angle_deg;
extern bool apparent_hasData;
extern uint32_t apparent_lastUpdate_ms;
extern char apparent_source[16];

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
  
  // LITE: Only process Apparent Wind (R=Relative)
  if(ref == 'R') {
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    apparent_angle_deg = (float)newAngle;
    if (hasSpeed) {
      apparent_speed_kn = newSpeed;
    }
    apparent_hasData = true;
    apparent_lastUpdate_ms = millis();
    strncpy(apparent_source, "MWV(R)", sizeof(apparent_source) - 1);
    
    // Update single display
    display.windAngle_deg = newAngle;
    if (hasSpeed) {
      display.windSpeed_kn = newSpeed;
    }
    display.lastUpdate_ms = millis();
    xSemaphoreGive(dataMutex);
    
    hasMwvR = true;
    return true;
  }
  // LITE: Ignore True Wind (T)
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
  
  if(n>=4) {
    float spd = atof(f[3]);
    if(spd>=0 && spd<200) {
      newSpeed = spd;
      hasSpeed = true;
    }
  }
  
  // LITE: VWR is Apparent Wind - update single display
  xSemaphoreTake(dataMutex, portMAX_DELAY);
  apparent_angle_deg = (float)newAngle;
  if (hasSpeed) {
    apparent_speed_kn = newSpeed;
  }
  apparent_hasData = true;
  apparent_lastUpdate_ms = millis();
  strncpy(apparent_source, "VWR", sizeof(apparent_source) - 1);
  
  // Update single display
  display.windAngle_deg = newAngle;
  if (hasSpeed) {
    display.windSpeed_kn = newSpeed;
  }
  display.lastUpdate_ms = millis();
  xSemaphoreGive(dataMutex);
  
  hasVwr = true;
  return true;
}

// LITE: GPS parsers removed (RMC, VTG, HDT, HDM)
// LITE: True Wind parser removed (VWT)

/* ========= Main Parser Dispatcher ========= */

bool parseNMEALine(char* line){
  if(strlen(line)<6 || line[0]!='$') return false;
  static char tmp[256];
  size_t L = min(strlen(line), sizeof(tmp)-1);
  
  // LITE: Apparent Wind sentences only (MWV, VWR)
  memcpy(tmp, line, L); tmp[L]=0;
  if(hasFormatter(tmp,"MWV") && parseMWV(tmp)) return true;
  memcpy(tmp,line,L); tmp[L]=0;
  if(hasFormatter(tmp,"VWR") && parseVWR(tmp)) return true;
  
  return false;
}
