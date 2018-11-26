/***********************************************************************
 * $Id:: mw_usbd_mscuser.c 1957 2015-03-06 20:19:00Z usb00423                  $
 *
 * Project: USB device ROM Stack
 *
 * Description:
 *     Mass Storage Class Custom User Module.
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
#include "mw_usbd_core.h"
#include "mw_usbd_hw.h"
#include "mw_usbd_msc.h"
#include "mw_usbd_mscuser.h"

#ifndef FALSE
#define FALSE 0
#define TRUE !FALSE
#endif

/* forward declarations */
/* MSC Requests Callback Functions */
ErrorCode_t mwMSC_Reset(USB_MSC_CTRL_T *pMscCtrl);

ErrorCode_t mwMSC_GetMaxLUN(USB_MSC_CTRL_T *pMscCtrl);

/* MSC Bulk Callback Functions */
void mwMSC_GetCBW(USB_MSC_CTRL_T *pMscCtrl);

void mwMSC_SetCSW(USB_MSC_CTRL_T *pMscCtrl);

void mwMSC_BulkIn(USB_MSC_CTRL_T *pMscCtrl);

void mwMSC_BulkOut(USB_MSC_CTRL_T *pMscCtrl);

void mwMSC_BulkOutNak(USB_MSC_CTRL_T *pMscCtrl);

/*
 *  Set Stall for MSC Endpoint
 *  Parameters:      pMscCtrl: Handle to MSC structure
 *									 EPNum: Endpoint Number
 *                       EPNum.0..3: Address
 *                       EPNum.7:    Dir
 *  Return Value:    None
 */

void mwMSC_SetStallEP(USB_MSC_CTRL_T *pMscCtrl, uint32_t EPNum) {			/* set EP halt status according stall status */
	pMscCtrl->pUsbCtrl->hw_api->SetStallEP(pMscCtrl->pUsbCtrl, EPNum);
	pMscCtrl->pUsbCtrl->ep_halt  |=  (EPNum & 0x80) ? ((1 << 16) << (EPNum & 0x0F)) : (1 << EPNum);
}

/*
 *  MSC Mass Storage Reset Request Callback
 *  Called automatically on Mass Storage Reset Request
 *  Parameters:      pMscCtrl: Handle to MSC structure (global SetupPacket and EP0Buf)
 *  Return Value:    ErrorCode_t type to indicate success or error condition.
 */

ErrorCode_t mwMSC_Reset(USB_MSC_CTRL_T *pMscCtrl) {
	pMscCtrl->pUsbCtrl->ep_stall = 0x00000000;			/* EP must stay stalled */
	pMscCtrl->CSW.dSignature = 0;					/* invalid signature */

	pMscCtrl->BulkStage = MSC_BS_CBW;
	return LPC_OK;
}

/*
 *  MSC Get Max LUN Request Callback
 *  Called automatically on Get Max LUN Request
 *  Parameters:      pMscCtrl: Handle to MSC structure (global SetupPacket and EP0Buf)
 *  Return Value:    ErrorCode_t type to indicate success or error condition.
 */

ErrorCode_t mwMSC_GetMaxLUN(USB_MSC_CTRL_T *pMscCtrl) {

	pMscCtrl->pUsbCtrl->EP0Buf[0] = 0;				/* No LUN associated with this device */
	return LPC_OK;
}

/*
 *  MSC Bulk Out Read request routine
 *  Parameters:      pMscCtrl: Handle to MSC structure (global variables)
 *										 buff - pointer to read data
 *  Return Value:    None
 */

void mwMSC_ReadReqBulkEp(USB_MSC_CTRL_T *pMscCtrl, uint8_t *buff) {

	if ( pMscCtrl->pUsbCtrl->device_speed == USB_HIGH_SPEED ) {
		pMscCtrl->pUsbCtrl->hw_api->ReadReqEP(pMscCtrl->pUsbCtrl, pMscCtrl->epout_num, buff, USB_HS_MAX_BULK_PACKET);
	}
	else {
		pMscCtrl->pUsbCtrl->hw_api->ReadReqEP(pMscCtrl->pUsbCtrl, pMscCtrl->epout_num, buff, USB_FS_MAX_BULK_PACKET);
	}
}

/*
 *  MSC Memory Read Callback
 *  Called automatically on Memory Read Event
 *  Parameters:      pMscCtrl: Handle to MSC structure (global variables)
 *  Return Value:    None
 */

void mwMSC_MemoryRead(USB_MSC_CTRL_T *pMscCtrl) {
	uint32_t n;
	uint8_t *buff;

	if ( pMscCtrl->pUsbCtrl->device_speed == USB_HIGH_SPEED ) {
		if (pMscCtrl->Length > USB_HS_MAX_BULK_PACKET) {
			n = USB_HS_MAX_BULK_PACKET;
		}
		else {
			n = pMscCtrl->Length;
		}
	}
	else {
		if (pMscCtrl->Length > USB_FS_MAX_BULK_PACKET) {
			n = USB_FS_MAX_BULK_PACKET;
		}
		else {
			n = pMscCtrl->Length;
		}
	}

	/* MemorySize check is done in mwMSC_RWSetup */
	/* use the default MSC buffer */
	buff = pMscCtrl->BulkBuf;
	/* read data from user callback. User could update buffer
	 * pointer to his own buffer without making extra copy.
	 */
	pMscCtrl->MSC_Read(((uint32_t) pMscCtrl->Offset & 0xFFFFFFFF), &buff, n, (pMscCtrl->Offset >> 32));
	/* send data to host */
	pMscCtrl->pUsbCtrl->hw_api->WriteEP(pMscCtrl->pUsbCtrl, pMscCtrl->epin_num, buff, n);
	pMscCtrl->Offset += n;
	pMscCtrl->Length -= n;

	pMscCtrl->CSW.dDataResidue -= n;

	if (pMscCtrl->Length == 0) {
		pMscCtrl->BulkStage = MSC_BS_DATA_IN_LAST;
	}

	if (pMscCtrl->BulkStage != MSC_BS_DATA_IN) {
		pMscCtrl->CSW.bStatus = CSW_CMD_PASSED;
	}
}

