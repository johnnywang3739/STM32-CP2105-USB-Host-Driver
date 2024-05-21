#include "usbh_cp2105.h"

static USBH_StatusTypeDef USBH_CP2105_InterfaceInit(USBH_HandleTypeDef *phost);
static USBH_StatusTypeDef USBH_CP2105_InterfaceDeInit(USBH_HandleTypeDef *phost);
static USBH_StatusTypeDef USBH_CP2105_ClassRequest(USBH_HandleTypeDef *phost);
static USBH_StatusTypeDef USBH_CP2105_Process(USBH_HandleTypeDef *phost);
static USBH_StatusTypeDef USBH_CP2105_SOFProcess(USBH_HandleTypeDef *phost);

static USBH_StatusTypeDef GetLineCoding(USBH_HandleTypeDef *phost, CP2105_LineCodingTypeDef *linecoding, uint8_t port);
static USBH_StatusTypeDef SetLineCoding(USBH_HandleTypeDef *phost, CP2105_LineCodingTypeDef *linecoding, uint8_t port);
static USBH_StatusTypeDef SetBaudRate(USBH_HandleTypeDef *phost, uint32_t baudRate, uint8_t port);
static USBH_StatusTypeDef GetBaudRate(USBH_HandleTypeDef *phost, uint32_t *baudRate, uint8_t port);
static void CP2105_ProcessTransmission(USBH_HandleTypeDef *phost);
static void CP2105_ProcessReception(USBH_HandleTypeDef *phost);

USBH_ClassTypeDef CP2105_ClassDriver = {
  "CP2105",
  USB_CP2105_CLASS,
  USBH_CP2105_InterfaceInit,
  USBH_CP2105_InterfaceDeInit,
  USBH_CP2105_ClassRequest,
  USBH_CP2105_Process,
  USBH_CP2105_SOFProcess,
  NULL,
};
static USBH_StatusTypeDef USBH_CP2105_InterfaceInit(USBH_HandleTypeDef *phost) {
    USBH_StatusTypeDef status;
    uint8_t interface;
    CP2105_HandleTypeDef *CP2105_Handle;

    // Find the interface for CP2105
    interface = USBH_FindInterface(phost, CP2105_INTERFACE_CLASS_CODE, CP2105_INTERFACE_SUBCLASS_CODE, CP2105_INTERFACE_PROTOCOL_CODE);

    if ((interface == 0xFFU) || (interface >= USBH_MAX_NUM_INTERFACES)) {
        USBH_DbgLog("Cannot Find the interface for CP2105 Interface Class.", phost->pActiveClass->Name);
        return USBH_FAIL;
    }

    // Select the interface
    status = USBH_SelectInterface(phost, interface);
    if (status != USBH_OK) {
        return USBH_FAIL;
    }

    // Allocate memory for CP2105 Handle
    phost->pActiveClass->pData = (CP2105_HandleTypeDef *)USBH_malloc(sizeof(CP2105_HandleTypeDef));
    CP2105_Handle = (CP2105_HandleTypeDef *)phost->pActiveClass->pData;

    if (CP2105_Handle == NULL) {
        USBH_DbgLog("Cannot allocate memory for CP2105 Handle");
        return USBH_FAIL;
    }

    (void)USBH_memset(CP2105_Handle, 0, sizeof(CP2105_HandleTypeDef));

    // Iterate through the endpoint descriptors and assign the sizes correctly
    for (uint8_t i = 0; i < 2; i++) {
        USBH_EpDescTypeDef *ep_desc = &phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[i];

        if ((ep_desc->bEndpointAddress & 0x80U) != 0U) {
            // IN endpoint
            CP2105_Handle->Port[i].InEp = ep_desc->bEndpointAddress;
            CP2105_Handle->Port[i].InEpSize = ep_desc->wMaxPacketSize;
            if (i == CP2105_STD_PORT) {
                CP2105_Handle->Port[i].InEpSize = 32; // Correct for Standard Port
            } else {
                CP2105_Handle->Port[i].InEpSize = 64; // Correct for Enhanced Port
            }
            CP2105_Handle->Port[i].InPipe = USBH_AllocPipe(phost, ep_desc->bEndpointAddress);
            (void)USBH_OpenPipe(phost, CP2105_Handle->Port[i].InPipe, CP2105_Handle->Port[i].InEp, phost->device.address, phost->device.speed, USB_EP_TYPE_BULK, CP2105_Handle->Port[i].InEpSize);
        } else {
            // OUT endpoint
            CP2105_Handle->Port[i].OutEp = ep_desc->bEndpointAddress;
            CP2105_Handle->Port[i].OutEpSize = ep_desc->wMaxPacketSize;
            if (i == CP2105_STD_PORT) {
                CP2105_Handle->Port[i].OutEpSize = 32; // Correct for Standard Port
            } else {
                CP2105_Handle->Port[i].OutEpSize = 64; // Correct for Enhanced Port
            }
            CP2105_Handle->Port[i].OutPipe = USBH_AllocPipe(phost, ep_desc->bEndpointAddress);
            (void)USBH_OpenPipe(phost, CP2105_Handle->Port[i].OutPipe, CP2105_Handle->Port[i].OutEp, phost->device.address, phost->device.speed, USB_EP_TYPE_BULK, CP2105_Handle->Port[i].OutEpSize);
        }
    }

    CP2105_Handle->state = CP2105_IDLE_STATE;

    return USBH_OK;
}




