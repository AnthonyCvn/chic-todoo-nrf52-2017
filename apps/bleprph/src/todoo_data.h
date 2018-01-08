/* 
* Definition of the data structure "todoo"
* The purpose of this structure is to store all
* informations about the scheduled activities
*
* For each activity, the information of the time,
* (start and end), date and the addresse of the
* data stored in the external SPI memory are stored.
* The size of the data stored are in number of byte
* and stored in data_size
*
* The genral parameters (which theme, transition,..)
* are also stored with this structure.
*
* CHIC - China Hardware Innovation Camp - Todoo
* https://chi.camp/projects/todoo/
*
* Anthony Cavin
* anthony.cavin.ac@gmail.com
* 14 November 2017
*/

#ifndef TODOO_DATA_H_INCLUDED
#define TODOO_DATA_H_INCLUDED

#define N_BYTES_TIME 3

#define MAX_ACTIVITY 256        // 8 bits (0-255)
#define N_BYTES_PICTURE 16200   // picture 90px90p 2B/p
#define N_BYTES_128x128_BMP 32834-10000
#define N_BYTES_90x90_BMP   16266

#define B_SEC   2
#define B_MIN   1
#define B_HOUR  0

// Address of fix images in external SPI flash memory
#define ADD_TEST_PIC        (0x010000)
#define ADD_ADV_REQ_PIC     (0x018042)
#define ADD_SHARING_PIC     (0x020084)
#define ADD_FREE_TIME_PIC   (0x0280C6)
#define ADD_BRAND_PIC       (0x030108)

#define ADD_FIRST_ACTIVITY_PIC    (0x03814A)
#define NUM_BYTE_ACTIVITY_PIC     (0x3F48)

// 1) 1B theme
// 2) 3B hour actuel [Hour] [minute] [second]
// 3) 1B day [day of the week] 0-6 from Monday to Sunday
// 4) 1B Number of activity to send
// 5  for(first activity to the last one)
// 6)   1B day of the activity that are send, 0 for Monday
// 7)   2B Start activity [hour] [minute]
// 8)   2B End activity [hour] [minute]
// 9) end
// 5  for(first activity to the last one)
// 6)   16200B picture 90px90p in Bitmap (2B/p) for each activity 
// 9) end

/* State declaration */
typedef enum {
    boot,
    ble_request,
    advertize,
    pairing,
    receive_clock,
    receive_data_todoo,
    receive_pictures,
    shows_activity
}STATE;

struct Parameters{
    uint8_t  theme;   
    uint8_t  transition;
    uint8_t  num_activity;
    uint8_t  day;
    uint8_t  time[N_BYTES_TIME];
};
struct Activity{
    uint8_t  day;
    uint8_t  start_time[N_BYTES_TIME];
    uint8_t  end_time[N_BYTES_TIME];
    uint32_t data_add;
    uint16_t data_size;
};
struct Todoo_data{
    struct Parameters *parameters;
    struct Activity   *activity;
    STATE which_state;
    uint8_t config_state;
};



extern struct Todoo_data *todoo;


#endif // TODOO_DATA_H_INCLUDED