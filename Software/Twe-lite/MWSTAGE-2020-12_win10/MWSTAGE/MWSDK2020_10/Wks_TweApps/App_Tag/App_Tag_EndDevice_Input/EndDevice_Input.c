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

#include "EndDevice_Input.h"

#include "utils.h"

#include "config.h"
#include "common.h"

#include "adc.h"
#include "SMBus.h"

// Serial options
#include "serial.h"
#include "fprintf.h"
#include "sprintf.h"

#include "input_string.h"
#include "Interactive.h"
#include "flash.h"

#include "ADXL345.h"

/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/

/**
 * DI1 の割り込み（立ち上がり）で起床後
 *   - PWM1 に DUTY50% で 100ms のブザー制御
 *
 * 以降１秒置きに起床して、DI1 が Hi (スイッチ開) かどうかチェックし、
 * Lo になったら、割り込みスリープに遷移、Hi が維持されていた場合は、
 * 一定期間 .u16Slp 経過後にブザー制御を 100ms 実施する。
 */
tsTimerContext sTimerPWM[1]; //!< タイマー管理構造体  @ingroup MASTER

/**
 * アプリケーションごとの振る舞いを記述するための関数テーブル
 */
tsCbHandler *psCbHandler = NULL;

/****************************************************************************/
/***        Type Definitions                                              ***/
/****************************************************************************/

/****************************************************************************/
/***        Local Function Prototypes                                     ***/
/****************************************************************************/
static void vInitHardware(int f_warm_start);
#if !( defined(OTA) || defined(SWING) )
static void vInitPulseCounter();
#endif
static void vInitADC();

static void vSerialInit();
void vSerInitMessage();
void vProcessSerialCmd(tsSerCmd_Context *pCmd);

/****************************************************************************/
/***        Exported Variables                                            ***/
/****************************************************************************/
uint32 u32InputMask;
uint32 u32InputSubMask;

/****************************************************************************/
/***        Local Variables                                               ***/
/****************************************************************************/
// Local data used by the tag during operation
tsAppData_Ed sAppData;

tsFILE sSerStream;
tsSerialPortSetup sSerPort;

uint8 u8Interrupt;		// ADXL345
uint8 u8PowerUp; // 0x01:from Deep

uint8 u8ConfPort  = PORT_CONF2;

uint8 u8ADCPort[4];

uint8 DIO_SNS_POWER = 0;

void *pvProcessEv1, *pvProcessEv2;
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
		pvProcessEv1 = NULL;
		pvProcessEv2 = NULL;
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

		sToCoNet_AppContext.bRxOnIdle = FALSE;		// 受信回路は開かない

		sToCoNet_AppContext.u8CCA_Level = 1;
		sToCoNet_AppContext.u8CCA_Retry = 0;

		sToCoNet_AppContext.u16TickHz = 1000;

		// アプリケーション保持構造体の初期化
		memset(&sAppData, 0x00, sizeof(sAppData));

		// SPRINTFの初期化(128バイトのバッファを確保する)
		SPRINTF_vInit128();

#if !defined(OTA) || !defined(SWING)
		// 設定をロードするセクタの選択
		vPortAsInput(SETTING_BIT1);
		vPortAsInput(SETTING_BIT2);
		vPortAsInput(SETTING_BIT3);
		vPortAsInput(SETTING_BIT4);

		if(bPortRead(SETTING_BIT1)){
			sAppData.u8SettingsID += 0x01;
			vPortDisablePullup(SETTING_BIT1);
		}

		if(bPortRead(SETTING_BIT2)){
			sAppData.u8SettingsID += 0x02;
			vPortDisablePullup(SETTING_BIT2);
		}

		if(bPortRead(SETTING_BIT3)){
			sAppData.u8SettingsID += 0x04;
			vPortDisablePullup(SETTING_BIT3);
		}

		if(bPortRead(SETTING_BIT4)){
			sAppData.u8SettingsID += 0x08;
			vPortDisablePullup(SETTING_BIT4);
		}

