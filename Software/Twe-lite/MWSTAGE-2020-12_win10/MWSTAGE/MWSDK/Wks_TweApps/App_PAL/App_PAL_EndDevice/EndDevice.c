/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */

/****************************************************************************/
/***        Include files                                                 ***/
/****************************************************************************/
#include <string.h>

#include <jendefs.h>
#include <AppHardwareApi.h>

#include "AppQueueApi_ToCoNet.h"

#include "config.h"
#include "ccitt8.h"
#include "Interrupt.h"

#ifdef USE_CUE
#include "App_CUE.h"
#else
#include "EndDevice.h"
#endif

#include "utils.h"

#include "config.h"

#include "adc.h"
#include "SMBus.h"

// Serial options
#include "serial.h"
#include "fprintf.h"
#include "sprintf.h"

#include "input_string.h"
#include "Interactive.h"
#include "flash.h"

/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/
/**
 * アプリケーションごとの振る舞いを記述するための関数テーブル
 */
tsCbHandler *psCbHandler = NULL;
tsCbHandler *psCbHandler_Sub = NULL;

/****************************************************************************/
/***        Type Definitions                                              ***/
/****************************************************************************/

/****************************************************************************/
/***        Local Function Prototypes                                     ***/
/****************************************************************************/
static void vInitHardware(int f_warm_start);
//static void vInitPulseCounter();
static void vInitADC();

static void vSerialInit();
void vSerInitMessage();
void vProcessSerialCmd(tsSerCmd_Context *pCmd);

/****************************************************************************/
/***        Exported Variables                                            ***/
/****************************************************************************/

/****************************************************************************/
/***        Local Variables                                               ***/
/****************************************************************************/
// Local data used by the tag during operation
tsAppData sAppData;

tsFILE sSerStream;
tsSerialPortSetup sSerPort;

tsPALData sPALData;

uint8 u8PowerUp;

void *pvProcessEv;
void *pvProcessEv_Sub;
#ifdef USE_CUE
tsCbHandler *psCbHandler_OTA = NULL;
void *pvProcessEv_OTA;
#endif
void (*pf_cbProcessSerialCmd)(tsSerCmd_Context *);

/****************************************************************************/
/***        Functions                                                     ***/
/****************************************************************************/

/**
 * 始動時の処理
 */
