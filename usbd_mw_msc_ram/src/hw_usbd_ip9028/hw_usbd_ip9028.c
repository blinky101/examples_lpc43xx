/***********************************************************************
* $Id:: hw_usbd_ip9028.c 1234 2014-01-08 18:37:23Z usb10131           $
*
* Project: IP9208 USB-OTG device controller definitions
*
* Description:
*     This file contains USBD driver support for the IP9028 USB device
*  controller.
*
***********************************************************************
*   Copyright(C) 2011, NXP Semiconductor
*   All rights reserved.
*
* Software that is described herein is for illustrative purposes only
* which provides customers with programming information regarding the
* products. This software is supplied "AS IS" without any warranties.
* NXP Semiconductors assumes no responsibility or liability for the
* use of the software, conveys no license or title under any patent,
* copyright, or mask work right to the product. NXP Semiconductors
* reserves the right to make changes in the software without
* notification. NXP Semiconductors also make no representation or
* warranty that such application will be suitable for the specified
* use without further testing or modification.
**********************************************************************/

/** @ingroup HAL USBD_STACK
 * @{
 */

#include <string.h>
#include "mw_usbd.h"
#include "mw_usbd_core.h"
#include "mw_usbd_hw.h"
#include "hw_usbd_ip9028.h"

typedef struct __USBD_HW_DATA_T
{
  DQH_T* ep_QH;
  DTD_T* ep_TD;
  uint32_t ep_read_len[USB_MAX_EP_NUM];
  USB_OTG_REGS_T* regs;
  USB_CORE_CTRL_T* pCtrl;

} USBD_HW_DATA_T;


/**
 * @brief   Get Endpoint Physical Address.
 * @param [in] EPNum Endpoint Number.
 *                       EPNum.0..3: Address
 *                       EPNum.7:    Dir
 * @retval  Endpoint Physical Address.
 *
 * Example Usage:
 * @code
 *    USB_SetStallEP(hUsb, 0x83); // stall ep3_IN.
 * @endcode
 */

static uint32_t EPAdr(uint32_t EPNum)
{
  uint32_t val;

  val = (EPNum & 0x0F) << 1;
  if (EPNum & 0x80)
  {
    val += 1;
  }
  return (val);
}

/**
 * @brief   Get Endpoint Physical Address.
 * @param [in] EPNum Endpoint Number.
 *                       EPNum.0..3: Address
 *                       EPNum.7:    Dir
 * @retval  Endpoint Physical Address.
 *
 * Example Usage:
 * @code
 *    USB_SetStallEP(hUsb, 0x83); // stall ep3_IN.
 * @endcode
 */
 void hwUSB_ForceFullSpeed(USBD_HANDLE_T hUsb, uint32_t con)
{
  USB_CORE_CTRL_T* pCtrl = (USB_CORE_CTRL_T*)hUsb;
  USBD_HW_DATA_T* drv = (USBD_HW_DATA_T*)pCtrl->hw_data;

  if (con)
    drv->regs->portsc1 |= USBPRTS_PFSC;
  else
    drv->regs->portsc1 &= ~USBPRTS_PFSC;
}


/*
*  USB Connect Function
*   Called by the User to Connect/Disconnect USB
*    Parameters:      con:   Connect/Disconnect
*    Return Value:    None
*/

void hwUSB_Connect(USBD_HANDLE_T hUsb, uint32_t con)
{
  USB_CORE_CTRL_T* pCtrl = (USB_CORE_CTRL_T*)hUsb;
  USBD_HW_DATA_T* drv = (USBD_HW_DATA_T*)pCtrl->hw_data;

  if (con)
    drv->regs->usbcmd |= USBCMD_RS;
  else
    drv->regs->usbcmd &= ~USBCMD_RS;
}


/*
*  USB Reset Function
*   Called automatically on USB Reset
*    Return Value:    None
*/

