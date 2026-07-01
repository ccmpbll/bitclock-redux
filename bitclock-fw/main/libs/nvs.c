#include "nvs.h"

#include <nvs.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static const char *NVS_NAMESPACE =
    "canary"; // original project codename was canary
static const char *NVS_ID_TIMEZONE = "tz";
static const char *NVS_ID_TIMEZONE_LABEL = "tz_lbl";
static const char *NVS_ID_TEMP_UNIT = "temp_unit";
static const char *NVS_ID_CLOCK_FORMAT = "clk_fmt";
static const char *NVS_ID_NTP_SERVER   = "ntp_host";
static const char *NVS_ID_MQTT_HOST    = "mqtt_host";
static const char *NVS_ID_MQTT_PORT    = "mqtt_port";
static const char *NVS_ID_MQTT_USER    = "mqtt_user";
static const char *NVS_ID_MQTT_PASS    = "mqtt_pass";
static const char *NVS_ID_MQTT_PREFIX   = "mqtt_pfx";
static const char *NVS_ID_MQTT_INTERVAL = "mqtt_intv";

static bitclock_nvs_temp_unit_val_t temp_unit = BITCLOCK_NVS_TEMP_UNIT_VAL_NONE;
static bitclock_nvs_clock_format_val_t clock_format =
    BITCLOCK_NVS_CLOCK_FORMAT_VAL_NONE;

static const char *timezone_str = NULL;
static const char *timezone_label_str = NULL;
static const char *ntp_server_str = NULL;
static const char *mqtt_host_str   = NULL;
static uint16_t    mqtt_port_val   = 0;
static const char *mqtt_user_str   = NULL;
static const char *mqtt_pass_str   = NULL;
static const char *mqtt_prefix_str   = NULL;
static uint16_t    mqtt_interval_val  = 0;

// Validated blob load, shared by every string setting below. Corrupted NVS
// blobs (e.g. from a dangling-pointer write or pre-web-admin firmware) can
// contain binary bytes that break JSON when embedded in /api/status, so a
// loaded string must be null-terminated printable ASCII or it's dropped.
#define LOAD_NVS_STR(key, dest) do { \
  size_t _sz = 0; \
  err = nvs_get_blob(handle, key, NULL, &_sz); \
  if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) { return err; } \
  if (_sz > 0) { \
    char *_buf = malloc(_sz); \
    if (!_buf) { nvs_close(handle); return ESP_ERR_NO_MEM; } \
    err = nvs_get_blob(handle, key, _buf, &_sz); \
    if (err != ESP_OK) { free(_buf); return err; } \
    bool _v = (_sz > 0 && _buf[_sz - 1] == '\0'); \
    for (size_t _i = 0; _v && _i < _sz - 1; _i++) { \
      unsigned char _c = (unsigned char)_buf[_i]; \
      if (_c < 0x20 || _c > 0x7E) _v = false; \
    } \
    if (_v) { dest = _buf; } else { free(_buf); } \
  } \
} while (0)

// Shared helper for blob setters: write, then keep a heap copy so we never
// hold a pointer into the caller's stack frame.
#define BLOB_SETTER(key, dest) \
  nvs_handle_t handle; esp_err_t err; \
  err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle); \
  if (err != ESP_OK) return err; \
  err = nvs_set_blob(handle, key, val, size); \
  nvs_close(handle); \
  if (err != ESP_OK) return err; \
  char *copy = malloc(size); \
  if (copy) { memcpy(copy, val, size); free((void *)dest); dest = copy; } \
  return ESP_OK;

esp_err_t bitclock_nvs_init() {
  nvs_handle_t handle;
  esp_err_t err;

  err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
  if (err != ESP_OK) {
    return err;
  }

  LOAD_NVS_STR(NVS_ID_TIMEZONE,       timezone_str);
  LOAD_NVS_STR(NVS_ID_TIMEZONE_LABEL, timezone_label_str);

  // Temp unit
  err = nvs_get_u8(handle, NVS_ID_TEMP_UNIT, &temp_unit);
  if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
    return err;
  }

  // Clock format
  err = nvs_get_u8(handle, NVS_ID_CLOCK_FORMAT, &clock_format);
  if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
    return err;
  }

  LOAD_NVS_STR(NVS_ID_NTP_SERVER,   ntp_server_str);
  LOAD_NVS_STR(NVS_ID_MQTT_HOST,    mqtt_host_str);
  LOAD_NVS_STR(NVS_ID_MQTT_USER,    mqtt_user_str);
  LOAD_NVS_STR(NVS_ID_MQTT_PASS,    mqtt_pass_str);
  LOAD_NVS_STR(NVS_ID_MQTT_PREFIX,  mqtt_prefix_str);

  err = nvs_get_u16(handle, NVS_ID_MQTT_PORT, &mqtt_port_val);
  if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
    return err;
  }

  err = nvs_get_u16(handle, NVS_ID_MQTT_INTERVAL, &mqtt_interval_val);
  if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
    return err;
  }

  nvs_close(handle);

  return ESP_OK;
}

