/*
 * Memory_operations.h
 *
 *  Created on: Mar 27, 2024
 *      Author: alice
 */

#ifndef INC_MEMORY_OPERATIONS_H_
#define INC_MEMORY_OPERATIONS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "main.h"
#include "SPI.h"
#include "SPI_NAND.h"

#define BYTES_PER_SAMPLE 17 // 6 axis imu * 2 (acc and gyro)
#define SAMPLES_PER_PAGE 240 // 4096/BYTES_PER_SAMPLE rounded
#define PPG_NAND_SAMPLE_MARKER 0xA5U
#define PPG_NAND_BYTES_PER_SAMPLE 11U
#define PPG_NAND_SAMPLES_PER_PAGE (SPI_NAND_PAGE_SIZE / PPG_NAND_BYTES_PER_SAMPLE)

typedef struct bookmark
{
  uint16_t blocco_scritto;
  uint8_t pagina_scritta;
  int b;

}NAND_info;
typedef struct Time
      {
          uint8_t hh;
          uint8_t mm;
          uint8_t ss;
          uint16_t sss;
      }Time_Struct;

void find_bad_blocks(uint16_t *bad_blocks);
void erase_good_blocks(uint8_t *bad_blocks);
NAND_info read_memory(int b, NAND_info indice, uint16_t *blocco_letto, uint8_t *pagina_letta, uint16_t bad_blocks[2048], uint8_t *data_letto);
void write_info(NAND_info segnalibro, uint16_t bad_blocks[2048]);
NAND_info read_info(uint16_t bad_blocks[2048]);
void write_packet(uint16_t sample, Time_Struct timestamp, uint8_t *gyroscope,uint8_t *accelerometer, uint8_t *NAND_packet);
void write_ppg_packet(uint16_t sample_index, uint32_t timestamp_ms,
                      uint32_t red_sample, uint32_t ir_sample,
                      uint8_t *NAND_packet);
void append_ppg_sample_to_nand(uint32_t timestamp_ms, uint32_t red_sample,
                               uint32_t ir_sample);

#ifdef __cplusplus
}
#endif

#endif /* INC_MEMORY_OPERATIONS_H_ */
