/* 
 * CHIC - China Hardware Innovation Camp - Todoo
 * https://chi.camp/projects/todoo/
 *
 * Anthony Cavin
 * anthony.cavin.ac@gmail.com
 * 2018, January 11
 * 
 * Receive data from the Todoo android application through a Gatt server.
 * 
 * The data are receive according to the following order:
 * 1) 1B theme
 * 2) 3B heure actuel [Heur] [minute] [second]
 * 3) 1B date [jour de la semaine] 0-6 du lundi au dimanche
 * 4) 1B nombre d'activité envoyé
 * 5  for(première activité à la dernière)
 * 6)   1B indiquer le jour de l'activité qui arrive, 0 pour le lundi 
 * 7)   2B début d'activité [heure] [minute]
 * 8)   2B fin d'activité [heure] [minute]
 * 9) end
 * 5  for(première activité à la dernière)
 * 6)   16200B image 90px90p en bitmap de chaque activité 
 * 9) end
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
#include <stdio.h>
#include <string.h>
#include "bsp/bsp.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "bleprph.h"

/* Include for SPI and LCD driver */
#include <hal/hal_spi.h>
#include "lcd/lcd.h"
#include "lcd/st7735.h"
#include "lcd/stm32_adafruit_lcd.h"
#include "hal/hal_gpio.h"
// #include "mcu/nrf51_hal.h"

#include "flashtask.h"

#include <SST26/SST26.h>
#include "mcu/nrf52_hal.h"

#include "todoo_data.h"



// Todoo structure to managae each activity
struct Todoo_data *todoo = NULL;

// SPI for LCD
static struct hal_spi_settings screen_SPI_settings = {
    .data_order = HAL_SPI_MSB_FIRST,
    .data_mode  = HAL_SPI_MODE3,
    .baudrate   = 8000, // max 8000
    .word_size  = HAL_SPI_WORD_SIZE_8BIT,
};

// SPI for memory settings
struct nrf52_hal_spi_cfg spi_cfg = {
    .sck_pin      = 16 ,
    .mosi_pin     = 17,
    .miso_pin     = 18,
    .ss_pin       = 20 
};
struct sst26_dev *my_sst26_dev = NULL;


uint8_t rxbuf[5];

// GPIO declaration
int g_led_pin;
int ncs_lcd;
int sck_lcd;  
int mosi_lcd;  
int dc_lcd; 
int pwm_lcd; 

// SPI MEMORY TEST
void SPI_MEMORY_init(void);

#define MESSAGE_SIZE 128
//uint8_t databuf[25];

// Static value for write and read process 
static uint8_t gatt_svr_data_trans[MESSAGE_SIZE];


/*** UUID for custom Bluetooth service and characteristic */ 
// Service  497e3f64-2806-11e7-93ae-92361f002671
static const ble_uuid128_t gatt_svr_todoo_uuid =
        BLE_UUID128_INIT(0x71, 0x26, 0x00, 0x1f, 0x36, 0x92, 0xae, 0x93,
                         0xe7 ,0x11, 0x06, 0x28, 0x64, 0x3f, 0x7e, 0x49);
// Characteristic 497e41e4-2806-11e7-93ae-92361f002671 
static const ble_uuid128_t gatt_svr_chr_todoo_trans_uuid =
        BLE_UUID128_INIT(0x71, 0x26, 0x00, 0x1f, 0x36, 0x92, 0xae, 0x93,
                         0xe7 ,0x11, 0x06, 0x28, 0xe4, 0x41, 0x7e, 0x49);

// Call back function for the GATT custom service to transfer data
static int
gatt_svr_chr_trans_data(uint16_t conn_handle, uint16_t attr_handle,
                             struct ble_gatt_access_ctxt *ctxt,
                             void *arg);

static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    {
         //  GATT Service to transfer data (smartphone to device)
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &gatt_svr_todoo_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) { {
             //Characteristic write data 
            .uuid = &gatt_svr_chr_todoo_trans_uuid.u,
            .access_cb = gatt_svr_chr_trans_data,
            .flags = BLE_GATT_CHR_F_READ |
                     BLE_GATT_CHR_F_WRITE,
        }, {
            0, /* No more characteristics in this service. */
        } },
    },

    {
        0,  /* No more services. */
    },
};

