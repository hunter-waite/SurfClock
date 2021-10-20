/* RMT example -- RGB LED Strip
   This example code is in the Public Domain (or CC0 licensed, at your option.)
   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/rmt.h"
#include "led_strip.h"
#include "ratings.h"

static const char *tag = "LED Strip";

#define RMT_TX_CHANNEL RMT_CHANNEL_0

void clear_led_strip(led_strip_t *strip)
{
    strip->clear(strip, 50);
}

void update_led_strip(led_strip_t *strip, Rating rating)
{
    strip->clear(strip, 50);
    // Show simple rainbow chasing pattern
    ESP_LOGI(tag, "Updating LED Strip");
    for (int j = 0; j < rating.num_leds; j++) {
        ESP_ERROR_CHECK(strip->set_pixel(strip, j, rating.red, rating.green, rating.blue));
    }
    // Flush RGB values to LEDs
    ESP_ERROR_CHECK(strip->refresh(strip, 100));
}

led_strip_t *init_led_strip(void)
{
    rmt_config_t config = RMT_DEFAULT_CONFIG_TX(CONFIG_EXAMPLE_RMT_TX_GPIO, RMT_TX_CHANNEL);
    // set counter clock to 40MHz
    config.clk_div = 2;

    ESP_ERROR_CHECK(rmt_config(&config));
    ESP_ERROR_CHECK(rmt_driver_install(config.channel, 0, 0));

    // install ws2812 driver
    led_strip_config_t strip_config = LED_STRIP_DEFAULT_CONFIG(CONFIG_EXAMPLE_STRIP_LED_NUMBER, (led_strip_dev_t)config.channel);
    led_strip_t *strip = led_strip_new_rmt_ws2812(&strip_config);
    if (!strip) {
        ESP_LOGE(tag, "install WS2812 driver failed");
    }
    // Clear LED strip (turn off all LEDs)
    ESP_ERROR_CHECK(strip->clear(strip, 100));
    return(strip);
}