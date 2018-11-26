/***********************************************************************
 * $Id:: mw_usbd_core.c 2221 2015-12-17 23:28:25Z usb00423                     $
 *
 * Project: USB device ROM Stack
 *
 * Description:
 *     USB Core Module.
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
#include "mw_usbd_hw.h"
#include "mw_usbd_core.h"
#include "mw_usbd_desc.h"

#ifndef FALSE
#define FALSE 0
#define TRUE !FALSE
#endif

/* HW_API_T structure */
extern const USBD_HW_API_T hw_api;

/* forward function declarations */
ErrorCode_t USB_InvokeEp0Hdlrs (USB_CORE_CTRL_T *pCtrl, uint32_t event);

/*
 *  Reset USB Core
 *  Parameters:      hUsb: Handle to the USB device stack.
 *  Return Value:    None
 */

void mwUSB_ResetCore(USBD_HANDLE_T hUsb)
{
	USB_CORE_CTRL_T *pCtrl = (USB_CORE_CTRL_T *) hUsb;

	/* Below device_status update is to address the USB 2.0 device failure issue in USB3.0 CV test
	   when BUS_POWERed device is set in the descriptor. The easier way to set "pCtrl->device_status = 0;",
	   then the problem will go away. If so, we don't have to find which descriptor to use and what
	   bmAttributes is. */
	pCtrl->device_status = 0;

	/* CHIRP occurs during the USB RESET. Thus, the initial USB speed is FULL. */
	pCtrl->device_speed = USB_FULL_SPEED;
	pCtrl->device_addr = 0;
	pCtrl->config_value = 0;
	/* enable EP0 by default */
	pCtrl->ep_mask  = 0x00010001;
	pCtrl->ep_halt  = 0x00000000;
	pCtrl->ep_stall = 0x00000000;
	/* inform all class handler about reset event*/
	USB_InvokeEp0Hdlrs(pCtrl, USB_EVT_RESET);
}

/*
 *  USB Request - Setup Stage
 *  Parameters:      hUsb: Handle to the USB device stack. (global pCtrl->SetupPacket)
 *  Return Value:    None
 */

void mwUSB_SetupStage(USBD_HANDLE_T hUsb)
{
	USB_CORE_CTRL_T *pCtrl = (USB_CORE_CTRL_T *) hUsb;
	pCtrl->hw_api->ReadSetupPkt(pCtrl, 0x00, (uint32_t *) &pCtrl->SetupPacket);
}

/*
 *  USB Request - Data In Stage
 *  Parameters:      hUsb: Handle to the USB device stack. (global EP0Data)
 *  Return Value:    None
 */

void mwUSB_DataInStage(USBD_HANDLE_T hUsb)
{
	USB_CORE_CTRL_T *pCtrl = (USB_CORE_CTRL_T *) hUsb;
	uint32_t cnt;

	if (pCtrl->EP0Data.Count > (((USB_DEVICE_DESCRIPTOR *) (pCtrl->device_desc))->bMaxPacketSize0)) {
		cnt = ((USB_DEVICE_DESCRIPTOR *) (pCtrl->device_desc))->bMaxPacketSize0;
	}
	else {
		cnt = pCtrl->EP0Data.Count;
	}
	cnt = pCtrl->hw_api->WriteEP(pCtrl, 0x80, pCtrl->EP0Data.pData, cnt);
	pCtrl->EP0Data.pData += cnt;
	pCtrl->EP0Data.Count -= cnt;
}

/*
 *  USB Request - Data Out Stage
 *  Parameters:      hUsb: Handle to the USB device stack. (global pCtrl->EP0Data)
 *  Return Value:    None
 */

void mwUSB_DataOutStage(USBD_HANDLE_T hUsb)
{
	USB_CORE_CTRL_T *pCtrl = (USB_CORE_CTRL_T *) hUsb;
	uint32_t cnt;

	cnt = pCtrl->hw_api->ReadEP(pCtrl, 0x00, pCtrl->EP0Data.pData);
	pCtrl->EP0Data.pData += cnt;
	pCtrl->EP0Data.Count -= cnt;
}

/*
 *  USB Request - Status In Stage
 *  Parameters:      hUsb: Handle to the USB device stack.
 *  Return Value:    None
 */

void mwUSB_StatusInStage(USBD_HANDLE_T hUsb)
{
	USB_CORE_CTRL_T *pCtrl = (USB_CORE_CTRL_T *) hUsb;
	pCtrl->hw_api->WriteEP(pCtrl, 0x80, 0, 0);
}

/*
 *  USB Request - Status Out Stage
 *  Parameters:      hUsb: Handle to the USB device stack.
 *  Return Value:    None
 */

void mwUSB_StatusOutStage(USBD_HANDLE_T hUsb)
{
	USB_CORE_CTRL_T *pCtrl = (USB_CORE_CTRL_T *) hUsb;
	pCtrl->hw_api->ReadEP(pCtrl, 0x00, pCtrl->EP0Buf);
}

/*
 *  USB stall ep0 for invalid request
 *  Parameters:      hUsb: Handle to the USB device stack.
 *  Return Value:    None
 */

void mwUSB_StallEp0(USBD_HANDLE_T hUsb)
{
	USB_CORE_CTRL_T *pCtrl = (USB_CORE_CTRL_T *) hUsb;
	pCtrl->hw_api->SetStallEP(pCtrl, 0x80);
	pCtrl->EP0Data.Count = 0;
}

/*
 *  Get Status USB Request
 *  Parameters:      hUsb: Handle to the USB device stack. (global pCtrl->SetupPacket)
 *  Return Value:    ErrorCode_t type to indicate success or error condition.
 */

