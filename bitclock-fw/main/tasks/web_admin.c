#include "web_admin.h"

#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "libs/nvs.h"
#include "libs/ota.h"
#include "libs/sensor_utils.h"
#include "libs/version.h"
#include "mdns.h"
#include "tasks/scd4x.h"
#include "tasks/sgp4x.h"
#include "tasks/sht4x.h"
#include "tasks/weather.h"
#include "tasks/wifi.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "web_admin";

static httpd_handle_t server = NULL;

extern const uint8_t admin_html_start[] asm("_binary_admin_html_start");
extern const uint8_t admin_html_end[] asm("_binary_admin_html_end");
extern const uint8_t tz_json_start[] asm("_binary_tz_json_start");
extern const uint8_t tz_json_end[] asm("_binary_tz_json_end");

// Minimal URL form decode: %XX -> byte, '+' -> space.
static void uri_decode(char *dst, const char *src, size_t dst_len) {
  size_t di = 0;
  for (size_t si = 0; src[si] && di + 1 < dst_len; si++) {
    if (src[si] == '%' && src[si + 1] && src[si + 2]) {
      char hex[3] = {src[si + 1], src[si + 2], '\0'};
      dst[di++] = (char)strtol(hex, NULL, 16);
      si += 2;
    } else if (src[si] == '+') {
      dst[di++] = ' ';
    } else {
      dst[di++] = src[si];
    }
  }
  dst[di] = '\0';
}

// Extract a field from an x-www-form-urlencoded body into decoded `out`.
// Returns true if the key was present.
static bool form_field(const char *body, const char *key, char *out,
                       size_t out_len) {
  out[0] = '\0';
  size_t key_len = strlen(key);
  const char *p = body;
  while ((p = strstr(p, key)) != NULL) {
    // Must be at start or preceded by '&', and followed by '='.
    bool at_boundary = (p == body) || (p[-1] == '&');
    if (at_boundary && p[key_len] == '=') {
      const char *val = p + key_len + 1;
      const char *end = strchr(val, '&');
      size_t raw_len = end ? (size_t)(end - val) : strlen(val);
      char raw[256];
      if (raw_len >= sizeof(raw))
        raw_len = sizeof(raw) - 1;
      memcpy(raw, val, raw_len);
      raw[raw_len] = '\0';
      uri_decode(out, raw, out_len);
      return true;
    }
    p += key_len;
  }
  return false;
}

static esp_err_t root_get(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html");
  httpd_resp_send(req, (const char *)admin_html_start,
                  admin_html_end - admin_html_start);
  return ESP_OK;
}

static esp_err_t timezones_get(httpd_req_t *req) {
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, (const char *)tz_json_start,
                  tz_json_end - tz_json_start);
  return ESP_OK;
}

