#include "clock.h"

#include "aqi_grid.h"
#include "libs/nvs.h"
#include "libs/sensor_utils.h"
#include "lvgl/styles.h"
#include "lvgl/utils.h"
#include <math.h>
#include <time.h>

lv_helper_view_mode_clock_data_t lv_helper_view_mode_clock_data;

// Layout constants
#define SCREEN_W      264
#define SCREEN_H      176
#define DIVIDER_X     150   // left panel: 0–149, right panel: 150–263
#define RIGHT_PAD     8
#define LEFT_PAD      6

// Left panel vertical layout
#define TIME_H        60
#define SUBTEXT_H     16
#define GAP_TIME_DAY  16
#define GAP_DAY_DATE  8
#define LEFT_TOTAL_H  (TIME_H + GAP_TIME_DAY + SUBTEXT_H + GAP_DAY_DATE + SUBTEXT_H)
#define LEFT_TOP_Y    ((SCREEN_H - LEFT_TOTAL_H) / 2)

// Right panel rows (5 sensor rows, evenly spaced)
#define NUM_ROWS      5
#define ROW_H         (SCREEN_H / NUM_ROWS)
#define ROW_TOP(i)    ((i) * ROW_H + (ROW_H - SUBTEXT_H) / 2)

static lv_obj_t *time_label;
static lv_obj_t *day_label;
static lv_obj_t *date_label;

static lv_obj_t *temp_lbl, *temp_val;
static lv_obj_t *hum_lbl,  *hum_val;
static lv_obj_t *co2_lbl,  *co2_val;
static lv_obj_t *voc_lbl,  *voc_val;
static lv_obj_t *nox_lbl,  *nox_val;

static lv_obj_t *make_left_label(lv_style_t *style, int y) {
  lv_obj_t *lbl = lv_label_create(lv_screen_active());
  lv_obj_add_style(lbl, style, LV_PART_MAIN);
  lv_obj_set_width(lbl, DIVIDER_X - LEFT_PAD * 2);
  lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_pos(lbl, LEFT_PAD, y);
  return lbl;
}

static lv_obj_t *make_row_label(int row) {
  lv_obj_t *lbl = lv_label_create(lv_screen_active());
  lv_obj_add_style(lbl, &sublabel_style, LV_PART_MAIN);
  lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, DIVIDER_X + RIGHT_PAD, ROW_TOP(row));
  return lbl;
}

static lv_obj_t *make_row_value(int row) {
  lv_obj_t *lbl = lv_label_create(lv_screen_active());
  lv_obj_add_style(lbl, &sublabel_style, LV_PART_MAIN);
  lv_obj_align(lbl, LV_ALIGN_TOP_RIGHT, -RIGHT_PAD, ROW_TOP(row));
  return lbl;
}

void lv_helper_clock_create() {
  // Left panel: time, day, date
  time_label = make_left_label(&medium_number_style, LEFT_TOP_Y);
  day_label  = make_left_label(&sublabel_style,
                               LEFT_TOP_Y + TIME_H + GAP_TIME_DAY);
  date_label = make_left_label(&sublabel_style,
                               LEFT_TOP_Y + TIME_H + GAP_TIME_DAY + SUBTEXT_H + GAP_DAY_DATE);

  // Right panel: sensor label + value pairs
  temp_lbl = make_row_label(0); lv_label_set_text(temp_lbl, "TEMP");
  temp_val = make_row_value(0);
  hum_lbl  = make_row_label(1); lv_label_set_text(hum_lbl,  "HUMID");
  hum_val  = make_row_value(1);
  co2_lbl  = make_row_label(2); lv_label_set_text(co2_lbl,  "CO\xe2\x82\x82");
  co2_val  = make_row_value(2);
  voc_lbl  = make_row_label(3); lv_label_set_text(voc_lbl,  "VOC");
  voc_val  = make_row_value(3);
  nox_lbl  = make_row_label(4); lv_label_set_text(nox_lbl,  "NO\xe2\x82\x93");
  nox_val  = make_row_value(4);
}

void lv_helper_clock_update(lv_helper_view_mode_clock_data_t *data) {
  static struct tm timeinfo;
  localtime_r(&data->curtime, &timeinfo);

  // Time
  static char time_str[6];
  static char hour_str[3];
  static char minute_str[3];
  if (data->hour24) {
    snprintf(hour_str, sizeof(hour_str), "%u", timeinfo.tm_hour);
  } else {
    snprintf(hour_str, sizeof(hour_str), "%u",
             timeinfo.tm_hour % 12 == 0 ? 12 : timeinfo.tm_hour % 12);
  }
  strftime(minute_str, sizeof(minute_str), "%M", &timeinfo);
  snprintf(time_str, sizeof(time_str), "%s:%s", hour_str, minute_str);
  set_text_if_changed(time_label, time_str);

  // Day and date (separate labels)
  static char day_str[10];
  strftime(day_str, sizeof(day_str), "%A", &timeinfo);
  str_to_upper(day_str);
  set_text_if_changed(day_label, day_str);

  static char date_str[10];
  static char month_str[4];
  strftime(month_str, sizeof(month_str), "%b", &timeinfo);
  snprintf(date_str, sizeof(date_str), "%s %u", month_str, timeinfo.tm_mday);
  str_to_upper(date_str);
  set_text_if_changed(date_label, date_str);

  // Sensor readings
  lv_helper_view_mode_aqi_data_t *aqi = &lv_helper_view_mode_aqi_data;
  bool fahrenheit =
      bitclock_nvs_get_temp_unit() != BITCLOCK_NVS_TEMP_UNIT_VAL_CELSIUS;
  float temp = fahrenheit ? celsius_to_fahrenheit(aqi->temp_celsius)
                          : aqi->temp_celsius;

  static char temp_str[8], hum_str[6], co2_str[6], voc_str[6], nox_str[6];
  snprintf(temp_str, sizeof(temp_str), "%.0f\xc2\xb0%c",
           roundf(temp), fahrenheit ? 'F' : 'C');
  snprintf(hum_str,  sizeof(hum_str),  "%u%%", aqi->humidity_pct);
  snprintf(co2_str,  sizeof(co2_str),  "%u",   aqi->co2_ppm);
  snprintf(voc_str,  sizeof(voc_str),  "%ld",  (long)aqi->voc_index);
  snprintf(nox_str,  sizeof(nox_str),  "%ld",  (long)aqi->nox_index);

  set_text_if_changed(temp_val, temp_str);
  set_text_if_changed(hum_val,  hum_str);
  set_text_if_changed(co2_val,  co2_str);
  set_text_if_changed(voc_val,  voc_str);
  set_text_if_changed(nox_val,  nox_str);
}
