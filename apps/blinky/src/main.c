/**
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 * 
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <assert.h>
#include <string.h>

#include "sysinit/sysinit.h"
#include "os/os.h"
#include "bsp/bsp.h"
#include "hal/hal_gpio.h"
#ifdef ARCH_sim
#include "mcu/mcu_sim.h"
#endif

//#include "hal/hal_I2C.h"
#include "hal/hal_uart.h"
#include "console/console.h"
#include "lcd/stm32_adafruit_lcd.h"

#define BUFSIZE 100

#define MAX_INPUT 100

// Static value for write and read process 
//static uint8_t buffer[BUFSIZE];
//static uint8_t data_pos=0;

/* New task */
#include "screentask.h"
#include "flashtask.h"

/* First task and LED toggling */
static volatile uint8_t g_task1_loops;
int g_led_pin;

///////////////////////////////////////////////////////// Todoo structure
#include "todoo_data.h"

struct Todoo_data *todoo = NULL;
static void init_structure_exemple(struct Todoo_data *todoo);

// 1 /////////////////////////////////////////////////// Definition and calback for timer and GPIO interuption
// Define button and test point pin
#define BUTTON_ADV_PIN  8
#define TEST_POINT_6  15
// Timer task number and stask size.
#define MY_TIMER_INTERRUPT_TASK_PRIO  4
#define MY_TIMER_INTERRUPT_TASK_STACK_SZ    512

// initilize event call back
static void my_interrupt_ev_cb(struct os_event *);

static struct os_event gpio_ev = {
    .ev_cb = my_interrupt_ev_cb,
};

static void my_timer_ev_cb(struct os_event *);
static struct os_eventq my_timer_interrupt_eventq;

static os_stack_t my_timer_interrupt_task_stack[MY_TIMER_INTERRUPT_TASK_STACK_SZ];
static struct os_task my_timer_interrupt_task_str;

/* The timer callout */
static struct os_callout my_callout;
/*
 * Event callback function for the timer events every second.
 */
static void my_timer_ev_cb(struct os_event *ev)
{
    assert(ev != NULL);

    ++todoo->parameters->time[B_SEC];
    if(sec>59){
        todoo->parameters->time[B_SEC] = 0;
        ++todoo->parameters->time[B_MIN];
        if(min>59){
            todoo->parameters->time[B_MIN] = 0;
            ++todoo->parameters->time[B_HOUR];
            if(hour>23){
                todoo->parameters->time[B_HOUR] = 0;
            }

        }
    }

    os_callout_reset(&my_callout, OS_TICKS_PER_SEC);
}
static void my_timer_interrupt_task(void *arg)
{
    while (1) {
        os_eventq_run(&my_timer_interrupt_eventq);
    }
}
/*
 * Event callback function for interrupt events. Start advertising if button is pressed more than 8 sec
 */
static void my_interrupt_ev_cb(struct os_event *ev)
{
    assert(ev != NULL);

    static uint8_t capture_time, logic_time_pressed = 0;
    
    hal_gpio_write(TEST_POINT_6, 0);

    if(logic_time_pressed == 0){
        capture_time = sec;
        logic_time_pressed = 1;
    }else{
        logic_time_pressed = 0;
        if((sec-capture_time) >= 8){
            hal_gpio_write(TEST_POINT_6, 1);
        }
    }
}

static void
my_gpio_irq(void *arg)
{   
    // Post an interrupt event to the my_timer_interrupt_eventq event queue:
    os_eventq_put(&my_timer_interrupt_eventq, &gpio_ev);
}

/*
 * Task and interupt initialization for the Todoo app
 */
static void
init_todoo(void)
{
    /* 
     * Initialize and enable interrupts for the pin for button 1 and 
     * configure the button with pull up resistor on the nrf52dk.
     */ 
    hal_gpio_irq_init(BUTTON_ADV_PIN, my_gpio_irq, NULL, HAL_GPIO_TRIG_RISING, HAL_GPIO_PULL_UP);  
    hal_gpio_irq_enable(BUTTON_ADV_PIN);
    hal_gpio_init_out(TEST_POINT_6, 1);

    /* 
     * Initialize the data structure todoo and allocate 
     * enough memory depending of the number of activities
     */

    todoo = malloc(sizeof(struct Todoo_data));

    todoo->parameters = malloc(sizeof(struct Parameters));
    todoo->parameters->num_activity=2;
    todoo->activity = malloc(todoo->parameters->num_activity*sizeof(struct Activity));

    init_structure_exemple(todoo);
}
// 1 ///////////////////////////////////////////////////// 1 ///////////////////////////////////////////////////

