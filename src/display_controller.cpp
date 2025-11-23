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

// External references (LITE: single display)
extern SemaphoreHandle_t dataMutex;
extern DisplayConfig display;
extern DFRobot_GP8403 dac;
extern const uint8_t LEDC_CHANNEL;
extern bool ledcActive;
extern uint32_t lastFreq;
extern int lastAngleSent;

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
  // Read display angle with mutex protection
  int displayAngle;
  xSemaphoreTake(dataMutex, portMAX_DELAY);
  displayAngle = display.windAngle_deg;
  xSemaphoreGive(dataMutex);
  
  int adj = wrap360(displayAngle + display.offsetDeg);
  float r = adj * DEG_TO_RAD;
  float s = sinf(r), c = cosf(r);
  float amp = DAC_VAMP_BASE;
  int sin_mV = mvClamp(DAC_VCEN + (int)lroundf(amp * s));
  int cos_mV = mvClamp(DAC_VCEN + (int)lroundf(amp * c));
  
  dac.setDACOutVoltage(sin_mV, CH_SIN);
  dac.setDACOutVoltage(cos_mV, CH_COS);
  
  // Track direction changes
  if (adj != lastAngleSent) {
    Serial.printf("Direction: %d° (sin:%dmV cos:%dmV)\n", adj, sin_mV, cos_mV);
  }
  lastAngleSent = adj;
}

/* ========= LEDC Pulse Generation ========= */

void startDisplay() {
  if (!ledcActive && display.enabled) {
    // Setup LEDC channel
    ledcSetup(LEDC_CHANNEL, LEDC_BASE_FREQ, LEDC_TIMER_RESOLUTION);
    ledcAttachPin(display.pulsePin, LEDC_CHANNEL);
    ledcActive = true;
    lastFreq = 0; // Reset frequency tracking
    
    Serial.println("Display LEDC started");
    updateDisplayPulse();
  }
}

void stopDisplay() {
  if (ledcActive) {
    ledcWrite(LEDC_CHANNEL, 0); // Stop PWM
    ledcDetachPin(display.pulsePin);
    ledcActive = false;
    lastFreq = 0; // Reset frequency tracking
    pinMode(display.pulsePin, INPUT);
    Serial.println("Display LEDC stopped");
  }
}

// LITE Plus: Update display data from selected source
void updateDisplayData() {
  xSemaphoreTake(dataMutex, portMAX_DELAY);
  uint8_t dataType = display.dataType;
  uint32_t now = millis();
  
  // Update display based on selected data type
  switch(dataType) {
    case DATA_APPARENT_WIND:
      if (apparent_hasData && (now - apparent_lastUpdate_ms) < DATA_TIMEOUT_MS) {
        display.windSpeed_kn = apparent_speed_kn;
        display.windAngle_deg = (int)apparent_angle_deg;
        display.lastUpdate_ms = apparent_lastUpdate_ms;
      }
      break;
      
    case DATA_TRUE_WIND:
      if (true_hasData && (now - true_lastUpdate_ms) < DATA_TIMEOUT_MS) {
        display.windSpeed_kn = true_speed_kn;
        display.windAngle_deg = (int)true_angle_deg;
        display.lastUpdate_ms = true_lastUpdate_ms;
      }
      break;
      
    case DATA_SOG:
      if (gps_hasSOG && (now - gps_lastUpdate_ms) < DATA_TIMEOUT_MS) {
        display.windSpeed_kn = gps_sog_kn;
        display.windAngle_deg = 0;  // SOG has no angle
        display.lastUpdate_ms = gps_lastUpdate_ms;
      }
      break;
      
    case DATA_COG:
      if (gps_hasCOG && (now - gps_lastUpdate_ms) < DATA_TIMEOUT_MS) {
        display.windSpeed_kn = 0;  // COG has no speed
        display.windAngle_deg = (int)gps_cog_deg;
        display.lastUpdate_ms = gps_lastUpdate_ms;
      }
      break;
  }
  xSemaphoreGive(dataMutex);
}

void updateDisplayPulse() {
  if (!ledcActive) return;
  
  // Read display speed with mutex protection
  float currentSpeed;
  uint32_t lastUpdate;
  uint8_t dataType;
  xSemaphoreTake(dataMutex, portMAX_DELAY);
  currentSpeed = display.windSpeed_kn;
  lastUpdate = display.lastUpdate_ms;
  dataType = display.dataType;
  xSemaphoreGive(dataMutex);
  
  // Check for data timeout
  uint32_t now = millis();
  uint32_t dataAge = now - lastUpdate;
  bool sourceDataStale = false;
  
  // LITE Plus: Check selected data source
  xSemaphoreTake(dataMutex, portMAX_DELAY);
  if (dataType == DATA_APPARENT_WIND && apparent_hasData) {
    sourceDataStale = (now - apparent_lastUpdate_ms) > DATA_TIMEOUT_MS;
  } else if (dataType == DATA_TRUE_WIND && true_hasData) {
    sourceDataStale = (now - true_lastUpdate_ms) > DATA_TIMEOUT_MS;
  } else if ((dataType == DATA_SOG || dataType == DATA_COG) && (gps_hasSOG || gps_hasCOG)) {
    sourceDataStale = (now - gps_lastUpdate_ms) > DATA_TIMEOUT_MS;
  }
  xSemaphoreGive(dataMutex);
  
  // Zero speed if either display data or source data is stale
  if (dataAge > DATA_TIMEOUT_MS || sourceDataStale) {
    currentSpeed = 0.0f;
    if (lastFreq != 0) {
      Serial.printf("Display timeout! Display age=%u ms, source stale=%d - zeroing speed\n", 
                    dataAge, sourceDataStale);
    }
  }
  
  if (strcmp(display.type, "sumlog") == 0 || strcmp(display.type, "logicwind") == 0) {
    // Stop immediately if raw speed is 0
    if (currentSpeed < 0.01f && lastFreq != 0) {
      ledcWrite(LEDC_CHANNEL, 0);
      lastFreq = 0;
      Serial.println("Display stopped (speed=0)");
      return;
    }
    
    // Pulse calculation (same for both types)
    float freq = currentSpeed * display.sumlogK;
    if (freq > (float)display.sumlogFmax) freq = (float)display.sumlogFmax;
    
    if (freq < 0.01f) {
      // Stop PWM when frequency too low
      if (lastFreq != 0) {
        ledcWrite(LEDC_CHANNEL, 0);
        lastFreq = 0;
        Serial.println("Display stopped (freq too low)");
      }
    } else {
      // Round frequency to reduce jitter
      uint32_t freqInt = (uint32_t)(freq + 0.5f);
      
      // Only update if frequency actually changed
      if (freqInt != lastFreq) {
        if (freqInt == 0) {
          ledcWrite(LEDC_CHANNEL, 0);
          lastFreq = 0;
          Serial.println("Display stopped (0Hz avoided)");
        } else {
          // Calculate duty cycle (0-1023 for 10-bit resolution)
          uint32_t duty = (uint32_t)((1023 * display.pulseDuty) / 100);
          
          // Set frequency and duty cycle
          ledcChangeFrequency(LEDC_CHANNEL, freqInt, LEDC_TIMER_RESOLUTION);
          ledcWrite(LEDC_CHANNEL, duty);
          
          lastFreq = freqInt;
          Serial.printf("Display freq=%uHz (speed=%.1f kn)\n", freqInt, currentSpeed);
        }
      }
    }
  } else {
    // Unknown type - no pulse, stop PWM
    if (lastFreq != 0) {
      ledcWrite(LEDC_CHANNEL, 0);
      lastFreq = 0;
    }
  }
}
