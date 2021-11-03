/* 
 *  Performs an HTTP get request from surfline.com using their hidden surfline
 *  V2 api. It requests a single day's worth of data containing the waves and
 *  their optimalScore
 * 
 *  Surfline returns from their API a JSON. The whole 
 * 
 *  Modified from the examples given by EspressIf
 * 
 *  Modified By: Hunter Waite
 * 
 */

#include <string.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

#include "cJSON.h"
#include "led_strip.c"
#include "ssd1306_util.c"

/* Constants that aren't configurable in menuconfig */

/* Long range not sure if this solution would works as the surline API is not 
 * documented and can be changed at any time without notice */
#define WEB_SERVER "services.surfline.com"  // surfline api host
#define WEB_PORT "80"                       // http port
/* oceanside spot ID */
//#define WEB_PATH "/kbyg/regions/forecasts/conditions?subregionId=58581a836630e24c44878fd7&days=1"

/* slo spot ID */
#define WEB_PATH "/kbyg/regions/forecasts/conditions?subregionId=58581a836630e24c44879014&days=1"

/* South Carolina spot ID */
//#define WEB_PATH "/kbyg/regions/forecasts/conditions?subregionId=58581a836630e24c44878fdf&days=1"

/* cayucos spot ID because it is always poor to fair */
//#define WEB_PATH "/kbyg/regions/forecasts/conditions?subregionId=5842041f4e65fad6a77089a2&days=1"

#define DELAY_TIME  10000   // 10 second delay time between each get request

#define BUFFER_SIZE 2048    // 2048 byte character buffer

static const char *T = "JSON Parser";

static const char *REQUEST = "GET " WEB_PATH " HTTP/1.0\r\n"
    "Host: "WEB_SERVER":"WEB_PORT"\r\n"
    "User-Agent: esp-idf/1.0 esp32\r\n"
    "\r\n";

/* takes in a rating string from the surfline api and returns the correct 
   Rating struct filled out
   TODO: Make it a static variable and don't update the LEDs if the rating
   remains the same
*/
Rating calculate_rating(char *r)
{
    Rating rating;

    ESP_LOGI(T, "Rating: %s\n", r);

    if(!strncmp(r, P, MAX_COMP_SHORT))
    {
        rating.red = 0;
        rating.green = 17;
        rating.blue = 255;
    }
    else if(!strncmp(r, P_TO_F, MAX_COMP_LONG))
    {
        rating.red = 0;
        rating.green = 234;
        rating.blue = 255;
    }
    else if(!strncmp(r, F, MAX_COMP_SHORT))
    {
        rating.red = 0;
        rating.green = 255;
        rating.blue = 0;
    }
    else if(!strncmp(r, F_TO_G, MAX_COMP_LONG))
    {
        rating.red = 255;
        rating.green = 242;
        rating.blue = 0;
    }
    else if(!strncmp(r, G, MAX_COMP_SHORT))
    {
        rating.red = 255;
        rating.green = 145;
        rating.blue = 0;
    }
    else if(!strncmp(r, G_TO_E, MAX_COMP_LONG))
    {
        rating.red = 255;
        rating.green = 0;
        rating.blue = 0;
    }
    else if(!strncmp(r, E, MAX_COMP_SHORT))
    {
        rating.red = 204;
        rating.green = 0;
        rating.blue = 255;
    }
    else // flat or a not supported value
    {
        rating.red = 100;
        rating.green = 0;
        rating.blue = 0;
    }

    return rating;
}

/* takes in a string and uses CJSON to parse objects, for now prints to 
    terminal */