void hwUSB_Reset(USBD_HANDLE_T hUsb)
{
  USB_CORE_CTRL_T* pCtrl = (USB_CORE_CTRL_T*)hUsb;
  USBD_HW_DATA_T* drv = (USBD_HW_DATA_T*)pCtrl->hw_data;
  uint32_t i;

  USB_SetSpeedMode(pCtrl, USB_FULL_SPEED);  
  /* disable all EPs */
  for (i = 1; i < pCtrl->max_num_ep; i++)
  {
    drv->regs->endptctrl[i] &= ~(EPCTRL_RXE | EPCTRL_TXE);
  }

  /* Clear all pending interrupts */
  drv->regs->endptnak   = 0xFFFFFFFF;
  drv->regs->endptnaken = 0;
  drv->regs->usbsts     = 0xFFFFFFFF;
  drv->regs->endptsetupstat = drv->regs->endptsetupstat;
  drv->regs->endptcomplete  = drv->regs->endptcomplete;
  while (drv->regs->endptprime)                  /* Wait until all bits are 0 */
  {
  }
  drv->regs->endptflush = 0xFFFFFFFF;
  while (drv->regs->endptflush); /* Wait until all bits are 0 */


  /* Set the interrupt Threshold control interval to 0 */
  drv->regs->usbcmd &= ~0x00FF0000;

  /* Zero out the Endpoint queue heads */
  memset((void*)drv->ep_QH, 0, 2 * pCtrl->max_num_ep * sizeof(DQH_T));
  /* Zero out the device transfer descriptors */
  memset((void*)drv->ep_TD, 0, 2 * pCtrl->max_num_ep * sizeof(DTD_T));
  memset((void*)drv->ep_read_len, 0, pCtrl->max_num_ep * sizeof(uint32_t));
  /* Configure the Endpoint List Address */
  /* make sure it in on 64 byte boundary !!! */
  /* init list address */
  drv->regs->asynclistaddr__endpointlistaddr = (uint32_t)drv->ep_QH;
  /* Initialize device queue heads for non ISO endpoint only */
  for (i = 0; i < (2 * pCtrl->max_num_ep); i++)
  {
    drv->ep_QH[i].next_dTD = (uint32_t) & drv->ep_TD[i];
  }
  /* Enable interrupts */
  drv->regs->usbintr =  USBSTS_UI
                      | USBSTS_UEI
                      | USBSTS_PCI
                      | USBSTS_URI
                      | USBSTS_SLI
                      | USBSTS_NAKI;
  /* enable ep0 IN and ep0 OUT */
  drv->ep_QH[0].cap  = QH_MAXP(USB_ENDPOINT_0_HS_MAXP)
                  | QH_IOS
                  | QH_ZLT;
  drv->ep_QH[1].cap  = QH_MAXP(USB_ENDPOINT_0_HS_MAXP)
                  | QH_IOS
                  | QH_ZLT;
  /* enable EP0 */
  drv->regs->endptctrl[0] = EPCTRL_RXE | EPCTRL_RXR | EPCTRL_TXE | EPCTRL_TXR;

}



/*
*  USB Remote Wakeup Function
*   Called automatically on USB Remote Wakeup
*    Return Value:    None
*/

void hwUSB_WakeUp(USBD_HANDLE_T hUsb)
{
  USB_CORE_CTRL_T* pCtrl = (USB_CORE_CTRL_T*)hUsb;
  USBD_HW_DATA_T* drv = (USBD_HW_DATA_T*)pCtrl->hw_data;

  if (pCtrl->device_status & USB_GETSTATUS_REMOTE_WAKEUP)
  {
    /* Set FPR bit in PORTSCX reg p63 */
    drv->regs->portsc1 |= USBPRTS_FPR ;
  }
}


/*
*  USB Remote Wakeup Configuration Function
*    Parameters:      cfg:   Enable/Disable
*    Return Value:    None
*/

void hwUSB_WakeUpCfg(USBD_HANDLE_T hUsb, uint32_t cfg)
{
  /* Not needed */
}


/*
*  USB Set Address Function
*    Parameters:      adr:   USB Address
*    Return Value:    None
*/

void hwUSB_SetAddress(USBD_HANDLE_T hUsb, uint32_t adr)
{
  USB_CORE_CTRL_T* pCtrl = (USB_CORE_CTRL_T*)hUsb;
  USBD_HW_DATA_T* drv = (USBD_HW_DATA_T*)pCtrl->hw_data;

  drv->regs->periodiclistbase__deviceaddr = USBDEV_ADDR(adr);
  drv->regs->periodiclistbase__deviceaddr |= USBDEV_ADDR_AD;
}


/*
*  USB set test mode Function
*    Parameters:      mode:   test mode
*    Return Value:    TRUE if supported else FALSE
*/

ErrorCode_t hwUSB_SetTestMode(USBD_HANDLE_T hUsb, uint8_t mode)
{
  USB_CORE_CTRL_T* pCtrl = (USB_CORE_CTRL_T*)hUsb;
  USBD_HW_DATA_T* drv = (USBD_HW_DATA_T*)pCtrl->hw_data;
  uint32_t portsc;

  if ((mode > 0) && (mode < 8))
  {
    /* read port control register*/
    portsc = drv->regs->portsc1 & ~(0xF << 16);
    /* set the test mode */
    drv->regs->portsc1 = portsc | (mode << 16);
    return LPC_OK;
  }
  return (ERR_USBD_INVALID_REQ);
}

