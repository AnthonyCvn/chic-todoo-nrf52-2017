#include <assert.h>
#include <string.h>

#include "sysinit/sysinit.h"
#include "os/os.h"
#include "bsp/bsp.h"
#include "hal/hal_gpio.h"
#ifdef ARCH_sim
#include "mcu/mcu_sim.h"
#endif

/* lcd header */
#include "lcd/stm32_adafruit_lcd.h"

/* SPI */
#include <hal/hal_spi.h>

static volatile int g_task1_loops;

static struct hal_spi_settings screen_SPI_settings = {
    .data_order = HAL_SPI_MSB_FIRST,
    .data_mode  = HAL_SPI_MODE0,
    .baudrate   = 8000, // max 8000
    .word_size  = HAL_SPI_WORD_SIZE_8BIT,
};

// GPIO declaration
int g_led_pin;
int ncs_lcd;
int sck_lcd;  
int mosi_lcd;  
int dc_lcd; 
int pwm_lcd; 

uint8_t rxbuf[5];


/* New task for the LCD management */
void
screen_task_handler(void *arg)
{
    hal_spi_disable(1);
    hal_spi_config(1, &screen_SPI_settings);
    hal_spi_enable(1);


    /*  GPIO configuration. */
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
    hal_gpio_init_out(pwm_lcd, 1);

    st7735_DisplayOff();
    BSP_LCD_Init();

    while (1) {
        ++g_task1_loops;

        /* Wait one second */
        os_time_delay(OS_TICKS_PER_SEC);

        /* Toggle the LED */
        hal_gpio_toggle(g_led_pin);
        BSP_LCD_Clear(LCD_COLOR_RED);
        BSP_LCD_Clear(LCD_COLOR_GREEN);
        BSP_LCD_Clear(LCD_COLOR_BLUE);
        BSP_LCD_Clear(LCD_COLOR_CYAN);
        BSP_LCD_DrawCircle(64, 64, 10);
        BSP_LCD_DrawCircle(64, 64, 20);
        BSP_LCD_DrawCircle(64, 64, 30);
        BSP_LCD_DrawCircle(64, 64, 40);
        BSP_LCD_DrawCircle(64, 64, 50);
        BSP_LCD_DrawCircle(64, 64, 60);
        BSP_LCD_DrawCircle(64, 64, 70);
        BSP_LCD_DrawCircle(64, 64, 80);
        BSP_LCD_DrawCircle(64, 64, 90);
    }
}

//////////////////////////////* LCD IO functions *//////////////////
// transmit byte serially, MSB first
void send_8bit_serial(uint8_t *Data)
{
   int i;
   unsigned char data;

   data = *Data;

   // send bits 7..0
   for (i = 0; i < 8; i++)
   {
       // consider leftmost bit
       // set line high if bit is 1, low if bit is 0
       if (data & 0x80){
           hal_gpio_init_out(mosi_lcd, 1);
       }
       else{
           hal_gpio_init_out(mosi_lcd, 0);
       }
           
       // pulse clock to indicate that bit value should be read
       hal_gpio_init_out(sck_lcd, 0);
       hal_gpio_init_out(sck_lcd, 1);

       // shift byte left so next bit will be leftmost
       data <<= 1;
   }
}

void LCD_IO_Init(void){
	
	hal_gpio_write(pwm_lcd, 1);
	/* LCD chip select high */
	hal_gpio_write(ncs_lcd, 1); //LCD_CS_HIGH();
}
void LCD_IO_WriteMultipleData(uint8_t *pData, uint32_t pData_numb){
	/*Send the data by SPI1 (could be adapted by changing the hspiX)*/
    int j=0;
    
	/* Reset LCD control line CS */
	hal_gpio_write(ncs_lcd, 0);

	/* Set LCD data/command line DC to high */
	hal_gpio_write(dc_lcd, 1);

	/* Send Command */
	/* While the SPI in TransmitReceive process, user can transmit data through
         "pData" buffer */
    for(j=0;j<pData_numb;j++){
        //send_8bit_serial(pData+j);
        hal_spi_txrx(1, pData+j, rxbuf, 1);
    }


	/* Deselect : Chip Select high */
	hal_gpio_write(ncs_lcd, 1);
}
void LCD_IO_WriteReg(uint8_t Reg){
	/*Send the register address by SPI1 (could be adapted by changing the hspiX)*/

	/* Reset LCD control line CS */
	hal_gpio_write(ncs_lcd, 0);

	/* Set LCD data/command line DC to Low */
	hal_gpio_write(dc_lcd, 0);

	/* Send Command */
	/* While the SPI in TransmitReceive process, user can transmit data through
	     "Reg" buffer*/
        //send_8bit_serial(&Reg);
        hal_spi_txrx(1, &Reg, rxbuf, 1);

	/* Deselect : Chip Select high */
	hal_gpio_write(ncs_lcd, 1);
}
void LCD_Delay(uint32_t delay){
    
    os_time_delay(delay);
}

