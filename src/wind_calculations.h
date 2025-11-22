#pragma once

#include <Arduino.h>
#include "web_ui.h"

/* ========= Wind Calculations Module ========= */
// Handles True Wind, VMG, and other wind-related calculations

// Check if NMEA sentence matches display's data type
bool sentenceMatchesDataType(const char* sentenceType, char reference, uint8_t dataType);

// Update displays that match the given sentence
void updateDisplaysForSentence(const char* sentenceType, char reference, int angle, float speed, bool hasSpeed);

// Calculate True Wind from Apparent Wind + GPS data
bool calculateTrueWind(float& trueSpeed, float& trueAngle);

// Calculate VMG (Velocity Made Good)
bool calculateVMG(float& vmg);

// Helper: wrap angle to 0-360 range
int wrap360(int deg);
