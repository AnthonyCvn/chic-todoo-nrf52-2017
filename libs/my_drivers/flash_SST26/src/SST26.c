/*
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

#include <os/os.h>

#include <hal/hal_spi.h>
#include <hal/hal_gpio.h>
#include <hal/hal_flash.h>
#include <hal/hal_flash_int.h>
#include <SST26/SST26.h>

#include <string.h>

#define ULBPR           0x98
#define SE              0x20    /**< Erase 4 KBytes of Memory Array */
#define BE              0xD8    /**< Erase 64, 32 or 8 KBytes of Memory Array */
#define CE              0xC7    /**< Erase Full Array */
#define PP              0x02    /**< Page Program */
#define READ            0x03    /**< Read Memory */
#define WREN            0x06    /**< Write Enable */

#define STATUS_REGISTER 0x05

#define MAX_PAGE_SIZE   256

#define STATUS_BUSY     (1 << 7)



static inline void sst26_write_enable(struct sst26_dev *dev);
static inline void sst26_write_address(struct sst26_dev *dev, uint32_t address);

/*
static const struct hal_flash_funcs sst26_flash_funcs = {
    .hff_read         = sst26_read,
    .hff_write        = sst26_write,
    .hff_erase_sector = sst26_erase_sector,
    .hff_sector_info  = sst26_sector_info,
    .hff_init         = sst26_init,
};
*/

static struct sst26_dev sst26_default_dev = {
    /* struct hal_flash for compatibility */
    .hal = {
        .hf_itf        = NULL,
        .hf_base_addr  = 0,
        .hf_size       = 8192 * 512,  /* FIXME: depends on page size */
        .hf_sector_cnt = 8192,
        .hf_align      = 0,
    },

    /* SPI settings + updates baudrate on _init */
    .settings           = NULL,

    /* Configurable fields that must be populated by user app */
    .spi_num            = 0,
    .spi_cfg            = NULL,
    .ss_pin             = 0,
    .baudrate           = 1000, //100
    .page_size          = 256,
    .disable_auto_erase = 0,
};

static struct hal_spi_settings sst26_default_settings = {
    .data_order = HAL_SPI_MSB_FIRST,
    .data_mode  = HAL_SPI_MODE0, //HAL_SPI_MODE3
    .baudrate   = 1000, //100
    .word_size  = HAL_SPI_WORD_SIZE_8BIT,
};

static uint8_t g_page_buffer[MAX_PAGE_SIZE];

static uint8_t
sst26_read_status(struct sst26_dev *dev)
{
    uint8_t val;

    hal_gpio_write(dev->ss_pin, 0);

    hal_spi_tx_val(dev->spi_num, STATUS_REGISTER);
    val = hal_spi_tx_val(dev->spi_num, 0xff);

    hal_gpio_write(dev->ss_pin, 1);

    return val;
}

static inline bool
sst26_device_ready(struct sst26_dev *dev)
{
    return ((sst26_read_status(dev) & STATUS_BUSY) != 0);
}

static inline void
sst26_write_enable(struct sst26_dev *dev)
{
    hal_gpio_write(dev->ss_pin, 0);
    hal_spi_tx_val(dev->spi_num, WREN);
    hal_gpio_write(dev->ss_pin, 1);

}

static inline void
sst26_write_address(struct sst26_dev *dev, uint32_t address)
{    
    uint8_t addr_hi = address >> 16;
    uint8_t addr_mi = ( address & 0x00FF00 ) >> 8;
    uint8_t addr_lo = ( address & 0x0000FF );

    hal_spi_tx_val(dev->spi_num, addr_hi);
    hal_spi_tx_val(dev->spi_num, addr_mi);
    hal_spi_tx_val(dev->spi_num, addr_lo);

}

/* FIXME: add timeout */
static inline void
sst26_wait_ready(struct sst26_dev *dev)
{
    while (sst26_device_ready(dev)) {
        os_time_delay(OS_TICKS_PER_SEC / 10000);
    }
}

// number of necessary page 
static inline uint16_t
sst26_calc_page_count(struct sst26_dev *dev, uint32_t addr, size_t len)
{
    uint16_t page_count;
    uint16_t page_size = dev->page_size;

    page_count = 1 + (len / (page_size + 1));

    // addr % page_size gives the number not use in a page 
    if ((addr % page_size) + len > page_size * page_count) {
        page_count++;
    }

    return page_count;
}

static inline uint32_t
sst26_page_start_address(struct sst26_dev *dev, uint32_t addr)
{
    return (addr & ~(dev->page_size - 1)); // Page address: A[bit 23: bit 8]
}

static inline uint32_t
sst26_page_next_addr(struct sst26_dev *dev, uint32_t addr)
{
    return (sst26_page_start_address(dev, addr) + dev->page_size);
}


// FIXME: assume buf has enough space?
int
sst26_read(const struct hal_flash *hal_flash_dev, uint32_t addr, void *buf,
                uint32_t len)
{
    uint16_t n;
    struct sst26_dev *dev;
    uint8_t *u8buf;
    uint16_t read = READ;

    u8buf = (uint8_t *) buf;

    dev = (struct sst26_dev *) hal_flash_dev;

    hal_gpio_write(dev->ss_pin, 0);

    hal_spi_tx_val(dev->spi_num, read);

    sst26_write_address(dev, addr);

    for (n = 0; n < len; n++) {
        u8buf[n] = hal_spi_tx_val(dev->spi_num, 0xff);
    }

    hal_gpio_write(dev->ss_pin, 1);

    return 0;
}

