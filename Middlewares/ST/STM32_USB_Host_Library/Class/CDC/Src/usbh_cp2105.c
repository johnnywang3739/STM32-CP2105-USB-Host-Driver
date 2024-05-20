#include "usbh_cp2105.h"


static USBH_StatusTypeDef USBH_CP2105_InterfaceInit(USBH_HandleTypeDef *phost);
static USBH_StatusTypeDef USBH_CP2105_InterfaceDeInit(USBH_HandleTypeDef *phost);
static USBH_StatusTypeDef USBH_CP2105_ClassRequest(USBH_HandleTypeDef *phost);
static USBH_StatusTypeDef USBH_CP2105_Process(USBH_HandleTypeDef *phost);
static USBH_StatusTypeDef USBH_CP2105_SOFProcess(USBH_HandleTypeDef *phost);

static USBH_StatusTypeDef GetLineCoding(USBH_HandleTypeDef *phost, CP2105_LineCodingTypeDef *linecoding, uint8_t port);
static USBH_StatusTypeDef SetLineCoding(USBH_HandleTypeDef *phost, CP2105_LineCodingTypeDef *linecoding, uint8_t port);

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



USBH_StatusTypeDef USBH_CP2105_InterfaceInit(USBH_HandleTypeDef *phost) {
  USBH_StatusTypeDef status;
  uint8_t interface;
  CP2105_HandleTypeDef *CP2105_Handle;
  
  interface = USBH_FindInterface(phost, CP2105_INTERFACE_CLASS_CODE, CP2105_INTERFACE_SUBCLASS_CODE, CP2105_INTERFACE_PROTOCOL_CODE);

  if ((interface == 0xFFU) || (interface >= USBH_MAX_NUM_INTERFACES)) {
    USBH_DbgLog("Cannot Find the interface for CP2105 Interface Class.", phost->pActiveClass->Name);
    return USBH_FAIL;
  }

  status = USBH_SelectInterface(phost, interface);
  if (status != USBH_OK) {
    return USBH_FAIL;
  }

  phost->pActiveClass->pData = (CP2105_HandleTypeDef *)USBH_malloc(sizeof(CP2105_HandleTypeDef));
  CP2105_Handle = (CP2105_HandleTypeDef *)phost->pActiveClass->pData;

  if (CP2105_Handle == NULL) {
    USBH_DbgLog("Cannot allocate memory for CP2105 Handle");
    return USBH_FAIL;
  }

  (void)USBH_memset(CP2105_Handle, 0, sizeof(CP2105_HandleTypeDef));

  for (uint8_t i = 0; i < 2; i++) {
    if ((phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[0].bEndpointAddress & 0x80U) != 0U) {
      CP2105_Handle->Port[i].NotifEp = phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[0].bEndpointAddress;
      CP2105_Handle->Port[i].NotifEpSize  = phost->device.CfgDesc.Itf_Desc[interface].Ep_Desc[0].wMaxPacketSize;
    }

    CP2105_Handle->Port[i].NotifPipe = USBH_AllocPipe(phost, CP2105_Handle->Port[i].NotifEp);

    (void)USBH_OpenPipe(phost, CP2105_Handle->Port[i].NotifPipe, CP2105_Handle->Port[i].NotifEp, phost->device.address, phost->device.speed, USB_EP_TYPE_INTR, CP2105_Handle->Port[i].NotifEpSize);
  }

  CP2105_Handle->state = CP2105_IDLE_STATE;

  return USBH_OK;
}