void cbAppColdStart(bool_t bAfterAhiInit) {
	if (!bAfterAhiInit) {
		// before AHI initialization (very first of code)
		pvProcessEv = NULL;
		pf_cbProcessSerialCmd = NULL;

		// check Deep boot
		u8PowerUp = 0;
		if (u16AHI_PowerStatus() & 0x01) {
			u8PowerUp = 0x01;
		}

		// check DIO source
		sAppData.bWakeupByButton = FALSE;
		if(u8AHI_WakeTimerFiredStatus()) {
		} else
    	if(u32AHI_DioWakeStatus() & u32DioPortWakeUp) {
			// woke up from DIO events
    		sAppData.bWakeupByButton = TRUE;
		}

		// Module Registration
		ToCoNet_REG_MOD_ALL();
	} else {
		// disable brown out detect
		vAHI_BrownOutConfigure(0,//0:2.0V 1:2.3V
				FALSE,
				FALSE,
				FALSE,
				FALSE);

		SPRINTF_vInit128();
		sAppData.u32DIO_startup = ~u32PortReadBitmap(); // この時点では全部入力ポート

		// TWELITE NETの初期化
		sToCoNet_AppContext.bRxOnIdle = FALSE;		// 受信回路は開かない
		sToCoNet_AppContext.u8CCA_Level = 1;
		sToCoNet_AppContext.u8CCA_Retry = 0;
		sToCoNet_AppContext.u16TickHz = 1000;

		// アプリケーション保持構造体の初期化
		memset(&sAppData, 0x00, sizeof(sAppData));
		memset(&sPALData, 0x00, sizeof(sPALData));

		sAppData.bColdStart = TRUE;

		// 内部EEPROMからの読み出し
		sAppData.bFlashLoaded = Config_bLoad(&sAppData.sFlash);

		sAppData.u8Retry = ((sAppData.sFlash.sData.u8pow>>4)&0x0F) + 0x80;		// 強制再送
		sToCoNet_AppContext.u8TxPower = sAppData.sFlash.sData.u8pow&0x0F;
		sToCoNet_AppContext.u32AppId = sAppData.sFlash.sData.u32appid;
		sToCoNet_AppContext.u8Channel = sAppData.sFlash.sData.u8ch;

		// version info
		sAppData.u32ToCoNetVersion = ToCoNet_u32GetVersion();

		// Other Hardware
		vInitHardware(FALSE);

		// IDが初期値ならDIPスイッチの値を使用する
#ifdef USE_CUE
		sAppData.u8LID = (sAppData.sFlash.sData.u8id==0) ? 1:sAppData.sFlash.sData.u8id;
#else
		sAppData.u8LID = (sAppData.sFlash.sData.u8id==0) ? ((sAppData.u8DIPSW&0x07)+1+(sAppData.u8DIPSW&0x08?0x80:0)):sAppData.sFlash.sData.u8id+(sAppData.u8DIPSW&0x08?0x80:0);
#endif

		sAppData.u8LID = sAppData.u8LID | (IS_APPCONF_OPT_TO_NOTICE()?0x80:0x00);

		// Sleep時間の計算(割り算は遅いので、要検討)
		sAppData.u32Sleep_min = sAppData.sFlash.sData.u32Slp/60;
		sAppData.u8Sleep_sec = sAppData.sFlash.sData.u32Slp-(sAppData.u32Sleep_min*60);

		if ( sAppData.bConfigMode ) {
			// 設定モードで起動
#ifdef USE_CUE
			sToCoNet_AppContext.u32AppId = APP_ID_OTA;
			sToCoNet_AppContext.u8Channel = CHANNEL_OTA;
			sToCoNet_AppContext.bRxOnIdle = TRUE;
#ifdef OTA
			sToCoNet_AppContext.u16ShortAddress = SHORTADDR_OTA;
			vInitAppConfigMaster();
#else
			vInitAppCUEConfig();
#endif

#else
			vInitAppConfig();
#endif

			// インタラクティブモードの初期化
			Interactive_vInit();
		} else
#ifndef USE_CUE
		// 何も刺さっていない
		if ( sAppData.u8SnsID == PKT_ID_NOCONNECT) {
			// 通常アプリで起動
			sToCoNet_AppContext.u8MacInitPending = TRUE; // 起動時の MAC 初期化を省略する(送信する時に初期化する)
			sToCoNet_AppContext.bSkipBootCalib = FALSE; // 起動時のキャリブレーションを行う

			// ADC の初期化
			vInitADC();

			// イベント処理の初期化
			vInitAppNOC();
		} else
		// PAL MAG
		if ( sAppData.u8SnsID == PKT_ID_MAG) {
			sToCoNet_AppContext.u8MacInitPending = TRUE; // 起動時の MAC 初期化を省略する(送信する時に初期化する)
			sToCoNet_AppContext.bSkipBootCalib = FALSE; // 起動時のキャリブレーションを行う

			// ADC の初期化
			vInitADC();
			vInitAppMAG();
		} else
		// PAL ENV
		if ( sAppData.u8SnsID == PKT_ID_AMB) {
			sToCoNet_AppContext.u8MacInitPending = TRUE; // 起動時の MAC 初期化を省略する(送信する時に初期化する)
			sToCoNet_AppContext.bSkipBootCalib = FALSE; // 起動時のキャリブレーションを行う

			// ADC の初期化
			vInitADC();
			vInitAppENV();
		} else
		// PAL MOT
		if ( sAppData.u8SnsID == PKT_ID_MOT) {
			sToCoNet_AppContext.u8MacInitPending = TRUE; // 起動時の MAC 初期化を省略する(送信する時に初期化する)
			sToCoNet_AppContext.bSkipBootCalib = FALSE; // 起動時のキャリブレーションを行う

			// ADC の初期化
			vInitADC();

			if( IS_APPCONF_OPT_EVENTMODE() ||
				IS_APPCONF_OPT_DICEMODE() ){
				vInitAppMOT_Event();
			}else{
				vInitAppMOT();
			}
		} else	
		if ( sAppData.u8SnsID == PKT_ID_LED) {
			sToCoNet_AppContext.u8MacInitPending = TRUE; // 起動時の MAC 初期化を省略する(送信する時に初期化する)
			sToCoNet_AppContext.bSkipBootCalib = FALSE; // 起動時のキャリブレーションを行う

			// ADC の初期化
			vInitADC();
			vInitAppLED();
		}else
#else
#ifndef OTA
		if ( sAppData.u8SnsID == PKT_ID_CUE) {
			sToCoNet_AppContext.u8MacInitPending = TRUE; // 起動時の MAC 初期化を省略する(送信する時に初期化する)
			sToCoNet_AppContext.bSkipBootCalib = FALSE; // 起動時のキャリブレーションを行う

			// ADC の初期化
			vInitADC();

			if( IS_APPCONF_OPT_EVENTMODE() ||
				IS_APPCONF_OPT_DICEMODE() ){
				vInitAppMOT_Event();
			}else if( IS_APPCONF_OPT_FIFOMODE() ){
				vInitAppMOT();
			}else if( IS_APPCONF_OPT_MAGMODE() ){
				vInitAppMAG();
			}else{
				vInitAppCUE();
			}
		} else
#endif
#endif
		{
			;
		} // 終端の else 節

		// イベント処理関数の登録
#ifdef USE_CUE
#ifndef OTA
		if( !IS_APPCONF_OPT_DISABLE_OTA() ){
			A_PRINTF(LB"! OTA INIT");
			vInitAppOTA();
			vInitOTAParam( 5, 100, 750 );
			if(pvProcessEv_OTA){
				ToCoNet_Event_Register_State_Machine(pvProcessEv_OTA);
			}
		}
#endif
#endif
		if (pvProcessEv) {
			ToCoNet_Event_Register_State_Machine(pvProcessEv);
		}
		if (pvProcessEv_Sub) {
			ToCoNet_Event_Register_State_Machine(pvProcessEv_Sub);
		}

		// ToCoNet DEBUG
		ToCoNet_vDebugInit(&sSerStream);
		ToCoNet_vDebugLevel(TOCONET_DEBUG_LEVEL);
	}
}