bitclock_nvs_temp_unit_val_t bitclock_nvs_get_temp_unit() { return temp_unit; }

esp_err_t
bitclock_nvs_set_temp_unit(bitclock_nvs_temp_unit_val_t new_temp_unit) {
  nvs_handle_t handle;
  esp_err_t err;

  err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
  if (err != ESP_OK) {
    return err;
  }

  err = nvs_set_u8(handle, NVS_ID_TEMP_UNIT, new_temp_unit);
  if (err != ESP_OK) {
    return err;
  }
  temp_unit = new_temp_unit;

  nvs_close(handle);

  return ESP_OK;
}

const char *bitclock_nvs_get_tz() { return timezone_str; }
esp_err_t bitclock_nvs_set_tz(const char *val, size_t size) {
  BLOB_SETTER(NVS_ID_TIMEZONE, timezone_str)
}

const char *bitclock_nvs_get_tz_label() { return timezone_label_str; }
esp_err_t bitclock_nvs_set_tz_label(const char *val, size_t size) {
  BLOB_SETTER(NVS_ID_TIMEZONE_LABEL, timezone_label_str)
}

bitclock_nvs_clock_format_val_t bitclock_nvs_get_clock_format() {
  return clock_format;
}

esp_err_t bitclock_nvs_set_clock_format(
    bitclock_nvs_clock_format_val_t new_clock_format) {
  nvs_handle_t handle;
  esp_err_t err;

  err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
  if (err != ESP_OK) {
    return err;
  }

  err = nvs_set_u8(handle, NVS_ID_CLOCK_FORMAT, new_clock_format);
  if (err != ESP_OK) {
    return err;
  }
  clock_format = new_clock_format;

  nvs_close(handle);

  return ESP_OK;
}

const char *bitclock_nvs_get_ntp_server() { return ntp_server_str; }
esp_err_t bitclock_nvs_set_ntp_server(const char *val, size_t size) {
  BLOB_SETTER(NVS_ID_NTP_SERVER, ntp_server_str)
}

const char *bitclock_nvs_get_mqtt_host() { return mqtt_host_str; }
esp_err_t bitclock_nvs_set_mqtt_host(const char *val, size_t size) {
  BLOB_SETTER(NVS_ID_MQTT_HOST, mqtt_host_str)
}

uint16_t bitclock_nvs_get_mqtt_port() { return mqtt_port_val; }
esp_err_t bitclock_nvs_set_mqtt_port(uint16_t port) {
  nvs_handle_t handle;
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
  if (err != ESP_OK) return err;
  err = nvs_set_u16(handle, NVS_ID_MQTT_PORT, port);
  nvs_close(handle);
  if (err == ESP_OK) mqtt_port_val = port;
  return err;
}

const char *bitclock_nvs_get_mqtt_user() { return mqtt_user_str; }
esp_err_t bitclock_nvs_set_mqtt_user(const char *val, size_t size) {
  BLOB_SETTER(NVS_ID_MQTT_USER, mqtt_user_str)
}

const char *bitclock_nvs_get_mqtt_pass() { return mqtt_pass_str; }
esp_err_t bitclock_nvs_set_mqtt_pass(const char *val, size_t size) {
  BLOB_SETTER(NVS_ID_MQTT_PASS, mqtt_pass_str)
}

const char *bitclock_nvs_get_mqtt_prefix() { return mqtt_prefix_str; }
esp_err_t bitclock_nvs_set_mqtt_prefix(const char *val, size_t size) {
  BLOB_SETTER(NVS_ID_MQTT_PREFIX, mqtt_prefix_str)
}

uint16_t bitclock_nvs_get_mqtt_interval() { return mqtt_interval_val; }
esp_err_t bitclock_nvs_set_mqtt_interval(uint16_t seconds) {
  nvs_handle_t handle;
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
  if (err != ESP_OK) return err;
  err = nvs_set_u16(handle, NVS_ID_MQTT_INTERVAL, seconds);
  nvs_close(handle);
  if (err == ESP_OK) mqtt_interval_val = seconds;
  return err;
}
