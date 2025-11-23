#pragma once

#include <Arduino.h>

/* ========= Wind Calculations Module ========= */
// LITE Multi: True Wind and VMG calculations

// Helper functions
int wrap360(int deg);

// Calculate True Wind from Apparent Wind + GPS
// Uses HDM/HDT if available, falls back to COG
bool calculateTrueWind(float& trueSpeed, float& trueAngle);

// Calculate VMG (Velocity Made Good towards wind)
// VMG = SOG × cos(TWA)
// Uses HDM/HDT if available, falls back to COG
bool calculateVMG(float& vmg);

// Update all calculations (call periodically)
void updateCalculations();