/*
*  USB Configure Function
*    Parameters:      cfg:   Configure/Deconfigure
*    Return Value:    None
*/

void hwUSB_Configure(USBD_HANDLE_T hUsb, uint32_t cfg)
{
  //USB_CORE_CTRL_T* pCtrl = (USB_CORE_CTRL_T*)hUsb;
  //USBD_HW_DATA_T* drv = (USBD_HW_DATA_T*)pCtrl->hw_data;

}

/*
*  Configure USB Endpoint according to Descriptor
*    Parameters:      pEPD:  Pointer to Endpoint Descriptor
*    Return Value:    None
*/

void hwUSB_ConfigEP(USBD_HANDLE_T hUsb, USB_ENDPOINT_DESCRIPTOR *pEPD)
{
  USB_CORE_CTRL_T* pCtrl = (USB_CORE_CTRL_T*)hUsb;
  USBD_HW_DATA_T* drv = (USBD_HW_DATA_T*)pCtrl->hw_data;
  uint32_t num, lep;
  uint32_t ep_cfg;
  uint8_t  bmAttributes;

  lep = pEPD->bEndpointAddress & 0x7F;
  num = EPAdr(pEPD->bEndpointAddress);

  ep_cfg = drv->regs->endptctrl[lep];
  /* mask the attributes we are not-intersetd in */
  bmAttributes = pEPD->bmAttributes & USB_ENDPOINT_TYPE_MASK;
  /* set EP type */
  if (bmAttributes != USB_ENDPOINT_TYPE_ISOCHRONOUS)
  {
    /* init EP capabilities */
    drv->ep_QH[num].cap  = QH_MAXP(pEPD->wMaxPacketSize)
                      | QH_IOS
                      | QH_ZLT ;
  }
  else
  {
    /* init EP capabilities */
    drv->ep_QH[num].cap  = QH_MAXP(pEPD->wMaxPacketSize) 
						| QH_MULT(((pEPD->wMaxPacketSize >> 11) & 0x3) + 1);
  }
  /* The next DTD pointer is INVALID */
  drv->ep_TD[num].next_dTD = 0x01 ;
  /* setup EP control register */
  if (pEPD->bEndpointAddress & 0x80)
  {
    ep_cfg &= ~0xFFFF0000;
    ep_cfg |= EPCTRL_TX_TYPE(bmAttributes)
              | EPCTRL_TXR;
  }
  else
  {
    ep_cfg &= ~0xFFFF;
    ep_cfg |= EPCTRL_RX_TYPE(bmAttributes)
              | EPCTRL_RXR;
  }
  drv->regs->endptctrl[lep] = ep_cfg;
}


/*
*  Set Direction for USB Control Endpoint
*    Parameters:      dir:   Out (dir == 0), In (dir <> 0)
*    Return Value:    None
*/

void hwUSB_DirCtrlEP(USBD_HANDLE_T hUsb, uint32_t dir)
{
  /* Not needed */
}


/*
*  Enable USB Endpoint
*    Parameters:      EPNum: Endpoint Number
*                       EPNum.0..3: Address
*                       EPNum.7:    Dir
*    Return Value:    None
*/

void hwUSB_EnableEP(USBD_HANDLE_T hUsb, uint32_t EPNum)
{
  USB_CORE_CTRL_T* pCtrl = (USB_CORE_CTRL_T*)hUsb;
  USBD_HW_DATA_T* drv = (USBD_HW_DATA_T*)pCtrl->hw_data;
  uint32_t lep, bitpos;

  lep = EPNum & 0x0F;

  if (EPNum & 0x80)
  {
    drv->regs->endptctrl[lep] |= EPCTRL_TXE;
  }
  else
  {
    drv->regs->endptctrl[lep] |= EPCTRL_RXE;
    /* enable NAK interrupt */
    bitpos = USB_EP_BITPOS(EPNum);
    drv->regs->endptnaken |= _BIT(bitpos);
  }
}


/*
*  Disable USB Endpoint
*    Parameters:      EPNum: Endpoint Number
*                       EPNum.0..3: Address
*                       EPNum.7:    Dir
*    Return Value:    None
*/

void hwUSB_DisableEP(USBD_HANDLE_T hUsb, uint32_t EPNum)
{
  USB_CORE_CTRL_T* pCtrl = (USB_CORE_CTRL_T*)hUsb;
  USBD_HW_DATA_T* drv = (USBD_HW_DATA_T*)pCtrl->hw_data;
  uint32_t lep, bitpos;

  lep = EPNum & 0x0F;

  if (EPNum & 0x80)
  {
    drv->regs->endptctrl[lep] &= ~EPCTRL_TXE;
  }
  else
  {
    /* disable NAK interrupt */
    bitpos = USB_EP_BITPOS(EPNum);
    drv->regs->endptnaken &= ~_BIT(bitpos);
    drv->regs->endptctrl[lep] &= ~EPCTRL_RXE;
  }
}


