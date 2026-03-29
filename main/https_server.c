#include "https_server.h"
#include "intercom_audio.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "esp_https_server.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_system.h"

#if !CONFIG_HTTPD_WS_SUPPORT
#error HTTPD_WS_SUPPORT must be enabled in menuconfig
#endif

static const char *TAG = "HTTPS_SERVER";
static httpd_handle_t s_server = NULL;

extern const unsigned char servercert_pem_start[] asm("_binary_servercert_pem_start");
extern const unsigned char servercert_pem_end[]   asm("_binary_servercert_pem_end");

extern const unsigned char prvtkey_pem_start[] asm("_binary_prvtkey_pem_start");
extern const unsigned char prvtkey_pem_end[]   asm("_binary_prvtkey_pem_end");

/* ===== Commands from PWA ===== */

static void cmd_toggle_lock(void)
{
    ESP_LOGI(TAG, "cmd_toggle_lock()");
}

static void cmd_save_schedule(const char *msg)
{
    int from_minutes = -1;
    int to_minutes = -1;

    int matched = sscanf(
        msg,
        "{\"type\":\"save_schedule\",\"span\":\"%d,%d\"}",
        &from_minutes,
        &to_minutes
    );

    if (matched == 2) {
        ESP_LOGI(TAG, "cmd_save_schedule(): from=%d, to=%d", from_minutes, to_minutes);
    } else {
        ESP_LOGW(TAG, "cmd_save_schedule(): parse failed: %s", msg);
    }
}

static void cmd_test_microphone(void)
{
    ESP_LOGI(TAG, "cmd_test_microphone()");
}

static void cmd_start_talk(const char *msg)
{
    int sample_rate = 0;
    int channels = 0;
    char format[32] = {0};

    int matched = sscanf(
        msg,
        "{\"type\":\"start_talk\",\"sampleRate\":%d,\"channels\":%d,\"format\":\"%31[^\"]\"}",
        &sample_rate,
        &channels,
        format
    );

    if (matched == 3) {
        ESP_LOGI(TAG, "cmd_start_talk(): sample_rate=%d channels=%d format=%s",
                 sample_rate, channels, format);
    } else {
        ESP_LOGW(TAG, "cmd_start_talk(): parse failed: %s", msg);
    }

    esp_err_t err = intercom_audio_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "intercom_audio_start failed: %s", esp_err_to_name(err));
    }
}

static void cmd_stop_talk(void)
{
    ESP_LOGI(TAG, "cmd_stop_talk()");

    esp_err_t err = intercom_audio_stop();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "intercom_audio_stop failed: %s", esp_err_to_name(err));
    }
}

/* ===== HTTP root page ===== */

static esp_err_t root_get_handler(httpd_req_t *req)
{
    static const char html[] =
        "<!DOCTYPE html>"
        "<html>"
        "<head><title>ESP HTTPS/WSS</title></head>"
        "<body>"
        "<h1>Hello Secure World!</h1>"
        "<button onclick='connectWs()'>Connect WS</button>"
        "<button onclick='sendMsg()'>Send hello</button>"
        "<pre id='log'></pre>"
        "<script>"
        "let ws = null;"
        "function log(s){document.getElementById('log').textContent += s + '\\n';}"
        "function connectWs(){"
        "  ws = new WebSocket('wss://' + window.location.host + '/ws');"
        "  ws.onopen = () => log('WS connected');"
        "  ws.onmessage = (event) => log('WS message: ' + event.data);"
        "  ws.onclose = () => log('WS closed');"
        "  ws.onerror = () => log('WS error');"
        "}"
        "function sendMsg(){"
        "  if (!ws) { log('WS not connected'); return; }"
        "  ws.send('hello from browser');"
        "}"
        "</script>"
        "</body>"
        "</html>";

    ESP_LOGI(TAG, "HTTP GET /");
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
}

/* ===== WebSocket handler ===== */