static USBH_StatusTypeDef USBH_CP2105_InterfaceDeInit(USBH_HandleTypeDef *phost) {
  CP2105_HandleTypeDef *CP2105_Handle = (CP2105_HandleTypeDef *)phost->pActiveClass->pData;

  for (uint8_t i = 0; i < 2; i++) {
    if (CP2105_Handle->Port[i].NotifPipe != 0U) {
      (void)USBH_ClosePipe(phost, CP2105_Handle->Port[i].NotifPipe);
      (void)USBH_FreePipe(phost, CP2105_Handle->Port[i].NotifPipe);
      CP2105_Handle->Port[i].NotifPipe = 0U;
    }

    if (CP2105_Handle->Port[i].InPipe != 0U) {
      (void)USBH_ClosePipe(phost, CP2105_Handle->Port[i].InPipe);
      (void)USBH_FreePipe(phost, CP2105_Handle->Port[i].InPipe);
      CP2105_Handle->Port[i].InPipe = 0U;
    }

    if (CP2105_Handle->Port[i].OutPipe != 0U) {
      (void)USBH_ClosePipe(phost, CP2105_Handle->Port[i].OutPipe);
      (void)USBH_FreePipe(phost, CP2105_Handle->Port[i].OutPipe);
      CP2105_Handle->Port[i].OutPipe = 0U;
    }
  }

  if (phost->pActiveClass->pData != NULL) {
    USBH_free(phost->pActiveClass->pData);
    phost->pActiveClass->pData = NULL;
  }

  return USBH_OK;
}

