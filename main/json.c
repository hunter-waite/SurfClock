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
#include "tag.h"

/* Constants that aren't configurable in menuconfig */

/* Long range not sure if this solution would works as the surline API is not 
 * documented and can be changed at any time without notice */
#define WEB_SERVER "services.surfline.com"  // surfline api host
#define WEB_PORT "80"                       // http port
/* oceanside spot ID */
#define WEB_PATH "/kbyg/regions/forecasts/conditions?subregionId=58581a836630e24c44878fd7&days=1"

#define DELAY_TIME  10000   // 10 second delay time between each get request

#define BUFFER_SIZE 2048    // 1000 byte character buffer

static const char *REQUEST = "GET " WEB_PATH " HTTP/1.0\r\n"
    "Host: "WEB_SERVER":"WEB_PORT"\r\n"
    "User-Agent: esp-idf/1.0 esp32\r\n"
    "\r\n";

void parse_json(char *json, int json_len)
{
    ESP_LOGI(TAG, "%s\n", json);
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
            ESP_LOGE(TAG, "DNS lookup failed err=%d res=%p", err, res);
            vTaskDelay(DELAY_TIME / portTICK_PERIOD_MS);
            continue;
        }

        /* Code to print the resolved IP.
            Note: inet_ntoa is non-reentrant, look at ipaddr_ntoa_r for "real" code */
        addr = &((struct sockaddr_in *)res->ai_addr)->sin_addr;
        ESP_LOGI(TAG, "DNS lookup succeeded. IP=%s", inet_ntoa(*addr));

        s = socket(res->ai_family, res->ai_socktype, 0);
        if(s < 0) {
            ESP_LOGE(TAG, "... Failed to allocate socket.");
            freeaddrinfo(res);
            vTaskDelay(DELAY_TIME / portTICK_PERIOD_MS);
            continue;
        }
        ESP_LOGI(TAG, "... allocated socket");

        if(connect(s, res->ai_addr, res->ai_addrlen) != 0) {
            ESP_LOGE(TAG, "... socket connect failed errno=%d", errno);
            close(s);
            freeaddrinfo(res);
            vTaskDelay(DELAY_TIME / portTICK_PERIOD_MS);
            continue;
        }

        ESP_LOGI(TAG, "... connected");
        freeaddrinfo(res);

        if (write(s, REQUEST, strlen(REQUEST)) < 0) {
            ESP_LOGE(TAG, "... socket send failed");
            close(s);
            vTaskDelay(DELAY_TIME / portTICK_PERIOD_MS);
            continue;
        }
        ESP_LOGI(TAG, "... socket send success");

        struct timeval receiving_timeout;
        receiving_timeout.tv_sec = 5;
        receiving_timeout.tv_usec = 0;
        if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &receiving_timeout,
                sizeof(receiving_timeout)) < 0) {
            ESP_LOGE(TAG, "... failed to set socket receiving timeout");
            close(s);
            vTaskDelay(DELAY_TIME / portTICK_PERIOD_MS);
            continue;
        }
        ESP_LOGI(TAG, "... set socket receiving timeout success");

        bzero(recv_buf, sizeof(recv_buf));
        r = read(s, recv_buf, sizeof(recv_buf)-1);
        for(int i = 0; i < r; i++) {
            putchar(recv_buf[i]);
        }

        parse_json(recv_buf, r);

        ESP_LOGI(TAG, "... done reading from socket. Last read return=%d errno=%d.", r, errno);
        close(s);
        vTaskDelay(DELAY_TIME / portTICK_PERIOD_MS);
    }
}

void init_request(void)
{
    xTaskCreate(&get_surline_data, "get_surline_data", 4096, NULL, 5, NULL);
}