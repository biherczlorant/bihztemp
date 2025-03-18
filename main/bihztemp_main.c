#include <FreeRTOS.h>
#include <display.h>
#include <esp_log.h>
#include <task.h>

int app_main(void) {
    display_init();
    display_temp_and_hum_screen("21\xb0" "C", "70%");
    return 0;
}
