/********************************** (C) COPYRIGHT *******************************
 * File Name          : usb_desc.h
 * Author             : WCH
 * Version            : V1.0.0
 * Date               : 2026/07/01
 * Description        : Header file cho UAC2 High-Speed Audio DAC
*********************************************************************************/
#ifndef USER_USB_DESC_H_
#define USER_USB_DESC_H_

#include "debug.h"

/* Định nghĩa thông tin thiết bị */
#define DEF_FILE_VERSION             0x02
#define DEF_USB_VID                  0x1A86
#define DEF_USB_PID                  0xFE0E   /* Đổi sang PID khác CDC để tránh trùng cache driver cũ của Windows */
#define DEF_IC_PRG_VER               DEF_FILE_VERSION

/* Định nghĩa kích thước Endpoint tối đa */
#define DEF_USBD_UEP0_SIZE           64     /* Endpoint 0 Control */
#define DEF_USBD_HS_PACK_SIZE        512
#define DEF_USBD_HS_ISO_PACK_SIZE    1024   /* Max packet cho Isochronous Audio OUT */

/* Chiều dài cố định của các mảng Descriptor */
#define DEF_USBD_DEVICE_DESC_LEN     18
#define DEF_USBD_CONFIG_FS_DESC_LEN  0      /* Chúng ta bỏ hẳn Full-Speed */
#define DEF_USBD_CONFIG_HS_DESC_LEN  195
#define DEF_USBD_REPORT_DESC_LEN     0
#define DEF_USBD_LANG_DESC_LEN       4
#define DEF_USBD_MANU_DESC_LEN       14     /* "wch.cn" */
#define DEF_USBD_PROD_DESC_LEN       28     /* "CH32V307 UAC2" */
#define DEF_USBD_SN_DESC_LEN         18     /* "0123456" */
#define DEF_USBD_QUALFY_DESC_LEN     10
#define DEF_USBD_BOS_DESC_LEN        0

/* Khai báo các mảng descriptor để file ch32v30x_usbhs_device.c gọi */
extern const uint8_t MyDevDescr[];
extern const uint8_t MyCfgDescr_HS[];
extern const uint8_t MyLangDescr[];
extern const uint8_t MyManuInfo[];
extern const uint8_t MyProdInfo[];
extern const uint8_t MySerNumInfo[];
extern const uint8_t MyQuaDesc[];

#endif /* USER_USB_DESC_H_ */