/*
 *  MSC Memory Write Callback
 *  Called automatically on Memory Write Event
 *  Parameters:      pMscCtrl: Handle to MSC structure (global variables)
 *  Return Value:    None
 */

void mwMSC_MemoryWrite(USB_MSC_CTRL_T *pMscCtrl) {

	/* MemorySize check is done in mwMSC_RWSetup */
	/* write data recived to user destination through callback */
	pMscCtrl->MSC_Write(((uint32_t) pMscCtrl->Offset & 0xFFFFFFFF),
						&pMscCtrl->rx_buf /*pMscCtrl->BulkBuf*/,
						pMscCtrl->BulkLen,
						(pMscCtrl->Offset >> 32));

	pMscCtrl->Offset += pMscCtrl->BulkLen;
	pMscCtrl->Length -= pMscCtrl->BulkLen;

	pMscCtrl->CSW.dDataResidue -= pMscCtrl->BulkLen;

	if ((pMscCtrl->Length == 0) || (pMscCtrl->BulkStage == MSC_BS_CSW)) {
		pMscCtrl->CSW.bStatus = CSW_CMD_PASSED;
		mwMSC_SetCSW(pMscCtrl);
		/* transfer is done revert the rx_buff to be same as BukBuf */
		pMscCtrl->rx_buf = pMscCtrl->BulkBuf;
	}
	else {
		/* check if more data is coming to enqueue hwUSB_ReadReqEP */
		/* re-use queue logic present in mwMSC_BulkOutNak() function */
		mwMSC_ReadReqBulkEp(pMscCtrl, pMscCtrl->rx_buf);
	}
}

/*
 *  MSC Memory Verify Callback
 *  Called automatically on Memory Verify Event
 *  Parameters:      pMscCtrl: Handle to MSC structure (global variables)
 *  Return Value:    None
 */

void mwMSC_MemoryVerify(USB_MSC_CTRL_T *pMscCtrl) {

	/* MemorySize check is done in mwMSC_RWSetup */
	/* read data using user callback and verify */
	if (pMscCtrl->MSC_Verify(((uint32_t) pMscCtrl->Offset & 0xFFFFFFFF), pMscCtrl->BulkBuf, pMscCtrl->BulkLen,
							 (pMscCtrl->Offset >> 32)) != LPC_OK) {
		pMscCtrl->MemOK = FALSE;
	}

	pMscCtrl->Offset += pMscCtrl->BulkLen;
	pMscCtrl->Length -= pMscCtrl->BulkLen;

	pMscCtrl->CSW.dDataResidue -= pMscCtrl->BulkLen;

	if ((pMscCtrl->Length == 0) || (pMscCtrl->BulkStage == MSC_BS_CSW)) {
		pMscCtrl->CSW.bStatus = (pMscCtrl->MemOK) ? CSW_CMD_PASSED : CSW_CMD_FAILED;
		mwMSC_SetCSW(pMscCtrl);
	}
}

/*
 *  MSC SCSI Read/Write Setup Callback
 *  Parameters:      pMscCtrl: Handle to MSC structure (global variables)
 *  Return Value:    ErrorCode_t type to indicate success or error condition.
 */

ErrorCode_t mwMSC_RWSetup(USB_MSC_CTRL_T *pMscCtrl) {
	uint32_t n;

	/* Logical Block Address of First Block */
	n = (pMscCtrl->CBW.CB[2] << 24) |
		(pMscCtrl->CBW.CB[3] << 16) |
		(pMscCtrl->CBW.CB[4] <<  8) |
		(pMscCtrl->CBW.CB[5] <<  0);

	pMscCtrl->Offset = n * pMscCtrl->BlockSize;

	/* Number of Blocks to transfer */
	switch (pMscCtrl->CBW.CB[0]) {
	case SCSI_READ10:
	case SCSI_WRITE10:
	case SCSI_VERIFY10:
		n = (pMscCtrl->CBW.CB[7] <<  8) |
			(pMscCtrl->CBW.CB[8] <<  0);
		break;

	case SCSI_READ12:
	case SCSI_WRITE12:
		n = (pMscCtrl->CBW.CB[6] << 24) |
			(pMscCtrl->CBW.CB[7] << 16) |
			(pMscCtrl->CBW.CB[8] <<  8) |
			(pMscCtrl->CBW.CB[9] <<  0);
		break;

	default:
		break;
	}

	pMscCtrl->Length = n * pMscCtrl->BlockSize;

	if (pMscCtrl->CBW.dDataLength == 0) {
		/* host requests no data */
		pMscCtrl->CSW.bStatus = CSW_CMD_FAILED;
		mwMSC_SetCSW(pMscCtrl);
		return ERR_USBD_INVALID_REQ;
	}

	if ( (pMscCtrl->CBW.dDataLength != pMscCtrl->Length) ||
		 ((pMscCtrl->Offset + pMscCtrl->Length) > pMscCtrl->MemorySize) ) {
		if ((pMscCtrl->CBW.bmFlags & 0x80) != 0) {	/* stall appropriate EP */
			mwMSC_SetStallEP(pMscCtrl, pMscCtrl->epin_num);
		}
		else {
			mwMSC_SetStallEP(pMscCtrl, pMscCtrl->epout_num);
		}

		pMscCtrl->CSW.bStatus = CSW_CMD_FAILED;
		mwMSC_SetCSW(pMscCtrl);

		return ERR_USBD_INVALID_REQ;
	}

	return LPC_OK;
}

