// display_controller.cpp - Display pulse generation and DAC control

#include "display_controller.h"
#include "DFRobot_GP8403.h"
#include <Wire.h>

// LITE: Helper function moved from wind_calculations.h
inline int wrap360(int deg) {
  while (deg < 0) deg += 360;
  while (deg >= 360) deg -= 360;
  return deg;
}
// DEG_TO_RAD is already defined in Arduino.h

// External references (LITE Multi: 3 displays + global direction)
extern SemaphoreHandle_t dataMutex;
extern DisplayConfig displays[3];
extern DFRobot_GP8403 dac;
extern const uint8_t LEDC_CHANNELS[3];
extern bool ledcActive[3];
extern uint32_t lastFreq[3];
extern int lastAngleSent;

extern uint8_t global_direction_source;
extern int global_wind_angle;

// LITE Plus: Multiple data sources
extern float apparent_speed_kn;
extern float apparent_angle_deg;
extern bool apparent_hasData;
extern uint32_t apparent_lastUpdate_ms;

extern float true_speed_kn;
extern float true_angle_deg;
extern bool true_hasData;
extern uint32_t true_lastUpdate_ms;

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

void setOutputsDeg(int deg){
  // LITE Multi: Use global direction (no per-display offset)
  int adj = wrap360(deg);
  float r = adj * DEG_TO_RAD;
  float s = sinf(r), c = cosf(r);
  float amp = DAC_VAMP_BASE;
  int sin_mV = mvClamp(DAC_VCEN + (int)lroundf(amp * s));
  int cos_mV = mvClamp(DAC_VCEN + (int)lroundf(amp * c));
  
  dac.setDACOutVoltage(sin_mV, CH_SIN);
  dac.setDACOutVoltage(cos_mV, CH_COS);
  
  // Track direction changes
  if (adj != lastAngleSent) {
    Serial.printf("Global Direction: %d° (sin:%dmV cos:%dmV)\n", adj, sin_mV, cos_mV);
  }
  lastAngleSent = adj;
}

/* ========= LEDC Pulse Generation ========= */

void startDisplay(int displayNum) {
  if (displayNum < 0 || displayNum >= 3) return;
  
  if (!ledcActive[displayNum] && displays[displayNum].enabled) {
    // Setup LEDC channel with separate timer
    ledcSetup(LEDC_CHANNELS[displayNum], LEDC_BASE_FREQ, LEDC_TIMER_RESOLUTION);
    ledcAttachPin(displays[displayNum].pulsePin, LEDC_CHANNELS[displayNum]);
    ledcActive[displayNum] = true;
    lastFreq[displayNum] = 0;
    
    Serial.printf("Display %d LEDC started\n", displayNum);
    updateDisplayPulse(displayNum);
  }
}

void stopDisplay(int displayNum) {
  if (displayNum < 0 || displayNum >= 3) return;
  
  if (ledcActive[displayNum]) {
    ledcWrite(LEDC_CHANNELS[displayNum], 0);
    ledcDetachPin(displays[displayNum].pulsePin);
    ledcActive[displayNum] = false;
    lastFreq[displayNum] = 0;
    pinMode(displays[displayNum].pulsePin, INPUT);
    Serial.printf("Display %d LEDC stopped\n", displayNum);
  }
}

// LITE Multi: Update global direction from selected source
void updateGlobalDirection() {
  xSemaphoreTake(dataMutex, portMAX_DELAY);
  uint32_t now = millis();
  
  // Update global direction based on selected source
  switch(global_direction_source) {
    case DATA_APPARENT_WIND:
      if (apparent_hasData && (now - apparent_lastUpdate_ms) < DATA_TIMEOUT_MS) {
        global_wind_angle = (int)apparent_angle_deg;
      }
      break;
      
    case DATA_TRUE_WIND:
      if (true_hasData && (now - true_lastUpdate_ms) < DATA_TIMEOUT_MS) {
        global_wind_angle = (int)true_angle_deg;
      }
      break;
      
    case DATA_COG:
      if (gps_hasCOG && (now - gps_lastUpdate_ms) < DATA_TIMEOUT_MS) {
        global_wind_angle = (int)gps_cog_deg;
      }
      break;
  }
  xSemaphoreGive(dataMutex);
  
  // Update DAC with global direction
  setOutputsDeg(global_wind_angle);
}

