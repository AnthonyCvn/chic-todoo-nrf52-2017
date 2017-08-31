/* My Task */
struct os_task flashtask;
/* My Task Stack */
#define FLASHTASK_STACK_SIZE OS_STACK_ALIGN(256)
os_stack_t flashtask_stack[FLASHTASK_STACK_SIZE];
/* My Task Priority */
#define FLASHTASK_PRIO (110) // Main task priority = 127

void flash_task_handler(void *arg);