/*
 *  Check Data IN Format
 *  Parameters:      pMscCtrl: Handle to MSC structure (global variables)
 *  Return Value:    ErrorCode_t type to indicate success or error condition.
 */

ErrorCode_t mwMSC_DataInFormat(USB_MSC_CTRL_T *pMscCtrl) {

	if (pMscCtrl->CBW.dDataLength == 0) {
		pMscCtrl->CSW.bStatus = CSW_PHASE_ERROR;
		mwMSC_SetCSW(pMscCtrl);
		return ERR_USBD_INVALID_REQ;
	}
	if ((pMscCtrl->CBW.bmFlags & 0x80) == 0) {
		mwMSC_SetStallEP(pMscCtrl, pMscCtrl->epout_num);
		pMscCtrl->CSW.bStatus = CSW_PHASE_ERROR;
		mwMSC_SetCSW(pMscCtrl);
		return ERR_USBD_INVALID_REQ;
	}
	return LPC_OK;
}

/*
 *  Perform Data IN Transfer
 *  Parameters:      pMscCtrl: Handle to MSC structure (global variables)
 *  Return Value:    None
 */

void mwMSC_DataInTransfer(USB_MSC_CTRL_T *pMscCtrl) {

	if (pMscCtrl->BulkLen >= pMscCtrl->CBW.dDataLength) {
		pMscCtrl->BulkLen = pMscCtrl->CBW.dDataLength;
		pMscCtrl->BulkStage = MSC_BS_DATA_IN_LAST;
	}
	else {
		pMscCtrl->BulkStage = MSC_BS_DATA_IN_LAST_STALL;/* short or zero packet */
	}

	pMscCtrl->pUsbCtrl->hw_api->WriteEP(pMscCtrl->pUsbCtrl, pMscCtrl->epin_num, pMscCtrl->BulkBuf, pMscCtrl->BulkLen);

	pMscCtrl->CSW.dDataResidue -= pMscCtrl->BulkLen;
	pMscCtrl->CSW.bStatus = CSW_CMD_PASSED;
}

/*
 *  MSC SCSI Test Unit Ready Callback
 *  Parameters:      pMscCtrl: Handle to MSC structure (global variables)
 *  Return Value:    None
 */

void mwMSC_TestUnitReady(USB_MSC_CTRL_T *pMscCtrl) {

	if (pMscCtrl->CBW.dDataLength != 0) {
		if ((pMscCtrl->CBW.bmFlags & 0x80) != 0) {
			mwMSC_SetStallEP(pMscCtrl, pMscCtrl->epin_num);
		}
		else {
			mwMSC_SetStallEP(pMscCtrl, pMscCtrl->epout_num);
		}
	}

	pMscCtrl->CSW.bStatus = CSW_CMD_PASSED;
	mwMSC_SetCSW(pMscCtrl);
}

/*
 *  MSC SCSI Request Sense Callback
 *  Parameters:      pMscCtrl: Handle to MSC structure (global variables)
 *  Return Value:    None
 */

void mwMSC_RequestSense(USB_MSC_CTRL_T *pMscCtrl) {

	if (mwMSC_DataInFormat(pMscCtrl) != LPC_OK) {
		return;
	}
	/*
	   pMscCtrl->BulkBuf[ 0] = 0x70;          // Response Code
	   pMscCtrl->BulkBuf[ 1] = 0x00;
	   pMscCtrl->BulkBuf[ 2] = 0x02;          // Sense Key
	   pMscCtrl->BulkBuf[ 3] = 0x00;
	   pMscCtrl->BulkBuf[ 4] = 0x00;
	   pMscCtrl->BulkBuf[ 5] = 0x00;
	   pMscCtrl->BulkBuf[ 6] = 0x00;
	   pMscCtrl->BulkBuf[ 7] = 0x0A;          // Additional Length
	   pMscCtrl->BulkBuf[ 8] = 0x00;
	   pMscCtrl->BulkBuf[ 9] = 0x00;
	   pMscCtrl->BulkBuf[10] = 0x00;
	   pMscCtrl->BulkBuf[11] = 0x00;
	   pMscCtrl->BulkBuf[12] = 0x30;          // ASC
	   pMscCtrl->BulkBuf[13] = 0x01;          // ASCQ
	   pMscCtrl->BulkBuf[14] = 0x00;
	   pMscCtrl->BulkBuf[15] = 0x00;
	   pMscCtrl->BulkBuf[16] = 0x00;
	   pMscCtrl->BulkBuf[17] = 0x00;
	 */
	memset((void *) &pMscCtrl->BulkBuf[0], 0, 18);
	pMscCtrl->BulkBuf[0] = 0x70;		// Response Code
	pMscCtrl->BulkBuf[2] = 0x02;		// Sense Key
	pMscCtrl->BulkBuf[7] = 0x0A;		// Additional Length
	pMscCtrl->BulkBuf[12] = 0x30;		// ASC
	pMscCtrl->BulkBuf[13] = 0x01;		// ASCQ

	pMscCtrl->BulkLen = 18;
	mwMSC_DataInTransfer(pMscCtrl);
}

