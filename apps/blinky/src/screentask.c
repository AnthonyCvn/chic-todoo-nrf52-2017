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
#include "lcd/picture.h"

/* SPI */
#include <hal/hal_spi.h>
#include <SST26/SST26.h>
#include "mcu/nrf51_hal.h"
#include "screentask.h"
#include "lcd/lcd.h"
#include "lcd/st7735.h"
#include "lcd/stm32_adafruit_lcd.h"

#define BAR_LENGTH 436

static volatile int g_task1_loops;

static struct hal_spi_settings screen_SPI_settings = {
    .data_order = HAL_SPI_MSB_FIRST,
    .data_mode  = HAL_SPI_MODE3,
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

void refresh_time_ptr(uint8_t vert_pos, uint8_t second,uint8_t minute,uint8_t hour, uint8_t * ptr){
    int i;
    for(i=0;i<vert_pos;i++){
        ptr[i]=' ';
    }

    ptr[i]=hour/10+48;
    ptr[i+1]=hour%10+48;
    ptr[i+2]=':';
    ptr[i+3]=minute/10+48;
    ptr[i+4]=minute%10+48;
    ptr[i+5]=':';
    ptr[i+6]=second/10+48;
    ptr[i+7]=second%10+48;

}

uint32_t refresh_task_percent(uint32_t current_task_time, uint8_t second, uint8_t minute, uint8_t hour){
    uint32_t second_left, percent;
    second_left = second + 60 * minute + 3600 * hour;

    if(current_task_time > second_left){
        percent = ((current_task_time - second_left)*100)/current_task_time;
    }else{
        percent = 0;
    }

    return percent;
}

void draw_time_bar(uint32_t task_percent){
    BSP_LCD_SetTextColor(LCD_COLOR_WHITE);
    
    //BSP_LCD_FillRect(19, 0, 109, 19);
    //BSP_LCD_FillRect(109, 0, 19, 128);
    //BSP_LCD_FillRect(19, 109, 109, 19);

    uint32_t temp;

    if(task_percent>70){

        temp = 280-(13*task_percent)/5;
        BSP_LCD_FillLoading(0, (uint16_t) temp, BOTTOM);
    
    }else if(task_percent>69){

        BSP_LCD_FillRect(0, 100, 20, 7);

    }else if(task_percent>68){
        
        BSP_LCD_FillLoading(0, 108, LEFT);

    }else if(task_percent>67){
        
        BSP_LCD_FillLoading(1, 108, LEFT);

    }else if(task_percent>30){

        temp = 175-(13*task_percent)/5;
        BSP_LCD_FillLoading((uint16_t) temp, 108,LEFT); 

    }else if(task_percent>29){

        BSP_LCD_FillRect(100, 108, 8, 20);

    }else if(task_percent>28){

        BSP_LCD_FillLoading(108, 114, TOP);

    }else{

        temp = 39+(13*task_percent)/5;
        BSP_LCD_FillLoading(108, (uint16_t) temp, TOP);

    }

    //BSP_LCD_SetTextColor(LCD_COLOR_BLACK);

}


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
    //st7735_DisplayOn();

    //BSP_LCD_Clear(LCD_COLOR_RED);
    //BSP_LCD_Clear(LCD_COLOR_GREEN);
    //BSP_LCD_Clear(LCD_COLOR_BLUE);

    //BSP_LCD_Clear(LCD_COLOR_WHITE);
    //BSP_LCD_DrawBitmap(0, 0, (uint8_t *)bmp0_test_image);

    //BSP_LCD_DrawBitmap(0, 0, (uint8_t *)bmp0_test_image);
    //BSP_LCD_DrawBitmap(0, 0, (uint8_t *)bluetooth_icon);



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



    
    //sst26_chip_erase((struct hal_flash *) my_sst26_dev);
    
    //sst26_write((struct hal_flash *) my_sst26_dev, 0, &bmp0_test_image, sizeof(bmp0_test_image));



   // BSP_LCD_DrawBitmap(0, 0, (uint8_t *)bmp0_test_image);
    //BSP_LCD_Clear(LCD_COLOR_BLUE);


    /////////////////////////////////////////////////////////////////////////////////////////// WRITE IN MEMORY

    os_time_delay(OS_TICKS_PER_SEC/2);

    //sst26_chip_erase((struct hal_flash *) my_sst26_dev);
    //PIC_BLUETOOTH_ADV_ADD
    //sst26_write((struct hal_flash *) my_sst26_dev, PIC_BLUETOOTH_ADV_ADD , &bluetooth_advertize, 32834);
    BSP_LCD_Clear(LCD_COLOR_BLUE);
    os_time_delay(OS_TICKS_PER_SEC/2);
    ext_memory_bitmap_to_LCD(0, 0, PIC_BLUETOOTH_ADV_ADD, (struct hal_flash *) my_sst26_dev);

