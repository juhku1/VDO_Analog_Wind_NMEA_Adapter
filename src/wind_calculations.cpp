// wind_calculations.cpp - Wind calculations and data processing

#include "wind_calculations.h"
#include "display_controller.h"
#include <math.h>

// External references to global state
extern SemaphoreHandle_t dataMutex;
extern DisplayConfig displays[3];
extern char lastSentenceType[];

// Apparent Wind data
extern float apparent_speed_kn;
extern float apparent_angle_deg;
extern bool apparent_hasData;
extern uint32_t apparent_lastUpdate_ms;
extern char apparent_source[];

// True Wind data
extern float true_speed_kn;
extern float true_angle_deg;
extern bool true_hasData;
extern uint32_t true_lastUpdate_ms;
extern char true_source[];

// VMG data
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

/* ========= Helper Functions ========= */

int wrap360(int d){ 
  d %= 360; 
  if(d < 0) d += 360; 
  return d; 
}

/* ========= Sentence Matching ========= */

bool sentenceMatchesDataType(const char* sentenceType, char reference, uint8_t dataType) {
  if (dataType == DATA_APPARENT_WIND) {
    // Apparent Wind: MWV(R) or VWR
    if (strcmp(sentenceType, "MWV") == 0 && reference == 'R') return true;
    if (strcmp(sentenceType, "VWR") == 0 && reference == '\0') return true;
  }
  else if (dataType == DATA_TRUE_WIND) {
    // True Wind: MWV(T) or VWT
    if (strcmp(sentenceType, "MWV") == 0 && reference == 'T') return true;
    if (strcmp(sentenceType, "VWT") == 0 && reference == '\0') return true;
  }
  
  return false;
}

/* ========= Display Updates ========= */

void updateDisplaysForSentence(const char* sentenceType, char reference, int angle, float speed, bool hasSpeed) {
  xSemaphoreTake(dataMutex, portMAX_DELAY);
  
  uint32_t now = millis();
  
  // Store Apparent Wind data for True Wind calculation
  if ((strcmp(sentenceType, "MWV") == 0 && reference == 'R') || strcmp(sentenceType, "VWR") == 0) {
    apparent_angle_deg = angle;
    if (hasSpeed) {
      apparent_speed_kn = speed;
    }
    apparent_hasData = true;
    apparent_lastUpdate_ms = now;
    
    // Track source
    if (strcmp(sentenceType, "MWV") == 0) {
      snprintf(apparent_source, 16, "MWV(R)");
    } else {
      snprintf(apparent_source, 16, "VWR");
    }
  }
  
  // Store True Wind data
  if ((strcmp(sentenceType, "MWV") == 0 && reference == 'T') || strcmp(sentenceType, "VWT") == 0) {
    true_angle_deg = angle;
    if (hasSpeed) {
      true_speed_kn = speed;
    }
    true_hasData = true;
    true_lastUpdate_ms = now;
    
    // Track source
    if (strcmp(sentenceType, "MWV") == 0) {
      snprintf(true_source, 16, "MWV(T)");
    } else {
      snprintf(true_source, 16, "VWT");
    }
  }
  
  // Update displays that match sentence directly
  for (int i = 0; i < 3; i++) {
    if (!displays[i].enabled) continue;
    
    // Check if this sentence matches display's data type
    if (sentenceMatchesDataType(sentenceType, reference, displays[i].dataType)) {
      displays[i].windAngle_deg = angle;
      if (hasSpeed) {
        displays[i].windSpeed_kn = speed;
      }
      displays[i].lastUpdate_ms = now;
    }
  }
  
  // Update lastSentenceType for debugging
  if (reference != '\0') {
    snprintf(lastSentenceType, 32, "%s(%c)", sentenceType, reference);
  } else {
    snprintf(lastSentenceType, 32, "%s", sentenceType);
  }
  
  xSemaphoreGive(dataMutex);
  
  // Try to calculate True Wind for displays that need it
  float trueSpeed, trueAngle;
  if (calculateTrueWind(trueSpeed, trueAngle)) {
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    
    // Store calculated True Wind data
    if (!true_hasData || (now - true_lastUpdate_ms) > 1000) {
      true_speed_kn = trueSpeed;
      true_angle_deg = trueAngle;
      true_hasData = true;
      true_lastUpdate_ms = now;
      snprintf(true_source, 16, "Calculated");
    }
    
    for (int i = 0; i < 3; i++) {
      if (!displays[i].enabled) continue;
      
      // If display wants True Wind but we don't have direct data, use calculated
      if (displays[i].dataType == DATA_TRUE_WIND) {
        // Only update if we don't have recent direct True Wind data
        if ((now - displays[i].lastUpdate_ms) > 1000) {
          displays[i].windAngle_deg = (int)trueAngle;
          displays[i].windSpeed_kn = trueSpeed;
          displays[i].lastUpdate_ms = now;
        }
      }
    }
    xSemaphoreGive(dataMutex);
  }
  
  // Calculate VMG for displays that need it
  float vmg;
  if (calculateVMG(vmg)) {
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    
    // Store VMG data
    vmg_kn = vmg;
    vmg_hasData = true;
    vmg_lastUpdate_ms = now;
    
    for (int i = 0; i < 3; i++) {
      if (!displays[i].enabled) continue;
      
      if (displays[i].dataType == DATA_VMG) {
        displays[i].windSpeed_kn = vmg;
        displays[i].windAngle_deg = 0; // VMG has no angle
        displays[i].lastUpdate_ms = now;
      }
    }
    xSemaphoreGive(dataMutex);
  }
  
  // Update SOG/COG for displays that need it
  xSemaphoreTake(dataMutex, portMAX_DELAY);
  if (gps_hasSOG || gps_hasCOG) {
    for (int i = 0; i < 3; i++) {
      if (!displays[i].enabled) continue;
      
      if (displays[i].dataType == DATA_SOG && gps_hasSOG) {
        displays[i].windSpeed_kn = gps_sog_kn;
        displays[i].windAngle_deg = 0; // SOG has no angle
        displays[i].lastUpdate_ms = now;
      }
      else if (displays[i].dataType == DATA_COG && gps_hasCOG) {
        displays[i].windSpeed_kn = 0; // COG is angle only
        displays[i].windAngle_deg = (int)gps_cog_deg;
        displays[i].lastUpdate_ms = now;
      }
    }
  }
  xSemaphoreGive(dataMutex);
  
  // Update pulse outputs for all active displays
  updateAllDisplayPulses();
}