ErrorCode_t USB_ReqGetStatus(USBD_HANDLE_T hUsb)
{
	USB_CORE_CTRL_T *pCtrl = (USB_CORE_CTRL_T *) hUsb;
	uint32_t n, m;
	ErrorCode_t ret = ERR_USBD_INVALID_REQ;

	/* Added ROOT2 compliance test support. */
	if ( (pCtrl->device_addr & 0x7F) == 0x0 ) {
		return ERR_USBD_INVALID_REQ;
	}
	if ( (pCtrl->SetupPacket.wValue.W != 0) || (pCtrl->SetupPacket.wLength != 2) ) {
		return ERR_USBD_INVALID_REQ;
	}

	switch (pCtrl->SetupPacket.bmRequestType.BM.Recipient) {
	case REQUEST_TO_DEVICE:
		if (pCtrl->SetupPacket.wIndex.W == 0) {
			pCtrl->EP0Data.pData = (uint8_t *) &pCtrl->device_status;
			ret = LPC_OK;
		}
		break;

	case REQUEST_TO_INTERFACE:
		/* Added ROOT2 compliance test support. */
		if ((pCtrl->config_value != 0) && (pCtrl->SetupPacket.wIndex.WB.L < pCtrl->num_interfaces) &&
			(pCtrl->SetupPacket.wIndex.WB.H == 0x0)) {
			*((uint16_t *) pCtrl->EP0Buf) = 0;
			pCtrl->EP0Data.pData = pCtrl->EP0Buf;
			ret = LPC_OK;
		}
		break;

	case REQUEST_TO_ENDPOINT:
		n = pCtrl->SetupPacket.wIndex.WB.L & 0x8F;
		m = (n & 0x80) ? ((1 << 16) << (n & 0x0F)) : (1 << n);
		if (((pCtrl->config_value != 0) || ((n & 0x0F) == 0)) && (pCtrl->ep_mask & m)) {
			*((uint16_t *) pCtrl->EP0Buf) = (pCtrl->ep_halt & m) ? 1 : 0;
			pCtrl->EP0Data.pData = pCtrl->EP0Buf;
			ret = LPC_OK;
		}
		break;

	default:
		break;
	}
	return ret;
}

/*
 *  Set/Clear Feature USB Request
 *  Parameters:      hUsb: Handle to the USB device stack.
 *										sc:    0 - Clear, 1 - Set
 *                            (global pCtrl->SetupPacket)
 *  Return Value:    ErrorCode_t type to indicate success or error condition.
 */

ErrorCode_t USB_ReqSetClrFeature(USBD_HANDLE_T hUsb, uint32_t sc)
{
	USB_CORE_CTRL_T *pCtrl = (USB_CORE_CTRL_T *) hUsb;
	uint32_t n, m;
	ErrorCode_t ret = ERR_USBD_INVALID_REQ;

	/* Added ROOT2 compliance test support. */
	if ( (pCtrl->device_addr & 0x7F) == 0x0 ) {
		return ERR_USBD_INVALID_REQ;
	}

	switch (pCtrl->SetupPacket.bmRequestType.BM.Recipient) {
	case REQUEST_TO_DEVICE:
		if (pCtrl->SetupPacket.wValue.W == USB_FEATURE_REMOTE_WAKEUP) {
			if (pCtrl->USB_WakeUpCfg) {
				/* User defined wake up configuration. */
				pCtrl->USB_WakeUpCfg(pCtrl, sc);
			}
			else {
				pCtrl->hw_api->WakeUpCfg(hUsb, sc);
			}

			if (sc) {
				pCtrl->device_status |=  USB_GETSTATUS_REMOTE_WAKEUP;
			}
			else {
				pCtrl->device_status &= ~USB_GETSTATUS_REMOTE_WAKEUP;
			}
			ret = LPC_OK;
		}
		else if (pCtrl->SetupPacket.wValue.W == USB_FEATURE_TEST_MODE) {
			ret = pCtrl->hw_api->SetTestMode(pCtrl, pCtrl->SetupPacket.wIndex.WB.H);
		}
		break;

	case REQUEST_TO_ENDPOINT:
		n = pCtrl->SetupPacket.wIndex.WB.L & 0x8F;
		m = (n & 0x80) ? ((1 << 16) << (n & 0x0F)) : (1 << n);
		if ((pCtrl->config_value != 0) && ((n & 0x0F) != 0) && (pCtrl->ep_mask & m)) {
			if (pCtrl->SetupPacket.wValue.W == USB_FEATURE_ENDPOINT_STALL) {
				if (sc) {
					pCtrl->hw_api->SetStallEP(pCtrl, n);
					pCtrl->ep_halt |=  m;
				}
				else if ((pCtrl->ep_stall & m) == 0) {
					pCtrl->hw_api->ClrStallEP(pCtrl, n);
					pCtrl->ep_halt &= ~m;
				}
				ret = LPC_OK;
			}
		}
		break;

	// case REQUEST_TO_INTERFACE:
	default:
		break;
	}
	return ret;
}

/*
 *  Set Address USB Request
 *  Parameters:      pCtrl: Handle to Core Control Structure (global pCtrl->SetupPacket)
 *  Return Value:    ErrorCode_t type to indicate success or error condition.
 */

ErrorCode_t USB_ReqSetAddress(USB_CORE_CTRL_T *pCtrl)
{
	/* Added ROOT2 compliance test support. */
	if ( (pCtrl->SetupPacket.wIndex.W != 0) || (pCtrl->SetupPacket.wLength != 0) ) {
		return ERR_USBD_INVALID_REQ;
	}
	if (pCtrl->config_value != 0) {
		return ERR_USBD_INVALID_REQ;
	}
	if (pCtrl->SetupPacket.bmRequestType.BM.Recipient == REQUEST_TO_DEVICE) {
		/* Added ROOT2 compliance test support. */
		if ( pCtrl->SetupPacket.wValue.WB.L > 0x7F ) {
			return ERR_USBD_INVALID_REQ;
		}
		pCtrl->device_addr = 0x80 | pCtrl->SetupPacket.wValue.WB.L;
		return LPC_OK;
	}
	return ERR_USBD_INVALID_REQ;
}

/*
 *  Get Descriptor USB Request
 *  Parameters:      hUsb: Handle to the USB device stack. (global pCtrl->SetupPacket)
 *  Return Value:    ErrorCode_t type to indicate success or error condition.
 */

