// display_controller.cpp - Display pulse generation and DAC control

#include "display_controller.h"
#include "wind_calculations.h"
#include "DFRobot_GP8403.h"
#include <Wire.h>

// External references
extern SemaphoreHandle_t dataMutex;
extern DisplayConfig displays[3];
extern DFRobot_GP8403 dac;
extern const uint8_t LEDC_CHANNELS[3];
extern bool ledcActive[3];
extern uint32_t lastFreq[3];
extern int lastAngleSent;

// Data source tracking
extern float apparent_speed_kn;
extern float apparent_angle_deg;
extern bool apparent_hasData;
extern uint32_t apparent_lastUpdate_ms;

extern float true_speed_kn;
extern float true_angle_deg;
extern bool true_hasData;
extern uint32_t true_lastUpdate_ms;

extern float vmg_kn;
extern bool vmg_hasData;
extern uint32_t vmg_lastUpdate_ms;

extern float gps_sog_kn;
extern float gps_cog_deg;
extern bool gps_hasSOG;
extern bool gps_hasCOG;
extern uint32_t gps_lastUpdate_ms;

/* ========= Helper Functions ========= */

int mvClamp(int mv){ 
  if(mv < DAC_VMIN) return DAC_VMIN; 
  if(mv > DAC_VMAX) return DAC_VMAX; 
  return mv; 
}

/* ========= DAC Output ========= */

void setOutputsDeg(int displayNum, int deg){
  if (displayNum < 0 || displayNum >= 3) return;
  
  // Read per-display angle with mutex protection
  int displayAngle;
  xSemaphoreTake(dataMutex, portMAX_DELAY);
  displayAngle = displays[displayNum].windAngle_deg;
  xSemaphoreGive(dataMutex);
  
  int adj = wrap360(displayAngle + displays[displayNum].offsetDeg);
  float r = adj * DEG_TO_RAD;
  float s = sinf(r), c = cosf(r);
  float amp = DAC_VAMP_BASE;
  int sin_mV = mvClamp(DAC_VCEN + (int)lroundf(amp * s));
  int cos_mV = mvClamp(DAC_VCEN + (int)lroundf(amp * c));
  
  // Only update DAC for display 0 (primary display)
  if (displayNum == 0) {
    dac.setDACOutVoltage(sin_mV, CH_SIN);
    dac.setDACOutVoltage(cos_mV, CH_COS);
    
    // Track direction changes
    if (adj != lastAngleSent) {
      Serial.printf("Display %d Direction: %d° (sin:%dmV cos:%dmV)\n", displayNum, adj, sin_mV, cos_mV);
    }
    lastAngleSent = adj;
  }
}

/* ========= LEDC Pulse Generation ========= */

void startDisplay(int displayNum) {
  if (displayNum < 0 || displayNum >= 3) return;
  
  if (!ledcActive[displayNum] && displays[displayNum].enabled) {
    // Setup LEDC channel with separate timer
    ledcSetup(LEDC_CHANNELS[displayNum], LEDC_BASE_FREQ, LEDC_TIMER_RESOLUTION);
    ledcAttachPin(displays[displayNum].pulsePin, LEDC_CHANNELS[displayNum]);
    ledcActive[displayNum] = true;
    lastFreq[displayNum] = 0; // Reset frequency tracking
    
    Serial.printf("Display %d LEDC started\n", displayNum);
    updateDisplayPulse(displayNum);
  }
}

void stopDisplay(int displayNum) {
  if (displayNum < 0 || displayNum >= 3) return;
  
  if (ledcActive[displayNum]) {
    ledcWrite(LEDC_CHANNELS[displayNum], 0); // Stop PWM
    ledcDetachPin(displays[displayNum].pulsePin);
    ledcActive[displayNum] = false;
    lastFreq[displayNum] = 0; // Reset frequency tracking
    pinMode(displays[displayNum].pulsePin, INPUT);
    Serial.printf("Display %d LEDC stopped\n", displayNum);
  }
}

