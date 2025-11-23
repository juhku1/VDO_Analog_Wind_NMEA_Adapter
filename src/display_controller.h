#pragma once

#include <Arduino.h>
#include "web_ui.h"

/* ========= Speed Pulse Controller Module ========= */
// Handles pulse generation and DAC output for wind instruments
// LITE Multi: 3 speed pulse outputs with shared direction

// Start/stop speed pulse output
void startSpeedPulse(int pulseNum);
void stopSpeedPulse(int pulseNum);

// Update direction output from selected source
void updateDirectionOutput();

// Update speed pulse from selected source
void updateSpeedPulseSpeed(int pulseNum);

// Update speed pulse frequency
void updateSpeedPulse(int pulseNum);

// Set DAC output for direction (shared by all Logic Wind instruments)
void setDirectionOutput(int degrees);

// Helper: clamp voltage to valid range
int mvClamp(int mv);