USBH_StatusTypeDef USBH_CP2105_InterfaceDeInit(USBH_HandleTypeDef *phost) {
  CP2105_HandleTypeDef *CP2105_Handle = (CP2105_HandleTypeDef *)phost->pActiveClass->pData;

  if (CP2105_Handle->Port[0].NotifPipe != 0U) {
    (void)USBH_ClosePipe(phost, CP2105_Handle->Port[0].NotifPipe);
    (void)USBH_FreePipe(phost, CP2105_Handle->Port[0].NotifPipe);
    CP2105_Handle->Port[0].NotifPipe = 0U;
  }

  for (uint8_t i = 0; i < 2; i++) {
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




// static USBH_StatusTypeDef USBH_CP2105_ClassRequest(USBH_HandleTypeDef *phost) {
//     USBH_StatusTypeDef status = USBH_BUSY;
//     CP2105_HandleTypeDef *CP2105_Handle = (CP2105_HandleTypeDef *)phost->pActiveClass->pData;

//     switch (CP2105_Handle->state) {
//         case CP2105_IDLE_STATE:
//             status = SetLineCoding(phost, CP2105_Handle->pUserLineCoding, CP2105_PORT1);
//             if (status == USBH_OK) {
//                 CP2105_Handle->state = CP2105_SET_LINE_CODING_STATE_PORT1;
//             } else if (status == USBH_NOT_SUPPORTED) {
//                 USBH_ErrLog("Control error: CP2105: Device Set Line Coding configuration failed");
//                 return USBH_FAIL;
//             }
//             break;

//         case CP2105_SET_LINE_CODING_STATE_PORT1:
//             status = SetLineCoding(phost, CP2105_Handle->pUserLineCoding, CP2105_PORT2);
//             if (status == USBH_OK) {
//                 CP2105_Handle->state = CP2105_GET_LINE_CODING_STATE_PORT1;
//             } else if (status == USBH_NOT_SUPPORTED) {
//                 USBH_ErrLog("Control error: CP2105: Device Set Line Coding configuration failed");
//                 return USBH_FAIL;
//             }
//             break;

//         case CP2105_GET_LINE_CODING_STATE_PORT1:
//             status = GetLineCoding(phost, &CP2105_Handle->LineCoding, CP2105_PORT1);
//             if (status == USBH_OK) {
//                 CP2105_Handle->state = CP2105_GET_LINE_CODING_STATE_PORT2;
//             } else if (status == USBH_NOT_SUPPORTED) {
//                 USBH_ErrLog("Control error: CP2105: Device Get Line Coding configuration failed");
//                 return USBH_FAIL;
//             }
//             break;

//         case CP2105_GET_LINE_CODING_STATE_PORT2:
//             status = GetLineCoding(phost, &CP2105_Handle->LineCoding, CP2105_PORT2);
//             if (status == USBH_OK) {
//                 phost->pUser(phost, HOST_USER_CLASS_ACTIVE);
//                 CP2105_Handle->state = CP2105_IDLE_STATE;
//             } else if (status == USBH_NOT_SUPPORTED) {
//                 USBH_ErrLog("Control error: CP2105: Device Get Line Coding configuration failed");
//                 return USBH_FAIL;
//             }
//             break;

//         default:
//             break;
//     }

//     return status;
// }

static USBH_StatusTypeDef USBH_CP2105_ClassRequest(USBH_HandleTypeDef *phost)
{
  USBH_StatusTypeDef status = USBH_BUSY;
  CP2105_HandleTypeDef *CP2105_Handle = (CP2105_HandleTypeDef *)phost->pActiveClass->pData;

  /* Issue the get line coding request */
  status = GetLineCoding(phost, &CP2105_Handle->LineCoding, CP2105_ENH_PORT);
  if (status == USBH_OK)
  {
    phost->pUser(phost, HOST_USER_CLASS_ACTIVE);
  }
  else if (status == USBH_NOT_SUPPORTED)
  {
    USBH_ErrLog("Control error: CDC: Device Get Line Coding configuration failed");
  }
  else
  {
    /* .. */
  }

  return status;
}

// static USBH_StatusTypeDef USBH_CP2105_ClassRequest(USBH_HandleTypeDef *phost) {
//     USBH_StatusTypeDef status = USBH_BUSY;
//     CP2105_HandleTypeDef *CP2105_Handle = (CP2105_HandleTypeDef *)phost->pActiveClass->pData;

//     switch (CP2105_Handle->state) {
//         case CP2105_IDLE_STATE:
//             status = SetLineCoding(phost, &CP2105_Handle->LineCoding, CP2105_PORT1);
//             if (status == USBH_OK) {
//             	phost->pUser(phost, HOST_USER_CLASS_ACTIVE);

//                 CP2105_Handle->state = CP2105_GET_LAST_LINE_CODING_STATE;
//             } else if (status == USBH_NOT_SUPPORTED) {
//                 USBH_ErrLog("Control error: CP2105: Device Get Line Coding configuration failed");
//                 return USBH_FAIL;
//             }
//             break;

//         case CP2105_GET_LAST_LINE_CODING_STATE:
//             status = SetLineCoding(phost, &CP2105_Handle->LineCoding, CP2105_PORT1);
//             if (status == USBH_OK) {
//                 phost->pUser(phost, HOST_USER_CLASS_ACTIVE);
//                 CP2105_Handle->state = CP2105_IDLE_STATE;
//             } else if (status == USBH_NOT_SUPPORTED) {
//                 USBH_ErrLog("Control error: CP2105: Device Get Line Coding configuration failed");
//                 return USBH_FAIL;
//             }
//             break;

//         default:
//             break;
//     }

//     return status;
// }


USBH_StatusTypeDef USBH_CP2105_Process(USBH_HandleTypeDef *phost) {
  USBH_StatusTypeDef status = USBH_OK;
  CP2105_HandleTypeDef *CP2105_Handle = (CP2105_HandleTypeDef *)phost->pActiveClass->pData;
  USBH_URBStateTypeDef URB_Status;

  switch (CP2105_Handle->state) {
    case CP2105_IDLE_STATE:
      break;

    case CP2105_TRANSFER_DATA:
      CP2105_ProcessTransmission(phost);
      CP2105_ProcessReception(phost);
      break;

    case CP2105_SET_LINE_CODING_STATE:
      status = SetLineCoding(phost, CP2105_Handle->pUserLineCoding, CP2105_STD_PORT);
      if (status == USBH_OK) {
        CP2105_Handle->state = CP2105_IDLE_STATE;
      }
      break;

    case CP2105_ERROR_STATE:
      status = USBH_FAIL;
      break;

    default:
      break;
  }

  return status;
}

USBH_StatusTypeDef USBH_CP2105_SOFProcess(USBH_HandleTypeDef *phost) {
  return USBH_OK;
}

// static USBH_StatusTypeDef GetLineCoding(USBH_HandleTypeDef *phost, CP2105_LineCodingTypeDef *linecoding, uint8_t port) {
//   return USBH_CtlReq(phost, (uint8_t *)linecoding, 7U);
// }

static USBH_StatusTypeDef SetLineCoding(USBH_HandleTypeDef *phost, CP2105_LineCodingTypeDef *linecoding, uint8_t port) {
    phost->Control.setup.b.bmRequestType = 0x41; 
    phost->Control.setup.b.bRequest = 0x03;
    phost->Control.setup.b.wValue.w = 0x800; 
    phost->Control.setup.b.wIndex.w = port;
    phost->Control.setup.b.wLength.w = 0x00;

    return USBH_CtlReq(phost, (uint8_t *)linecoding, 0x00);
}

static USBH_StatusTypeDef GetLineCoding(USBH_HandleTypeDef *phost, CP2105_LineCodingTypeDef *linecoding, uint8_t port) {
    phost->Control.setup.b.bmRequestType = 0xc1; 
    phost->Control.setup.b.bRequest = 0x04;
   phost->Control.setup.b.wValue.w = 0x00;
   phost->Control.setup.b.wIndex.w = port;
    phost->Control.setup.b.wLength.w =2; 

    return USBH_CtlReq(phost, (uint8_t *)linecoding, 2);
}


// //
//static USBH_StatusTypeDef SetLineCoding(USBH_HandleTypeDef *phost, CP2105_LineCodingTypeDef *linecoding, uint8_t port) {
//    phost->Control.setup.b.bmRequestType = USB_H2D | USB_REQ_TYPE_CLASS | USB_REQ_RECIPIENT_INTERFACE;
//    phost->Control.setup.b.bRequest = CP2105_SET_LINE_CODING;
//    phost->Control.setup.b.wValue.w = 0U;
//    phost->Control.setup.b.wIndex.w = port;
//    phost->Control.setup.b.wLength.w = sizeof(CP2105_LineCodingTypeDef);
//
//    return USBH_CtlReq(phost, (uint8_t *)linecoding, sizeof(CP2105_LineCodingTypeDef));
//}


static void CP2105_ProcessTransmission(USBH_HandleTypeDef *phost) {
  CP2105_HandleTypeDef *CP2105_Handle = (CP2105_HandleTypeDef *)phost->pActiveClass->pData;
  USBH_URBStateTypeDef URB_Status;

  switch (CP2105_Handle->data_tx_state) {
    case CP2105_SEND_DATA:
      if (CP2105_Handle->TxDataLength > CP2105_Handle->Port[0].OutEpSize) {
        (void)USBH_BulkSendData(phost, CP2105_Handle->pTxData, CP2105_Handle->Port[0].OutEpSize, CP2105_Handle->Port[0].OutPipe, 1U);
        CP2105_Handle->TxDataLength -= CP2105_Handle->Port[0].OutEpSize;
        CP2105_Handle->pTxData += CP2105_Handle->Port[0].OutEpSize;
      } else {
        (void)USBH_BulkSendData(phost, CP2105_Handle->pTxData, (uint16_t)CP2105_Handle->TxDataLength, CP2105_Handle->Port[0].OutPipe, 1U);
        CP2105_Handle->TxDataLength = 0U;
      }
      CP2105_Handle->data_tx_state = CP2105_SEND_DATA_WAIT;
      break;

    case CP2105_SEND_DATA_WAIT:
      URB_Status = USBH_LL_GetURBState(phost, CP2105_Handle->Port[0].OutPipe);

      if (URB_Status == USBH_URB_DONE) {
        if (CP2105_Handle->TxDataLength > 0U) {
          CP2105_Handle->data_tx_state = CP2105_SEND_DATA;
        } else {
          CP2105_Handle->data_tx_state = CP2105_IDLE_STATE;
          USBH_CP2105_TransmitCallback(phost);
        }
      }
      break;

    default:
      break;
  }
}

static void CP2105_ProcessReception(USBH_HandleTypeDef *phost) {
  CP2105_HandleTypeDef *CP2105_Handle = (CP2105_HandleTypeDef *)phost->pActiveClass->pData;
  USBH_URBStateTypeDef URB_Status;
  uint32_t length;

  switch (CP2105_Handle->data_rx_state) {
    case CP2105_RECEIVE_DATA:
      (void)USBH_BulkReceiveData(phost, CP2105_Handle->pRxData, CP2105_Handle->Port[0].InEpSize, CP2105_Handle->Port[0].InPipe);
      CP2105_Handle->data_rx_state = CP2105_RECEIVE_DATA_WAIT;
      break;

    case CP2105_RECEIVE_DATA_WAIT:
      URB_Status = USBH_LL_GetURBState(phost, CP2105_Handle->Port[0].InPipe);

      if (URB_Status == USBH_URB_DONE) {
        length = USBH_LL_GetLastXferSize(phost, CP2105_Handle->Port[0].InPipe);

        if (((CP2105_Handle->RxDataLength - length) > 0U) && (length > CP2105_Handle->Port[0].InEpSize)) {
          CP2105_Handle->RxDataLength -= length;
          CP2105_Handle->pRxData += length;
          CP2105_Handle->data_rx_state = CP2105_RECEIVE_DATA;
        } else {
          CP2105_Handle->RxDataLength = 0U;
          CP2105_Handle->data_rx_state = CP2105_IDLE_STATE;
          USBH_CP2105_ReceiveCallback(phost);
        }
      }
      break;

    default:
      break;
  }
}

void USBH_CP2105_LineCodingChanged(USBH_HandleTypeDef *phost) {
  /* This function should be implemented in the user file to process the changes */
}

void USBH_CP2105_TransmitCallback(USBH_HandleTypeDef *phost) {
  /* This function should be implemented in the user file to process the data transmission completion */
}

void USBH_CP2105_ReceiveCallback(USBH_HandleTypeDef *phost) {
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
    CP2105_Handle->pTxData = pbuff;
    CP2105_Handle->TxDataLength = length;
    CP2105_Handle->data_tx_state = CP2105_SEND_DATA;
    CP2105_Handle->state = CP2105_TRANSFER_DATA;
    return USBH_OK;
  } else {
    return USBH_BUSY;
  }
}

USBH_StatusTypeDef USBH_CP2105_Receive(USBH_HandleTypeDef *phost, uint8_t *pbuff, uint32_t length, uint8_t port) {
  CP2105_HandleTypeDef *CP2105_Handle = (CP2105_HandleTypeDef *)phost->pActiveClass->pData;

  if ((CP2105_Handle->state == CP2105_IDLE_STATE) || (CP2105_Handle->state == CP2105_TRANSFER_DATA)) {
    CP2105_Handle->pRxData = pbuff;
    CP2105_Handle->RxDataLength = length;
    CP2105_Handle->data_rx_state = CP2105_RECEIVE_DATA;
    CP2105_Handle->state = CP2105_TRANSFER_DATA;
    return USBH_OK;
  } else {
    return USBH_BUSY;
  }
}