/**
 * スリープ復帰時の処理
 * @param bAfterAhiInit
 */
void cbAppWarmStart(bool_t bAfterAhiInit) {
	if (!bAfterAhiInit) {
		// before AHI init, very first of code.
		//  to check interrupt source, etc.

		sAppData.bWakeupByButton = FALSE;
		sAppData.u32WakeupDIOStatus = u32AHI_DioWakeStatus();
		sAppData.u8WakeupByTimer = u8AHI_WakeTimerFiredStatus();
		sAppData.u32DIO_startup = (~u32AHI_DioReadInput())&0x1FFFFF;
//		if( u8AHI_WakeTimerFiredStatus() ){
//		} else
		if( sAppData.u32WakeupDIOStatus & u32DioPortWakeUp ) {
			// woke up from DIO events
			sAppData.bWakeupByButton = TRUE;
		}
	} else {
		// disable brown out detect
		vAHI_BrownOutConfigure(0,//0:2.0V 1:2.3V
				FALSE,
				FALSE,
				FALSE,
				FALSE);

		// 他のハードの待ち
		Interactive_vReInit();
		vInitHardware(TRUE);

		sAppData.bColdStart = FALSE;

		// TOCONET DEBUG
		ToCoNet_vDebugInit(&sSerStream);
		ToCoNet_vDebugLevel(TOCONET_DEBUG_LEVEL);

		// ADC の初期化
		vInitADC();

		if (!sAppData.bWakeupByButton) {
			// タイマーで起きた
		} else {
			// ボタンで起床した
		}

	}
}

/**
 * メイン処理
 */
void cbToCoNet_vMain(void) {
	if (psCbHandler && psCbHandler->pf_cbToCoNet_vMain) {
		(*psCbHandler->pf_cbToCoNet_vMain)();
	}
	if (psCbHandler_Sub && psCbHandler_Sub->pf_cbToCoNet_vMain) {
		(*psCbHandler_Sub->pf_cbToCoNet_vMain)();
	}
#ifdef USE_CUE
	if (psCbHandler_OTA && psCbHandler_OTA->pf_cbToCoNet_vMain) {
		(*psCbHandler_OTA->pf_cbToCoNet_vMain)();
	}
#endif
}

/**
 * 受信処理
 */