void updateDisplayPulse(int displayNum) {
  if (displayNum < 0 || displayNum >= 3 || !ledcActive[displayNum]) return;
  
  DisplayConfig &disp = displays[displayNum];
  
  // Read per-display speed with mutex protection
  float currentSpeed;
  uint32_t lastUpdate;
  uint8_t dataType;
  xSemaphoreTake(dataMutex, portMAX_DELAY);
  currentSpeed = disp.windSpeed_kn;
  lastUpdate = disp.lastUpdate_ms;
  dataType = disp.dataType;
  xSemaphoreGive(dataMutex);
  
  // Check for data timeout - both display data AND source data must be fresh
  uint32_t now = millis();
  uint32_t dataAge = now - lastUpdate;
  bool sourceDataStale = false;
  
  // Check if the source data for this display type is stale
  xSemaphoreTake(dataMutex, portMAX_DELAY);
  if (dataType == DATA_APPARENT_WIND && apparent_hasData) {
    sourceDataStale = (now - apparent_lastUpdate_ms) > DATA_TIMEOUT_MS;
  } else if (dataType == DATA_TRUE_WIND && true_hasData) {
    sourceDataStale = (now - true_lastUpdate_ms) > DATA_TIMEOUT_MS;
  } else if (dataType == DATA_VMG && vmg_hasData) {
    sourceDataStale = (now - vmg_lastUpdate_ms) > DATA_TIMEOUT_MS;
  } else if (dataType == DATA_SOG && gps_hasSOG) {
    sourceDataStale = (now - gps_lastUpdate_ms) > DATA_TIMEOUT_MS;
  } else if (dataType == DATA_COG && gps_hasCOG) {
    sourceDataStale = (now - gps_lastUpdate_ms) > DATA_TIMEOUT_MS;
  }
  xSemaphoreGive(dataMutex);
  
  // Zero speed if either display data or source data is stale
  if (dataAge > DATA_TIMEOUT_MS || sourceDataStale) {
    currentSpeed = 0.0f;
    if (lastFreq[displayNum] != 0) {
      Serial.printf("Display %d timeout! Display age=%u ms, source stale=%d - zeroing speed\n", 
                    displayNum, dataAge, sourceDataStale);
    }
  }
  
  if (strcmp(disp.type, "sumlog") == 0 || strcmp(disp.type, "logicwind") == 0) {
    // Stop immediately if raw speed is 0
    if (currentSpeed < 0.01f && lastFreq[displayNum] != 0) {
      ledcWrite(LEDC_CHANNELS[displayNum], 0);
      lastFreq[displayNum] = 0;
      Serial.printf("Display %d stopped (speed=0)\n", displayNum);
      return;
    }
    
    // Pulse calculation (same for both types)
    float freq = currentSpeed * disp.sumlogK;
    if (freq > (float)disp.sumlogFmax) freq = (float)disp.sumlogFmax;
    
    if (freq < 0.01f) {
      // Stop PWM when frequency too low
      if (lastFreq[displayNum] != 0) {
        ledcWrite(LEDC_CHANNELS[displayNum], 0);
        lastFreq[displayNum] = 0;
        Serial.printf("Display %d stopped (freq too low)\n", displayNum);
      }
    } else {
      // Round frequency to reduce jitter
      uint32_t freqInt = (uint32_t)(freq + 0.5f);
      
      // Only update if frequency actually changed
      if (freqInt != lastFreq[displayNum]) {
        if (freqInt == 0) {
          ledcWrite(LEDC_CHANNELS[displayNum], 0);
          lastFreq[displayNum] = 0;
          Serial.printf("Display %d stopped (0Hz avoided)\n", displayNum);
        } else {
          // Calculate duty cycle (0-1023 for 10-bit resolution)
          uint32_t duty = (uint32_t)((1023 * disp.pulseDuty) / 100);
          
          // Set frequency and duty cycle
          ledcChangeFrequency(LEDC_CHANNELS[displayNum], freqInt, LEDC_TIMER_RESOLUTION);
          ledcWrite(LEDC_CHANNELS[displayNum], duty);
          
          lastFreq[displayNum] = freqInt;
          Serial.printf("Display %d freq=%uHz (speed=%.1f kn)\n", displayNum, freqInt, currentSpeed);
        }
      }
    }
  } else {
    // Unknown type - no pulse, stop PWM
    if (lastFreq[displayNum] != 0) {
      ledcWrite(LEDC_CHANNELS[displayNum], 0);
      lastFreq[displayNum] = 0;
    }
  }
}

void updateAllDisplayPulses() {
  for (int i = 0; i < 3; i++) {
    if (ledcActive[i]) {
      updateDisplayPulse(i);
    }
  }
}