#endif

		// フラッシュメモリからの読み出し
		//   フラッシュからの読み込みが失敗した場合、ID=15 で設定する
		sAppData.bFlashLoaded = Config_bLoad(&sAppData.sFlash);

		sAppData.u8Retry = ((sAppData.sFlash.sData.u8pow>>4)&0x0F) + 0x80;		// 強制再送
		sToCoNet_AppContext.u8TxPower = sAppData.sFlash.sData.u8pow&0x0F;

		// version info
		sAppData.u32ToCoNetVersion = ToCoNet_u32GetVersion();

		// Other Hardware
		vInitHardware(FALSE);

		if (sAppData.bConfigMode) {
			// 設定モードで起動

			// 設定用のコンフィグ
			sToCoNet_AppContext.u32AppId = APP_ID_OTA;
			sToCoNet_AppContext.u8Channel = CHANNEL_OTA;
			sToCoNet_AppContext.bRxOnIdle = TRUE;

			// イベント処理の初期化
#ifdef OTA
			sToCoNet_AppContext.u16ShortAddress = SHORTADDR_OTA;
			vInitAppConfigMaster();
#else
			sToCoNet_AppContext.u16ShortAddress = 0x78; // 子機のアドレス（なんでも良い）
			vInitAppConfig();
#endif
			// インタラクティブモードの初期化
			Interactive_vInit();
		} else
