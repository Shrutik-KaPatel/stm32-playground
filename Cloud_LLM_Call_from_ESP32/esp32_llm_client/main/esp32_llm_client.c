#include <stdio.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "cJSON.h"

/* Pull WiFi creds and the Gemini key in from Kconfig (sdkconfig), never hardcoded here */
#define WIFI_SSID CONFIG_WIFI_SSID
#define WIFI_PASSWORD CONFIG_WIFI_PASSWORD
#define GEMINI_API_KEY CONFIG_GEMINI_API_KEY

static const char *TAG = "llm_client";

/* Event group lets app_main block until WiFi is actually connected, instead of guessing with a delay */
static EventGroupHandle_t wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0

/* Buffer to collect the HTTP response body as it streams in via the event handler */
static char response_buffer[4096];
static int response_len = 0;

/* Fires on WiFi/IP events. Reconnects automatically on disconnect, and signals
 * the event group once we actually have an IP address. */
static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "Disconnected, retrying...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

/* Brings up WiFi station mode and blocks until connected.
 * Same event-driven pattern as ESP32_to_Cloud_HTTP, not a fixed delay. */
static void wifi_init(void)
{
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Waiting for WiFi connection...");
    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
    ESP_LOGI(TAG, "WiFi connected");
}

/* Called repeatedly by esp_http_client as the response arrives in chunks.
 * We only care about HTTP_EVENT_ON_DATA: copy each chunk into response_buffer
 * until we've assembled the full body. */
static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    if (evt->event_id == HTTP_EVENT_ON_DATA) {
        if (response_len + evt->data_len < sizeof(response_buffer)) {
            memcpy(response_buffer + response_len, evt->data, evt->data_len);
            response_len += evt->data_len;
        }
    }
    return ESP_OK;
}

/* Builds the Gemini request body, sends it, and parses the reply.
 *
 * Request shape Gemini expects:
 *   { "contents": [ { "parts": [ { "text": "<prompt>" } ] } ] }
 *
 * Response shape we're pulling the answer out of:
 *   { "candidates": [ { "content": { "parts": [ { "text": "<answer>" } ] } } ] }
 */
static void call_gemini(const char *prompt)
{
    const char *url = "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash:generateContent";

    /* --- Build the JSON request body using cJSON --- */
    cJSON *root = cJSON_CreateObject();
    cJSON *contents = cJSON_AddArrayToObject(root, "contents");
    cJSON *content_obj = cJSON_CreateObject();
    cJSON *parts = cJSON_AddArrayToObject(content_obj, "parts");
    cJSON *part_obj = cJSON_CreateObject();
    cJSON_AddStringToObject(part_obj, "text", prompt);
    cJSON_AddItemToArray(parts, part_obj);
    cJSON_AddItemToArray(contents, content_obj);

    char *post_data = cJSON_PrintUnformatted(root);

    /* Reset the response buffer before each call */
    response_len = 0;
    memset(response_buffer, 0, sizeof(response_buffer));

    /* --- Configure and fire the HTTPS POST ---
     * crt_bundle_attach handles TLS cert validation the same way the
     * httpbin project did - no manual cert pinning needed. */
    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .event_handler = http_event_handler,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 15000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "x-goog-api-key", GEMINI_API_KEY);  /* Gemini's auth header */
    esp_http_client_set_post_field(client, post_data, strlen(post_data));

    esp_err_t err = esp_http_client_perform(client);

    /* --- Parse the response and pull out the generated text --- */
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP Status = %d", esp_http_client_get_status_code(client));

        cJSON *resp_json = cJSON_Parse(response_buffer);
        if (resp_json) {
            cJSON *candidates = cJSON_GetObjectItem(resp_json, "candidates");
            if (candidates && cJSON_GetArraySize(candidates) > 0) {
                cJSON *first = cJSON_GetArrayItem(candidates, 0);
                cJSON *content = cJSON_GetObjectItem(first, "content");
                cJSON *parts_arr = cJSON_GetObjectItem(content, "parts");
                if (parts_arr && cJSON_GetArraySize(parts_arr) > 0) {
                    cJSON *text = cJSON_GetObjectItem(cJSON_GetArrayItem(parts_arr, 0), "text");
                    if (text && cJSON_IsString(text)) {
                        ESP_LOGI(TAG, "Gemini says: %s", text->valuestring);
                    }
                }
            } else {
                /* No candidates usually means an error response (bad key, quota, etc.) - print raw to see why */
                ESP_LOGE(TAG, "No candidates in response, raw: %s", response_buffer);
            }
            cJSON_Delete(resp_json);
        } else {
            ESP_LOGE(TAG, "JSON parse failed, raw: %s", response_buffer);
        }
    } else {
        ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(err));
    }

    /* Clean up - client handle, the POST body string, and the cJSON tree we built */
    esp_http_client_cleanup(client);
    free(post_data);
    cJSON_Delete(root);
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    wifi_init();
    call_gemini("In one sentence, explain what vibration-based predictive maintenance is.");
}
