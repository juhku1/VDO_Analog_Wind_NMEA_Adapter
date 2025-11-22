#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>

/* ========= Network Manager Module ========= */
// Handles TCP/UDP connections and NMEA data polling

// TCP connection management
void ensureTCPConnected();
void pollTCP();

// UDP connection management
void ensureUDPBound();
void pollUDP();

// WiFi Station connection
void connectSTA();

// Bind network transport based on configuration
void bindTransport();

// FreeRTOS task for NMEA polling (runs on Core 1)
void nmeaPollTaskFunc(void* param);
