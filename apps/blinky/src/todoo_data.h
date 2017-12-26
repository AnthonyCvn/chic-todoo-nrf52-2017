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

#define MAX_ACTIVITY 256    // 8 bits (0-255)

#define B_SEC   2
#define B_MIN   1
#define B_HOUR  0


// 1) 1B theme
// 2) 3B heure actuel [Heur] [minute] [second]
// 3) 1B date [jour de la semaine] 0-6 du lundi au dimanche
// 4) 1B nombre d'activité envoyé
// 5  for(première activité à la dernière)
// 6)   1B indiquer le jour de l'activité qui arrive, 0 pour le lundi 
// 7)   2B début d'activité [heure] [minute]
// 8)   2B fin d'activité [heure] [minute]
// 9) end
// 5  for(première activité à la dernière)
// 6)   16200B image 90px90p en bitmap de chaque activité 
// 9) end


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
};

extern struct Todoo_data *todoo;


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

#endif // TODOO_DATA_H_INCLUDED