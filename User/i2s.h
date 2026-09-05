/*
 * i2s.h
 *
 *  Created on: Jul 1, 2026
 *      Author: Ahri
 */

#ifndef USER_I2S_H_
#define USER_I2S_H_

#include "ch32v30x.h"

#define I2S_DMA_BUF_SIZE    1024*2  // Kích thước mảng DMA cho I2S
#define AUDIO_RING_SIZE       (32 * 1024)

extern uint8_t i2s_tx_dma_buf[I2S_DMA_BUF_SIZE];
extern volatile uint32_t audio_wr_ptr;
extern volatile uint32_t audio_rd_ptr;
extern volatile uint8_t playback_started;
extern volatile uint16_t current_bit_depth;
extern uint32_t available;
/* Debug: preview of current DMA buffer contents (first bytes) */
extern volatile uint8_t dma_preview[16];
extern volatile uint8_t dma_preview_ready;
/* Debug: preview of recent USB EP1 OUT packet (first bytes) */
extern volatile uint8_t usb_ep1_preview[16];
extern volatile uint8_t usb_ep1_preview_ready;
extern volatile uint16_t usb_ep1_last_len;
extern volatile uint32_t dma_irq_count;

void PLL3_Audio_Clock_Init(uint32_t AudioFreq);
void I2S2_Init(uint32_t AudioFreq, uint16_t DataFormat);
void I2S2_DMA_Init(void);
void Fetch_Audio_Data(uint8_t *dest, uint32_t length);


#endif /* USER_I2S_H_ */
