/*
 * Surf Clock main functionality
 * 
 * Created by: Hunter Waite
 */
#include "main.h"

void app_main(void)
{
    /* initializes wifi and checks to make sure it stays connected to it */
    init_wifi();

    /* constantly requests data from the surfline API */
    init_request();
    while(1){
        vTaskDelay(1);
    }
}
