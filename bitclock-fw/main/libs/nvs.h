#include "esp_err.h"

typedef uint8_t bitclock_nvs_temp_unit_val_t;
typedef uint8_t bitclock_nvs_clock_format_val_t;

#define BITCLOCK_NVS_TEMP_UNIT_VAL_NONE 0
#define BITCLOCK_NVS_TEMP_UNIT_VAL_CELSIUS 1
#define BITCLOCK_NVS_TEMP_UNIT_VAL_FAHRENHEIT 2

#define BITCLOCK_NVS_CLOCK_FORMAT_VAL_NONE 0
#define BITCLOCK_NVS_CLOCK_FORMAT_VAL_12HR 1
#define BITCLOCK_NVS_CLOCK_FORMAT_VAL_24HR 2

esp_err_t bitclock_nvs_init();

const char *bitclock_nvs_get_tz();
esp_err_t bitclock_nvs_set_tz(const char *tz, size_t size);

const char *bitclock_nvs_get_tz_label();
esp_err_t bitclock_nvs_set_tz_label(const char *label, size_t size);

bitclock_nvs_temp_unit_val_t bitclock_nvs_get_temp_unit();
esp_err_t bitclock_nvs_set_temp_unit(bitclock_nvs_temp_unit_val_t unit);

bitclock_nvs_clock_format_val_t bitclock_nvs_get_clock_format();
esp_err_t bitclock_nvs_set_clock_format(bitclock_nvs_clock_format_val_t format);

const char *bitclock_nvs_get_ntp_server();
esp_err_t bitclock_nvs_set_ntp_server(const char *server, size_t size);

const char *bitclock_nvs_get_mqtt_host();
esp_err_t bitclock_nvs_set_mqtt_host(const char *host, size_t size);

uint16_t bitclock_nvs_get_mqtt_port();
esp_err_t bitclock_nvs_set_mqtt_port(uint16_t port);

const char *bitclock_nvs_get_mqtt_user();
esp_err_t bitclock_nvs_set_mqtt_user(const char *user, size_t size);

const char *bitclock_nvs_get_mqtt_pass();
esp_err_t bitclock_nvs_set_mqtt_pass(const char *pass, size_t size);

const char *bitclock_nvs_get_mqtt_prefix();
esp_err_t bitclock_nvs_set_mqtt_prefix(const char *prefix, size_t size);
