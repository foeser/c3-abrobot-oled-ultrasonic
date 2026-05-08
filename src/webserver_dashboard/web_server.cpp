#include "web_server.h"

#include <WebServer.h>
#include <WiFi.h>
#include <esp_heap_caps.h>

#include "web_dashboard_html.h"

namespace {
	// Synchronous HTTP server (Arduino-ESP32 WebServer)
	WebServer g_server(80);
	MeasurementLog *g_log = nullptr;

	// Serve the single-page dashboard (offline HTML/CSS/JS)
	// send_P() is used so the (large) HTML string is served directly from flash (PROGMEM), not copied into RAM
	void handleIndex()
	{
		g_server.send_P(200, "text/html; charset=utf-8", WEB_DASHBOARD_HTML);
	}

	// Heap/fragmentation status endpoint
	// Observe RAM headroom and fragmentation (largest free block) for JSON-building safety
	void handleStatus()
	{
		const uint32_t freeHeap = static_cast<uint32_t>(ESP.getFreeHeap());
		const uint32_t largestFreeBlock8 =
			static_cast<uint32_t>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));

		String response;
		response.reserve(140);

		response += "{\"ok\":true,\"heap\":{\"freeHeap\":";
		response += String(freeHeap);
		response += ",\"largestFreeBlock8\":";
		response += String(largestFreeBlock8);
		response += "}}";

		g_server.send(200, "application/json", response);
	}

	// Returns JSON for the dashboard:
	// - sampleCount: count of stored samples
	// - storage: filesystem usage info
	// - samples: array of samples with uptimeSeconds + distanceCm
	// We build a complete JSON String and send it (not streamed) to avoid issues seen with mixed
	// "chunked" responses and manual WiFiClient writes (which can lead to browser fetch failures / resets).
	void handleMeasurements()
	{
		if (g_log == nullptr)
		{
			g_server.send(500, "application/json", "{\"error\":\"server not initialized\"}");
			return;
		}

		const String samplesJson = g_log->readJson();

		String response;
		response.reserve(samplesJson.length() + 140);

		response += "{\"sampleCount\":";
		response += String(g_log->sampleCount());
		response += ",\"storage\":{\"totalBytes\":";
		response += String(g_log->totalBytes());
		response += ",\"usedBytes\":";
		response += String(g_log->usedBytes());
		response += ",\"freeBytes\":";
		response += String(g_log->freeBytes());
		response += "},\"samples\":";
		response += samplesJson;
		response += "}";

		g_server.send(200, "application/json", response);
	}

	// Clears the stored measurement file. The dashboard calls this after user confirmation
	void handleClear()
	{
		if (g_log == nullptr)
		{
			g_server.send(500, "application/json", "{\"ok\":false,\"error\":\"server not initialized\"}");
			return;
		}

		if (!g_log->clear())
		{
			g_server.send(500, "application/json", "{\"ok\":false,\"error\":\"failed to clear log\"}");
			return;
		}

		g_server.send(200, "application/json", "{\"ok\":true}");
	}

	void handleNotFound()
	{
		g_server.send(404, "text/plain; charset=utf-8", "Not found");
	}
}

void webServerBegin(const char *ssid,
		    const char *password,
		    const IPAddress &ip,
		    const IPAddress &gateway,
		    const IPAddress &subnet,
		    MeasurementLog &log)
{
	g_log = &log;

	// Important: do not set WiFi.mode() here; main() decides between WIFI_AP and WIFI_AP_STA.
	WiFi.softAPConfig(ip, gateway, subnet);
	WiFi.softAP(ssid, password);

	Serial.println("AP started");
	Serial.print("AP IP: ");
	Serial.println(WiFi.softAPIP());

	g_server.on("/", HTTP_GET, handleIndex);
	g_server.on("/api/status", HTTP_GET, handleStatus);
	g_server.on("/api/measurements", HTTP_GET, handleMeasurements);
	g_server.on("/api/clear", HTTP_POST, handleClear);
	g_server.onNotFound(handleNotFound);

	g_server.begin();
	Serial.println("WebServer: started");
}

void webServerLoop()
{
	g_server.handleClient();
}