void cbToCoNet_vRxEvent(tsRxDataApp *pRx) {
	V_PRINTF(LB ">>> Rx tick=%d <<<", u32TickCount_ms & 0xFFFF);
	if (psCbHandler && psCbHandler->pf_cbToCoNet_vRxEvent) {
		(*psCbHandler->pf_cbToCoNet_vRxEvent)(pRx);
	}
	if (psCbHandler_Sub && psCbHandler_Sub->pf_cbToCoNet_vRxEvent) {
		(*psCbHandler_Sub->pf_cbToCoNet_vRxEvent)(pRx);
	}
#ifdef USE_CUE
	if (psCbHandler_OTA && psCbHandler_OTA->pf_cbToCoNet_vRxEvent) {
		(*psCbHandler_OTA->pf_cbToCoNet_vRxEvent)(pRx);
	}
#endif
}

/**
 * 送信完了イベント
 */
void cbToCoNet_vTxEvent(uint8 u8CbId, uint8 bStatus) {
	V_PRINTF(LB ">>> TxCmp %s(tick=%d,req=#%d) <<<",
			bStatus ? "Ok" : "Ng",
			u32TickCount_ms & 0xFFFF,
			u8CbId
			);

	if(sAppData.u8LedState == 0x00){
		LED_OFF();
	}

	if (psCbHandler && psCbHandler->pf_cbToCoNet_vTxEvent) {
		(*psCbHandler->pf_cbToCoNet_vTxEvent)(u8CbId, bStatus);
	}
	if (psCbHandler_Sub && psCbHandler_Sub->pf_cbToCoNet_vTxEvent) {
		(*psCbHandler_Sub->pf_cbToCoNet_vTxEvent)(u8CbId, bStatus);
	}
#ifdef USE_CUE
	if (psCbHandler_OTA && psCbHandler_OTA->pf_cbToCoNet_vTxEvent) {
		(*psCbHandler_OTA->pf_cbToCoNet_vTxEvent)(u8CbId, bStatus);
	}
#endif

	return;
}

/**
 * ネットワークイベント
 * @param eEvent
 * @param u32arg
 */
void cbToCoNet_vNwkEvent(teEvent eEvent, uint32 u32arg) {
	if (psCbHandler && psCbHandler->pf_cbToCoNet_vNwkEvent) {
		(*psCbHandler->pf_cbToCoNet_vNwkEvent)(eEvent, u32arg);
	}
	if (psCbHandler_Sub && psCbHandler_Sub->pf_cbToCoNet_vNwkEvent) {
		(*psCbHandler_Sub->pf_cbToCoNet_vNwkEvent)(eEvent, u32arg);
	}
#ifdef USE_CUE
	if (psCbHandler_OTA && psCbHandler_OTA->pf_cbToCoNet_vNwkEvent) {
		(*psCbHandler_OTA->pf_cbToCoNet_vNwkEvent)(eEvent, u32arg);
	}
#endif
}

/**
 * ハードウェアイベント処理（割り込み遅延実行）
 */
void cbToCoNet_vHwEvent(uint32 u32DeviceId, uint32 u32ItemBitmap) {
	switch (u32DeviceId) {
	case E_AHI_DEVICE_TICK_TIMER:
		if(sAppData.u8LedState == 2){
			vPortSet_TrueAsLo( OUTPUT_LED, u32TickCount_ms&0x0080 );
		}
		break;
	}

	if (psCbHandler && psCbHandler->pf_cbToCoNet_vHwEvent) {
		(*psCbHandler->pf_cbToCoNet_vHwEvent)(u32DeviceId, u32ItemBitmap);
	}
	if (psCbHandler_Sub && psCbHandler_Sub->pf_cbToCoNet_vHwEvent) {
		(*psCbHandler_Sub->pf_cbToCoNet_vHwEvent)(u32DeviceId, u32ItemBitmap);
	}
#ifdef USE_CUE
	if (psCbHandler_OTA && psCbHandler_OTA->pf_cbToCoNet_vHwEvent) {
		(*psCbHandler_OTA->pf_cbToCoNet_vHwEvent)(u32DeviceId, u32ItemBitmap);
	}
#endif
}

/**
 * ハードウェア割り込みハンドラ
 */