/*
 *  MSC SCSI Inquiry Callback
 *  Parameters:      pMscCtrl: Handle to MSC structure (global variables)
 *  Return Value:    None
 */

void mwMSC_Inquiry(USB_MSC_CTRL_T *pMscCtrl) {

	uint32_t i;

	if (mwMSC_DataInFormat(pMscCtrl) != LPC_OK) {
		return;
	}

	pMscCtrl->BulkBuf[0] = 0x00;		/* Direct Access Device */
	pMscCtrl->BulkBuf[1] = 0x80;		/* RMB = 1: Removable Medium */
	pMscCtrl->BulkBuf[2] = 0x00;		/* Version: No conformance claim to standard */
	pMscCtrl->BulkBuf[3] = 0x01;

	pMscCtrl->BulkBuf[4] = 36 - 4;		/* Additional Length */
	pMscCtrl->BulkBuf[5] = 0x80;		/* SCCS = 1: Storage Controller Component */
	pMscCtrl->BulkBuf[6] = 0x00;
	pMscCtrl->BulkBuf[7] = 0x00;

	/* Vendor Identification */
	/* Product Identification */
	/* Product Revision Level */
	for (i = 0; i < 28; i++) {
		pMscCtrl->BulkBuf[i + 8] = pMscCtrl->InquiryStr[i];
	}

	pMscCtrl->BulkLen = 36;
	mwMSC_DataInTransfer(pMscCtrl);
}

/*
 *  MSC SCSI Mode Sense (6-Byte) Callback
 *  Parameters:      pMscCtrl: Handle to MSC structure (global variables)
 *  Return Value:    None
 */

void mwMSC_ModeSense6(USB_MSC_CTRL_T *pMscCtrl) {

	if (mwMSC_DataInFormat(pMscCtrl) != LPC_OK) {
		return;
	}

	pMscCtrl->BulkBuf[0] = 0x03;
	pMscCtrl->BulkBuf[1] = 0x00;
	pMscCtrl->BulkBuf[2] = 0x00;
	pMscCtrl->BulkBuf[3] = 0x00;

	pMscCtrl->BulkLen = 4;
	mwMSC_DataInTransfer(pMscCtrl);
}

/*
 *  MSC SCSI Mode Sense (10-Byte) Callback
 *  Parameters:      pMscCtrl: Handle to MSC structure (global variables)
 *  Return Value:    None
 */

void mwMSC_ModeSense10(USB_MSC_CTRL_T *pMscCtrl) {

	if (mwMSC_DataInFormat(pMscCtrl) != LPC_OK) {
		return;
	}
	/*
	   pMscCtrl->BulkBuf[ 0] = 0x00;
	   pMscCtrl->BulkBuf[ 1] = 0x06;
	   pMscCtrl->BulkBuf[ 2] = 0x00;
	   pMscCtrl->BulkBuf[ 3] = 0x00;
	   pMscCtrl->BulkBuf[ 4] = 0x00;
	   pMscCtrl->BulkBuf[ 5] = 0x00;
	   pMscCtrl->BulkBuf[ 6] = 0x00;
	   pMscCtrl->BulkBuf[ 7] = 0x00;
	 */
	memset((void *) &pMscCtrl->BulkBuf[0], 0, 8);
	pMscCtrl->BulkBuf[1] = 0x06;

	pMscCtrl->BulkLen = 8;
	mwMSC_DataInTransfer(pMscCtrl);
}

/*
 *  MSC SCSI Read Capacity Callback
 *  Parameters:      pMscCtrl: Handle to MSC structure (global variables)
 *  Return Value:    None
 */

void mwMSC_ReadCapacity(USB_MSC_CTRL_T *pMscCtrl) {

	if (mwMSC_DataInFormat(pMscCtrl) != LPC_OK) {
		return;
	}

	/* Last Logical Block */
	pMscCtrl->BulkBuf[0] = ((pMscCtrl->BlockCount - 1) >> 24) & 0xFF;
	pMscCtrl->BulkBuf[1] = ((pMscCtrl->BlockCount - 1) >> 16) & 0xFF;
	pMscCtrl->BulkBuf[2] = ((pMscCtrl->BlockCount - 1) >>  8) & 0xFF;
	pMscCtrl->BulkBuf[3] = ((pMscCtrl->BlockCount - 1) >>  0) & 0xFF;

	/* Block Length */
	pMscCtrl->BulkBuf[4] = (pMscCtrl->BlockSize >> 24) & 0xFF;
	pMscCtrl->BulkBuf[5] = (pMscCtrl->BlockSize >> 16) & 0xFF;
	pMscCtrl->BulkBuf[6] = (pMscCtrl->BlockSize >>  8) & 0xFF;
	pMscCtrl->BulkBuf[7] = (pMscCtrl->BlockSize >>  0) & 0xFF;

	pMscCtrl->BulkLen = 8;
	mwMSC_DataInTransfer(pMscCtrl);
}

/*
 *  MSC SCSI Read Format Capacity Callback
 *  Parameters:      pMscCtrl: Handle to MSC structure (global variables)
 *  Return Value:    None
 */

