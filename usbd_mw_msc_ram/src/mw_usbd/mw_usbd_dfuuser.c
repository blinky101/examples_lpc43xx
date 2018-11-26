/***********************************************************************
 * $Id:: mw_usbd_dfuuser.c 576 2012-11-20 01:38:36Z usb10131                   $
 *
 * Project: USB device ROM Stack
 *
 * Description:
 *     Device Firmware Upgrade Class Custom User Module.
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

#include <string.h>	/*for memcpy */
#include "mw_usbd.h"
#include "mw_usbd_hw.h"
#include "mw_usbd_core.h"
#include "mw_usbd_desc.h"
#include "mw_usbd_dfu.h"
#include "mw_usbd_dfuuser.h"

static ErrorCode_t mwDFU_handle_dnload(USBD_DFU_CTRL_T *pDfuCtrl, uint16_t block_num, uint16_t len)
{
	/* Store Received Data into Flash or External Memory */
	pDfuCtrl->dfu_status = pDfuCtrl->DFU_Write(block_num, &pDfuCtrl->xfr_buf, len,
											   &pDfuCtrl->dfu_req_get_status.bwPollTimeout[0]);
	if (pDfuCtrl->dfu_status != DFU_STATUS_OK) {
		pDfuCtrl->dfu_state = DFU_STATE_dfuERROR;
		return ERR_USBD_STALL;	// RET_STALL;
	}
	return LPC_OK;	// RET_ZLP;
}

static ErrorCode_t mwDFU_handle_upload(USBD_DFU_CTRL_T *pDfuCtrl, uint16_t block_num, uint16_t len)
{
	USB_CORE_CTRL_T *pCtrl = pDfuCtrl->pUsbCtrl;
	int32_t copy_len;
	uint8_t *buff;

	if (len > pDfuCtrl->dfu_desc->wTransferSize) {
		/* Too big. Not that we'd really care, but it's a
		 * DFU protocol violation */
		pDfuCtrl->dfu_state = DFU_STATE_dfuERROR;
		pDfuCtrl->dfu_status = DFU_STATUS_errADDRESS;
		return ERR_USBD_STALL;	// RET_STALL;
	}
	buff = pDfuCtrl->xfr_buf;
	/* Fetch Data from Flash or External Memory */
	copy_len = pDfuCtrl->DFU_Read(block_num, &buff, len);
	/* Send EOF file frame as short data length packet
	 * which is less than maximum DFU transfer size
	 */
	if (copy_len == 0) {
		pCtrl->EP0Data.Count = 0;
		pDfuCtrl->dfu_state = DFU_STATE_dfuIDLE;
		return LPC_OK;	// RET_ZLP;
	}
	else if ( copy_len <= DFU_STATUS_errSTALLEDPKT) {
		pDfuCtrl->dfu_state = DFU_STATE_dfuERROR;
		pDfuCtrl->dfu_status = (uint8_t) (copy_len & 0xFF);
		return ERR_USBD_STALL;	// RET_STALL;
	}

	pCtrl->EP0Data.pData = buff;
	pCtrl->EP0Data.Count = copy_len;

	/* Send EOF file frame as short data length packet
	 * which is less than maximum DFU transfer size
	 */
	if (pCtrl->EP0Data.Count < pDfuCtrl->dfu_desc->wTransferSize) {
		pDfuCtrl->dfu_state = DFU_STATE_dfuIDLE;
		return LPC_OK;
	}
	return LPC_OK;
}

/*
 * Returns the DFU Protocol Status
 */
