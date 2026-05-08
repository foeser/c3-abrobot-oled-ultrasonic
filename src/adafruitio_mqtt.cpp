#include "adafruitio_mqtt.h"

#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif

#ifndef AIO_USERNAME
#define AIO_USERNAME ""
#endif

#ifndef AIO_KEY
#define AIO_KEY ""
#endif

#ifndef AIO_FEED_DISTANCE_CM
#define AIO_FEED_DISTANCE_CM "distance-cm"
#endif

namespace {
static constexpr const char *AIO_MQTT_HOST = "io.adafruit.com";
static constexpr uint16_t AIO_MQTT_PORT = 1883;
}

AdafruitIoMqtt::AdafruitIoMqtt()
	: m_wifiClient()
	, m_mqtt(m_wifiClient)
{
}

void AdafruitIoMqtt::begin()
{
	if (!shouldEnable())
	{
		Serial.println("Adafruit IO MQTT: disabled (missing AIO credentials and/or WiFi SSID).");
		return;
	}

	Serial.println("Adafruit IO MQTT: enabled.");
	m_mqtt.setKeepAlive(30);
	m_mqtt.setServer(AIO_MQTT_HOST, AIO_MQTT_PORT);
}

void AdafruitIoMqtt::loop()
{
	if (!shouldEnable())
		return;

	ensureConnected();
	m_mqtt.loop();
}

void AdafruitIoMqtt::setStatusLogIntervalMs(uint32_t intervalMs)
{
	m_statusLogIntervalMs = intervalMs;
	// log soon after enabling (without forcing an immediate print in begin())
	m_lastStatusLogAtMs = 0;
}

bool AdafruitIoMqtt::publishDistanceCm(float distanceCm, bool retained)
{
	if (!shouldEnable())
		return false;

	if (!m_mqtt.connected())
		return false;

	char payload[16];
	snprintf(payload, sizeof(payload), "%.1f", distanceCm);

	const String topic = distanceTopic();
	const bool ok = m_mqtt.publish(topic.c_str(), payload, retained);

	Serial.print("AIO publish: ");
	Serial.print(topic);
	Serial.print(" => ");
	Serial.print(payload);
	Serial.print(" (ok=");
	Serial.print(ok ? "yes" : "no");
	Serial.println(")");

	return ok;
}

bool AdafruitIoMqtt::hasStaCredentials() const
{
	return (WIFI_SSID[0] != '\0');
}

bool AdafruitIoMqtt::hasAioCredentials() const
{
	return (AIO_USERNAME[0] != '\0' && AIO_KEY[0] != '\0');
}

bool AdafruitIoMqtt::shouldEnable() const
{
	return hasStaCredentials() && hasAioCredentials();
}

String AdafruitIoMqtt::distanceTopic() const
{
	String t;
	t.reserve(64);
	t += AIO_USERNAME;
	t += "/feeds/";
	t += AIO_FEED_DISTANCE_CM;
	return t;
}

void AdafruitIoMqtt::ensureConnected()
{
	// Optional periodic status logging (heartbeat) consolidated here.
	if (m_statusLogIntervalMs != 0)
	{
		const uint32_t nowMs = millis();
		if (m_lastStatusLogAtMs == 0 || (nowMs - m_lastStatusLogAtMs) >= m_statusLogIntervalMs)
		{
			m_lastStatusLogAtMs = nowMs;
			Serial.print("Adafruit IO MQTT connected: ");
			Serial.println(m_mqtt.connected() ? "yes" : "no");
		}
	}

	if (WiFi.status() != WL_CONNECTED)
		return;

	if (m_mqtt.connected())
		return;

	const uint32_t nowMs = millis();
	if (nowMs - m_lastConnectAttemptMs < 5000)
		return;
	m_lastConnectAttemptMs = nowMs;

	String clientId = String("tankmonitor-") + WiFi.macAddress();

	Serial.print("Adafruit IO MQTT: connecting as ");
	Serial.println(clientId);

	const bool ok = m_mqtt.connect(clientId.c_str(), AIO_USERNAME, AIO_KEY);
	Serial.print("Adafruit IO MQTT connected: ");
	Serial.println(ok ? "yes" : "no");

	if (!ok)
	{
		Serial.print("MQTT state: ");
		Serial.println(m_mqtt.state());
	}
}