#include <display.h>
#include <FreeRTOS.h>
#include <task.h>
#include <esp_log.h>

int app_main(void){
    init_display();
    display_test("test");
    return 0;
}

