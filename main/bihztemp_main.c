#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <task.h>

#include "dht/dht.h"
#include "display.h"
#include "network.h"

int app_main(void) {
    int server_delay = 0;

    display_init();
    dht_init(12, true);

    network_start();

    while (1) {
        dht_write_sensor_data_to_display();
        if (network_is_connected() && ++server_delay == 6) {
            network_push_data_to_server();
            server_delay = 0;
        }

        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
    return 0;
}