void mwMSC_ReadFormatCapacity(USB_MSC_CTRL_T *pMscCtrl) {

	if (mwMSC_DataInFormat(pMscCtrl) != LPC_OK) {
		return;
	}

	pMscCtrl->BulkBuf[0] = 0x00;
	pMscCtrl->BulkBuf[1] = 0x00;
	pMscCtrl->BulkBuf[2] = 0x00;
	pMscCtrl->BulkBuf[3] = 0x08;		/* Capacity List Length */

	/* Block Count */
	pMscCtrl->BulkBuf[4] = (pMscCtrl->BlockCount >> 24) & 0xFF;
	pMscCtrl->BulkBuf[5] = (pMscCtrl->BlockCount >> 16) & 0xFF;
	pMscCtrl->BulkBuf[6] = (pMscCtrl->BlockCount >>  8) & 0xFF;
	pMscCtrl->BulkBuf[7] = (pMscCtrl->BlockCount >>  0) & 0xFF;

	/* Block Length */
	pMscCtrl->BulkBuf[8] = 0x02;		/* Descriptor Code: Formatted Media */
	pMscCtrl->BulkBuf[9] = (pMscCtrl->BlockSize >> 16) & 0xFF;
	pMscCtrl->BulkBuf[10] = (pMscCtrl->BlockSize >>  8) & 0xFF;
	pMscCtrl->BulkBuf[11] = (pMscCtrl->BlockSize >>  0) & 0xFF;

	pMscCtrl->BulkLen = 12;
	mwMSC_DataInTransfer(pMscCtrl);
}

/*
 *  MSC Get Command Block Wrapper Callback
 *  Parameters:      pMscCtrl: Handle to MSC structure (global variables)
 *  Return Value:    None
 */

void mwMSC_GetCBW(USB_MSC_CTRL_T *pMscCtrl) {
	uint32_t n;

	for (n = 0; n < sizeof(MSC_CBW); n++) {
		*((uint8_t *) &pMscCtrl->CBW + n) = pMscCtrl->BulkBuf[n];
	}
	if ((pMscCtrl->BulkLen == sizeof(MSC_CBW)) && (pMscCtrl->CBW.dSignature == MSC_CBW_Signature)) {
		/* Valid CBW */
		pMscCtrl->CSW.dTag = pMscCtrl->CBW.dTag;
		pMscCtrl->CSW.dDataResidue = pMscCtrl->CBW.dDataLength;
		if ((pMscCtrl->CBW.bLUN != 0)     ||
			(pMscCtrl->CBW.bCBLength < 1) ||
			(pMscCtrl->CBW.bCBLength > 16) ) {
fail:
			if (pMscCtrl->CBW.dDataLength != 0) {
				if (pMscCtrl->CBW.bmFlags & 0x80)
					mwMSC_SetStallEP(pMscCtrl, pMscCtrl->epin_num);
				else
					mwMSC_SetStallEP(pMscCtrl, pMscCtrl->epout_num);
			}
			pMscCtrl->CSW.bStatus = CSW_CMD_FAILED;
			mwMSC_SetCSW(pMscCtrl);
		}
		else {
			switch (pMscCtrl->CBW.CB[0]) {
			case SCSI_TEST_UNIT_READY:
				mwMSC_TestUnitReady(pMscCtrl);
				break;

			case SCSI_REQUEST_SENSE:
				mwMSC_RequestSense(pMscCtrl);
				break;

			case SCSI_INQUIRY:
				mwMSC_Inquiry(pMscCtrl);
				break;

			case SCSI_MODE_SENSE6:
				mwMSC_ModeSense6(pMscCtrl);
				break;

			case SCSI_MODE_SENSE10:
				mwMSC_ModeSense10(pMscCtrl);
				break;

			case SCSI_READ_FORMAT_CAPACITIES:
				mwMSC_ReadFormatCapacity(pMscCtrl);
				break;

			case SCSI_READ_CAPACITY:
				mwMSC_ReadCapacity(pMscCtrl);
				break;

			case SCSI_READ10:
			case SCSI_READ12:
				if (mwMSC_RWSetup(pMscCtrl) == LPC_OK) {
					if ((pMscCtrl->CBW.bmFlags & 0x80) != 0) {
						pMscCtrl->BulkStage = MSC_BS_DATA_IN;
						mwMSC_MemoryRead(pMscCtrl);
					}
					else {
						mwMSC_SetStallEP(pMscCtrl, pMscCtrl->epout_num);
						pMscCtrl->CSW.bStatus = CSW_PHASE_ERROR;
						mwMSC_SetCSW(pMscCtrl);
					}
				}
				break;

			case SCSI_WRITE10:
			case SCSI_WRITE12:
				if (mwMSC_RWSetup(pMscCtrl) == LPC_OK) {
					if ((pMscCtrl->CBW.bmFlags & 0x80) == 0) {
						pMscCtrl->BulkStage = MSC_BS_DATA_OUT;
						pMscCtrl->rx_buf = pMscCtrl->BulkBuf;
						/* get destination buffer */
						if (pMscCtrl->MSC_GetWriteBuf) {
							pMscCtrl->MSC_GetWriteBuf(((uint32_t) pMscCtrl->Offset & 0xFFFFFFFF),
													  &pMscCtrl->rx_buf,
													  pMscCtrl->BulkLen,
													  (pMscCtrl->Offset >> 32));
						}
						mwMSC_ReadReqBulkEp(pMscCtrl, pMscCtrl->rx_buf);
					}
					else {
						mwMSC_SetStallEP(pMscCtrl, pMscCtrl->epin_num);
						pMscCtrl->CSW.bStatus = CSW_PHASE_ERROR;
						mwMSC_SetCSW(pMscCtrl);
					}
				}
				break;

			case SCSI_VERIFY10:
				if ((pMscCtrl->CBW.CB[1] & 0x02) == 0) {
					// BYTCHK = 0 -> CRC Check (not implemented)
					pMscCtrl->CSW.bStatus = CSW_CMD_PASSED;
					mwMSC_SetCSW(pMscCtrl);
					break;
				}

				if (mwMSC_RWSetup(pMscCtrl) == LPC_OK) {
					if ((pMscCtrl->CBW.bmFlags & 0x80) == 0) {
						pMscCtrl->BulkStage = MSC_BS_DATA_OUT;
						pMscCtrl->MemOK = TRUE;
					}
					else {
						mwMSC_SetStallEP(pMscCtrl, pMscCtrl->epin_num);
						pMscCtrl->CSW.bStatus = CSW_PHASE_ERROR;
						mwMSC_SetCSW(pMscCtrl);
					}
				}
				break;

			case SCSI_FORMAT_UNIT:
			case SCSI_START_STOP_UNIT:
			case SCSI_MEDIA_REMOVAL:
			case SCSI_MODE_SELECT10:
			case SCSI_MODE_SELECT6:
			default:
				goto fail;
			}
		}
	}
	else {
		/* Invalid CBW */
		mwMSC_SetStallEP(pMscCtrl, pMscCtrl->epin_num);
		/* set EP to stay stalled */
		pMscCtrl->pUsbCtrl->ep_stall |=  (1 << (16 + (pMscCtrl->epin_num  & 0x0F)));
		mwMSC_SetStallEP(pMscCtrl, pMscCtrl->epout_num);
		/* set EP to stay stalled */
		pMscCtrl->pUsbCtrl->ep_stall |=  (1 << pMscCtrl->epout_num);
		pMscCtrl->BulkStage = MSC_BS_ERROR;
	}
}

