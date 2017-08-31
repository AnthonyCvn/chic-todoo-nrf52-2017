#include <assert.h>
#include <string.h>

#include "sysinit/sysinit.h"
#include "os/os.h"
#include "bsp/bsp.h"
#include "hal/hal_gpio.h"
#ifdef ARCH_sim
#include "mcu/mcu_sim.h"
#endif

#include <SST26/SST26.h>

#include "mcu/nrf51_hal.h"


static volatile int g_task1_loops;


/* New task for the memory management */
void
flash_task_handler(void *arg)
{
    
    static const uint8_t SPI_SCK_PIN  = 24;
    static const uint8_t SPI_MOSI_PIN = 21;
    static const uint8_t SPI_MISO_PIN = 22;
    static const uint8_t SPI_SS_PIN   = 23;
  
    struct nrf51_hal_spi_cfg spi_cfg = {
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

    int rc;
    rc = sst26_init((struct hal_flash *) my_sst26_dev);
    if (rc) {
        // XXX: error handling 
    }

    
    uint8_t i;

    uint32_t addr=0;
    uint32_t len=50;


    static uint8_t buf[10];

    

    while (1) {
        ++g_task1_loops;

        /* Wait one second */
        os_time_delay(OS_TICKS_PER_SEC);

        for(i=10;i<len;i++){
            buf[i]=i;
        }

        sst26_write((struct hal_flash *) my_sst26_dev, addr, buf, len);

        //os_time_delay(OS_TICKS_PER_SEC);

        sst26_read((struct hal_flash *) my_sst26_dev, addr, buf, len);

    }
}
