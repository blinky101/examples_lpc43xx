#if 0
/***********************************************************************
 * $Id:: mw_usbd_adcuser.c 165 2011-04-14 17:41:11Z usb10131                   $
 *
 * Project: USB device ROM Stack
 *
 * Description:
 *     USB Audio Device Class Custom User Module.
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

/* USB header files */
#include "mw_usbd.h"
#include "mw_usbd_core.h"
#include "mw_usbd_audio.h"
#include "mw_usbd_adcuser.h"

volatile uint16_t VolCur = 0x003D;		/* Volume Current Value */
uint16_t VolMin = 0x0000;		/* Volume Minimum Value */
uint16_t VolMax = 0x003F;		/* Volume Maximum Value */
uint16_t VolRes = 0x0001;		/* Volume Resolution */
uint32_t g_sample_rate[2] = {48000, 48000};

/*
 *  Audio Device Class Interface Get Request Callback
 *   Called automatically on ADC Interface Get Request
 *    Parameters:      None (global SetupPacket and EP0Buf)
 *    Return Value:    LPC_OK - Success, ERR_USBD_INVALID_REQ - Error
 */

ErrorCode_t ADC_IF_GetRequest(void) {

	/*
	   Interface = SetupPacket.wIndex.WB.L;
	   EntityID  = SetupPacket.wIndex.WB.H;
	   Request   = SetupPacket.bRequest;
	   Value     = SetupPacket.wValue.W;
	   ...
	 */

	if (pCtrl->SetupPacket.wIndex.W == 0x0200) {
		/* Feature Unit: Interface = 0, ID = 2 */
		if (pCtrl->SetupPacket.wValue.WB.L == 0) {
			/* Master Channel */
			switch (pCtrl->SetupPacket.wValue.WB.H) {
			case AUDIO_MUTE_CONTROL:
				switch (pCtrl->SetupPacket.bRequest) {
				case AUDIO_REQUEST_GET_CUR:
					pCtrl->EP0Buf[0] = Mute;
					return LPC_OK;
				}
				break;

			case AUDIO_VOLUME_CONTROL:
				switch (pCtrl->SetupPacket.bRequest) {
				case AUDIO_REQUEST_GET_CUR:
					*((__packed uint16_t *) pCtrl->EP0Buf) = VolCur;
					return LPC_OK;

				case AUDIO_REQUEST_GET_MIN:
					*((__packed uint16_t *) pCtrl->EP0Buf) = VolMin;
					return LPC_OK;

				case AUDIO_REQUEST_GET_MAX:
					*((__packed uint16_t *) pCtrl->EP0Buf) = VolMax;
					return LPC_OK;

				case AUDIO_REQUEST_GET_RES:
					*((__packed uint16_t *) pCtrl->EP0Buf) = VolRes;
					return LPC_OK;
				}
				break;
			}
		}
	}

	return ERR_USBD_INVALID_REQ;	/* Not Supported */
}

/*
 *  Audio Device Class Interface Set Request Callback
 *   Called automatically on ADC Interface Set Request
 *    Parameters:      None (global SetupPacket and EP0Buf)
 *    Return Value:    LPC_OK - Success, ERR_USBD_INVALID_REQ - Error
 */

BOOL_32 ADC_IF_SetRequest(void) {

	/*
	   Interface = SetupPacket.wIndex.WB.L;
	   EntityID  = SetupPacket.wIndex.WB.H;
	   Request   = SetupPacket.bRequest;
	   Value     = SetupPacket.wValue.W;
	   ...
	 */
	if (pCtrl->SetupPacket.wIndex.W == 0x0200) {
		/* Feature Unit: Interface = 0, ID = 2 */
		if (SetupPacket.wValue.WB.L == 0) {
			/* Master Channel */
			switch (pCtrl->SetupPacket.wValue.WB.H) {
			case AUDIO_MUTE_CONTROL:
				switch (pCtrl->SetupPacket.bRequest) {
				case AUDIO_REQUEST_SET_CUR:
					Mute = pCtrl->EP0Buf[0];
					return LPC_OK;
				}
				break;

			case AUDIO_VOLUME_CONTROL:
				switch (pCtrl->SetupPacket.bRequest) {
				case AUDIO_REQUEST_SET_CUR:
					I2S_set_volume(*((__packed uint16_t *) pCtrl->EP0Buf));
					return LPC_OK;
				}
				break;
			}
		}
	}

	return ERR_USBD_INVALID_REQ;	/* Not Supported */
}