int
sst26_write(const struct hal_flash *hal_flash_dev, uint32_t addr,
        const void *buf, uint32_t len)
{
    uint16_t bfa;
    uint32_t n;
    uint32_t start_addr;
    uint16_t index;
    uint16_t amount;
    int page_count;
    uint8_t *u8buf;
    uint16_t page_size;
    struct sst26_dev *dev;

    dev = (struct sst26_dev *) hal_flash_dev;

    page_size = dev->page_size;
    page_count = sst26_calc_page_count(dev, addr, len);
    u8buf = (uint8_t *) buf;
    index = 0;

    while (page_count--) {
        sst26_wait_ready(dev);

        bfa = addr % page_size;
        start_addr = sst26_page_start_address(dev, addr);

        /**
         * If the page is not being written from the beginning,
         * read first the current data to rewrite back.
         *
         * The whole page is read here for the case of some the
         * real data ending before the end of the page, data must
         * be written back again.
         */
        if (bfa || len < page_size) {
            sst26_read(hal_flash_dev, start_addr, g_page_buffer,page_size);
            sst26_wait_ready(dev);
        }


        sst26_write_enable(dev);   // Enable write
        hal_gpio_write(dev->ss_pin, 0);

        /* TODO: ping-pong between page 1 and 2? */
        hal_spi_tx_val(dev->spi_num, PP); // Page programm command

        sst26_write_address(dev, start_addr); // start address of the page

        /**
         * Write back extra stuff at the beginning of page.
         */
        if (bfa) {
            amount = addr - start_addr;
            for (n = 0; n < amount; n++) {
                hal_spi_tx_val(dev->spi_num, g_page_buffer[n]);
            }
        }

        if (len + bfa <= page_size) {
            amount = len;
        } else {
            amount = page_size - bfa;
        }

        /**
         * Data we want to write!
         */
        for (n = 0; n < amount; n++) {
            hal_spi_tx_val(dev->spi_num, u8buf[index++]);
        }

        /**
         * Write back extra stuff at the ending of page.
         */
        if (bfa + len < page_size) {
            for (n = len + bfa; n < page_size; n++) {
                hal_spi_tx_val(dev->spi_num, g_page_buffer[n]);
            }
        }

        hal_gpio_write(dev->ss_pin, 1);

        addr = sst26_page_next_addr(dev, addr);
        len -= amount;
    }

    return 0;
}

int
sst26_sector_erase(const struct hal_flash *hal_flash_dev,
                    uint32_t sector_address)
{
    struct sst26_dev *dev;
    
    dev = (struct sst26_dev *) hal_flash_dev;

    sst26_wait_ready(dev);

    sst26_write_enable(dev);   // Enable write
    hal_gpio_write(dev->ss_pin, 0);
    hal_spi_tx_val(dev->spi_num, SE);
    sst26_write_address(dev, sector_address);
    hal_gpio_write(dev->ss_pin, 1);

    return 0;
}

int
sst26_block_erase(const struct hal_flash *hal_flash_dev,
                    uint32_t block_address)
{
    struct sst26_dev *dev;
    
    dev = (struct sst26_dev *) hal_flash_dev;
    
    sst26_wait_ready(dev);

    sst26_write_enable(dev);   // Enable write
    hal_gpio_write(dev->ss_pin, 0);
    hal_spi_tx_val(dev->spi_num, BE);
    sst26_write_address(dev, block_address);
    hal_gpio_write(dev->ss_pin, 1);

    return 0;
}

int
sst26_chip_erase(const struct hal_flash *hal_flash_dev)
{
    struct sst26_dev *dev;
    
    dev = (struct sst26_dev *) hal_flash_dev;
    
    sst26_wait_ready(dev);
    
    sst26_write_enable(dev);   // Enable write
    hal_gpio_write(dev->ss_pin, 0);
    hal_spi_tx_val(dev->spi_num, CE);
    hal_gpio_write(dev->ss_pin, 1);

    return 0;
}

struct sst26_dev *
sst26_default_config(void)
{
    struct sst26_dev *dev;

    dev = malloc(sizeof(sst26_default_dev));
    if (!dev) {
        return NULL;
    }

    memcpy(dev, &sst26_default_dev, sizeof(sst26_default_dev));
    return dev;
}

int
sst26_init(const struct hal_flash *hal_flash_dev)
{
    int rc;
    struct hal_spi_settings *settings;
    struct sst26_dev *dev;

    dev = (struct sst26_dev *) hal_flash_dev;

    /* only alloc new settings if using non-default */
    if (dev->baudrate == sst26_default_settings.baudrate) {
        dev->settings = &sst26_default_settings;
    } else {
        settings = malloc(sizeof(sst26_default_settings));
        if (!settings) {
            return -1;
        }
        memcpy(settings, &sst26_default_settings, sizeof(sst26_default_settings));
        sst26_default_settings.baudrate = dev->baudrate;
    }

    hal_gpio_init_out(dev->ss_pin, 1);

    
    rc = hal_spi_init(dev->spi_num, dev->spi_cfg, HAL_SPI_TYPE_MASTER);
    if (rc) {
        return (rc);
    }
    

    hal_spi_disable(dev->spi_num);

    rc = hal_spi_config(dev->spi_num, dev->settings);
    if (rc) {
        return (rc);
    }

    hal_spi_set_txrx_cb(dev->spi_num, NULL, NULL);

    hal_spi_enable(dev->spi_num);


    sst26_wait_ready(dev);
    hal_gpio_write(dev->ss_pin, 0);
    hal_spi_tx_val(dev->spi_num, ULBPR); // Global unlock
    hal_gpio_write(dev->ss_pin, 1);

    return 0;
}
