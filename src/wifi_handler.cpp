#include "wifi_handler.h"
#include "config.h"
#include "logger.h"
#include "state_handler.h"
#include "camera_handler.h"
#include <WiFi.h>
#include <NimBLEDevice.h>
#include "ble_handler.h"
#include <ESPAsyncWebServer.h>

AsyncWebServer server(80);
AsyncWebSocket ws("/audio");

void send_audio_data_to_wifi_clients(const uint8_t* data, size_t len) {
    ws.binaryAll(data, len);
}

void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len){
  //Handle WebSocket events
}

#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;

void stream_video(void *pvParameters) {
    camera_fb_t *fb = NULL;
    while (true) {
        if (get_current_state() == STATE_STREAMING_AV_WIFI) {
            fb = esp_camera_fb_get();
            if (!fb) {
                log_message("Camera capture failed");
                vTaskDelay(1000);
                continue;
            }

            // Here we would send the frame to all connected clients
            // This part is complex and will be implemented next.
            // For now, we just release the buffer.

            esp_camera_fb_return(fb);
        }
        vTaskDelay(pdMS_TO_TICKS(80)); // ~12.5 FPS
    }
}

void start_wifi_hotspot() {
    set_current_state(STATE_WIFI_MODE);
    log_message("Starting Wi-Fi hotspot...");

    // De-initialize BLE to fully free the radio for Wi-Fi
    log_message("[WIFI] De-initializing BLE stack...");
    NimBLEDevice::deinit();
    vTaskDelay(pdMS_TO_TICKS(100)); // Allow time for de-init to complete

    if (!WiFi.softAP(WIFI_SSID, WIFI_PASSWORD)) {
        log_message("[WIFI] ERROR: Failed to start Soft AP!");
        set_current_state(STATE_ERROR);
        return;
    }
    IPAddress myIP = WiFi.softAPIP();
    log_message("AP IP address: %s", myIP.toString().c_str());

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", "Hello, from OpenGlasses!");
    });

    server.on("/stream", HTTP_GET, [](AsyncWebServerRequest *request){
        AsyncResponseStream *response = request->beginResponseStream(_STREAM_CONTENT_TYPE);
        
        // This is a simplified version. A real implementation would handle multiple clients
        // and stream frames in a separate task.
        while(true){
            camera_fb_t * fb = esp_camera_fb_get();
            if (!fb) {
                log_message("Camera capture failed");
                // request->send(500, "text/plain", "Camera capture failed");
                break;
            }
            
            response->print("--");
            response->print(PART_BOUNDARY);
            response->print("\r\n");
            response->print("Content-Type: image/jpeg\r\n");
            response->print("Content-Length: ");
            response->print(fb->len);
            response->print("\r\n\r\n");
            
            response->write(fb->buf, fb->len);
            
            response->print("\r\n");
            esp_camera_fb_return(fb);
            
            vTaskDelay(pdMS_TO_TICKS(80)); // Delay between frames
        }
        request->send(response);
    });

    ws.onEvent(onWsEvent);
    server.addHandler(&ws);

    server.begin();
    log_message("Web server started.");
    set_current_state(STATE_STREAMING_AV_WIFI);
}

void stop_wifi_hotspot() {
    log_message("Stopping Wi-Fi hotspot...");
    ws.closeAll();
    WiFi.softAPdisconnect(true);
    server.end();
    log_message("Wi-Fi hotspot stopped.");

    // Re-initialize BLE to allow connections again
    log_message("[WIFI] Re-initializing BLE stack...");
    configure_ble();

    set_current_state(STATE_IDLE);
}
