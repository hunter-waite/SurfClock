/*
 * Surf Clock main functionality
 * 
 * Created by: Hunter Waite
 */
#include "main.h"

void app_main(void)
{
    // initializes wifi
    init_wifi();
    init_request();
    while(1){}
}
