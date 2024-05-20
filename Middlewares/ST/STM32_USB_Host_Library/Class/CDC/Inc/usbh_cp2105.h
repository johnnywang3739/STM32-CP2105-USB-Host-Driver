/*
 * usbh_cp2105.h
 *
 *  Created on: May 14, 2024
 *      Author: JohnnyWang
 */

#ifndef ST_STM32_USB_HOST_LIBRARY_CLASS_CDC_INC_USBH_CP2105_H_
#define ST_STM32_USB_HOST_LIBRARY_CLASS_CDC_INC_USBH_CP2105_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "usbh_core.h"

#define USB_CP2105_CLASS                     0xFFU  // Vendor Specific
#define CP2105_INTERFACE_CLASS_CODE          0xFFU
#define CP2105_INTERFACE_SUBCLASS_CODE       0x00U
#define CP2105_INTERFACE_PROTOCOL_CODE       0x00U

/* Class-Specific Request Codes */
#define CP210x_SET_LINE_CODING               0x03U
#define CP210x_GET_LINE_CODING               0x04U
#define CP2105_SET_CONTROL_LINE_STATE        0x22U

/* Port Indexes */
#define CP2105_STD_PORT                      1U
#define CP2105_ENH_PORT                      0U

/* States */
#define CP2105_IDLE_STATE                    0U
#define CP2105_SET_LINE_CODING_STATE         1U
#define CP2105_GET_LAST_LINE_CODING_STATE    2U
#define CP2105_TRANSFER_DATA                 3U
#define CP2105_ERROR_STATE                   4U
#define CP2105_SEND_DATA                     5U
#define CP2105_SEND_DATA_WAIT                6U
#define CP2105_RECEIVE_DATA                  7U
#define CP2105_RECEIVE_DATA_WAIT             8U

typedef struct {
  uint32_t dwDTERate;    // Data terminal rate in bits per second
  uint8_t  bCharFormat;  // Stop bits: 0 = 1 Stop bit, 1 = 1.5 Stop bits, 2 = 2 Stop bits
  uint8_t  bParityType;  // Parity: 0 = None, 1 = Odd, 2 = Even, 3 = Mark, 4 = Space
  uint8_t  bDataBits;    // Data bits (5, 6, 7, 8, or 16)
} CP2105_LineCodingTypeDef;

typedef struct {
  uint8_t NotifEp;
  uint16_t NotifEpSize;
  uint8_t InEp;
  uint16_t InEpSize;
  uint8_t OutEp;
  uint16_t OutEpSize;
  uint8_t NotifPipe;
  uint8_t InPipe;
  uint8_t OutPipe;
} CP2105_PortTypeDef;

typedef struct {
  uint8_t state;
  uint8_t data_tx_state;
  uint8_t data_rx_state;
  CP2105_LineCodingTypeDef LineCoding;
  CP2105_LineCodingTypeDef *pUserLineCoding;
  uint8_t *pTxData;
  uint8_t *pRxData;
  uint32_t TxDataLength;
  uint32_t RxDataLength;
  CP2105_PortTypeDef Port[2];
} CP2105_HandleTypeDef;

extern USBH_ClassTypeDef CP2105_ClassDriver;


#ifdef __cplusplus
}
#endif



#endif /* ST_STM32_USB_HOST_LIBRARY_CLASS_CDC_INC_USBH_CP2105_H_ */
