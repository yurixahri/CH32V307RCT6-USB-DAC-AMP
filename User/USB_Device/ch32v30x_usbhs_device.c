/********************************** (C) COPYRIGHT *******************************
* File Name          : ch32v30x_usbhs_device.c
* Author             : WCH
* Version            : V1.0.0
* Date               : 2023/11/20
* Description        : This file provides all the USBHS firmware functions.
*********************************************************************************
* Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for 
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/
#include "ch32v30x_usbhs_device.h"
#include "i2s.h"

/******************************************************************************/
/* Variable Definition */
/* test mode */
volatile uint8_t  USBHS_Test_Flag;
__attribute__ ((aligned(4))) uint8_t IFTest_Buf[ 53 ] =
{
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
    0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE,
    0xFE,//26
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,//37
    0x7F, 0xBF, 0xDF, 0xEF, 0xF7, 0xFB, 0xFD,//44
    0xFC, 0x7E, 0xBF, 0xDF, 0xEF, 0xF7, 0xFB, 0xFD, 0x7E//53
};

/* Global */
const uint8_t    *pUSBHS_Descr;

/* Setup Request */
volatile uint8_t  USBHS_SetupReqCode;
volatile uint8_t  USBHS_SetupReqType;
volatile uint16_t USBHS_SetupReqValue;
volatile uint16_t USBHS_SetupReqIndex;
volatile uint16_t USBHS_SetupReqLen;

/* USB Device Status */
volatile uint8_t  USBHS_DevConfig;
volatile uint8_t  USBHS_DevAddr;
volatile uint16_t USBHS_DevMaxPackLen;
volatile uint8_t  USBHS_DevSpeed;
volatile uint8_t  USBHS_DevSleepStatus;
volatile uint8_t  USBHS_DevEnumStatus;

/* Endpoint Buffer */
__attribute__ ((aligned(4))) uint8_t USBHS_EP0_Buf[ DEF_USBD_UEP0_SIZE ];
/* Cấu hình vùng đệm cho UAC2 Endpoint 1 */
__attribute__ ((aligned(4))) uint8_t USBHS_EP1_Rx_Buf[ DEF_USBD_HS_ISO_PACK_SIZE ]; // Nhận Isochronous Audio (1024 Bytes)
//__attribute__ ((aligned(4))) uint8_t USBHS_EP1_Tx_Buf[ 64 ];

//__attribute__ ((aligned(4))) uint8_t USBHS_EP2_Rx_Buf[ DEF_USBD_HS_ISO_PACK_SIZE ]; // Nhận Isochronous Audio (1024 Bytes)
__attribute__ ((aligned(4))) uint8_t USBHS_EP2_Tx_Buf[ 64 ];

/* Debug: capture first bytes of the latest EP1 OUT packet for inspection */
volatile uint8_t usb_ep1_preview[16] __attribute__ ((aligned(4)));
volatile uint8_t usb_ep1_preview_ready = 0;
volatile uint16_t usb_ep1_last_len = 0;

/* Endpoint tx busy flag */
volatile uint8_t  USBHS_Endp_Busy[ DEF_UEP_NUM ];

static __attribute__((aligned(4))) uint8_t uac2_range_data[] = {
    0x04, 0x00,             // wNumRanges = 4

    0x44, 0xAC, 0x00, 0x00, // wMin = 44100
    0x44, 0xAC, 0x00, 0x00, // wMax = 44100
    0x00, 0x00, 0x00, 0x00, // wRes = 0

    0x80, 0xBB, 0x00, 0x00, // wMin = 48000
    0x80, 0xBB, 0x00, 0x00, // wMax = 48000
    0x00, 0x00, 0x00, 0x00, // wRes = 0

    0x00, 0x77, 0x01, 0x00, // wMin = 96000
    0x00, 0x77, 0x01, 0x00, // wMax = 96000
    0x00, 0x00, 0x00, 0x00,  // wRes = 0

    0x00, 0xEE, 0x02, 0x00, // wMin = 192000
    0x00, 0xEE, 0x02, 0x00, // wMax = 192000
    0x00, 0x00, 0x00, 0x00  // wRes = 0
};

__attribute__ ((aligned(4))) uint8_t current_audio_freq[4] = {0x80, 0xBB, 0x00, 0x00};
__attribute__ ((aligned(4))) uint8_t audio_ring_buf[AUDIO_RING_SIZE];

volatile uint32_t audio_feedback_val = 0x00060000;
volatile uint16_t current_bit_depth = I2S_DataFormat_16b;
volatile uint32_t dynamic_fb = 0;
volatile uint32_t ep1_in_cnt = 0;
volatile int64_t smoothed_avail_fp = (AUDIO_RING_SIZE / 2) << 16; // Bộ lọc Fixed-Point
volatile int64_t feedback_integral = 0;          // Bộ tích phân PI
volatile uint32_t current_avail;
volatile int32_t smoothed_avail;
volatile uint8_t current_alt_setting = 0;
uint32_t target_freq;

/******************************************************************************/
/* Interrupt Service Routine Declaration*/
void USBHS_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));

/*********************************************************************
 * @fn      USB_TestMode_Deal
 *
 * @brief   Eye Diagram Test Function Processing.
 *
 * @return  none
 *
 */
