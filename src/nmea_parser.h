#pragma once

#include <Arduino.h>

/* ========= NMEA Parser Module ========= */
// Handles parsing of NMEA 0183 sentences

// Validation functions
bool nmeaChecksumOK(const char* s);
bool hasFormatter(const char* s, const char* fmt3);
int splitCSV(char* line, char* fields[], int maxf);

// Sentence parsers (LITE: Apparent Wind only)
bool parseMWV(char* line);
bool parseVWR(char* line);

// Main parser dispatcher
bool parseNMEALine(char* line);
