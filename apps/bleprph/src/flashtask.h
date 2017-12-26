/* My Task */
struct os_task flashtask;
/* My Task Stack */
#define FLASHTASK_STACK_SIZE OS_STACK_ALIGN(256)
os_stack_t flashtask_stack[FLASHTASK_STACK_SIZE];
/* My Task Priority */
#define FLASHTASK_PRIO (110) // Main task priority = 127

void flash_task_handler(void *arg);

/* FIFO declaration between gatt service task and flash task */
#define FIFO_TASK_WIDTH  128
#define FIFO_TASK_HEIGHT 80 

#define nCS_LCD   (19)
#define SCK_LCD   (16)
#define MOSI_LCD  (17)
#define DC_LCD    (15)
#define PWM_LCD   (14)

typedef struct array
{
    uint8_t buffer[FIFO_TASK_WIDTH ];
} array_type;

typedef struct task_reader
{
    uint32_t maxcnt;
    uint32_t ptr;
    uint8_t fline;
} FIFO_task_reader_type;

extern array_type FIFO_task[FIFO_TASK_HEIGHT];
extern FIFO_task_reader_type FIFO_task_reader;
