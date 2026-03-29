#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Start the HTTPS server.
 *
 * Returns:
 * - ESP_OK on success
 * - error code otherwise
 */
esp_err_t https_server_start(void);

/**
 * Stop the HTTPS server if running.
 *
 * Returns:
 * - ESP_OK on success
 * - error code otherwise
 */
esp_err_t https_server_stop(void);

/**
 * Get the current server handle.
 *
 * Returns NULL if server is not running.
 */
httpd_handle_t https_server_get_handle(void);

#ifdef __cplusplus
}
#endif