/*
*  Reset USB Endpoint
*    Parameters:      EPNum: Endpoint Number
*                       EPNum.0..3: Address
*                       EPNum.7:    Dir
*    Return Value:    None
*/

void hwUSB_ResetEP(USBD_HANDLE_T hUsb, uint32_t EPNum)
{
  USB_CORE_CTRL_T* pCtrl = (USB_CORE_CTRL_T*)hUsb;
  USBD_HW_DATA_T* drv = (USBD_HW_DATA_T*)pCtrl->hw_data;
  uint32_t bit_pos = USB_EP_BITPOS(EPNum);
  uint32_t lep = EPNum & 0x0F;

  /* flush EP buffers */
  drv->regs->endptflush = _BIT(bit_pos);
  while (drv->regs->endptflush & _BIT(bit_pos));
  /* reset data toggles */
  if (EPNum & 0x80)
  {
    drv->regs->endptctrl[lep] |= EPCTRL_TXR;
  }
  else
  {
    drv->regs->endptctrl[lep] |= EPCTRL_RXR;
  }
}


/**
 * @brief   Generates STALL signalling for requested endpoint.
 * @param [in] hUsb  Handle to USBD stack instance.
 * @param [in] EPNum Endpoint Number.
 *                       EPNum.0..3: Address
 *                       EPNum.7:    Dir
 * @retval  void.
 *
 * Example Usage:
 * @code
 *    USB_SetStallEP(hUsb, 0x83); // stall ep3_IN.
 * @endcode
 */
void hwUSB_SetStallEP(USBD_HANDLE_T hUsb, uint32_t EPNum)
{
  USB_CORE_CTRL_T* pCtrl = (USB_CORE_CTRL_T*)hUsb;
  USBD_HW_DATA_T* drv = (USBD_HW_DATA_T*)pCtrl->hw_data;
  uint32_t lep;

  lep = EPNum & 0x0F;

  if (EPNum & 0x80)
  {
    drv->regs->endptctrl[lep] |= EPCTRL_TXS;
  }
  else
  {
    drv->regs->endptctrl[lep] |= EPCTRL_RXS;
  }
}


/**
 * @name    USB_ClrStallEP
 * @brief   Clear Stall for USB Endpoint.
 * @ingroup USB Device Stack
 *
 * Clear STALL state for the requested endpoint.
 *
 * @param [in] hUsb  Handle to USBD stack instance.
 * @param [in] EPNum Endpoint Number.
 *                       EPNum.0..3: Address
 *                       EPNum.7:    Dir
 * @retval  void.
 *
 * Example Usage:
 * @code
 *    USB_ClrStallEP(hUsb, 0x83); // clear stall ep3_IN.
 * @endcode
 */
void hwUSB_ClrStallEP(USBD_HANDLE_T hUsb, uint32_t EPNum)
{
  USB_CORE_CTRL_T* pCtrl = (USB_CORE_CTRL_T*)hUsb;
  USBD_HW_DATA_T* drv = (USBD_HW_DATA_T*)pCtrl->hw_data;
  uint32_t lep;

  lep = EPNum & 0x0F;

  if (EPNum & 0x80)
  {
    drv->regs->endptctrl[lep] &= ~EPCTRL_TXS;
    /* reset data toggle */
    drv->regs->endptctrl[lep] |= EPCTRL_TXR;
  }
  else
  {
    drv->regs->endptctrl[lep] &= ~EPCTRL_RXS;
    /* reset data toggle */
    drv->regs->endptctrl[lep] |= EPCTRL_RXR;
  }
}

/*
*  Function to enable/disable selected USB event.
*/

