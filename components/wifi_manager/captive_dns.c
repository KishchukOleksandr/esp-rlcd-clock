#include "captive_dns.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"

static const char *TAG = "captive_dns";

#define DNS_PORT                53
#define DNS_MAX_PACKET_SIZE     256
#define DNS_TASK_STACK_SIZE     3072
#define DNS_TASK_PRIORITY       4

/* SoftAP IP 192.168.4.1 in network byte order */
static const uint8_t s_ap_ip_octets[4] = {192, 168, 4, 1};

static int s_dns_sock = -1;
static volatile bool s_dns_running = false;
static TaskHandle_t s_dns_task_handle = NULL;

typedef struct __attribute__((packed)) {
    uint16_t id;
    uint16_t flags;
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t nscount;
    uint16_t arcount;
} dns_header_t;

static void captive_dns_task(void *arg)
{
    (void)arg;
    uint8_t rx_buf[DNS_MAX_PACKET_SIZE];
    uint8_t tx_buf[DNS_MAX_PACKET_SIZE + 16];

    while (s_dns_running && s_dns_sock >= 0) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);

        int len = recvfrom(s_dns_sock, rx_buf, sizeof(rx_buf), 0,
                           (struct sockaddr *)&client_addr, &addr_len);
        if (len < (int)sizeof(dns_header_t)) {
            continue;
        }

        const dns_header_t *req_hdr = (const dns_header_t *)rx_buf;
        uint16_t qdcount = ntohs(req_hdr->qdcount);
        if (qdcount == 0U || len > DNS_MAX_PACKET_SIZE) {
            continue;
        }

        /* Find end of first question QNAME + QTYPE(2) + QCLASS(2) */
        int idx = (int)sizeof(dns_header_t);
        while (idx < len && rx_buf[idx] != 0U) {
            idx += (int)rx_buf[idx] + 1;
        }
        idx += 5; /* Null label (1) + QTYPE (2) + QCLASS (2) */
        if (idx > len) {
            continue;
        }

        memcpy(tx_buf, rx_buf, (size_t)idx);
        dns_header_t *resp_hdr = (dns_header_t *)tx_buf;
        resp_hdr->flags = htons(0x8180U); /* Standard query response, no error */
        resp_hdr->qdcount = htons(1U);
        resp_hdr->ancount = htons(1U);
        resp_hdr->nscount = 0U;
        resp_hdr->arcount = 0U;

        /* Append DNS A-record answer pointing to 192.168.4.1 */
        uint8_t answer[16] = {
            0xC0, 0x0C,             /* Pointer to QNAME at offset 12 */
            0x00, 0x01,             /* TYPE A (IPv4) */
            0x00, 0x01,             /* CLASS IN */
            0x00, 0x00, 0x00, 0x1E, /* TTL = 30 seconds */
            0x00, 0x04,             /* RDLENGTH = 4 bytes */
            s_ap_ip_octets[0],
            s_ap_ip_octets[1],
            s_ap_ip_octets[2],
            s_ap_ip_octets[3],
        };

        memcpy(&tx_buf[idx], answer, sizeof(answer));
        int resp_len = idx + (int)sizeof(answer);

        sendto(s_dns_sock, tx_buf, (size_t)resp_len, 0,
               (const struct sockaddr *)&client_addr, addr_len);
    }

    s_dns_task_handle = NULL;
    vTaskDelete(NULL);
}

esp_err_t captive_dns_start(void)
{
    if (s_dns_running) {
        return ESP_OK;
    }

    s_dns_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s_dns_sock < 0) {
        ESP_LOGE(TAG, "Failed to create UDP socket for Captive Portal DNS");
        return ESP_FAIL;
    }

    struct timeval tv = {
        .tv_sec = 1,
        .tv_usec = 0,
    };
    setsockopt(s_dns_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(DNS_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };

    if (bind(s_dns_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        ESP_LOGE(TAG, "Failed to bind Captive Portal DNS socket on port %d", DNS_PORT);
        close(s_dns_sock);
        s_dns_sock = -1;
        return ESP_FAIL;
    }

    s_dns_running = true;
    BaseType_t ret = xTaskCreate(captive_dns_task, "captive_dns", DNS_TASK_STACK_SIZE,
                                 NULL, DNS_TASK_PRIORITY, &s_dns_task_handle);
    if (ret != pdPASS) {
        s_dns_running = false;
        close(s_dns_sock);
        s_dns_sock = -1;
        ESP_LOGE(TAG, "Failed to create captive_dns task");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Captive Portal DNS responder started on UDP port %d (-> 192.168.4.1)", DNS_PORT);
    return ESP_OK;
}

void captive_dns_stop(void)
{
    s_dns_running = false;
    if (s_dns_sock >= 0) {
        close(s_dns_sock);
        s_dns_sock = -1;
    }
}
