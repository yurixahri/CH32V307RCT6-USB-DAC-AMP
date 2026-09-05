/********************************** (C) COPYRIGHT *******************************
* File Name          : main.c
* Author             : WCH
* Version            : V1.0.0
* Date               : 2021/06/06
* Description        : Main program body.
*********************************************************************************
* Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for 
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/

/*
 *@Note
 USART Print debugging routine:
 USART1_Tx(PA9).
 This example demonstrates using USART1(PA9) as a print debug port output.

*/

#include "UART/UART.h"
#include "debug.h"
#include "i2s.h"
#include  "USB_Device/ch32v30x_usbhs_device.h"


/* Global typedef */

/* Global define */

/* Global Variable */


/*********************************************************************
 * @fn      main
 *
 * @brief   Main program.
 *
 * @return  none
 */
int main(void){

    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    SystemCoreClockUpdate();
    Delay_Init( );
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    // USART_Printf_Init( 115200 );

//  printf( "SystemClk:%d\r\n", SystemCoreClock );
//  printf( "ChipID:%08x\r\n", DBGMCU_GetCHIPID() );
    // printf( "Simulate USB-CDC Device running on USBHS Controller\r\n" );

    // RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
    // RCC_Configuration( );

    /* Tim2 init */
    // TIM2_Init( );

    /* Usart1 init */
    // UART2_Init( 1, DEF_UARTx_BAUDRATE, DEF_UARTx_STOPBIT, DEF_UARTx_PARITY );

    /* USB20 device init */
    USBHS_RCC_Init( );
    USBHS_Device_Init( ENABLE );

//  uint32_t count = 0;
    // uint32_t sample_idx = 0;
    while(1){
//       if(count >= 45000000){
//            // Format dạng CSV: Sample, Raw_Avail, Smooth_Avail, Integral, Dynamic_FB
//          sample_idx++;
        //    printf("%lu,%lu,%ld,%ld,%lu\r\n",
        //            (uint32_t)sample_idx,
        //            (uint32_t)current_avail,
        //            (int32_t)smoothed_avail,
        //            (int32_t)feedback_integral,
        //            (uint32_t)dynamic_fb);

//            printf("%u, %u, %u, %08lx, %u\r\n",
//                   audio_wr_ptr,
//                   audio_rd_ptr,
//                   available,
//                   dynamic_fb);
//            printf("FB: %02X %02X %02X %02X\r\n", USBHS_EP2_Tx_Buf[0], USBHS_EP2_Tx_Buf[1], USBHS_EP2_Tx_Buf[2], USBHS_EP2_Tx_Buf[3]);
//            count = 0;
//       }else {
//            ++count;
//        }
//  if (dma_preview_ready) {
//      for (int i=0;i<16;i++) printf("%02X ", dma_preview[i]);
//      printf("\r\n");
//      dma_preview_ready = 0;
//  }
//
//  if (usb_ep1_preview_ready) {
//      printf("EP1_IN(len=%u): ", (unsigned)usb_ep1_last_len);
//      for (int i=0;i<16;i++) printf("%02X ", usb_ep1_preview[i]);
//      printf("\r\n");
//      usb_ep1_preview_ready = 0;
//      usb_ep1_last_len = 0;
//  }
//      UART2_DataRx_Deal( );
//      UART2_DataTx_Deal( );
    }
}
