#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>

class AdafruitIoMqtt
{
public:
    AdafruitIoMqtt();

    void begin();
    void loop();
    bool publishDistanceCm(float distanceCm, bool retained = true);

    // Optional periodic status logging (Serial)
    void setStatusLogIntervalMs(uint32_t intervalMs);

private:
    bool hasStaCredentials() const;
    bool hasAioCredentials() const;
    bool shouldEnable() const;

    void ensureConnected();
    String distanceTopic() const;

private:
    WiFiClient m_wifiClient;
    PubSubClient m_mqtt;

    uint32_t m_lastConnectAttemptMs = 0;

    uint32_t m_statusLogIntervalMs = 0; // 0 disables
    uint32_t m_lastStatusLogAtMs = 0;
};