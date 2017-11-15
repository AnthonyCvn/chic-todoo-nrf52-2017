/* My Task */
struct os_task screentask;
/* My Task Stack */
#define SCREENTASK_STACK_SIZE OS_STACK_ALIGN(256)
os_stack_t screentask_stack[SCREENTASK_STACK_SIZE];
/* My Task Priority */
#define SCREENTASK_PRIO (120) // Main task priority = 127

void screen_task_handler(void *arg);

void send_8bit_serial(uint8_t *Data);