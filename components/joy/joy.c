#include "joy.h"
#include "esp_adc/adc_oneshot.h"

#define ADC1_CHAN0    ADC_CHANNEL_6
#define ADC1_CHAN1    ADC_CHANNEL_7
#define POS_READINGS  5

adc_oneshot_unit_handle_t adc1_handle;
int32_t joy_x_pos;
int32_t joy_y_pos;

int32_t joy_init() {
    // Configure an ADC one-shot unit
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    // Configure ADC1 Channels 6 and 7
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC1_CHAN0, &config));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC1_CHAN1, &config));

    // ADC1 Joystick Calibration
    int_fast32_t xpos;
    int_fast32_t ypos;
    int32_t xposT = 0;
    int32_t yposT = 0;

    for (uint8_t i = 0; i < POS_READINGS; i++) {
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC1_CHAN0, &xpos));
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC1_CHAN1, &ypos));

        xposT += xpos;
        yposT += ypos;
    }

    joy_x_pos = xposT / POS_READINGS;
    joy_y_pos = yposT / POS_READINGS;

    return 0;
}

int32_t joy_deinit() {
    if (adc1_handle != NULL) {
        ESP_ERROR_CHECK(adc_oneshot_del_unit(adc1_handle));
    }

    return 0;
}

void joy_get_displacement(int32_t *dcx, int32_t *dcy) {
    int_fast32_t x_pos;
    int_fast32_t y_pos;

    ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC1_CHAN0, &x_pos));
    ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC1_CHAN1, &y_pos));

    *dcx = x_pos - joy_x_pos;
    *dcy = y_pos - joy_y_pos;
}