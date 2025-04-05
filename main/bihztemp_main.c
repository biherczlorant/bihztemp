#include <dht/dht.h>
#include <display.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <task.h>

int app_main(void) {
    display_init();
    dht_init(12, true);

    while (1) {
        dht_write_sensor_data_to_display();
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    return 0;
}