ErrorCode_t USB_ReqGetDescriptor(USBD_HANDLE_T hUsb)
{
	USB_CORE_CTRL_T *pCtrl = (USB_CORE_CTRL_T *) hUsb;
	uint8_t  *pD = NULL;
	uint32_t len, n;

	switch (pCtrl->SetupPacket.bmRequestType.BM.Recipient) {
	case REQUEST_TO_DEVICE:
		switch (pCtrl->SetupPacket.wValue.WB.H) {
		case USB_DEVICE_DESCRIPTOR_TYPE:
			pCtrl->EP0Data.pData = (uint8_t *) pCtrl->device_desc;
			len = USB_DEVICE_DESC_SIZE;
			break;

		case USB_CONFIGURATION_DESCRIPTOR_TYPE:
			if (pCtrl->device_speed == USB_HIGH_SPEED) {
				pD = pCtrl->high_speed_desc;
			}
			else {
				pD = pCtrl->full_speed_desc;
			}

			if (((USB_CONFIGURATION_DESCRIPTOR *) pD)->bDescriptorType != USB_CONFIGURATION_DESCRIPTOR_TYPE) {
				((USB_CONFIGURATION_DESCRIPTOR *) pD)->bDescriptorType = USB_CONFIGURATION_DESCRIPTOR_TYPE;
			}

			for (n = 0; n != pCtrl->SetupPacket.wValue.WB.L; n++) {
				if (((USB_CONFIGURATION_DESCRIPTOR *) pD)->bLength != 0) {
					pD += ((USB_CONFIGURATION_DESCRIPTOR *) pD)->wTotalLength;
				}
			}
			if (((USB_CONFIGURATION_DESCRIPTOR *) pD)->bLength == 0) {
				return ERR_USBD_INVALID_REQ;
			}
			pCtrl->EP0Data.pData = pD;
			len = ((USB_CONFIGURATION_DESCRIPTOR *) pD)->wTotalLength;
			break;

		case USB_BOS_TYPE:
			if ( ((USB_DEVICE_DESCRIPTOR *) (pCtrl->device_desc))->bcdUSB <= 0x0200 ) {
				/* BOS Descriptor is not supported on USB 2.0, but only supported on USB2.0x or USB 2.0 extension. */
				return ERR_USBD_INVALID_REQ;
			}
			pD = (uint8_t *) pCtrl->bos_descriptor;
			for (n = 0; n != pCtrl->SetupPacket.wValue.WB.L; n++) {
				if (((USB_BOS_DESCRIPTOR *) pD)->bLength != 0) {
					pD += ((USB_BOS_DESCRIPTOR *) pD)->wTotalLength;
				}
			}
			if (((USB_BOS_DESCRIPTOR *) pD)->bLength == 0) {
				return ERR_USBD_INVALID_REQ;
			}
			pCtrl->EP0Data.pData = pD;
			len = ((USB_BOS_DESCRIPTOR *) pD)->wTotalLength;
			break;

		case USB_STRING_DESCRIPTOR_TYPE:
			if (pCtrl->USB_ReqGetStringDesc != NULL) {
				if (pCtrl->USB_ReqGetStringDesc(hUsb, &pCtrl->SetupPacket, &pD) != LPC_OK) {
					return ERR_USBD_INVALID_REQ;
				}
			}
			if (pD == NULL) {
				pD = (uint8_t *) pCtrl->string_desc;

				for (n = 0; n != pCtrl->SetupPacket.wValue.WB.L; n++) {
					if (((USB_STRING_DESCRIPTOR *) pD)->bLength != 0) {
						pD += ((USB_STRING_DESCRIPTOR *) pD)->bLength;
					}
				}
			}
			if (((USB_STRING_DESCRIPTOR *) pD)->bLength == 0) {
				return ERR_USBD_INVALID_REQ;
			}
			pCtrl->EP0Data.pData = pD;
			len = ((USB_STRING_DESCRIPTOR *) pD)->bLength;
			break;

		case USB_DEVICE_QUALIFIER_DESCRIPTOR_TYPE:
			/* USB Chapter 9. page 9.6.2 */
			if (pCtrl->device_qualifier == 0) {	/* for USB_FULL_SPEED_ONLY no device qualifier*/
				return ERR_USBD_INVALID_REQ;
			}

			pCtrl->EP0Data.pData = (uint8_t *) pCtrl->device_qualifier;
			len = USB_DEVICE_QUALI_SIZE;
			break;

		case USB_OTHER_SPEED_CONFIG_DESCRIPTOR_TYPE:
			if (pCtrl->device_qualifier == 0) {	/* for USB_FULL_SPEED_ONLY no device qualifier*/
				return ERR_USBD_INVALID_REQ;
			}

			/* select other speed configuration */
			/* This is a tricky one that, if HS is supported in configuration descriptor,
			   then other speed descriptor should show FS is also supported, vice versa. That's
			   why below pD pointer is swapped to show both speeds are supported. */
			if (pCtrl->device_speed == USB_HIGH_SPEED) {
				pD = pCtrl->full_speed_desc;
			}
			else {
				pD = pCtrl->high_speed_desc;
			}

			for (n = 0; n != pCtrl->SetupPacket.wValue.WB.L; n++) {
				if (((USB_OTHER_SPEED_CONFIGURATION *) pD)->bLength != 0) {
					pD += ((USB_OTHER_SPEED_CONFIGURATION *) pD)->wTotalLength;
				}
			}
			if (((USB_OTHER_SPEED_CONFIGURATION *) pD)->bLength == 0) {
				return ERR_USBD_INVALID_REQ;
			}
			((USB_CONFIGURATION_DESCRIPTOR *) pD)->bDescriptorType = USB_OTHER_SPEED_CONFIG_DESCRIPTOR_TYPE;
			pCtrl->EP0Data.pData = pD;
			len = ((USB_OTHER_SPEED_CONFIGURATION *) pD)->wTotalLength;
			break;

		default:
			return ERR_USBD_INVALID_REQ;
		}
		break;

	case REQUEST_TO_INTERFACE:
		return ERR_USBD_INVALID_REQ;

	default:
		return ERR_USBD_INVALID_REQ;
	}

	if (pCtrl->EP0Data.Count > len) {
		pCtrl->EP0Data.Count = len;
	}

	return LPC_OK;
}

/*
 *  Get Configuration USB Request
 *    Parameters:      hUsb: Handle to the USB device stack. (global pCtrl->SetupPacket)
 *    Return Value:    ErrorCode_t type to indicate success or error condition.
 */

