/*
 * CHIC - China Hardware Innovation Camp - Todoo
 * https://chi.camp/projects/todoo/
 *
 * Anthony Cavin
 * anthony.cavin.ac@gmail.com
 * 2018, January 11
 *
 * Main function to initialize
 * - Bluetooth nimble
 * - RTC clock
 * - GPIO interuption
 * - Screentask (Manage LCD, see screentask.c) 
 * - Flashtask (Manage external memory, see flashtask.c) 
 * 
 * Based on BLEPRPH example and under Apache mynewt license:
 * 
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
#include <stdio.h>
#include <errno.h>
#include "sysinit/sysinit.h"
#include "bsp/bsp.h"
#include "os/os.h"
#include "bsp/bsp.h"
#include "hal/hal_gpio.h"
#include "console/console.h"
#include "hal/hal_system.h"
#include "config/config.h"
#include "split/split.h"

/* BLE */
#include "nimble/ble.h"
#include "host/ble_hs.h"
#include "services/gap/ble_svc_gap.h"

/* Application-specified header. */
#include "bleprph.h"

/* New task */
#include "screentask.h"
#include "flashtask.h"
#include "todoo_data.h"

/** Log data. */
struct log bleprph_log;

static int bleprph_gap_event(struct ble_gap_event *event, void *arg);


// 1 /////////////////////////////////////////////////// Definition and calback for timer and GPIO interuption
/* 
*  Definition and calback for timer and GPIO interuption
*/
// Define button and LED
#define LED_BLUE    7
#define LED_GREEN   8
#define LED_RED     9
#define BUTTON_ADV_PIN  25
#define STANDBY    26

// Timer task number and stask size.
#define MY_TIMER_INTERRUPT_TASK_PRIO  50 
#define MY_TIMER_INTERRUPT_TASK_STACK_SZ    (512/4)

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

    todoo->parameters->time[B_SEC] = (todoo->parameters->time[B_SEC] + 1) % 60;   
    if(!todoo->parameters->time[B_SEC]){   
        todoo->parameters->time[B_MIN] = (todoo->parameters->time[B_MIN] + 1) % 60;   
        if(!todoo->parameters->time[B_MIN]){
            todoo->parameters->time[B_HOUR] = (todoo->parameters->time[B_HOUR] + 1) % 12;
        }         
    }     
    
    if(current_task_time > 0){
        --current_task_time;
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
    
    hal_gpio_toggle(LED_BLUE);

    
    if(logic_time_pressed == 0){
        capture_time = sec;
        logic_time_pressed = 1;
    }else{
        logic_time_pressed = 0;
        if((sec-capture_time) >= 8){
            //hal_gpio_write(LED_BLUE, 1);
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
    hal_gpio_init_out(STANDBY, 1);

    
    todoo = malloc(sizeof(struct Todoo_data));
    todoo->parameters = malloc(sizeof(struct Parameters));
}
// 1 ///////////////////////////////////////////////////// 1 ///////////////////////////////////////////////////


static void
bleprph_advertise(void)
{
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;
    const char *name;
    int rc;

    /**
     *  Set the advertisement data included in our advertisements:
     *     o Flags (indicates advertisement type and other general info).
     *     o Advertising tx power.
     *     o Device name.
     *     o 16-bit service UUIDs (alert notifications).
     */

    memset(&fields, 0, sizeof fields);

    /* Advertise two flags:
     *     o Discoverability in forthcoming advertisement (general)
     *     o BLE-only (BR/EDR unsupported).
     */
    fields.flags = BLE_HS_ADV_F_DISC_GEN |
                   BLE_HS_ADV_F_BREDR_UNSUP;

    /* Indicate that the TX power level field should be included; have the
     * stack fill this value automatically.  This is done by assiging the
     * special value BLE_HS_ADV_TX_PWR_LVL_AUTO.
     */
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;

    name = ble_svc_gap_device_name();
    fields.name = (uint8_t *)name;
    fields.name_len = strlen(name);
    fields.name_is_complete = 1;

    
    fields.uuids16 = (ble_uuid16_t[]){
        BLE_UUID16_INIT(GATT_SVR_SVC_ALERT_UUID)
    };
    fields.num_uuids16 = 1;
    fields.uuids16_is_complete = 1;
    

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        BLEPRPH_LOG(ERROR, "error setting advertisement data; rc=%d\n", rc);
        return;
    }

    /* Begin advertising. */
    memset(&adv_params, 0, sizeof adv_params);
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER,
                           &adv_params, bleprph_gap_event, NULL);
    if (rc != 0) {
        BLEPRPH_LOG(ERROR, "error enabling advertisement; rc=%d\n", rc);
        return;
    }
}

/**
 * The nimble host executes this callback when a GAP event occurs.  The
 * application associates a GAP event callback with each connection that forms.
 * bleprph uses the same callback for all connections.
 *
 * @param event                 The type of event being signalled.
 * @param ctxt                  Various information pertaining to the event.
 * @param arg                   Application-specified argument; unuesd by
 *                                  bleprph.
 *
 * @return                      0 if the application successfully handled the
 *                                  event; nonzero on failure.  The semantics
 *                                  of the return code is specific to the
 *                                  particular GAP event being signalled.
 */
