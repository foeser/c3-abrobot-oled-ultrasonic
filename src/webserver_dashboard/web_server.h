#pragma once

#include <Arduino.h>
#include <IPAddress.h>

#include "../measurement_log.h"

// Start SoftAP + HTTP server and register dashboard + API endpoints
//
// Endpoints:
// - GET  /                 : offline dashboard HTML (embedded, no external CDN)
// - GET  /api/measurements : JSON with stored samples + storage stats
// - POST /api/clear        : clears stored measurement log
//
// Note: Server is synchronous; webServerLoop() must be called frequently to stay responsive
void webServerBegin(const char *ssid,
                    const char *password,
                    const IPAddress &ip,
                    const IPAddress &gateway,
                    const IPAddress &subnet,
                    MeasurementLog &log);

// Handle client requests
void webServerLoop();