static void mwDFU_handle_getstatus(USBD_DFU_CTRL_T *pDfuCtrl)
{
	uint8_t statebc = DFU_STATE_dfuINVALID;	/* Busy status */
	USB_CORE_CTRL_T *pCtrl = pDfuCtrl->pUsbCtrl;

	pDfuCtrl->dfu_req_get_status.iString = 0;
	switch (pDfuCtrl->dfu_state) {
	case DFU_STATE_dfuDNLOAD_SYNC:
	/* ***TBD *** block in progress case ???***/
	case DFU_STATE_dfuDNBUSY:
		pDfuCtrl->dfu_state = DFU_STATE_dfuDNLOAD_IDLE;
		statebc = DFU_STATE_dfuDNBUSY;
		break;

	case DFU_STATE_dfuMANIFEST_SYNC:
	case DFU_STATE_dfuMANIFEST:
	/* We should not get status request on this state!! Something wrong with host */
	case DFU_STATE_dfuMANIFEST_WAIT_RST:
		pDfuCtrl->download_done = 1;
		/* If we're MainfestationTolerant */
		if (pDfuCtrl->dfu_desc->bmAttributes & USB_DFU_MANIFEST_TOL) {
			pDfuCtrl->dfu_state = DFU_STATE_dfuIDLE;
		}
		else {
			pDfuCtrl->dfu_state = DFU_STATE_dfuMANIFEST_WAIT_RST;
		}
		statebc = DFU_STATE_dfuMANIFEST;

	default:
		break;
	}

	if ((statebc != DFU_STATE_dfuINVALID) && pDfuCtrl->DFU_GetStatus) {
		uint32_t ret, tout;
		uint8_t *ptr = &pDfuCtrl->dfu_req_get_status.bwPollTimeout[0];
		ptr[0] = ptr[1] = ptr[2] = 0;	/* Reset the timeout value */
		pDfuCtrl->dfu_req_get_status.iString = 0;	/* Reset String value */

		ret = pDfuCtrl->DFU_GetStatus(&tout, statebc == DFU_STATE_dfuMANIFEST);
		pDfuCtrl->dfu_status = ret;
		if (ret != DFU_STATUS_OK) {
			pDfuCtrl->dfu_state = DFU_STATE_dfuERROR;
			if (DFU_STATUS_errVENDOR == (ret & 0xFF)) {
				pDfuCtrl->dfu_req_get_status.iString = (ret >> 8) & 0xFF;
			}
		}
		else {
			/* If non-zero timeout given then the app is busy writing/erasing stuff */
			ptr[0] = tout & 0xFF;
			ptr[1] = (tout >> 8) & 0xFF;
			ptr[2] = (tout >> 16) & 0xFF;
			if (tout) {
				pDfuCtrl->dfu_state = statebc;
				pDfuCtrl->download_done = 0;
			}
		}
	}

	/* send status response */
	pDfuCtrl->dfu_req_get_status.bStatus = pDfuCtrl->dfu_status;
	pDfuCtrl->dfu_req_get_status.bState = pDfuCtrl->dfu_state;

	pCtrl->EP0Data.pData = (uint8_t *) &pDfuCtrl->dfu_req_get_status;
	pCtrl->EP0Data.Count = DFU_GET_STATUS_SIZE;
}

/*
 * Returns the DFU State
 */
static void mwDFU_handle_getstate(USBD_DFU_CTRL_T *pDfuCtrl)
{
	USB_CORE_CTRL_T *pCtrl = pDfuCtrl->pUsbCtrl;
	pCtrl->EP0Data.pData = (uint8_t *) &pDfuCtrl->dfu_state;
	pCtrl->EP0Data.Count = sizeof(uint8_t);
}

static ErrorCode_t mwDFU_handle_invalid_req(USBD_DFU_CTRL_T *pDfuCtrl)
{
	switch (pDfuCtrl->dfu_state) {
	case DFU_STATE_appIDLE:
		break;

	case DFU_STATE_appDETACH:
		pDfuCtrl->dfu_state = DFU_STATE_appIDLE;
		break;

	case DFU_STATE_dfuMANIFEST_WAIT_RST:
		return LPC_OK;

	default:
		pDfuCtrl->dfu_state = DFU_STATE_dfuERROR;
		break;
	}
	return ERR_USBD_STALL;
}

/*
 * Handle DFU Protocl State Machine
 */