static int
bleprph_gap_event(struct ble_gap_event *event, void *arg)
{
    struct ble_gap_conn_desc desc;
    int rc;

    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        /* A new connection was established or a connection attempt failed. */
        BLEPRPH_LOG(INFO, "connection %s; status=%d ",
                       event->connect.status == 0 ? "established" : "failed",
                       event->connect.status);
        if (event->connect.status == 0) {
            rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
            assert(rc == 0);
            // bleprph_print_conn_desc(&desc);
        }
        BLEPRPH_LOG(INFO, "\n");

        if (event->connect.status != 0) {
            /* Connection failed; resume advertising. */
            bleprph_advertise();
        }
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        BLEPRPH_LOG(INFO, "disconnect; reason=%d ", event->disconnect.reason);
        // bleprph_print_conn_desc(&event->disconnect.conn);
        BLEPRPH_LOG(INFO, "\n");

        /* Connection terminated; resume advertising. */
        bleprph_advertise();
        return 0;

    case BLE_GAP_EVENT_CONN_UPDATE:
        /* The central has updated the connection parameters. */
        BLEPRPH_LOG(INFO, "connection updated; status=%d ",
                    event->conn_update.status);
        rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
        assert(rc == 0);
        // bleprph_print_conn_desc(&desc);
        BLEPRPH_LOG(INFO, "\n");
        return 0;

    case BLE_GAP_EVENT_ENC_CHANGE:
        /* Encryption has been enabled or disabled for this connection. */
        BLEPRPH_LOG(INFO, "encryption change event; status=%d ",
                    event->enc_change.status);
        rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
        assert(rc == 0);
        // bleprph_print_conn_desc(&desc);
        BLEPRPH_LOG(INFO, "\n");
        return 0;

    case BLE_GAP_EVENT_SUBSCRIBE:
        BLEPRPH_LOG(INFO, "subscribe event; conn_handle=%d attr_handle=%d "
                          "reason=%d prevn=%d curn=%d previ=%d curi=%d\n",
                    event->subscribe.conn_handle,
                    event->subscribe.attr_handle,
                    event->subscribe.reason,
                    event->subscribe.prev_notify,
                    event->subscribe.cur_notify,
                    event->subscribe.prev_indicate,
                    event->subscribe.cur_indicate);
        return 0;

    case BLE_GAP_EVENT_MTU:
        BLEPRPH_LOG(INFO, "mtu update event; conn_handle=%d cid=%d mtu=%d\n",
                    event->mtu.conn_handle,
                    event->mtu.channel_id,
                    event->mtu.value);
        return 0;
    }

    return 0;
}

static void
bleprph_on_reset(int reason)
{
    BLEPRPH_LOG(ERROR, "Resetting state; reason=%d\n", reason);
}

static void
bleprph_on_sync(void)
{
    /* Begin advertising. */
    bleprph_advertise();
}


// GPIO configuration
int g_led_pin;


/**
 * main
 *
 * The main task for the project. This function initializes the packages,
 * then starts serving events from default event queue.
 */
int
main(void)
{
    int rc;

    /* Initialize OS */
    sysinit();

    /* Set initial BLE device address. */
    memcpy(g_dev_addr, (uint8_t[6]){0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a}, 6);

    /* Initialize the bleprph log. */
    log_register("bleprph", &bleprph_log, &log_console_handler, NULL,
                 LOG_SYSLEVEL);

    /* Initialize the NimBLE host configuration. */
    log_register("ble_hs", &ble_hs_log, &log_console_handler, NULL,
                 LOG_SYSLEVEL);
    ble_hs_cfg.reset_cb = bleprph_on_reset;
    ble_hs_cfg.sync_cb = bleprph_on_sync;
    ble_hs_cfg.gatts_register_cb = gatt_svr_register_cb;

    rc = gatt_svr_init();
    assert(rc == 0);

    /* Set the default device name. */
    rc = ble_svc_gap_device_name_set("Todoo");
    assert(rc == 0);

    conf_load();
    
    /* If this app is acting as the loader in a split image setup, jump into
     * the second stage application instead of starting the OS.
     */
#if MYNEWT_VAL(SPLIT_LOADER)
    {
        void *entry;
        rc = split_app_go(&entry, true);
        if (rc == 0) {
            hal_system_start(entry);
        }
    }
#endif

    /*
     * As the last thing, process events from default event queue.
     */

    /* Initialise new tasks. */ 
    os_task_init(&screentask, "screentask", screen_task_handler, NULL, 
    SCREENTASK_PRIO, OS_WAIT_FOREVER, screentask_stack,
    SCREENTASK_STACK_SIZE);
	    
    os_task_init(&flashtask, "flashtask", flash_task_handler, NULL, 
    FLASHTASK_PRIO, OS_WAIT_FOREVER, flashtask_stack,
    FLASHTASK_STACK_SIZE);


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

    /* GPIO intit (or LED toggling only)*/
    int r_led_pin;
    r_led_pin = LED_RED;
    hal_gpio_init_out(r_led_pin, 0);

    int g_led_pin;
    g_led_pin = LED_GREEN;
    hal_gpio_init_out(g_led_pin, 0);

    int b_led_pin;
    b_led_pin = LED_BLUE;
    hal_gpio_init_out(b_led_pin, 0);

    while (1) {
        os_eventq_run(os_eventq_dflt_get());
    }
    return 0;
}