ErrorCode_t hwUSB_EnableEvent(USBD_HANDLE_T hUsb, uint32_t EPNum, 
  uint32_t event_type, uint32_t enable)
{
  USB_CORE_CTRL_T* pCtrl = (USB_CORE_CTRL_T*)hUsb;
  USBD_HW_DATA_T* drv = (USBD_HW_DATA_T*)pCtrl->hw_data;
  uint32_t bitpos;
  ErrorCode_t ret = LPC_OK;
  volatile uint32_t* reg = &drv->regs->usbintr;
  uint32_t bitmask = 0;

  switch (event_type)
  {
    case USB_EVT_RESET:
      /* enable reset event */
      bitmask = USBSTS_URI;
      break;
    case USB_EVT_SOF:
      /* enable SOF event */
      bitmask = USBSTS_SRI;
      break;
    case USB_EVT_DEV_STATE:
      bitmask = (USBSTS_PCI | USBSTS_SLI);
      break;
    case USB_EVT_DEV_ERROR:
      /* enable error event */
      bitmask = USBSTS_UEI;
      break;
    case USB_EVT_OUT_NAK:
    case USB_EVT_IN_NAK:
      /* enable NAK interrupts */
      reg = &drv->regs->endptnaken;
      bitpos = USB_EP_BITPOS(EPNum);
      bitmask = _BIT(bitpos);
      break;
    default:
      ret = ERR_USBD_INVALID_REQ;
      break;

  }
  if (enable) {
   *reg |= bitmask;
  } else {
   *reg &= ~bitmask;
  }

  return ret;
}

/**
 * @name    hwUSB_ProgDTD
 * @brief   Program transfer descriptors for USB Endpoint.
 * @ingroup USB Device Stack
 *
 * Program transfer descriptors for USB Endpoint.
 *
 * @param [in] hUsb  Handle to USBD stack instance.
 * @param [in] Edpt Endpoint index. eg. EP3_IN = 7.
 * @param [in] ptrBuff  Pointer to transfer buffer.
 * @param [in] TsfSize  Length of the transfer buffer.
 *
 * @retval  void.
 *
 * Example Usage:
 * @code
 *    hwUSB_ProgDTD(hUsb, 7, pdata, 64); // clear stall ep3_IN.
 * @endcode
 */
void hwUSB_ProgDTD(USBD_HANDLE_T hUsb, uint32_t Edpt, uint32_t ptrBuff, uint32_t TsfSize)
{
  USB_CORE_CTRL_T* pCtrl = (USB_CORE_CTRL_T*)hUsb;
  USBD_HW_DATA_T* drv = (USBD_HW_DATA_T*)pCtrl->hw_data;
  DTD_T*  pDTD ;

  pDTD    = (DTD_T*) & drv->ep_TD[ Edpt ];

  /* Zero out the device transfer descriptors */
  memset((void*)pDTD, 0, sizeof(DTD_T));
  /* The next DTD pointer is INVALID */
  pDTD->next_dTD = 0x01 ;

  /* Length */
  pDTD->total_bytes = ((TsfSize & 0x7fff) << 16);
  pDTD->total_bytes |= TD_IOC ;
  pDTD->total_bytes |= 0x80 ;
  
  /* convert buffer address from virtual to physical */
  if (pCtrl->virt_to_phys)
    ptrBuff = pCtrl->virt_to_phys((void*)ptrBuff);

  pDTD->buffer0 = ptrBuff;
  pDTD->buffer1 = (ptrBuff + 0x1000) & 0xfffff000;
  pDTD->buffer2 = (ptrBuff + 0x2000) & 0xfffff000;
  pDTD->buffer3 = (ptrBuff + 0x3000) & 0xfffff000;
  pDTD->buffer4 = (ptrBuff + 0x4000) & 0xfffff000;


  drv->ep_QH[Edpt].next_dTD = (uint32_t)(&drv->ep_TD[ Edpt ]);
  drv->ep_QH[Edpt].total_bytes &= (~0xC0) ;
}

/*
*  Read USB Endpoint Data
*    Parameters:      EPNum: Endpoint Number
*                       EPNum.0..3: Address
*                       EPNum.7:    Dir
*                     pData: Pointer to Data Buffer
*    Return Value:    Number of bytes read
*/
uint32_t hwUSB_ReadSetupPkt(USBD_HANDLE_T hUsb, uint32_t EPNum, uint32_t *pData)
{
  USB_CORE_CTRL_T* pCtrl = (USB_CORE_CTRL_T*)hUsb;
  USBD_HW_DATA_T* drv = (USBD_HW_DATA_T*)pCtrl->hw_data;
  uint32_t  setup_int, cnt = 0;
  uint32_t num = EPAdr(EPNum);

  setup_int = drv->regs->endptsetupstat ;
  /* Clear the setup interrupt */
  drv->regs->endptsetupstat = setup_int;
  /* ********************************** */
  /*  Check if we have received a setup */
  /* ********************************** */

  if (setup_int & _BIT(0))                    /* Check only for bit 0 */
    /* No setup are admitted on other endpoints than 0 */
  {
    do
    {
      /* Setup in a setup - must consider only the second setup */
      /* - Set the tripwire */
      drv->regs->usbcmd |= USBCMD_SUTW ;

      /* Transfer Set-up data to the user buffer */
      pData[0] = drv->ep_QH[num].setup[0];
      pData[1] = drv->ep_QH[num].setup[1];
      cnt = 8;

    } while (!(drv->regs->usbcmd & USBCMD_SUTW)) ;

    /* setup in a setup - Clear the tripwire */
    drv->regs->usbcmd &= (~USBCMD_SUTW);
  }
  
  while ((setup_int = drv->regs->endptsetupstat) != 0)
  {
    /* Clear the setup interrupt */
    drv->regs->endptsetupstat = setup_int;
  }
  /* flush any pending Control endpoint tranfers. Note, it is possible
  for the device controller to receive setup packets before previous
  control transfers complete. Existing control packets in progress must be
  flushed and the new control packet completed.  */
  drv->regs->endptflush = 0x00010001;
  while (drv->regs->endptflush & 0x00010001);

  return cnt;
}

