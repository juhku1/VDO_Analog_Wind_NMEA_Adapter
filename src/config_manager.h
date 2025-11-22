#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include "web_ui.h"

/* ========= Config Manager Module ========= */
// Handles NVS (Non-Volatile Storage) operations for configuration

// Load/save all configuration
void loadConfig();
void saveAllConfig();

// Save individual display configuration
void saveDisplayConfig(int displayNum);

// Save network configuration
void saveNetworkConfig();
