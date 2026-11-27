/*
 * i2s.c
 *
 *  Created on: Jul 1, 2026
 *      Author: Ahri
 */
#include "i2s.h"
#include "string.h"
#include "USB_Device/ch32v30x_usbhs_device.h"

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
    // PA8 = HIGH (1) -> Select 24.576 MHz XO for 48kHz / 96kHz / 192kHz
    if (AudioFreq == 44100 ) {
        GPIO_ResetBits(GPIOA, GPIO_Pin_8);
    } else {
        GPIO_SetBits(GPIOA, GPIO_Pin_8);
    }

    DMA_Cmd(DMA1_Channel5, DISABLE);
    I2S_Cmd(SPI2, DISABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, DISABLE);

    // 3. Initialize PLL3 internal audio clock source BEFORE enabling SPI2 APB clock
    PLL3_Audio_Clock_Init(AudioFreq);

    // Enable SPI2 APB clock AFTER I2S2 clock source is selected
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE);
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
    SPI_I2S_DeInit(SPI2);

    // 4. Configure I2S2 Pins (PB12-WS, PB13-CK, PB15-SD)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_15;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

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
    uint32_t bytes_per_frame = (current_bit_depth == I2S_DataFormat_16b) ? 4 : 8;
    uint32_t bytes_needed = frames * bytes_per_frame;

    // 2. Underflow protection
    static uint32_t underflow_count = 0;
    uint16_t *dest_16 = (uint16_t *)dest;
    if (available < bytes_needed) {
        memset(dest, 0, length);
        underflow_count++;
        if (underflow_count > 25) { // ~66ms sustained underflow -> stream stopped
            playback_started = 0;
            underflow_count = 0;
        }
        return;
    }
    underflow_count = 0;

    // 3. Elastic Rate Adaptation with Smooth Linear Interpolation (Zero-Click Resampling)
    // Eliminates both macro-overflows (after 37s on Android) AND subtle micro-clicks
    // by smoothly interpolating across the 128-sample block instead of hard sample drops.
    static uint8_t slip_state = 0; // 0: idle, 1: draining (buffer high), 2: filling (buffer low)
    static uint8_t slip_timer = 0;
    int32_t slip = 0;

    // Hysteresis control:
    // When buffer rises above 20,480 bytes (~62.5%), enter draining mode
    // Drain until buffer falls back below 17,408 bytes (~53%), then return to idle
    if (available > (AUDIO_RING_SIZE * 5) / 8) {
        slip_state = 1;
    } else if (slip_state == 1 && available <= (AUDIO_RING_SIZE / 2) + 1024) {
        slip_state = 0;
    }
    // When buffer drops below 12,288 bytes (~37.5%), enter filling mode
    // Fill until buffer climbs back above 15,360 bytes (~47%), then return to idle
    else if (available < (AUDIO_RING_SIZE * 3) / 8) {
        slip_state = 2;
    } else if (slip_state == 2 && available >= (AUDIO_RING_SIZE / 2) - 1024) {
        slip_state = 0;
    }

    if (slip_state != 0) {
        slip_timer++;
        // Pace: 1 slip every 6 callbacks (~16ms = ~62.5 slips/sec, perfectly countering 54.8 Hz drift)
        if (slip_timer >= 6) {
            slip_timer = 0;
            slip = (slip_state == 1) ? 1 : -1;
        }
    } else {
        slip_timer = 0;
    }

    uint32_t idx = 0;

    if (slip == 1) {
        // --- SMOOTH COMPRESS: Interpolate 129 input frames into 128 output frames ---
        // S_i -> S_{i+1} linear crossfade across the 128 samples:
        // Out[i] = S[i] + ((S[i+1] - S[i]) * i) / 128
        // At i=0: Out[0] = S[0] (exact match with previous block)
        // At i=127: Out[127] ~= S[128] (exact match with next block S[128..129])
        // Result: 100% continuous waveform, ZERO impulse, ZERO pop/click!
        if (current_bit_depth == I2S_DataFormat_16b) {
            for (uint32_t i = 0; i < frames; i++) {
                uint32_t c_pos = (local_rd + (i * 4)) & (AUDIO_RING_SIZE - 1);
                uint32_t n_pos = (local_rd + ((i + 1) * 4)) & (AUDIO_RING_SIZE - 1);

                int16_t s0_L = (int16_t)((audio_ring_buf[(c_pos + 1) & (AUDIO_RING_SIZE - 1)] << 8) | audio_ring_buf[c_pos]);
                int16_t s0_R = (int16_t)((audio_ring_buf[(c_pos + 3) & (AUDIO_RING_SIZE - 1)] << 8) | audio_ring_buf[(c_pos + 2) & (AUDIO_RING_SIZE - 1)]);

                int16_t s1_L = (int16_t)((audio_ring_buf[(n_pos + 1) & (AUDIO_RING_SIZE - 1)] << 8) | audio_ring_buf[n_pos]);
                int16_t s1_R = (int16_t)((audio_ring_buf[(n_pos + 3) & (AUDIO_RING_SIZE - 1)] << 8) | audio_ring_buf[(n_pos + 2) & (AUDIO_RING_SIZE - 1)]);

                int16_t out_L = s0_L + (int16_t)(((int32_t)(s1_L - s0_L) * (int32_t)i) >> 7);
                int16_t out_R = s0_R + (int16_t)(((int32_t)(s1_R - s0_R) * (int32_t)i) >> 7);

                dest_16[idx++] = (uint16_t)out_L;
                dest_16[idx++] = 0x0000;
                dest_16[idx++] = (uint16_t)out_R;
                dest_16[idx++] = 0x0000;
            }
            local_rd = (local_rd + ((frames + 1) * 4)) & (AUDIO_RING_SIZE - 1);
        } else {
            for (uint32_t i = 0; i < frames; i++) {
                uint32_t c_pos = (local_rd + (i * 8)) & (AUDIO_RING_SIZE - 1);
                uint32_t n_pos = (local_rd + ((i + 1) * 8)) & (AUDIO_RING_SIZE - 1);

                uint8_t l0 = audio_ring_buf[c_pos];
                uint8_t l1 = audio_ring_buf[(c_pos + 1) & (AUDIO_RING_SIZE - 1)];
                uint8_t l2 = audio_ring_buf[(c_pos + 2) & (AUDIO_RING_SIZE - 1)];
                uint8_t l3 = audio_ring_buf[(c_pos + 3) & (AUDIO_RING_SIZE - 1)];

                uint8_t r0 = audio_ring_buf[(c_pos + 4) & (AUDIO_RING_SIZE - 1)];
                uint8_t r1 = audio_ring_buf[(c_pos + 5) & (AUDIO_RING_SIZE - 1)];
                uint8_t r2 = audio_ring_buf[(c_pos + 6) & (AUDIO_RING_SIZE - 1)];
                uint8_t r3 = audio_ring_buf[(c_pos + 7) & (AUDIO_RING_SIZE - 1)];

                int32_t s0_L = (int32_t)((uint32_t)l0 | ((uint32_t)l1 << 8) | ((uint32_t)l2 << 16) | ((uint32_t)l3 << 24));
                int32_t s0_R = (int32_t)((uint32_t)r0 | ((uint32_t)r1 << 8) | ((uint32_t)r2 << 16) | ((uint32_t)r3 << 24));

                uint8_t nl0 = audio_ring_buf[n_pos];
                uint8_t nl1 = audio_ring_buf[(n_pos + 1) & (AUDIO_RING_SIZE - 1)];
                uint8_t nl2 = audio_ring_buf[(n_pos + 2) & (AUDIO_RING_SIZE - 1)];
                uint8_t nl3 = audio_ring_buf[(n_pos + 3) & (AUDIO_RING_SIZE - 1)];

                uint8_t nr0 = audio_ring_buf[(n_pos + 4) & (AUDIO_RING_SIZE - 1)];
                uint8_t nr1 = audio_ring_buf[(n_pos + 5) & (AUDIO_RING_SIZE - 1)];
                uint8_t nr2 = audio_ring_buf[(n_pos + 6) & (AUDIO_RING_SIZE - 1)];
                uint8_t nr3 = audio_ring_buf[(n_pos + 7) & (AUDIO_RING_SIZE - 1)];

                int32_t s1_L = (int32_t)((uint32_t)nl0 | ((uint32_t)nl1 << 8) | ((uint32_t)nl2 << 16) | ((uint32_t)nl3 << 24));
                int32_t s1_R = (int32_t)((uint32_t)nr0 | ((uint32_t)nr1 << 8) | ((uint32_t)nr2 << 16) | ((uint32_t)nr3 << 24));

                int32_t out_L = s0_L + (int32_t)(((int64_t)(s1_L - s0_L) * (int64_t)i) >> 7);
                int32_t out_R = s0_R + (int32_t)(((int64_t)(s1_R - s0_R) * (int64_t)i) >> 7);

                dest_16[idx++] = (uint16_t)((uint32_t)out_L >> 16);
                dest_16[idx++] = (uint16_t)((uint32_t)out_L & 0xFFFF);
                dest_16[idx++] = (uint16_t)((uint32_t)out_R >> 16);
                dest_16[idx++] = (uint16_t)((uint32_t)out_R & 0xFFFF);
            }
            local_rd = (local_rd + ((frames + 1) * 8)) & (AUDIO_RING_SIZE - 1);
        }
    } else if (slip == -1) {
        // --- SMOOTH STRETCH: Interpolate 127 input frames into 128 output frames ---
        if (current_bit_depth == I2S_DataFormat_16b) {
            for (uint32_t i = 0; i < frames; i++) {
                uint32_t k = (i * (frames - 2)) >> 7; // map 0..127 to 0..126
                uint32_t frac = (i * (frames - 2)) & 0x7F;
                uint32_t c_pos = (local_rd + (k * 4)) & (AUDIO_RING_SIZE - 1);
                uint32_t n_pos = (local_rd + ((k + 1) * 4)) & (AUDIO_RING_SIZE - 1);

                int16_t s0_L = (int16_t)((audio_ring_buf[(c_pos + 1) & (AUDIO_RING_SIZE - 1)] << 8) | audio_ring_buf[c_pos]);
                int16_t s0_R = (int16_t)((audio_ring_buf[(c_pos + 3) & (AUDIO_RING_SIZE - 1)] << 8) | audio_ring_buf[(c_pos + 2) & (AUDIO_RING_SIZE - 1)]);

                int16_t s1_L = (int16_t)((audio_ring_buf[(n_pos + 1) & (AUDIO_RING_SIZE - 1)] << 8) | audio_ring_buf[n_pos]);
                int16_t s1_R = (int16_t)((audio_ring_buf[(n_pos + 3) & (AUDIO_RING_SIZE - 1)] << 8) | audio_ring_buf[(n_pos + 2) & (AUDIO_RING_SIZE - 1)]);

                int16_t out_L = s0_L + (int16_t)(((int32_t)(s1_L - s0_L) * (int32_t)frac) >> 7);
                int16_t out_R = s0_R + (int16_t)(((int32_t)(s1_R - s0_R) * (int32_t)frac) >> 7);

                dest_16[idx++] = (uint16_t)out_L;
                dest_16[idx++] = 0x0000;
                dest_16[idx++] = (uint16_t)out_R;
                dest_16[idx++] = 0x0000;
            }
            local_rd = (local_rd + ((frames - 1) * 4)) & (AUDIO_RING_SIZE - 1);
        } else {
            for (uint32_t i = 0; i < frames; i++) {
                uint32_t k = (i * (frames - 2)) >> 7; // map 0..127 to 0..126
                uint32_t frac = (i * (frames - 2)) & 0x7F;
                uint32_t c_pos = (local_rd + (k * 8)) & (AUDIO_RING_SIZE - 1);
                uint32_t n_pos = (local_rd + ((k + 1) * 8)) & (AUDIO_RING_SIZE - 1);

                uint8_t l0 = audio_ring_buf[c_pos];
                uint8_t l1 = audio_ring_buf[(c_pos + 1) & (AUDIO_RING_SIZE - 1)];
                uint8_t l2 = audio_ring_buf[(c_pos + 2) & (AUDIO_RING_SIZE - 1)];
                uint8_t l3 = audio_ring_buf[(c_pos + 3) & (AUDIO_RING_SIZE - 1)];

                uint8_t r0 = audio_ring_buf[(c_pos + 4) & (AUDIO_RING_SIZE - 1)];
                uint8_t r1 = audio_ring_buf[(c_pos + 5) & (AUDIO_RING_SIZE - 1)];
                uint8_t r2 = audio_ring_buf[(c_pos + 6) & (AUDIO_RING_SIZE - 1)];
                uint8_t r3 = audio_ring_buf[(c_pos + 7) & (AUDIO_RING_SIZE - 1)];

                int32_t s0_L = (int32_t)((uint32_t)l0 | ((uint32_t)l1 << 8) | ((uint32_t)l2 << 16) | ((uint32_t)l3 << 24));
                int32_t s0_R = (int32_t)((uint32_t)r0 | ((uint32_t)r1 << 8) | ((uint32_t)r2 << 16) | ((uint32_t)r3 << 24));

                uint8_t nl0 = audio_ring_buf[n_pos];
                uint8_t nl1 = audio_ring_buf[(n_pos + 1) & (AUDIO_RING_SIZE - 1)];
                uint8_t nl2 = audio_ring_buf[(n_pos + 2) & (AUDIO_RING_SIZE - 1)];
                uint8_t nl3 = audio_ring_buf[(n_pos + 3) & (AUDIO_RING_SIZE - 1)];

                uint8_t nr0 = audio_ring_buf[(n_pos + 4) & (AUDIO_RING_SIZE - 1)];
                uint8_t nr1 = audio_ring_buf[(n_pos + 5) & (AUDIO_RING_SIZE - 1)];
                uint8_t nr2 = audio_ring_buf[(n_pos + 6) & (AUDIO_RING_SIZE - 1)];
                uint8_t nr3 = audio_ring_buf[(n_pos + 7) & (AUDIO_RING_SIZE - 1)];

                int32_t s1_L = (int32_t)((uint32_t)nl0 | ((uint32_t)nl1 << 8) | ((uint32_t)nl2 << 16) | ((uint32_t)nl3 << 24));
                int32_t s1_R = (int32_t)((uint32_t)nr0 | ((uint32_t)nr1 << 8) | ((uint32_t)nr2 << 16) | ((uint32_t)nr3 << 24));

                int32_t out_L = s0_L + (int32_t)(((int64_t)(s1_L - s0_L) * (int64_t)frac) >> 7);
                int32_t out_R = s0_R + (int32_t)(((int64_t)(s1_R - s0_R) * (int64_t)frac) >> 7);

                dest_16[idx++] = (uint16_t)((uint32_t)out_L >> 16);
                dest_16[idx++] = (uint16_t)((uint32_t)out_L & 0xFFFF);
                dest_16[idx++] = (uint16_t)((uint32_t)out_R >> 16);
                dest_16[idx++] = (uint16_t)((uint32_t)out_R & 0xFFFF);
            }
            local_rd = (local_rd + ((frames - 1) * 8)) & (AUDIO_RING_SIZE - 1);
        }
    } else {
        // --- NORMAL PLAYBACK: 100% Bit-Perfect 1:1 Direct Read (Standard on PC) ---
        if (current_bit_depth == I2S_DataFormat_16b) {
            for (uint32_t i = 0; i < frames; i++) {
                uint8_t l0 = audio_ring_buf[local_rd];
                uint8_t l1 = audio_ring_buf[(local_rd + 1) & (AUDIO_RING_SIZE - 1)];
                uint8_t r0 = audio_ring_buf[(local_rd + 2) & (AUDIO_RING_SIZE - 1)];
                uint8_t r1 = audio_ring_buf[(local_rd + 3) & (AUDIO_RING_SIZE - 1)];
                local_rd = (local_rd + 4) & (AUDIO_RING_SIZE - 1);

                uint16_t left_audio  = (uint16_t)((l1 << 8) | l0);
                uint16_t right_audio = (uint16_t)((r1 << 8) | r0);

                dest_16[idx++] = left_audio;    // High half of left slot
                dest_16[idx++] = 0x0000;        // Low half of left slot
                dest_16[idx++] = right_audio;   // High half of right slot
                dest_16[idx++] = 0x0000;        // Low half of right slot
            }
        } else {
            for (uint32_t i = 0; i < frames; i++) {
                uint8_t l0 = audio_ring_buf[local_rd];
                uint8_t l1 = audio_ring_buf[(local_rd + 1) & (AUDIO_RING_SIZE - 1)];
                uint8_t l2 = audio_ring_buf[(local_rd + 2) & (AUDIO_RING_SIZE - 1)];
                uint8_t l3 = audio_ring_buf[(local_rd + 3) & (AUDIO_RING_SIZE - 1)];

                uint8_t r0 = audio_ring_buf[(local_rd + 4) & (AUDIO_RING_SIZE - 1)];
                uint8_t r1 = audio_ring_buf[(local_rd + 5) & (AUDIO_RING_SIZE - 1)];
                uint8_t r2 = audio_ring_buf[(local_rd + 6) & (AUDIO_RING_SIZE - 1)];
                uint8_t r3 = audio_ring_buf[(local_rd + 7) & (AUDIO_RING_SIZE - 1)];
                local_rd = (local_rd + 8) & (AUDIO_RING_SIZE - 1);

                uint32_t val_L = (uint32_t)l0 | ((uint32_t)l1 << 8) | ((uint32_t)l2 << 16) | ((uint32_t)l3 << 24);
                uint32_t val_R = (uint32_t)r0 | ((uint32_t)r1 << 8) | ((uint32_t)r2 << 16) | ((uint32_t)r3 << 24);

                dest_16[idx++] = (uint16_t)(val_L >> 16);
                dest_16[idx++] = (uint16_t)(val_L & 0xFFFF);
                dest_16[idx++] = (uint16_t)(val_R >> 16);
                dest_16[idx++] = (uint16_t)(val_R & 0xFFFF);
            }
        }
    }

    if (!i2s_reinit_pending) {
        audio_rd_ptr = local_rd;
    }
}
