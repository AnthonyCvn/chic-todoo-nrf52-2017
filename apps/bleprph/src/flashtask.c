/* 
 * CHIC - China Hardware Innovation Camp - Todoo
 * https://chi.camp/projects/todoo/
 *
 * Anthony Cavin
 * anthony.cavin.ac@gmail.com
 * 2018, January 11
 *
 * Task to manage the External memory
 *
 * All needed parameter are stored in structure todoo
 * (see todoo_data.h)
 * 
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

#include "flashtask.h"
#include "todoo_data.h"

#include <SST26/SST26.h>

#include "mcu/nrf52_hal.h"


static volatile int g_task1_loops;

/* FIFO global definition between gatt service task and flash task */
array_type FIFO_task[FIFO_TASK_HEIGHT];

FIFO_task_reader_type FIFO_task_reader = {
    .maxcnt = MAX_FIFO_WIDTH,
    .ptr    = (uint32_t)&FIFO_task,
    .fline  = 0
};

/* New task for the memory management */
void
flash_task_handler(void *arg)
{
    
    static const uint8_t SPI_SCK_PIN  = 16;
    static const uint8_t SPI_MOSI_PIN = 17;
    static const uint8_t SPI_MISO_PIN = 18;
    static const uint8_t SPI_SS_PIN   = 20;
  
    struct nrf52_hal_spi_cfg spi_cfg = {
        .sck_pin      = SPI_SCK_PIN ,
        .mosi_pin     = SPI_MOSI_PIN,
        .miso_pin     = SPI_MISO_PIN,
        .ss_pin       = SPI_SS_PIN 
    };

    struct sst26_dev *my_sst26_dev = NULL;
    
    my_sst26_dev = sst26_default_config();
    my_sst26_dev->spi_num = 0;
    my_sst26_dev->spi_cfg = &spi_cfg;
    my_sst26_dev->ss_pin = spi_cfg.ss_pin;

    
    //int rc;
    //rc = sst26_init((struct hal_flash *) my_sst26_dev);
    //if (rc) {
        // XXX: error handling 
    //}
    

    //static uint8_t warmtest = 0xaa;
    //sst26_write((struct hal_flash *) my_sst26_dev, 0, &warmtest, 1);
    //sst26_write((struct hal_flash *) my_sst26_dev, addr, buf, len);
    //sst26_read((struct hal_flash *) my_sst26_dev, addr, buf, len);

    static uint32_t addr = ADD_FIRST_ACTIVITY_PIC;

    while (1) {
        ++g_task1_loops;


        if(FIFO_task_reader.fline != 0){
            -- FIFO_task_reader.fline;
            //sst26_write((struct hal_flash *) my_sst26_dev, addr, &FIFO_task[FIFO_task_reader.fline], FIFO_task[FIFO_task_reader.fline].N);
            addr+=FIFO_task[FIFO_task_reader.fline].N;
        }
        /* Wait 1/6 second */
        os_time_delay(OS_TICKS_PER_SEC/6);
    }
}
