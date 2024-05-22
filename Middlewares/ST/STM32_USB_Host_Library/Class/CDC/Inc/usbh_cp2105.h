#ifndef USBH_CP2105_H
#define USBH_CP2105_H

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
#define CP210x_GET_BAUDRATE                  0x1DU
#define CP210x_SET_BAUDRATE                  0x1EU

#define CP2105_SET_CONTROL_LINE_STATE        0x22U

/* Port Indexes */
#define CP2105_STD_PORT                      1U
#define CP2105_ENH_PORT                      0U

/* States */
typedef enum {
    CP2105_IDLE_STATE = 0,
    CP2105_SET_LINE_CODING_STATE,
    CP2105_GET_LAST_LINE_CODING_STATE,
    CP2105_TRANSFER_DATA,
    CP2105_ERROR_STATE,
    CP2105_SEND_DATA,
    CP2105_SEND_DATA_WAIT,
    CP2105_RECEIVE_DATA,
    CP2105_RECEIVE_DATA_WAIT,
    CP2105_GET_LINE_CODING_STATE_PORT1,
    CP2105_GET_LINE_CODING_STATE_PORT2,
    CP2105_GET_BAUD_RATE_STATE_PORT1,
    CP2105_GET_BAUD_RATE_STATE_PORT2,
    CP2105_ENABLE_INTERFACE_STATE_PORT1,
    CP2105_ENABLE_INTERFACE_STATE_PORT2,
    CP2105_SET_BAUDRATE,
    CP2105_TRANSMIT_DATA,
    CP2105_PROCESS_DATA,
    CP2105_ERROR
} CP2105_StateTypeDef;

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
  uint8_t data_tx_state;
  uint8_t data_rx_state;
} CP2105_PortTypeDef;

typedef struct {
  uint8_t state;
  CP2105_LineCodingTypeDef LineCoding[2];
  uint32_t BaudRate[2];  
  CP2105_LineCodingTypeDef *pUserLineCoding;
  uint8_t *pTxData[2];
  uint8_t *pRxData[2];
  uint32_t TxDataLength[2];
  uint32_t RxDataLength[2];
  CP2105_PortTypeDef Port[2];
} CP2105_HandleTypeDef;

extern USBH_ClassTypeDef CP2105_ClassDriver;

USBH_StatusTypeDef USBH_CP2105_SetBaudRate(USBH_HandleTypeDef *phost, uint32_t baudRate, uint8_t port);
USBH_StatusTypeDef USBH_CP2105_GetBaudRate(USBH_HandleTypeDef *phost, uint32_t *baudRate, uint8_t port);
USBH_StatusTypeDef USBH_CP2105_SetLineCoding(USBH_HandleTypeDef *phost, CP2105_LineCodingTypeDef *linecoding, uint8_t port);
USBH_StatusTypeDef USBH_CP2105_GetLineCoding(USBH_HandleTypeDef *phost, CP2105_LineCodingTypeDef *linecoding, uint8_t port);
USBH_StatusTypeDef USBH_CP2105_Transmit(USBH_HandleTypeDef *phost, uint8_t *pbuff, uint32_t length, uint8_t port);
USBH_StatusTypeDef USBH_CP2105_Receive(USBH_HandleTypeDef *phost, uint8_t *pbuff, uint32_t length, uint8_t port);
USBH_StatusTypeDef USBH_CP2105_SetBaudRate(USBH_HandleTypeDef *phost, uint32_t baudRate, uint8_t port);
void USBH_CP2105_TransmitCallback(USBH_HandleTypeDef *phost, uint8_t port);
void USBH_CP2105_ReceiveCallback(USBH_HandleTypeDef *phost, uint8_t port);
void USBH_CP2105_LineCodingChanged(USBH_HandleTypeDef *phost, uint8_t port);



#ifdef __cplusplus
}
#endif

#endif /* USBH_CP2105_H */
