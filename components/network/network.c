#include "network.h"

#include <esp_event.h>
#include <esp_log.h>
#include <esp_smartconfig.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <netdb.h>
#include <nvs_flash.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <tcpip_adapter.h>

#include "dht/dht.h"

static EventGroupHandle_t wifi_event_group;

static const int CONNECTED_BIT = BIT0;
static const int ESPTOUCH_DONE_BIT = BIT1;
static const char* TAG = "network";

static bool is_connected = false;
static uint8_t m_connected_ssid[33];

static void smartconfig_task(void* parm);

static void event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT) {
        if (event_id == WIFI_EVENT_STA_START) {
            xTaskCreate(smartconfig_task, "smartconfig_task",
                        4096, NULL, 3, NULL);
        } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
            esp_wifi_connect();
            xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
    } else if (event_base == SC_EVENT) {
        if (event_id == SC_EVENT_SCAN_DONE) {
            ESP_LOGI(TAG, "Scan done");
        } else if (event_id == SC_EVENT_FOUND_CHANNEL) {
            ESP_LOGI(TAG, "Found channel");
        } else if (event_id == SC_EVENT_GOT_SSID_PSWD) {
            ESP_LOGI(TAG, "Got SSID and password");

            smartconfig_event_got_ssid_pswd_t* evt =
                (smartconfig_event_got_ssid_pswd_t*)event_data;
            wifi_config_t wifi_config;

            bzero(&wifi_config, sizeof(wifi_config_t));
            memcpy(wifi_config.sta.ssid, evt->ssid,
                   sizeof(wifi_config.sta.ssid));
            memcpy(wifi_config.sta.password, evt->password,
                   sizeof(wifi_config.sta.password));

            ESP_LOGI(TAG, "SSID: %s", wifi_config.sta.ssid);
            memcpy(m_connected_ssid, wifi_config.sta.ssid,
                   sizeof(wifi_config.sta.ssid));
            ESP_LOGI(TAG, "PASSWORD: %s", wifi_config.sta.password);

            ESP_ERROR_CHECK(esp_wifi_disconnect());
            ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
            ESP_ERROR_CHECK(esp_wifi_connect());
        } else if (event_id == SC_EVENT_SEND_ACK_DONE) {
            xEventGroupSetBits(wifi_event_group, ESPTOUCH_DONE_BIT);
        }
    }
}

static void smartconfig_task(void* parm) {
    ESP_LOGI(TAG, "Smartconfig task started");
    EventBits_t uxBits;

    ESP_ERROR_CHECK(esp_smartconfig_set_type(SC_TYPE_ESPTOUCH));

    smartconfig_start_config_t cfg = SMARTCONFIG_START_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_smartconfig_start(&cfg));

    while (1) {
        uxBits = xEventGroupWaitBits(wifi_event_group,
                                     CONNECTED_BIT | ESPTOUCH_DONE_BIT, true,
                                     false, portMAX_DELAY);
        if (uxBits & CONNECTED_BIT) {
            ESP_LOGI(TAG, "WiFi Connected to ap");
            is_connected = true;
        }
        if (uxBits & ESPTOUCH_DONE_BIT) {
            ESP_LOGI(TAG, "smartconfig over");
            esp_smartconfig_stop();
            vTaskDelete(NULL);
        }
    }
}

void network_start(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                               &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                               &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(SC_EVENT, ESP_EVENT_ANY_ID,
                                               &event_handler, NULL));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
}
void network_push_data_to_server() {
    const char* server = CONFIG_API_SERVER_URL;
    const char* port = "80";
    const char* json_data = "{\"temperature\":%d,\"humidity\":%d}";
    const char* request_template =
        "POST /data/create HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %d\r\n"
        "\r\n"
        "%s";

    char request[512];
    char body[64];
    int16_t temperature = dht_get_last_temp() / 10;
    int16_t humidity = dht_get_last_humid() / 10;
    snprintf(body, sizeof(body), json_data, temperature, humidity);
    snprintf(request, sizeof(request), request_template, server, strlen(body),
             body);

    ESP_LOGI(TAG, "request: %s", request);

    struct addrinfo hints = {0}, *res;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(server, port, &hints, &res) != 0) {
        ESP_LOGE(TAG, "DNS lookup failed\n");
        return;
    }

    int sock = socket(res->ai_family, res->ai_socktype, 0);
    if (sock < 0) {
        ESP_LOGE(TAG, "Socket creation failed\n");
        freeaddrinfo(res);
        return;
    }

    if (connect(sock, res->ai_addr, res->ai_addrlen) != 0) {
        ESP_LOGE(TAG, "Connection failed\n");
        close(sock);
        freeaddrinfo(res);
        return;
    }

    freeaddrinfo(res);

    write(sock, request, strlen(request));

    char response[512];
    int len = read(sock, response, sizeof(response) - 1);
    if (len > 0) {
        response[len] = 0;
        ESP_LOGI(TAG, "Server response: %s\n", response);
    }

    close(sock);
    ESP_LOGI(TAG, "data sent to server");
}

bool network_is_connected(void) { return is_connected; }

uint8_t* network_get_ap_name(void) { return m_connected_ssid; }