ErrorCode_t USB_ReqGetConfiguration(USBD_HANDLE_T hUsb)
{
	USB_CORE_CTRL_T *pCtrl = (USB_CORE_CTRL_T *) hUsb;

	/* Added ROOT2 compliance test support. */
	if ( (pCtrl->device_addr & 0x7F) == 0x0 ) {
		return ERR_USBD_INVALID_REQ;
	}
	if ( (pCtrl->SetupPacket.wValue.W != 0) || (pCtrl->SetupPacket.wLength != 1) ) {
		return ERR_USBD_INVALID_REQ;
	}
	if (pCtrl->SetupPacket.bmRequestType.BM.Recipient == REQUEST_TO_DEVICE) {
		pCtrl->EP0Data.pData = &pCtrl->config_value;
		return LPC_OK;
	}
	return ERR_USBD_INVALID_REQ;
}

/*
 *  Set Configuration USB Request
 *    Parameters:      hUsb: Handle to the USB device stack. (global pCtrl->SetupPacket)
 *    Return Value:    ErrorCode_t type to indicate success or error condition.
 */

ErrorCode_t USB_ReqSetConfiguration(USBD_HANDLE_T hUsb)
{
	USB_CORE_CTRL_T *pCtrl = (USB_CORE_CTRL_T *) hUsb;
	USB_COMMON_DESCRIPTOR *pD;
	uint32_t alt = 0, n, m;
	uint32_t new_addr;

	/* Added ROOT2 compliance test support. */
	if ( (pCtrl->device_addr & 0x7F) == 0x0 ) {
		return ERR_USBD_INVALID_REQ;
	}
	if ( (pCtrl->SetupPacket.wIndex.W != 0) || (pCtrl->SetupPacket.wLength != 0) ) {
		return ERR_USBD_INVALID_REQ;
	}

	switch (pCtrl->SetupPacket.bmRequestType.BM.Recipient) {
	case REQUEST_TO_DEVICE:

		if (pCtrl->SetupPacket.wValue.WB.L) {
			if (pCtrl->device_speed == USB_HIGH_SPEED) {
				pD = (USB_COMMON_DESCRIPTOR *) pCtrl->high_speed_desc;
			}
			else {
				pD = (USB_COMMON_DESCRIPTOR *) pCtrl->full_speed_desc;
			}

			while (pD->bLength) {
				switch (pD->bDescriptorType) {
				case USB_CONFIGURATION_DESCRIPTOR_TYPE:
					if (((USB_CONFIGURATION_DESCRIPTOR *) pD)->bConfigurationValue == pCtrl->SetupPacket.wValue.WB.L) {
						pCtrl->config_value = pCtrl->SetupPacket.wValue.WB.L;
						pCtrl->num_interfaces = ((USB_CONFIGURATION_DESCRIPTOR *) pD)->bNumInterfaces;
						for (n = 0; n < USB_MAX_IF_NUM; n++) {
							pCtrl->alt_setting[n] = 0;
						}
						for (n = 1; n < pCtrl->max_num_ep; n++) {
							if (pCtrl->ep_mask & (1 << n)) {
								pCtrl->hw_api->DisableEP(pCtrl, n);
							}
							if (pCtrl->ep_mask & ((1 << 16) << n)) {
								pCtrl->hw_api->DisableEP(pCtrl, n | 0x80);
							}
						}
						pCtrl->ep_mask = 0x00010001;
						pCtrl->ep_halt = 0x00000000;
						pCtrl->ep_stall = 0x00000000;
						pCtrl->hw_api->Configure(pCtrl, TRUE);
						if (((USB_CONFIGURATION_DESCRIPTOR *) pD)->bmAttributes & USB_CONFIG_POWERED_MASK) {
							pCtrl->device_status |=  USB_GETSTATUS_SELF_POWERED;
						}
						else {
							pCtrl->device_status &= ~USB_GETSTATUS_SELF_POWERED;
						}
					}
					else {
						new_addr = (uint32_t) pD + ((USB_CONFIGURATION_DESCRIPTOR *) pD)->wTotalLength;
						pD = (USB_COMMON_DESCRIPTOR *) new_addr;
						continue;
					}
					break;

				case USB_INTERFACE_DESCRIPTOR_TYPE:
					n = ((USB_INTERFACE_DESCRIPTOR *) pD)->bInterfaceNumber;
					pCtrl->alt_setting[n] += 0x10; /* increment number of alt interfaces */
					alt = ((USB_INTERFACE_DESCRIPTOR *) pD)->bAlternateSetting;
					break;

				case USB_ENDPOINT_DESCRIPTOR_TYPE:
					if (alt == 0) {
						n = ((USB_ENDPOINT_DESCRIPTOR *) pD)->bEndpointAddress & 0x8F;
						m = (n & 0x80) ? ((1 << 16) << (n & 0x0F)) : (1 << n);
						pCtrl->ep_mask |= m;
						pCtrl->hw_api->ConfigEP(pCtrl, (USB_ENDPOINT_DESCRIPTOR *) pD);
						pCtrl->hw_api->EnableEP(pCtrl, n);
						pCtrl->hw_api->ResetEP(pCtrl, n);
					}
					break;
				}
				new_addr = (uint32_t) pD + pD->bLength;
				pD = (USB_COMMON_DESCRIPTOR *) new_addr;
			}
		}
		else {
			pCtrl->config_value = 0;
			for (n = 1; n < pCtrl->max_num_ep; n++) {
				if (pCtrl->ep_mask & (1 << n)) {
					pCtrl->hw_api->DisableEP(pCtrl, n);
				}
				if (pCtrl->ep_mask & ((1 << 16) << n)) {
					pCtrl->hw_api->DisableEP(pCtrl, n | 0x80);
				}
			}
			pCtrl->ep_mask  = 0x00010001;
			pCtrl->ep_halt  = 0x00000000;
			pCtrl->ep_stall = 0x00000000;
			pCtrl->hw_api->Configure(pCtrl, FALSE);
		}

		if (pCtrl->config_value != pCtrl->SetupPacket.wValue.WB.L) {
			return ERR_USBD_INVALID_REQ;
		}
		break;

	default:
		return ERR_USBD_INVALID_REQ;
	}
	return LPC_OK;
}