/*
*  Enque read request
*    Parameters:      EPNum: Endpoint Number
*                       EPNum.0..3: Address
*                       EPNum.7:    Dir
*                     pData: Pointer to Data Buffer
*    Return Value:    Number of bytes read
*/

uint32_t hwUSB_ReadReqEP(USBD_HANDLE_T hUsb, uint32_t EPNum, uint8_t *pData, uint32_t len)
{
  USB_CORE_CTRL_T* pCtrl = (USB_CORE_CTRL_T*)hUsb;
  USBD_HW_DATA_T* drv = (USBD_HW_DATA_T*)pCtrl->hw_data;
  uint32_t num = EPAdr(EPNum);
  uint32_t n = USB_EP_BITPOS(EPNum);

  if ( (drv->regs->endptstatus & _BIT(n)) == 0){
	  hwUSB_ProgDTD(hUsb, num, (uint32_t)pData, len);
	  drv->ep_read_len[EPNum & 0x0F] = len;
	  /* prime the endpoint for read */
	  drv->regs->endptprime = _BIT(n) ;
	  /* check if priming succeeded */
	  while (drv->regs->endptprime & _BIT(n));
  }
  else {
	len = 0;
  }

  return len;
}

/*
*  Read USB Endpoint Data
*    Parameters:      EPNum: Endpoint Number
*                       EPNum.0..3: Address
*                       EPNum.7:    Dir
*                     pData: Pointer to Data Buffer
*    Return Value:    Number of bytes read
*/

uint32_t hwUSB_ReadEP(USBD_HANDLE_T hUsb, uint32_t EPNum, uint8_t *pData)
{
  USB_CORE_CTRL_T* pCtrl = (USB_CORE_CTRL_T*)hUsb;
  USBD_HW_DATA_T* drv = (USBD_HW_DATA_T*)pCtrl->hw_data;
  uint32_t cnt, n;
  DTD_T*  pDTD ;

  n = EPAdr(EPNum);
  pDTD    = (DTD_T*) &drv->ep_TD [ n ] ;

  /* return the total bytes read */
  cnt  = (pDTD->total_bytes >> 16) & 0x7FFF;
  cnt = drv->ep_read_len[EPNum & 0x0F] - cnt;
  /* make buffer cache coherent*/
  if (pCtrl->cache_flush)
    pCtrl->cache_flush((uint32_t*)pData, (uint32_t*)(pData + cnt));

  return (cnt);
}


/*
*  Write USB Endpoint Data
*    Parameters:      EPNum: Endpoint Number
*                       EPNum.0..3: Address
*                       EPNum.7:    Dir
*                     pData: Pointer to Data Buffer
*                     cnt:   Number of bytes to write
*    Return Value:    Number of bytes written
*/

uint32_t hwUSB_WriteEP(USBD_HANDLE_T hUsb, uint32_t EPNum, uint8_t *pData, uint32_t cnt)
{
  USB_CORE_CTRL_T* pCtrl = (USB_CORE_CTRL_T*)hUsb;
  USBD_HW_DATA_T* drv = (USBD_HW_DATA_T*)pCtrl->hw_data;
  uint32_t n = USB_EP_BITPOS(EPNum);

  /* make buffer cache coherent*/
  if (pCtrl->cache_flush)
    pCtrl->cache_flush((uint32_t*)pData, (uint32_t*)(pData + cnt));

  if ((drv->regs->endptstatus & _BIT(n)) == 0) {
	  hwUSB_ProgDTD(hUsb, EPAdr(EPNum), (uint32_t)pData, cnt);
	  /* prime the endpoint for transmit */
	  drv->regs->endptprime = _BIT(n) ;

	  /* check if priming succeeded */
	  while (drv->regs->endptprime & _BIT(n));
  }
  else {
	cnt = 0;
  }

  return (cnt);
}