void USB_TestMode_Deal( void )
{
    /* start test */
    USBHS_Test_Flag &= ~0x80;
    if( USBHS_SetupReqIndex == 0x0100 )
    {
        /* Test_J */
        USBHSD->SUSPEND &= ~TEST_MASK;
        USBHSD->SUSPEND |= TEST_J;
        USBHSD->CONTROL |= USBHS_UC_HOST_MODE;
    }
    else if( USBHS_SetupReqIndex == 0x0200 )
    {
        /* Test_K */
        USBHSD->SUSPEND &= ~TEST_MASK;
        USBHSD->SUSPEND |= TEST_K;
        USBHSD->CONTROL |= USBHS_UC_HOST_MODE;
    }
    else if( USBHS_SetupReqIndex == 0x0300 )
    {
        /* Test_SE0_NAK */
        USBHSD->SUSPEND &= ~TEST_MASK;
        USBHSD->SUSPEND |= TEST_PACKET;
        USBHSD->DEV_AD = 0x00;
        USBHSD->UEP0_RX_CTRL = USBHS_UEP_R_RES_NAK;
        USBHSD->UEP0_TX_CTRL = USBHS_UEP_T_RES_NAK;
    }
    else if( USBHS_SetupReqIndex == 0x0400 )
    {
        /* Test_Packet */ 
        USBHSD->SUSPEND &= ~TEST_MASK;
        USBHSD->SUSPEND |= TEST_PACKET;

        USBHSD->CONTROL |= USBHS_UC_HOST_MODE;
        USBHSH->HOST_EP_CONFIG = USBHS_UH_EP_TX_EN | USBHS_UH_EP_RX_EN;
        USBHSH->HOST_EP_TYPE |= 0xff;

        USBHSH->HOST_TX_DMA = (uint32_t)(&IFTest_Buf[ 0 ]);
        USBHSH->HOST_TX_LEN = 53;
        USBHSH->HOST_EP_PID = ( USB_PID_SETUP << 4 );
        USBHSH->INT_FG = USBHS_UIF_TRANSFER;
    }
}

/*********************************************************************
 * @fn      USBHS_RCC_Init
 *
 * @brief   Initializes the clock for USB2.0 High speed device.
 *
 * @return  none
 */
void USBHS_RCC_Init( void )
{
    RCC_USBCLK48MConfig(RCC_USBCLK48MCLKSource_USBPHY);
    RCC_USBHSPLLCLKConfig(RCC_HSBHSPLLCLKSource_HSE);
    RCC_USBHSConfig(RCC_USBPLL_Div2);
    RCC_USBHSPLLCKREFCLKConfig(RCC_USBHSPLLCKREFCLK_4M);
    RCC_USBHSPHYPLLALIVEcmd(ENABLE);
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_USBHS, ENABLE);
}

/*********************************************************************
 * @fn      USBHS_Device_Endp_Init
 *
 * @brief   Initializes USB device endpoints.
 *
 * @return  none
 */
void USBHS_Device_Endp_Init ( void ){
    /* EP0 Control */
    USBHSD->UEP0_DMA     = (uint32_t)USBHS_EP0_Buf;
    USBHSD->UEP0_MAX_LEN = DEF_USBD_UEP0_SIZE;
    USBHSD->UEP0_TX_CTRL = USBHS_UEP_T_RES_NAK;
    USBHSD->UEP0_RX_CTRL = USBHS_UEP_R_RES_ACK;

    /* EP1: Nhận nhạc (RX) */
    USBHSD->UEP1_RX_DMA   = (uint32_t)USBHS_EP1_Rx_Buf;
    USBHSD->UEP1_MAX_LEN  = DEF_USBD_HS_ISO_PACK_SIZE;
    USBHSD->ENDP_TYPE = (USBHSD->ENDP_TYPE & ~(3 << 2)) | (1 << 2); // Isochronous
    USBHSD->UEP1_RX_CTRL = USBHS_UEP_R_RES_ACK;
    USBHSD->UEP1_TX_CTRL = USBHS_UEP_T_RES_STALL;

    /* EP2: Gửi Feedback (TX) */
    USBHSD->UEP2_TX_DMA   = (uint32_t)USBHS_EP2_Tx_Buf;
    USBHSD->UEP2_MAX_LEN  = 4; // Cực kỳ an toàn!
    USBHSD->ENDP_TYPE = (USBHSD->ENDP_TYPE & ~(3 << 18)) | (1 << 18); // Isochronous
    USBHSD->UEP2_RX_CTRL = USBHS_UEP_R_RES_STALL;
    USBHSD->UEP2_TX_CTRL = USBHS_UEP_T_RES_NAK;

    /* Kích hoạt vật lý: EP1_R và EP2_T */
    USBHSD->ENDP_CONFIG = USBHS_UEP1_R_EN | USBHS_UEP2_T_EN;
}

/*********************************************************************
 * @fn      USBHS_Device_Init
 *case USB_REQ_SET_CONFIGURATION:
 * @brief   Initializes USB high-speed device.
 *
 * @return  none
 */