static void
init_structure_exemple(struct Todoo_data *todoo){
    int i;

    todoo->parameters->theme = 1;
    todoo->parameters->transition = 1;
    todoo->parameters->num_activity = 2;

    todoo->parameters->date[0] = 15;
    todoo->parameters->date[1] = 15;
    todoo->parameters->date[2] = 15;

    todoo->parameters->time[0] = 15;
    todoo->parameters->time[1] = 15;
    todoo->parameters->time[2] = 15;


    for(i=0;i<todoo->parameters->num_activity;i++){
        todoo->activity[i].date[0] = 15;
        todoo->activity[i].date[1] = 15;
        todoo->activity[i].date[2] = 15;

        todoo->activity[i].start_time[0] = 15;
        todoo->activity[i].start_time[1] = 15 + 2*i;
        todoo->activity[i].start_time[2] = 15;

        todoo->activity[i].end_time[0] = 15;
        todoo->activity[i].end_time[1] = 16 + 2*i;
        todoo->activity[i].end_time[2] = 15;

        todoo->activity[i].data_add = 17;
        todoo->activity[i].data_size = 17;
    }

}

// 2 //////////////////////////////////////////////////////// 2 ////////////////////////////////////////////////



void r0_call_back(void); 

void r0_call_back(void){
    char buf[MAX_INPUT];
    int rc; 
    int full_line;
    int off;

    off = 0;
    while (1) {
        rc = console_read(buf + off, MAX_INPUT - off, &full_line);
        if (rc <= 0 && !full_line) {
            continue;
        }
        off += rc;
        if (!full_line) {
            if (off == MAX_INPUT) {
                /*
                 * Full line, no newline yet. Reset the input buffer.
                 */
                off = 0;
            }
            continue;
        }
        /* Got full line - break out of the loop and process the input data */
        hal_uart_blocking_tx(0, buf[0]);
        hal_uart_blocking_tx(0, buf[1]);
        hal_uart_blocking_tx(0, buf[2]);
        hal_uart_blocking_tx(0, buf[3]);
        hal_uart_blocking_tx(0, buf[4]);
        break;
    }
}



/*
uint8_t addr=0x55;
uint16_t r_nbytes=1;
uint16_t t_nbytes=1;
uint8_t rb[1]; 
uint8_t tb[1];    

//struct hal_i2c_master_data rxdata = {.address = addr, .len = rx_bytes, .buffer = rb};
struct hal_i2c_master_data txdata = {.address = 0x55, .len = 1, .buffer = tb};

uint8_t address = 0x55;
int result = 0;
*/

/**
 * main
 *
 * The main task for the project. This function initializes packages,
 * and then blinks the BSP LED in a loop.
 *
 * @return int NOTE: this function should never return!
 */
int
main(int argc, char **argv)
{
    int rc;

#ifdef ARCH_sim
    mcu_sim_parse_args(argc, argv);
#endif

    sysinit();


    /* Initialise new tasks. */
    os_task_init(&screentask, "screentask", screen_task_handler, NULL, 
    SCREENTASK_PRIO, OS_WAIT_FOREVER, screentask_stack,
    SCREENTASK_STACK_SIZE);
	
    os_task_init(&flashtask, "flashtask", flash_task_handler, NULL, 
    FLASHTASK_PRIO, OS_WAIT_FOREVER, flashtask_stack,
    FLASHTASK_STACK_SIZE);

    g_led_pin = LED_BLINK_PIN;
    hal_gpio_init_out(g_led_pin, 1);


    //console_init(console_rx_cb rx_cb);

    char *str = "Hello World!";
    char *ptr = str;

    while(*ptr) {
        hal_uart_blocking_tx(0, *ptr++);
    }
    hal_uart_blocking_tx(0, '\n');

    console_rx_cb rx_cb=&r0_call_back;

    console_init(rx_cb);
    


    // Initialize timer
    /* Use a dedicate event queue for timer and interrupt events */
    os_eventq_init(&my_timer_interrupt_eventq);
    /* 
     * Create the task to process timer and interrupt events from the
     * my_timer_interrupt_eventq event queue.
     */
    os_task_init(&my_timer_interrupt_task_str, "timer_interrupt_task", 
                    my_timer_interrupt_task, NULL, 
                    MY_TIMER_INTERRUPT_TASK_PRIO, OS_WAIT_FOREVER, 
                    my_timer_interrupt_task_stack, 
                    MY_TIMER_INTERRUPT_TASK_STACK_SZ);
    /* 
    * Initialize the callout for a timer event.  
    * The my_timer_ev_cb callback function processes the timer events.
    */
    os_callout_init(&my_callout, &my_timer_interrupt_eventq, my_timer_ev_cb, NULL);

    os_callout_reset(&my_callout, OS_TICKS_PER_SEC);

    
    // Task and interupt initialization for the Todoo app
    init_todoo();


    while (1) {
        ++g_task1_loops;
        
        /* Wait one second */
        os_time_delay(OS_TICKS_PER_SEC);

        /* Toggle the LED */
        hal_gpio_toggle(g_led_pin);


        //uart_hal_start_rx(struct uart_dev *dev);


        /*
        // Address: 0x550x55
        // RemainingCapacity():  0x0C and 0x0D
        //hal_i2c_master_read(0, &txdata, 50, 0);
        result = hal_i2c_master_probe(0, address, OS_TICKS_PER_SEC);
        result++;
        result--;
        */
    }
    assert(0);

    return rc;
}