/*
 *  MSC Set Command Status Wrapper Callback
 *  Parameters:      pMscCtrl: Handle to MSC structure (global variables)
 *  Return Value:    None
 */

void mwMSC_SetCSW(USB_MSC_CTRL_T *pMscCtrl) {

	pMscCtrl->CSW.dSignature = MSC_CSW_Signature;
	pMscCtrl->pUsbCtrl->hw_api->WriteEP(pMscCtrl->pUsbCtrl, pMscCtrl->epin_num, (uint8_t *) &pMscCtrl->CSW,
										sizeof(MSC_CSW));
	pMscCtrl->BulkStage = MSC_BS_CSW;
}

/*
 *  MSC Bulk In Callback
 *  Parameters:      pMscCtrl: Handle to MSC structure (global variables)
 *  Return Value:    None
 */

void mwMSC_BulkIn(USB_MSC_CTRL_T *pMscCtrl) {

	switch (pMscCtrl->BulkStage) {
	case MSC_BS_DATA_IN:
		switch (pMscCtrl->CBW.CB[0]) {
		case SCSI_READ10:
		case SCSI_READ12:
			mwMSC_MemoryRead(pMscCtrl);
			break;

		default:
			break;
		}
		break;

	case MSC_BS_DATA_IN_LAST:
		mwMSC_SetCSW(pMscCtrl);
		break;

	case MSC_BS_DATA_IN_LAST_STALL:
		mwMSC_SetStallEP(pMscCtrl, pMscCtrl->epin_num);
		mwMSC_SetCSW(pMscCtrl);
		break;

	case MSC_BS_CSW:
		pMscCtrl->BulkStage = MSC_BS_CBW;
		break;

	default:
		break;
	}
}

/*
 *  MSC Bulk Out Callback
 *  Parameters:      pMscCtrl: Handle to MSC structure (global variables)
 *  Return Value:    None
 */

void mwMSC_BulkOut(USB_MSC_CTRL_T *pMscCtrl) {

	pMscCtrl->BulkLen = pMscCtrl->pUsbCtrl->hw_api->ReadEP(pMscCtrl->pUsbCtrl, pMscCtrl->epout_num, pMscCtrl->rx_buf);
	switch (pMscCtrl->BulkStage) {
	case MSC_BS_CBW:
		mwMSC_GetCBW(pMscCtrl);
		break;

	case MSC_BS_DATA_OUT:
		switch (pMscCtrl->CBW.CB[0]) {
		case SCSI_WRITE10:
		case SCSI_WRITE12:
			mwMSC_MemoryWrite(pMscCtrl);
			break;

		case SCSI_VERIFY10:
			mwMSC_MemoryVerify(pMscCtrl);
			break;

		default:
			break;
		}
		break;

	case MSC_BS_CSW:
		break;

	default:
		mwMSC_SetStallEP(pMscCtrl, pMscCtrl->epout_num);
		pMscCtrl->CSW.bStatus = CSW_PHASE_ERROR;
		mwMSC_SetCSW(pMscCtrl);
		break;
	}
}

/*
 *  Default MSC Class Handler
 *  Parameters:     hUsb: Handle to the USB device stack.
 *                                  data: Pointer to the data which will be passed when callback function is called by the stack.
 *                                  event:  Type of endpoint event. See \ref USBD_EVENT_T for more details.
 *  Return Value:   ErrorCode_t type to indicate success or error condition.
 */