static esp_err_t status_get(httpd_req_t *req) {
  // Sensors can report NaN before they have warmed up; emit 0 so the JSON
  // stays valid (JSON has no NaN/Infinity literal).
  float temp_c = sht4x_current_temp_celsius();
  float humidity = sht4x_current_relative_humidity();
  if (!isfinite(temp_c))
    temp_c = 0;
  if (!isfinite(humidity))
    humidity = 0;
  const char *tz = bitclock_nvs_get_tz();
  const char *weather = bitclock_nvs_get_weather_path();

  char json[512];
  snprintf(json, sizeof(json),
           "{\"temp_c\":%.1f,\"temp_f\":%.1f,\"humidity\":%.1f,\"co2\":%u,"
           "\"voc\":%ld,\"nox\":%ld,\"wifi_connected\":%s,\"temp_unit\":%u,"
           "\"clock_format\":%u,\"app_selection\":%u,\"timezone\":\"%s\","
           "\"weather_path\":\"%s\",\"fw_version\":\"%s\"}",
           temp_c, celsius_to_fahrenheit(temp_c), humidity,
           scd4x_current_co2_ppm(),
           (long)sgp4x_current_voc_index(), (long)sgp41_current_nox_index(),
           bitclock_wifi_has_ip() ? "true" : "false",
           bitclock_nvs_get_temp_unit(), bitclock_nvs_get_clock_format(),
           bitclock_nvs_get_app_selection(), tz ? tz : "",
           weather ? weather : "", BITCLOCK_FW_VERSION);

  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

static int read_body(httpd_req_t *req, char *buf, size_t buf_len) {
  size_t total = req->content_len;
  if (total >= buf_len)
    total = buf_len - 1;
  size_t received = 0;
  while (received < total) {
    int r = httpd_req_recv(req, buf + received, total - received);
    if (r <= 0)
      return -1;
    received += r;
  }
  buf[received] = '\0';
  return received;
}

static esp_err_t settings_post(httpd_req_t *req) {
  char body[512];
  if (read_body(req, body, sizeof(body)) < 0) {
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }

  char val[256];

  if (form_field(body, "temp_unit", val, sizeof(val)) && val[0])
    bitclock_nvs_set_temp_unit((bitclock_nvs_temp_unit_val_t)atoi(val));

  if (form_field(body, "clock_format", val, sizeof(val)) && val[0])
    bitclock_nvs_set_clock_format((bitclock_nvs_clock_format_val_t)atoi(val));

  if (form_field(body, "app_selection", val, sizeof(val)) && val[0])
    bitclock_nvs_set_app_selection((bitclock_nvs_app_selection_val_t)atoi(val));

  if (form_field(body, "timezone", val, sizeof(val)) && val[0]) {
    bitclock_nvs_set_tz(val, strlen(val) + 1);
    setenv("TZ", val, 1);
    tzset();
  }

  if (form_field(body, "weather_path", val, sizeof(val))) {
    bitclock_nvs_set_weather_path(val, strlen(val) + 1);
    xEventGroupSetBits(weather_event_group_handle,
                       WEATHER_EVENT_LOCATION_CHANGED);
  }

  httpd_resp_set_type(req, "text/plain");
  httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

static esp_err_t wifi_post(httpd_req_t *req) {
  char body[256];
  if (read_body(req, body, sizeof(body)) < 0) {
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }

  char ssid[64] = {0};
  char password[128] = {0};
  form_field(body, "ssid", ssid, sizeof(ssid));
  form_field(body, "password", password, sizeof(password));

  if (ssid[0] == '\0') {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_send(req, "Missing SSID", HTTPD_RESP_USE_STRLEN);
    return ESP_FAIL;
  }

  wifi_config_t cfg = {0};
  strncpy((char *)cfg.sta.ssid, ssid, sizeof(cfg.sta.ssid));
  strncpy((char *)cfg.sta.password, password, sizeof(cfg.sta.password));
  esp_wifi_set_config(WIFI_IF_STA, &cfg);

  httpd_resp_set_type(req, "text/plain");
  httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);

  vTaskDelay(pdMS_TO_TICKS(500));
  esp_restart();
  return ESP_OK;
}

static esp_err_t ota_post(httpd_req_t *req) {
  size_t total = req->content_len;
  if (total == 0) {
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }

  esp_err_t err = bitclock_ota_start(total);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "OTA start failed: %d", err);
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }

  char buf[1024];
  size_t received = 0;
  while (received < total) {
    int r = httpd_req_recv(req, buf, sizeof(buf));
    if (r <= 0) {
      bitclock_ota_abort();
      httpd_resp_send_500(req);
      return ESP_FAIL;
    }
    if (bitclock_ota_write(buf, r) != ESP_OK) {
      bitclock_ota_abort();
      httpd_resp_send_500(req);
      return ESP_FAIL;
    }
    received += r;
  }

  if (!bitclock_ota_ready_to_update() ||
      bitclock_ota_complete() != ESP_OK) {
    bitclock_ota_abort();
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }

  httpd_resp_set_type(req, "text/plain");
  httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);

  vTaskDelay(pdMS_TO_TICKS(500));
  esp_restart();
  return ESP_OK;
}

static void mdns_start() {
  esp_err_t err = mdns_init();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "mdns init failed: %d", err);
    return;
  }
  mdns_hostname_set("bitclock");
  mdns_instance_name_set("Bitclock");
  mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
}

void web_admin_start() {
  if (server) {
    return;
  }

  mdns_start();

  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.lru_purge_enable = true;
  config.uri_match_fn = httpd_uri_match_wildcard;
  config.stack_size = 8192;

  if (httpd_start(&server, &config) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start admin HTTP server");
    return;
  }

  httpd_uri_t routes[] = {
      {.uri = "/", .method = HTTP_GET, .handler = root_get},
      {.uri = "/api/timezones", .method = HTTP_GET, .handler = timezones_get},
      {.uri = "/api/status", .method = HTTP_GET, .handler = status_get},
      {.uri = "/api/settings", .method = HTTP_POST, .handler = settings_post},
      {.uri = "/api/wifi", .method = HTTP_POST, .handler = wifi_post},
      {.uri = "/api/ota", .method = HTTP_POST, .handler = ota_post},
  };
  for (size_t i = 0; i < sizeof(routes) / sizeof(routes[0]); i++) {
    httpd_register_uri_handler(server, &routes[i]);
  }

  ESP_LOGI(TAG, "Admin server started at http://bitclock.local");
}

void web_admin_stop() {
  if (server) {
    httpd_stop(server);
    server = NULL;
  }
  mdns_free();
}