uint8 cbToCoNet_u8HwInt(uint32 u32DeviceId, uint32 u32ItemBitmap) {
	bool_t bRet = FALSE;
	if (psCbHandler && psCbHandler->pf_cbToCoNet_u8HwInt) {
		bRet = (*psCbHandler->pf_cbToCoNet_u8HwInt)(u32DeviceId, u32ItemBitmap);
	}
	if (psCbHandler_Sub && psCbHandler_Sub->pf_cbToCoNet_u8HwInt) {
		bRet = (*psCbHandler_Sub->pf_cbToCoNet_u8HwInt)(u32DeviceId, u32ItemBitmap);
	}
#ifdef USE_CUE
	if (psCbHandler_OTA && psCbHandler_OTA->pf_cbToCoNet_u8HwInt) {
		bRet = (*psCbHandler_OTA->pf_cbToCoNet_u8HwInt)(u32DeviceId, u32ItemBitmap);
	}
#endif
	return bRet;
}

/**
 * ADCの初期化
 */
static void vInitADC() {
	// ADC
	vADC_Init(&sAppData.sObjADC, &sAppData.sADC, TRUE);
	// 初期化待ち
	//vADC_WaitInit();

	sAppData.u8AdcState = 0xFF; // 初期化中
	sAppData.sObjADC.u8SourceMask = TEH_ADC_SRC_VOLT | TEH_ADC_SRC_ADC_1;
}

/**
 * パルスカウンタの初期化
 * - cold boot 時に1回だけ初期化する
 */
/*static void vInitPulseCounter() {
	// カウンタのスタート
	bAHI_StartPulseCounter(E_AHI_PC_0); // start it

	// カウンタの設定
	bAHI_PulseCounterConfigure(
		E_AHI_PC_1,
		1,      // 0:RISE, 1:FALL EDGE
		0,      // Debounce 0:off, 1:2samples, 2:4samples, 3:8samples
		FALSE,   // Combined Counter (32bitカウンタ)
		FALSE);  // Interrupt (割り込み)

	// カウンタのセット
	bAHI_SetPulseCounterRef(
		E_AHI_PC_1,
		0x0); // 何か事前に値を入れておく

	// カウンタのスタート
	bAHI_StartPulseCounter(E_AHI_PC_1); // start it
}*/

/**
 * ハードウェアの初期化を行う
 * @param f_warm_start TRUE:スリープ起床時
 */