ErrorCode_t mwMSC_ep0_hdlr(USBD_HANDLE_T hUsb, void *data, uint32_t event)
{
	USB_CORE_CTRL_T *pCtrl = (USB_CORE_CTRL_T *) hUsb;
	USB_MSC_CTRL_T *pMscCtrl = (USB_MSC_CTRL_T *) data;
	ErrorCode_t ret = ERR_USBD_UNHANDLED;
	uint32_t n, m;

	/* Check if the request is for this instance of interface. IF not return immediately. */
	// if
	//  return ret;

	switch (event) {
	case USB_EVT_SETUP:
		if ((pCtrl->SetupPacket.bmRequestType.BM.Type == REQUEST_CLASS) &&
			(pCtrl->SetupPacket.bmRequestType.BM.Recipient == REQUEST_TO_INTERFACE) &&
			(pCtrl->SetupPacket.wIndex.WB.L == pMscCtrl->if_num) ) {
			switch (pCtrl->SetupPacket.bRequest) {
			case MSC_REQUEST_RESET:
				if ((pCtrl->SetupPacket.wValue.W == 0) &&				/* RESET with invalid parameters -> STALL */
					(pCtrl->SetupPacket.wLength  == 0)) {
					if (mwMSC_Reset(pMscCtrl) == LPC_OK) {
						mwUSB_StatusInStage(pCtrl);
						return LPC_OK;
					}
				}
				break;

			case MSC_REQUEST_GET_MAX_LUN:
				if ((pCtrl->SetupPacket.wValue.W == 0) &&				/* GET_MAX_LUN with invalid parameters -> STALL */
					(pCtrl->SetupPacket.wLength  == 1)) {
					if (mwMSC_GetMaxLUN(pMscCtrl) == LPC_OK) {
						pCtrl->EP0Data.pData = pCtrl->EP0Buf;
						mwUSB_DataInStage(pCtrl);
						return LPC_OK;
					}
				}
				break;

			default:
				break;
			}
		}
		else if ((pCtrl->SetupPacket.bRequest == USB_REQUEST_CLEAR_FEATURE) &&
				 (pCtrl->SetupPacket.bmRequestType.BM.Recipient == REQUEST_TO_ENDPOINT) &&
				 (pCtrl->SetupPacket.wValue.W == USB_FEATURE_ENDPOINT_STALL)) {
			/* handle special case for clear stall on BULK EP */
			n = pCtrl->SetupPacket.wIndex.WB.L & 0x8F;
			m = (n & 0x80) ? ((1 << 16) << (n & 0x0F)) : (1 << n);
			if ((pCtrl->config_value != 0) && (pCtrl->ep_mask & m) &&
				(n == pMscCtrl->epin_num) && ((pCtrl->ep_halt & m) != 0)) {

				if ((pCtrl->ep_stall & m) == 0) {
					pMscCtrl->pUsbCtrl->hw_api->ClrStallEP(pMscCtrl->pUsbCtrl, n);
					mwUSB_StatusInStage(pMscCtrl->pUsbCtrl);
					/* Compliance Test: rewrite CSW after unstall */
					if (pMscCtrl->CSW.dSignature == MSC_CSW_Signature) {
						pMscCtrl->pUsbCtrl->hw_api->WriteEP(pMscCtrl->pUsbCtrl,
															pMscCtrl->epin_num,
															(uint8_t *) &pMscCtrl->CSW,
															sizeof(MSC_CSW));
					}
					pCtrl->ep_halt &= ~m;
				}
				else {
					mwUSB_StatusInStage(pMscCtrl->pUsbCtrl);
				}

				if (pCtrl->USB_Feature_Event) {
					pCtrl->USB_Feature_Event(pCtrl);
				}

				return LPC_OK;
			}

		}
		break;
  
	case USB_EVT_RESET:
		/* Fix for artf548083 we should reset the MSC controller is USB reset is received.*/
		mwMSC_Reset(pMscCtrl);
		break;

	default:
		break;
	}
	return ret;
}

/*
 *  Default MSC Bulk IN Handler
 *  Parameters:     hUsb: Handle to the USB device stack.
 *                                  data: Pointer to the data which will be passed when callback function is called by the stack.
 *                                  event:  Type of endpoint event. See \ref USBD_EVENT_T for more details.
 *  Return Value:   ErrorCode_t type to indicate success or error condition.
 */

ErrorCode_t mwMSC_bulk_in_hdlr(USBD_HANDLE_T hUsb, void *data, uint32_t event)
{
	USB_MSC_CTRL_T *pMscCtrl = (USB_MSC_CTRL_T *) data;

	if (event == USB_EVT_IN) {
		mwMSC_BulkIn(pMscCtrl);
	}
	return LPC_OK;
}

/*
 *  Default MSC Bulk OUT Handler
 *  Parameters:     hUsb: Handle to the USB device stack.
 *                                  data: Pointer to the data which will be passed when callback function is called by the stack.
 *                                  event:  Type of endpoint event. See \ref USBD_EVENT_T for more details.
 *  Return Value:   ErrorCode_t type to indicate success or error condition.
 */

ErrorCode_t mwMSC_bulk_out_hdlr(USBD_HANDLE_T hUsb, void *data, uint32_t event)
{
	USB_MSC_CTRL_T *pMscCtrl = (USB_MSC_CTRL_T *) data;
	switch (event) {
	case USB_EVT_OUT_NAK:
		mwMSC_ReadReqBulkEp(pMscCtrl, pMscCtrl->BulkBuf);
		break;

	case USB_EVT_OUT:
		mwMSC_BulkOut(pMscCtrl);
		break;

	default:
		break;
	}
	return LPC_OK;
}

/**
 * @brief   Get memory required by MSC class.
 * @param [in/out] param parameter structure used for initialisation.
 * @retval  Length required for MSC data structure and buffers.
 *
 * Example Usage:
 * @code
 *    mem_req = mwMSC_GetMemSize(param);
 * @endcode
 */
uint32_t mwMSC_GetMemSize(USBD_MSC_INIT_PARAM_T *param)
{
	uint32_t req_len = 0;

	/* calculate required length */
	req_len += sizeof(USB_MSC_CTRL_T);	/* memory for MSC controller structure */
	req_len += 4;	/* for alignment overhead */
	req_len &= ~0x3;

	return req_len;
}