static int
gatt_svr_chr_write(struct os_mbuf *om, uint16_t min_len, uint16_t max_len,
                   void *dst, uint16_t *len)
{
    uint16_t om_len;
    int rc;

    om_len = OS_MBUF_PKTLEN(om);
    if (om_len < min_len || om_len > max_len) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }

    rc = ble_hs_mbuf_to_flat(om, dst, max_len, len);
    if (rc != 0) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    return 0;
}

// Call back function for a custom GATT Service
static int
gatt_svr_chr_trans_data(uint16_t conn_handle, uint16_t attr_handle,
                             struct ble_gatt_access_ctxt *ctxt,
                             void *arg)
{
    const ble_uuid_t *uuid;
    int rc;
    static uint8_t first_packet=1;

    uuid = ctxt->chr->uuid;

    /* Determine which characteristic is being accessed by examining its
     * 128-bit UUID.
     */

    if (ble_uuid_cmp(uuid, &gatt_svr_chr_todoo_trans_uuid.u) == 0) {
        switch (ctxt->op) {
        case BLE_GATT_ACCESS_OP_READ_CHR :
            rc = os_mbuf_append(ctxt->om, &gatt_svr_data_trans[0],
                                sizeof gatt_svr_data_trans);

            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;

        case BLE_GATT_ACCESS_OP_WRITE_CHR :
            rc = gatt_svr_chr_write(ctxt->om,
                                    1, //sizeof gatt_svr_data_trans
                                    sizeof gatt_svr_data_trans,
                                    &gatt_svr_data_trans[0], NULL);
            
            
            if(first_packet){
                /* 
                * Initialize the data structure todoo and allocate 
                * enough memory depending of the number of activities
                */
                /*
                todoo = malloc(sizeof(struct Todoo_data));
                todoo->parameters = malloc(sizeof(struct Parameters));
                */
                todoo->parameters->time[B_HOUR] = gatt_svr_data_trans[1];
                todoo->parameters->time[B_MIN]  = gatt_svr_data_trans[2];
                todoo->parameters->time[B_SEC]  = gatt_svr_data_trans[3];
                todoo->parameters->day  = gatt_svr_data_trans[4];
                todoo->parameters->num_activity  = gatt_svr_data_trans[5];

                todoo->activity = malloc(todoo->parameters->num_activity*sizeof(struct Activity));
         
                int i_act=0;
                for(i_act=0;i_act<todoo->parameters->num_activity;i_act++){
                    todoo->activity[i_act].day  = gatt_svr_data_trans[6+i_act*5];
                    todoo->activity[i_act].start_time[B_HOUR] = gatt_svr_data_trans[7+i_act*5];
                    todoo->activity[i_act].start_time[B_MIN]  = gatt_svr_data_trans[8+i_act*5];
                    todoo->activity[i_act].end_time[B_HOUR] = gatt_svr_data_trans[9+i_act*5];
                    todoo->activity[i_act].end_time[B_MIN]  = gatt_svr_data_trans[10+i_act*5];
                    todoo->activity[i_act].data_add  = ADD_FIRST_ACTIVITY_PIC+i_act*NUM_BYTE_ACTIVITY_PIC;
                }

                //memcpy(&FIFO_task[FIFO_task_reader.fline], &gatt_svr_data_trans[11*todoo->parameters->num_activity], MESSAGE_SIZE-11*todoo->parameters->num_activity);
                FIFO_task[FIFO_task_reader.fline].N = MESSAGE_SIZE-11*todoo->parameters->num_activity;
                FIFO_task_reader.fline ++;
                first_packet = 0;

                todoo->config_state = 1;
                todoo->which_state = shows_activity;
                
                // Print parameters (for debug only)
                /*
                BSP_LCD_DisplayChar(10, 0, todoo->parameters->time[B_HOUR]/10+48);
                BSP_LCD_DisplayChar(15, 0, todoo->parameters->time[B_HOUR]%10+48);
                BSP_LCD_DisplayChar(25, 0, todoo->parameters->time[B_MIN]/10+48);
                BSP_LCD_DisplayChar(30, 0, todoo->parameters->time[B_MIN]%10+48);
                BSP_LCD_DisplayChar(40, 0,todoo->parameters->time[B_SEC]/10+48);
                BSP_LCD_DisplayChar(45, 0,todoo->parameters->time[B_SEC]%10+48);
                BSP_LCD_DisplayChar(10, 10, 48+todoo->parameters->day);
                BSP_LCD_DisplayChar(10, 20, 48+todoo->parameters->num_activity);
                for(i_act=0;i_act<todoo->parameters->num_activity;i_act++){
                    BSP_LCD_DisplayChar(10+i_act*20, 40, 48+todoo->activity[i_act].day );
                    BSP_LCD_DisplayChar(10+i_act*20, 50, todoo->activity[i_act].start_time[B_HOUR]/10+48);
                    BSP_LCD_DisplayChar(15+i_act*20, 50, todoo->activity[i_act].start_time[B_HOUR]%10+48);
                    BSP_LCD_DisplayChar(10+i_act*20, 60, todoo->activity[i_act].start_time[B_MIN]/10+48);
                    BSP_LCD_DisplayChar(15+i_act*20, 60, todoo->activity[i_act].start_time[B_MIN]%10+48);
                    BSP_LCD_DisplayChar(10+i_act*20, 70, todoo->activity[i_act].end_time[B_HOUR]/10+48);
                    BSP_LCD_DisplayChar(15+i_act*20, 70, todoo->activity[i_act].end_time[B_HOUR]%10+48);
                    BSP_LCD_DisplayChar(10+i_act*20, 80, todoo->activity[i_act].end_time[B_MIN]/10+48);
                    BSP_LCD_DisplayChar(15+i_act*20, 80, todoo->activity[i_act].end_time[B_MIN]%10+48);
                }
                */
                //st7735_SetDisplayWindow(20, 20, 90, 90);
                //st7735_WriteReg(LCD_REG_54, 0x48);
                //st7735_SetCursor(20, 20); 
               //LCD_IO_WriteMultipleData((uint8_t*) &gatt_svr_data_trans[11], MESSAGE_SIZE-11);
            }else
            {
                //memcpy(&FIFO_task[FIFO_task_reader.fline], &gatt_svr_data_trans[1], MESSAGE_SIZE-1);
                FIFO_task[FIFO_task_reader.fline].N = MESSAGE_SIZE-1;
                FIFO_task_reader.fline ++;

                //state->which =  shows_activity;
                //state->config = 1;
                //mystate=shows_activity;
                //myconfig=1;
                //STATE_ENUM temp = shows_activity;
                //uint8_t conf_temp = 1;
                //memcpy(&mystate, &temp, 1);
                //memcpy(&myconfig, &conf_temp, 1);

            }


            //if(i>=16200){
                //BSP_LCD_DrawBitmap(20,20,todoo->Bpic);
            //}


            /*
            * TEST between the FIFO and the external memory
            */
            //memcpy(&FIFO_task[FIFO_task_reader.fline], &gatt_svr_data_trans[0], MESSAGE_SIZE);
            //FIFO_task_reader.fline ++;

            // TEST WITH MEMORY
            //sst26_write((struct hal_flash *) my_sst26_dev, i, &gatt_svr_data_trans[0], MESSAGE_SIZE);
            //i+=MESSAGE_SIZE;

            /*
            * TEST WITH LCD: write 128 pixel per 128 pixel from the smartphone
            */
            //LCD_IO_WriteMultipleData((uint8_t*) &gatt_svr_data_trans[0], MESSAGE_SIZE);
            
            /* 
             * OTHER TEST WITH LCD: write the caracter sended
            */
            //BSP_LCD_DisplayChar(40, 40, gatt_svr_data_trans[0]);
            //BSP_LCD_DisplayChar(50, 50, gatt_svr_data_trans[1]);
            return rc;

        default:
            assert(0);
            return BLE_ATT_ERR_UNLIKELY;
        }
    }

    /* Unknown characteristic; the nimble stack should not have called this
     * function.
     */
          
    assert(0);
    return BLE_ATT_ERR_UNLIKELY;
}

