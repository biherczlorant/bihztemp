#include "display.h"

#include <driver/gpio.h>
#include <driver/i2c.h>
#include <esp_err.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <ssd1306/ssd1306.h>

#define SCL_PIN 14
#define SDA_PIN 2
#define DISPLAY_WIDTH 128
#define DISPLAY_HEIGHT 64

static ssd1306_t display = {.i2c_port = I2C_NUM_0,
                            .i2c_addr = SSD1306_I2C_ADDR_0,
                            .screen = SSD1306_SCREEN,
                            .width = DISPLAY_WIDTH,
                            .height = DISPLAY_HEIGHT};

static uint8_t buffer[DISPLAY_WIDTH * DISPLAY_HEIGHT / 8] = {0};

void init_display(void) {
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = SDA_PIN;
    conf.sda_pullup_en = 1;
    conf.scl_io_num = SCL_PIN;
    conf.scl_pullup_en = 1;
    conf.clk_stretch_tick = 300;
    ESP_ERROR_CHECK(i2c_driver_install(I2C_NUM_0, conf.mode));
    ESP_ERROR_CHECK(i2c_param_config(I2C_NUM_0, &conf));

    while (ssd1306_init(&display) != 0) {
        ESP_LOGE("display", "couldn't init display");
        vTaskDelay(100);
    }
    ssd1306_set_segment_remapping_enabled(&display, true);
    ssd1306_set_scan_direction_fwd(&display, false);
    ESP_LOGI("display", "initialized display");
}

void display_test(char* string) {
    ssd1306_set_whole_display_lighting(&display, false);
    const font_info_t* font = font_builtin_fonts[FONT_FACE_GLCD5x7];
    int err = ssd1306_draw_string(&display, buffer, font, 0, 0, string,
                                  OLED_COLOR_WHITE, OLED_COLOR_BLACK);
    if (err) ESP_LOGE("display", "couldnt draw string");
    err = ssd1306_load_frame_buffer(&display, buffer);
    if (err) ESP_LOGE("display", "couldnt add to buffer");
}
