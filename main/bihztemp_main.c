#include <FreeRTOS.h>
#include <display.h>
#include <esp_log.h>
#include <task.h>

int app_main(void) {
    init_display();
    display_test("test");
    return 0;
}