void
gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg)
{
    char buf[BLE_UUID_STR_LEN];

    switch (ctxt->op) {
    case BLE_GATT_REGISTER_OP_SVC:
        BLEPRPH_LOG(DEBUG, "registered service %s with handle=%d\n",
                    ble_uuid_to_str(ctxt->svc.svc_def->uuid, buf),
                    ctxt->svc.handle);
        break;

    case BLE_GATT_REGISTER_OP_CHR:
        BLEPRPH_LOG(DEBUG, "registering characteristic %s with "
                           "def_handle=%d val_handle=%d\n",
                    ble_uuid_to_str(ctxt->chr.chr_def->uuid, buf),
                    ctxt->chr.def_handle,
                    ctxt->chr.val_handle);
        break;

    case BLE_GATT_REGISTER_OP_DSC:
        BLEPRPH_LOG(DEBUG, "registering descriptor %s with handle=%d\n",
                    ble_uuid_to_str(ctxt->dsc.dsc_def->uuid, buf),
                    ctxt->dsc.handle);
        break;

    default:
        assert(0);
        break;
    }
}

int
gatt_svr_init(void)
{
    int rc;

    rc = ble_gatts_count_cfg(gatt_svr_svcs);
    if (rc != 0) {
        return rc;
    }

    rc = ble_gatts_add_svcs(gatt_svr_svcs);
    if (rc != 0) {
        return rc;
    }

    // ONLY INITIALIZE FOR DEBUG PURPOSE
    //SPI_LCD_init(); // FOR LCD VALIDATION TEST
    //SPI_MEMORY_init(); // FOR SPI MEMORY VALIDATION TEST

    return 0;
}


