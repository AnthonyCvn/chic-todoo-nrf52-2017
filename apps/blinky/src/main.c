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

static volatile int g_task1_loops;

/* For LED toggling */
int g_led_pin;
   

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