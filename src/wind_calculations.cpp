// wind_calculations.cpp - LITE Multi: True Wind and VMG calculations

#include "wind_calculations.h"
#include "web_ui.h"
#include <Arduino.h>
#include <math.h>

// External references
extern SemaphoreHandle_t dataMutex;

// Apparent Wind data
extern float apparent_speed_kn;
extern float apparent_angle_deg;
extern bool apparent_hasData;
extern uint32_t apparent_lastUpdate_ms;

// True Wind data (calculated)
extern float true_speed_kn;
extern float true_angle_deg;
extern bool true_hasData;
extern uint32_t true_lastUpdate_ms;
extern char true_source[16];

// VMG data (calculated)
extern float vmg_kn;
extern bool vmg_hasData;
extern uint32_t vmg_lastUpdate_ms;

// GPS data
extern float gps_sog_kn;
extern float gps_cog_deg;
extern float gps_heading_deg;
extern bool gps_hasSOG;
extern bool gps_hasCOG;
extern bool gps_hasHeading;
extern uint32_t gps_lastUpdate_ms;

// Helper: wrap angle to 0-360
int wrap360(int deg) {
  while (deg < 0) deg += 360;
  while (deg >= 360) deg -= 360;
  return deg;
}

// DEG_TO_RAD and RAD_TO_DEG are already defined in Arduino.h

/* ========= True Wind Calculation ========= */

bool calculateTrueWind(float& trueSpeed, float& trueAngle) {
  // Need: Apparent Wind + SOG + Heading (or COG as fallback)
  xSemaphoreTake(dataMutex, portMAX_DELAY);
  
  bool hasApparent = apparent_hasData && (millis() - apparent_lastUpdate_ms) < DATA_TIMEOUT_MS;
  bool hasGPS = gps_hasSOG && (millis() - gps_lastUpdate_ms) < DATA_TIMEOUT_MS;
  bool hasHeading = gps_hasHeading && (millis() - gps_lastUpdate_ms) < DATA_TIMEOUT_MS;
  bool hasCOG = gps_hasCOG && (millis() - gps_lastUpdate_ms) < DATA_TIMEOUT_MS;
  
  if (!hasApparent || !hasGPS || (!hasHeading && !hasCOG)) {
    xSemaphoreGive(dataMutex);
    return false;
  }
  
  // Get data
  float aws = apparent_speed_kn;
  float awa = apparent_angle_deg;
  float sog = gps_sog_kn;
  float heading = hasHeading ? gps_heading_deg : gps_cog_deg;  // Fallback to COG
  
  xSemaphoreGive(dataMutex);
  
  // Convert AWA to radians
  float awa_rad = awa * DEG_TO_RAD;
  
  // Calculate boat velocity components (forward/sideways)
  float boat_forward = sog;  // Boat speed forward
  float boat_side = 0.0f;    // Assume no sideways (simplification)
  
  // Calculate apparent wind components relative to boat
  float aws_forward = aws * cos(awa_rad);
  float aws_side = aws * sin(awa_rad);
  
  // Calculate true wind components (remove boat motion)
  float tws_forward = aws_forward - boat_forward;
  float tws_side = aws_side - boat_side;
  
  // Calculate true wind speed and angle
  trueSpeed = sqrt(tws_forward * tws_forward + tws_side * tws_side);
  float twa_rad = atan2(tws_side, tws_forward);
  float twa = twa_rad * RAD_TO_DEG;
  
  // Convert to 0-360 range
  trueAngle = wrap360((int)twa);
  
  return true;
}

/* ========= VMG Calculation ========= */

bool calculateVMG(float& vmg) {
  // VMG towards wind = SOG × cos(TWA)
  // Need: SOG + True Wind Angle
  
  xSemaphoreTake(dataMutex, portMAX_DELAY);
  
  bool hasSOG = gps_hasSOG && (millis() - gps_lastUpdate_ms) < DATA_TIMEOUT_MS;
  bool hasTrueWind = true_hasData && (millis() - true_lastUpdate_ms) < DATA_TIMEOUT_MS;
  bool hasHeading = gps_hasHeading && (millis() - gps_lastUpdate_ms) < DATA_TIMEOUT_MS;
  bool hasCOG = gps_hasCOG && (millis() - gps_lastUpdate_ms) < DATA_TIMEOUT_MS;
  
  if (!hasSOG || !hasTrueWind || (!hasHeading && !hasCOG)) {
    xSemaphoreGive(dataMutex);
    return false;
  }
  
  // Get data
  float sog = gps_sog_kn;
  float twd = true_angle_deg;  // True Wind Direction
  float heading = hasHeading ? gps_heading_deg : gps_cog_deg;  // Fallback to COG
  
  xSemaphoreGive(dataMutex);
  
  // Calculate True Wind Angle (TWA) = True Wind Direction - Heading
  float twa = twd - heading;
  
  // Normalize to -180 to +180
  while (twa > 180) twa -= 360;
  while (twa < -180) twa += 360;
  
  // VMG = SOG × cos(TWA)
  vmg = sog * cos(twa * DEG_TO_RAD);
  
  // Log if using COG fallback
  return true;
}

/* ========= Update Calculations ========= */

void updateCalculations() {
  uint32_t now = millis();
  
  // Calculate True Wind (if not already received from NMEA)
  float calcTrueSpeed, calcTrueAngle;
  if (calculateTrueWind(calcTrueSpeed, calcTrueAngle)) {
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    
    // Only update if we don't have True Wind from NMEA, or if NMEA data is stale
    bool nmeaTrueStale = !true_hasData || (now - true_lastUpdate_ms) > DATA_TIMEOUT_MS;
    
    if (nmeaTrueStale) {
      true_speed_kn = calcTrueSpeed;
      true_angle_deg = calcTrueAngle;
      true_hasData = true;
      true_lastUpdate_ms = now;
      strncpy(true_source, "Calculated", sizeof(true_source) - 1);
    }
    
    xSemaphoreGive(dataMutex);
  }
  
  // Calculate VMG
  float calcVMG;
  if (calculateVMG(calcVMG)) {
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    vmg_kn = calcVMG;
    vmg_hasData = true;
    vmg_lastUpdate_ms = now;
    xSemaphoreGive(dataMutex);
  }
}
