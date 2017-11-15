#include <hal/hal_flash_int.h>
#include <hal/hal_spi.h>

volatile uint8_t min,sec,hour, day;
volatile uint32_t current_task_time;

/* My Task */
struct os_task screentask;
/* My Task Stack */
#define SCREENTASK_STACK_SIZE OS_STACK_ALIGN(256)
os_stack_t screentask_stack[SCREENTASK_STACK_SIZE];
/* My Task Priority */
#define SCREENTASK_PRIO (120) // Main task priority = 127

void screen_task_handler(void *arg);

void ext_memory_bitmap_to_LCD(uint16_t Xpos, uint16_t Ypos, uint32_t addr, const struct hal_flash * sst26_dev);

void send_8bit_serial(uint8_t *Data);