/**
 * @brief   Get memory required by DFU class.
 * @param [in/out] param parameter structure used for initialisation.
 * @retval  Length required for DFU data structure and buffers.
 *
 * Example Usage:
 * @code
 *    mem_req = mwDFU_GetMemSize(param); 
 * @endcode
 */

uint32_t hwUSB_GetMemSize(USBD_API_INIT_PARAM_T* param)
{
  uint32_t req_len = 0;

  /* calculate required length */
  req_len += ((2 * param->max_num_ep) * sizeof(DQH_T));  /* ep queue heads */
  req_len += ((2 * param->max_num_ep) * sizeof(DTD_T));  /* ep transfer descriptors */
  req_len += sizeof(USBD_HW_DATA_T);   /* memory for hw driver data structure */
  req_len += sizeof(USB_CORE_CTRL_T);  /* memory for USBD controller structure */
  req_len += 8; /* for alignment overhead */
  req_len &= ~0x7;

  return req_len;
}

/*
*  USB Initialize Function
*   Called by the User to initialize USB
*    Return Value:    None
*/

ErrorCode_t hwUSB_Init(USBD_HANDLE_T* phUsb, USB_CORE_DESCS_T* pDesc, USBD_API_INIT_PARAM_T* param)
{
  DQH_T* ep_QH;
  DTD_T* ep_TD;
  USBD_HW_DATA_T* drv;
  USB_CORE_CTRL_T* pCtrl;

  /* check for memory alignment */
  if ((param->mem_base &  (2048 - 1)) && 
    (param->mem_size < hwUSB_GetMemSize(param)))
    return ERR_USBD_BAD_MEM_BUF;

  /* allocate memory for ep_QH which should be on 2048 alignment */
  ep_QH = (DQH_T*)param->mem_base;
  param->mem_base += ((2 * param->max_num_ep) * sizeof(DQH_T));
  param->mem_size -= ((2 * param->max_num_ep) * sizeof(DQH_T));

  /* allocate memory for ep_TD which should be on 32 byte alignment.*/
  ep_TD = (DTD_T*)param->mem_base;
  param->mem_base += ((2 * param->max_num_ep) * sizeof(DTD_T));
  param->mem_size -= ((2 * param->max_num_ep) * sizeof(DTD_T));
  /* allocate memory for hardware driver data structure */
  drv = (USBD_HW_DATA_T*)param->mem_base;
  param->mem_base += sizeof(USBD_HW_DATA_T);
  param->mem_size -= sizeof(USBD_HW_DATA_T);
  /* align to 4 byte boundary */
  while (param->mem_base & 0x03) param->mem_base++;
  /* allocate memory for USBD controller data structure */
  pCtrl = (USB_CORE_CTRL_T*)param->mem_base;
  param->mem_base += sizeof(USB_CORE_CTRL_T);
  param->mem_size -= sizeof(USB_CORE_CTRL_T);
  /* align to 4 byte boundary */
  while (param->mem_base & 0x03) param->mem_base++;

  /* now init USBD stack */
  mwUSB_InitCore(pCtrl, pDesc, param);

  /* now initialize data structures */
  memset((void*)drv, 0, sizeof(USBD_HW_DATA_T));
  /* set stack control and hw control pointer */
  drv->pCtrl = pCtrl;
  pCtrl->hw_data = (void*)drv;
  /* set up regs */
  drv->regs  = (USB_OTG_REGS_T* )param->usb_reg_base;

  /* check if the eP_QH are allocated in cached region. If so get the
   * uncached address value.
   */
  if (param->virt_to_phys)  {
    drv->ep_QH = (DQH_T*)param->virt_to_phys((void*) &ep_QH[0]);
    drv->ep_TD = (DTD_T*)param->virt_to_phys((void*) &ep_TD[0]);
  }
  else {
    drv->ep_QH = ep_QH;
    drv->ep_TD = ep_TD;
  }

  /* reset the controller */
  drv->regs->usbcmd = USBCMD_RST;
  /* wait for reset to complete */
  while (drv->regs->usbcmd & USBCMD_RST);

  /* Program the controller to be the USB device controller */
  drv->regs->usbmode =   USBMODE_CM_DEV
                       /*| USBMODE_SDIS*/
                       | USBMODE_SLOM ;

  /* set OTG transcever in proper state */
  drv->regs->otgsc = _BIT(3) /*| _BIT(0)*/;

  hwUSB_Reset(pCtrl);
  hwUSB_SetAddress(pCtrl, 0);

  /* return the handle */
  *phUsb = (USBD_HANDLE_T)pCtrl;

  return LPC_OK;
}


/*
*  USB Interrupt Service Routine
*/