/*
 *  Get Interface USB Request
 *    Parameters:      hUsb: Handle to the USB device stack. (global pCtrl->SetupPacket)
 *    Return Value:    ErrorCode_t type to indicate success or error condition.
 */

ErrorCode_t USB_ReqGetInterface(USBD_HANDLE_T hUsb)
{
	USB_CORE_CTRL_T *pCtrl = (USB_CORE_CTRL_T *) hUsb;

	/* Added ROOT2 compliance test support. */
	if ( (pCtrl->SetupPacket.wValue.W != 0) || (pCtrl->SetupPacket.wLength != 1) ) {
		return ERR_USBD_INVALID_REQ;
	}
	if (pCtrl->SetupPacket.bmRequestType.BM.Recipient == REQUEST_TO_INTERFACE) {
		/* check device is configured, interface number is valid and has more than one alt interface */
		if ((pCtrl->config_value != 0) &&
			(pCtrl->SetupPacket.wIndex.WB.L < pCtrl->num_interfaces) &&
			(((pCtrl->lpm_setting & (1u <<30)) == 0) || (pCtrl->alt_setting[pCtrl->SetupPacket.wIndex.WB.L] > 0x10))) {
			/* return lower nibble which contains alt interface number */
			pCtrl->EP0Buf[0] = (pCtrl->alt_setting[pCtrl->SetupPacket.wIndex.WB.L] & 0x0F);
			pCtrl->EP0Data.pData = pCtrl->EP0Buf;
			return LPC_OK;
		}
	}
	return ERR_USBD_INVALID_REQ;
}

/*
 *  Set Interface USB Request
 *    Parameters:      hUsb: Handle to the USB device stack. (global pCtrl->SetupPacket)
 *    Return Value:    ErrorCode_t type to indicate success or error condition.
 */

ErrorCode_t USB_ReqSetInterface(USBD_HANDLE_T hUsb)
{
	USB_CORE_CTRL_T *pCtrl = (USB_CORE_CTRL_T *) hUsb;
	USB_COMMON_DESCRIPTOR *pD;
	uint32_t ifn = 0, alt = 0, old = 0, msk = 0, n, m;
	uint32_t new_addr;
	ErrorCode_t ret = ERR_USBD_INVALID_REQ;

	switch (pCtrl->SetupPacket.bmRequestType.BM.Recipient) {
	case REQUEST_TO_INTERFACE:
		/* Added ROOT2 compliance test support. */
		if ( (pCtrl->config_value == 0) || ((pCtrl->device_addr & 0x7F) == 0x0 ) ||
			 (pCtrl->SetupPacket.wLength != 0) || (pCtrl->SetupPacket.wIndex.WB.L > pCtrl->num_interfaces)) {
			return ERR_USBD_INVALID_REQ;
		}

		ret = ERR_USBD_INVALID_REQ;
		if (pCtrl->device_speed == USB_HIGH_SPEED) {
			pD = (USB_COMMON_DESCRIPTOR *) pCtrl->high_speed_desc;
		}
		else {
			pD = (USB_COMMON_DESCRIPTOR *) pCtrl->full_speed_desc;
		}

		while (pD->bLength) {
			switch (pD->bDescriptorType) {
			case USB_CONFIGURATION_DESCRIPTOR_TYPE:
				if (((USB_CONFIGURATION_DESCRIPTOR *) pD)->bConfigurationValue != pCtrl->config_value) {
					new_addr = (uint32_t) pD + ((USB_CONFIGURATION_DESCRIPTOR *) pD)->wTotalLength;
					pD = (USB_COMMON_DESCRIPTOR *) new_addr;
					continue;
				}
				break;

			case USB_INTERFACE_DESCRIPTOR_TYPE:
				ifn = ((USB_INTERFACE_DESCRIPTOR *) pD)->bInterfaceNumber;
				alt = ((USB_INTERFACE_DESCRIPTOR *) pD)->bAlternateSetting;
				msk = 0;

				if (ifn == pCtrl->SetupPacket.wIndex.WB.L) {
					/* To pass ROOT2 test we should return STALL to USB_REQUEST_SET_INTERFACE 
					request if there are no alternate interfaces. */
					if ((pCtrl->alt_setting[pCtrl->SetupPacket.wIndex.WB.L] <= 0x10) && (pCtrl->lpm_setting & (1u << 30))){
						return ERR_USBD_INVALID_REQ;
					}

					if (alt == pCtrl->SetupPacket.wValue.WB.L) {
						ret = LPC_OK;
						old = pCtrl->alt_setting[ifn] & 0x0F;
						pCtrl->alt_setting[ifn] &= 0xF0;
						pCtrl->alt_setting[ifn] |= (uint8_t) alt;
					}
				}
				break;

			case USB_ENDPOINT_DESCRIPTOR_TYPE:
				if (ifn == pCtrl->SetupPacket.wIndex.WB.L) {
					n = ((USB_ENDPOINT_DESCRIPTOR *) pD)->bEndpointAddress & 0x8F;
					m = (n & 0x80) ? ((1 << 16) << (n & 0x0F)) : (1 << n);
					if (alt == pCtrl->SetupPacket.wValue.WB.L) {
						pCtrl->ep_mask |=  m;
						pCtrl->ep_halt &= ~m;
						pCtrl->hw_api->ConfigEP(pCtrl, (USB_ENDPOINT_DESCRIPTOR *) pD);
						pCtrl->hw_api->EnableEP(pCtrl, n);
						pCtrl->hw_api->ResetEP(pCtrl, n);
						msk |= m;
					}
					else if ((alt == old) && ((msk & m) == 0)) {
						pCtrl->ep_mask &= ~m;
						pCtrl->ep_halt &= ~m;
						pCtrl->hw_api->DisableEP(pCtrl, n);
					}
				}
				break;
			}
			new_addr = (uint32_t) pD + pD->bLength;
			pD = (USB_COMMON_DESCRIPTOR *) new_addr;
		}
		break;

	default:
		return ERR_USBD_INVALID_REQ;
	}

	return ret;
}

