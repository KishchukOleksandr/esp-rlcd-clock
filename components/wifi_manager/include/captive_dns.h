#ifndef CAPTIVE_DNS_H
#define CAPTIVE_DNS_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Start the lightweight Captive Portal UDP DNS responder on port 53 (FR-003).
 *        Resolves wildcard A-record queries from SoftAP clients to `192.168.4.1`.
 *
 * @return ESP_OK on success.
 */
esp_err_t captive_dns_start(void);

/**
 * @brief Stop the Captive Portal UDP DNS responder if running.
 */
void captive_dns_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* CAPTIVE_DNS_H */