/*
 *  Audio Device Class EndPoint Get Request Callback
 *   Called automatically on ADC EndPoint Get Request
 *    Parameters:      None (global SetupPacket and EP0Buf)
 *    Return Value:    LPC_OK - Success, ERR_USBD_INVALID_REQ - Error
 */

ErrorCode_t ADC_EP_GetRequest(void) {

	/*
	   EndPoint = SetupPacket.wIndex.WB.L;
	   Request  = SetupPacket.bRequest;
	   Value    = SetupPacket.wValue.W;
	   ...
	 */
	int dir = CODEC_DIR_SPEAKER;

	switch (pCtrl->SetupPacket.wIndex.W) {
	case USB_ADC_IN_EP:
		dir = CODEC_DIR_MIC;

	case USB_ADC_OUT_EP:
		/* Feature Unit: Interface = 0, ID = 2 */
		if (pCtrl->SetupPacket.wValue.WB.L == 0) {
			/* Master Channel */
			if (pCtrl->SetupPacket.wValue.WB.H == AUDIO_CONTROL_SAMPLING_FREQ) {
				if (pCtrl->SetupPacket.bRequest == AUDIO_REQUEST_GET_CUR) {
					pCtrl->EP0Buf[0] = (uint8_t) (g_sample_rate[dir] & 0xFF);
					pCtrl->EP0Buf[1] = (uint8_t) ((g_sample_rate[dir] >> 8) & 0xFF);
					pCtrl->EP0Buf[2] = (uint8_t) ((g_sample_rate[dir] >> 16) & 0xFF);
					return LPC_OK;
				}
			}
		}
		break;

	default:
		break;
	}
	return ERR_USBD_INVALID_REQ;	/* Not Supported */
}

/*
 *  Audio Device Class EndPoint Set Request Callback
 *   Called automatically on ADC EndPoint Set Request
 *    Parameters:      None (global SetupPacket and EP0Buf)
 *    Return Value:    LPC_OK - Success, ERR_USBD_INVALID_REQ - Error
 */

ErrorCode_t ADC_EP_SetRequest(void) {

	/*
	   EndPoint = SetupPacket.wIndex.WB.L;
	   Request  = SetupPacket.bRequest;
	   Value    = SetupPacket.wValue.W;
	   ...
	 */
	int dir = CODEC_DIR_SPEAKER;
	uint32_t rate;

	switch (pCtrl->SetupPacket.wIndex.W) {
	case USB_ADC_IN_EP:
		dir = CODEC_DIR_MIC;

	case USB_ADC_OUT_EP:
		/* Feature Unit: Interface = 0, ID = 2 */
		if (pCtrl->SetupPacket.wValue.WB.L == 0) {
			/* Master Channel */
			if (pCtrl->SetupPacket.wValue.WB.H == AUDIO_CONTROL_SAMPLING_FREQ) {
				rate = pCtrl->EP0Buf[0] | (pCtrl->EP0Buf[1] << 8) | (pCtrl->EP0Buf[2] << 16);
				if (pCtrl->SetupPacket.bRequest == AUDIO_REQUEST_SET_CUR) {
					rate = I2S_set_sample_rate(dir, rate);
					if (rate != 0) {
						g_sample_rate[dir] = rate;
						return LPC_OK;
					}
				}
			}
		}
		break;

	default:
		break;
	}
	return ERR_USBD_INVALID_REQ;	/* Not Supported */
}

