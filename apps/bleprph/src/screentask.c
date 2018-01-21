/* 
 * CHIC - China Hardware Innovation Camp - Todoo
 * https://chi.camp/projects/todoo/
 *
 * Anthony Cavin
 * anthony.cavin.ac@gmail.com
 * 2018, January 11
 *
 * Task to manage the LCD Screen.
 * OS_time delay of one second to let time for the other task to run
 *
 * Each activity are screen on the LCD with the activity's image stored
 * in external memory.
 *
 * All needed parameter are stored in structure todoo
 * (see todoo_data.h)
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

/* lcd header */
#include "lcd/stm32_adafruit_lcd.h"

/* SPI */
#include <hal/hal_spi.h>
#include <SST26/SST26.h>
#include "mcu/nrf52_hal.h"
#include "screentask.h"
#include "lcd/lcd.h"
#include "lcd/st7735.h"
#include "lcd/stm32_adafruit_lcd.h"

/* Todoo structure */
#include "todoo_data.h"

#define BAR_LENGTH 436


/* GPIO */
#define nCS_MEM   (20)
#define nCS_LCD   (19)
#define SCK_LCD   (16)
#define MOSI_LCD  (17)
#define DC_LCD    (15)
#define PWM_LCD   (14)


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

// Small SPI read buffer 
uint8_t rxbuf[5];

/*
* Image buffer to store an image during the transition between external 
* SPI memory and the LCD.
*/
uint8_t image_buf[50];

/*
* Refresh the time in ASCII format
*/
void refresh_time_ptr(uint8_t hor_pos,uint32_t current_task_time, uint8_t * ptr){
    uint8_t i, h, m, s;
    for(i=0;i<hor_pos;i++){
        ptr[i]=' ';
    }

    h = current_task_time/3600;
    m = (current_task_time % 3600)/60;
    s = (current_task_time % 3600) % 60;
  
    ptr[hor_pos]    = h/10+48;
    ptr[hor_pos+1]  = h%10+48;
    ptr[hor_pos+2]  =':';
    ptr[hor_pos+3]  = m/10+48;
    ptr[hor_pos+4]  = m%10+48;
    ptr[hor_pos+5]  =':';
    ptr[hor_pos+6]  = s/10+48;
    ptr[hor_pos+7]  = s%10+48;

    for(i=hor_pos+8;i<20;i++){
        ptr[i]=' ';
    }
}

/*
* Small function to calculate how much time is left in the activity (in percent)
*/
uint32_t refresh_task_percent(uint32_t current_task_time, uint32_t total_task_time){
    return (100 - ((total_task_time - current_task_time)*100)/total_task_time);
}

/* 
* Detect which activity must be running:
*  -If the real time is between two activiy:
*   act_code[0] = previous activity number
*   act_code[1] = next activity number
* 
*  -If the real time is during an activiy:
*  act_code[0] = act_code[1] = actual activity
*/ 
void which_activity(struct Todoo_data *todoo, uint8_t* act_code){
    uint8_t i;
    uint32_t current_min = 24*60*todoo->parameters->day+60*todoo->parameters->time[B_HOUR]+todoo->parameters->time[B_MIN];
    uint32_t start_min;
    uint32_t end_min;

    for(i=act_code[0];i<MAX_ACTIVITY;i++){
        start_min = 24*60*todoo->activity[i].day+60*todoo->activity[i].start_time[B_HOUR]+todoo->activity[i].start_time[B_MIN];
        end_min = 24*60*todoo->activity[i].day+60*todoo->activity[i].end_time[B_HOUR]+todoo->activity[i].end_time[B_MIN];
        if(current_min>=start_min){
            act_code[0]=i;
            if(current_min<end_min){
                act_code[1]=i;             
            }else
            {
                act_code[1]=i+1;
            }
        }else
        {
            break;
        }

    }    
}

/* 
*  Calculate the time in second of an activity
*/ 
int current_task_time_calculation(struct Todoo_data *todoo, uint16_t activity_num_start, uint16_t activity_num_end){
    uint32_t start, end;
    if(activity_num_start==activity_num_end){
        start = todoo->activity[activity_num_start].start_time[B_SEC] + 60 * todoo->activity[activity_num_start].start_time[B_MIN] + 3600 * todoo->activity[activity_num_start].start_time[B_HOUR];
        end =   todoo->activity[activity_num_end].end_time[B_SEC] + 60 * todoo->activity[activity_num_end].end_time[B_MIN] + 3600 * todoo->activity[activity_num_end].end_time[B_HOUR];
    }else
    {
        start = todoo->activity[activity_num_start].end_time[B_SEC] + 60 * todoo->activity[activity_num_start].end_time[B_MIN] + 3600 * todoo->activity[activity_num_start].end_time[B_HOUR];
        end =   todoo->activity[activity_num_end].start_time[B_SEC] + 60 * todoo->activity[activity_num_end].start_time[B_MIN] + 3600 * todoo->activity[activity_num_end].start_time[B_HOUR];
    }

    return end-start;
}

