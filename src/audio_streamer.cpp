#include "audio_streamer.h"
#include "logger.h"
#include "config.h"
#include "state_handler.h"
#include "ble_handler.h"
#include <driver/i2s.h>

TaskHandle_t g_audio_task_handle = NULL;

void audio_streaming_task(void *pvParameters) {
    log_message("Audio streaming task started");

    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 4,
        .dma_buf_len = 1024,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0
    };

    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_SCK_PIN,
        .ws_io_num = I2S_WS_PIN,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = I2S_SD_PIN
    };

    i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_NUM_0, &pin_config);

    size_t bytes_read;
    uint8_t buffer[1024];

    while (true) {
        if (get_current_state() != STATE_STREAMING_AUDIO_BLE) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        while (get_current_state() == STATE_STREAMING_AUDIO_BLE) {
            i2s_read(I2S_NUM_0, buffer, sizeof(buffer), &bytes_read, portMAX_DELAY);
            if (bytes_read > 0 && is_ble_connected()) {
                notify_audio_data(buffer, bytes_read);
            } else if (!is_ble_connected()) {
                log_message("BLE disconnected, stopping audio stream.");
                set_current_state(STATE_IDLE);
                break; 
            }
        }
    }
}

void start_audio_stream() {
    if (get_current_state() != STATE_CONNECTED_BLE) {
        log_message("Cannot start audio stream, not in CONNECTED_BLE state.");
        return;
    }
    log_message("Starting audio stream");
    set_current_state(STATE_STREAMING_AUDIO_BLE);

    if (g_audio_task_handle == NULL) {
        xTaskCreate(
            audio_streaming_task,
            "AudioStreamerTask",
            4096,
            NULL,
            5,
            &g_audio_task_handle
        );
    }
}

void stop_audio_stream() {
    if (get_current_state() != STATE_STREAMING_AUDIO_BLE) {
        log_message("Cannot stop audio stream, not in STREAMING_AUDIO_BLE state.");
        return;
    }
    log_message("Stopping audio stream");
    set_current_state(STATE_CONNECTED_BLE);
}
