#include <stdint.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_twai.h"
#include "esp_twai_onchip.h"

#define TX_PIN CONFIG_EXAMPLE_TWAI_TX_GPIO
#define RX_PIN CONFIG_EXAMPLE_TWAI_RX_GPIO
#define REQUEST_ID 0x200
#define RESPONSE_ID 0x201

static const char *TAG = "can_test";
static QueueHandle_t rx_queue;

typedef struct {
    uint32_t id;
    size_t len;
    uint8_t bytes[8];
} rx_message_t;

static void log_received(const rx_message_t *msg)
{
    char data[3 * 8 + 1] = {0};
    for (size_t i = 0; i < msg->len && i < sizeof(msg->bytes); ++i) {
        snprintf(data + i * 3, sizeof(data) - i * 3, "%02X ", msg->bytes[i]);
    }
    ESP_LOGI(TAG, "RX ID=0x%03lX DLC=%u data=%s",
             (unsigned long)msg->id, (unsigned)msg->len, data);
}

static bool IRAM_ATTR on_rx(twai_node_handle_t node,
                            const twai_rx_done_event_data_t *event, void *context)
{
    (void)event;
    (void)context;
    rx_message_t msg = {0};
    twai_frame_t frame = {.buffer = msg.bytes, .buffer_len = sizeof(msg.bytes)};
    if (twai_node_receive_from_isr(node, &frame) != ESP_OK) return false;
    msg.id = frame.header.id;
    msg.len = twaifd_dlc2len(frame.header.dlc);
    BaseType_t woken = pdFALSE;
    xQueueSendFromISR(rx_queue, &msg, &woken);
    return woken == pdTRUE;
}

void app_main(void)
{
    rx_queue = xQueueCreate(8, sizeof(rx_message_t));
    if (!rx_queue) { ESP_LOGE(TAG, "RX queue allocation failed"); return; }
    twai_onchip_node_config_t config = {
        .io_cfg = {.tx = TX_PIN, .rx = RX_PIN, .quanta_clk_out = -1,
                   .bus_off_indicator = -1},
        .bit_timing = {.bitrate = 125000},
        .tx_queue_depth = 4,
        .fail_retry_cnt = 3,
    };
    twai_node_handle_t node = NULL;
    ESP_ERROR_CHECK(twai_new_node_onchip(&config, &node));
    twai_event_callbacks_t callbacks = {.on_rx_done = on_rx};
    ESP_ERROR_CHECK(twai_node_register_event_callbacks(node, &callbacks, NULL));
    ESP_ERROR_CHECK(twai_node_enable(node));
    ESP_LOGI(TAG, "CAN 125 kbit/s TX=GPIO%d RX=GPIO%d", TX_PIN, RX_PIN);

    while (true) {
        twai_node_status_t status;
        if (twai_node_get_info(node, &status, NULL) == ESP_OK &&
            status.state == TWAI_ERROR_BUS_OFF) {
            ESP_LOGW(TAG, "Bus-off: check wiring, termination and bitrate");
            twai_node_recover(node);
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        uint8_t payload[8];
        for (size_t i = 0; i < sizeof(payload); i += sizeof(uint32_t)) {
            uint32_t random = esp_random();
            for (size_t j = 0; j < sizeof(uint32_t); ++j) {
                payload[i + j] = (uint8_t)(random >> (8 * j));
            }
        }
        twai_frame_t frame = {.header.id = REQUEST_ID,
                              .buffer = payload, .buffer_len = sizeof(payload)};
        esp_err_t err = twai_node_transmit(node, &frame, 100);
        if (err == ESP_OK) err = twai_node_transmit_wait_all_done(node, 300);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "TX failed: %s", esp_err_to_name(err));
        } else {
            ESP_LOGI(TAG, "TX ID=0x%03X data=%02X %02X %02X %02X %02X %02X %02X %02X",
                     REQUEST_ID, payload[0], payload[1], payload[2], payload[3],
                     payload[4], payload[5], payload[6], payload[7]);
        }
        TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(1000);
        rx_message_t received;
        while ((int32_t)(deadline - xTaskGetTickCount()) > 0) {
            if (xQueueReceive(rx_queue, &received, deadline - xTaskGetTickCount()) == pdTRUE) {
                if (received.id == RESPONSE_ID) {
                    log_received(&received);
                }
            }
        }
    }
}