static USBH_StatusTypeDef USBH_CP2105_ClassRequest(USBH_HandleTypeDef *phost) {
    USBH_StatusTypeDef status = USBH_BUSY;
    CP2105_HandleTypeDef *CP2105_Handle = (CP2105_HandleTypeDef *)phost->pActiveClass->pData;

    switch (CP2105_Handle->state) {
        case CP2105_IDLE_STATE:
            status = GetLineCoding(phost, &CP2105_Handle->LineCoding[CP2105_STD_PORT], CP2105_STD_PORT);
            if (status == USBH_OK) {
                CP2105_Handle->state = CP2105_GET_LINE_CODING_STATE_PORT1;
                status = USBH_BUSY;
            } else if (status == USBH_NOT_SUPPORTED) {
                USBH_ErrLog("Control error: CP2105: Device Get Line Coding configuration failed for Standard Port");
                return USBH_FAIL;
            }
            break;

        case CP2105_GET_LINE_CODING_STATE_PORT1:
            status = GetLineCoding(phost, &CP2105_Handle->LineCoding[CP2105_ENH_PORT], CP2105_ENH_PORT);
            if (status == USBH_OK) {
                CP2105_Handle->state = CP2105_GET_LINE_CODING_STATE_PORT2;
                status = USBH_BUSY;
            } else if (status == USBH_NOT_SUPPORTED) {
                USBH_ErrLog("Control error: CP2105: Device Get Line Coding configuration failed for Enhanced Port");
                return USBH_FAIL;
            }
            break;

        case CP2105_GET_LINE_CODING_STATE_PORT2:
            status = GetBaudRate(phost, &CP2105_Handle->BaudRate[CP2105_STD_PORT], CP2105_STD_PORT);
            if (status == USBH_OK) {
                CP2105_Handle->state = CP2105_GET_BAUD_RATE_STATE_PORT1;
                status = USBH_BUSY;
            } else if (status == USBH_NOT_SUPPORTED) {
                USBH_ErrLog("Control error: CP2105: Device Get Baud Rate configuration failed for Standard Port");
                return USBH_FAIL;
            }
            break;

        case CP2105_GET_BAUD_RATE_STATE_PORT1:
            status = GetBaudRate(phost, &CP2105_Handle->BaudRate[CP2105_ENH_PORT], CP2105_ENH_PORT);
            if (status == USBH_OK) {
                CP2105_Handle->state = CP2105_GET_BAUD_RATE_STATE_PORT2;
                status = USBH_BUSY;
            } else if (status == USBH_NOT_SUPPORTED) {
                USBH_ErrLog("Control error: CP2105: Device Get Baud Rate configuration failed for Enhanced Port");
                return USBH_FAIL;
            }
            break;

        case CP2105_GET_BAUD_RATE_STATE_PORT2:
            phost->pUser(phost, HOST_USER_CLASS_ACTIVE);
            CP2105_Handle->state = CP2105_IDLE_STATE;
            status = USBH_OK;
            break;

        default:
            break;
    }

    return status;
}

static USBH_StatusTypeDef USBH_CP2105_Process(USBH_HandleTypeDef *phost)
{
    USBH_StatusTypeDef status = USBH_BUSY;
    USBH_StatusTypeDef req_status = USBH_OK;
    CP2105_HandleTypeDef *CP2105_Handle = (CP2105_HandleTypeDef *)phost->pActiveClass->pData;

    switch (CP2105_Handle->state)
    {
        case CP2105_IDLE_STATE:
            status = USBH_OK;
            break;

        case CP2105_SET_LINE_CODING_STATE:
            req_status = SetLineCoding(phost, CP2105_Handle->pUserLineCoding, CP2105_STD_PORT);
            if (req_status == USBH_OK)
            {
                CP2105_Handle->state = CP2105_GET_LAST_LINE_CODING_STATE;
            }
            else
            {
                if (req_status != USBH_BUSY)
                {
                    CP2105_Handle->state = CP2105_ERROR_STATE;
                }
            }
            break;

        case CP2105_GET_LAST_LINE_CODING_STATE:
            req_status = GetLineCoding(phost, &CP2105_Handle->LineCoding[CP2105_STD_PORT], CP2105_STD_PORT);
            if (req_status == USBH_OK)
            {
                CP2105_Handle->state = CP2105_GET_BAUD_RATE_STATE_PORT1;
            }
            else
            {
                if (req_status != USBH_BUSY)
                {
                    CP2105_Handle->state = CP2105_ERROR_STATE;
                }
            }
            break;

        case CP2105_GET_BAUD_RATE_STATE_PORT1:
            req_status = GetBaudRate(phost, &CP2105_Handle->BaudRate[CP2105_STD_PORT], CP2105_STD_PORT);
            if (req_status == USBH_OK)
            {
                CP2105_Handle->state = CP2105_GET_LINE_CODING_STATE_PORT2;
            }
            else
            {
                if (req_status != USBH_BUSY)
                {
                    CP2105_Handle->state = CP2105_ERROR_STATE;
                }
            }
            break;

        case CP2105_GET_LINE_CODING_STATE_PORT2:
            req_status = GetLineCoding(phost, &CP2105_Handle->LineCoding[CP2105_ENH_PORT], CP2105_ENH_PORT);
            if (req_status == USBH_OK)
            {
                CP2105_Handle->state = CP2105_GET_BAUD_RATE_STATE_PORT2;
            }
            else
            {
                if (req_status != USBH_BUSY)
                {
                    CP2105_Handle->state = CP2105_ERROR_STATE;
                }
            }
            break;

        case CP2105_GET_BAUD_RATE_STATE_PORT2:
            req_status = GetBaudRate(phost, &CP2105_Handle->BaudRate[CP2105_ENH_PORT], CP2105_ENH_PORT);
            if (req_status == USBH_OK)
            {
                CP2105_Handle->state = CP2105_IDLE_STATE;
                USBH_CDC_LineCodingChanged(phost);  // Notify user that line coding has changed
                status = USBH_OK;
            }
            else
            {
                if (req_status != USBH_BUSY)
                {
                    CP2105_Handle->state = CP2105_ERROR_STATE;
                }
            }
            break;

        case CP2105_TRANSFER_DATA:
            CP2105_ProcessTransmission(phost);
            CP2105_ProcessReception(phost);
            break;

        case CP2105_ERROR_STATE:
            req_status = USBH_ClrFeature(phost, 0x00U);
            if (req_status == USBH_OK)
            {
                CP2105_Handle->state = CP2105_IDLE_STATE;
            }
            break;

        default:
            break;
    }

    return status;
}