/*
 *  Invoke and Handle Class handlers
 *  Parameters:      pCtrl: Handle to Core Control Structure
 *										event: Class event
 *  Return Value:    ErrorCode_t type to indicate success or error condition.
 */

ErrorCode_t USB_InvokeEp0Hdlrs(USB_CORE_CTRL_T *pCtrl, uint32_t event)
{
	uint32_t inf = 0;
	ErrorCode_t ret = ERR_USBD_UNHANDLED;

	for (inf = 0; inf < pCtrl->num_ep0_hdlrs; inf++) {
		/* check if a valid handler is installed */
		if (pCtrl->ep0_hdlr_cb[inf] != 0) {
			/*invoke the handlers */
			ret = pCtrl->ep0_hdlr_cb[inf](pCtrl, pCtrl->ep0_cb_data[inf], event);
			/* if un-handled continue to next handler */
			if (ret != ERR_USBD_UNHANDLED) {
				if (ret != LPC_OK) {
					/* STALL requested */
					// if (ret == ERR_USBD_STALL)
					mwUSB_StallEp0(pCtrl);
				}
				/* Event is handled so return the result */
				break;
			}
		}
	}
	return ret;
}

/*
 *  Setup Packet Handling Callback routine
 *  Parameters:      hUsb: Handle to USB Stack
 *  Return Value:    ErrorCode_t type to indicate success or error condition.
 */

ErrorCode_t USB_EvtSetupHandler(USBD_HANDLE_T hUsb)
{
	USB_CORE_CTRL_T *pCtrl = (USB_CORE_CTRL_T *) hUsb;
	ErrorCode_t ret = ERR_USBD_UNHANDLED;

	mwUSB_SetupStage(pCtrl);
	pCtrl->hw_api->DirCtrlEP(pCtrl, pCtrl->SetupPacket.bmRequestType.BM.Dir);
	pCtrl->EP0Data.Count = pCtrl->SetupPacket.wLength;	/* Number of bytes to transfer */

	/* Give class handlers first chance to handle the event.
	 * If unhandled do the standard/default handling of the events.
	 */
	ret = USB_InvokeEp0Hdlrs(pCtrl, USB_EVT_SETUP);
	/* One of the callabcks has handled the event. Just return. */
	if (ret != ERR_USBD_UNHANDLED) {
		return ret;
	}

	switch (pCtrl->SetupPacket.bmRequestType.BM.Type) {

	case REQUEST_STANDARD:
		switch (pCtrl->SetupPacket.bRequest) {

		case USB_REQUEST_GET_STATUS:
			ret = pCtrl->USB_ReqGetStatus(pCtrl);
			if ( ret == LPC_OK) {
				mwUSB_DataInStage(pCtrl);
			}
			break;

		case USB_REQUEST_CLEAR_FEATURE:
			ret = pCtrl->USB_ReqSetClrFeature(pCtrl, 0);
			if ( ret == LPC_OK) {
				mwUSB_StatusInStage(pCtrl);
				if (pCtrl->USB_Feature_Event) {
					pCtrl->USB_Feature_Event(pCtrl);
				}
			}
			break;

		case USB_REQUEST_SET_FEATURE:
			ret = pCtrl->USB_ReqSetClrFeature(pCtrl, 1);
			if ( ret == LPC_OK) {
				mwUSB_StatusInStage(pCtrl);
				if (pCtrl->USB_Feature_Event) {
					pCtrl->USB_Feature_Event(pCtrl);
				}
			}
			break;

		case USB_REQUEST_SET_ADDRESS:
			ret = USB_ReqSetAddress(pCtrl);
			if ( ret == LPC_OK) {
				mwUSB_StatusInStage(pCtrl);
			}
			break;

		case USB_REQUEST_GET_DESCRIPTOR:
			ret = pCtrl->USB_ReqGetDescriptor(pCtrl);
			if (ret == LPC_OK) {
				mwUSB_DataInStage(pCtrl);
			}
			break;

		case USB_REQUEST_SET_DESCRIPTOR:
			/* Root2 compliance change. Don't set ret = LPC_OK so that we set stall condition on EP0_IN.
			   Since IP3511 can't stall EP0_OUT we need to accept the EP0_OUT data and send stall on EP0_IN in status stage.
			 */
			pCtrl->hw_api->SetStallEP(pCtrl, 0x00);			/* not supported */
			break;

		case USB_REQUEST_GET_CONFIGURATION:
			ret = pCtrl->USB_ReqGetConfiguration(pCtrl);
			if (ret == LPC_OK) {
				mwUSB_DataInStage(pCtrl);
			}
			break;

		case USB_REQUEST_SET_CONFIGURATION:
			ret = pCtrl->USB_ReqSetConfiguration(pCtrl);
			if (ret == LPC_OK) {
				mwUSB_StatusInStage(pCtrl);
				if (pCtrl->USB_Configure_Event) {
					pCtrl->USB_Configure_Event(pCtrl);
				}
			}
			break;

		case USB_REQUEST_GET_INTERFACE:
			ret = pCtrl->USB_ReqGetInterface(pCtrl);
			if (ret == LPC_OK) {
				mwUSB_DataInStage(pCtrl);
			}
			break;

		case USB_REQUEST_SET_INTERFACE:
			ret = pCtrl->USB_ReqSetInterface(pCtrl);
			if (ret == LPC_OK) {
				mwUSB_StatusInStage(pCtrl);
				if (pCtrl->USB_Interface_Event) {
					pCtrl->USB_Interface_Event(pCtrl);
				}
			}
			break;

		default:
			// mwUSB_StallEp0(pCtrl);
			break;
		}
		break;	/* end case REQUEST_STANDARD */

	case REQUEST_CLASS:
		/* All CLASS request should have been handled by class handlers.
		 * If we are here it is illegal so stall the EP.
		 */
		// mwUSB_StallEp0(pCtrl);
		break;
	/* end case REQUEST_CLASS */

	case REQUEST_VENDOR:
		if (pCtrl->USB_ReqVendor) {
			ret = pCtrl->USB_ReqVendor(pCtrl, USB_EVT_SETUP);
			if (ret == LPC_OK) {
				if (pCtrl->SetupPacket.wLength) {
					if (pCtrl->SetupPacket.bmRequestType.BM.Dir == REQUEST_DEVICE_TO_HOST) {
						mwUSB_DataInStage(pCtrl);
					}
				}
				else {
					mwUSB_StatusInStage(pCtrl);
				}
			}
		}
		break;	/* end case REQUEST_VENDOR */

	default:
		// mwUSB_StallEp0(pCtrl);
		break;
	}
	if (ret != LPC_OK) {
		mwUSB_StallEp0(pCtrl);
	}

	return LPC_OK;
}