/* ========= Wind Calculations ========= */

bool calculateTrueWind(float& trueSpeed, float& trueAngle) {
  // Need: Apparent Wind + SOG + COG (or Heading)
  xSemaphoreTake(dataMutex, portMAX_DELAY);
  
  bool hasData = apparent_hasData && gps_hasSOG && (gps_hasCOG || gps_hasHeading);
  if (!hasData) {
    xSemaphoreGive(dataMutex);
    return false;
  }
  
  // Check data age (max 5 seconds old)
  uint32_t now = millis();
  if ((now - apparent_lastUpdate_ms) > 5000 || (now - gps_lastUpdate_ms) > 5000) {
    xSemaphoreGive(dataMutex);
    return false;
  }
  
  float aws = apparent_speed_kn;  // Apparent Wind Speed
  float awa = apparent_angle_deg; // Apparent Wind Angle (relative to bow)
  float sog = gps_sog_kn;         // Speed Over Ground
  float cog = gps_hasCOG ? gps_cog_deg : gps_heading_deg; // Course/Heading
  
  xSemaphoreGive(dataMutex);
  
  // Convert to radians
  float awa_rad = awa * DEG_TO_RAD;
  
  // Apparent Wind vector components (relative to boat)
  float aws_x = aws * sin(awa_rad);  // Cross component
  float aws_y = aws * cos(awa_rad);  // Forward component
  
  // Boat velocity vector (subtract from apparent to get true)
  // Boat moves forward, so subtract from forward component
  float tws_x = aws_x;
  float tws_y = aws_y - sog;
  
  // True Wind Speed and Angle
  trueSpeed = sqrt(tws_x * tws_x + tws_y * tws_y);
  trueAngle = atan2(tws_x, tws_y) * RAD_TO_DEG;
  
  // Normalize angle to 0-360
  if (trueAngle < 0) trueAngle += 360;
  
  Serial.printf("Calculated True Wind: AWS=%.1f@%.0f° + SOG=%.1f -> TWS=%.1f@%.0f°\n",
                aws, awa, sog, trueSpeed, trueAngle);
  
  return true;
}

bool calculateVMG(float& vmg) {
  // For now, just return SOG (simple VMG without waypoint)
  // Future: VMG = SOG × cos(angle to waypoint)
  xSemaphoreTake(dataMutex, portMAX_DELAY);
  
  if (!gps_hasSOG) {
    xSemaphoreGive(dataMutex);
    return false;
  }
  
  vmg = gps_sog_kn;
  xSemaphoreGive(dataMutex);
  
  return true;
}