ErrorCode_t ADC_ep0_hdlr(USB_CORE_CTRL_T *pCtrl, void *data, uint32_t event)
{
	ErrorCode_t ret = ERR_USBD_UNHANDLED;
	if (pCtrl->SetupPacket.bmRequestType.BM.Type == REQUEST_CLASS) {
		switch (event) {
		case USB_EVT_SETUP:
			if ((pCtrl->SetupPacket.bmRequestType.BM.Recipient == REQUEST_TO_INTERFACE) &&
				((pCtrl->SetupPacket.wIndex.WB.L == USB_ADC_CIF_NUM)  ||	/* IF number correct? */
				 (pCtrl->SetupPacket.wIndex.WB.L == USB_ADC_SIF1_NUM) ||
				 (pCtrl->SetupPacket.wIndex.WB.L == USB_ADC_SIF2_NUM)) ) {
				switch (pCtrl->SetupPacket.bRequest) {
				case AUDIO_REQUEST_GET_CUR:
				case AUDIO_REQUEST_GET_MIN:
				case AUDIO_REQUEST_GET_MAX:
				case AUDIO_REQUEST_GET_RES:
					if (ADC_IF_GetRequest() == LPC_OK) {
						pCtrl->EP0Data.pData = pCtrl->EP0Buf;					/* point to data to be sent */
						mwUSB_DataInStage(pCtrl);							/* send requested data */
						return LPC_OK;
					}
					break;

				case AUDIO_REQUEST_SET_CUR:
					//                case AUDIO_REQUEST_SET_MIN:
					//                case AUDIO_REQUEST_SET_MAX:
					//                case AUDIO_REQUEST_SET_RES:
					pCtrl->EP0Data.pData = pCtrl->EP0Buf;						/* data to be received */
					return LPC_OK;
				}
			}
			else if (pCtrl->SetupPacket.bmRequestType.BM.Recipient == REQUEST_TO_ENDPOINT) {
				switch (pCtrl->SetupPacket.bRequest) {
				case AUDIO_REQUEST_GET_CUR:
				case AUDIO_REQUEST_GET_MIN:
				case AUDIO_REQUEST_GET_MAX:
				case AUDIO_REQUEST_GET_RES:
					if (ADC_EP_GetRequest() == LPC_OK) {
						pCtrl->EP0Data.pData = pCtrl->EP0Buf;						/* point to data to be sent */
						mwUSB_DataInStage(pCtrl);								/* send requested data */
						return LPC_OK;
					}
					break;

				case AUDIO_REQUEST_SET_CUR:
					//              case AUDIO_REQUEST_SET_MIN:
					//              case AUDIO_REQUEST_SET_MAX:
					//              case AUDIO_REQUEST_SET_RES:
					pCtrl->EP0Data.pData = pCtrl->EP0Buf;							/* data to be received */
					return LPC_OK;
				}
			}
			break;

		case USB_EVT_OUT:
			if ((pCtrl->SetupPacket.bmRequestType.BM.Recipient == REQUEST_TO_INTERFACE) &&
				((pCtrl->SetupPacket.wIndex.WB.L == USB_ADC_CIF_NUM)  ||	/* IF number correct? */
				 (pCtrl->SetupPacket.wIndex.WB.L == USB_ADC_SIF1_NUM) ||
				 (pCtrl->SetupPacket.wIndex.WB.L == USB_ADC_SIF2_NUM)) ) {
				switch (pCtrl->SetupPacket.bRequest) {
				case AUDIO_REQUEST_SET_CUR:
					//                      case AUDIO_REQUEST_SET_MIN:
					//                      case AUDIO_REQUEST_SET_MAX:
					//                      case AUDIO_REQUEST_SET_RES:
					if (ADC_IF_SetRequest()) {
						mwUSB_StatusInStage(pCtrl);					/* send Acknowledge */
						return LPC_OK;
					}
					break;
				}
			}
			else if (pCtrl->SetupPacket.bmRequestType.BM.Recipient == REQUEST_TO_ENDPOINT) {
				switch (pCtrl->SetupPacket.bRequest) {
				case AUDIO_REQUEST_SET_CUR:
					//                    case AUDIO_REQUEST_SET_MIN:
					//                    case AUDIO_REQUEST_SET_MAX:
					//                    case AUDIO_REQUEST_SET_RES:
					if (ADC_EP_SetRequest() == LPC_OK) {
						mwUSB_StatusInStage(pCtrl);						/* send Acknowledge */
						return LPC_OK;
					}
					break;
				}
			}
			break;

		default:
			break;
		}
	}
	return ret;
}

#endif