/*
 *  EP OUT Handling Callback routine
 *  Parameters:      hUsb: Handle to USB Stack
 *  Return Value:    ErrorCode_t type to indicate success or error condition.
 */

ErrorCode_t USB_EvtOutHandler(USBD_HANDLE_T hUsb)
{
	USB_CORE_CTRL_T *pCtrl = (USB_CORE_CTRL_T *) hUsb;
	ErrorCode_t ret = LPC_OK;

	if (pCtrl->SetupPacket.bmRequestType.BM.Dir == REQUEST_HOST_TO_DEVICE) {
		if (pCtrl->EP0Data.Count) {											/* still data to receive ? */

			mwUSB_DataOutStage(pCtrl);										/* receive data */

			if (pCtrl->EP0Data.Count == 0) {								/* data complete ? */

				/* Give class handlers first chance to handle the event.
				 * If unhandled do the standard/default handling of the events.
				 */
				ret = USB_InvokeEp0Hdlrs(pCtrl, USB_EVT_OUT);
				if (ret != ERR_USBD_UNHANDLED) {
					return ret;	/* One of the callabcks has handled the event. Just return. */

				}
				/* except vendor callback everything else should have been
				 * handled in class handlers. If not stall the ep0_in to give negative handshake.
				 */
				if ((pCtrl->SetupPacket.bmRequestType.BM.Type == REQUEST_VENDOR) &&
					(pCtrl->USB_ReqVendor != 0)) {
					if (pCtrl->USB_ReqVendor(pCtrl, USB_EVT_SETUP) == LPC_OK) {

						mwUSB_StatusInStage(pCtrl);
						return LPC_OK;
					}
				}
				/* all other condition stall EP */
				mwUSB_StallEp0(pCtrl);
			}
		}
	}
	else {
		mwUSB_StatusOutStage(pCtrl);										/* receive Acknowledge */
	}

	return LPC_OK;
}

/*
 *  USB Endpoint 0 Event Callback
 *    Parameter:       hUsb Handle to the USB device stack.
 *											data: Pointer to the data which will be passed when callback function is called by the stack.
 *											event  Type of endpoint event.
 *   Return:					ErrorCode_t type to indicate success or error condition.
 */

ErrorCode_t USB_EndPoint0(USBD_HANDLE_T hUsb, void *data, uint32_t event)
{
	USB_CORE_CTRL_T *pCtrl = (USB_CORE_CTRL_T *) hUsb;
	ErrorCode_t ret = ERR_USBD_UNHANDLED;

	switch (event) {
	case USB_EVT_SETUP:
		pCtrl->USB_EvtSetupHandler(pCtrl);
		break;	/* end case USB_EVT_SETUP */

	case USB_EVT_OUT_NAK:
		/* Give class handlers first chance to handle the event.
		 * If not handled do the standard/default handling of the events.
		 */
		ret = USB_InvokeEp0Hdlrs(pCtrl, USB_EVT_OUT_NAK);
		/* One of the callbacks has handled the event. Just return. */
		if (ret == ERR_USBD_UNHANDLED) {

			if (pCtrl->SetupPacket.bmRequestType.BM.Dir == 0) {
				pCtrl->hw_api->ReadReqEP(pCtrl, 0x00, pCtrl->EP0Data.pData, pCtrl->EP0Data.Count);
			}
			else {
				/* might be zero length pkt */
				pCtrl->hw_api->ReadReqEP(pCtrl, 0x00, pCtrl->EP0Data.pData, 0);
			}
		}
		break;

	case USB_EVT_OUT:
		pCtrl->USB_EvtOutHandler(pCtrl);
		break;	/* end case USB_EVT_OUT */

	case USB_EVT_IN:
		if (pCtrl->SetupPacket.bmRequestType.BM.Dir == REQUEST_DEVICE_TO_HOST) {
			ret = USB_InvokeEp0Hdlrs(pCtrl, USB_EVT_IN);
			/* One of the callbacks has handled the event. Just return. */
			if (ret == ERR_USBD_UNHANDLED) {
				mwUSB_DataInStage(pCtrl);			/* send data */
			}
		}
		else {
			if (pCtrl->device_addr & 0x80) {
				pCtrl->device_addr &= 0x7F;
				pCtrl->hw_api->SetAddress(pCtrl, pCtrl->device_addr);
			}
		}
		break;	/* end case USB_EVT_IN */

	case USB_EVT_OUT_STALL:
		pCtrl->hw_api->ClrStallEP(pCtrl, 0x00);
		break;

	case USB_EVT_IN_STALL:
		pCtrl->hw_api->ClrStallEP(pCtrl, 0x80);
		break;
	}
	return LPC_OK;
}

/* cache and mmu translation functions */
uint32_t USBD_virt_to_phys(void *vaddr)
{
	/* retun virtual address as physical address */
	return (uint32_t) vaddr;
}

void USBD_cache_flush(uint32_t *start_adr, uint32_t *end_adr)
{
	/* do nothing*/
}

/*
 *  Core function initialization routine
 *  Parameters:     pCtrl: Handle to Core Control Structure.
 *                                  param: Structure containing Core function driver module
 *						      initialization parameters.
 *									pdescr: Pointer to USB descriptors
 *  Return Value:   None
 */

