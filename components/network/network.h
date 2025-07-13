#ifndef COMPONENTS_NETWORK_NETWORK_H_
#define COMPONENTS_NETWORK_NETWORK_H_

#include <stdbool.h>
#include <stdint.h>

void network_start(void);

bool network_is_connected(void);
uint8_t* network_get_ap_name(void);

void network_push_data_to_server(void);

#endif  // COMPONENTS_NETWORK_NETWORK_H_
