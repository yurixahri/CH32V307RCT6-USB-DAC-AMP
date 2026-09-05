/********************************** (C) COPYRIGHT *******************************
 * File Name          : usb_desc.c
 * Author             : WCH
 * Version            : V1.0.0
 * Date               : 2026/07/01
 * Description        : Định nghĩa toàn bộ mảng Descriptor cho UAC2 Audio
*********************************************************************************/
#include "usb_desc.h"

/* 1. Device Descriptor cho UAC2 Device */
__attribute__ ((aligned(4))) const uint8_t MyDevDescr[] =
{
    0x12,       // bLength (18 bytes)
    0x01,       // bDescriptorType (Device)
    0x00, 0x02, // bcdUSB (2.00 - Bắt buộc cho High-Speed UAC2)
    0xEF,       // bDeviceClass (Miscellaneous Device Class)
    0x02,       // bDeviceSubClass (Common Class)
    0x01,       // bDeviceProtocol (Interface Association Descriptor)
    DEF_USBD_UEP0_SIZE, // bMaxPacketSize0 (64)
    (uint8_t)DEF_USB_VID, (uint8_t)(DEF_USB_VID >> 8),  // idVendor
    (uint8_t)DEF_USB_PID, (uint8_t)(DEF_USB_PID >> 8),  // idProduct
    DEF_IC_PRG_VER, 0x00, // bcdDevice
    0x01,       // iManufacturer (Chuỗi tên nhà sản xuất)
    0x02,       // iProduct (Chuỗi tên thiết bị)
    0x03,       // iSerialNumber
    0x01        // bNumConfigurations (1 cấu hình)
};

/* Định nghĩa mã ID đặc trưng của UAC2 */
#define UAC2_CLOCK_SOURCE_ID                  0x01
#define UAC2_INPUT_TERMINAL_ID                0x02
#define UAC2_FEATURE_UNIT_ID                  0x03
#define UAC2_OUTPUT_TERMINAL_ID               0x04

