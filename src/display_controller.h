#pragma once

#include <Arduino.h>
#include "web_ui.h"

/* ========= Display Controller Module ========= */
// Handles pulse generation and DAC output for wind displays
// LITE: Single display only

// Start/stop display output
void startDisplay();
void stopDisplay();

// Update display data from selected source (LITE Plus)
void updateDisplayData();

// Update display pulse output
void updateDisplayPulse();

// Set DAC output for direction (Logic Wind only)
void setOutputsDeg(int degrees);

// Helper: clamp voltage to valid range
int mvClamp(int mv);