/*
 *  MSC function initialization routine
 *  Parameters:     hUsb: Handle to the USB device stack.
 *                                  param: Structure containing MSC function driver module
 *						      initialization parameters.
 *  Return Value:   ErrorCode_t type to indicate success or error condition.
 */
ErrorCode_t mwMSC_init(USBD_HANDLE_T hUsb, USBD_MSC_INIT_PARAM_T *param)
{
	uint32_t new_addr, i, ep_indx;
	ErrorCode_t ret = LPC_OK;
	USB_MSC_CTRL_T *pMscCtrl;
	USB_ENDPOINT_DESCRIPTOR *pEpDesc;
	USB_INTERFACE_DESCRIPTOR *pIntfDesc = (USB_INTERFACE_DESCRIPTOR *) param->intf_desc;

	/* check for memory alignment */
	if ((param->mem_base &  0x3) &&
		(param->mem_size < sizeof(USB_MSC_CTRL_T))) {
		return ERR_USBD_BAD_MEM_BUF;
	}

	/* allocate memory for the control data structure */
	pMscCtrl = (USB_MSC_CTRL_T *) param->mem_base;
	param->mem_base += sizeof(USB_MSC_CTRL_T);
	param->mem_size -= sizeof(USB_MSC_CTRL_T);
	/* align to 4 byte boundary */
	while (param->mem_base & 0x03) {
		param->mem_base++;
		param->mem_size--;
	}

	/* Init control structures with passed params */
	memset((void *) pMscCtrl, 0, sizeof(USB_MSC_CTRL_T));
	pMscCtrl->pUsbCtrl = (USB_CORE_CTRL_T *) hUsb;
	pMscCtrl->InquiryStr = param->InquiryStr;
	pMscCtrl->BlockCount = param->BlockCount;
	pMscCtrl->BlockSize = param->BlockSize;
	pMscCtrl->MemorySize = (param->MemorySize == 0) ? param->MemorySize64 : param->MemorySize;
	/* user defined functions */
	if ((param->MSC_Write == 0) ||
		(param->MSC_Read == 0) ||
		(param->MSC_Verify == 0)) {
		return ERR_API_INVALID_PARAM2;
	}

	pMscCtrl->MSC_Write = param->MSC_Write;
	pMscCtrl->MSC_Read = param->MSC_Read;
	pMscCtrl->MSC_Verify = param->MSC_Verify;
	pMscCtrl->MSC_GetWriteBuf = param->MSC_GetWriteBuf;
	pMscCtrl->rx_buf = pMscCtrl->BulkBuf;

	/* parse the interface descriptor */
	if ((pIntfDesc->bDescriptorType == USB_INTERFACE_DESCRIPTOR_TYPE) &&
		(pIntfDesc->bInterfaceClass == USB_DEVICE_CLASS_STORAGE) &&
		(pIntfDesc->bInterfaceSubClass == MSC_SUBCLASS_SCSI) &&
		(pIntfDesc->bNumEndpoints >= 2) ) {

		/* store interface number */
		pMscCtrl->if_num = pIntfDesc->bInterfaceNumber;
		new_addr = (uint32_t) pIntfDesc + pIntfDesc->bLength;
		/* move to next descriptor */
		for (i = 0; i < pIntfDesc->bNumEndpoints; i++) {
			pEpDesc = (USB_ENDPOINT_DESCRIPTOR *) new_addr;
			new_addr = (uint32_t) pEpDesc + pEpDesc->bLength;

			/* parse endpoint descriptor */
			if ((pEpDesc->bDescriptorType == USB_ENDPOINT_DESCRIPTOR_TYPE) &&
				(pEpDesc->bmAttributes == USB_ENDPOINT_TYPE_BULK)) {

				if (pEpDesc->bEndpointAddress & USB_ENDPOINT_DIRECTION_MASK) {
					/* store BULK IN endpoint */
					pMscCtrl->epin_num = pEpDesc->bEndpointAddress;
					ep_indx = ((pMscCtrl->epin_num & 0x0F) << 1) + 1;
					/* register endpoint interrupt handler */
					ret = mwUSB_RegisterEpHandler(hUsb, ep_indx, mwMSC_bulk_in_hdlr, pMscCtrl);
				}
				else {
					/* store BULK OUT endpoint */
					pMscCtrl->epout_num = pEpDesc->bEndpointAddress;
					ep_indx = ((pMscCtrl->epout_num & 0x0F) << 1);
					/* register endpoint interrupt handler */
					ret = mwUSB_RegisterEpHandler(hUsb, ep_indx, mwMSC_bulk_out_hdlr, pMscCtrl);
				}
			}
		}

	}
	else {
		return ERR_USBD_BAD_INTF_DESC;
	}

	if ( (pMscCtrl->epin_num == 0) || (pMscCtrl->epout_num == 0) || (ret != LPC_OK) ) {
		return ERR_USBD_BAD_EP_DESC;
	}

	/* register ep0 handler */
	/* check if user wants his own handler */
	if (param->MSC_Ep0_Hdlr == 0) {
		ret = mwUSB_RegisterClassHandler(hUsb, mwMSC_ep0_hdlr, pMscCtrl);
	}
	else {
		ret = mwUSB_RegisterClassHandler(hUsb, param->MSC_Ep0_Hdlr, pMscCtrl);
		param->MSC_Ep0_Hdlr = mwMSC_ep0_hdlr;
	}

	return ret;
}