/* 2. Configuration Descriptor cho chế độ USB High-Speed (169 Bytes) */
/* Configuration Descriptor cho UAC2 High-Speed */
__attribute__ ((aligned(4))) const uint8_t MyCfgDescr_HS[] =
{
    /* 1. Configuration Descriptor */
    0x09,       // bLength
    0x02,       // bDescriptorType (CONFIGURATION)
    0xC3, 0x00, // wTotalLength = 195 bytes (0x00C3)
    0x02,       // bNumInterfaces
    0x01,       // bConfigurationValue
    0x00,       // iConfiguration
    0x80,       // bmAttributes
    0xFA,       // bMaxPower (500mA)

    /* 2. Interface Association Descriptor (IAD) */
    0x08,       // bLength
    0x0B,       // bDescriptorType (IAD)
    0x00,       // bFirstInterface
    0x02,       // bInterfaceCount
    0x01,       // bFunctionClass (AUDIO)
    0x00,       // bFunctionSubClass
    0x20,       // bFunctionProtocol (IP_VERSION_02_00)
    0x00,       // iFunction

    /* 3. Interface 0 - Audio Control (AC) Descriptor */
    0x09,       // bLength
    0x04,       // bDescriptorType (INTERFACE)
    0x00,       // bInterfaceNumber (0)
    0x00,       // bAlternateSetting (0)
    0x00,       // bNumEndpoints (0)
    0x01,       // bInterfaceClass (AUDIO)
    0x01,       // bInterfaceSubClass (AUDIO_CONTROL)
    0x20,       // bInterfaceProtocol (IP_VERSION_02_00)
    0x00,       // iInterface

    /* 3.1 Class-Specific AC Interface Header Descriptor */
    0x09,       // bLength
    0x24,       // bDescriptorType (CS_INTERFACE)
    0x01,       // bDescriptorSubtype (HEADER)
    0x00, 0x02, // bcdADC (2.00)
    0x01,       // bCategory (Desktop Speaker)
    0x36, 0x00, // wTotalLength (Tăng lên 36 bytes cho cụm AC vì thêm Selector)
    0x00,       // bmControls

    /* 3.2 Clock Source Descriptor (ID = 1) */
    0x08,       // bLength
    0x24,       // bDescriptorType (CS_INTERFACE)
    0x0A,       // bDescriptorSubtype (CLOCK_SOURCE)
    0x01,       // bClockID (1)
    0x03,       // bmAttributes (Internal Programmable Clock)
    0x07,       // bmControls (Clock Freq Control = Host programmable)
    0x00,       // bAssocTerminal
    0x00,       // iClockSource

    /* 3.3 BỔ SUNG: Clock Selector Descriptor (ID = 4) */
    0x08,       // bLength
    0x24,       // bDescriptorType (CS_INTERFACE)
    0x0B,       // bDescriptorSubtype (CLOCK_SELECTOR)
    0x04,       // bClockID (Gán ID mới cho Selector là 4)
    0x01,       // bNrInPins (Chỉ có 1 đầu vào clock lấy từ Clock Source ID 1)
    0x01,       // baCSourceID(1) (Kết nối đầu vào từ Clock Source ID = 1)
    0x03,       // bmControls (Host có quyền điều khiển)
    0x00,       // iClockSelector

    /* 3.4 Input Terminal Descriptor (ID = 2) */
    0x11,       // bLength
    0x24,       // bDescriptorType (CS_INTERFACE)
    0x02,       // bDescriptorSubtype (INPUT_TERMINAL)
    0x02,       // bTerminalID (2)
    0x01, 0x01, // wTerminalType (USB Streaming)
    0x00,       // bAssocTerminal
    0x04,       // bCSourceID (ĐỔI TỪ 1 THÀNH 4: Giờ phải ăn theo nguồn Clock xuất ra từ Selector ID 4)
    0x02,       // bNrChannels (2 Kênh)
    0x03, 0x00, 0x00, 0x00, // bmChannelConfig
    0x00,       // iChannelNames
    0x00, 0x00, // bmControls
    0x00,       // iTerminal

    /* 3.5 Output Terminal Descriptor (ID = 3) */
    0x0C,       // bLength
    0x24,       // bDescriptorType (CS_INTERFACE)
    0x03,       // bDescriptorSubtype (OUTPUT_TERMINAL)
    0x03,       // bTerminalID (3)
    0x01, 0x03, // wTerminalType (Speaker)
    0x00,       // bAssocTerminal
    0x02,       // bSourceID (Kết nối từ Input Terminal ID = 2)
    0x04,       // bCSourceID (ĐỔI TỪ 1 THÀNH 4: Ăn theo nguồn Clock xuất ra từ Selector ID 4)
    0x00, 0x00, // bmControls
    0x00,       // iTerminal

    /* ==================================================================== */
    /* 4. Interface 1 - Audio Streaming (AS) - ALTERNATE SETTING 0 (Zero BW)*/
    0x09,       // bLength
    0x04,       // bDescriptorType (INTERFACE)
    0x01,       // bInterfaceNumber (1)
    0x00,       // bAlternateSetting (0)
    0x00,       // bNumEndpoints (0)
    0x01,       // bInterfaceClass (AUDIO)
    0x02,       // bInterfaceSubClass (AUDIO_STREAMING)
    0x20,       // bInterfaceProtocol (IP_VERSION_02_00)
    0x00,       // iInterface

    /* ==================================================================== */
    /* 5. Interface 1 - Audio Streaming (AS) - ALTERNATE SETTING 1 (16-BIT) */
    0x09,       // bLength
    0x04,       // bDescriptorType (INTERFACE)
    0x01,       // bInterfaceNumber (1)
    0x01,       // bAlternateSetting (1)
    0x02,       // bNumEndpoints (2)
    0x01,       // bInterfaceClass (AUDIO)
    0x02,       // bInterfaceSubClass (AUDIO_STREAMING)
    0x20,       // bInterfaceProtocol (IP_VERSION_02_00)
    0x00,       // iInterface

    /* 5.1 Class-Specific AS General Descriptor */
    0x10,       // bLength
    0x24,       // bDescriptorType (CS_INTERFACE)
    0x01,       // bDescriptorSubtype (AS_GENERAL)
    0x02,       // bTerminalLink (Input Terminal ID = 2)
    0x00,       // bmControls
    0x01,       // bFormatType (FORMAT_TYPE_I)
    0x01, 0x00, 0x00, 0x00, // bmFormats (PCM)
    0x02,       // bNrChannels (2)
    0x03, 0x00, 0x00, 0x00, // bmChannelConfig
    0x00,       // iChannelNames

    /* 5.2 Class-Specific AS Format Type Descriptor (16-bit) */
    0x06,       // bLength
    0x24,       // bDescriptorType (CS_INTERFACE)
    0x02,       // bDescriptorSubtype (FORMAT_TYPE)
    0x01,       // bFormatType (FORMAT_TYPE_I)
    0x02,       // bSubcarrierSize (2 Bytes mỗi sample)
    0x10,       // bBitResolution (16 Bits)

    /* 5.3 Endpoint 1 OUT Descriptor (Audio Data) */
    0x07,       // bLength
    0x05,       // bDescriptorType (ENDPOINT)
    0x01,       // bEndpointAddress (EP1 OUT)
    0x05,       // bmAttributes (Isochronous, Asynchronous)
    (uint8_t)DEF_USBD_HS_ISO_PACK_SIZE, (uint8_t)(DEF_USBD_HS_ISO_PACK_SIZE >> 8), // wMaxPacketSize (1024)
    0x01,       // bInterval (1 Microframe = 125us)

    /* 5.4 Class-Specific AS Isochronous Audio Data Endpoint Descriptor */
    0x08,       // bLength
    0x25,       // bDescriptorType (CS_ENDPOINT)
    0x01,       // bDescriptorSubtype (EP_GENERAL)
    0x00,       // bmAttributes
    0x00,       // bmControls
    0x00,       // bLockDelayUnits
    0x00, 0x00, // wLockDelay

    /* 5.5 Endpoint 2 IN Descriptor (Feedback) */
    0x07,       // bLength
    0x05,       // bDescriptorType (ENDPOINT)
    0x82,       // bEndpointAddress (EP2 IN)
    0x11,       // bmAttributes (Isochronous, Feedback Endpoint)
    0x04, 0x00, // wMaxPacketSize (4 Bytes)
    0x01,       // bInterval (1 Microframe = 125us)

    /* ==================================================================== */
    /* 6. Interface 1 - Audio Streaming (AS) - ALTERNATE SETTING 2 (24-BIT) */
    0x09,       // bLength
    0x04,       // bDescriptorType (INTERFACE)
    0x01,       // bInterfaceNumber (1)
    0x02,       // bAlternateSetting (2) -> Profile thứ 2
    0x02,       // bNumEndpoints (2)
    0x01,       // bInterfaceClass (AUDIO)
    0x02,       // bInterfaceSubClass (AUDIO_STREAMING)
    0x20,       // bInterfaceProtocol (IP_VERSION_02_00)
    0x00,       // iInterface

    /* 6.1 Class-Specific AS General Descriptor */
    0x10,       // bLength
    0x24,       // bDescriptorType (CS_INTERFACE)
    0x01,       // bDescriptorSubtype (AS_GENERAL)
    0x02,       // bTerminalLink (Input Terminal ID = 2)
    0x00,       // bmControls
    0x01,       // bFormatType (FORMAT_TYPE_I)
    0x01, 0x00, 0x00, 0x00, // bmFormats (PCM)
    0x02,       // bNrChannels (2)
    0x03, 0x00, 0x00, 0x00, // bmChannelConfig
    0x00,       // iChannelNames

    /* 6.2 Class-Specific AS Format Type Descriptor (24-bit) */
    0x06,       // bLength
    0x24,       // bDescriptorType (CS_INTERFACE)
    0x02,       // bDescriptorSubtype (FORMAT_TYPE)
    0x01,       // bFormatType (FORMAT_TYPE_I)
    0x04,       // bSubcarrierSize (4 Bytes mỗi sample cho UAC2 24-bit)
    0x18,       // bBitResolution (24 Bits)

    /* 6.3 Endpoint 1 OUT Descriptor (Audio Data) */
    0x07,       // bLength
    0x05,       // bDescriptorType (ENDPOINT)
    0x01,       // bEndpointAddress (EP1 OUT)
    0x05,       // bmAttributes (Isochronous, Asynchronous)
    (uint8_t)DEF_USBD_HS_ISO_PACK_SIZE, (uint8_t)(DEF_USBD_HS_ISO_PACK_SIZE >> 8), // wMaxPacketSize (1024)
    0x01,       // bInterval (1 Microframe = 125us)

    /* 6.4 Class-Specific AS Isochronous Audio Data Endpoint Descriptor */
    0x08,       // bLength
    0x25,       // bDescriptorType (CS_ENDPOINT)
    0x01,       // bDescriptorSubtype (EP_GENERAL)
    0x00,       // bmAttributes
    0x00,       // bmControls
    0x00,       // bLockDelayUnits
    0x00, 0x00, // wLockDelay

    /* 6.5 Endpoint 2 IN Descriptor (Feedback) */
    0x07,       // bLength
    0x05,       // bDescriptorType (ENDPOINT)
    0x82,       // bEndpointAddress (EP2 IN)
    0x11,       // bmAttributes (Isochronous, Feedback Endpoint)
    0x04, 0x00, // wMaxPacketSize (4 Bytes)
    0x01,       // bInterval (1 Microframe = 125us)
};