void SPI_MEMORY_init(void){
    static uint8_t warmtest = 0xaa;
    my_sst26_dev = sst26_default_config();
    my_sst26_dev->spi_num = 0;
    my_sst26_dev->spi_cfg = &spi_cfg;
    my_sst26_dev->ss_pin = spi_cfg.ss_pin;

    int rc;
    rc = sst26_init((struct hal_flash *) my_sst26_dev);
    if (rc) {
        // XXX: error handling 
    }
    sst26_write((struct hal_flash *) my_sst26_dev, 0, &warmtest, 1);
}
 /*
 * Initialize SPI and GPIO
 */
void SPI_LCD_init(void){    

    hal_spi_disable(0);
    hal_spi_config(0, &screen_SPI_settings);
    hal_spi_enable(0);
    
    g_led_pin = LED_BLINK_PIN;
    ncs_lcd=nCS_LCD;
    sck_lcd=SCK_LCD;  
    mosi_lcd=MOSI_LCD; 
    dc_lcd=DC_LCD;
    pwm_lcd=PWM_LCD;

    hal_gpio_init_out(g_led_pin, 1);
    hal_gpio_init_out(ncs_lcd, 1);
    hal_gpio_init_out(sck_lcd, 1);
    hal_gpio_init_out(mosi_lcd, 1);
    hal_gpio_init_out(dc_lcd, 1);
    hal_gpio_init_out(pwm_lcd, 0);

    st7735_DisplayOff();
    BSP_LCD_Init();
    st7735_DisplayOn();

    BSP_LCD_Clear(LCD_COLOR_RED);
    BSP_LCD_Clear(LCD_COLOR_GREEN);
    BSP_LCD_Clear(LCD_COLOR_BLUE);

    //Init LCD driver to receveive pixels in location 0, 0
    st7735_SetDisplayWindow(0, 0, 128, 128);
    st7735_WriteReg(LCD_REG_54, 0x48);
    st7735_SetCursor(0, 0);  


}