    os_time_delay(OS_TICKS_PER_SEC/2);

    //sst26_write((struct hal_flash *) my_sst26_dev, PIC_BLUETOOTH_BRAND_ADD, &brand_128x128, 32834);
    os_time_delay(OS_TICKS_PER_SEC/2);
    BSP_LCD_Clear(LCD_COLOR_BLUE);
    os_time_delay(OS_TICKS_PER_SEC/2);
    ext_memory_bitmap_to_LCD(0, 0, PIC_BLUETOOTH_BRAND_ADD, (struct hal_flash *) my_sst26_dev);

    os_time_delay(OS_TICKS_PER_SEC/2);
    
    //sst26_write((struct hal_flash *) my_sst26_dev, PIC_BLUETOOTH_SHOWER_ADD, &shower_88x88, 15554);
    BSP_LCD_Clear(LCD_COLOR_BLUE);
    os_time_delay(OS_TICKS_PER_SEC/2);
    ext_memory_bitmap_to_LCD(20, 20, PIC_BLUETOOTH_SHOWER_ADD, (struct hal_flash *) my_sst26_dev);

    os_time_delay(OS_TICKS_PER_SEC/2);
        
    //sst26_write((struct hal_flash *) my_sst26_dev, PIC_BLUETOOTH_TSHIRT_ADD, &dress_up_88x88, 15554);
    BSP_LCD_Clear(LCD_COLOR_BLUE);
    os_time_delay(OS_TICKS_PER_SEC/2);
    ext_memory_bitmap_to_LCD(20, 20, PIC_BLUETOOTH_TSHIRT_ADD, (struct hal_flash *) my_sst26_dev);

    /////////////////////////////////////////////////////////////////////////////////////////// WRITE IN MEMORY


    //BSP_LCD_DrawBitmap(0, 0, (uint8_t *)bluetooth_advertize);

    //os_time_delay(OS_TICKS_PER_SEC);

    //BSP_LCD_DrawBitmap(0, 0, (uint8_t *)brand_128x128);

    //os_time_delay(OS_TICKS_PER_SEC);
    os_time_delay(OS_TICKS_PER_SEC*4);
    //BSP_LCD_DrawBitmap(20, 20, (uint8_t *)dress_up_88x88);
    os_time_delay(OS_TICKS_PER_SEC*4);

    //BSP_LCD_DrawBitmap(20, 20, (uint8_t *)shower_88x88);
    //os_time_delay(OS_TICKS_PER_SEC);

    //BSP_LCD_Clear(LCD_COLOR_BLUE);

    //uint16_t color;
    //int k;

    //unsigned char pbmp[100];

    uint8_t ptr_clock[10];
    uint32_t task_percent;
    current_task_time = 100; // in second

    BSP_LCD_SetTextColor(LCD_COLOR_RED);

    BSP_LCD_FillRect(0, 0, 128, 20);   //Bottom
    BSP_LCD_FillRect(108, 20, 20, 108); //Right
    BSP_LCD_FillRect(20, 108, 88, 20); //Top
    BSP_LCD_FillRect(0, 20, 20, 108);   //Left

    BSP_LCD_SetTextColor(LCD_COLOR_WHITE);
    BSP_LCD_FillLoading(0, 21, BOTTOM);
    BSP_LCD_FillLoading(0, 23, BOTTOM);

    //BSP_LCD_Filltopcorner(20);

    BSP_LCD_FillCircle(118, 36, 8);

    BSP_LCD_SetTextColor(LCD_COLOR_RED);
    BSP_LCD_FillCircle(118, 36, 5);

    BSP_LCD_SetTextColor(LCD_COLOR_WHITE);
    BSP_LCD_SetBackColor(LCD_COLOR_RED);

