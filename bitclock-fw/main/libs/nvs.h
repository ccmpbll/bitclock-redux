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

bitclock_nvs_temp_unit_val_t bitclock_nvs_get_temp_unit();
esp_err_t bitclock_nvs_set_temp_unit(bitclock_nvs_temp_unit_val_t unit);

bitclock_nvs_clock_format_val_t bitclock_nvs_get_clock_format();
esp_err_t bitclock_nvs_set_clock_format(bitclock_nvs_clock_format_val_t format);