#if !defined(OTA)
		// SHT21
		if ( sAppData.sFlash.sData.u8mode == PKT_ID_SHT21 ) {
			sToCoNet_AppContext.bSkipBootCalib = FALSE; // 起動時のキャリブレーションを行う
			sToCoNet_AppContext.u8MacInitPending = TRUE; // 起動時の MAC 初期化を省略する(送信する時に初期化する)

			// ADC の初期化
			vInitADC();

			// イベント処理の初期化
			vInitAppSHT21();
		} else
		//	ボタン起動モード
		if ( sAppData.sFlash.sData.u8mode == PKT_ID_BUTTON ) {
			sToCoNet_AppContext.u8MacInitPending = TRUE; // 起動時の MAC 初期化を省略する(送信する時に初期化する)
			sToCoNet_AppContext.bSkipBootCalib = FALSE; // 起動時のキャリブレーションを省略する(保存した値を確認)
			sToCoNet_AppContext.u8CPUClk = 3;

			// ADC の初期化
			vInitADC();

			// イベント処理の初期化
			vInitAppButton();
		} else
		if ( sAppData.sFlash.sData.u8mode == PKT_ID_UART ) {
			sToCoNet_AppContext.bSkipBootCalib = TRUE; // 起動時のキャリブレーションを省略する(保存した値を確認)

			// UART 処理
			vInitAppUart();

			// インタラクティブモード
			Interactive_vInit();
		} else
		//	磁気スイッチなど
		if ( sAppData.sFlash.sData.u8mode == PKT_ID_IO_TIMER ) {
			// ドアタイマーで起動
			// sToCoNet_AppContext.u8CPUClk = 1; // runs at 8MHz (Doze を利用するのであまり効果が無いかもしれない)
			sToCoNet_AppContext.bSkipBootCalib = FALSE; // 起動時のキャリブレーションを行う
			sToCoNet_AppContext.u8MacInitPending = TRUE; // 起動時の MAC 初期化を省略する(送信する時に初期化する)

			// イベント処理の初期化
			vInitAppDoorTimer();
		} else
		// BME280
		if ( sAppData.sFlash.sData.u8mode == PKT_ID_BME280 ) {
			sToCoNet_AppContext.bSkipBootCalib = FALSE; // 起動時のキャリブレーションを行う
			sToCoNet_AppContext.u8MacInitPending = TRUE; // 起動時の MAC 初期化を省略する(送信する時に初期化する)
			// ADC の初期化
			vInitADC();

			// イベント処理の初期化
			vInitAppBME280();
		} else
		// S11059-02
		if ( sAppData.sFlash.sData.u8mode == PKT_ID_S1105902 ) {
			sToCoNet_AppContext.bSkipBootCalib = FALSE; // 起動時のキャリブレーションを行う
			sToCoNet_AppContext.u8MacInitPending = TRUE; // 起動時の MAC 初期化を省略する(送信する時に初期化する)
			// ADC の初期化
			vInitADC();

			// イベント処理の初期化
			vInitAppS1105902();
		} else
		// ADT7410
		if ( sAppData.sFlash.sData.u8mode == PKT_ID_ADT7410 ) {
			sToCoNet_AppContext.bSkipBootCalib = FALSE; // 起動時のキャリブレーションを行う
			sToCoNet_AppContext.u8MacInitPending = TRUE; // 起動時の MAC 初期化を省略する(送信する時に初期化する)

			// ADC の初期化
			vInitADC();

			// イベント処理の初期化
			vInitAppADT7410();
		} else
		// MPL115A2
		if ( sAppData.sFlash.sData.u8mode == PKT_ID_MPL115A2 ) {
			sToCoNet_AppContext.bSkipBootCalib = FALSE; // 起動時のキャリブレーションを行う
			sToCoNet_AppContext.u8MacInitPending = TRUE; // 起動時の MAC 初期化を省略する(送信する時に初期化する)

			// ADC の初期化
			vInitADC();

			// イベント処理の初期化
			vInitAppMPL115A2();
		} else
		// LIS3DH
		if ( sAppData.sFlash.sData.u8mode == PKT_ID_LIS3DH ) {
			sToCoNet_AppContext.bSkipBootCalib = FALSE; // 起動時のキャリブレーションを行う
			sToCoNet_AppContext.u8MacInitPending = TRUE; // 起動時の MAC 初期化を省略する(送信する時に初期化する)

			// ADC の初期化
			vInitADC();

			// イベント処理の初期化
			vInitAppLIS3DH();
		} else
		// L3GD20
		if ( sAppData.sFlash.sData.u8mode == PKT_ID_L3GD20 ) {
			sToCoNet_AppContext.bSkipBootCalib = FALSE; // 起動時のキャリブレーションを行う
			sToCoNet_AppContext.u8MacInitPending = TRUE; // 起動時の MAC 初期化を省略する(送信する時に初期化する)
			// ADC の初期化
			vInitADC();

			// イベント処理の初期化
			vInitAppL3GD20();
		} else
		// ADXL345
		if ( sAppData.sFlash.sData.u8mode == PKT_ID_ADXL345 ) {
			sToCoNet_AppContext.bSkipBootCalib = FALSE; // 起動時のキャリブレーションを行う
			sToCoNet_AppContext.u8MacInitPending = TRUE; // 起動時の MAC 初期化を省略する(送信する時に初期化する)
			sToCoNet_AppContext.u8CPUClk = 3; // runs at 32Mhz

			// ADC の初期化
			vInitADC();

			// イベント処理の初期化
			if( sAppData.sFlash.sData.i16param&LOWENERGY ){
				vInitAppADXL345_LowEnergy();
			}else if ( sAppData.sFlash.sData.i16param&AIRVOLUME ){
				vInitAppADXL345_AirVolume();
			}else if(sAppData.sFlash.sData.i16param&FIFO){
				vInitAppADXL345_FIFO();
			}else{
				vInitAppADXL345();
			}
		} else
		// ADXL345
		if ( sAppData.sFlash.sData.u8mode == PKT_ID_ADXL345_LOWENERGY ) {
			sToCoNet_AppContext.bSkipBootCalib = FALSE; // 起動時のキャリブレーションを行う
			sToCoNet_AppContext.u8MacInitPending = TRUE; // 起動時の MAC 初期化を省略する(送信する時に初期化する)
			sToCoNet_AppContext.u8CPUClk = 3; // runs at 32Mhz

			// ADC の初期化
			vInitADC();

			// イベント処理の初期化
			vInitAppADXL345_LowEnergy();
		} else
		// TSL2561
		if ( sAppData.sFlash.sData.u8mode == PKT_ID_TSL2561 ) {
			sToCoNet_AppContext.bSkipBootCalib = FALSE; // 起動時のキャリブレーションを行う
			sToCoNet_AppContext.u8MacInitPending = TRUE; // 起動時の MAC 初期化を省略する(送信する時に初期化する)

			// ADC の初期化
			vInitADC();

			// イベント処理の初期化
			vInitAppTSL2561();
		} else
		// MAX31855
		if ( sAppData.sFlash.sData.u8mode == PKT_ID_MAX31855 ) {
			sToCoNet_AppContext.bSkipBootCalib = FALSE; // 起動時のキャリブレーションを行う
			sToCoNet_AppContext.u8MacInitPending = TRUE; // 起動時の MAC 初期化を省略する(送信する時に初期化する)

			// ADC の初期化
			vInitADC();

			// イベント処理の初期化
			vInitAppMAX31855();
		} else
		// SHT31
		if ( sAppData.sFlash.sData.u8mode == PKT_ID_SHT31 ) {
			sToCoNet_AppContext.bSkipBootCalib = FALSE; // 起動時のキャリブレーションを行う
			sToCoNet_AppContext.u8MacInitPending = TRUE; // 起動時の MAC 初期化を省略する(送信する時に初期化する)

			// ADC の初期化
			vInitADC();

			// イベント処理の初期化
			vInitAppSHT31();
		} else
		// SHTC3
		if ( sAppData.sFlash.sData.u8mode == PKT_ID_SHTC3 ) {
			sToCoNet_AppContext.bSkipBootCalib = FALSE; // 起動時のキャリブレーションを行う
			sToCoNet_AppContext.u8MacInitPending = TRUE; // 起動時の MAC 初期化を省略する(送信する時に初期化する)

			// ADC の初期化
			vInitADC();

			// イベント処理の初期化
			vInitAppSHTC3();
		} else
		//	LM61等のアナログセンサ用
		if ( sAppData.sFlash.sData.u8mode == PKT_ID_STANDARD	// アナログセンサ
			|| sAppData.sFlash.sData.u8mode == PKT_ID_LM61) {	// LM61
			// 通常アプリで起動
			sToCoNet_AppContext.u8CPUClk = 3; // runs at 32Mhz
			sToCoNet_AppContext.u8MacInitPending = TRUE; // 起動時の MAC 初期化を省略する(送信する時に初期化する)
			sToCoNet_AppContext.bSkipBootCalib = FALSE; // 起動時のキャリブレーションを行う

			// ADC の初期化
			vInitADC();

#ifndef SWING
			if( !(sAppData.sFlash.sData.i16param&0x0001) && sAppData.sFlash.sData.u8mode == PKT_ID_STANDARD ){
				vInitPulseCounter();
			}
#endif
			// イベント処理の初期化
			vInitAppStandard();
		} else
		//	2つセンサを使用する場合
		if ( sAppData.sFlash.sData.u8mode == PKT_ID_MULTISENSOR ) {
			// 通常アプリで起動
			sToCoNet_AppContext.u8CPUClk = 3; // runs at 32Mhz
			sToCoNet_AppContext.u8MacInitPending = TRUE;
			sToCoNet_AppContext.bSkipBootCalib = FALSE;

			// ADC の初期化
			vInitADC();

			// イベント処理の初期化
			vInitAppMultiSensor();
		} else