ErrorCode_t mwDFU_handle_setup(USBD_DFU_CTRL_T *pDfuCtrl)
{
	ErrorCode_t ret = ERR_USBD_UNHANDLED;
	uint8_t req = pDfuCtrl->pUsbCtrl->SetupPacket.bRequest;
	uint16_t len = pDfuCtrl->pUsbCtrl->SetupPacket.wLength;

	switch (req) {

	case USB_REQ_DFU_GETSTATUS:
		mwDFU_handle_getstatus(pDfuCtrl);
		ret = LPC_OK;
		break;

	case USB_REQ_DFU_GETSTATE:
		if (pDfuCtrl->dfu_state == DFU_STATE_dfuDNBUSY) {
			ret = mwDFU_handle_invalid_req(pDfuCtrl);
		}
		else {
			mwDFU_handle_getstate(pDfuCtrl);
			ret = LPC_OK;
		}
		break;

	case USB_REQ_DFU_DETACH:
		if (pDfuCtrl->dfu_state == DFU_STATE_appIDLE) {
			pDfuCtrl->dfu_state = DFU_STATE_appDETACH;
			ret = LPC_OK;
		}
		else {
			ret = mwDFU_handle_invalid_req(pDfuCtrl);
		}
		break;

	case USB_REQ_DFU_CLRSTATUS:
		if (pDfuCtrl->dfu_state == DFU_STATE_dfuERROR) {
			pDfuCtrl->dfu_state = DFU_STATE_dfuIDLE;
			pDfuCtrl->dfu_status = DFU_STATUS_OK;
			ret = LPC_OK;
		}
		else {
			ret = mwDFU_handle_invalid_req(pDfuCtrl);
		}
		break;

	case USB_REQ_DFU_ABORT:
		switch (pDfuCtrl->dfu_state) {
		case DFU_STATE_dfuIDLE:
		case DFU_STATE_dfuDNLOAD_SYNC:
		case DFU_STATE_dfuDNLOAD_IDLE:
		case DFU_STATE_dfuMANIFEST_SYNC:
		case DFU_STATE_dfuUPLOAD_IDLE:
			pDfuCtrl->dfu_state = DFU_STATE_dfuIDLE;
			// mwUSB_StatusInStage(pDfuCtrl->pUsbCtrl);
			ret = LPC_OK;
			break;

		default:
			ret = mwDFU_handle_invalid_req(pDfuCtrl);
			break;
		}
		break;

	case USB_REQ_DFU_DNLOAD:
		switch (pDfuCtrl->dfu_state) {
		case DFU_STATE_dfuIDLE:
			if (len == 0) {
				ret = mwDFU_handle_invalid_req(pDfuCtrl);
				break;
			}

		/* fall through */
		case DFU_STATE_dfuDNLOAD_IDLE:
			if (len > pDfuCtrl->dfu_desc->wTransferSize) {
				/* Too big. Not that we'd really care, but it's a
				 * DFU protocol violation */
				pDfuCtrl->dfu_state = DFU_STATE_dfuERROR;
				pDfuCtrl->dfu_status = DFU_STATUS_errADDRESS;
				ret = ERR_USBD_STALL;
			}
			else {
				/* end of transfer indicator */
				if (len == 0) {
					pDfuCtrl->dfu_state = DFU_STATE_dfuMANIFEST_SYNC;
					mwUSB_StatusInStage(pDfuCtrl->pUsbCtrl);// RET_ZLP;
					return LPC_OK;
				}
				pDfuCtrl->dfu_state = DFU_STATE_dfuDNLOAD_SYNC;
				pDfuCtrl->DFU_Write(pDfuCtrl->pUsbCtrl->SetupPacket.wValue.W,
									&pDfuCtrl->xfr_buf, 0, &pDfuCtrl->dfu_req_get_status.bwPollTimeout[0]);
				/* setup transfer buffer */
				pDfuCtrl->pUsbCtrl->EP0Data.pData = pDfuCtrl->xfr_buf;
				ret = LPC_OK;
			}
			break;

		default:
			ret = mwDFU_handle_invalid_req(pDfuCtrl);
			break;
		}
		break;

	case USB_REQ_DFU_UPLOAD:
		switch (pDfuCtrl->dfu_state) {
		case DFU_STATE_dfuIDLE:
			pDfuCtrl->dfu_state = DFU_STATE_dfuUPLOAD_IDLE;

		case DFU_STATE_dfuUPLOAD_IDLE:
			/* state transition if less data then requested */
			ret = mwDFU_handle_upload(pDfuCtrl, pDfuCtrl->pUsbCtrl->SetupPacket.wValue.W, len);
			break;

		default:
			ret = mwDFU_handle_invalid_req(pDfuCtrl);
			break;
		}
		break;

	default:
		break;
	}

	return ret;
}

void mwDFU_reset_event(USBD_DFU_CTRL_T *pDfuCtrl)
{
	/* Handles USB Cable unplug event */
	if (pDfuCtrl->dfu_state == DFU_STATE_appIDLE) {
		pDfuCtrl->dfu_state = DFU_STATE_appIDLE;
	}
	else {
		/* after detach & reset the DFU interface should be 0 per DFU 1.1 spec*/
		pDfuCtrl->if_num = 0;
		pDfuCtrl->dfu_state = DFU_STATE_dfuIDLE;
	}
	pDfuCtrl->dfu_status = DFU_STATUS_OK;
	pDfuCtrl->download_done = 0;
}

