/***********************************************************************
 * $Id:: mw_usbd_hiduser.c 1728 2014-09-19 17:56:35Z nxp73930                  $
 *
 * Project: USB device ROM Stack
 *
 * Description:
 *     HID Custom User Module.
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

#include <string.h>
#include "mw_usbd.h"
#include "mw_usbd_core.h"
#include "mw_usbd_hw.h"
#include "mw_usbd_hid.h"
#include "mw_usbd_hiduser.h"

/*
 *  HID Get report descriptor Request Callback
 *   Called automatically on HID Get report descriptor
 *    Parameters:      hHid: Handle to the HID structure.
 *										 pSetup: Pointer to setup packet received from host.
 *										 pBuf: Pointer to a pointer of data buffer containing report descriptor.
 *                                       length:  Amount of data copied to destination buffer.
 *    Return Value:    ErrorCode_t type to indicate success or error condition.
 *                               LPC_OK On success.
 *                               ERR_USBD_UNHANDLED  Event is not handled hence pass the event to next in line.
 *                               ERR_USBD_xxx  For other error conditions
 */

ErrorCode_t mwHID_GetReportDesc(USBD_HANDLE_T hHid, USB_SETUP_PACKET *pSetup, uint8_t * *pBuf, uint16_t *plength) {
	USB_HID_CTRL_T *pHidCtrl = (USB_HID_CTRL_T *) hHid;
	*pBuf = pHidCtrl->report_data[pSetup->wValue.WB.L].desc;
	*plength = pHidCtrl->report_data[pSetup->wValue.WB.L].len;

	return LPC_OK;
}

/*
 *  HID Get Idle Request Callback
 *   Called automatically on HID Get Idle Request
 *    Parameters:      pHidCtrl: Handle to the HID structure (global SetupPacket and EP0Buf)
 *    Return Value:    ErrorCode_t type to indicate success or error condition.
 */

ErrorCode_t mwHID_GetIdle(USB_HID_CTRL_T *pHidCtrl) {

	USB_CORE_CTRL_T *pCtrl = pHidCtrl->pUsbCtrl;
	pCtrl->EP0Buf[0] = pHidCtrl->report_data[pCtrl->SetupPacket.wValue.WB.L].idle_time;
	return LPC_OK;
}

/*
 *  HID Set Idle Request Callback
 *   Called automatically on HID Set Idle Request
 *    Parameters:      pHidCtrl: Handle to the HID structure (global SetupPacket)
 *    Return Value:    ErrorCode_t type to indicate success or error condition.
 *                               LPC_OK On success.
 *                               ERR_USBD_UNHANDLED  Event is not handled hence pass the event to next in line.
 *                               ERR_USBD_xxx  For other error conditions
 */

ErrorCode_t mwHID_SetIdle(USB_HID_CTRL_T *pHidCtrl) {

	USB_CORE_CTRL_T *pCtrl = pHidCtrl->pUsbCtrl;
	uint8_t idleTime =  pCtrl->SetupPacket.wValue.WB.H;

	pHidCtrl->report_data[pCtrl->SetupPacket.wValue.WB.L].idle_time = idleTime;

	/* Idle Handling if needed */
	if (pHidCtrl->HID_SetIdle) {
		return pHidCtrl->HID_SetIdle(pHidCtrl, &pCtrl->SetupPacket, idleTime);
	}

	return LPC_OK;
}

/*
 *  HID Get Protocol Request Callback
 *   Called automatically on HID Get Protocol Request
 *    Parameters:      pHidCtrl: Handle to the HID structure (global SetupPacket)
 *    Return Value:    ErrorCode_t type to indicate success or error condition.
 */

ErrorCode_t mwHID_GetProtocol(USB_HID_CTRL_T *pHidCtrl) {

	pHidCtrl->pUsbCtrl->EP0Buf[0] = pHidCtrl->protocol;
	return LPC_OK;
}

/*
 *  HID Set Protocol Request Callback
 *   Called automatically on HID Set Protocol Request
 *    Parameters:      pHidCtrl: Handle to the HID structure (global SetupPacket)
 *    Return Value:    ErrorCode_t type to indicate success or error condition.
 */

ErrorCode_t mwHID_SetProtocol(USB_HID_CTRL_T *pHidCtrl) {

	pHidCtrl->protocol = pHidCtrl->pUsbCtrl->SetupPacket.wValue.WB.L;

	/* Protocol Handling if needed */
	if (pHidCtrl->HID_SetProtocol) {
		return pHidCtrl->HID_SetProtocol(pHidCtrl, &pHidCtrl->pUsbCtrl->SetupPacket, pHidCtrl->protocol);
	}

	return LPC_OK;
}