void hwUSB_ISR(USBD_HANDLE_T hUsb)
{
  USB_CORE_CTRL_T* pCtrl = (USB_CORE_CTRL_T*)hUsb;
  USBD_HW_DATA_T* drv = (USBD_HW_DATA_T*)pCtrl->hw_data;
  uint32_t disr, val, n, ep_indx;

  disr = drv->regs->usbsts;                      /* Device Interrupt Status */
  drv->regs->usbsts = disr;
  /* lets handle events we are interested in */
  disr = disr & drv->regs->usbintr;

  /* Device Status Interrupt (Reset, Connect change, Suspend/Resume) */
  if (disr & USBSTS_URI)                      /* Reset */
  {
    hwUSB_Reset(hUsb);
    mwUSB_ResetCore(pCtrl);
    if (pCtrl->USB_Reset_Event)
      pCtrl->USB_Reset_Event(hUsb);
    goto isr_end;
  }

  if (disr & USBSTS_SLI)                   /* Suspend */
  {
    /*drv->regs->otgsc &= ~_BIT(0);*/
    if (pCtrl->USB_Suspend_Event)
      pCtrl->USB_Suspend_Event(hUsb);
  }

  if (disr & USBSTS_PCI)                  /* Resume */
  {
    /* check if device isoperating in HS mode or full speed */
    if (drv->regs->portsc1 & _BIT(9))
      USB_SetSpeedMode(pCtrl, USB_HIGH_SPEED);  

    /*drv->regs->otgsc |= _BIT(0);*/

    if (pCtrl->USB_Resume_Event)
      pCtrl->USB_Resume_Event(hUsb);
  }

  /* handle setup status interrupts */
  val = drv->regs->endptsetupstat;
  /* Only EP0 will have setup packets so call EP0 handler */
  if (val)
  {
    /* Clear the endpoint complete CTRL OUT & IN when */
    /* a Setup is received */
    drv->regs->endptcomplete = 0x00010001;
    /* enable NAK inetrrupts */
    drv->regs->endptnaken |= 0x00010001;
    if (pCtrl->ep_event_hdlr[0])
      pCtrl->ep_event_hdlr[0](pCtrl, pCtrl->ep_hdlr_data[0], USB_EVT_SETUP);
  }

  /* handle completion interrupts */
  val = drv->regs->endptcomplete;
  ep_indx = 0;
  if (val)
  {
    drv->regs->endptnak = val;
    for (n = 0; n < pCtrl->max_num_ep; n++)
    {
      if (val & _BIT(n))
      {
        if (pCtrl->ep_event_hdlr[ep_indx])
          pCtrl->ep_event_hdlr[ep_indx](pCtrl, pCtrl->ep_hdlr_data[ep_indx], USB_EVT_OUT);

        drv->regs->endptcomplete = _BIT(n);
      }
      if (val & _BIT(n + 16))
      {
        drv->ep_TD [(n << 1) + 1 ].total_bytes &= 0xC0;
        if (pCtrl->ep_event_hdlr[ep_indx + 1])
          pCtrl->ep_event_hdlr[ep_indx + 1](pCtrl, pCtrl->ep_hdlr_data[ep_indx + 1], USB_EVT_IN);

        drv->regs->endptcomplete = _BIT(n + 16);
      }
      ep_indx += 2;
    }
  }

  if (disr & USBSTS_NAKI)
  {
    val = drv->regs->endptnak;
    val &= drv->regs->endptnaken;
    /* handle NAK interrupts */
    ep_indx = 0;
    if (val)
    {
      for (n = 0; n < pCtrl->max_num_ep; n++)
      {

        if (val & _BIT(n))
        {
          if (pCtrl->ep_event_hdlr[ep_indx])
            pCtrl->ep_event_hdlr[ep_indx](pCtrl, pCtrl->ep_hdlr_data[ep_indx], USB_EVT_OUT_NAK);
        }
        if (val & _BIT(n + 16))
        {
          if (pCtrl->ep_event_hdlr[ep_indx + 1])
            pCtrl->ep_event_hdlr[ep_indx + 1](pCtrl, pCtrl->ep_hdlr_data[ep_indx + 1], USB_EVT_IN_NAK);
        }
        ep_indx += 2;
      }
      drv->regs->endptnak = val;
    }
  }

  /* Start of Frame Interrupt */
  if (disr & USBSTS_SRI)
  {
    if (pCtrl->USB_SOF_Event)
      pCtrl->USB_SOF_Event(hUsb);
  }

  /* Error Interrupt */
  if (disr & USBSTS_UEI)
  {
    if (pCtrl->USB_Error_Event)
      pCtrl->USB_Error_Event(hUsb, disr);
  }

isr_end:
  return;
}

/**
 * @}
 */