void USBHS_Device_Init ( FunctionalState sta )
{
    if( sta )
    {
        USBHSD->CONTROL = USBHS_UC_CLR_ALL | USBHS_UC_RESET_SIE;
        Delay_Us(10);
        USBHSD->CONTROL &= ~USBHS_UC_RESET_SIE;
        USBHSD->HOST_CTRL = USBHS_UH_PHY_SUSPENDM;
        USBHSD->CONTROL = USBHS_UC_DMA_EN | USBHS_UC_INT_BUSY | USBHS_UC_SPEED_HIGH;
        USBHSD->INT_EN = USBHS_UIE_SETUP_ACT | USBHS_UIE_TRANSFER | USBHS_UIE_DETECT | USBHS_UIE_SUSPEND;
        USBHS_Device_Endp_Init( );
        USBHSD->CONTROL |= USBHS_UC_DEV_PU_EN;
        NVIC_EnableIRQ( USBHS_IRQn );
    }
    else
    {
        USBHSD->CONTROL = USBHS_UC_CLR_ALL | USBHS_UC_RESET_SIE;
        Delay_Us(10);
        USBHSD->CONTROL = 0;
        NVIC_DisableIRQ( USBHS_IRQn );
    }
}

/*********************************************************************
 * @fn      USBHS_Endp_DataUp
 *
 * @brief   usbhs device data upload
 *          input: endp  - end-point numbers
 *                 *pubf - data buffer
 *                 len   - load data length
 *                 mod   - 0: DEF_UEP_DMA_LOAD 1: DEF_UEP_CPY_LOAD
 *
 * @return  none
 */
uint8_t USBHS_Endp_DataUp( uint8_t endp, uint8_t *pbuf, uint16_t len, uint8_t mod )
{
    uint8_t endp_buf_mode, endp_en, endp_tx_ctrl;

    /* DMA config, endp_ctrl config, endp_len config */
    if( (endp>=DEF_UEP1) && (endp<=DEF_UEP15) )
    {
        endp_en =  USBHSD->ENDP_CONFIG;
        if( endp_en & USBHSD_UEP_TX_EN( endp ) )
        {
            if( (USBHS_Endp_Busy[ endp ] & DEF_UEP_BUSY) == 0x00 )
            {
                endp_buf_mode = USBHSD->BUF_MODE;
                /* if end-point buffer mode is double buffer */
                if( endp_buf_mode & USBHSD_UEP_DOUBLE_BUF( endp ) )
                {
                    /* end-point buffer mode is double buffer */
                    /* only end-point tx enable  */
                    if( (endp_en & USBHSD_UEP_RX_EN( endp )) == 0x00 )
                    {
                        endp_tx_ctrl = USBHSD_UEP_TXCTRL( endp );
                        if( mod == DEF_UEP_DMA_LOAD )
                        {
                            if( (endp_tx_ctrl & USBHS_UEP_T_TOG_DATA1) ==  0 )
                            {
                                /* use UEPn_TX_DMA */
                                USBHSD_UEP_TXDMA( endp ) = (uint32_t)pbuf;
                            }
                            else
                            {
                                /* use UEPn_RX_DMA */
                                USBHSD_UEP_RXDMA( endp ) = (uint32_t)pbuf;
                            }
                        }
                        else if( mod == DEF_UEP_CPY_LOAD )
                        {
                            if( (endp_tx_ctrl & USBHS_UEP_T_TOG_DATA1) ==  0 )
                            {
                                /* use UEPn_TX_DMA */
                                memcpy( USBHSD_UEP_TXBUF(endp), pbuf, len );
                            }
                            else
                            {
                                /* use UEPn_RX_DMA */
                                memcpy( USBHSD_UEP_RXBUF(endp), pbuf, len );
                            }
                        }
                        else
                        {
                            return 1;
                        }
                    }
                    else
                    {
                        return 1;
                    }
                }
                else
                {
                    /* end-point buffer mode is single buffer */
                    if( mod == DEF_UEP_DMA_LOAD )
                    {

                        USBHSD_UEP_TXDMA( endp ) = (uint32_t)pbuf;
                    }
                    else if( mod == DEF_UEP_CPY_LOAD )
                    {
                        memcpy( USBHSD_UEP_TXBUF(endp), pbuf, len );
                    }
                    else
                    {
                        return 1;
                    }
                }

                /* end-point n response tx ack */
                USBHSD_UEP_TLEN( endp ) = len;
                USBHSD_UEP_TXCTRL( endp ) = (USBHSD_UEP_TXCTRL( endp ) &= ~USBHS_UEP_T_RES_MASK) | USBHS_UEP_T_RES_ACK;
                /* Set end-point busy */
                USBHS_Endp_Busy[ endp ] |= DEF_UEP_BUSY;
            }
            else
            {
                return 1;
            }
        }
        else
        {
            return 1;
        }
    }
    else
    {
        return 1;
    }

    return 0;
}

/*********************************************************************
 * @fn      USBHS_IRQHandler
 *
 * @brief   This function handles USBHS exception.
 *
 * @return  none
 */