void parse_json(char *recv_buf, int recv_len, led_strip_t *strip)
{
    // initial json
    const cJSON *data = NULL;

    // conditions object
    const cJSON *condition = NULL;
    const cJSON *conditions = NULL;

    // subsets of conditions
    const cJSON *am = NULL;
    const cJSON *rating = NULL;
    const cJSON *maxHeight = NULL;
    const cJSON *minHeight = NULL;
    
    const char *error_ptr = NULL;

    Rating r;

    // time value
    struct tm tm;
    struct tm *adjusted;
    time_t t;

    // instead of using an NTP server get time date from HTTP header
    char *content = strstr(recv_buf, "Date: ");

    // move past "Date: "
    content += 6;

    if(content)
    {
        ESP_LOGI(T, "%s\n", content);
        // parse for time data
        if (strptime(content, "%a, %d %b %Y %H:%M:%S", &tm) == NULL)
        {
            ESP_LOGI(T, "Could not parse time from HTTP header");
        }
    }

    t = mktime(&tm);
    t -= 25200;
    adjusted = localtime(&t);

    ESP_LOGI(T, "hour: %d; minute: %d; second: %d\n", adjusted->tm_hour, adjusted->tm_min, adjusted->tm_sec);


    /* get rid of the HTTP header */
    content = strstr(recv_buf, "\r\n\r\n");

    ESP_LOGI(T, "Parsing JSON\n");

    if (content != NULL) {
        content += 4; // Offset by 4 bytes to start of content
    }
    else {
        ESP_LOGE(T, "End of Header Not Found\n");
    }

    /* parse the given json */
    cJSON *json = cJSON_Parse(content);

    /* check for invalid JSON response */
    if (json == NULL)
    {
        ESP_LOGE(T, "\tError With First Parse\n");
        error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL)
        {
            ESP_LOGE(T, "%s\n", error_ptr);
        }
        cJSON_Delete(json);
        return;
    }

    // get data portion from JSON
    data = cJSON_GetObjectItemCaseSensitive(json, "data");

    // get conditions portion of JSON
    conditions = cJSON_GetObjectItemCaseSensitive(data, "conditions");

    // different conditions requires this
    cJSON_ArrayForEach(condition, conditions)
    {
        // get the morning condition report
        am = cJSON_GetObjectItemCaseSensitive(condition, "am");

        // get the morning rating
        rating = cJSON_GetObjectItemCaseSensitive(am, "rating");

        // check and make sure it is a valid rating
        if (!cJSON_IsString(rating))
        {
            ESP_LOGE(T, "\tWrong Rating\n");
            error_ptr = cJSON_GetErrorPtr();
            if (error_ptr != NULL)
            {
                ESP_LOGE(T, "%s\n", error_ptr);
            }
            cJSON_Delete(json);
            return;
        }

        // get maximum wave height
        maxHeight = cJSON_GetObjectItemCaseSensitive(am, "maxHeight");

        // make sure maximum height is valid
        if (!cJSON_IsNumber(maxHeight))
        {
            ESP_LOGE(T, "\tWrong Max Height\n");
            error_ptr = cJSON_GetErrorPtr();
            if (error_ptr != NULL)
            {
                ESP_LOGE(T, "%s\n", error_ptr);
            }
            cJSON_Delete(json);
            return;
        }

        // get minimum height
        minHeight = cJSON_GetObjectItemCaseSensitive(am, "minHeight");

        // make sure minimum height is valid
        if (!cJSON_IsNumber(minHeight))
        {
            ESP_LOGE(T, "\tWrong Min Height\n");
            error_ptr = cJSON_GetErrorPtr();
            if (error_ptr != NULL)
            {
                ESP_LOGE(T, "%s\n", error_ptr);
            }
            cJSON_Delete(json);
            return;
        }

        // log values to console
        ESP_LOGI(T, "Wave Height: %d-%d ft\n", 
            minHeight->valueint, 
            maxHeight->valueint);

        ESP_LOGI(T, "\tRating: %s\n", rating->valuestring);

        if(rating)
        {
            r = calculate_rating(rating->valuestring);
        }

        r.num_leds = maxHeight->valueint;
        update_led_strip((led_strip_t *)strip, r);

        char data_str[16] = {0};
        sprintf(data_str, "%02d:%02d", adjusted->tm_hour, adjusted->tm_min);
        ESP_LOGI(T, "%s\n", data_str);
        ssd1306_clear_screen(ssd1306_dev, 0x00);
        ssd1306_draw_3216char(ssd1306_dev, 24, 0, data_str[0]);
        ssd1306_draw_3216char(ssd1306_dev, 40, 0, data_str[1]);
        ssd1306_draw_3216char(ssd1306_dev, 56, 0, data_str[2]);
        ssd1306_draw_3216char(ssd1306_dev, 72, 0, data_str[3]);
        ssd1306_draw_3216char(ssd1306_dev, 88, 0, data_str[4]);

        char rating_str[16] = {0};
        for(int i = 0; i < strlen(rating->valuestring); i++)
        {
            if(rating->valuestring[i] == '_')
            {
                rating->valuestring[i] = ' ';
            }
        }
        sprintf(rating_str, "%s", rating->valuestring);
        int center_val = 32;
        if(strlen(rating->valuestring) == 12)
        {
            center_val = 16;
        }
        ssd1306_draw_string(ssd1306_dev, center_val, 40, (const uint8_t *)rating_str, 16, 1);
        ssd1306_refresh_gram(ssd1306_dev);
    }

    // clear all old JSON values
    cJSON_Delete(json);
    return;

}