static USBH_StatusTypeDef USBH_CP2105_SOFProcess(USBH_HandleTypeDef *phost) {
  return USBH_OK;
}

static USBH_StatusTypeDef SetLineCoding(USBH_HandleTypeDef *phost, CP2105_LineCodingTypeDef *linecoding, uint8_t port) {
  phost->Control.setup.b.bmRequestType = 0x41;
  phost->Control.setup.b.bRequest = 0x03;
  phost->Control.setup.b.wValue.w = 0x800;
  phost->Control.setup.b.wIndex.w = port;
  phost->Control.setup.b.wLength.w = 0U;

  return USBH_CtlReq(phost, (uint8_t *)linecoding, 0U);
}

static USBH_StatusTypeDef GetLineCoding(USBH_HandleTypeDef *phost, CP2105_LineCodingTypeDef *linecoding, uint8_t port) {
  phost->Control.setup.b.bmRequestType = 0xc1;
  phost->Control.setup.b.bRequest = 0x04;
  phost->Control.setup.b.wValue.w = 0x00;
  phost->Control.setup.b.wIndex.w = port;
  phost->Control.setup.b.wLength.w = 2U;

  return USBH_CtlReq(phost, (uint8_t *)linecoding, 2U);
}



static USBH_StatusTypeDef SetBaudRate(USBH_HandleTypeDef *phost, uint32_t baudRate, uint8_t port)
{
    phost->Control.setup.b.bmRequestType = 0x41;
    phost->Control.setup.b.bRequest = CP210x_SET_BAUDRATE;
    phost->Control.setup.b.wValue.w = 0;
    phost->Control.setup.b.wIndex.w = port;
    phost->Control.setup.b.wLength.w = 4;

    USBH_StatusTypeDef status = USBH_CtlReq(phost, (uint8_t*)&baudRate, 4);
    if (status == USBH_OK) {
        CP2105_HandleTypeDef *CP2105_Handle = (CP2105_HandleTypeDef *)phost->pActiveClass->pData;
        CP2105_Handle->BaudRate[port] = baudRate; // Store the baud rate in the handle
    }
    return status;
}