/* 
*  Draw the time bar according to the activity spend time
*/ 
void draw_time_bar(uint32_t task_percent){

    uint32_t temp;

    BSP_LCD_SetTextColor(LCD_COLOR_WHITE);

    if(task_percent>70){

        temp = 281-(13*task_percent)/5;
        BSP_LCD_FillLoading(0, (uint16_t) temp, BOTTOM);
        BSP_LCD_FillRect(0, 21, 20, temp-20);
    
    }else if(task_percent>69){

        BSP_LCD_FillRect(0, 21, 20, 86);

    }else if(task_percent>68){
        
        BSP_LCD_FillLoading(0, 108, LEFT);

    }else if(task_percent>67){
        
        BSP_LCD_FillLoading(1, 108, LEFT);

    }else if(task_percent>30){

        temp = 175-(13*task_percent)/5;
        BSP_LCD_FillLoading((uint16_t) temp, 108,LEFT);
        BSP_LCD_FillRect(0, 108, temp, 20);

    }else if(task_percent>29){

        BSP_LCD_FillRect(0, 108, 108, 20);

    }else if(task_percent>28){

        BSP_LCD_FillLoading(108, 114, TOP);

    }else{

        temp = 35+(13*task_percent)/5;
        BSP_LCD_FillLoading(108, (uint16_t) temp, TOP);
        BSP_LCD_FillRect(108, temp+14, 20, 128-temp);

    }
}

/* 
*  Initialize time bar graphics in red
*/ 
void initialize_screen_bar(){

    BSP_LCD_SetTextColor(LCD_COLOR_RED);
    BSP_LCD_FillRect(0, 0, 128, 20);   //Bottom
    BSP_LCD_FillRect(108, 0, 20, 128); //Right
    BSP_LCD_FillRect(20, 108, 88, 20); //Top
    BSP_LCD_FillRect(0, 20, 20, 108);   //Left
    BSP_LCD_SetTextColor(LCD_COLOR_WHITE);
    BSP_LCD_FillLoading(0, 21, BOTTOM);
    BSP_LCD_FillLoading(0, 23, BOTTOM);
    BSP_LCD_SetTextColor(LCD_COLOR_WHITE);
    BSP_LCD_SetBackColor(LCD_COLOR_RED);
}

