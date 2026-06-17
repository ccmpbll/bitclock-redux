#include "nvs.h"

#include <nvs.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static const char *NVS_NAMESPACE =
    "canary"; // original project codename was canary
static const char *NVS_ID_TIMEZONE = "tz";
static const char *NVS_ID_TEMP_UNIT = "temp_unit";
static const char *NVS_ID_CLOCK_FORMAT = "clk_fmt";

static bitclock_nvs_temp_unit_val_t temp_unit = BITCLOCK_NVS_TEMP_UNIT_VAL_NONE;
static bitclock_nvs_clock_format_val_t clock_format =
    BITCLOCK_NVS_CLOCK_FORMAT_VAL_NONE;

static const char *timezone_str = NULL;

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