static USBH_StatusTypeDef GetBaudRate(USBH_HandleTypeDef *phost, uint32_t *baudRate, uint8_t port)
{
    phost->Control.setup.b.bmRequestType = 0xc1;
    phost->Control.setup.b.bRequest = CP210x_GET_BAUDRATE;
    phost->Control.setup.b.wValue.w = 0;
    phost->Control.setup.b.wIndex.w = port;
    phost->Control.setup.b.wLength.w = 4;

    USBH_StatusTypeDef status = USBH_CtlReq(phost, (uint8_t*)baudRate, 4);
    if (status == USBH_OK) {
        CP2105_HandleTypeDef *CP2105_Handle = (CP2105_HandleTypeDef *)phost->pActiveClass->pData;
        CP2105_Handle->BaudRate[port] = *baudRate; // Store the baud rate in the handle
    }
    return status;
}

static void CP2105_ProcessTransmission(USBH_HandleTypeDef *phost) {
  CP2105_HandleTypeDef *CP2105_Handle = (CP2105_HandleTypeDef *)phost->pActiveClass->pData;
  USBH_URBStateTypeDef URB_Status;

  for (uint8_t port = 0; port < 2; port++) {
    switch (CP2105_Handle->Port[port].data_tx_state) {
      case CP2105_SEND_DATA:
        USBH_UsrLog("Sending data on port %d", port);
        if (CP2105_Handle->TxDataLength[port] > CP2105_Handle->Port[port].OutEpSize) {
          USBH_UsrLog("Sending %d bytes", CP2105_Handle->Port[port].OutEpSize);
          (void)USBH_BulkSendData(phost, CP2105_Handle->pTxData[port], CP2105_Handle->Port[port].OutEpSize, CP2105_Handle->Port[port].OutPipe, 1U);
          CP2105_Handle->TxDataLength[port] -= CP2105_Handle->Port[port].OutEpSize;
          CP2105_Handle->pTxData[port] += CP2105_Handle->Port[port].OutEpSize;
        } else {
          USBH_UsrLog("Sending last %d bytes", CP2105_Handle->TxDataLength[port]);
          (void)USBH_BulkSendData(phost, CP2105_Handle->pTxData[port], (uint16_t)CP2105_Handle->TxDataLength[port], CP2105_Handle->Port[port].OutPipe, 1U);
          CP2105_Handle->TxDataLength[port] = 0U;
        }
        CP2105_Handle->Port[port].data_tx_state = CP2105_SEND_DATA_WAIT;
        break;

      case CP2105_SEND_DATA_WAIT:
        URB_Status = USBH_LL_GetURBState(phost, CP2105_Handle->Port[port].OutPipe);
        USBH_UsrLog("URB_Status: %d", URB_Status);

        if (URB_Status == USBH_URB_DONE) {
          USBH_UsrLog("Data sent on port %d", port);
          if (CP2105_Handle->TxDataLength[port] > 0U) {
            CP2105_Handle->Port[port].data_tx_state = CP2105_SEND_DATA;
          } else {
            CP2105_Handle->Port[port].data_tx_state = CP2105_IDLE_STATE;
            USBH_CP2105_TransmitCallback(phost, port);
          }
        }
        break;

      default:
        break;
    }
  }
}


static void CP2105_ProcessReception(USBH_HandleTypeDef *phost) {
    CP2105_HandleTypeDef *CP2105_Handle = (CP2105_HandleTypeDef *)phost->pActiveClass->pData;
    USBH_URBStateTypeDef URB_Status;
    uint32_t length;

    for (uint8_t port = 0; port < 2; port++) {
        switch (CP2105_Handle->Port[port].data_rx_state) {
            case CP2105_RECEIVE_DATA:
                (void)USBH_BulkReceiveData(phost, CP2105_Handle->pRxData[port], CP2105_Handle->Port[port].InEpSize, CP2105_Handle->Port[port].InPipe);
                CP2105_Handle->Port[port].data_rx_state = CP2105_RECEIVE_DATA_WAIT;
                break;

            case CP2105_RECEIVE_DATA_WAIT:
                URB_Status = USBH_LL_GetURBState(phost, CP2105_Handle->Port[port].InPipe);

                if (URB_Status == USBH_URB_DONE) {
                    length = USBH_LL_GetLastXferSize(phost, CP2105_Handle->Port[port].InPipe);

                    if (((CP2105_Handle->RxDataLength[port] - length) > 0U) && (length > CP2105_Handle->Port[port].InEpSize)) {
                        CP2105_Handle->RxDataLength[port] -= length;
                        CP2105_Handle->pRxData[port] += length;
                        CP2105_Handle->Port[port].data_rx_state = CP2105_RECEIVE_DATA;
                    } else {
                        CP2105_Handle->RxDataLength[port] = 0U;
                        CP2105_Handle->Port[port].data_rx_state = CP2105_IDLE_STATE;
                        USBH_CP2105_ReceiveCallback(phost, port);
                    }
                }
                break;

            default:
                break;
        }
    }
}