    while (1) {
        ++g_task1_loops;

        /* Wait one second */
        /*
        for(k=0;k<16;k++){
            color=(1<<k);
            BSP_LCD_Clear(color);
            os_time_delay(OS_TICKS_PER_SEC);
        }
        */

        //BSP_LCD_DrawBitmap(0, 0, (uint8_t *)bmp0_test_image);
        
            //sst26_write((struct hal_flash *) my_sst26_dev, 0, &bmp0_test_image, 17461);
        
            //BSP_LCD_Clear(LCD_COLOR_BLUE);
        
        //ext_memory_bitmap_to_LCD(0, 0, 0, (struct hal_flash *) my_sst26_dev);
        os_time_delay(OS_TICKS_PER_SEC/4);

        refresh_time_ptr(2 , sec, min, hour, &ptr_clock[0]);

        task_percent = refresh_task_percent(current_task_time, sec, min, hour);

        draw_time_bar(task_percent);

        BSP_LCD_DisplayStringAtLine(7, &ptr_clock[0]);




        //sst26_read((struct hal_flash *) my_sst26_dev, 0, pbmp, 10);

        //sst26_read((struct hal_flash *) my_sst26_dev, 0, pbmp, 100);
    
        /* 
        BSP_LCD_DrawCircle(64, 64, 10);
        BSP_LCD_DrawCircle(64, 64, 20);
        BSP_LCD_DrawCircle(64, 64, 30);
        BSP_LCD_DrawCircle(64, 64, 40);
        BSP_LCD_DrawCircle(64, 64, 50);
        BSP_LCD_DrawCircle(64, 64, 60);
        BSP_LCD_DrawCircle(64, 64, 70);
        BSP_LCD_DrawCircle(64, 64, 80);
        BSP_LCD_DrawCircle(64, 64, 90);
        */
    }
}

//////////////////////////////* LCD IO functions *//////////////////
void ext_memory_bitmap_to_LCD(uint16_t Xpos, uint16_t Ypos,  uint32_t addr, const struct hal_flash * sst26_dev){

        /*Send the data by SPI1 (could be adapted by changing the hspiX)*/
        uint32_t j=0;
        uint32_t height = 0, width  = 0;
        uint32_t index = 0, size = 0;

        /* Image buffer */
        unsigned char pbmp[100];
                
        /* Read a set of bytes */
        sst26_read((struct hal_flash *) sst26_dev, addr, &pbmp[0], 100);
        
        /* Read bitmap width */
        width = *(uint16_t *) (pbmp + 18);
        width |= (*(uint16_t *) (pbmp + 20)) << 16;
        
        /* Read bitmap height */
        height = *(uint16_t *) (pbmp + 22);
        height |= (*(uint16_t *) (pbmp + 24)) << 16;
        
        
        /* Remap Ypos, st7735 works with inverted X in case of bitmap */
        /* X = 0, cursor is on Top corner */

        Ypos = BSP_LCD_GetYSize() - Ypos - height;
        
        st7735_SetDisplayWindow(Xpos, Ypos, width, height);
        
        /* Read bitmap size */
        size = *(volatile uint16_t *) (pbmp + 2);
        size |= (*(volatile uint16_t *) (pbmp + 4)) << 16;
        /* Get bitmap data address offset */
        index = *(volatile uint16_t *) (pbmp + 10);
        index |= (*(volatile uint16_t *) (pbmp + 12)) << 16;
        size = (size - index)/2;
        //pbmp += index;
        
        /* Set GRAM write direction and BGR = 0 */
        /* Memory access control: MY = 0, MX = 1, MV = 0, ML = 0 */
        st7735_WriteReg(LCD_REG_54, 0x48);
      
        /* Set Cursor */
        st7735_SetCursor(Xpos, Ypos);  

        /* Reset LCD control line CS */
        hal_gpio_write(ncs_lcd, 0);
    
        /* Set LCD data/command line DC to high */
        hal_gpio_write(dc_lcd, 1);
        
    
        /* Send Command */
        /* While the SPI in TransmitReceive process, user can transmit data through
             "pData" buffer */
    
        for(j=0;j<size*2;j+=2){                
            sst26_read((struct hal_flash *) sst26_dev, j+addr+index, &pbmp[0], 2);      
            hal_spi_txrx(1, &pbmp[1], rxbuf, 1);
            hal_spi_txrx(1, &pbmp[0], rxbuf, 1);
        }
    
        /* Deselect : Chip Select high */
        hal_gpio_write(ncs_lcd, 1);

        /* Set GRAM write direction and BGR = 0 */
        /* Memory access control: MY = 1, MX = 1, MV = 0, ML = 0 */
        // st7735_WriteReg(LCD_REG_54, 0x60); //0xC0 
        // LCD_REG_54 change RBG to GBR

}


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
    uint32_t j=0;
    
	/* Reset LCD control line CS */
	hal_gpio_write(ncs_lcd, 0);

	/* Set LCD data/command line DC to high */
    hal_gpio_write(dc_lcd, 1);

	/* Send Command */
	/* While the SPI in TransmitReceive process, user can transmit data through
         "pData" buffer */

    if(pData_numb==1){
        hal_spi_txrx(1, pData, rxbuf, 1);
    }
    else{
        for(j=0;j<pData_numb;j+=2){
            //send_8bit_serial(pData+j);
            hal_spi_txrx(1, pData+j+1, rxbuf, 1);
            hal_spi_txrx(1, pData+j, rxbuf, 1);
        }
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