static void get_surline_data(void *strip)
{
    const struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };
    struct addrinfo *res;
    struct in_addr *addr;
    int s, r;
    char recv_buf[BUFFER_SIZE];
    while(1) {
        int err = getaddrinfo(WEB_SERVER, WEB_PORT, &hints, &res);

        if(err != 0 || res == NULL) {
            ESP_LOGE(T, "DNS lookup failed err=%d res=%p", err, res);
            vTaskDelay(DELAY_TIME / portTICK_PERIOD_MS);
            continue;
        }

        /* Code to print the resolved IP.
            Note: inet_ntoa is non-reentrant, look at ipaddr_ntoa_r for "real" code */
        addr = &((struct sockaddr_in *)res->ai_addr)->sin_addr;
        ESP_LOGI(T, "DNS lookup succeeded. IP=%s", inet_ntoa(*addr));

        s = socket(res->ai_family, res->ai_socktype, 0);
        if(s < 0) {
            ESP_LOGE(T, "... Failed to allocate socket.");
            freeaddrinfo(res);
            vTaskDelay(DELAY_TIME / portTICK_PERIOD_MS);
            continue;
        }
        ESP_LOGI(T, "... allocated socket");

        if(connect(s, res->ai_addr, res->ai_addrlen) != 0) {
            ESP_LOGE(T, "... socket connect failed errno=%d", errno);
            close(s);
            freeaddrinfo(res);
            vTaskDelay(DELAY_TIME / portTICK_PERIOD_MS);
            continue;
        }

        ESP_LOGI(T, "... connected");
        freeaddrinfo(res);

        if (write(s, REQUEST, strlen(REQUEST)) < 0) {
            ESP_LOGE(T, "... socket send failed");
            close(s);
            vTaskDelay(DELAY_TIME / portTICK_PERIOD_MS);
            continue;
        }
        ESP_LOGI(T, "... socket send success");

        struct timeval receiving_timeout;
        receiving_timeout.tv_sec = 5;
        receiving_timeout.tv_usec = 0;
        if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &receiving_timeout,
                sizeof(receiving_timeout)) < 0) {
            ESP_LOGE(T, "... failed to set socket receiving timeout");
            close(s);
            vTaskDelay(DELAY_TIME / portTICK_PERIOD_MS);
            continue;
        }
        ESP_LOGI(T, "... set socket receiving timeout success");

        // zero out receive buffer for next fill
        bzero(recv_buf, sizeof(recv_buf));

        // wait 5 seconds to make sure the socket has completely recieved the message
        // realistically this should be a call to recv or something similar
        vTaskDelay(5000 / portTICK_PERIOD_MS);

        // read from the socket and get data
        r = read(s, recv_buf, sizeof(recv_buf));

        // parse the incoming json for data needed
        parse_json(recv_buf, r, (led_strip_t *)strip);

        ESP_LOGI(T, "... done reading from socket. Last read return=%d errno=%d.", r, errno);
        close(s);

        // delay until calling again
        vTaskDelay(DELAY_TIME / portTICK_PERIOD_MS);
    }
}

void init_request(led_strip_t *strip)
{
    // create the JSON requests task, pass in the led strip pointer for access
    xTaskCreate(&get_surline_data, "get_surline_data", 8192, strip, 5, NULL);
}