/*
 * Surf Clock main functionality
 * 
 * Created by: Hunter Waite
 */
#include "main.h"

led_strip_t *strip;

void app_main(void)
{
    /* initializes wifi and checks to make sure it stays connected to it */
    init_wifi();

    /* initialize led strip, this includes the rmt module */
    strip = init_led_strip();

    /* initialize json collection, constantly requests data from the surfline API */
    init_request(strip);

    /* initializes the OLED and sets the global variable ssd1306_dev as a reference */
    init_oled();
    ssd1306_refresh_gram(ssd1306_dev);
    ssd1306_clear_screen(ssd1306_dev, 0x00);

    while(1){
        vTaskDelay(100);
    }
}