// LITE Multi: Update display speed from selected source
void updateDisplaySpeed(int displayNum) {
  if (displayNum < 0 || displayNum >= 3) return;
  
  xSemaphoreTake(dataMutex, portMAX_DELAY);
  uint8_t speedSource = displays[displayNum].speedSource;
  uint32_t now = millis();
  
  // Update display speed based on selected source
  switch(speedSource) {
    case DATA_APPARENT_WIND:
      if (apparent_hasData && (now - apparent_lastUpdate_ms) < DATA_TIMEOUT_MS) {
        displays[displayNum].currentSpeed_kn = apparent_speed_kn;
        displays[displayNum].lastUpdate_ms = apparent_lastUpdate_ms;
      }
      break;
      
    case DATA_TRUE_WIND:
      if (true_hasData && (now - true_lastUpdate_ms) < DATA_TIMEOUT_MS) {
        displays[displayNum].currentSpeed_kn = true_speed_kn;
        displays[displayNum].lastUpdate_ms = true_lastUpdate_ms;
      }
      break;
      
    case DATA_SOG:
      if (gps_hasSOG && (now - gps_lastUpdate_ms) < DATA_TIMEOUT_MS) {
        displays[displayNum].currentSpeed_kn = gps_sog_kn;
        displays[displayNum].lastUpdate_ms = gps_lastUpdate_ms;
      }
      break;
      
    case DATA_COG:
      // COG has no speed, set to 0
      displays[displayNum].currentSpeed_kn = 0;
      if (gps_hasCOG && (now - gps_lastUpdate_ms) < DATA_TIMEOUT_MS) {
        displays[displayNum].lastUpdate_ms = gps_lastUpdate_ms;
      }
      break;
  }
  xSemaphoreGive(dataMutex);
}

void updateDisplayPulse(int displayNum) {
  if (displayNum < 0 || displayNum >= 3 || !ledcActive[displayNum]) return;
  
  // LITE Multi: Read display speed with mutex protection
  float currentSpeed;
  uint32_t lastUpdate;
  xSemaphoreTake(dataMutex, portMAX_DELAY);
  currentSpeed = displays[displayNum].currentSpeed_kn;
  lastUpdate = displays[displayNum].lastUpdate_ms;
  xSemaphoreGive(dataMutex);
  
  // Check for data timeout
  uint32_t now = millis();
  uint32_t dataAge = now - lastUpdate;
  
  // Zero speed if data is stale
  if (dataAge > DATA_TIMEOUT_MS) {
    currentSpeed = 0.0f;
    if (lastFreq[displayNum] != 0) {
      Serial.printf("Display %d timeout! age=%u ms - zeroing speed\n", displayNum, dataAge);
    }
  }
  
  if (strcmp(displays[displayNum].type, "sumlog") == 0 || strcmp(displays[displayNum].type, "logicwind") == 0) {
    // Stop immediately if raw speed is 0
    if (currentSpeed < 0.01f && lastFreq[displayNum] != 0) {
      ledcWrite(LEDC_CHANNELS[displayNum], 0);
      lastFreq[displayNum] = 0;
      Serial.printf("Display %d stopped (speed=0)\n", displayNum);
      return;
    }
    
    // Pulse calculation (same for both types)
    float freq = currentSpeed * displays[displayNum].sumlogK;
    if (freq > (float)displays[displayNum].sumlogFmax) freq = (float)displays[displayNum].sumlogFmax;
    
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
          uint32_t duty = (uint32_t)((1023 * displays[displayNum].pulseDuty) / 100);
          
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
