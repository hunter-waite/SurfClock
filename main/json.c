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

/* Constants that aren't configurable in menuconfig */

/* Long range not sure if this solution would works as the surline API is not 
 * documented and can be changed at any time without notice */
#define WEB_SERVER "services.surfline.com"  // surfline api host
#define WEB_PORT "80"                       // http port
/* oceanside spot ID */
#define WEB_PATH "/kbyg/regions/forecasts/conditions?subregionId=58581a836630e24c44878fd7&days=1"

#define DELAY_TIME  10000   // 10 second delay time between each get request

#define BUFFER_SIZE 2048    // 2500 B byte character buffer

static const char *T = "JSON Parser";

static const char *REQUEST = "GET " WEB_PATH " HTTP/1.0\r\n"
    "Host: "WEB_SERVER":"WEB_PORT"\r\n"
    "User-Agent: esp-idf/1.0 esp32\r\n"
    "\r\n";

/* takes in a string and uses CJSON to parse objects, for now prints to 
    terminal */
void parse_json(char *recv_buf, int recv_len)
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

    /* get rid of the HTTP header */
    char *content = strstr(recv_buf, "\r\n\r\n");

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
        ESP_LOGE(T, "\tError When First Parse\n");
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
    }

    // clear all old JSON values
    cJSON_Delete(json);
    return;

}

static void get_surline_data(void *pvParameters)
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

        // wait 2 seconds to make sure the socket has completely recieved the message
        vTaskDelay(2 / portTICK_PERIOD_MS);

        // read from the socket and get data
        r = read(s, recv_buf, sizeof(recv_buf));

        // parse the incoming json for data needed
        parse_json(recv_buf, r);

        ESP_LOGI(T, "... done reading from socket. Last read return=%d errno=%d.", r, errno);
        close(s);

        // delay until calling again
        vTaskDelay(DELAY_TIME / portTICK_PERIOD_MS);
    }
}

void init_request(void)
{
    xTaskCreate(&get_surline_data, "get_surline_data", 4096, NULL, 5, NULL);
}