ErrorCode_t mwDFU_Ep0_Hdlr(USBD_HANDLE_T hUsb, void *data, uint32_t event)
{
	USB_CORE_CTRL_T *pCtrl = (USB_CORE_CTRL_T *) hUsb;
	USBD_DFU_CTRL_T *pDfuCtrl = (USBD_DFU_CTRL_T *) data;
	ErrorCode_t ret = ERR_USBD_UNHANDLED;

	/* Check if the request is for this instance of interface. IF not return immediately. */
	if ((pCtrl->SetupPacket.wIndex.WB.L != pDfuCtrl->if_num) && (event != USB_EVT_RESET)) {
		return ret;
	}

	switch (event) {
	case USB_EVT_SETUP:
		if ((pCtrl->SetupPacket.bmRequestType.BM.Type == REQUEST_CLASS) &&
			(pCtrl->SetupPacket.bmRequestType.BM.Recipient == REQUEST_TO_INTERFACE)) {
			/* handle setup packets */
			ret = mwDFU_handle_setup(pDfuCtrl);
			if (ret == LPC_OK) {
				if (( pCtrl->SetupPacket.bRequest == USB_REQ_DFU_GETSTATUS) ||
					( pCtrl->SetupPacket.bRequest == USB_REQ_DFU_GETSTATE) ||
					( pCtrl->SetupPacket.bRequest == USB_REQ_DFU_UPLOAD) ) {
					/* send status/state/upload data to host*/
					mwUSB_DataInStage(pDfuCtrl->pUsbCtrl);
					/* tell user if download finished */
					if (pDfuCtrl->download_done) {
						pDfuCtrl->DFU_Done();
						/* wait for next download */
						pDfuCtrl->download_done = 0;
					}
				}
				else if (pCtrl->SetupPacket.bRequest != USB_REQ_DFU_DNLOAD) {
					/* send status handshake */
					mwUSB_StatusInStage(pCtrl);
					/* check if we need to detach ourselves */
					if ( (pDfuCtrl->dfu_state == DFU_STATE_appDETACH) &&
						 (pDfuCtrl->DFU_Detach != 0)) {
						pDfuCtrl->DFU_Detach(hUsb);
					}
				}
			}
		}
		else if ((pCtrl->SetupPacket.bmRequestType.BM.Type == REQUEST_STANDARD) &&
				 (pCtrl->SetupPacket.bRequest == USB_REQUEST_GET_DESCRIPTOR) &&
				 (pCtrl->SetupPacket.wValue.WB.H == USB_DFU_DESCRIPTOR_TYPE)) {

			pCtrl->EP0Data.pData = (uint8_t *) pDfuCtrl->dfu_desc;
			pCtrl->EP0Data.Count = USB_DFU_DESCRIPTOR_SIZE;
			mwUSB_DataInStage(pCtrl);
			ret = LPC_OK;
		}
		break;

	case USB_EVT_OUT:
		if ((pCtrl->SetupPacket.bmRequestType.BM.Type == REQUEST_CLASS) &&
			(pCtrl->SetupPacket.bmRequestType.BM.Recipient == REQUEST_TO_INTERFACE)) {

			if (pCtrl->SetupPacket.bRequest == USB_REQ_DFU_DNLOAD) {
				/* handle image download */
				ret = mwDFU_handle_dnload(pDfuCtrl, pCtrl->SetupPacket.wValue.W, pCtrl->SetupPacket.wLength);
				if (ret == LPC_OK) {
					mwUSB_StatusInStage(pCtrl);
				}
			}
		}
		break;

	case USB_EVT_RESET:
		mwDFU_reset_event(pDfuCtrl);
		break;

	default:
		break;
	}
	return ret;
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
uint32_t mwDFU_GetMemSize(USBD_DFU_INIT_PARAM_T *param)
{
	uint32_t req_len = 0;

	/* calculate required length */
	req_len += sizeof(USBD_DFU_CTRL_T);	/* memory for DFU controller structure */
	req_len += param->wTransferSize;/* for transfer buffer */
	req_len += 4;	/* for alignment overhead */
	req_len &= ~0x3;

	return req_len;
}

/*
 * Initialize DFU State Machine
 */
ErrorCode_t mwDFU_init(USBD_HANDLE_T hUsb, USBD_DFU_INIT_PARAM_T *param, uint32_t init_state)
{
	USBD_DFU_CTRL_T *pDfuCtrl;
	uint32_t next_desc_addr;
	USB_INTERFACE_DESCRIPTOR *tmp, *pIntfDesc = (USB_INTERFACE_DESCRIPTOR *) param->intf_desc;
	USB_DFU_FUNC_DESCRIPTOR *pDfuDesc;
	ErrorCode_t ret = LPC_OK;

	/* check for memory alignment */
	if ((param->mem_base &  0x3) &&
		(param->mem_size < sizeof(USBD_DFU_CTRL_T))) {
		return ERR_USBD_BAD_MEM_BUF;
	}

	/* allocate memory for the control data structure */
	pDfuCtrl = (USBD_DFU_CTRL_T *) param->mem_base;
	param->mem_base += sizeof(USBD_DFU_CTRL_T);
	param->mem_size -= sizeof(USBD_DFU_CTRL_T);
	/* align to 4 byte boundary */
	while (param->mem_base & 0x03) {
		param->mem_base++;
		param->mem_size--;
	}

	/* Init control structures with passed params */
	memset((void *) pDfuCtrl, 0, sizeof(USBD_DFU_CTRL_T));
	pDfuCtrl->pUsbCtrl = (USB_CORE_CTRL_T *) hUsb;
	/* allocate dfu transfer buffer */
	pDfuCtrl->xfr_buf = (uint8_t *) param->mem_base;
	param->mem_base += param->wTransferSize;
	param->mem_size -= param->wTransferSize;

	/* user defined functions */
	if ((param->DFU_Write == 0) ||
		(param->DFU_Done == 0) ||
		(param->DFU_Read == 0) ) {
		return ERR_API_INVALID_PARAM2;
	}

	pDfuCtrl->DFU_Write = param->DFU_Write;
	pDfuCtrl->DFU_Read = param->DFU_Read;
	pDfuCtrl->DFU_Done = param->DFU_Done;
	pDfuCtrl->DFU_Detach = param->DFU_Detach;
	pDfuCtrl->DFU_GetStatus = param->DFU_GetStatus;

	next_desc_addr = (uint32_t) param->intf_desc;
	/* parse the interface descriptor */
	if ((pIntfDesc->bDescriptorType != USB_INTERFACE_DESCRIPTOR_TYPE) ||
		(pIntfDesc->bInterfaceClass != USB_DEVICE_CLASS_APP) ||
		(pIntfDesc->bInterfaceSubClass != USB_DFU_SUBCLASS)) {

		return ERR_USBD_BAD_INTF_DESC;
	}
	/* store interface number */
	pDfuCtrl->if_num = pIntfDesc->bInterfaceNumber;
	/* next should be DFU descriptor */
	next_desc_addr += pIntfDesc->bLength;
	tmp = (USB_INTERFACE_DESCRIPTOR *) next_desc_addr;
	/* Check for alt interface descriptors */
	while (tmp->bDescriptorType == USB_INTERFACE_DESCRIPTOR_TYPE &&
		   tmp->bInterfaceClass == USB_DEVICE_CLASS_APP &&
		   tmp->bInterfaceSubClass == USB_DFU_SUBCLASS &&
		   tmp->bInterfaceNumber == pDfuCtrl->if_num) {
		next_desc_addr += tmp->bLength;
		tmp = (USB_INTERFACE_DESCRIPTOR *) next_desc_addr;
	}
	pDfuDesc = (USB_DFU_FUNC_DESCRIPTOR *) next_desc_addr;
	/* parse the DFU functional descriptor */
	if ((pDfuDesc->bDescriptorType != USB_DFU_DESCRIPTOR_TYPE) ||
		(pDfuDesc->wTransferSize != param->wTransferSize) ||
		((pDfuDesc->bmAttributes & USB_DFU_WILL_DETACH) && (param->DFU_Detach == 0)) ) {

		return ERR_USBD_BAD_DESC;
	}
	/* store DFU descriptor pointer */
	pDfuCtrl->dfu_desc = (USB_DFU_FUNC_DESCRIPTOR *) next_desc_addr;

	/* register ep0 handler */
	/* check if user wants his own handler */
	if (param->DFU_Ep0_Hdlr == 0) {
		ret = mwUSB_RegisterClassHandler(hUsb, mwDFU_Ep0_Hdlr, pDfuCtrl);
	}
	else {
		ret = mwUSB_RegisterClassHandler(hUsb, param->DFU_Ep0_Hdlr, pDfuCtrl);
		param->DFU_Ep0_Hdlr = mwDFU_Ep0_Hdlr;
	}

	/* set dfu state variables */
	pDfuCtrl->dfu_state = (init_state == DFU_STATE_dfuIDLE) ? DFU_STATE_dfuIDLE : DFU_STATE_appIDLE;
	pDfuCtrl->dfu_status = DFU_STATUS_OK;
	pDfuCtrl->download_done = 0;

	return ret;
}