/*
 *  Default HID Class Handler
 *  Parameters:     hUsb: Handle to the USB device stack.
 *                                  data: Pointer to the data which will be passed when callback function is called by the stack.
 *                                  event:  Type of endpoint event. See \ref USBD_EVENT_T for more details.
 *  Return Value:    ErrorCode_t type to indicate success or error condition.
 */

ErrorCode_t mwHID_ep0_hdlr(USBD_HANDLE_T hUsb, void *data, uint32_t event)
{
	ErrorCode_t ret = ERR_USBD_UNHANDLED;
	USB_CORE_CTRL_T *pCtrl = (USB_CORE_CTRL_T *) hUsb;
	USB_HID_CTRL_T *pHidCtrl = (USB_HID_CTRL_T *) data;
	uint16_t len = 0;
	uint8_t *buff;

	/* Check if the request is for this instance of interface. IF not return immediately. */
	if ((pCtrl->SetupPacket.wIndex.WB.L != pHidCtrl->if_num)) {
		return ret;
	}

	switch (event) {
	case USB_EVT_SETUP:
		if ((pCtrl->SetupPacket.bmRequestType.BM.Recipient == REQUEST_TO_INTERFACE) ) {

			if (pCtrl->SetupPacket.bmRequestType.BM.Type == REQUEST_CLASS) {
				switch (pCtrl->SetupPacket.bRequest) {
				case HID_REQUEST_GET_REPORT:
					pCtrl->EP0Data.pData = pCtrl->EP0Buf;						/* point to data to be sent */
					/* allow user to copy data to EP0Buf or change the pointer to his own buffer */
					ret = pHidCtrl->HID_GetReport(pHidCtrl, &pCtrl->SetupPacket,
												  &pCtrl->EP0Data.pData, &pCtrl->EP0Data.Count);
					if (ret == LPC_OK) {
						mwUSB_DataInStage(pCtrl);							/* send requested data */
					}
					break;

				case HID_REQUEST_SET_REPORT:
					/* ask user where to recieve the report */
					pCtrl->EP0Data.pData = pCtrl->EP0Buf;				/* data to be received */
					ret = pHidCtrl->HID_SetReport(pHidCtrl, &pCtrl->SetupPacket,
												  &pCtrl->EP0Data.pData, 0);
					break;

				case HID_REQUEST_GET_IDLE:
					ret = mwHID_GetIdle(pHidCtrl);
					if (ret == LPC_OK) {
						pCtrl->EP0Data.pData = pCtrl->EP0Buf;					/* point to data to be sent */
						mwUSB_DataInStage(pCtrl);							/* send requested data */
					}
					break;

				case HID_REQUEST_SET_IDLE:
					ret = mwHID_SetIdle(pHidCtrl);
					if ( ret == LPC_OK) {
						mwUSB_StatusInStage(pCtrl);							/* send Acknowledge */
					}
					break;

				case HID_REQUEST_GET_PROTOCOL:
					ret = mwHID_GetProtocol(pHidCtrl);
					if ( ret == LPC_OK) {
						pCtrl->EP0Data.pData = pCtrl->EP0Buf;					/* point to data to be sent */
						mwUSB_DataInStage(pCtrl);							/* send requested data */
					}
					break;

				case HID_REQUEST_SET_PROTOCOL:
					ret = mwHID_SetProtocol(pHidCtrl);
					if ( ret == LPC_OK) {
						mwUSB_StatusInStage(pCtrl);							/* send Acknowledge */
					}
					break;

				default:
					break;
				}
			}
			else if ((pCtrl->SetupPacket.bmRequestType.BM.Type == REQUEST_STANDARD) &&
					 (pCtrl->SetupPacket.bRequest == USB_REQUEST_GET_DESCRIPTOR)) {
				switch (pCtrl->SetupPacket.wValue.WB.H) {
				case HID_HID_DESCRIPTOR_TYPE:
					pCtrl->EP0Data.pData = pHidCtrl->hid_desc;
					len = ((USB_COMMON_DESCRIPTOR *) pHidCtrl->hid_desc)->bLength;
					ret = LPC_OK;
					break;

				case HID_REPORT_DESCRIPTOR_TYPE:
					ret = pHidCtrl->HID_GetReportDesc(pHidCtrl, &pCtrl->SetupPacket,
													  &pCtrl->EP0Data.pData, &len);
					break;

				case HID_PHYSICAL_DESCRIPTOR_TYPE:
					if (pHidCtrl->HID_GetPhysDesc == 0) {
						ret = (ERR_USBD_STALL);	/* HID Physical Descriptor is not supported */
					}
					else {
						ret = pHidCtrl->HID_GetPhysDesc(pHidCtrl, &pCtrl->SetupPacket,
														&pCtrl->EP0Data.pData, &len);
					}
					break;
				}
				if (ret == LPC_OK) {
					if (pCtrl->EP0Data.Count > len) {
						pCtrl->EP0Data.Count = len;
					}

					mwUSB_DataInStage(pCtrl);
				}
			}
		}
		break;

	case USB_EVT_OUT:
		if ((pCtrl->SetupPacket.bmRequestType.BM.Type == REQUEST_CLASS) &&
			(pCtrl->SetupPacket.bmRequestType.BM.Recipient == REQUEST_TO_INTERFACE)) {
			switch (pCtrl->SetupPacket.bRequest) {
			case HID_REQUEST_SET_REPORT:
				buff = pCtrl->EP0Data.pData - pCtrl->SetupPacket.wLength;
				ret = pHidCtrl->HID_SetReport(pHidCtrl, &pCtrl->SetupPacket, &buff,
											  pCtrl->SetupPacket.wLength);
				if ( ret == LPC_OK) {

					/* check if user wants us to use his buffer address instead of EP0Buff */
					if (buff != pCtrl->EP0Buf) {
						pCtrl->EP0Data.pData = buff;
					}
					else {
						pCtrl->EP0Data.pData = pCtrl->EP0Buf;
					}

					mwUSB_StatusInStage(pCtrl);						/* send Acknowledge */
				}
				break;
			}
		}
		break;

	default:
		break;
	}
	return ret;
}