/* 3. Device Qualifier Descriptor */
const uint8_t MyQuaDesc[] =
{
    0x0A,       // bLength
    0x06,       // bDescriptorType (Device Qualifier)
    0x00, 0x02, // bcdUSB (2.00)
    0xEF,       // bDeviceClass
    0x02,       // bDeviceSubClass
    0x01,       // bDeviceProtocol
    0x40,       // bMaxPacketSize0 (64)
    0x01,       // bNumConfigurations
    0x00        // Reserved
};

/* 4. Language Descriptor (Mã ngôn ngữ: English US) */
const uint8_t MyLangDescr[] =
{
    0x04, 0x03, 0x09, 0x04
};

/* 5. Manufacturer Descriptor (Tên nhà sản xuất dạng Unicode) */
const uint8_t MyManuInfo[] =
{
    0x0E, 0x03,
    'w', 0, 'c', 0, 'h', 0, '.', 0, 'c', 0, 'n', 0
};

/* 6. Product Information (Tên thiết bị sẽ hiện trên Windows) */
const uint8_t MyProdInfo[] = {
    28,         // bLength (13 ký tự * 2 + 2 = 28 bytes)
    0x03,       // bDescriptorType (STRING)
    'C', 0, 'H', 0, '3', 0, '2', 0, 'V', 0, '3', 0, '0', 0, '7', 0,
    ' ', 0, 'U', 0, 'A', 0, 'C', 0, '2', 0
};

/* 7. Serial Number Information */
const uint8_t MySerNumInfo[] =
{
    18, 0x03,   // bLength (8 ký tự * 2 + 2 = 18 bytes)
    '0', 0, '1', 0, '2', 0, '3', 0, '4', 0, '5', 0, '6', 0, '7', 0
};
