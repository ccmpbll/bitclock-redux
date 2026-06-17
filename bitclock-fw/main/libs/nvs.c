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
static const char *NVS_ID_MQTT_PREFIX  = "mqtt_pfx";

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
static const char *mqtt_prefix_str = NULL;

esp_err_t bitclock_nvs_init() {
  nvs_handle_t handle;
  esp_err_t err;

  err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
  if (err != ESP_OK) {
    return err;
  }

  // Timezone
  size_t required_size = 0;
  err = nvs_get_blob(handle, NVS_ID_TIMEZONE, NULL, &required_size);
  if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
    return err;
  }
  if (required_size > 0) {
    char *tz_buf = malloc(required_size);
    err = nvs_get_blob(handle, NVS_ID_TIMEZONE, tz_buf, &required_size);
    if (err != ESP_OK) {
      free(tz_buf);
      return err;
    }
    // Validate: must be null-terminated printable ASCII. Corrupted NVS blobs
    // (e.g. from a dangling-pointer write or pre-web-admin firmware) contain
    // binary bytes that break JSON when embedded in /api/status.
    bool valid = (required_size > 0 && tz_buf[required_size - 1] == '\0');
    for (size_t i = 0; valid && i < required_size - 1; i++) {
      unsigned char c = (unsigned char)tz_buf[i];
      if (c < 0x20 || c > 0x7E)
        valid = false;
    }
    if (valid) {
      timezone_str = tz_buf;
    } else {
      free(tz_buf);
      // Leave timezone_str NULL; user can re-set via web admin.
    }
  }

  // Timezone label (display name for dropdown preselection)
  size_t lbl_size = 0;
  err = nvs_get_blob(handle, NVS_ID_TIMEZONE_LABEL, NULL, &lbl_size);
  if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
    return err;
  }
  if (lbl_size > 0) {
    char *lbl_buf = malloc(lbl_size);
    err = nvs_get_blob(handle, NVS_ID_TIMEZONE_LABEL, lbl_buf, &lbl_size);
    if (err != ESP_OK) {
      free(lbl_buf);
      return err;
    }
    bool lbl_valid = (lbl_size > 0 && lbl_buf[lbl_size - 1] == '\0');
    for (size_t i = 0; lbl_valid && i < lbl_size - 1; i++) {
      unsigned char c = (unsigned char)lbl_buf[i];
      if (c < 0x20 || c > 0x7E)
        lbl_valid = false;
    }
    if (lbl_valid) {
      timezone_label_str = lbl_buf;
    } else {
      free(lbl_buf);
    }
  }

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

  // NTP server
  size_t ntp_size = 0;
  err = nvs_get_blob(handle, NVS_ID_NTP_SERVER, NULL, &ntp_size);
  if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
    return err;
  }
  if (ntp_size > 0) {
    char *ntp_buf = malloc(ntp_size);
    err = nvs_get_blob(handle, NVS_ID_NTP_SERVER, ntp_buf, &ntp_size);
    if (err != ESP_OK) {
      free(ntp_buf);
      return err;
    }
    bool ntp_valid = (ntp_size > 0 && ntp_buf[ntp_size - 1] == '\0');
    for (size_t i = 0; ntp_valid && i < ntp_size - 1; i++) {
      unsigned char c = (unsigned char)ntp_buf[i];
      if (c < 0x20 || c > 0x7E)
        ntp_valid = false;
    }
    if (ntp_valid) {
      ntp_server_str = ntp_buf;
    } else {
      free(ntp_buf);
    }
  }

  // MQTT config (blob keys with same validation pattern)
  #define LOAD_MQTT_STR(key, dest) do { \
    size_t _sz = 0; \
    err = nvs_get_blob(handle, key, NULL, &_sz); \
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) { return err; } \
    if (_sz > 0) { \
      char *_buf = malloc(_sz); \
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

  LOAD_MQTT_STR(NVS_ID_MQTT_HOST,   mqtt_host_str);
  LOAD_MQTT_STR(NVS_ID_MQTT_USER,   mqtt_user_str);
  LOAD_MQTT_STR(NVS_ID_MQTT_PASS,   mqtt_pass_str);
  LOAD_MQTT_STR(NVS_ID_MQTT_PREFIX, mqtt_prefix_str);

  err = nvs_get_u16(handle, NVS_ID_MQTT_PORT, &mqtt_port_val);
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

esp_err_t bitclock_nvs_set_tz(const char *tz, size_t size) {
  nvs_handle_t handle;
  esp_err_t err;

  err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
  if (err != ESP_OK) {
    return err;
  }

  err = nvs_set_blob(handle, NVS_ID_TIMEZONE, tz, size);
  nvs_close(handle);
  if (err != ESP_OK) {
    return err;
  }

  // Copy to heap so we never hold a pointer into the caller's stack frame.
  char *copy = malloc(size);
  if (copy) {
    memcpy(copy, tz, size);
    free((void *)timezone_str);
    timezone_str = copy;
  }

  return ESP_OK;
}

const char *bitclock_nvs_get_tz_label() { return timezone_label_str; }

esp_err_t bitclock_nvs_set_tz_label(const char *label, size_t size) {
  nvs_handle_t handle;
  esp_err_t err;

  err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
  if (err != ESP_OK) {
    return err;
  }

  err = nvs_set_blob(handle, NVS_ID_TIMEZONE_LABEL, label, size);
  nvs_close(handle);
  if (err != ESP_OK) {
    return err;
  }

  char *copy = malloc(size);
  if (copy) {
    memcpy(copy, label, size);
    free((void *)timezone_label_str);
    timezone_label_str = copy;
  }

  return ESP_OK;
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

esp_err_t bitclock_nvs_set_ntp_server(const char *server, size_t size) {
  nvs_handle_t handle;
  esp_err_t err;

  err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
  if (err != ESP_OK) {
    return err;
  }

  err = nvs_set_blob(handle, NVS_ID_NTP_SERVER, server, size);
  nvs_close(handle);
  if (err != ESP_OK) {
    return err;
  }

  char *copy = malloc(size);
  if (copy) {
    memcpy(copy, server, size);
    free((void *)ntp_server_str);
    ntp_server_str = copy;
  }

  return ESP_OK;
}

// Shared helper for blob setters
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
