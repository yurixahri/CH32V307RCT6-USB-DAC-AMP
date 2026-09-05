/*
 * i2s.c
 *
 *  Created on: Jul 1, 2026
 *      Author: Ahri
 */
#include "i2s.h"
#include "string.h"

__attribute__ ((aligned(4))) uint8_t i2s_tx_dma_buf[I2S_DMA_BUF_SIZE];
extern uint8_t audio_ring_buf[];
volatile uint32_t audio_wr_ptr = 0;
volatile uint32_t audio_rd_ptr = 0;
uint32_t available = 0;

/* Debug preview buffer: capture first bytes written to DMA half-buffer */
volatile uint8_t dma_preview[16] __attribute__ ((aligned(4)));
volatile uint8_t dma_preview_ready = 0;

void PLL3_Audio_Clock_Init(uint32_t AudioFreq) {
    RCC_HSEConfig(RCC_HSE_ON);
    if (RCC_WaitForHSEStartUp() == READY) {
        RCC_PLL3Cmd(DISABLE);
        while (RCC_GetFlagStatus(RCC_FLAG_PLL3RDY) != RESET);
        if (AudioFreq == 44100) {
            RCC_PREDIV2Config(RCC_PREDIV2_Div5);
            RCC_PLL3Config(RCC_PLL3Mul_14);
        } else if (AudioFreq == 48000) {
            RCC_PREDIV2Config(RCC_PREDIV2_Div1);
            RCC_PLL3Config(RCC_PLL3Mul_14);
        } else if (AudioFreq == 96000) {
            RCC_PREDIV2Config(RCC_PREDIV2_Div2);
            RCC_PLL3Config(RCC_PLL3Mul_20);
        }else if (AudioFreq == 192000) {
            RCC_PREDIV2Config(RCC_PREDIV2_Div1);
            RCC_PLL3Config(RCC_PLL3Mul_10);
        }

        RCC_PLL3Cmd(ENABLE);
        while (RCC_GetFlagStatus(RCC_FLAG_PLL3RDY) == RESET);

        RCC_I2S2CLKConfig(RCC_I2S2CLKSource_PLL3_VCO);
    }
}

void I2S2_Init(uint32_t AudioFreq, uint16_t DataFormat){
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    I2S_InitTypeDef I2S_InitStructure = {0};

    // 1. Enable Clocks for GPIOA, GPIOB, and AFIO
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);

    // 2. Configure PA8 (Mux Select Output)

    // PA8 = LOW (0) -> Select 22.5792 MHz XO for 44.1kHz
    // PA8 = HIGH (1) -> Select 24.576 MHz XO for 48kHz / 96kHz
    if (AudioFreq == 44100 ) {
        GPIO_ResetBits(GPIOA, GPIO_Pin_8);
    } else {
        GPIO_SetBits(GPIOA, GPIO_Pin_8);
    }

    Delay_Ms(5);

    // 3. Initialize PLL3 internal audio clock source
    PLL3_Audio_Clock_Init(AudioFreq);

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE);

    // 4. Configure I2S2 Pins (PB12-WS, PB13-CK, PB15-SD)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_15;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    I2S_Cmd(SPI2, DISABLE);
    memset(i2s_tx_dma_buf, 0, sizeof(i2s_tx_dma_buf));

    I2S_InitStructure.I2S_Mode = I2S_Mode_MasterTx;
    I2S_InitStructure.I2S_Standard = I2S_Standard_Phillips;
    I2S_InitStructure.I2S_DataFormat = I2S_DataFormat_32b;
    I2S_InitStructure.I2S_MCLKOutput = I2S_MCLKOutput_Disable; // External Mux delivers MCLK directly to DAC
    I2S_InitStructure.I2S_AudioFreq = AudioFreq;
    I2S_InitStructure.I2S_CPOL = I2S_CPOL_Low;

    I2S_Init(SPI2, &I2S_InitStructure);

    if (AudioFreq == 192000) {
        SPI2->I2SPR = (0 << 9) | (1 << 8) | 6;
    } else if (AudioFreq == 96000) {
        SPI2->I2SPR = (0 << 9) | (0 << 8) | 13;
    } else if (AudioFreq == 48000) {
        SPI2->I2SPR = (0 << 9) | (1 << 8) | 36;
    } else if (AudioFreq == 44100) {
        SPI2->I2SPR = (0 << 9) | (0 << 8) | 0x08;
    }

    SPI_I2S_DMACmd(SPI2, SPI_I2S_DMAReq_Tx, ENABLE);
    I2S2_DMA_Init();
    I2S_Cmd(SPI2, ENABLE);
}

void I2S2_DMA_Init(void) {
    DMA_InitTypeDef DMA_InitStructure = {0};
    NVIC_InitTypeDef NVIC_InitStructure = {0};

    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
    DMA_DeInit(DMA1_Channel5);
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&SPI2->DATAR;
    DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)i2s_tx_dma_buf;
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralDST;
    DMA_InitStructure.DMA_BufferSize = I2S_DMA_BUF_SIZE / 2;
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;
    DMA_InitStructure.DMA_Priority = DMA_Priority_VeryHigh;
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel5, &DMA_InitStructure);

    DMA_ITConfig(DMA1_Channel5, DMA_IT_HT | DMA_IT_TC, ENABLE);

    NVIC_InitStructure.NVIC_IRQChannel = DMA1_Channel5_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    DMA_Cmd(DMA1_Channel5, ENABLE);
}

