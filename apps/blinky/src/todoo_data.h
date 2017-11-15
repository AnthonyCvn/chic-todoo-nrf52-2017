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

#define N_BYTES_DATE_OR_TIME 3

#define B_DAY   0
#define B_MONTH 1
#define B_YEAR  2

#define B_SEC   0
#define B_MIN   1
#define B_HOUR  2


struct Parameters{
    uint8_t  theme;
    uint8_t  transition;
    uint8_t  num_activity;
    uint8_t  date[N_BYTES_DATE_OR_TIME];
    uint8_t  time[N_BYTES_DATE_OR_TIME];
};
struct Activity{
    uint8_t  date[N_BYTES_DATE_OR_TIME];
    uint8_t  start_time[N_BYTES_DATE_OR_TIME];
    uint8_t  end_time[N_BYTES_DATE_OR_TIME];
    uint32_t data_add;
    uint16_t data_size;
};
struct Todoo_data{
    struct Parameters *parameters;
    struct Activity   *activity;
};

extern struct Todoo_data *todoo;

#endif // TODOO_DATA_H_INCLUDED