static void vInitHardware(int f_warm_start)
{
	// Serial Port の初期化
	{
		tsUartOpt sUartOpt;
		memset(&sUartOpt, 0, sizeof(tsUartOpt));
		uint32 u32baud = UART_BAUD;

		// BPS ピンが Lo の時は 38400bps
		if ( IS_APPCONF_OPT_UART_FORCE_SETTINGS() ) {
			u32baud = sAppData.sFlash.sData.u32baud_safe;
			sUartOpt.bHwFlowEnabled = FALSE;
			sUartOpt.bParityEnabled = UART_PARITY_ENABLE;
			sUartOpt.u8ParityType = UART_PARITY_TYPE;
			sUartOpt.u8StopBit = UART_STOPBITS;

			// 設定されている場合は、設定値を採用する (v1.0.3)
			switch(sAppData.sFlash.sData.u8parity & APPCONF_UART_CONF_PARITY_MASK) {
			case 0:
				sUartOpt.bParityEnabled = FALSE;
				break;
			case 1:
				sUartOpt.bParityEnabled = TRUE;
				sUartOpt.u8ParityType = E_AHI_UART_ODD_PARITY;
				break;
			case 2:
				sUartOpt.bParityEnabled = TRUE;
				sUartOpt.u8ParityType = E_AHI_UART_EVEN_PARITY;
				break;
			}

			// ストップビット
			if (sAppData.sFlash.sData.u8parity & APPCONF_UART_CONF_STOPBIT_MASK) {
				sUartOpt.u8StopBit = E_AHI_UART_2_STOP_BITS;
			} else {
				sUartOpt.u8StopBit = E_AHI_UART_1_STOP_BIT;
			}

			// 7bitモード
			if (sAppData.sFlash.sData.u8parity & APPCONF_UART_CONF_WORDLEN_MASK) {
				sUartOpt.u8WordLen = 7;
			} else {
				sUartOpt.u8WordLen = 8;
			}

			vSerialInit(u32baud, &sUartOpt);
		} else {
			vSerialInit(u32baud, NULL);
		}

	}

	// SMBUS の初期化
	vSMBusInit();

#ifdef USE_CUE
	sAppData.u8SnsID = PKT_ID_CUE;
	sPALData.u8PALModel = PKT_ID_CUE;
	sPALData.u8PALVersion = 1;
#else
	if(!f_warm_start || sAppData.u8SnsID == 0){
		if(bGetPALOptions()){
			sAppData.u8SnsID = sPALData.u8PALModel;

			// Warm Start でかつ PALのEEPROMが認識された場合、(つまり、PALが刺さっていない状態からPALが刺さった場合)再起動する。
			if( f_warm_start && sAppData.u8SnsID != 0 ){
				vAHI_SwReset();		// Rebootする
			}
		}else{
			sAppData.u8SnsID = PKT_ID_NOCONNECT;
		}
	}
#endif

#ifdef OTA
	vPortSetLo(PORT_OUT4);		//	WD
	vPortAsOutput(PORT_OUT4);		//	WD
	vPortDisablePullup(PORT_OUT4);	//	WD
	vPortAsOutput(PORT_INPUT3);		//	WD_ENB

	// MONOSTICK を想定する
	vPortAsOutput(PORT_OUT1);
	vAHI_DoSetDataOut(2, 0);
	bAHI_DoEnableOutputs(TRUE); // MISO を出力用に

	// 設定モードとして起動
	sAppData.bConfigMode = TRUE;
#else
	// WDTのための出力変化
	vPortSetHi(WDT_OUT);
	if(!f_warm_start){
		// WakeTimer1をTWELITEのグローバル時間カウント用にする
		vAHI_WakeTimerEnable(E_AHI_WAKE_TIMER_1, FALSE);	// 割り込み無効
		vAHI_WakeTimerStartLarge( E_AHI_WAKE_TIMER_1, 0ULL );

		// WDTを制御するポートの初期化
		vPortDisablePullup(WDT_OUT);
		vPortAsOutput(WDT_OUT);

		// LEDの初期化
		LED_OFF();
		vPortDisablePullup(OUTPUT_LED);
		vPortAsOutput(OUTPUT_LED);

		vPortAsInput(INPUT_SWSET);
#ifdef USE_CUE
		if( bPortRead(INPUT_SWSET) || (u8PowerUp == 0x00 && !IS_APPCONF_OPT_DISABLE_OTA()) ){
#else
		if( bPortRead(INPUT_SWSET) ){
#endif
			// 設定モードとして起動
			sAppData.bConfigMode = TRUE;
		}

		if( sAppData.u8SnsID != PKT_ID_NOCONNECT || sAppData.u8SnsID != PKT_ID_CUE ){
			// DIPSWの読み込み
			vPortAsInput(INPUT_DIP1);
			if(bPortRead(INPUT_DIP1)){
				sAppData.u8DIPSW |= 1;
				vPortDisablePullup(INPUT_DIP1);
			}
			vPortAsInput(INPUT_DIP2);
			if(bPortRead(INPUT_DIP2)){
				sAppData.u8DIPSW |= 2;
				vPortDisablePullup(INPUT_DIP2);
			}
			vPortAsInput(INPUT_DIP3);
			if(bPortRead(INPUT_DIP3)){
				sAppData.u8DIPSW |= 4;
				vPortDisablePullup(INPUT_DIP3);
			}
			if( sAppData.u8SnsID != PKT_ID_AMB ){
				vPortAsInput(INPUT_DIP4);
				if(bPortRead(INPUT_DIP4)){
					sAppData.u8DIPSW |= 8;
					vPortDisablePullup(INPUT_DIP4);
				}
			}
		}

		u32DioPortWakeUp = 1UL<<INPUT_SWSET;

		switch(sAppData.u8SnsID){
			case PKT_ID_NOCONNECT:
				vPortDisablePullup(0);
				vPortDisablePullup(1);

				vAHI_DioSetPullup(1UL << (13), 0x00);
				vPortAsInput(13);
				vPortAsInput(11);
				vPortAsInput(16);
				break;
			case PKT_ID_MAG:
				vPortDisablePullup(SNS_EN);
				vPortAsInput(SNS_EN);
				vPortDisablePullup(SNS_INT);
				vPortAsInput(SNS_INT);
				u32DioPortWakeUp |= ( 1UL<<SNS_EN | 1UL<<SNS_INT );
				break;
			case PKT_ID_AMB:
				vPortSetLo( EH_BOOT );
				vPortDisablePullup( EH_BOOT );
				vPortAsOutput( EH_BOOT );
				vPortDisablePullup(SNS_INT);
				vPortAsInput(SNS_INT);
				u32DioPortWakeUp |= (1UL<<SNS_INT);
				break;
			case PKT_ID_MOT:
				vPortDisablePullup(SNS_INT);
				vPortAsInput(SNS_INT);
				u32DioPortWakeUp |= (1UL<<SNS_INT);
				break;
			case PKT_ID_LED:
				vPortSetHi(SNS_EN);
				vPortDisablePullup(SNS_EN);
				vPortAsOutput(SNS_EN);

				vPortDisablePullup(SNS_INT);
				vPortAsInput(SNS_INT);
				u32DioPortWakeUp |= (1UL<<SNS_INT);
				break;
			case PKT_ID_CUE:
				vPortDisablePullup(SNS_INT);
				vPortAsInput(SNS_INT);
				u32DioPortWakeUp |= (1UL<<SNS_INT);

				vPortDisablePullup(SNS_EN);
				vPortAsInput(SNS_EN);
				u32DioPortWakeUp |= (1UL<<SNS_EN);

				vPortDisablePullup(INPUT_PC);
				vPortAsInput(INPUT_PC);
				u32DioPortWakeUp |= (1UL<<INPUT_PC);

			default:
				break;
		}
	}
#endif
}

/** @ingroup MASTER
 * UART を初期化する
 * @param u32Baud ボーレート
 */
void vSerialInit(uint32 u32Baud, tsUartOpt *pUartOpt) {
	/* Create the debug port transmit and receive queues */
	static uint8 au8SerialTxBuffer[1024];
	static uint8 au8SerialRxBuffer[128];

	/* Initialise the serial port to be used for debug output */
	sSerPort.pu8SerialRxQueueBuffer = au8SerialRxBuffer;
	sSerPort.pu8SerialTxQueueBuffer = au8SerialTxBuffer;
	sSerPort.u32BaudRate = u32Baud;
	sSerPort.u16AHI_UART_RTS_LOW = 0xffff;
	sSerPort.u16AHI_UART_RTS_HIGH = 0xffff;
	sSerPort.u16SerialRxQueueSize = sizeof(au8SerialRxBuffer);
	sSerPort.u16SerialTxQueueSize = sizeof(au8SerialTxBuffer);
	sSerPort.u8SerialPort = UART_PORT;
	sSerPort.u8RX_FIFO_LEVEL = E_AHI_UART_FIFO_LEVEL_1;
	SERIAL_vInitEx(&sSerPort, pUartOpt);

	/* prepare stream for vfPrintf */
	sSerStream.bPutChar = SERIAL_bTxChar;
	sSerStream.u8Device = UART_PORT;
}

/**
 * 初期化メッセージ
 */
void vSerInitMessage() {
	A_PRINTF(LB LB"!INF MONO WIRELESS %s V%d-%02d-%d", APP_NAME, VERSION_MAIN, VERSION_SUB, VERSION_VAR);
	A_FLUSH();
	A_PRINTF(LB"!INF AID:%08x,SID:%08x,LID:%02x,PID:%02x",
			sToCoNet_AppContext.u32AppId, ToCoNet_u32GetSerial(), sAppData.u8LID, sPALData.u8PALModel);
	A_FLUSH();
	A_PRINTF(LB"!INF DIO --> %020b", sAppData.u32DIO_startup);
	if (sAppData.bFlashLoaded == 0) {
		A_PRINTF(LB"!INF Default config (no save info)...");
	}
	A_FLUSH();
}

/**
 * コマンド受け取り時の処理
 * @param pCmd
 */
void vProcessSerialCmd(tsSerCmd_Context *pCmd) {
	// アプリのコールバックを呼び出す
	if (pf_cbProcessSerialCmd) {
		(*pf_cbProcessSerialCmd)(pCmd);
	}
	return;
}
/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/