void USBH_CP2105_LineCodingChanged(USBH_HandleTypeDef *phost, uint8_t port) {
  /* This function should be implemented in the user file to process the changes */
}

void USBH_CP2105_TransmitCallback(USBH_HandleTypeDef *phost, uint8_t port) {
  /* This function should be implemented in the user file to process the data transmission completion */
}

void USBH_CP2105_ReceiveCallback(USBH_HandleTypeDef *phost, uint8_t port) {
  /* This function should be implemented in the user file to process the data reception completion */
}

USBH_StatusTypeDef USBH_CP2105_SetLineCoding(USBH_HandleTypeDef *phost, CP2105_LineCodingTypeDef *linecoding, uint8_t port) {
  CP2105_HandleTypeDef *CP2105_Handle = (CP2105_HandleTypeDef *)phost->pActiveClass->pData;
  CP2105_Handle->pUserLineCoding = linecoding;
  CP2105_Handle->state = CP2105_SET_LINE_CODING_STATE;
  return USBH_OK;
}

USBH_StatusTypeDef USBH_CP2105_GetLineCoding(USBH_HandleTypeDef *phost, CP2105_LineCodingTypeDef *linecoding, uint8_t port) {
  CP2105_HandleTypeDef *CP2105_Handle = (CP2105_HandleTypeDef *)phost->pActiveClass->pData;
  return GetLineCoding(phost, linecoding, port);
}

USBH_StatusTypeDef USBH_CP2105_Transmit(USBH_HandleTypeDef *phost, uint8_t *pbuff, uint32_t length, uint8_t port) {
  CP2105_HandleTypeDef *CP2105_Handle = (CP2105_HandleTypeDef *)phost->pActiveClass->pData;

  if ((CP2105_Handle->state == CP2105_IDLE_STATE) || (CP2105_Handle->state == CP2105_TRANSFER_DATA)) {
    CP2105_Handle->pTxData[port] = pbuff;
    CP2105_Handle->TxDataLength[port] = length;
    CP2105_Handle->Port[port].data_tx_state = CP2105_SEND_DATA;
    CP2105_Handle->state = CP2105_TRANSFER_DATA;
    USBH_UsrLog("Starting transmission of %lu bytes on port %d", length, port);
    return USBH_OK;
  } else {
    USBH_ErrLog("Transmission busy");
    return USBH_BUSY;
  }
}

USBH_StatusTypeDef USBH_CP2105_Receive(USBH_HandleTypeDef *phost, uint8_t *pbuff, uint32_t length, uint8_t port) {
  CP2105_HandleTypeDef *CP2105_Handle = (CP2105_HandleTypeDef *)phost->pActiveClass->pData;

  if ((CP2105_Handle->state == CP2105_IDLE_STATE) || (CP2105_Handle->state == CP2105_TRANSFER_DATA)) {
    CP2105_Handle->pRxData[port] = pbuff;
    CP2105_Handle->RxDataLength[port] = length;
    CP2105_Handle->Port[port].data_rx_state = CP2105_RECEIVE_DATA;
    CP2105_Handle->state = CP2105_TRANSFER_DATA;
    return USBH_OK;
  } else {
    return USBH_BUSY;
  }
}