static esp_err_t ws_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        ESP_LOGI(TAG, "WebSocket handshake completed");
        return ESP_OK;
    }

    httpd_ws_frame_t ws_pkt = {0};
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "httpd_ws_recv_frame(get len) failed: %s", esp_err_to_name(ret));
        intercom_audio_stop();
        return ret;
    }

    if (ws_pkt.len == 0) {
        return ESP_OK;
    }

    uint8_t buf[1024] = {0};

    if (ws_pkt.len > sizeof(buf) - 1) {
        ESP_LOGW(TAG, "WS frame too large: %d", ws_pkt.len);
        return ESP_ERR_INVALID_SIZE;
    }

    ws_pkt.payload = buf;
    ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "httpd_ws_recv_frame(payload) failed: %s", esp_err_to_name(ret));
        intercom_audio_stop();
        return ret;
    }

    if (ws_pkt.type == HTTPD_WS_TYPE_TEXT) {
        const char *msg = (const char *)ws_pkt.payload;
        ESP_LOGI(TAG, "WS text: %s", msg);

        if (strstr(msg, "\"type\":\"toggle_lock\"")) {
            cmd_toggle_lock();
        } else if (strstr(msg, "\"type\":\"test_microphone\"")) {
            cmd_test_microphone();
        } else if (strstr(msg, "\"type\":\"save_schedule\"")) {
            cmd_save_schedule(msg);
        } else if (strstr(msg, "\"type\":\"start_talk\"")) {
            cmd_start_talk(msg);
        } else if (strstr(msg, "\"type\":\"stop_talk\"")) {
            cmd_stop_talk();
        } else {
            ESP_LOGW(TAG, "Unknown WS text message: %s", msg);
        }

    } else if (ws_pkt.type == HTTPD_WS_TYPE_BINARY) {
        static uint32_t s_chunk_count = 0;
        s_chunk_count++;

        if ((s_chunk_count % 50) == 0) {
            ESP_LOGI(TAG, "WS audio chunks received: %lu",
                     (unsigned long)s_chunk_count);
        }

        esp_err_t err = intercom_audio_enqueue(ws_pkt.payload, ws_pkt.len);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "intercom_audio_enqueue failed: %s", esp_err_to_name(err));
        }

    } else if (ws_pkt.type == HTTPD_WS_TYPE_PING) {
        ESP_LOGD(TAG, "Received WS PING");

    } else if (ws_pkt.type == HTTPD_WS_TYPE_PONG) {
        ESP_LOGD(TAG, "Received WS PONG");

    } else if (ws_pkt.type == HTTPD_WS_TYPE_CLOSE) {
        ESP_LOGI(TAG, "Received WS CLOSE");
        intercom_audio_stop();
    }

    return ESP_OK;
}

/* ===== URI table ===== */

static const httpd_uri_t root = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = root_get_handler,
    .user_ctx = NULL
};

static const httpd_uri_t ws = {
    .uri = "/ws",
    .method = HTTP_GET,
    .handler = ws_handler,
    .user_ctx = NULL,
    .is_websocket = true,
    .handle_ws_control_frames = true
};

/* ===== Server lifecycle ===== */

esp_err_t https_server_start(void)
{
    ESP_LOGI(TAG, "Free heap before HTTPS start: %u", (unsigned)esp_get_free_heap_size());
    ESP_LOGI(TAG, "Min free heap before HTTPS start: %u", (unsigned)esp_get_minimum_free_heap_size());
    ESP_LOGI(TAG, "Free internal heap before HTTPS start: %u", (unsigned)esp_get_free_internal_heap_size());

    if (s_server != NULL) {
        ESP_LOGI(TAG, "HTTPS/WSS server already running");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Starting HTTPS/WSS server");

    httpd_ssl_config_t conf = HTTPD_SSL_CONFIG_DEFAULT();
    conf.httpd.max_req_hdr_len = 2048;
    conf.httpd.max_open_sockets = 4;

    conf.servercert = servercert_pem_start;
    conf.servercert_len = servercert_pem_end - servercert_pem_start;

    conf.prvtkey_pem = prvtkey_pem_start;
    conf.prvtkey_len = prvtkey_pem_end - prvtkey_pem_start;

    esp_err_t ret = httpd_ssl_start(&s_server, &conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "httpd_ssl_start failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = httpd_register_uri_handler(s_server, &root);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register root URI: %s", esp_err_to_name(ret));
        httpd_ssl_stop(s_server);
        s_server = NULL;
        return ret;
    }

    ret = httpd_register_uri_handler(s_server, &ws);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register WS URI: %s", esp_err_to_name(ret));
        httpd_ssl_stop(s_server);
        s_server = NULL;
        return ret;
    }

    ESP_LOGI(TAG, "HTTPS/WSS server started");
    return ESP_OK;
}

esp_err_t https_server_stop(void)
{
    if (s_server == NULL) {
        return ESP_OK;
    }

    esp_err_t ret = httpd_ssl_stop(s_server);
    if (ret == ESP_OK) {
        s_server = NULL;
    }
    return ret;
}

httpd_handle_t https_server_get_handle(void)
{
    return s_server;
}
