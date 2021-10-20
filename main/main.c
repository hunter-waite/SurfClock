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

    while(1){
        vTaskDelay(1);
    }
}