/**
 * @brief   Get memory required by HID class.
 * @param [in/out] param parameter structure used for initialisation.
 * @retval  Length required for HID data structure and buffers.
 *
 * Example Usage:
 * @code
 *    mem_req = mwHID_GetMemSize(param);
 * @endcode
 */
uint32_t mwHID_GetMemSize(USBD_HID_INIT_PARAM_T *param)
{
	uint32_t req_len = 0;

	/* calculate required length */
	req_len += sizeof(USB_HID_CTRL_T);	/* memory for HID controller structure */
	req_len += param->max_reports * sizeof(USB_HID_REPORT_T);
	req_len += 8;	/* for alignment overhead */
	req_len &= ~0x7;

	return req_len;
}

/*
 *  HID function initialization routine
 *  Parameters:     hUsb: Handle to the USB device stack.
 *                                  param: Structure containing HID function driver module
 *						      initialization parameters.
 *  Return Value:   ErrorCode_t type to indicate success or error condition.
 */

ErrorCode_t mwHID_init(USBD_HANDLE_T hUsb, USBD_HID_INIT_PARAM_T *param)
{
	uint32_t new_addr, i, ep_indx;
	ErrorCode_t ret = LPC_OK;
	USB_HID_CTRL_T *pHidCtrl;
	HID_DESCRIPTOR *pHidDesc;
	USB_ENDPOINT_DESCRIPTOR *pEpDesc;
	USB_INTERFACE_DESCRIPTOR *pIntfDesc = (USB_INTERFACE_DESCRIPTOR *) param->intf_desc;

	/* check for memory alignment */
	if ((param->mem_base &  0x3) &&
		(param->mem_size < (sizeof(USB_HID_CTRL_T) + (param->max_reports * sizeof(USB_HID_REPORT_T))))) {
		return ERR_USBD_BAD_MEM_BUF;
	}

	/* allocate memory for the control data structure */
	pHidCtrl = (USB_HID_CTRL_T *) param->mem_base;
	param->mem_base += sizeof(USB_HID_CTRL_T);
	param->mem_size -= sizeof(USB_HID_CTRL_T);

	/* Init control structures with passed params */
	memset((void *) pHidCtrl, 0, sizeof(USB_HID_CTRL_T));
	/* store handle to USBD stack */
	pHidCtrl->pUsbCtrl = (USB_CORE_CTRL_T *) hUsb;
	/* allocate report array */
	pHidCtrl->report_data = (USB_HID_REPORT_T *) param->mem_base;
	param->mem_base += param->max_reports * sizeof(USB_HID_REPORT_T);
	param->mem_size -= param->max_reports * sizeof(USB_HID_REPORT_T);
	/* align to 4 byte boundary */
	while (param->mem_base & 0x03) {
		param->mem_base++;
		param->mem_size--;
	}
	/* init report data  */
	memset((void *) pHidCtrl->report_data, 0, param->max_reports * sizeof(USB_HID_REPORT_T));
	for (i = 0; i < param->max_reports; i++) {
		memcpy(&pHidCtrl->report_data[i], &param->report_data[i], sizeof(USB_HID_REPORT_T));
	}

	/* user defined functions */
	if ((param->HID_GetReport == 0) ||
		(param->HID_SetReport == 0) ) {
		return ERR_API_INVALID_PARAM2;
	}

	pHidCtrl->HID_GetReport = param->HID_GetReport;
	pHidCtrl->HID_SetReport = param->HID_SetReport;
	pHidCtrl->HID_GetPhysDesc = param->HID_GetPhysDesc;
	pHidCtrl->HID_SetIdle = param->HID_SetIdle;
	pHidCtrl->HID_SetProtocol = param->HID_SetProtocol;
	pHidCtrl->HID_GetReportDesc = (param->HID_GetReportDesc) ?
								  param->HID_GetReportDesc : mwHID_GetReportDesc;

	/* parse the interface descriptor */
	if ((pIntfDesc->bDescriptorType == USB_INTERFACE_DESCRIPTOR_TYPE) &&
		(pIntfDesc->bInterfaceClass == USB_DEVICE_CLASS_HUMAN_INTERFACE) ) {

		/* store interface number */
		pHidCtrl->if_num = pIntfDesc->bInterfaceNumber;
		pHidCtrl->protocol = pIntfDesc->bInterfaceProtocol;

		/* move to next descriptor */
		new_addr = (uint32_t) pIntfDesc + pIntfDesc->bLength;
		/* now we should have HID descriptor */
		pHidDesc = (HID_DESCRIPTOR *) new_addr;
		if (pHidDesc->bDescriptorType != HID_HID_DESCRIPTOR_TYPE) {
			return ERR_USBD_BAD_DESC;
		}

		/* store HID descriptor address */
		pHidCtrl->hid_desc = (uint8_t *) new_addr;

		/* move to next descriptor */
		new_addr += pHidDesc->bLength;

		for (i = 0; i < pIntfDesc->bNumEndpoints; i++) {
			pEpDesc = (USB_ENDPOINT_DESCRIPTOR *) new_addr;
			new_addr = (uint32_t) pEpDesc + pEpDesc->bLength;

			/* parse endpoint descriptor */
			if ((pEpDesc->bDescriptorType == USB_ENDPOINT_DESCRIPTOR_TYPE) &&
				(pEpDesc->bmAttributes == USB_ENDPOINT_TYPE_INTERRUPT)) {

				if (pEpDesc->bEndpointAddress & USB_ENDPOINT_DIRECTION_MASK) {
					/* store Interrupt IN endpoint */
					pHidCtrl->epin_adr = pEpDesc->bEndpointAddress;
					ep_indx = ((pHidCtrl->epin_adr & 0x0F) << 1) + 1;
					/* register endpoint interrupt handler if provided*/
					if (param->HID_EpIn_Hdlr != 0) {
						ret = mwUSB_RegisterEpHandler(hUsb, ep_indx, param->HID_EpIn_Hdlr, pHidCtrl);
					}
				}
				else {
					/* store Interrupt OUT endpoint */
					pHidCtrl->epout_adr = pEpDesc->bEndpointAddress;
					ep_indx = ((pHidCtrl->epout_adr & 0x0F) << 1);
					/* register endpoint interrupt handler if provided*/
					if (param->HID_EpOut_Hdlr != 0) {
						ret = mwUSB_RegisterEpHandler(hUsb, ep_indx, param->HID_EpOut_Hdlr, pHidCtrl);
					}
				}
			}
		}
	}
	else {
		return ERR_USBD_BAD_INTF_DESC;
	}

	if (ret != LPC_OK) {
		return ERR_USBD_BAD_EP_DESC;
	}

	/* register ep0 handler */
	/* check if user wants his own handler */
	if (param->HID_Ep0_Hdlr == 0) {
		ret = mwUSB_RegisterClassHandler(hUsb, mwHID_ep0_hdlr, pHidCtrl);
	}
	else {
		ret = mwUSB_RegisterClassHandler(hUsb, param->HID_Ep0_Hdlr, pHidCtrl);
		param->HID_Ep0_Hdlr = mwHID_ep0_hdlr;
	}

	return ret;
}