void DMA1_Channel5_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void DMA1_Channel5_IRQHandler(void) {
    if (DMA_GetITStatus(DMA1_IT_HT5) != RESET) {
        DMA_ClearITPendingBit(DMA1_IT_HT5);
        Fetch_Audio_Data(&i2s_tx_dma_buf[0], I2S_DMA_BUF_SIZE / 2);
    }
    if (DMA_GetITStatus(DMA1_IT_TC5) != RESET) {
        DMA_ClearITPendingBit(DMA1_IT_TC5);
        Fetch_Audio_Data(&i2s_tx_dma_buf[I2S_DMA_BUF_SIZE / 2], I2S_DMA_BUF_SIZE / 2);
    }
}

volatile uint8_t playback_started = 0;
volatile uint32_t underflow_count = 0;
void Fetch_Audio_Data(uint8_t *dest, uint32_t length) {
    uint32_t local_rd, local_wr;

    __disable_irq();
    local_rd = audio_rd_ptr;
    local_wr = audio_wr_ptr;
    __enable_irq();

    available = (local_wr >= local_rd) ?
                     (local_wr - local_rd) :
                     (AUDIO_RING_SIZE - local_rd + local_wr);

    // 1. Initial buffer safety buildup
    if (!playback_started) {
        if (available >= AUDIO_RING_SIZE / 2) {
            playback_started = 1;
        } else {
            memset(dest, 0, length);
            return;
        }
    }

    uint32_t frames = length / 8;
    uint32_t bytes_needed = (current_bit_depth == I2S_DataFormat_16b) ? (frames * 4) : (frames * 8);

    // 2. Underflow protection
    uint16_t *dest_16 = (uint16_t *)dest;
    if (available < bytes_needed) {
        underflow_count++;
        printf("underflow: %u", underflow_count);
        memset(dest, 0, length);
        // playback_started = 0;
        return;
    }

    uint32_t idx = 0;

    if (current_bit_depth == I2S_DataFormat_16b) {
        // --- 16-BIT DECODING (Left Channel first, Right Channel second) ---
        for (uint32_t i = 0; i < frames; i++) {
            uint8_t l0 = audio_ring_buf[local_rd++]; if(local_rd >= AUDIO_RING_SIZE) local_rd = 0;
            uint8_t l1 = audio_ring_buf[local_rd++]; if(local_rd >= AUDIO_RING_SIZE) local_rd = 0;
            uint8_t r0 = audio_ring_buf[local_rd++]; if(local_rd >= AUDIO_RING_SIZE) local_rd = 0;
            uint8_t r1 = audio_ring_buf[local_rd++]; if(local_rd >= AUDIO_RING_SIZE) local_rd = 0;

            uint16_t left_audio  = (uint16_t)((l1 << 8) | l0);
            uint16_t right_audio = (uint16_t)((r1 << 8) | r0);

            // Swapped to standard I2S ordering: LEFT -> RIGHT
            dest_16[idx++] = left_audio;    // High half of left slot
            dest_16[idx++] = 0x0000;        // Low half of left slot
            dest_16[idx++] = right_audio;   // High half of right slot
            dest_16[idx++] = 0x0000;        // Low half of right slot
        }
    } else {
        // --- 24-BIT DECODING (Left Channel first, Right Channel second) ---
        for (uint32_t i = 0; i < frames; i++) {
            uint8_t l0 = audio_ring_buf[local_rd++]; if(local_rd >= AUDIO_RING_SIZE) local_rd = 0;
            uint8_t l1 = audio_ring_buf[local_rd++]; if(local_rd >= AUDIO_RING_SIZE) local_rd = 0;
            uint8_t l2 = audio_ring_buf[local_rd++]; if(local_rd >= AUDIO_RING_SIZE) local_rd = 0;
            uint8_t l3 = audio_ring_buf[local_rd++]; if(local_rd >= AUDIO_RING_SIZE) local_rd = 0;

            uint8_t r0 = audio_ring_buf[local_rd++]; if(local_rd >= AUDIO_RING_SIZE) local_rd = 0;
            uint8_t r1 = audio_ring_buf[local_rd++]; if(local_rd >= AUDIO_RING_SIZE) local_rd = 0;
            uint8_t r2 = audio_ring_buf[local_rd++]; if(local_rd >= AUDIO_RING_SIZE) local_rd = 0;
            uint8_t r3 = audio_ring_buf[local_rd++]; if(local_rd >= AUDIO_RING_SIZE) local_rd = 0;

            uint32_t val_L = l0 | ((uint32_t)l1 << 8) | ((uint32_t)l2 << 16) | ((uint32_t)l3 << 24);
            uint32_t val_R = r0 | ((uint32_t)r1 << 8) | ((uint32_t)r2 << 16) | ((uint32_t)r3 << 24);

            // Swapped to standard I2S ordering: LEFT -> RIGHT
            dest_16[idx++] = (uint16_t)(val_L >> 16);
            dest_16[idx++] = (uint16_t)(val_L & 0xFFFF);
            dest_16[idx++] = (uint16_t)(val_R >> 16);
            dest_16[idx++] = (uint16_t)(val_R & 0xFFFF);
        }
    }

    audio_rd_ptr = local_rd;
}