#endif // OTA
	    {
			;
		} // 終端の else 節

		// イベント処理関数の登録
		if (pvProcessEv1) {
			ToCoNet_Event_Register_State_Machine(pvProcessEv1);
		}
		if (pvProcessEv2) {
			ToCoNet_Event_Register_State_Machine(pvProcessEv2);
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
		uint32 u32WakeStatus = u32AHI_DioWakeStatus();
		if(u8AHI_WakeTimerFiredStatus()) {
		} else
		if( ( u32WakeStatus & PORT_INPUT_MASK_ADXL345) && sAppData.sFlash.sData.u8mode == PKT_ID_ADXL345 ){
			sAppData.bWakeupByButton = TRUE;
		}else
		if( u32WakeStatus & u32DioPortWakeUp) {
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

		vPortSetSns(TRUE);
//		vPortAsOutput(DIO_SNS_POWER);

		// TOCONET DEBUG
		ToCoNet_vDebugInit(&sSerStream);
		ToCoNet_vDebugLevel(TOCONET_DEBUG_LEVEL);

		//	センサ特有の初期化
		//	リードスイッチとUART用でない限りADCを初期化
		if( sAppData.sFlash.sData.u8mode != PKT_ID_UART &&
			sAppData.sFlash.sData.u8mode != PKT_ID_IO_TIMER ){
			// ADC の初期化
			vInitADC();
		}

		//	ADXL345の場合、割り込みの原因を判別する。
		if( sAppData.sFlash.sData.u8mode == PKT_ID_ADXL345 ){
			u8Interrupt = u8Read_Interrupt();
		}

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
}

/**
 * 受信処理
 */
void cbToCoNet_vRxEvent(tsRxDataApp *pRx) {
	if (psCbHandler && psCbHandler->pf_cbToCoNet_vRxEvent) {
		(*psCbHandler->pf_cbToCoNet_vRxEvent)(pRx);
	}
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

#ifndef OTA
	if( !(sAppData.sFlash.sData.u8mode == 0x35 && (sAppData.sFlash.sData.i16param&AIRVOLUME)) ){
		LED_OFF(LED);
	}
#endif

	if (psCbHandler && psCbHandler->pf_cbToCoNet_vTxEvent) {
		(*psCbHandler->pf_cbToCoNet_vTxEvent)(u8CbId, bStatus);
	}

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
}

/**
 * ハードウェアイベント処理（割り込み遅延実行）
 */
void cbToCoNet_vHwEvent(uint32 u32DeviceId, uint32 u32ItemBitmap) {
	if (psCbHandler && psCbHandler->pf_cbToCoNet_vHwEvent) {
		(*psCbHandler->pf_cbToCoNet_vHwEvent)(u32DeviceId, u32ItemBitmap);
	}
}

/**
 * ハードウェア割り込みハンドラ
 */
uint8 cbToCoNet_u8HwInt(uint32 u32DeviceId, uint32 u32ItemBitmap) {
	bool_t bRet = FALSE;
	if (psCbHandler && psCbHandler->pf_cbToCoNet_u8HwInt) {
		bRet = (*psCbHandler->pf_cbToCoNet_u8HwInt)(u32DeviceId, u32ItemBitmap);
	}
	return bRet;
}

/**
 * ADCの初期化
 */
static void vInitADC() {
	// ADC
	vADC_Init(&sAppData.sObjADC, &sAppData.sADC, TRUE);
	sAppData.u8AdcState = 0xFF; // 初期化中

	memset( u8ADCPort, 0xFF, sizeof(u8ADCPort) );

#ifdef SWING
	if( sAppData.sFlash.sData.u8mode == PKT_ID_BUTTON || (0x30 < sAppData.sFlash.sData.u8mode && sAppData.sFlash.sData.u8mode < 0x50) ){
		sAppData.sObjADC.u8SourceMask = TEH_ADC_SRC_VOLT;
		u8ADCPort[0] = TEH_ADC_IDX_ADC_4;
		u8ADCPort[1] = TEH_ADC_IDX_ADC_4;
	}else if(sAppData.sFlash.sData.u8mode == PKT_ID_STANDARD || sAppData.sFlash.sData.u8mode == PKT_ID_LM61){
		vPortDisablePullup(0);
		sAppData.sObjADC.u8SourceMask =
				TEH_ADC_SRC_VOLT | TEH_ADC_SRC_ADC_3;
		u8ADCPort[0] = TEH_ADC_IDX_ADC_3;
		u8ADCPort[1] = TEH_ADC_IDX_ADC_3;
	}else{
		vPortDisablePullup(0);
		vPortDisablePullup(1);
		sAppData.sObjADC.u8SourceMask =
				TEH_ADC_SRC_VOLT | TEH_ADC_SRC_ADC_3 | TEH_ADC_SRC_ADC_4;
		u8ADCPort[0] = TEH_ADC_IDX_ADC_3;
		u8ADCPort[1] = TEH_ADC_IDX_ADC_4;
	}
#else
	if( sAppData.sFlash.sData.u8mode == PKT_ID_STANDARD && (sAppData.sFlash.sData.i16param&0x0001) ){
		vPortDisablePullup(0);
		vPortDisablePullup(1);
		sAppData.sObjADC.u8SourceMask =
				TEH_ADC_SRC_VOLT | TEH_ADC_SRC_ADC_1 | TEH_ADC_SRC_ADC_2 | TEH_ADC_SRC_ADC_3 | TEH_ADC_SRC_ADC_4;
		u8ADCPort[0] = TEH_ADC_IDX_ADC_1;
		u8ADCPort[1] = TEH_ADC_IDX_ADC_2;
		u8ADCPort[2] = TEH_ADC_IDX_ADC_3;
		u8ADCPort[3] = TEH_ADC_IDX_ADC_4;
	}else{
		sAppData.sObjADC.u8SourceMask =
				TEH_ADC_SRC_VOLT | TEH_ADC_SRC_ADC_1 | TEH_ADC_SRC_ADC_2;
		u8ADCPort[0] = TEH_ADC_IDX_ADC_1;
		u8ADCPort[1] = TEH_ADC_IDX_ADC_2;
	}
#endif
}

/**
 * パルスカウンタの初期化
 * - cold boot 時に1回だけ初期化する
 */
#if !(defined(OTA) || defined(SWING))
static void vInitPulseCounter() {
	// カウンタの設定
	bAHI_PulseCounterConfigure(
		E_AHI_PC_0,
		1,      // 0:RISE, 1:FALL EDGE
		0,      // Debounce 0:off, 1:2samples, 2:4samples, 3:8samples
		FALSE,   // Combined Counter (32bitカウンタ)
		FALSE);  // Interrupt (割り込み)


	// カウンタのセット
	bAHI_SetPulseCounterRef(
		E_AHI_PC_0,
		0x0); // 何か事前に値を入れておく

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
}
#endif

/**
 * ハードウェアの初期化を行う
 * @param f_warm_start TRUE:スリープ起床時
 */
static void vInitHardware(int f_warm_start) {

	// Serial Port の初期化
	{
		tsUartOpt sUartOpt;
		memset(&sUartOpt, 0, sizeof(tsUartOpt));
		uint32 u32baud = UART_BAUD;

		// BPS ピンが Lo の時は 38400bps
		vPortAsInput(PORT_BAUD);
		if (sAppData.bFlashLoaded && (bPortRead(PORT_BAUD) || IS_APPCONF_OPT_UART_FORCE_SETTINGS() )) {
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
	if(!f_warm_start){
		// リセットICの無効化(イの一番に処理)
		if(sAppData.sFlash.sData.u8mode < 0x61 || 0x70 < sAppData.sFlash.sData.u8mode ){
			vPortSetLo(DIO_VOLTAGE_CHECKER);
			vPortAsOutput(DIO_VOLTAGE_CHECKER);
			vPortDisablePullup(DIO_VOLTAGE_CHECKER);

			// １次キャパシタ(e.g. 220uF)とスーパーキャパシタ (1F) の直結制御用(イの一番に処理)
			vPortSetHi(DIO_SUPERCAP_CONTROL);
			vPortAsOutput(DIO_SUPERCAP_CONTROL);
			vPortDisablePullup(DIO_SUPERCAP_CONTROL);
		}

		// WDT用出力の初期化
		vPortSetLo(3);
		vPortAsOutput(3);
		vPortDisablePullup(3);

		//	送信ステータスなどのLEDのための出力
		LED_OFF(LED);
		vPortAsOutput(LED);
		vPortDisablePullup(LED);

	// M2がLoなら、設定モードとして動作する
#ifdef SWING
		u8ConfPort = 16;		// SWINGのSETピンはSCLの副ポート
#endif

		vPortAsInput(u8ConfPort);
		bool_t bSetPort = bPortRead(u8ConfPort);
		if( u8PowerUp == 0x00 && (IS_APPCONF_OPT_PASS_SETTINGS() == FALSE || bSetPort ) ){
			sAppData.bConfigMode = TRUE;
		}
#ifdef SWING
		vPortDisablePullup(u8ConfPort);
		u8ConfPort = 17;		// SDA
		vPortAsInput(u8ConfPort);
		if( bPortRead(u8ConfPort) &&
			sAppData.sFlash.sData.u8mode == PKT_ID_BUTTON &&
			!sAppData.bConfigMode ){
			sAppData.sFlash.sData.u8mode = PKT_ID_LM61;
		}
		vPortDisablePullup(u8ConfPort);
#endif

	// センサー用の制御 (Lo:Active), OPTION による制御を行っているのでフラッシュ読み込み後の制御が必要
#ifdef SWING
		// I2Cセンサを使用するときとそれ以外の時で使用するポートを変える。
		if( 0x30 < sAppData.sFlash.sData.u8mode && sAppData.sFlash.sData.u8mode < 0x50 ){
			DIO_SNS_POWER = 1;
		}else{
			DIO_SNS_POWER = 16;
		}
#else
		DIO_SNS_POWER = PORT_OUT3;
#endif

		// SWINGのインタラクティブモードの場合はセンサ用のDIO制御は有効にしない
		if(u8ConfPort == PORT_CONF2 || sAppData.bConfigMode == FALSE ){
			vPortSetSns(TRUE);
			vPortAsOutput(DIO_SNS_POWER);
			vPortDisablePullup(DIO_SNS_POWER);
		}

#ifdef SWING
		// DIO1がLoもしくはI2CセンサモードだったらApp_Tagモードに変更
		vPortAsInput(1);
		if( (bPortRead(1) &&
			IS_APPCONF_OPT_APP_TWELITE() &&
			!sAppData.bConfigMode) ||
			( 0x30 < sAppData.sFlash.sData.u8mode && sAppData.sFlash.sData.u8mode < 0x50) ){
			sAppData.sFlash.sData.u32Opt -= 0x10;
		}
		vPortDisablePullup(1);
#endif

		// configure network
		if( IS_APPCONF_OPT_APP_TWELITE() && sAppData.sFlash.sData.u32appid == APP_ID && sAppData.sFlash.sData.u8ch == CHANNEL ){
			sToCoNet_AppContext.u32AppId = APP_TWELITE_ID;
			sToCoNet_AppContext.u8Channel = APP_TWELITE_CHANNEL;
		}else{
			if( IS_APPCONF_OPT_APP_TWELITE() && sAppData.sFlash.sData.u32appid == APP_ID ){
				sToCoNet_AppContext.u32AppId = APP_TWELITE_ID;
			}else{
				sToCoNet_AppContext.u32AppId = sAppData.sFlash.sData.u32appid;
			}
			sToCoNet_AppContext.u8Channel = sAppData.sFlash.sData.u8ch;
		}


		// 入力ポートを明示的に指定する
		if( sAppData.sFlash.sData.u8mode == PKT_ID_BUTTON ){
			u32InputMask = PORT_INPUT_MASK;
			u32InputSubMask = 0;
			vPortAsInput(DIO_BUTTON);

			if( IS_DBLEDGE_INT() ){		// 両方のエッジをとる場合
				vPortAsInput( PORT_INPUT2 );
				u32InputSubMask = 1UL<<PORT_INPUT2;

				/* 複数入力を使用する場合 */
				if(IS_MULTI_INPUT()){
					vPortAsInput( PORT_INPUT3 );
					vPortAsInput( PORT_SDA );
					u32InputMask |= 1UL<<PORT_INPUT3;
					u32InputSubMask |= 1UL<<PORT_SDA;
				}

				vAHI_DioSetPullup( 0, u32InputMask|u32InputSubMask ); // 内部プルアップを無効
				vAHI_DioWakeEnable( u32InputMask|u32InputSubMask , 0 ); // also use as DIO WAKE SOURCE
				vAHI_DioWakeEdge( u32InputSubMask, u32InputMask ); // 割り込みエッジ
			}else{
				/* 複数入力を使用する場合 */
				if(IS_MULTI_INPUT()){
					vPortAsInput( PORT_INPUT2 );
					vPortAsInput( PORT_INPUT3 );
					vPortAsInput( PORT_SDA );
					u32InputMask |= 1UL<<PORT_INPUT2;
					u32InputMask |= 1UL<<PORT_INPUT3;
					u32InputMask |= 1UL<<PORT_SDA;
				}

				if( IS_INVERSE_INT() ){	// 立上りを見る場合
					vAHI_DioSetPullup( 0, u32InputMask ); // 内部プルアップを無効
					vAHI_DioWakeEnable( u32InputMask, 0 ); // also use as DIO WAKE SOURCE
					vAHI_DioWakeEdge( u32InputMask, 0 ); // 割り込みエッジ
				}else{
					vAHI_DioWakeEnable( u32InputMask, 0 ); // also use as DIO WAKE SOURCE
					vAHI_DioWakeEdge( 0 ,u32InputMask ); // 割り込みエッジ
				}
			}

			// 連照してDI1の状態を確定する場合
			if( IS_INPUT_TIMER() ){
				sAppData.sBTM_Config.bmPortMask = u32InputMask;
				sAppData.sBTM_Config.u16Tick_ms = 1;
				//sAppData.sBTM_Config.u8MaxHistory = 20;
				sAppData.sBTM_Config.u8MaxHistory = sAppData.sFlash.sData.u8wait;
				sAppData.sBTM_Config.u8DeviceTimer = 0xFF; // TickTimer を流用する。
				sAppData.pr_BTM_handler = prBTM_InitExternal(&sAppData.sBTM_Config);
				vBTM_Enable();
			}

			u32DioPortWakeUp = u32InputMask|u32InputSubMask;

		}else if( sAppData.sFlash.sData.u8mode == PKT_ID_ADXL345 ){
			if(sAppData.sFlash.sData.i16param&AIRVOLUME){
				vPortAsInput(PORT_INPUT2);
				vPortAsInput(PORT_INPUT3);
				vPortDisablePullup(PORT_INPUT2);
				vPortDisablePullup(PORT_INPUT3);
				vAHI_DioWakeEnable(PORT_INPUT_MASK_AIRVOLUME, 0); // also use as DIO WAKE SOURCE
				vAHI_DioWakeEdge(PORT_INPUT_MASK_AIRVOLUME, 0); // 割り込みエッジ(立上りに設定)
			}else{
				vPortAsInput(DIO_BUTTON);
				vPortAsInput(PORT_INPUT2);
				vPortAsInput(PORT_INPUT3);

				if(sAppData.sFlash.sData.i16param&FIFO){
					vPortDisablePullup(DIO_BUTTON);
				}

				vPortDisablePullup(PORT_INPUT2);
				vPortDisablePullup(PORT_INPUT3);
				vAHI_DioWakeEnable(PORT_INPUT_MASK_ADXL345, 0); // also use as DIO WAKE SOURCE
				vAHI_DioWakeEdge(PORT_INPUT_MASK_ADXL345, 0); // 割り込みエッジ(立上りに設定)
			}
		}else{
			vPortAsInput(DIO_BUTTON);
			//	入力ボタンのプルアップを停止する
			if (sAppData.sFlash.sData.u8mode == PKT_ID_IO_TIMER){	// ドアタイマー
				vPortDisablePullup(DIO_BUTTON); // 外部プルアップのため
			}
			vAHI_DioWakeEnable(PORT_INPUT_MASK, 0); // also use as DIO WAKE SOURCE
			vAHI_DioWakeEdge(0, PORT_INPUT_MASK); // 割り込みエッジ（立下りに設定）
		}
	}
#endif

	if( (0x31 <=  sAppData.sFlash.sData.u8mode && sAppData.sFlash.sData.u8mode < 0x50) || sAppData.sFlash.sData.u8mode == 0xD1 ){
		// SMBUS の初期化
		vSMBusInit();
	}
}

/** @ingroup MASTER
 * UART を初期化する
 * @param u32Baud ボーレート
 */
void vSerialInit(uint32 u32Baud, tsUartOpt *pUartOpt) {
	/* Create the debug port transmit and receive queues */
	static uint8 au8SerialTxBuffer[768];
	static uint8 au8SerialRxBuffer[256];

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
	V_PRINTF(LB LB"*** %s (ED_Inp) %d.%02d-%d ***", APP_NAME, VERSION_MAIN, VERSION_SUB, VERSION_VAR);
	V_PRINTF(LB"* App ID:%08x Long Addr:%08x Short Addr %04x LID %02d Calib=%d",
			sToCoNet_AppContext.u32AppId, ToCoNet_u32GetSerial(), sToCoNet_AppContext.u16ShortAddress,
			sAppData.sFlash.sData.u8id,
			sAppData.sFlash.sData.u16RcClock);
	V_FLUSH();
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

/**
 * センサーアクティブ時のポートの制御を行う
 *
 * @param bActive ACTIVE時がTRUE
 */
void vPortSetSns(bool_t bActive)
{
	if (IS_APPCONF_OPT_INVERSE_SNS_ACTIVE()) {
		bActive = !bActive;
	}
	if( sAppData.sFlash.sData.u8mode == PKT_ID_IO_TIMER ){
		bActive = !bActive;
	}

	//A_PRINTF( LB"%s", bActive?"TRUE":"FALSE" );
	//A_FLUSH();

	vPortSet_TrueAsLo(DIO_SNS_POWER, bActive);
}
/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/
