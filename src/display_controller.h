#pragma once

#include <Arduino.h>
#include "web_ui.h"

/* ========= Display Controller Module ========= */
// Handles pulse generation and DAC output for wind displays

// Start/stop display output
void startDisplay(int displayNum);
void stopDisplay(int displayNum);

// Update display pulse output
void updateDisplayPulse(int displayNum);
void updateAllDisplayPulses();

// Set DAC output for direction (Logic Wind only)
void setOutputsDeg(int degrees);

// Helper: clamp voltage to valid range
int mvClamp(int mv);