void  mwUSB_InitCore(USB_CORE_CTRL_T *pCtrl, USB_CORE_DESCS_T *pdescr, USBD_API_INIT_PARAM_T *param)
{
	COMPILE_TIME_ASSERT((offsetof(USB_CORE_CTRL_T, SetupPacket) & 0x3) == 0);
	COMPILE_TIME_ASSERT((offsetof(USB_CORE_CTRL_T, EP0Buf) & 0x3) == 0);

	/* init USB controller struct */
	memset((void *) pCtrl, 0, sizeof(USB_CORE_CTRL_T));
	pCtrl->max_num_ep = param->max_num_ep;
	/* assign default implementation to virtual functions*/
	mwUSB_RegisterEpHandler(pCtrl, 0, USB_EndPoint0, pCtrl);
	mwUSB_RegisterEpHandler(pCtrl, 1, USB_EndPoint0, pCtrl);
	pCtrl->USB_EvtSetupHandler = USB_EvtSetupHandler;
	pCtrl->USB_EvtOutHandler = USB_EvtOutHandler;

	pCtrl->USB_ReqGetStatus = USB_ReqGetStatus;
	pCtrl->USB_ReqSetClrFeature = USB_ReqSetClrFeature;
	pCtrl->USB_ReqGetDescriptor = USB_ReqGetDescriptor;
	pCtrl->USB_ReqGetConfiguration = USB_ReqGetConfiguration;
	pCtrl->USB_ReqSetConfiguration = USB_ReqSetConfiguration;
	pCtrl->USB_ReqGetInterface = USB_ReqGetInterface;
	pCtrl->USB_ReqSetInterface = USB_ReqSetInterface;

	/* cache and mmu translation functions */
	pCtrl->virt_to_phys = (param->virt_to_phys) ? param->virt_to_phys : USBD_virt_to_phys;
	pCtrl->cache_flush = (param->cache_flush) ? param->cache_flush : USBD_cache_flush;

	/* copy other callback */
	pCtrl->USB_WakeUpCfg     = param->USB_WakeUpCfg;
	pCtrl->USB_Power_Event   = param->USB_Power_Event;
	pCtrl->USB_Reset_Event   = param->USB_Reset_Event;
	pCtrl->USB_Suspend_Event = param->USB_Suspend_Event;
	pCtrl->USB_Resume_Event  = param->USB_Resume_Event;
	pCtrl->USB_SOF_Event     = param->USB_SOF_Event;
	pCtrl->USB_Error_Event   = param->USB_Error_Event;

	pCtrl->USB_Configure_Event = param->USB_Configure_Event;
	pCtrl->USB_Interface_Event = param->USB_Interface_Event;
	pCtrl->USB_Feature_Event   = param->USB_Feature_Event;
	pCtrl->USB_ReqGetStringDesc = param->USB_ReqGetStringDesc;

	/* parse descriptors */
	pCtrl->device_desc = pdescr->device_desc;
	pCtrl->string_desc = pdescr->string_desc;
	pCtrl->full_speed_desc = pdescr->full_speed_desc;
	pCtrl->high_speed_desc = pdescr->high_speed_desc;
	pCtrl->device_qualifier = pdescr->device_qualifier;
	pCtrl->bos_descriptor = pdescr->bos_descriptor;
	pCtrl->hw_api = &hw_api;
}

/*
 *  Function to register class specific EP0 event handler with USB device stack.
 *  Parameters:     hUsb Handle to the USB device stack.
 *                                  pfn  Class specific EP0 handler function.
 *									data Pointer to the data which will be passed when callback function is called by the stack.
 *  Return Value:   ErrorCode_t type to indicate success or error condition.
 */

ErrorCode_t mwUSB_RegisterClassHandler(USBD_HANDLE_T hUsb, USB_EP_HANDLER_T pfn, void *data)
{
	USB_CORE_CTRL_T *pCtrl = (USB_CORE_CTRL_T *) hUsb;

	/* check if index is within limits. */
	if (pCtrl->num_ep0_hdlrs >= USB_MAX_IF_NUM) {
		return ERR_USBD_TOO_MANY_CLASS_HDLR;
	}

	pCtrl->ep0_hdlr_cb[pCtrl->num_ep0_hdlrs] = pfn;
	pCtrl->ep0_cb_data[pCtrl->num_ep0_hdlrs] = data;
	/* increment handler index */
	pCtrl->num_ep0_hdlrs++;

	return LPC_OK;
}

/*
 *  Function to register interrupt/event handler for the requested endpoint with USB device stack.
 *  Parameters:     hUsb Handle to the USB device stack.
 *									ep_index  Endpoint index.
 *                                  pfn  Endpoint event handler function.
 *									data Pointer to the data which will be passed when callback function is called by the stack.
 *  Return Value:   ErrorCode_t type to indicate success or error condition.
 */

ErrorCode_t mwUSB_RegisterEpHandler(USBD_HANDLE_T hUsb, uint32_t ep_indx, USB_EP_HANDLER_T pfn, void *data)
{
	USB_CORE_CTRL_T *pCtrl = (USB_CORE_CTRL_T *) hUsb;

	/* check if index is within limits. */
	if (ep_indx > (2 * pCtrl->max_num_ep)) {
		return ERR_API_INVALID_PARAM2;
	}

	pCtrl->ep_event_hdlr[ep_indx] = pfn;
	pCtrl->ep_hdlr_data[ep_indx] = data;

	return LPC_OK;
}

/*
 *  Function to get default EP handler and its parameter for a particular endpoint.
 *  Parameters:     hUsb Handle to the USB device stack.
 *									ep_index  Endpoint index.
 *                                  ep_handler  Double Pointer to Endpoint event handler function which is updated in the function.
 *									data Pointer to the data which will be passed when callback function is called by the stack.
 *  Return Value:   ErrorCode_t type to indicate success or error condition.
 */

ErrorCode_t mwUSB_GetEpHandler(USBD_HANDLE_T hUsb, uint32_t ep_index, USB_EP_HANDLER_T *ep_handler, void * *data)
{
	USB_CORE_CTRL_T *pCtrl = (USB_CORE_CTRL_T *) hUsb;

	/* check if index is within limits. */
	if (ep_index > (2 * pCtrl->max_num_ep)) {
		return ERR_API_INVALID_PARAM2;
	}

	*ep_handler = pCtrl->ep_event_hdlr[ep_index];
	*data = pCtrl->ep_hdlr_data[ep_index];

	return LPC_OK;
}