void USBHS_IRQHandler( void )
{
    uint8_t  intflag, intst, errflag;
    uint16_t len;

    intflag = USBHSD->INT_FG;
    intst = USBHSD->INT_ST;

    if( intflag & USBHS_UIF_TRANSFER )
    {
        switch ( intst & USBHS_UIS_TOKEN_MASK )
        {
            /* 1. Xử lý trạng thái DATA IN (Chip gửi dữ liệu lên PC) */
            case USBHS_UIS_TOKEN_IN:
                switch ( intst & ( USBHS_UIS_TOKEN_MASK | USBHS_UIS_ENDP_MASK ) )
                {
                    case USBHS_UIS_TOKEN_IN | DEF_UEP0:
                        if( USBHS_SetupReqLen == 0 )
                        {
                            USBHSD->UEP0_RX_CTRL = USBHS_UEP_R_TOG_DATA1 | USBHS_UEP_R_RES_ACK;
                        }
                        if ( ( USBHS_SetupReqType & USB_REQ_TYP_MASK ) == USB_REQ_TYP_STANDARD )
                        {
                            switch( USBHS_SetupReqCode )
                            {
                                case USB_GET_DESCRIPTOR:
                                    len = USBHS_SetupReqLen;
                                    if( len >= DEF_USBD_UEP0_SIZE ){
                                        len = DEF_USBD_UEP0_SIZE;
                                    }

                                    /* Chỉ thực hiện copy và dịch con trỏ nếu thực sự còn data */
                                    if( len > 0 ){
                                        memcpy(USBHS_EP0_Buf, pUSBHS_Descr, len);
                                        pUSBHS_Descr += len;
                                        USBHS_SetupReqLen -= len;
                                    }

                                    USBHSD->UEP0_TX_LEN = len;
                                    USBHSD->UEP0_TX_CTRL ^= USBHS_UEP_T_TOG_DATA1;
                                    break;

                                case USB_SET_ADDRESS:
                                    USBHSD->DEV_AD = USBHS_DevAddr;
                                    break;

                                default:
                                    USBHSD->UEP0_TX_LEN = 0;
                                    break;
                            }
                        }
                        break;

                    /* Endpoint 2 IN: Nơi gửi Feedback dữ liệu Audio */
                    case USBHS_UIS_TOKEN_IN | DEF_UEP2: {
                        uint32_t wr_ptr = audio_wr_ptr;
                        uint32_t rd_ptr = audio_rd_ptr;

                        current_avail = (wr_ptr >= rd_ptr) ?
                                         (wr_ptr - rd_ptr) :
                                         (AUDIO_RING_SIZE - rd_ptr + wr_ptr);

                        // 1. Tăng Time Constant của Low-Pass Filter (~32ms)
                        // >> 8 giúp giá trị ổn định hơn, triệt tiêu hoàn toàn rung chấn từ USB
                        int64_t current_fp = (int64_t)current_avail << 16;
                        smoothed_avail_fp = smoothed_avail_fp - (smoothed_avail_fp >> 8) + (current_fp >> 8);

                        smoothed_avail = smoothed_avail_fp >> 16;

                        // Tốt nhất không nên hardcode 2048, phòng trường hợp bạn đổi AUDIO_RING_SIZE
                        int32_t target_level = AUDIO_RING_SIZE / 2;
                        int32_t error = smoothed_avail - target_level;

                        // 2. Bộ Tích Phân (Integral)
                        feedback_integral += error;

                        if(feedback_integral > 1500000) feedback_integral = 1500000; // Siết lại Anti-windup
                        if(feedback_integral < -1500000) feedback_integral = -1500000;

                        // 3. Tuning hệ số P và I
                        // Giảm Kp xuống (dịch trái 1 thay vì 2) để mượt hơn
                        int32_t p_term = error << 1;

                        // Giảm nhẹ Ki để hệ thống không bị over-shoot khi bù trừ
                        int32_t i_term = feedback_integral >> 11;

                        dynamic_fb = audio_feedback_val - (p_term + i_term);

                        // 4. Giữ nguyên kỷ luật thép (Biên độ 5%)
                        uint32_t max_fb = audio_feedback_val + (audio_feedback_val / 20);
                        uint32_t min_fb = audio_feedback_val - (audio_feedback_val / 20);

                        if (dynamic_fb > max_fb) dynamic_fb = max_fb;
                        if (dynamic_fb < min_fb) dynamic_fb = min_fb;

                        USBHS_EP2_Tx_Buf[0] = dynamic_fb & 0xFF;
                        USBHS_EP2_Tx_Buf[1] = (dynamic_fb >> 8) & 0xFF;
                        USBHS_EP2_Tx_Buf[2] = (dynamic_fb >> 16) & 0xFF;
                        USBHS_EP2_Tx_Buf[3] = (dynamic_fb >> 24) & 0xFF;

                        USBHSD->UEP2_TX_LEN = 4;
                        USBHSD->UEP2_TX_CTRL = (USBHSD->UEP2_TX_CTRL & ~USBHS_UEP_T_RES_MASK) | USBHS_UEP_T_RES_ACK;
                        break;
                    }
                    default :
                        // printf("%02X\n", intflag);
                        break;
                }
                break;

            /* 2. Xử lý trạng thái DATA OUT (PC đổ dữ liệu nhạc xuống Chip) */
            case USBHS_UIS_TOKEN_OUT:
                switch( intst & ( USBHS_UIS_TOKEN_MASK | USBHS_UIS_ENDP_MASK ) )
                {
                    case USBHS_UIS_TOKEN_OUT | DEF_UEP0:
                         if ( intst & USBHS_UIS_TOG_OK )
                         {
                             len = USBHSD->RX_LEN; // Lấy số byte nhận được thực tế trong bộ đệm EP0
                              // Kiểm tra điều kiện: Class Request, Hướng OUT (bit7=0), lệnh CUR (0x01), Entity ID = 1 (Clock Source), Selector = 1 (Sampling Freq)
                              if( ((USBHS_SetupReqType & USB_REQ_TYP_MASK) == USB_REQ_TYP_CLASS) &&
                                  ((USBHS_SetupReqType & 0x80) == 0) &&
                                  (USBHS_SetupReqCode == 0x01) &&
                                  ((uint8_t)(USBHS_SetupReqIndex >> 8) == 1) &&
                                  ((uint8_t)(USBHS_SetupReqValue >> 8) == 1) )
                              {
                                  if( len >= 4 ){
                                      // 1. Cập nhật dữ liệu từ bộ đệm USB vào mảng cấu hình tần số toàn cục
                                      current_audio_freq[0] = USBHS_EP0_Buf[0];
                                      current_audio_freq[1] = USBHS_EP0_Buf[1];
                                      current_audio_freq[2] = USBHS_EP0_Buf[2];
                                      current_audio_freq[3] = USBHS_EP0_Buf[3];

                                      // 2. Tính toán tần số mục tiêu từ mảng vừa cập nhật
                                      target_freq = (current_audio_freq[3] << 24) |
                                                             (current_audio_freq[2] << 16) |
                                                             (current_audio_freq[1] << 8)  |
                                                             current_audio_freq[0];

                                        if (target_freq == 44100) {
                                            // Feedback = (43750 / 8000) * 65536 = 358400 = 0x00057800
                                            audio_feedback_val = 0x00057800;
                                        } else if (target_freq == 48000) {
                                            // Feedback = (48076.92 / 8000) * 65536 = 393830 = 0x00060266 -> Làm tròn thành 0x00060266 hoặc 0x00060276
                                            audio_feedback_val = (uint32_t)((47945.205 / 8000.0) * 65536.0);
                                        } else if (target_freq == 96000) {
                                            // Feedback = (96153.85 / 8000) * 65536 = 787692 = 0x000C04EC
                                            audio_feedback_val = 0x000C04EC;
                                        }else if (target_freq == 192000) {
                                            audio_feedback_val = 0x001809D9;
                                        }

                                      smoothed_avail_fp = (AUDIO_RING_SIZE / 2) << 16;
                                      feedback_integral = 0;

                                      // 3. Tái cấu hình bộ chia xung của I2S theo tần số mới ngay lập tức
                                      DMA_Cmd(DMA1_Channel5, DISABLE);
                                      audio_wr_ptr = 0;
                                      audio_rd_ptr = 0;
                                      playback_started = 0;
                                      I2S2_Init(target_freq, current_bit_depth);
                                      DMA_Cmd(DMA1_Channel5, ENABLE);
                                  }
                              }

                              // Cập nhật lại độ dài SetupReqLen còn lại của tiến trình
                              if( USBHS_SetupReqLen >= len ) {
                                  USBHS_SetupReqLen -= len;
                              } else {
                                  USBHS_SetupReqLen = 0;
                              }

                              // Nếu đã nhận đủ dữ liệu, kết thúc Data Phase và chuyển sang Status Phase (Gửi gói ZLP IN để ACK với PC)
                              if( USBHS_SetupReqLen == 0 ){
                                  USBHSD->UEP0_TX_LEN  = 0;
                                  USBHSD->UEP0_TX_CTRL = USBHS_UEP_T_TOG_DATA1 | USBHS_UEP_T_RES_ACK;
                              }else{
                                  // Nếu còn dữ liệu cần nhận tiếp, đảo Toggle bit để chuẩn bị nhận gói sau
                                  USBHSD->UEP0_RX_CTRL ^= USBHS_UEP_R_TOG_DATA1;
                                  USBHSD->UEP0_RX_CTRL = (USBHSD->UEP0_RX_CTRL & ~USBHS_UEP_R_RES_MASK) | USBHS_UEP_R_RES_ACK;
                              }
                         }
                         break;

                    /* Endpoint 1 OUT: Đọc các gói ISOCHRONOUS Audio streaming tại đây */
                    case USBHS_UIS_TOKEN_OUT | DEF_UEP1:
//                        printf("audio at ep1");
                        if ( intst & USBHS_UIS_TOG_OK ){
                            len = USBHSD->RX_LEN; // Số byte nhạc nhận được từ PC trong Microframe này
                            if (len > 0){
                                uint32_t wr_ptr = audio_wr_ptr;
                                uint32_t free_space = (audio_rd_ptr + AUDIO_RING_SIZE - wr_ptr - 1) % AUDIO_RING_SIZE;

                                if (len <= free_space){
                                    // Giai đoạn 1: Đoạn dữ liệu từ vị trí wr_ptr đến cuối mảng Ring Buffer
                                    if (wr_ptr + len <= AUDIO_RING_SIZE){
                                        memcpy(&audio_ring_buf[wr_ptr], USBHS_EP1_Rx_Buf, len);
                                        /* Capture preview of first bytes for debugging and record length */
                                        if (!usb_ep1_preview_ready) {
                                            uint32_t copy_len = (len < sizeof(usb_ep1_preview)) ? len : sizeof(usb_ep1_preview);
                                            memcpy((void*)usb_ep1_preview, USBHS_EP1_Rx_Buf, copy_len);
                                            usb_ep1_preview_ready = 1;
                                            usb_ep1_last_len = len;
                                        }
                                        wr_ptr = (wr_ptr + len) % AUDIO_RING_SIZE;
                                    }else{  // Giai đoạn 2: Nếu bị tràn biên mảng, vòng ngược phần còn lại về đầu mảng
                                        uint32_t first_part = AUDIO_RING_SIZE - wr_ptr;
                                        memcpy(&audio_ring_buf[wr_ptr], USBHS_EP1_Rx_Buf, first_part);
                                        memcpy(&audio_ring_buf[0], &USBHS_EP1_Rx_Buf[first_part], len - first_part);
                                        /* Capture preview when wrapped and record length */
                                        if (!usb_ep1_preview_ready) {
                                            uint32_t copy_len = (first_part < sizeof(usb_ep1_preview)) ? first_part : sizeof(usb_ep1_preview);
                                            memcpy((void*)usb_ep1_preview, USBHS_EP1_Rx_Buf, copy_len);
                                            usb_ep1_preview_ready = 1;
                                            usb_ep1_last_len = len;
                                        }
                                        wr_ptr = len - first_part;
                                    }
                                }
                                audio_wr_ptr = wr_ptr; // Ghi nguyên tử, an toàn tuyệt đối không cần khóa ngắt
                            }

                            // Giữ Endpoint luôn sẵn sàng nhận gói tiếp theo
                            USBHSD->UEP1_RX_CTRL = (USBHSD->UEP1_RX_CTRL & ~USBHS_UEP_R_RES_MASK) | USBHS_UEP_R_RES_ACK;
                        }
                        break;

                    default:
                        break;
                }
                break;

            case USBHS_UIS_TOKEN_SOF:{
                break;
            }

            default :
                break;
        }
        USBHSD->INT_FG = USBHS_UIF_TRANSFER;
    }
    else if( intflag & USBHS_UIF_SETUP_ACT )
    {
        /* 3. Xử lý các gói tin SETUP khởi tạo lớp thiết bị */
        USBHSD->UEP0_TX_CTRL = USBHS_UEP_T_TOG_DATA1 | USBHS_UEP_T_RES_NAK;
        USBHSD->UEP0_RX_CTRL = USBHS_UEP_R_TOG_DATA1 | USBHS_UEP_R_RES_NAK;

        USBHS_SetupReqType  = pUSBHS_SetupReqPak->bRequestType;
        USBHS_SetupReqCode  = pUSBHS_SetupReqPak->bRequest;
        USBHS_SetupReqLen   = pUSBHS_SetupReqPak->wLength;
        USBHS_SetupReqValue = pUSBHS_SetupReqPak->wValue;
        USBHS_SetupReqIndex = pUSBHS_SetupReqPak->wIndex;

        len = 0;
        errflag = 0;

        if ( ( USBHS_SetupReqType & USB_REQ_TYP_MASK ) == USB_REQ_TYP_CLASS ){
            uint8_t control_selector = (uint8_t)(USBHS_SetupReqValue >> 8);
            uint8_t entity_id        = (uint8_t)(USBHS_SetupReqIndex >> 8);

            // 1. XỬ LÝ CHO CLOCK SOURCE (ID = 1)
            if (entity_id == 1){
                switch( USBHS_SetupReqCode ){
                    case 0x01: /* UAC2_CS_CUR_REQ */
                        if( control_selector == 1 ) { /* SAM_FREQ_CONTROL */
                            pUSBHS_Descr = current_audio_freq; // Trỏ tới biến toàn cục vừa tạo
                            len = (USBHS_SetupReqLen > 4) ? 4 : USBHS_SetupReqLen;
                            errflag = 0x00;
                        }
                        /* BỔ SUNG: Trả lời câu hỏi Clock có Valid không */
                        else if( control_selector == 2 ) { /* CLOCK_VALID_CONTROL */
                            static __attribute__((aligned(4))) uint8_t clock_valid = 1; // 1 = Valid/Hợp lệ
                            pUSBHS_Descr = &clock_valid;
                            len = (USBHS_SetupReqLen > 1) ? 1 : USBHS_SetupReqLen;
                            errflag = 0x00;
                        }
                        break;

                    case 0x02: /* UAC2_CS_RANGE_REQ */
                        if( control_selector == 1 ) {
                            pUSBHS_Descr = uac2_range_data;
                            len = (USBHS_SetupReqLen > sizeof(uac2_range_data)) ? sizeof(uac2_range_data) : USBHS_SetupReqLen;
                            errflag = 0x00;
                        }
                        break;
                }
            }else if (entity_id == 4){
                if (USBHS_SetupReqCode == 0x01) {
                    static __attribute__((aligned(4))) uint8_t selector_cur = 1;
                    pUSBHS_Descr = &selector_cur;
                    len = (USBHS_SetupReqLen > 1) ? 1 : USBHS_SetupReqLen;
                    errflag = 0x00;
                }
            }
        }else if( (USBHS_SetupReqType & 0x60) == 0x00 ){
            switch( USBHS_SetupReqCode ){
                case USB_GET_DESCRIPTOR:
                    if( ( USBHS_SetupReqValue >> 8 ) == USB_DESCR_TYP_DEVICE ){
                        pUSBHS_Descr = MyDevDescr;
                        len = DEF_USBD_DEVICE_DESC_LEN;
                    }
                    else if( ( USBHS_SetupReqValue >> 8 ) == USB_DESCR_TYP_CONFIG ){
                        pUSBHS_Descr = MyCfgDescr_HS;
                        len = DEF_USBD_CONFIG_HS_DESC_LEN;
                    }
                    else if( ( USBHS_SetupReqValue >> 8 ) == USB_DESCR_TYP_QUALIF ){
                        pUSBHS_Descr = MyQuaDesc;
                        len = 10;
                    }
                    else if( ( USBHS_SetupReqValue >> 8 ) == USB_DESCR_TYP_SPEED ){
                        pUSBHS_Descr = MyCfgDescr_HS;
                        len = DEF_USBD_CONFIG_HS_DESC_LEN;
                    }
                    else if( ( USBHS_SetupReqValue >> 8 ) == USB_DESCR_TYP_STRING ){
                        switch( (uint8_t)( USBHS_SetupReqValue & 0xFF ) ){
                            case 1:
                                pUSBHS_Descr = MyManuInfo;
                                break;
                            case 2:
                                pUSBHS_Descr = MyProdInfo;
                                break;
                            case 3:
                                pUSBHS_Descr = MySerNumInfo;
                                break;
                            case 0:
                                pUSBHS_Descr = MyLangDescr;
                                break;
                            default:
                                errflag = 0xFF;
                                break;
                        }

                        if (errflag != 0xFF) {
                            len = pUSBHS_Descr[0];
                        }
                    }else{
                        errflag = 0xFF;
                    }
                    if( errflag != 0xFF ) errflag = 0x00;
                    break;

                case USB_SET_ADDRESS:
                    USBHS_DevAddr = (uint8_t)( USBHS_SetupReqValue & 0xFF );
                    errflag = 0x00;
                    break;

                case USB_SET_CONFIGURATION:
                    USBHS_DevConfig = (uint8_t)( USBHS_SetupReqValue & 0xFF );
                    errflag = 0x00;
                    break;

                case USB_GET_CONFIGURATION:
                    USBHS_EP0_Buf[ 0 ] = USBHS_DevConfig;
                    if( USBHS_SetupReqLen > 1 ) USBHS_SetupReqLen = 1;
                    pUSBHS_Descr = USBHS_EP0_Buf;
                    len = USBHS_SetupReqLen;
                    errflag = 0x00;
                    break;

                case USB_CLEAR_FEATURE:
                    if( ( USBHS_SetupReqType & USB_REQ_RECIP_MASK ) == USB_REQ_RECIP_ENDP ){
                        if( (uint8_t)( USBHS_SetupReqValue & 0xFF ) == USB_REQ_FEAT_ENDP_HALT ){
                            if( (uint8_t)( USBHS_SetupReqIndex & 0xFF ) == 0x82 ){ // EP2 IN (Feedback)
                                USBHSD->UEP1_TX_CTRL = USBHS_UEP_T_TOG_DATA0 | USBHS_UEP_T_RES_NAK;
                            }
                            else if( (uint8_t)( USBHS_SetupReqIndex & 0xFF ) == 0x01 ){ // EP1 OUT (Audio Data)
                                USBHSD->UEP1_RX_CTRL = USBHS_UEP_R_TOG_DATA0 | USBHS_UEP_R_RES_ACK;
                            }
                        }
                    }
                    errflag = 0x00;
                    break;

                case USB_SET_INTERFACE:
                    // Kiểm tra xem có phải cấu hình cho Interface 1 (Streaming) không
                    if( (uint8_t)(USBHS_SetupReqIndex & 0xFF) == 0x01 ){
                        current_alt_setting = (uint8_t)(USBHS_SetupReqValue & 0xFF);
                        if (current_alt_setting == 0) {
                            DMA_Cmd(DMA1_Channel5, DISABLE);
                            SPI_I2S_DMACmd(SPI2, SPI_I2S_DMAReq_Tx, DISABLE);
                            I2S_Cmd(SPI2, DISABLE);

                            audio_wr_ptr = 0;
                            audio_rd_ptr = 0;
                            playback_started = 0;

                            memset(audio_ring_buf, 0, AUDIO_RING_SIZE);
                            memset(i2s_tx_dma_buf, 0, I2S_DMA_BUF_SIZE);

                            feedback_integral = 0;

                            errflag = 0x00;
                            break;      
                        } else if (current_alt_setting == 1) {
                            current_bit_depth = I2S_DataFormat_16b;
                            // existing init code
                        } else if (current_alt_setting == 2) {
                            current_bit_depth = I2S_DataFormat_24b;
                            // existing init code
                        }

                        target_freq = (current_audio_freq[3] << 24) |
                                               (current_audio_freq[2] << 16) |
                                               (current_audio_freq[1] << 8)  |
                                               current_audio_freq[0];
                        if (target_freq == 44100) audio_feedback_val = 0x00057800;
                        else if (target_freq == 48000) audio_feedback_val = (uint32_t)((47945.205 / 8000.0) * 65536.0);
                        else if (target_freq == 96000) audio_feedback_val = 0x000C04EC;
                        else if (target_freq == 192000) audio_feedback_val = 0x001809D9;


                        USBHS_EP2_Tx_Buf[0] = audio_feedback_val & 0xFF;
                        USBHS_EP2_Tx_Buf[1] = (audio_feedback_val >> 8) & 0xFF;
                        USBHS_EP2_Tx_Buf[2] = (audio_feedback_val >> 16) & 0xFF;
                        USBHS_EP2_Tx_Buf[3] = (audio_feedback_val >> 24) & 0xFF;
                        USBHSD->UEP2_TX_LEN = 4;
                        USBHSD->UEP2_TX_CTRL = (USBHSD->UEP2_TX_CTRL & ~USBHS_UEP_T_RES_MASK) | USBHS_UEP_T_RES_ACK;


                        DMA_Cmd(DMA1_Channel5, DISABLE);

                        // Xóa sạch con trỏ Ring Buffer và bộ đệm phát để đồng bộ lại từ đầu
                        memset(audio_ring_buf, 0, AUDIO_RING_SIZE);
                        memset(i2s_tx_dma_buf, 0, I2S_DMA_BUF_SIZE);
                        audio_wr_ptr = 0;
                        audio_rd_ptr = 0;
                        playback_started = 0; // Tắt flag phát nhạc để tích lũy buffer lại
                        smoothed_avail_fp = (AUDIO_RING_SIZE / 2) << 16;
                        smoothed_avail = AUDIO_RING_SIZE / 2;
                        feedback_integral = 0;

                        I2S2_Init(target_freq, current_bit_depth);

                        DMA_Cmd(DMA1_Channel5, ENABLE);
                    }
                    errflag = 0x00;
                    break;

                case USB_GET_INTERFACE:
                    USBHS_EP0_Buf[ 0 ] = current_alt_setting;
                    if( USBHS_SetupReqLen > 1 ) USBHS_SetupReqLen = 1;
                    pUSBHS_Descr = USBHS_EP0_Buf;
                    len = USBHS_SetupReqLen;
                    errflag = 0x00;
                    break;

                default:
                    errflag = 0xFF;
                    break;
            }
        }

        if( errflag == 0xFF ){
            USBHSD->UEP0_TX_CTRL = USBHS_UEP_T_TOG_DATA1 | USBHS_UEP_T_RES_STALL;
            USBHSD->UEP0_RX_CTRL = USBHS_UEP_R_TOG_DATA1 | USBHS_UEP_R_RES_STALL;
        }else{
            if( USBHS_SetupReqType & DEF_UEP_IN )
            {
                if( USBHS_SetupReqLen > len ) {
                    USBHS_SetupReqLen = len;
                }
                len = (USBHS_SetupReqLen > DEF_USBD_UEP0_SIZE) ? DEF_USBD_UEP0_SIZE : USBHS_SetupReqLen;
                memcpy( USBHS_EP0_Buf, pUSBHS_Descr, len );
                pUSBHS_Descr += len;
                USBHS_SetupReqLen -= len;

                USBHSD->UEP0_TX_LEN = len;
                USBHSD->UEP0_TX_CTRL = USBHS_UEP_T_TOG_DATA1 | USBHS_UEP_T_RES_ACK;
            }else{
                if( USBHS_SetupReqLen == 0 ){
                    USBHSD->UEP0_TX_LEN = 0;
                    USBHSD->UEP0_TX_CTRL = USBHS_UEP_T_TOG_DATA1 | USBHS_UEP_T_RES_ACK;
                }else{
                    USBHSD->UEP0_RX_CTRL = USBHS_UEP_R_TOG_DATA1 | USBHS_UEP_R_RES_ACK;
                }
            }
        }
        USBHSD->INT_FG = USBHS_UIF_SETUP_ACT;
    }else if( intflag & USBHS_UIF_BUS_RST ){
        USBHS_DevConfig = 0;
        USBHS_DevAddr = 0;
        USBHS_DevSleepStatus = 0;
        USBHS_DevEnumStatus = 0;

        USBHSD->DEV_AD = 0;
        USBHS_Device_Endp_Init( );
        USBHSD->INT_FG = USBHS_UIF_BUS_RST;
    }else if( intflag & USBHS_UIF_SUSPEND ){
        USBHSD->INT_FG = USBHS_UIF_SUSPEND;
    }else{
        USBHSD->INT_FG = intflag;
    }
}

/*********************************************************************
 * @fn      USBHS_Send_Resume
 *
 * @brief   USBHS device sends wake-up signal to host
 *
 * @return  none
 */
void USBHS_Send_Resume(void)
{

}
