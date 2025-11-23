#pragma once

#include <Arduino.h>
#include "web_ui.h"

/* ========= Display Controller Module ========= */
// Handles pulse generation and DAC output for wind displays
// LITE Multi: 3 displays with global direction

// Start/stop display output
void startDisplay(int displayNum);
void stopDisplay(int displayNum);

// Update global direction from selected source
void updateGlobalDirection();

// Update display speed from selected source
void updateDisplaySpeed(int displayNum);

// Update display pulse output
void updateDisplayPulse(int displayNum);

// Set DAC output for global direction (Logic Wind only)
void setOutputsDeg(int degrees);

// Helper: clamp voltage to valid range
int mvClamp(int mv);
