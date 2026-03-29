#ifndef _WIFI_H
#define _WIFI_H

#include <time.h>

/*
 *
 */
void wifi_init_sta(void);

/*
 * You should provide this from wifi.c or some time module.
 * Return Unix timestamp from SNTP/network.
 * Return 0 if time is not valid yet.
 */
time_t wifi_get_network_time(void);

/*
 *
 */
void wifi_sync_time_from_network(void);

#endif