/* LCD management task */
void
screen_task_handler(void *arg)
{

    hal_spi_disable(0);
    hal_spi_config(0, &screen_SPI_settings);
    hal_spi_enable(0);


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
    hal_gpio_init_out(pwm_lcd, 0);
    hal_gpio_init_out(nCS_MEM, 1);

    st7735_DisplayOff();
    BSP_LCD_Init();
    st7735_DisplayOn();

    /*  Initialize sst26 driver for the external memory */
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

    int rc;
    rc = sst26_init((struct hal_flash *) my_sst26_dev);
    if (rc) {
        // XXX: error handling 
    }


    /*
    * Write picture in permanent memory 
    */
    ///////////////////////////////////////////////////////////////////////////////////////////
    //sst26_chip_erase((struct hal_flash *) my_sst26_dev);
    //os_time_delay(OS_TICKS_PER_SEC);
    //sst26_write((struct hal_flash *) my_sst26_dev, ADD_BRAND , &brand_128x128, N_BYTES_128x128_BMP);
    //sst26_write((struct hal_flash *) my_sst26_dev, ADD_ADV_REQ , &advertize_request, N_BYTES_128x128_BMP);
    //sst26_write((struct hal_flash *) my_sst26_dev, ADD_SHARING , &sharing, N_BYTES_128x128_BMP);
    //sst26_write((struct hal_flash *) my_sst26_dev, ADD_FREE_TIME , &free_time, N_BYTES_90x90_BMP);
    //os_time_delay(2*OS_TICKS_PER_SEC);

    //sst26_read((struct hal_flash *) my_sst26_dev, ADD_BRAND_PIC, &image_buf, N_BYTES_128x128_BMP);
    //BSP_LCD_DrawBitmap(0,0,image_buf);
    //os_time_delay(2*OS_TICKS_PER_SEC);
    //sst26_read((struct hal_flash *) my_sst26_dev, ADD_ADV_REQ_PIC , &image_buf, N_BYTES_128x128_BMP);
    //BSP_LCD_DrawBitmap(0,0,image_buf);
    //os_time_delay(2*OS_TICKS_PER_SEC);
    //sst26_read((struct hal_flash *) my_sst26_dev, ADD_SHARING_PIC , &image_buf, N_BYTES_128x128_BMP);
    //BSP_LCD_DrawBitmap(0,0,image_buf);
    //os_time_delay(2*OS_TICKS_PER_SEC);
    //sst26_read((struct hal_flash *) my_sst26_dev, ADD_FREE_TIME_PIC , &image_buf, N_BYTES_90x90_BMP);
    //BSP_LCD_DrawBitmap(20,20,image_buf);
    //os_time_delay(2*OS_TICKS_PER_SEC);
    //os_time_delay(OS_TICKS_PER_SEC);
    ///////////////////////////////////////////////////////////////////////////////////////////


    /* Init variable */
    static uint8_t boot_counter=0;

    uint8_t ptr_clock[10];
    uint32_t task_percent;
    uint32_t task_time = 0;
    uint8_t act_code[2]={0};

    todoo->which_state = boot;
    todoo->config_state = 1;

    while (1) {
        ++g_task1_loops;
        /* 
        * LCD management  
        * Sleep during one second to let time for the other task to run
        */
        os_time_delay(OS_TICKS_PER_SEC);

        switch(todoo->which_state) {
            case boot :             
                if(todoo->config_state){
                    //sst26_read((struct hal_flash *) my_sst26_dev, ADD_BRAND_PIC , &image_buf, N_BYTES_128x128_BMP);
                    //BSP_LCD_DrawBitmap(0,0,image_buf);
                    ext_memory_bitmap_to_LCD(0 , 0,  ADD_BRAND_PIC, (struct hal_flash *) my_sst26_dev);
                    todoo->config_state  = 0;
                }
                ++boot_counter;
                if(boot_counter> 3){
                    todoo->which_state  =  ble_request;
                    todoo->config_state  = 1;
                    boot_counter = 0;
                }
                break;
                
            case ble_request :
                if(todoo->config_state){
                    //sst26_read((struct hal_flash *) my_sst26_dev, ADD_ADV_REQ_PIC, &image_buf, N_BYTES_128x128_BMP);
                    //BSP_LCD_DrawBitmap(0,0,image_buf);
                    ext_memory_bitmap_to_LCD(0 , 0,  ADD_ADV_REQ_PIC, (struct hal_flash *) my_sst26_dev);
                    todoo->config_state  = 0;
                }
                break;    
            case shows_activity : 
                if(todoo->config_state){
                    BSP_LCD_Clear(LCD_COLOR_WHITE);
                    which_activity(todoo, act_code);
               
                    if(act_code[0]!=act_code[1]){
                        //sst26_read((struct hal_flash *) my_sst26_dev, ADD_FREE_TIME_PIC, &image_buf, N_BYTES_90x90_BMP);
                        //BSP_LCD_DrawBitmap(20,20,image_buf);
                        
                        //ext_memory_bitmap_to_LCD(20 , 20,  ADD_FREE_TIME_PIC, (struct hal_flash *) my_sst26_dev);
                    }else{
                        // Show activity number in act_code[0]
                        //sst26_read((struct hal_flash *) my_sst26_dev, ADD_FREE_TIME_PIC, &image_buf, N_BYTES_90x90_BMP);
                        //BSP_LCD_DrawBitmap(20,20,image_buf);

                        //ext_memory_bitmap_to_LCD(20 , 20,  ADD_FREE_TIME_PIC, (struct hal_flash *) my_sst26_dev);
                    }
                    
                    task_time = current_task_time_calculation(todoo, act_code[0], act_code[1]);
                    current_task_time = task_time;

                    initialize_screen_bar();
                    todoo->config_state = 0;
                }



                    BSP_LCD_DisplayChar(10, 0, todoo->parameters->time[B_HOUR]/10+48);
                    BSP_LCD_DisplayChar(20, 0, todoo->parameters->time[B_HOUR]%10+48);
                    BSP_LCD_DisplayChar(35, 0, todoo->parameters->time[B_MIN]/10+48);
                    BSP_LCD_DisplayChar(45, 0, todoo->parameters->time[B_MIN]%10+48);
                    BSP_LCD_DisplayChar(60, 0,todoo->parameters->time[B_SEC]/10+48);
                    BSP_LCD_DisplayChar(70, 0,todoo->parameters->time[B_SEC]%10+48);
                    BSP_LCD_DisplayChar(10, 10, 48+todoo->parameters->day);
                    BSP_LCD_DisplayChar(10, 20, 48+todoo->parameters->num_activity);
                    int i_act;
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


                refresh_time_ptr(5 , current_task_time, &ptr_clock[0]);
                task_percent = refresh_task_percent(current_task_time, task_time);
                draw_time_bar(task_percent);
                BSP_LCD_SetFont(&Font12);
                BSP_LCD_DisplayStringAtLine(9, &ptr_clock[0]);
                
                if(!task_percent){
                    todoo->config_state = 1;
                }
                break;
              
            default :
                break;
        }
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
    
        /* Set LCD data/command line DC to high */
        hal_gpio_write(dc_lcd, 1);
        
    
        /* Send Command */
        /* While the SPI in TransmitReceive process, user can transmit data through
             "pData" buffer */
    
        for(j=0;j<size*2;j+=2){                
            sst26_read((struct hal_flash *) sst26_dev, j+addr+index, &pbmp[0], 2);

        /* Reset LCD control line CS */
        hal_gpio_write(ncs_lcd, 0);

            hal_spi_txrx(0, &pbmp[1], rxbuf, 1);
            hal_spi_txrx(0, &pbmp[0], rxbuf, 1);

        /* Deselect : Chip Select high */
        hal_gpio_write(ncs_lcd, 1);
        }
    

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
        //send_8bit_serial(pData);
        hal_spi_txrx(0, pData, rxbuf, 1);
    }
    else{
        for(j=0;j<pData_numb;j+=2){
            //send_8bit_serial(pData+j+1);
            //send_8bit_serial(pData+j);
            hal_spi_txrx(0, pData+j+1, rxbuf, 1);
            hal_spi_txrx(0, pData+j, rxbuf, 1);
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
        hal_spi_txrx(0, &Reg, rxbuf, 1);

	/* Deselect : Chip Select high */
	hal_gpio_write(ncs_lcd, 1);
}
void LCD_Delay(uint32_t delay){
    
    os_time_delay(delay);
}


/**
  * Draws loading bar.
  * Xpos: X position
  * Ypos: Y position
  * Orientation: Shape orientation
  */
  void BSP_LCD_FillLoading(uint16_t Xpos, uint16_t Ypos, Orientation Orient)
  {  
    uint8_t  i; 
    uint8_t Length_LUT[20] = {11,10,9,8,7,6,6,5,5,5,5,5,5,6,6,7,8,9,10,11};
    uint8_t CoinLength_LUT[20] = {20,18,16,14,12,11,9,8,7,6,4,4,3,2,1,1,0,0,0,0};
    
    //BSP_LCD_SetTextColor(DrawProp.TextColor);
    
    switch(Orient){
      case TOP:
        for(i=0;i<20;i++){
          BSP_LCD_DrawVLine(Xpos + i, Ypos+14-Length_LUT[i], Length_LUT[i]);
        }
      break;
      case BOTTOM:
        for(i=0;i<20;i++){
          BSP_LCD_DrawVLine(Xpos + i, Ypos, Length_LUT[i]);
        }
      break;
      case LEFT:
        for(i=0;i<20;i++){
          BSP_LCD_DrawHLine(Xpos, Ypos+i, Length_LUT[i]);
        }
      break;
      case RIGHT:
        for(i=0;i<20;i++){
          BSP_LCD_DrawHLine(Xpos+14-Length_LUT[i], Ypos+i, Length_LUT[i]);
        }
      break;
      case TOPLEFT:
        for(i=0;i<20;i++){
          BSP_LCD_DrawVLine(Xpos+i, Ypos, 8+CoinLength_LUT[i]);
        }
      break;
      case TOPRIGHT:
        for(i=0;i<20;i++){
          BSP_LCD_DrawHLine(Xpos, Ypos+i, 8+CoinLength_LUT[19-i]);
        }
      break;
      default:  return;     
      break;
    }
  }
