#include "wifi_setup.h"

#include "lvgl/lvgl.h"
#include "lvgl/styles.h"

static lv_obj_t *title_label;
static lv_obj_t *line1_label;
static lv_obj_t *line2_label;
static lv_obj_t *line3_label;

void lv_helper_wifi_setup_create(bool is_fallback) {
  lv_obj_t *screen = lv_screen_active();

  title_label = lv_label_create(screen);
  lv_obj_add_style(title_label, &sublabel_style, LV_PART_MAIN);
  lv_obj_align(title_label, LV_ALIGN_CENTER, 0, -50);
  lv_label_set_text(title_label,
                    is_fallback ? "WI-FI FAILED" : "WI-FI SETUP");

  line1_label = lv_label_create(screen);
  lv_obj_add_style(line1_label, &sublabel_style, LV_PART_MAIN);
  lv_obj_align(line1_label, LV_ALIGN_CENTER, 0, -10);
  lv_label_set_text(line1_label,
                    is_fallback ? "COULD NOT CONNECT." : "");

  line2_label = lv_label_create(screen);
  lv_obj_add_style(line2_label, &sublabel_style, LV_PART_MAIN);
  lv_obj_align(line2_label, LV_ALIGN_CENTER, 0, 20);
  lv_label_set_text(line2_label, "1. CONNECT TO:");

  line3_label = lv_label_create(screen);
  lv_obj_add_style(line3_label, &sublabel_style, LV_PART_MAIN);
  lv_obj_align(line3_label, LV_ALIGN_CENTER, 0, 45);
  lv_label_set_text(line3_label, "   BITCLOCK-SETUP");

  lv_obj_t *line4_label = lv_label_create(screen);
  lv_obj_add_style(line4_label, &sublabel_style, LV_PART_MAIN);
  lv_obj_align(line4_label, LV_ALIGN_CENTER, 0, 70);
  lv_label_set_text(line4_label, "2. OPEN BROWSER:");

  lv_obj_t *line5_label = lv_label_create(screen);
  lv_obj_add_style(line5_label, &sublabel_style, LV_PART_MAIN);
  lv_obj_align(line5_label, LV_ALIGN_CENTER, 0, 95);
  lv_label_set_text(line5_label, "   192.168.4.1");
}

void lv_helper_wifi_setup_update() {}
