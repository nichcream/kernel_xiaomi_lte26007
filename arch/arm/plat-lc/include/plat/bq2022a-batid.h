#ifndef __BQ2022A_H__
#define __BQ2022A_H__

struct bq2022a_platform_data {
    int (*bat_id_invalid)(void);
    int sdq_pin;
    int sdq_gpio;
};
#define READ_ROM            (0x33)
#define MATCH_ROM           (0x55)
#define SEARCH_ROM          (0xF0)
#define SKIP_ROM            (0xCC)
#define READ_MEMORY_PAGE    (0xC3)
#define READ_MEMORY_FIELD   (0xF0)
#define WRITE_MEMORY        (0x0F)
#define READ_STATUS         (0xAA)
#define WRITE_STATUS        (0x55)
#define PROGRAM_CONTROL     (0x5A)
#define PROGRAM_PROFILE     (0x99)
#define LOW_BYTE            (0x00)/* Write Address Low Byte*/
#define HIGH_BYTE           (0x00)/* Write Address High Byte*/


extern int bq2022a_read_bat_id(void);

#endif
