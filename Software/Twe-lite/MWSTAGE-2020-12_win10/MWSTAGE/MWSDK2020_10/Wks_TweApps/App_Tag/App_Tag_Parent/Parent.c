/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */

/****************************************************************************/
/***        Include files                                                 ***/
/****************************************************************************/
#include <string.h>

#include <jendefs.h>
#include <AppHardwareApi.h>

#include "utils.h"

#include "Parent.h"
#include "config.h"

#include "serial.h"
#include "fprintf.h"
#include "sprintf.h"

#include "btnMgr.h"

#include "Interactive.h"
#include "sercmd_gen.h"

#include "common.h"
#include "AddrKeyAry.h"

/****************************************************************************/
/***        ToCoNet Definitions                                           ***/
/****************************************************************************/
#define ToCoNet_USE_MOD_NWK_LAYERTREE // Network definition
#define ToCoNet_USE_MOD_NBSCAN // Neighbour scan module
#define ToCoNet_USE_MOD_NBSCAN_SLAVE // Neighbour scan slave module
//#define ToCoNet_USE_MOD_CHANNEL_MGR
#define ToCoNet_USE_MOD_NWK_MESSAGE_POOL
#define ToCoNet_USE_MOD_DUPCHK

// includes
#include "ToCoNet.h"
#include "ToCoNet_mod_prototype.h"

#include "app_event.h"

/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/
#define TOCONET_DEBUG_LEVEL 0
//#define USE_LCD

#ifdef USE_LCD
#include "LcdDriver.h"
#include "LcdDraw.h"
#include "LcdPrint.h"
#define V_PRINTF_LCD(...) vfPrintf(&sLcdStream, __VA_ARGS__)
#define V_PRINTF_LCD_BTM(...) vfPrintf(&sLcdStreamBtm, __VA_ARGS__)
#endif

/****************************************************************************/
/***        Type Definitions                                              ***/
/****************************************************************************/

/****************************************************************************/
/***        Local Function Prototypes                                     ***/
/****************************************************************************/
static void vProcessEvCore(tsEvent *pEv, teEvent eEvent, uint32 u32evarg);
static void vInitHardware(int f_warm_start);
static void vSerialInit(uint32 u32Baud, tsUartOpt *pUartOpt);

void vSerOutput_Standard(tsRxPktInfo sRxPktInfo, uint8 *p);
void vSerOutput_SmplTag3(tsRxPktInfo sRxPktInfo, uint8 *p);
void vSerOutput_Uart(tsRxPktInfo sRxPktInfo, uint8 *p);

void vSerOutput_Secondary();

void vSerInitMessage();
void vProcessSerialCmd(tsSerCmd_Context *pCmd);

void vLED_Toggle(void);

#ifdef USE_LCD
static void vLcdInit(void);
static void vLcdRefresh(void);
#endif
/****************************************************************************/
/***        Exported Variables                                            ***/
/****************************************************************************/

/****************************************************************************/
/***        Local Variables                                               ***/
/****************************************************************************/
tsAppData_Pa sAppData; // application information
tsFILE sSerStream; // serial output context
tsSerialPortSetup sSerPort; // serial port queue

tsSerCmd_Context sSerCmdOut; //!< シリアル出力用

tsAdrKeyA_Context sEndDevList; // 子機の発報情報を保存するデータベース

#ifdef USE_LCD
static tsFILE sLcdStream, sLcdStreamBtm;
#endif

#ifdef USE_MONOSTICK
static bool_t bVwd = FALSE;
#endif
static uint32 u32sec;
static uint32 u32TempCount_ms = 0;

/****************************************************************************/
/***        ToCoNet Callback Functions                                    ***/
/****************************************************************************/
/**
 * アプリケーションの起動時の処理
 * - ネットワークの設定
 * - ハードウェアの初期化
 */
void cbAppColdStart(bool_t bAfterAhiInit) {
	if (!bAfterAhiInit) {
		// before AHI init, very first of code.

		// Register modules
		ToCoNet_REG_MOD_ALL();

	} else {
		// disable brown out detect
		vAHI_BrownOutConfigure(0, //0:2.0V 1:2.3V
				FALSE, FALSE, FALSE, FALSE);

		// clear application context
		memset(&sAppData, 0x00, sizeof(sAppData));
		ADDRKEYA_vInit(&sEndDevList);
		SPRINTF_vInit128();

		// フラッシュメモリからの読み出し
		//   フラッシュからの読み込みが失敗した場合、ID=15 で設定する
		sAppData.bFlashLoaded = Config_bLoad(&sAppData.sFlash);

		// ToCoNet configuration
		sToCoNet_AppContext.u32AppId = sAppData.sFlash.sData.u32appid;
		sToCoNet_AppContext.u8Channel = sAppData.sFlash.sData.u8ch;
		sToCoNet_AppContext.u32ChMask = sAppData.sFlash.sData.u32chmask;

		sToCoNet_AppContext.bRxOnIdle = TRUE;
		sToCoNet_AppContext.u8TxMacRetry = 1;

		// Register
		ToCoNet_Event_Register_State_Machine(vProcessEvCore);

		// Others
		vInitHardware(FALSE);
		Interactive_vInit();

		// シリアルの書式出力のため
		if (IS_APPCONF_OPT_UART_BIN()) {
			SerCmdBinary_vInit(&sSerCmdOut, NULL, 128); // バッファを指定せず初期化
		} else {
			SerCmdAscii_vInit(&sSerCmdOut, NULL, 128); // バッファを指定せず初期化
		}
	}
}

/**
 * スリープ復帰時の処理（本アプリケーションでは処理しない)
 * @param bAfterAhiInit
 */
void cbAppWarmStart(bool_t bAfterAhiInit) {
	cbAppColdStart(bAfterAhiInit);
}

/**
 * メイン処理
 * - シリアルポートの処理
 */
void cbToCoNet_vMain(void) {
	/* handle uart input */
	vHandleSerialInput();
}

/**
 * ネットワークイベント。
 * - E_EVENT_TOCONET_NWK_START\n
 *   ネットワーク開始時のイベントを vProcessEvCore に伝達
 *
 * @param eEvent
 * @param u32arg
 */
void cbToCoNet_vNwkEvent(teEvent eEvent, uint32 u32arg) {
	switch (eEvent) {
	case E_EVENT_TOCONET_NWK_START:
		// send this event to the local event machine.
		ToCoNet_Event_Process(eEvent, u32arg, vProcessEvCore);
		break;
	default:
		break;
	}
}

/**
 * 子機または中継機を経由したデータを受信する。
 *
 * - アドレスを取り出して、内部のデータベースへ登録（メッセージプール送信用）
 * - UART に指定書式で出力する
 *   - 出力書式\n
 *     ::(受信元ルータまたは親機のアドレス):(シーケンス番号):(送信元アドレス):(LQI)<CR><LF>
 *
 * @param pRx 受信データ構造体
 */
void cbToCoNet_vRxEvent(tsRxDataApp *pRx) {
	tsRxPktInfo sRxPktInfo;

	uint8 *p = pRx->auData;

	// 暗号化対応時に平文パケットは受信しない
	if (IS_APPCONF_OPT_SECURE()) {
		if (!pRx->bSecurePkt) {
			return;
		}
	}

	// パケットの表示
	if (pRx->u8Cmd == TOCONET_PACKET_CMD_APP_DATA) {

		// LED の点灯を行う
		sAppData.u16LedDur_ct = 25;

		// 基本情報
		sRxPktInfo.u8lqi_1st = pRx->u8Lqi;
		sRxPktInfo.u32addr_1st = pRx->u32SrcAddr;

		// データの解釈
		uint8 u8b = G_OCTET();

		// 受信機アドレス
		sRxPktInfo.u32addr_rcvr = TOCONET_NWK_ADDR_PARENT;
		if (u8b == 'R') {
			// ルータからの受信
			sRxPktInfo.u32addr_1st = G_BE_DWORD();
			sRxPktInfo.u8lqi_1st = G_OCTET();

			sRxPktInfo.u32addr_rcvr = pRx->u32SrcAddr;
		}

		// ID などの基本情報
		sRxPktInfo.u8id = G_OCTET();
		sRxPktInfo.u16fct = G_BE_WORD();

		// パケットの種別により処理を変更
		sRxPktInfo.u8pkt = G_OCTET();

		// 出力用の関数を呼び出す
		if (IS_APPCONF_OPT_SmplTag() || IS_APPCONF_OPT_UART_CSV() ) {
			vSerOutput_SmplTag3( sRxPktInfo, p);
		} else if (IS_APPCONF_OPT_UART()) {
			vSerOutput_Uart(sRxPktInfo, p);
		} else {
			vSerOutput_Standard(sRxPktInfo, p);
		}

		// データベースへ登録（線形配列に格納している）
		ADDRKEYA_vAdd(&sEndDevList, sRxPktInfo.u32addr_1st, 0); // アドレスだけ登録。
	}
}

/**
 * 送信完了時のイベント
 * @param u8CbId
 * @param bStatus
 */
void cbToCoNet_vTxEvent(uint8 u8CbId, uint8 bStatus) {
	return;
}

/**
 * ハードウェア割り込みの遅延実行部
 *
 * - BTM による IO の入力状態をチェック\n
 *   ※ 本サンプルでは特別な使用はしていない
 *
 * @param u32DeviceId
 * @param u32ItemBitmap
 */
void cbToCoNet_vHwEvent(uint32 u32DeviceId, uint32 u32ItemBitmap) {

	switch (u32DeviceId) {
	case E_AHI_DEVICE_TICK_TIMER:
		// LED の点灯消灯を制御する
		if (sAppData.u16LedDur_ct) {
			sAppData.u16LedDur_ct--;
			vAHI_DoSetDataOut( 0, 0x01<<1 );
		} else {
			vAHI_DoSetDataOut( 0x01<<1, 0 );
		}

		static uint32 u32LedCt_before = 0;
		// LED の点灯消灯を制御する
		if (sAppData.u32LedCt) {
			u32LedCt_before = sAppData.u32LedCt;
			sAppData.u32LedCt--;
			vPortSetLo(PORT_OUT1);
		}else if(u32LedCt_before == 1){
			u32LedCt_before = 0;
			vPortSetHi(PORT_OUT1);
			sAppData.u8DO_State = 0;
		}

#ifdef USE_MONOSTICK
		bVwd = !bVwd;
		vPortSet_TrueAsLo(9, bVwd);
#endif
		break;

	default:
		break;
	}

}

/**
 * ハードウェア割り込み
 * - 処理なし
 *
 * @param u32DeviceId
 * @param u32ItemBitmap
 * @return
 */
uint8 cbToCoNet_u8HwInt(uint32 u32DeviceId, uint32 u32ItemBitmap) {
	return FALSE;
}

/****************************************************************************/
/***        Local Functions                                               ***/
/****************************************************************************/

/**
 * ハードウェアの初期化
 * @param f_warm_start
 */
static void vInitHardware(int f_warm_start) {
	// BAUD ピンが GND になっている場合、かつフラッシュの設定が有効な場合は、設定値を採用する (v1.0.3)
	tsUartOpt sUartOpt;
	memset(&sUartOpt, 0, sizeof(tsUartOpt));
	uint32 u32baud = UART_BAUD;
//	if (sAppData.bFlashLoaded && bPortRead(PORT_BAUD)) {
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

	ToCoNet_vDebugInit(&sSerStream);
	ToCoNet_vDebugLevel(TOCONET_DEBUG_LEVEL);

	// IO の設定
	vPortSetHi( PORT_OUT1 );
	vPortAsOutput( PORT_OUT1 );

	bAHI_DoEnableOutputs(TRUE);
	vAHI_DoSetDataOut( 0x01<<1, 0 );

#ifdef USE_MONOSTICK
	vPortSetLo(11);				// 外部のウォッチドッグを有効にする。
	vPortSet_TrueAsLo(9, bVwd);	// VWDをいったんHiにする。
	vPortAsOutput(11);			// DIO11を出力として使用する。
	vPortAsOutput(9);			// DIO9を出力として使用する。
#else
	vPortSetHi( PORT_OUT2 );
	vPortAsOutput( PORT_OUT2 );
#endif

	// LCD の設定
#ifdef USE_LCD
	vLcdInit();
#endif
}

/**
 * UART の初期化
 */
static void vSerialInit(uint32 u32Baud, tsUartOpt *pUartOpt) {
	/* Create the debug port transmit and receive queues */
	static uint8 au8SerialTxBuffer[1532];
	static uint8 au8SerialRxBuffer[512];

	/* Initialise the serial port to be used for debug output */
	sSerPort.pu8SerialRxQueueBuffer = au8SerialRxBuffer;
	sSerPort.pu8SerialTxQueueBuffer = au8SerialTxBuffer;
//	sSerPort.u32BaudRate = UART_BAUD;
	sSerPort.u32BaudRate = u32Baud;
	sSerPort.u16AHI_UART_RTS_LOW = 0xffff;
	sSerPort.u16AHI_UART_RTS_HIGH = 0xffff;
	sSerPort.u16SerialRxQueueSize = sizeof(au8SerialRxBuffer);
	sSerPort.u16SerialTxQueueSize = sizeof(au8SerialTxBuffer);
	sSerPort.u8SerialPort = UART_PORT;
	sSerPort.u8RX_FIFO_LEVEL = E_AHI_UART_FIFO_LEVEL_1;
	SERIAL_vInitEx(&sSerPort, pUartOpt);

	sSerStream.bPutChar = SERIAL_bTxChar;
	sSerStream.u8Device = UART_PORT;
}


/**
 * アプリケーション主要処理
 * - E_STATE_IDLE\n
 *   ネットワークの初期化、開始
 *
 * - E_STATE_RUNNING\n
 *   - データベースのタイムアウト処理
 *   - 定期的なメッセージプール送信
 *
 * @param pEv
 * @param eEvent
 * @param u32evarg
 */
static void vProcessEvCore(tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {

	if (eEvent == E_EVENT_TICK_SECOND) {
		u32sec++;
	}

	switch (pEv->eState) {
	case E_STATE_IDLE:
		if (eEvent == E_EVENT_START_UP) {
			vSerInitMessage();

			V_PRINTF(LB"[E_STATE_IDLE]");

			if (IS_APPCONF_OPT_SECURE()) {
				bool_t bRes = bRegAesKey(sAppData.sFlash.sData.u32EncKey);
				V_PRINTF(LB "*** Register AES key (%d) ***", bRes);
			}

			// Configure the Network
			sAppData.sNwkLayerTreeConfig.u8Layer = 0;
			sAppData.sNwkLayerTreeConfig.u8Role = TOCONET_NWK_ROLE_PARENT;

#ifndef OLDNET
			sAppData.sNwkLayerTreeConfig.u8StartOpt = TOCONET_MOD_LAYERTREE_STARTOPT_NB_BEACON;
			sAppData.sNwkLayerTreeConfig.u8Second_To_Beacon = TOCONET_MOD_LAYERTREE_DEFAULT_BEACON_DUR;
#endif
			sAppData.pContextNwk =
					ToCoNet_NwkLyTr_psConfig(&sAppData.sNwkLayerTreeConfig);
			if (sAppData.pContextNwk) {
				ToCoNet_Nwk_bInit(sAppData.pContextNwk);
				ToCoNet_Nwk_bStart(sAppData.pContextNwk);
			}

		} else if (eEvent == E_EVENT_TOCONET_NWK_START) {
			ToCoNet_Event_SetState(pEv, E_STATE_RUNNING);
		} else {
			;
		}
		break;

	case E_STATE_RUNNING:
		if (eEvent == E_EVENT_NEW_STATE) {
			V_PRINTF(LB"[E_STATE_RUNNING]");
		} else if (eEvent == E_EVENT_TICK_SECOND) {
			// 毎秒ごとのシリアル出力
			vSerOutput_Secondary();

			// 定期クリーン（タイムアウトしたノードを削除する）
			ADDRKEYA_bFind(&sEndDevList, 0, 0);

#if 0 // EndDevice_Remote との通信が必要な場合 MessagePool が使いやすいが、この時点では不要なのでコメントアウトする
			static uint8 u8Ct_s = 0;
			int i;

			// 共有情報（メッセージプール）の送信
			//   - OCTET: 経過秒 (0xFF は終端)
			//   - BE_DWORD: 発報アドレス
			if (++u8Ct_s > PARENT_MSGPOOL_TX_DUR_s) {
				uint8 au8pl[TOCONET_MOD_MESSAGE_POOL_MAX_MESSAGE];
					// メッセージプールの最大バイト数は 64 なので、これに収まる数とする。
				uint8 *q = au8pl;

				for (i = 0; i < ADDRKEYA_MAX_HISTORY; i++) {
					if (sEndDevList.au32ScanListAddr[i]) {

						uint16 u16Sec = (u32TickCount_ms
								- sEndDevList.au32ScanListTick[i]) / 1000;
						if (u16Sec >= 0xF0)
							continue; // 古すぎるので飛ばす

						S_OCTET(u16Sec & 0xFF);
						S_BE_DWORD(sEndDevList.au32ScanListAddr[i]);
					}
				}
				S_OCTET(0xFF);

				S_OCTET('A'); // ダミーデータ(不要：テスト目的)
				S_OCTET('B');
				S_OCTET('C');
				S_OCTET('D');

				ToCoNet_MsgPl_bSetMessage(0, 0, q - au8pl, au8pl);
				u8Ct_s = 0;
			}
#endif
		} else {
			;
		}
		break;

	default:
		break;
	}
}

/**
 * 初期化メッセージ
 */
void vSerInitMessage() {
	if (!IS_APPCONF_OPT_UART()) {
		A_PRINTF(LB "*** " APP_NAME " (Parent) %d.%02d-%d ***", VERSION_MAIN, VERSION_SUB, VERSION_VAR);
		A_PRINTF(LB "* App ID:%08x Long Addr:%08x Short Addr %04x LID %02d" LB,
				sToCoNet_AppContext.u32AppId, ToCoNet_u32GetSerial(), sToCoNet_AppContext.u16ShortAddress,
				sAppData.sFlash.sData.u8id);
	} else {
		uint8 u8buff[32], *q = u8buff;

		memset(u8buff, 0, sizeof(u8buff));
		memcpy(u8buff, APP_NAME, sizeof(APP_NAME));
		q += 16;

		S_OCTET(VERSION_MAIN);
		S_OCTET(VERSION_SUB);
		S_OCTET(VERSION_VAR);
		S_BE_DWORD(sToCoNet_AppContext.u32AppId);
		S_BE_DWORD(ToCoNet_u32GetSerial());

		sSerCmdOut.u16len = q - u8buff;
		sSerCmdOut.au8data = u8buff;

		sSerCmdOut.vOutput(&sSerCmdOut, &sSerStream);

		sSerCmdOut.au8data = NULL;
	}

#ifdef USE_LCD
	// 最下行を表示する
	V_PRINTF_LCD_BTM(" ToCoSamp IO Monitor %d.%02d-%d ", VERSION_MAIN, VERSION_SUB, VERSION_VAR);
	vLcdRefresh();
#endif
}

/**
 * 標準の出力
 */
void vSerOutput_Standard(tsRxPktInfo sRxPktInfo, uint8 *p) {
	// 受信機のアドレス
	A_PRINTF("::rc=%08X", sRxPktInfo.u32addr_rcvr);

	// LQI
	A_PRINTF(":lq=%d", sRxPktInfo.u8lqi_1st);

	// フレーム
	A_PRINTF(":ct=%04X", sRxPktInfo.u16fct);

	// 送信元子機アドレス
	A_PRINTF(":ed=%08X:id=%X", sRxPktInfo.u32addr_1st, sRxPktInfo.u8id);

	switch(sRxPktInfo.u8pkt) {
	case PKT_ID_MULTISENSOR:
		_C {
			uint8 u8batt = G_OCTET();
			uint16 u16adc1 = G_BE_WORD();
			uint16 u16adc2 = G_BE_WORD();
			uint8 u8SnsNum = G_OCTET();
			// センサー情報
			A_PRINTF(":ba=%04d:a1=%04d:a2=%04d", DECODE_VOLT(u8batt), u16adc1, u16adc2 );
			uint8 u8Sensor;
			uint8 i;
			for( i=0; i<u8SnsNum; i++ ){
				u8Sensor = G_OCTET();
				switch(u8Sensor){
				case PKT_ID_SHT21:
				case PKT_ID_SHT31:
				case PKT_ID_SHTC3:
				{
					int16 i16temp = G_BE_WORD();
					int16 i16humd = G_BE_WORD();
					A_PRINTF(":te=%04d:hu=%04d", i16temp, i16humd );
				}
					break;
				case PKT_ID_ADT7410:
				{
					int16 i16temp = G_BE_WORD();
					A_PRINTF(":te=%04d", i16temp );
				}
					break;
				case PKT_ID_MPL115A2:
				{
					int16 i16atmo = G_BE_WORD();
					// センサー情報
					A_PRINTF(":at=%04d", i16atmo );
				}
					break;
				case PKT_ID_LIS3DH:
				{
					int16 i16x = G_BE_WORD();
					int16 i16y = G_BE_WORD();
					int16 i16z = G_BE_WORD();
					A_PRINTF(":x=%04d:y=%04d:z=%04d", i16x, i16y, i16z );
				}
					break;
				case PKT_ID_ADXL345:
				{
					uint8 u8mode = G_OCTET();
					A_PRINTF(":md=%02X", u8mode );
					int16 i16x, i16y, i16z;
					if( u8mode == 0xfa ){
						uint8 u8num = G_OCTET();
						uint8 j;
						for( j=0; j<u8num; j++ ){
							i16x = G_BE_WORD();
							i16y = G_BE_WORD();
							i16z = G_BE_WORD();
							A_PRINTF(":x=%04d:y=%04d:z=%04d", i16x, i16y, i16z );
						}
					}else{
						i16x = G_BE_WORD();
						i16y = G_BE_WORD();
						i16z = G_BE_WORD();
						A_PRINTF(":x=%04d:y=%04d:z=%04d", i16x, i16y, i16z );
					}
				}
					break;
				case PKT_ID_L3GD20:
				{
					int16 i16x = G_BE_WORD();
					int16 i16y = G_BE_WORD();
					int16 i16z = G_BE_WORD();
					A_PRINTF(":gx=%04d:gy=%04d:gz=%04d", i16x, i16y, i16z );
				}
					break;
				case PKT_ID_TSL2561:
				{
					uint32	u32lux = G_BE_DWORD();
					A_PRINTF(":lx=%04d", u32lux );
				}
				break;
				case PKT_ID_S1105902:
				{
					int16 u16R = G_BE_WORD();
					int16 u16G = G_BE_WORD();
					int16 u16B = G_BE_WORD();
					int16 u16I = G_BE_WORD();
					A_PRINTF(":re=%04d:gr=%04d:bl=%04d:in:%04d", u16R, u16G, u16B, u16I );
				}
				break;
				case PKT_ID_BME280:
				{
					int16	i16temp = G_BE_WORD();
					uint16	u16hum = G_BE_WORD();
					uint16	u16atmo = G_BE_WORD();
					A_PRINTF(":te=%04d:hu=%04d:at=%04d", i16temp, u16hum, u16atmo);
				}
				break;
				default:
					break;
				}
			}
			A_PRINTF(LB);
		}
		break;
	case PKT_ID_BUTTON:
		_C {
			uint8 u8batt = G_OCTET();

			uint16 u16adc1 = G_BE_WORD();(void)u16adc1;
			uint16 u16adc2 = G_BE_WORD();(void)u16adc2;

			uint8 u8mode = G_OCTET();
			bool_t bRegularTransmit = ((u8mode&0x80)>>7);
			uint8 u8Bitmap = G_OCTET();

			if(!bRegularTransmit){
				if( (u8mode&0x04) || (u8mode&0x02) ){
					if(u8Bitmap){
						vPortSetLo(PORT_OUT1);
						sAppData.u8DO_State = 1;
						if(IS_APPCONF_OPT_DIO_AUTO_HI()){
							sAppData.u32LedCt = 250;
						}
					}else{
						vPortSetHi(PORT_OUT1);
						sAppData.u8DO_State = 0;
						sAppData.u32LedCt = 0;
					}
				}else{
					if( IS_APPCONF_OPT_DIO_AUTO_HI() ){
						// Turn on LED
						sAppData.u32LedCt = 250;
						sAppData.u8DO_State = 1;
					}else{
						vLED_Toggle();
					}
				}
			}

			// センサー情報
			A_PRINTF(":ba=%04d:bt=%04d" LB,
					DECODE_VOLT(u8batt), sAppData.u8DO_State );

#ifdef USE_LCD
			// LCD への出力
			V_PRINTF_LCD("%03d:%08X:%03d:%02X:P:%04d:%04d:\n",
					u32sec % 1000,
					sRxPktInfo.u32addr_1st,
					sRxPktInfo.u8lqi_1st,
					sRxPktInfo.u16fct & 0xFF,
					sAppData.u8DO_State
					);
			vLcdRefresh();
#endif
		}
		break;

	case PKT_ID_STANDARD:
		_C {
			uint8 u8batt = G_OCTET();

			uint16 u16adc1 = G_BE_WORD();
			uint16 u16adc2 = G_BE_WORD();
			uint16 u16pc1 = G_BE_WORD();
			uint16 u16pc2 = G_BE_WORD();

			// センサー情報
			if( u16adc1&0x8000 ){
				A_PRINTF(":ba=%04d:a1=%04d:a2=%04d:a3=%03d:a4=%03d" LB,
						DECODE_VOLT(u8batt), u16adc1&0x7FFF, u16adc2, u16pc1, u16pc2);
			}else{
				A_PRINTF(":ba=%04d:a1=%04d:a2=%04d:p0=%03d:p1=%03d" LB,
						DECODE_VOLT(u8batt), u16adc1, u16adc2, u16pc1, u16pc2);
			}

#ifdef USE_LCD
			// LCD への出力:
			V_PRINTF_LCD("%03d:%08X:%03d:%02X:A:%04d:%04d:%04d\n",
					u32sec % 1000,
					sRxPktInfo.u32addr_1st,
					sRxPktInfo.u8lqi_1st,
					sRxPktInfo.u16fct & 0xFF,
					u16adc1,
					u16adc2
					);
			vLcdRefresh();
#endif
		}
		break;

	case PKT_ID_LM61:
		_C {
			uint8 u8batt = G_OCTET();

			uint16	u16adc1 = G_BE_WORD();
			uint16	u16adc2 = G_BE_WORD();
			uint16	u16pc1 = G_BE_WORD();(void)u16pc1;
			uint16	u16pc2 = G_BE_WORD();(void)u16pc2;
			int16	bias = (int16)G_BE_WORD();

			// LM61用の温度変換
			//   Vo=T[℃]x10[mV]+600[mV]
			//   T     = Vo/10-60
			//   100xT = 10xVo-6000
			int32 iTemp = 10 * (int32)u16adc2 - 6000L + bias;

			// センサー情報
			A_PRINTF(":ba=%04d:te=%04d:a0=%04d:a1=%03d" LB,
					DECODE_VOLT(u8batt), iTemp, u16adc1, u16adc2);

#ifdef USE_LCD
			// LCD への出力
			V_PRINTF_LCD("%03d:%08X:%03d:%02X:L:%04d:%04d:%04d\n",
					u32sec % 1000,
					sRxPktInfo.u32addr_1st,
					sRxPktInfo.u8lqi_1st,
					sRxPktInfo.u16fct & 0xFF,
					u16adc1,
					u16adc2
					);
			vLcdRefresh();
#endif
		}
		break;

	case PKT_ID_SHT21:
		_C {
			uint8 u8batt = G_OCTET();

			uint16 u16adc1 = G_BE_WORD();
			uint16 u16adc2 = G_BE_WORD();
			int16 i16temp = G_BE_WORD();
			int16 i16humd = G_BE_WORD();

			// センサー情報
			A_PRINTF(":ba=%04d:a1=%04d:a2=%04d:te=%04d:hu=%04d" LB,
					DECODE_VOLT(u8batt), u16adc1, u16adc2, i16temp, i16humd);

#ifdef USE_LCD
			// LCD への出力
			V_PRINTF_LCD("%03d:%08X:%03d:%02X:S:%04d:%04d\n",
					u32sec % 1000,
					sRxPktInfo.u32addr_1st,
					sRxPktInfo.u8lqi_1st,
					sRxPktInfo.u16fct & 0xFF,
					i16temp,
					i16humd
					);
			vLcdRefresh();
#endif
		}
		break;

	case PKT_ID_SHT31:
	case PKT_ID_SHTC3:
		_C {
			uint8 u8batt = G_OCTET();

			uint16 u16adc1 = G_BE_WORD();
			uint16 u16adc2 = G_BE_WORD();
			int16 i16temp = G_BE_WORD();
			int16 i16humd = G_BE_WORD();

			// センサー情報
			A_PRINTF(":ba=%04d:a1=%04d:a2=%04d:te=%04d:hu=%04d" LB,
					DECODE_VOLT(u8batt), u16adc1, u16adc2, i16temp, i16humd);

#ifdef USE_LCD
			// LCD への出力
			V_PRINTF_LCD("%03d:%08X:%03d:%02X:S:%04d:%04d\n",
					u32sec % 1000,
					sRxPktInfo.u32addr_1st,
					sRxPktInfo.u8lqi_1st,
					sRxPktInfo.u16fct & 0xFF,
					i16temp,
					i16humd
					);
			vLcdRefresh();
#endif
		}
		break;

	case PKT_ID_MAX31855:
		_C {
			uint8 u8batt = G_OCTET();

			uint16 u16adc1 = G_BE_WORD();
			uint16 u16adc2 = G_BE_WORD();
			int32 i32temp = G_BE_DWORD();
			int32 i32itemp = G_BE_DWORD();

			// センサー情報
			A_PRINTF(":ba=%04d:a1=%04d:a2=%04d:te=%d:ite=%d" LB,
					DECODE_VOLT(u8batt), u16adc1, u16adc2, i32temp, i32itemp);

#ifdef USE_LCD
			// LCD への出力
			V_PRINTF_LCD("%03d:%08X:%03d:%02X:S:%04d:%04d\n",
					u32sec % 1000,
					sRxPktInfo.u32addr_1st,
					sRxPktInfo.u8lqi_1st,
					sRxPktInfo.u16fct & 0xFF,
					i32temp,
					i32itemp
					);
			vLcdRefresh();
#endif
		}
		break;

	case PKT_ID_S1105902:
		_C {
			uint8 u8batt = G_OCTET();

			uint16 u16adc1 = G_BE_WORD();
			uint16 u16adc2 = G_BE_WORD();
			int16 u16R = G_BE_WORD();
			int16 u16G = G_BE_WORD();
			int16 u16B = G_BE_WORD();
			int16 u16I = G_BE_WORD();

			// センサー情報
			A_PRINTF(":ba=%04d:a1=%04d:a2=%04d:re=%04d:gr=%04d:bl=%04d:in:%04d" LB,
					DECODE_VOLT(u8batt), u16adc1, u16adc2, u16R, u16G, u16B, u16I );

#ifdef USE_LCD
			// LCD への出力
			V_PRINTF_LCD("%03d:%08X:%03d:%02X:S:%04d:%04d\n",
					u32sec % 1000,
					sRxPktInfo.u32addr_1st,
					sRxPktInfo.u8lqi_1st,
					sRxPktInfo.u16fct & 0xFF,
					i16temp,
					i16humd
					);
			vLcdRefresh();
#endif
		}
		break;


	case PKT_ID_LIS3DH:
		_C {
			uint8 u8batt = G_OCTET();

			uint16 u16adc1 = G_BE_WORD();
			uint16 u16adc2 = G_BE_WORD();
			int16 i16x = G_BE_WORD();
			int16 i16y = G_BE_WORD();
			int16 i16z = G_BE_WORD();

			// センサー情報
			A_PRINTF(":ba=%04d:a1=%04d:a2=%04d:x=%04d:y=%04d:z=%04d" LB,
					DECODE_VOLT(u8batt), u16adc1, u16adc2, i16x, i16y, i16z );

#ifdef USE_LCD
			// LCD への出力
			V_PRINTF_LCD("%03d:%08X:%03d:%02X:I:%04d:%04d:%04d\n",
					u32sec % 1000,
					sRxPktInfo.u32addr_1st,
					sRxPktInfo.u8lqi_1st,
					sRxPktInfo.u16fct & 0xFF,
					i16x,
					i16y,
					i16z
					);
			vLcdRefresh();
#endif
		}
		break;

	case PKT_ID_L3GD20:
		_C {
			uint8 u8batt = G_OCTET();

			uint16 u16adc1 = G_BE_WORD();
			uint16 u16adc2 = G_BE_WORD();
			int16 i16x = G_BE_WORD();
			int16 i16y = G_BE_WORD();
			int16 i16z = G_BE_WORD();

			// センサー情報
			A_PRINTF(":ba=%04d:a1=%04d:a2=%04d:x=%04d:y=%04d:z=%04d" LB,
					DECODE_VOLT(u8batt), u16adc1, u16adc2, i16x, i16y, i16z );

#ifdef USE_LCD
			// LCD への出力
			V_PRINTF_LCD("%03d:%08X:%03d:%02X:I:%04d:%04d:%04d\n",
					u32sec % 1000,
					sRxPktInfo.u32addr_1st,
					sRxPktInfo.u8lqi_1st,
					sRxPktInfo.u16fct & 0xFF,
					i16x,
					i16y,
					i16z
					);
			vLcdRefresh();
#endif
		}
		break;

	case PKT_ID_ADXL345:
		_C {
			uint8 u8batt = G_OCTET();

			uint16 u16adc1 = G_BE_WORD();
			uint16 u16adc2 = G_BE_WORD();
			int16 i16x = G_BE_WORD();
			int16 i16y = G_BE_WORD();
			int16 i16z = G_BE_WORD();
			uint8 u8bitmap = G_OCTET();

			uint8 u8ActTapSource = ( u16adc1>>12 )|((u16adc2>>8)&0xF0);(void)u8ActTapSource;

			u16adc1 = u16adc1&0x0FFF;
			u16adc2 = u16adc2&0x0FFF;

			// センサー情報
			A_PRINTF(":ba=%04d:a1=%04d:a2=%04d",
					DECODE_VOLT(u8batt), u16adc1, u16adc2);

			if( u8bitmap == 0xf9 ){
				A_PRINTF( ":xmin=%04d:xave=%04d:xmax=%04d", i16x, i16y, i16z );
				uint16 u16Sample = G_BE_WORD();
				i16x = G_BE_WORD();
				i16y = G_BE_WORD();
				i16z = G_BE_WORD();
				A_PRINTF( ":ymin=%04d:yave=%04d:ymax=%04d", i16x, i16y, i16z );
				i16x = G_BE_WORD();
				i16y = G_BE_WORD();
				i16z = G_BE_WORD();
				A_PRINTF( ":zmin=%04d:zave=%04d:zmax=%04d", i16x, i16y, i16z, u16Sample );
			}else if(u8bitmap == 0xfa){
				uint8 u8num = G_OCTET();
				uint8 i;
				A_PRINTF( ":x=%04d:y=%04d;z=%04d", i16x, i16y, i16z );
				for( i=0; i<u8num-1; i++ ){
					i16x = G_BE_WORD();
					i16y = G_BE_WORD();
					i16z = G_BE_WORD();
					A_PRINTF( ":x=%04d:y=%04d:z=%04d", i16x, i16y, i16z );
				}
			}else{
				A_PRINTF( ":x=%04d:y=%04d:z=%04d", i16x, i16y, i16z );
			}
			A_PRINTF(LB);

#ifdef USE_LCD
			// LCD への出力
			V_PRINTF_LCD("%03d:%08X:%03d:%02X:X:%04d:%04d:%04d\n",
					u32sec % 1000,
					sRxPktInfo.u32addr_1st,
					sRxPktInfo.u8lqi_1st,
					sRxPktInfo.u16fct & 0xFF,
					i16x,
					i16y,
					i16z
					);
			vLcdRefresh();
#endif
		}
		break;


	case PKT_ID_ADT7410:
		_C {
			uint8 u8batt = G_OCTET();

			uint16	u16adc1 = G_BE_WORD();
			uint16	u16adc2 = G_BE_WORD();
			int16 i16temp = G_BE_WORD();

			// センサー情報
			A_PRINTF(":ba=%04d:a1=%04d:a2=%04d:te=%04d" LB,
					DECODE_VOLT(u8batt), u16adc1, u16adc2, i16temp);

#ifdef USE_LCD
			// LCD への出力
			V_PRINTF_LCD("%03d:%08X:%03d:%02X:D:%04d:%04d:%04d\n",
					u32sec % 1000,
					sRxPktInfo.u32addr_1st,
					sRxPktInfo.u8lqi_1st,
					sRxPktInfo.u16fct & 0xFF,
					u16adc1,
					u16adc2
					);
			vLcdRefresh();
#endif
		}
		break;

	case PKT_ID_TSL2561:
		_C {
			uint8 u8batt = G_OCTET();

			uint16	u16adc1 = G_BE_WORD();
			uint16	u16adc2 = G_BE_WORD();
			uint32	u32lux = G_BE_DWORD();

			// センサー情報
			A_PRINTF(":ba=%04d:a1=%04d:a2=%04d:lx=%04d" LB,
					DECODE_VOLT(u8batt), u16adc1, u16adc2, u32lux );

#ifdef USE_LCD
			// LCD への出力
			V_PRINTF_LCD("%03d:%08X:%03d:%02X:D:%04d:%04d:%04d\n",
					u32sec % 1000,
					sRxPktInfo.u32addr_1st,
					sRxPktInfo.u8lqi_1st,
					sRxPktInfo.u16fct & 0xFF,
					u16adc1,
					u16adc2
					);
			vLcdRefresh();
#endif
		}
		break;

	case PKT_ID_BME280:
		_C {
			uint8 u8batt = G_OCTET();

			uint16	u16adc1 = G_BE_WORD();
			uint16	u16adc2 = G_BE_WORD();
			int16	i16temp = G_BE_WORD();
			uint16	u16hum = G_BE_WORD();
			uint16	u16atmo = G_BE_WORD();

			// センサー情報
			A_PRINTF(":ba=%04d:a1=%04d:a2=%04d:tm=%04d:hu=%04d:at=%04d" LB,
					DECODE_VOLT(u8batt), u16adc1, u16adc2, i16temp, u16hum, u16atmo);

#ifdef USE_LCD
			// LCD への出力
			V_PRINTF_LCD("%03d:%08X:%03d:%02X:B:%04d:%04d:%04d:%04d:%04d\n",
					u32sec % 1000,
					sRxPktInfo.u32addr_1st,
					sRxPktInfo.u8lqi_1st,
					sRxPktInfo.u16fct & 0xFF,
					i16temp,
					u16hum,
					u16atmo,
					u16adc1
					);
			vLcdRefresh();
#endif
		}
		break;

	case PKT_ID_MPL115A2:
		_C {
			uint8 u8batt = G_OCTET();

			uint16	u16adc1 = G_BE_WORD();
			uint16	u16adc2 = G_BE_WORD();
			int16 i16atmo = G_BE_WORD();

			// センサー情報
			A_PRINTF(":ba=%04d:a1=%04d:a2=%04d:at=%04d" LB,
					DECODE_VOLT(u8batt), u16adc1, u16adc2, i16atmo);

#ifdef USE_LCD
			// LCD への出力
			V_PRINTF_LCD("%03d:%08X:%03d:%02X:M:%04d:%04d:%04d\n",
					u32sec % 1000,
					sRxPktInfo.u32addr_1st,
					sRxPktInfo.u8lqi_1st,
					sRxPktInfo.u16fct & 0xFF,
					u16atmo,
					u16adc1
					);
			vLcdRefresh();
#endif
		}
		break;


	case PKT_ID_IO_TIMER:
		_C {
			uint8 u8stat = G_OCTET();
			uint32 u32dur = G_BE_DWORD();

			A_PRINTF(":btn=%d:dur=%d" LB, u8stat, u32dur / 1000);

#ifdef USE_LCD
			// LCD への出力
			V_PRINTF_LCD("%03d:%08X:%03d:%02X:B:%04d:%04d\n",
					u32sec % 1000,
					sRxPktInfo.u32addr_1st,
					sRxPktInfo.u8lqi_1st,
					sRxPktInfo.u16fct & 0xFF,
					u8stat,
					u32dur / 1000
					);
			vLcdRefresh();
#endif
		}
		break;

	case PKT_ID_UART:
		_C {
			uint8 u8len = G_OCTET();

			sSerCmdOut.u16len = u8len;
			sSerCmdOut.au8data = p;

			sSerCmdOut.vOutput(&sSerCmdOut, &sSerStream);

			sSerCmdOut.au8data = NULL; // p は関数を抜けると無効であるため、念のため NULL に戻す
		}
		break;

	default:
		break;
	}
}

#define SEPARATER() (IS_APPCONF_OPT_UART_CSV() ? ',' : ';')
#define CONV_STR()  if(IS_APPCONF_OPT_UART_CSV()){\
						A_PRINTF("\t");\
					}

/**
 * 区切り文字を使用した出力(セミコロン区切り・カンマ区切り)
 */
void vSerOutput_SmplTag3( tsRxPktInfo sRxPktInfo, uint8 *p){
	if( sRxPktInfo.u8pkt == PKT_ID_MULTISENSOR ){
		uint8 u8batt = G_OCTET();
		uint16 u16adc1 = G_BE_WORD();
		uint16 u16adc2 = G_BE_WORD();
		uint8 u8SnsNum = G_OCTET();

		// センサー情報
		A_PRINTF( "%c%d%c", SEPARATER(), u32TickCount_ms/1000, SEPARATER() );

		CONV_STR();
		A_PRINTF( "%08X%c"			// 受信機のアドレス
				"%03d%c"			// LQI  (0-255)
				"%03d%c",		// 連番
				sRxPktInfo.u32addr_rcvr & 0x0FFFFFFF, SEPARATER(),
				sRxPktInfo.u8lqi_1st, SEPARATER(),
				sRxPktInfo.u16fct, SEPARATER()
		);

		CONV_STR();
		A_PRINTF( "%07x%c"		// シリアル番号
				"%04d%c"			// 電源電圧 (0-3600, mV)
				"%04d%c"			// ADC1
				"%04d%c"			// ADC2
				"%04d%c"			// 立ち上がりモード
				"%04d%c"			// DIのビットマップ
				"%c%c",			// 押しボタンフラグ
				sRxPktInfo.u32addr_1st & 0x0FFFFFFF, SEPARATER(),
				DECODE_VOLT(u8batt), SEPARATER(),
				u16adc1, SEPARATER(),
				u16adc2, SEPARATER(),
				0, SEPARATER(),
				0, SEPARATER(),
				'W', SEPARATER()
		);

		uint8 u8Sensor, i;
		for( i=0; i<u8SnsNum; i++ ){
			u8Sensor = G_OCTET();
			switch( u8Sensor){
				case PKT_ID_SHT21:
				{
					int16 i16temp = G_BE_WORD();
					int16 i16humd = G_BE_WORD();
					A_PRINTF("S%c%04d%c%04d%c", SEPARATER(), i16temp, SEPARATER(), i16humd, SEPARATER() );
				}
				break;
				case PKT_ID_SHT31:
				{
					int16 i16temp = G_BE_WORD();
					int16 i16humd = G_BE_WORD();
					A_PRINTF("H%c%04d%c%04d%c", SEPARATER(), i16temp, SEPARATER(), i16humd, SEPARATER() );
				}
				break;
				case PKT_ID_SHTC3:
				{
					int16 i16temp = G_BE_WORD();
					int16 i16humd = G_BE_WORD();
					A_PRINTF("H%c%04d%c%04d%c", SEPARATER(), i16temp, SEPARATER(), i16humd, SEPARATER() );
				}
				break;
				case PKT_ID_ADT7410:
				{
					int16 i16temp = G_BE_WORD();
					A_PRINTF("A%c%04d%c", SEPARATER(), i16temp, SEPARATER() );
				}
				break;
				case PKT_ID_MPL115A2:
				{
					int16 i16atmo = G_BE_WORD();
					A_PRINTF("M%c%04d%c", SEPARATER(), i16atmo, SEPARATER() );
				}
				break;
				case PKT_ID_LIS3DH:
				{
					int16 i16x = G_BE_WORD();
					int16 i16y = G_BE_WORD();
					int16 i16z = G_BE_WORD();
					A_PRINTF("I%c%04d%c%04d%c%04d%c", SEPARATER(), i16x, SEPARATER(), i16y, SEPARATER(), i16z, SEPARATER() );
				}
				break;
				case PKT_ID_ADXL345:
				{
					uint8 u8mode = G_OCTET();
					A_PRINTF( "X%c", SEPARATER() );

					CONV_STR();
					A_PRINTF( "%02X%c", u8mode, SEPARATER() );
					int16 i16x, i16y, i16z;
					if( u8mode == 0xfa ){
						uint8 u8num = G_OCTET();
						uint8 j;
						for( j=0; j<u8num; j++ ){
							i16x = G_BE_WORD();
							i16y = G_BE_WORD();
							i16z = G_BE_WORD();
							A_PRINTF("%04d%c%04d%c%04d%c", i16x, SEPARATER(), i16y, SEPARATER(), i16z, SEPARATER() );
						}
					}else{
						i16x = G_BE_WORD();
						i16y = G_BE_WORD();
						i16z = G_BE_WORD();
						A_PRINTF("%04d%c%04d%c%04d%c", i16x, SEPARATER(), i16y, SEPARATER(), i16z, SEPARATER() );
					}
				}
				break;
				case PKT_ID_L3GD20:
				{
					int16 i16x = G_BE_WORD();
					int16 i16y = G_BE_WORD();
					int16 i16z = G_BE_WORD();
					A_PRINTF("G%c%04d%c%04d%c%04d%c", SEPARATER(), i16x, SEPARATER(), i16y, SEPARATER(), i16z, SEPARATER() );
				}
				break;
				case PKT_ID_TSL2561:
				{
					uint32	u32lux = G_BE_DWORD();
					A_PRINTF("T%c%04d%c", SEPARATER(), u32lux, SEPARATER() );
				}
				break;
				case PKT_ID_S1105902:
				{
					int16 u16R = G_BE_WORD();
					int16 u16G = G_BE_WORD();
					int16 u16B = G_BE_WORD();
					int16 u16I = G_BE_WORD();
					A_PRINTF("C%c%04d%c%04d%c%04d%c%04d%c", SEPARATER(), u16R, SEPARATER(), u16G, SEPARATER(), u16B, SEPARATER(), u16I, SEPARATER() );
				}
				break;
				case PKT_ID_BME280:
				{
					int16	i16temp = G_BE_WORD();
					uint16	u16hum = G_BE_WORD();
					uint16	u16atmo = G_BE_WORD();
					A_PRINTF("B%c%04d%c%04d%c%04d%c", SEPARATER(), i16temp, SEPARATER(), u16hum, SEPARATER(), u16atmo, SEPARATER() );
				}
				break;
				default:
					break;
			}
		}
		A_PRINTF(LB);
	}
	//	押しボタン
	if ( sRxPktInfo.u8pkt == PKT_ID_BUTTON ) {
		uint8 u8batt = G_OCTET();
		uint16 u16adc1 = G_BE_WORD();
		uint16 u16adc2 = G_BE_WORD();
		uint8 u8mode = G_OCTET();
		uint8 u8Bitmap = G_OCTET();

		bool_t bRegularTransmit = ((u8mode&0x80)>>7);

		if( !bRegularTransmit ){
			if( (u8mode&0x04)  || (u8mode&0x02) ){
				if(u8Bitmap){
					vPortSetLo(PORT_OUT1);
					sAppData.u8DO_State = 1;
					if(IS_APPCONF_OPT_DIO_AUTO_HI()){
						sAppData.u32LedCt = 250;
					}
				}else{
					vPortSetHi(PORT_OUT1);
					sAppData.u8DO_State = 0;
					sAppData.u32LedCt = 0;
				}
			}else{
				if( IS_APPCONF_OPT_DIO_AUTO_HI() ){
					// Turn on LED
					sAppData.u32LedCt = 250;
					sAppData.u8DO_State = 1;
				}else{
					vLED_Toggle();
				}
			}
		}

		A_PRINTF( "%c"
				"%d%c",			// TIME STAMP
				SEPARATER(),
				u32TickCount_ms / 1000,  SEPARATER()
		);

		CONV_STR();
		A_PRINTF( "%08X%c"			// 受信機のアドレス
				"%03d%c"			// LQI  (0-255)
				"%03d%c",			// 連番
				sRxPktInfo.u32addr_rcvr & 0x0FFFFFFF, SEPARATER(),
				sRxPktInfo.u8lqi_1st, SEPARATER(),
				sRxPktInfo.u16fct, SEPARATER()
		);

		CONV_STR();
		A_PRINTF( "%07x%c"			// シリアル番号
				"%04d%c"			// 電源電圧 (0-3600, mV)
				"%04d%c"			// ADC1
				"%04d%c"			// ADC2
				"%04d%c"			// 立ち上がりモード
				"%04d%c"			// DIのビットマップ
				"%c%c"				// 押しボタンフラグ
				"%04d%c"			// ボタンの状態(LEDが光れば1、消えれば0)
				LB,
				sRxPktInfo.u32addr_1st & 0x0FFFFFFF, SEPARATER(),
				DECODE_VOLT(u8batt), SEPARATER(),
				u16adc1, SEPARATER(),
				u16adc2, SEPARATER(),
				u8mode, SEPARATER(),
				u8Bitmap, SEPARATER(),
				'P', SEPARATER(),
				sAppData.u8DO_State, SEPARATER()
		);

#ifdef USE_LCD
		// LCD への出力
		V_PRINTF_LCD("%03d:%08X:%03d:%02X:P:%04d:%04d:\n",
				u32sec % 1000,
				sRxPktInfo.u32addr_1st,
				sRxPktInfo.u8lqi_1st,
				sRxPktInfo.u16fct & 0xFF,
//				u8btn
				sAppData.u8DO_State
				);
		vLcdRefresh();
#endif
	}

	if (sRxPktInfo.u8pkt == PKT_ID_SHT21) {
		uint8 u8batt = G_OCTET();
		uint16 u16adc1 = G_BE_WORD();
		uint16 u16adc2 = G_BE_WORD(); (void)u16adc2;
		int16 i16temp = G_BE_WORD();
		int16 i16humd = G_BE_WORD();

		A_PRINTF( "%c"
				"%d%c",			// TIME STAMP
				SEPARATER(),
				u32TickCount_ms / 1000, SEPARATER()
		);

		CONV_STR();
		A_PRINTF( "%08X%c"			// 受信機のアドレス
				"%03d%c"			// LQI  (0-255)
				"%03d%c",			// 連番
				sRxPktInfo.u32addr_rcvr & 0x0FFFFFFF, SEPARATER(),
				sRxPktInfo.u8lqi_1st, SEPARATER(),
				sRxPktInfo.u16fct, SEPARATER()
		);

		CONV_STR();
		A_PRINTF( "%07x%c"			// シリアル番号
				"%04d%c"			// 電源電圧 (0-3600, mV)
				"%04d%c"			// SHT21 TEMP
				"%04d%c"			// SHT21 HUMID
				"%04d%c"			// adc1
				"%04d%c"			// adc2
				"%c%c"			// パケット識別子
				LB,
				sRxPktInfo.u32addr_1st & 0x0FFFFFFF, SEPARATER(),
				DECODE_VOLT(u8batt), SEPARATER(),
				i16temp, SEPARATER(),
				i16humd, SEPARATER(),
				u16adc1, SEPARATER(),
				u16adc2, SEPARATER(),
				'S', SEPARATER()
		);

#ifdef USE_LCD
		// LCD への出力
		V_PRINTF_LCD("%03d:%08X:%03d:%02X:S:%04d:%04d\n",
				u32sec % 1000,
				sRxPktInfo.u32addr_1st,
				sRxPktInfo.u8lqi_1st,
				sRxPktInfo.u16fct & 0xFF,
				i16temp,
				i16humd
				);
		vLcdRefresh();
#endif
	}

	if (sRxPktInfo.u8pkt == PKT_ID_SHT31) {
		uint8 u8batt = G_OCTET();
		uint16 u16adc1 = G_BE_WORD();
		uint16 u16adc2 = G_BE_WORD(); (void)u16adc2;
		int16 i16temp = G_BE_WORD();
		int16 i16humd = G_BE_WORD();

		A_PRINTF( "%c"
				"%d%c",			// TIME STAMP
				SEPARATER(),
				u32TickCount_ms / 1000, SEPARATER()
		);

		CONV_STR();
		A_PRINTF( "%08X%c"			// 受信機のアドレス
				"%03d%c"			// LQI  (0-255)
				"%03d%c",			// 連番
				sRxPktInfo.u32addr_rcvr & 0x0FFFFFFF, SEPARATER(),
				sRxPktInfo.u8lqi_1st, SEPARATER(),
				sRxPktInfo.u16fct, SEPARATER()
		);

		CONV_STR();
		A_PRINTF( "%07x%c"			// シリアル番号
				"%04d%c"			// 電源電圧 (0-3600, mV)
				"%04d%c"			// SHT21 TEMP
				"%04d%c"			// SHT21 HUMID
				"%04d%c"			// adc1
				"%04d%c"			// adc2
				"%c%c"			// パケット識別子
				LB,
				sRxPktInfo.u32addr_1st & 0x0FFFFFFF, SEPARATER(),
				DECODE_VOLT(u8batt), SEPARATER(),
				i16temp, SEPARATER(),
				i16humd, SEPARATER(),
				u16adc1, SEPARATER(),
				u16adc2, SEPARATER(),
				'H', SEPARATER()
		);
	}

	if (sRxPktInfo.u8pkt == PKT_ID_SHTC3) {
		uint8 u8batt = G_OCTET();
		uint16 u16adc1 = G_BE_WORD();
		uint16 u16adc2 = G_BE_WORD(); (void)u16adc2;
		int16 i16temp = G_BE_WORD();
		int16 i16humd = G_BE_WORD();

		A_PRINTF( "%c"
				"%d%c",			// TIME STAMP
				SEPARATER(),
				u32TickCount_ms / 1000, SEPARATER()
		);

		CONV_STR();
		A_PRINTF( "%08X%c"			// 受信機のアドレス
				"%03d%c"			// LQI  (0-255)
				"%03d%c",			// 連番
				sRxPktInfo.u32addr_rcvr & 0x0FFFFFFF, SEPARATER(),
				sRxPktInfo.u8lqi_1st, SEPARATER(),
				sRxPktInfo.u16fct, SEPARATER()
		);

		CONV_STR();
		A_PRINTF( "%07x%c"			// シリアル番号
				"%04d%c"			// 電源電圧 (0-3600, mV)
				"%04d%c"			// SHT21 TEMP
				"%04d%c"			// SHT21 HUMID
				"%04d%c"			// adc1
				"%04d%c"			// adc2
				"%c%c"			// パケット識別子
				LB,
				sRxPktInfo.u32addr_1st & 0x0FFFFFFF, SEPARATER(),
				DECODE_VOLT(u8batt), SEPARATER(),
				i16temp, SEPARATER(),
				i16humd, SEPARATER(),
				u16adc1, SEPARATER(),
				u16adc2, SEPARATER(),
				'Y', SEPARATER()
		);
	}

	if (sRxPktInfo.u8pkt == PKT_ID_MAX31855) {
		uint8 u8batt = G_OCTET();
		uint16 u16adc1 = G_BE_WORD();
		uint16 u16adc2 = G_BE_WORD(); (void)u16adc2;
		int32 i32temp = G_BE_DWORD();
		int32 i32itemp = G_BE_DWORD();

		A_PRINTF( "%c"
				"%d%c",			// TIME STAMP
				SEPARATER(),
				u32TickCount_ms / 1000, SEPARATER()
		);

		CONV_STR();
		A_PRINTF( "%08X%c"			// 受信機のアドレス
				"%03d%c"			// LQI  (0-255)
				"%03d%c",			// 連番
				sRxPktInfo.u32addr_rcvr & 0x0FFFFFFF, SEPARATER(),
				sRxPktInfo.u8lqi_1st, SEPARATER(),
				sRxPktInfo.u16fct, SEPARATER()
		);

		CONV_STR();
		A_PRINTF( "%07x%c"			// シリアル番号
				"%04d%c"			// 電源電圧 (0-3600, mV)
				"%04d%c"			// SHT21 TEMP
				"%04d%c"			// SHT21 HUMID
				"%04d%c"			// adc1
				"%04d%c"			// adc2
				"%c%c"			// パケット識別子
				LB,
				sRxPktInfo.u32addr_1st & 0x0FFFFFFF, SEPARATER(),
				DECODE_VOLT(u8batt), SEPARATER(),
				i32temp, SEPARATER(),
				i32itemp, SEPARATER(),
				u16adc1, SEPARATER(),
				u16adc2, SEPARATER(),
				'N', SEPARATER()
		);

#ifdef USE_LCD
		// LCD への出力
		V_PRINTF_LCD("%03d:%08X:%03d:%02X:S:%04d:%04d\n",
				u32sec % 1000,
				sRxPktInfo.u32addr_1st,
				sRxPktInfo.u8lqi_1st,
				sRxPktInfo.u16fct & 0xFF,
				i32temp,
				i32itemp
				);
		vLcdRefresh();
#endif
	}

	if (sRxPktInfo.u8pkt == PKT_ID_S1105902) {
		uint8 u8batt = G_OCTET();
		uint16 u16adc1 = G_BE_WORD();
		uint16 u16adc2 = G_BE_WORD(); (void)u16adc2;
		uint16 u16R = G_BE_WORD();
		uint16 u16G = G_BE_WORD();
		uint16 u16B = G_BE_WORD();
		uint16 u16I = G_BE_WORD();

		A_PRINTF( "%c"
				"%d%c",			// TIME STAMP
				SEPARATER(),
				u32TickCount_ms / 1000, SEPARATER()
		);

		CONV_STR();
		A_PRINTF( "%08X%c"			// 受信機のアドレス
				"%03d%c"			// LQI  (0-255)
				"%03d%c",			// 連番
				sRxPktInfo.u32addr_rcvr & 0x0FFFFFFF, SEPARATER(),
				sRxPktInfo.u8lqi_1st, SEPARATER(),
				sRxPktInfo.u16fct, SEPARATER()
		);

		CONV_STR();
		A_PRINTF( "%07x%c"			// シリアル番号
				"%04d%c"			// 電源電圧 (0-3600, mV)
				"%04d%c"			// SHT21 TEMP
				"%04d%c"			// SHT21 HUMID
				"%04d%c"			// adc1
				"%04d%c"			// adc2
				"%c%c"				// パケット識別子
				"%04d%c"			// Red
				"%04d%c"			// Green
				"%04d%c"			// Blue
				"%04d%c"			// Infrared
				LB,
				sRxPktInfo.u32addr_1st & 0x0FFFFFFF, SEPARATER(),
				DECODE_VOLT(u8batt), SEPARATER(),
				0, SEPARATER(),
				0, SEPARATER(),
				u16adc1, SEPARATER(),
				u16adc2, SEPARATER(),
				'C', SEPARATER(),
				u16R, SEPARATER(),
				u16G, SEPARATER(),
				u16B, SEPARATER(),
				u16I, SEPARATER()
		);

#ifdef USE_LCD
		// LCD への出力
		V_PRINTF_LCD("%03d:%08X:%03d:%02X:S:%04d:%04d\n",
				u32sec % 1000,
				sRxPktInfo.u32addr_1st,
				sRxPktInfo.u8lqi_1st,
				sRxPktInfo.u16fct & 0xFF,
				i16temp,
				i16humd
				);
		vLcdRefresh();
#endif
	}

	if (sRxPktInfo.u8pkt == PKT_ID_LIS3DH) {
		uint8 u8batt = G_OCTET();
		uint16 u16adc1 = G_BE_WORD();
		uint16 u16adc2 = G_BE_WORD(); (void)u16adc2;
		int16 i16x = G_BE_WORD();
		int16 i16y = G_BE_WORD();
		int16 i16z = G_BE_WORD();
		uint8 u8bitmap = G_OCTET();

		A_PRINTF("%c%d%c", SEPARATER(), u32TickCount_ms / 1000, SEPARATER() );

		CONV_STR();
		A_PRINTF(
			"%08X%c"			// 受信機のアドレス
			"%03d%c"			// LQI  (0-255)
			"%03d%c"			// 連番
			,
			sRxPktInfo.u32addr_rcvr & 0x0FFFFFFF, SEPARATER(),
			sRxPktInfo.u8lqi_1st, SEPARATER(),
			sRxPktInfo.u16fct, SEPARATER()
		);

		CONV_STR();
		A_PRINTF(
			"%07X%c"			// シリアル番号
			"%04d%c"			// 電源電圧 (0-3600, mV)
			,
			sRxPktInfo.u32addr_1st & 0x0FFFFFFF, SEPARATER(),
			DECODE_VOLT(u8batt), SEPARATER()
		);

		CONV_STR();
		A_PRINTF(
			"%04X%c"			// モード
			"%04X%c"			//
			"%04d%c"			// adc1
			"%04d%c"			// adc2
			"%c%c"				// パケット識別子
			"%04d%c"			// x
			"%04d%c"			// y
			"%04d%c"			// z
			LB,
			u8bitmap, SEPARATER(),
			0, SEPARATER(),
			u16adc1, SEPARATER(),
			u16adc2, SEPARATER(),
			'I', SEPARATER(),
			i16x, SEPARATER(),
			i16y, SEPARATER(),
			i16z, SEPARATER()
		);

#ifdef USE_LCD
		// LCD への出力
		V_PRINTF_LCD("%03d:%08X:%03d:%02X:I:%04d:%04d:%04d\n",
				u32sec % 1000,
				sRxPktInfo.u32addr_1st,
				sRxPktInfo.u8lqi_1st,
				sRxPktInfo.u16fct & 0xFF,
				i16x,
				i16y,
				i16z
				);
		vLcdRefresh();
#endif
	}

	if (sRxPktInfo.u8pkt == PKT_ID_L3GD20) {
		uint8 u8batt = G_OCTET();
		uint16 u16adc1 = G_BE_WORD();
		uint16 u16adc2 = G_BE_WORD(); (void)u16adc2;
		int16 i16x = G_BE_WORD();
		int16 i16y = G_BE_WORD();
		int16 i16z = G_BE_WORD();
		uint8 u8bitmap = G_OCTET();

		A_PRINTF("%c%d%c", SEPARATER(), u32TickCount_ms / 1000, SEPARATER() );

		CONV_STR();
		A_PRINTF(
			"%08X%c"			// 受信機のアドレス
			"%03d%c"			// LQI  (0-255)
			"%03d%c"			// 連番
			,
			sRxPktInfo.u32addr_rcvr & 0x0FFFFFFF, SEPARATER(),
			sRxPktInfo.u8lqi_1st, SEPARATER(),
			sRxPktInfo.u16fct, SEPARATER()
		);

		CONV_STR();
		A_PRINTF(
			"%07X%c"			// シリアル番号
			"%04d%c"			// 電源電圧 (0-3600, mV)
			,
			sRxPktInfo.u32addr_1st & 0x0FFFFFFF, SEPARATER(),
			DECODE_VOLT(u8batt), SEPARATER()
		);

		CONV_STR();
		A_PRINTF(
			"%04X%c"			// モード
			"%04X%c"			//
			"%04d%c"			// adc1
			"%04d%c"			// adc2
			"%c%c"				// パケット識別子
			"%04d%c"			// x
			"%04d%c"			// y
			"%04d%c"			// z
			LB,
			u8bitmap, SEPARATER(),
			0, SEPARATER(),
			u16adc1, SEPARATER(),
			u16adc2, SEPARATER(),
			'G', SEPARATER(),
			i16x, SEPARATER(),
			i16y, SEPARATER(),
			i16z, SEPARATER()
		);

#ifdef USE_LCD
		// LCD への出力
		V_PRINTF_LCD("%03d:%08X:%03d:%02X:I:%04d:%04d:%04d\n",
				u32sec % 1000,
				sRxPktInfo.u32addr_1st,
				sRxPktInfo.u8lqi_1st,
				sRxPktInfo.u16fct & 0xFF,
				i16x,
				i16y,
				i16z
				);
		vLcdRefresh();
#endif
	}


	if (sRxPktInfo.u8pkt == PKT_ID_ADXL345) {
		uint8 u8batt = G_OCTET();
		uint16 u16adc1 = G_BE_WORD();
		uint16 u16adc2 = G_BE_WORD(); (void)u16adc2;
		int16 i16x = G_BE_WORD();
		int16 i16y = G_BE_WORD();
		int16 i16z = G_BE_WORD();
		uint8 u8mode = G_OCTET();

		uint8 u8ActTapSource = ( u16adc1>>12 )|((u16adc2>>8)&0xF0);

		u16adc1 = u16adc1&0x0FFF;
		u16adc2 = u16adc2&0x0FFF;

		if( u8mode == 0xFA ){
			A_PRINTF( "%c%d.%02d%c", SEPARATER(), u32TickCount_ms / 1000, (u32TickCount_ms % 1000)/10, SEPARATER() );
		}else{
			A_PRINTF("%c%d%c", SEPARATER(), u32TickCount_ms / 1000, SEPARATER() );
		}

		CONV_STR();
		A_PRINTF(
			"%08X%c"			// 受信機のアドレス
			"%03d%c"			// LQI  (0-255)
			"%03d%c"			// 連番
			,
			sRxPktInfo.u32addr_rcvr & 0x0FFFFFFF, SEPARATER(),
			sRxPktInfo.u8lqi_1st, SEPARATER(),
			sRxPktInfo.u16fct, SEPARATER()
		);

		CONV_STR();
		A_PRINTF(
			"%07X%c"			// シリアル番号
			"%04d%c"			// 電源電圧 (0-3600, mV)
			,
			sRxPktInfo.u32addr_1st & 0x0FFFFFFF, SEPARATER(),
			DECODE_VOLT(u8batt), SEPARATER()
		);

		CONV_STR();
		A_PRINTF( "%04X%c",			// モード
				u8mode, SEPARATER()
    	);

		CONV_STR();
		A_PRINTF(
			"%04X%c"			//
			"%04d%c"			// adc1
			"%04d%c"			// adc2
			"%c%c"				// パケット識別子
			"%04d%c"			// x
			"%04d%c"			// y
			"%04d%c"			// z
			,
			u8ActTapSource, SEPARATER(),
			u16adc1, SEPARATER(),
			u16adc2, SEPARATER(),
			'X', SEPARATER(),
			i16x, SEPARATER(),
			i16y, SEPARATER(),
			i16z, SEPARATER()
		);

		if(u8mode == 0xFA){
			A_PRINTF( LB );
			uint8 u8num = G_OCTET();
			uint8 i;
			for( i=0; i<u8num-1; i++ ){
				i16x = G_BE_WORD();
				i16y = G_BE_WORD();
				i16z = G_BE_WORD();
				A_PRINTF( "%c%c%c%c%c%c%c%c%c%c%c%c%04d%c%04d%c%04d%c"LB, SEPARATER(), SEPARATER(), SEPARATER(), SEPARATER(), SEPARATER(), SEPARATER(), SEPARATER(), SEPARATER(), SEPARATER(), SEPARATER(), SEPARATER(), SEPARATER(), i16x, SEPARATER(), i16y, SEPARATER(), i16z, SEPARATER() );
			}
		}else if(u8mode == 0xF9){
			uint16 u16Sample = G_BE_WORD();
			i16x = G_BE_WORD();
			i16y = G_BE_WORD();
			i16z = G_BE_WORD();
			A_PRINTF( "%04d%c%04d%c%04d%c", i16x, SEPARATER(), i16y, SEPARATER(), i16z, SEPARATER() );
			i16x = G_BE_WORD();
			i16y = G_BE_WORD();
			i16z = G_BE_WORD();
			A_PRINTF( "%04d%c%04d%c%04d%c%d%c"LB, i16x, SEPARATER(), i16y, SEPARATER(), i16z, SEPARATER(), u16Sample, SEPARATER() );

		}else{
			A_PRINTF( LB );
		}

#ifdef USE_LCD
		// LCD への出力
		V_PRINTF_LCD("%03d:%08X:%03d:%02X:I:%04d:%04d:%04d\n",
				u32sec % 1000,
				sRxPktInfo.u32addr_1st,
				sRxPktInfo.u8lqi_1st,
				sRxPktInfo.u16fct & 0xFF,
				i16x,
				i16y,
				i16z
				);
		vLcdRefresh();
#endif
	}


	if (sRxPktInfo.u8pkt == PKT_ID_ADT7410) {
		uint8 u8batt = G_OCTET();
		uint16 u16adc1 = G_BE_WORD();
		uint16 u16adc2 = G_BE_WORD(); (void)u16adc2;
		int16 i16temp = G_BE_WORD();

		A_PRINTF( "%c"
				"%d%c",			// TIME STAMP
				SEPARATER(),
				u32TickCount_ms / 1000, SEPARATER()
		);

		CONV_STR();
		A_PRINTF( "%08X%c"			// 受信機のアドレス
				"%03d%c"			// LQI  (0-255)
				"%03d%c",			// 連番
				sRxPktInfo.u32addr_rcvr & 0x0FFFFFFF, SEPARATER(),
				sRxPktInfo.u8lqi_1st, SEPARATER(),
				sRxPktInfo.u16fct, SEPARATER()
		);

		CONV_STR();
		A_PRINTF( "%07x%c"			// シリアル番号
				"%04d%c"			// 電源電圧 (0-3600, mV)
				"%04d%c"			// TEMP
				"%04d%c"			// 
				"%04d%c"			// adc1
				"%04d%c"			// adc2
				"%c%c"			// パケット識別子
				LB,
				sRxPktInfo.u32addr_1st & 0x0FFFFFFF, SEPARATER(),
				DECODE_VOLT(u8batt), SEPARATER(),
				i16temp, SEPARATER(),
				0, SEPARATER(),
				u16adc1, SEPARATER(),
				u16adc2, SEPARATER(),
				'D', SEPARATER()
		);

#ifdef USE_LCD
		// LCD への出力
		V_PRINTF_LCD("%03d:%08X:%03d:%02X:S:%04d:%04d\n",
				u32sec % 1000,
				sRxPktInfo.u32addr_1st,
				sRxPktInfo.u8lqi_1st,
				sRxPktInfo.u16fct & 0xFF,
				i16temp,
				i16humd
				);
		vLcdRefresh();
#endif
	}

	if (sRxPktInfo.u8pkt == PKT_ID_TSL2561) {
		uint8 u8batt = G_OCTET();
		uint16 u16adc1 = G_BE_WORD();
		uint16 u16adc2 = G_BE_WORD(); (void)u16adc2;
		uint32 u32lux = G_BE_DWORD();

		A_PRINTF( "%c"
				"%d%c",			// TIME STAMP
				SEPARATER(),
				u32TickCount_ms / 1000, SEPARATER()
		);

		CONV_STR();
		A_PRINTF( "%08X%c"			// 受信機のアドレス
				"%03d%c"			// LQI  (0-255)
				"%03d%c",			// 連番
				sRxPktInfo.u32addr_rcvr & 0x0FFFFFFF, SEPARATER(),
				sRxPktInfo.u8lqi_1st, SEPARATER(),
				sRxPktInfo.u16fct, SEPARATER()
		);

		CONV_STR();
		A_PRINTF( "%07x%c"			// シリアル番号
				"%04d%c"			// 電源電圧 (0-3600, mV)
				"%04d%c"			// lux
				"%04d%c"			// 
				"%04d%c"			// adc1
				"%04d%c"			// adc2
				"%c%c"			// パケット識別子
				LB,
				sRxPktInfo.u32addr_1st & 0x0FFFFFFF, SEPARATER(),
				DECODE_VOLT(u8batt), SEPARATER(),
				u32lux, SEPARATER(),
				0, SEPARATER(),
				u16adc1, SEPARATER(),
				u16adc2, SEPARATER(),
				'T', SEPARATER()
		);
	}

	if (sRxPktInfo.u8pkt == PKT_ID_MPL115A2) {
		uint8 u8batt = G_OCTET();
		uint16 u16adc1 = G_BE_WORD();
		uint16 u16adc2 = G_BE_WORD();;
		int16 i16atmo = G_BE_WORD();

		A_PRINTF( "%c"
				"%d%c",			// TIME STAMP
				SEPARATER(),
				u32TickCount_ms / 1000, SEPARATER()
		);

		CONV_STR();
		A_PRINTF( "%08X%c"			// 受信機のアドレス
				"%03d%c"			// LQI  (0-255)
				"%03d%c",			// 連番
				sRxPktInfo.u32addr_rcvr & 0x0FFFFFFF, SEPARATER(),
				sRxPktInfo.u8lqi_1st, SEPARATER(),
				sRxPktInfo.u16fct, SEPARATER()
		);

		CONV_STR();
		A_PRINTF( "%07x%c"			// シリアル番号
				"%04d%c"			// 電源電圧 (0-3600, mV)
				"%04d%c"			// SHT21 TEMP
				"%04d%c"			// SHT21 HUMID
				"%04d%c"			// adc1
				"%04d%c"			// adc2
				"%c%c"			// パケット識別子
				LB,
				sRxPktInfo.u32addr_1st & 0x0FFFFFFF, SEPARATER(),
				DECODE_VOLT(u8batt), SEPARATER(),
				i16atmo, SEPARATER(),
				0, SEPARATER(),
				u16adc1, SEPARATER(),
				u16adc2, SEPARATER(),
				'M', SEPARATER()
		);

#ifdef USE_LCD
		// LCD への出力
		V_PRINTF_LCD("%03d:%08X:%03d:%02X:M:%04d\n",
				u32sec % 1000,
				sRxPktInfo.u32addr_1st,
				sRxPktInfo.u8lqi_1st,
				sRxPktInfo.u16fct & 0xFF,
				i16atmo
				);
		vLcdRefresh();
#endif
	}

	if (sRxPktInfo.u8pkt == PKT_ID_BME280) {
		uint8 u8batt = G_OCTET();
		uint16 u16adc1 = G_BE_WORD();
		uint16 u16adc2 = G_BE_WORD();;
		int16 i16temp = G_BE_WORD();
		uint16 u16hum = G_BE_WORD();
		uint16 u16atmo = G_BE_WORD();

		A_PRINTF( "%c"
				"%d%c",			// TIME STAMP
				SEPARATER(),
				u32TickCount_ms / 1000, SEPARATER()
		);

		CONV_STR();
		A_PRINTF( "%08X%c"			// 受信機のアドレス
				"%03d%c"			// LQI  (0-255)
				"%03d%c",			// 連番
				sRxPktInfo.u32addr_rcvr & 0x0FFFFFFF, SEPARATER(),
				sRxPktInfo.u8lqi_1st, SEPARATER(),
				sRxPktInfo.u16fct, SEPARATER()
		);

		CONV_STR();
		A_PRINTF( "%07x%c"			// シリアル番号
				"%04d%c"			// 電源電圧 (0-3600, mV)
				"%04d%c"			// TEMP
				"%04d%c"			// HUMID
				"%04d%c"			// adc1
				"%04d%c"			// adc2
				"%c%c"				// パケット識別子
				"%04d%c"			// atomo
				LB,
				sRxPktInfo.u32addr_1st & 0x0FFFFFFF, SEPARATER(),
				DECODE_VOLT(u8batt), SEPARATER(),
				i16temp, SEPARATER(),
				u16hum, SEPARATER(),
				u16adc1, SEPARATER(),
				u16adc2, SEPARATER(),
				'B', SEPARATER(),
				u16atmo, SEPARATER()
		);

#ifdef USE_LCD
			// LCD への出力
			V_PRINTF_LCD("%03d:%08X:%03d:%02X:B:%04d:%04d:%04d:%04d:%04d\n",
					u32sec % 1000,
					sRxPktInfo.u32addr_1st,
					sRxPktInfo.u8lqi_1st,
					sRxPktInfo.u16fct & 0xFF,
					i16temp,
					u16hum,
					u16atmo,
					u16adc1
					);
			vLcdRefresh();
#endif
	}

	if (sRxPktInfo.u8pkt == PKT_ID_STANDARD) {
		uint8 u8batt = G_OCTET();

		uint16 u16adc1 = G_BE_WORD();
		uint16 u16adc2 = G_BE_WORD();
		uint16 u16pc1 = G_BE_WORD(); (void)u16pc1;
		uint16 u16pc2 = G_BE_WORD(); (void)u16pc2;

		A_PRINTF( "%c"
				"%d%c",			// TIME STAMP
				SEPARATER(),
				u32TickCount_ms / 1000, SEPARATER()
		);

		CONV_STR();
		A_PRINTF( "%08X%c"			// 受信機のアドレス
				"%03d%c"			// LQI  (0-255)
				"%03d%c",			// 連番
				sRxPktInfo.u32addr_rcvr & 0x0FFFFFFF, SEPARATER(),
				sRxPktInfo.u8lqi_1st, SEPARATER(),
				sRxPktInfo.u16fct, SEPARATER()
		);

		CONV_STR();
		A_PRINTF( "%07x%c"			// シリアル番号
				"%04d%c"			// 電源電圧 (0-3600, mV)
				,
				sRxPktInfo.u32addr_1st & 0x0FFFFFFF, SEPARATER(),
				DECODE_VOLT(u8batt), SEPARATER()
		);


		if(u16adc1&0x8000){
			A_PRINTF( "%04d%c"		// AI1
				"%04d%c"			// AI2
				"%04d%c"			// AI3
				"%04d%c"			// AI4
				"%c%c"			// パケット識別子
				LB,
				u16adc1, SEPARATER(),
				u16adc2, SEPARATER(),
				u16pc1, SEPARATER(),
				u16pc2, SEPARATER(),
				'A', SEPARATER()
			);
		}else{
			A_PRINTF( "%04d%c"			// ADC2
				"%04d%c"			// SuperCAP 電圧(mV)
				"%04d%c"			// adc1
				"%04d%c"			// adc2
				"%c%c"			// パケット識別子
				LB,
				u16adc2, SEPARATER(),
				u16adc1*2*3, SEPARATER(),
				u16adc1, SEPARATER(),
				u16adc2, SEPARATER(),
				'A', SEPARATER()
			);
		}

#ifdef USE_LCD
		// LCD への出力
		V_PRINTF_LCD("%03d:%08X:%03d:%02X:S:%04d:%04d\n",
				u32sec % 1000,
				sRxPktInfo.u32addr_1st,
				sRxPktInfo.u8lqi_1st,
				sRxPktInfo.u16fct & 0xFF,
				u16adc2,
				u16adc1 * 2
				);
		vLcdRefresh();
#endif
	}

	if (sRxPktInfo.u8pkt == PKT_ID_LM61) {
		uint8 u8batt = G_OCTET();

		uint16	u16adc1 = G_BE_WORD();
		uint16	u16adc2 = G_BE_WORD();
		uint16	u16pc1 = G_BE_WORD(); (void)u16pc1;
		uint16	u16pc2 = G_BE_WORD(); (void)u16pc2;
		int16	bias = (int16)G_BE_WORD();

		// LM61用の温度変換
		//   Vo=T[℃]x10[mV]+600[mV]
		//   T     = Vo/10-60
		//   100xT = 10xVo-6000
		int32 iTemp = 10 * (int32)u16adc2 - 6000L + bias;

		A_PRINTF( "%c"
				"%d%c",			// TIME STAMP
				SEPARATER(),
				u32TickCount_ms / 1000, SEPARATER()
		);

		CONV_STR();
		A_PRINTF( "%08X%c"			// 受信機のアドレス
				"%03d%c"			// LQI  (0-255)
				"%03d%c",			// 連番
				sRxPktInfo.u32addr_rcvr & 0x0FFFFFFF, SEPARATER(),
				sRxPktInfo.u8lqi_1st, SEPARATER(),
				sRxPktInfo.u16fct, SEPARATER()
		);

		CONV_STR();
		A_PRINTF( "%07x%c"			// シリアル番号
				"%04d%c"			// 電源電圧 (0-3600, mV)
				"%04d%c"			// LM61温度
				"%04d%c"			// SuperCap の蓄電率
				"%04d%c"			// もともとの電圧
				"%04d%c"			// バイアスをかけた電圧
				"%c%c"			// パケット識別子
				LB,
				sRxPktInfo.u32addr_1st & 0x0FFFFFFF, SEPARATER(),
				DECODE_VOLT(u8batt), SEPARATER(),
				iTemp, SEPARATER(),
				u16adc1*2*3, SEPARATER(),
				u16adc1, SEPARATER(),
				u16adc2, SEPARATER(),
				'L', SEPARATER()
		);

#ifdef USE_LCD
		// LCD への出力
		V_PRINTF_LCD("%03d:%08X:%03d:%02X:L:%04d:%04d\n",
				u32sec % 1000,
				sRxPktInfo.u32addr_1st,
				sRxPktInfo.u8lqi_1st,
				sRxPktInfo.u16fct & 0xFF,
				iTemp,
				u16adc1 * 2
				);
		vLcdRefresh();
#endif
	}

	if (sRxPktInfo.u8pkt == PKT_ID_IO_TIMER) {
		uint8 u8stat = G_OCTET();
		uint32 u32dur = G_BE_DWORD();

		A_PRINTF( "%c"
				"%d%c",			// TIME STAMP
				SEPARATER(),
				u32TickCount_ms / 1000, SEPARATER()
		);

		CONV_STR();
		A_PRINTF( "%08X%c"			// 受信機のアドレス
				"%03d%c"			// LQI  (0-255)
				"%03d%c",			// 連番
				sRxPktInfo.u32addr_rcvr & 0x0FFFFFFF, SEPARATER(),
				sRxPktInfo.u8lqi_1st, SEPARATER(),
				sRxPktInfo.u16fct, SEPARATER()
		);

		CONV_STR();
		A_PRINTF( "%07x%c"			// シリアル番号
				"%s%c"			// 電源電圧 (0-3600, mV)
				"%s%c"			// SHT21 TEMP
				"%s%c"			// SHT21 HUMID
				"%s%c"			// adc1
				"%s%c"			// adc2
				"%c%c"			// パケット識別子
				"%04d%c"
				"%04d%c"
				LB,
				sRxPktInfo.u32addr_1st & 0x0FFFFFFF, SEPARATER(),
				"", SEPARATER(),
				"", SEPARATER(),
				"", SEPARATER(),
				"", SEPARATER(),
				"", SEPARATER(),
				'B', SEPARATER(),
				u8stat, SEPARATER(),
				u32dur, SEPARATER()
		);

#ifdef USE_LCD
		// LCD への出力
		V_PRINTF_LCD("%03d:%08X:%03d:%02X:B:%04d:%04d\n",
				u32sec % 1000,
				sRxPktInfo.u32addr_1st,
				sRxPktInfo.u8lqi_1st,
				sRxPktInfo.u16fct & 0xFF,
				u8stat,
				u32dur / 1000
				);
		vLcdRefresh();
#endif
	}
}

/**
 * UART形式の出力
 *
 * バイナリ形式では以下のような出力が得られる
 * A5 5A       <= ヘッダ
 * 80 12       <= ペイロード長 (XOR チェックサム手前まで)
 * 80 00 00 00 <= 受信機のアドレス (80000000 は親機)
 * 84          <= 最初に受信した受信機のLQI
 * 00 11       <= フレームカウント
 * 81 00 00 38 <= 送信機のアドレス
 * 00          <= 送信機の論理アドレス
 * 04          <= パケット種別 04=UART
 * 04          <= ペイロードのバイト数
 * 81 00 00 38 <= データ部 (たまたま送信機のアドレスをそのまま送信した)
 * 15          <= XOR チェックサム
 * 04          <= 終端
 */
void vSerOutput_Uart(tsRxPktInfo sRxPktInfo, uint8 *p) {
	uint8 u8buff[256], *q = u8buff; // 出力バッファ

	// 受信機のアドレス
	S_BE_DWORD(sRxPktInfo.u32addr_rcvr);

	// LQI
	S_OCTET(sRxPktInfo.u8lqi_1st);

	// フレーム
	S_BE_WORD(sRxPktInfo.u16fct);

	// 送信元子機アドレス
	S_BE_DWORD(sRxPktInfo.u32addr_1st);
	S_OCTET(sRxPktInfo.u8id);

	// パケットの種別により処理を変更
	S_OCTET(sRxPktInfo.u8pkt);

	switch(sRxPktInfo.u8pkt) {
	//	温度センサなど
	case PKT_ID_STANDARD:
	case PKT_ID_LM61:
	case PKT_ID_SHT21:
	case PKT_ID_SHT31:
	case PKT_ID_SHTC3:
		_C {
			uint8	u8batt = G_OCTET();
			uint16	u16adc0 = G_BE_WORD();
			uint16	u16adc1 = G_BE_WORD();
			int16	i16temp = G_BE_WORD();
			uint16	u16humi = G_BE_WORD();

			S_OCTET(u8batt); // batt
			S_BE_WORD(u16adc0);
			S_BE_WORD(u16adc1);
			S_BE_WORD(i16temp);
			S_BE_WORD(u16humi);
		}

		if (sRxPktInfo.u8pkt == PKT_ID_LM61) {
			int16	bias = G_BE_WORD();
			S_BE_WORD( bias );
		}
		break;

	case PKT_ID_MAX31855:
		_C {
			uint8	u8batt = G_OCTET();
			uint16	u16adc0 = G_BE_WORD();
			uint16	u16adc1 = G_BE_WORD();
			int32	i32temp = G_BE_DWORD();
			int32	i32itemp = G_BE_DWORD();

			S_OCTET(u8batt); // batt
			S_BE_WORD(u16adc0);
			S_BE_WORD(u16adc1);
			S_BE_DWORD(i32temp);
			S_BE_DWORD(i32itemp);
		}
		break;

	case PKT_ID_ADT7410:
		_C {
			uint8	u8batt = G_OCTET();
			uint16	u16adc0 = G_BE_WORD();
			uint16	u16adc1 = G_BE_WORD();
			int16	i16temp = G_BE_WORD();

			S_OCTET(u8batt); // batt
			S_BE_WORD(u16adc0);
			S_BE_WORD(u16adc1);
			S_BE_WORD(i16temp);
		}
		break;

	case PKT_ID_BME280:
		_C {
			uint8	u8batt = G_OCTET();
			uint16	u16adc0 = G_BE_WORD();
			uint16	u16adc1 = G_BE_WORD();
			int16	i16temp = G_BE_WORD();
			uint16	u16hum = G_BE_WORD();
			uint16	u16atmo = G_BE_WORD();

			S_OCTET(u8batt);		// batt
			S_BE_WORD(u16adc0);
			S_BE_WORD(u16adc1);
			S_BE_WORD(i16temp);		//	Result
			S_BE_WORD(u16hum);		//	Result
			S_BE_WORD(u16atmo);		//	Result
		}
		break;

	case PKT_ID_MPL115A2:
		_C {
			uint8	u8batt = G_OCTET();
			uint16	u16adc0 = G_BE_WORD();
			uint16	u16adc1 = G_BE_WORD();
			uint16	u16atmo = G_BE_WORD();

			S_OCTET(u8batt);		// batt
			S_BE_WORD(u16adc0);
			S_BE_WORD(u16adc1);
			S_BE_WORD(u16atmo);		//	Result
		}
		break;

	case PKT_ID_LIS3DH:
	case PKT_ID_ADXL345:
	case PKT_ID_L3GD20:
		_C {
			uint8	u8batt = G_OCTET();
			uint16	u16adc0 = G_BE_WORD();
			uint16	u16adc1 = G_BE_WORD();
			int16	i16x = G_BE_WORD();
			int16	i16y = G_BE_WORD();
			int16	i16z = G_BE_WORD();

			uint8 u8ActTapSource = ( u16adc0>>12 )|((u16adc1>>8)&0xF0);(void)u8ActTapSource;

			u16adc0 = u16adc0&0x0FFF;
			u16adc1 = u16adc1&0x0FFF;

			S_OCTET(u8batt); // batt
			S_BE_WORD(u16adc0);
			S_BE_WORD(u16adc1);

			if( sRxPktInfo.u8pkt == PKT_ID_ADXL345 ){
				uint8 u8mode = G_OCTET();
				S_OCTET(u8mode);

				if(u8mode == 0xfa){
					uint8 u8num = G_OCTET();
					S_OCTET(u8num);
					S_BE_WORD(i16x);		//	1回目は先に表示
					S_BE_WORD(i16y);		//
					S_BE_WORD(i16z);		//
					uint8 i;
					for( i=0; i<u8num-1; i++ ){
						i16x = G_BE_WORD();
						i16y = G_BE_WORD();
						i16z = G_BE_WORD();
						S_BE_WORD(i16x);		//	Result
						S_BE_WORD(i16y);		//	Result
						S_BE_WORD(i16z);		//	Result
					}
				}else if(u8mode == 0xF9 ){
					uint16 u16Sample = G_BE_WORD();
					S_BE_WORD(i16x);		//	X min
					S_BE_WORD(i16y);		//	X ave
					S_BE_WORD(i16z);		//	X max
					i16x = G_BE_WORD();
					i16y = G_BE_WORD();
					i16z = G_BE_WORD();
					S_BE_WORD(i16x);		//	Y min
					S_BE_WORD(i16y);		//	Y ave
					S_BE_WORD(i16z);		//	Y max
					i16x = G_BE_WORD();
					i16y = G_BE_WORD();
					i16z = G_BE_WORD();
					S_BE_WORD(i16x);		//	Z min
					S_BE_WORD(i16y);		//	Z ave
					S_BE_WORD(i16z);		//	Z max
					S_BE_WORD(u16Sample);	// 今回使用したサンプル数
				}else{
					S_BE_WORD(i16x);		//	Result
					S_BE_WORD(i16y);		//	Result
					S_BE_WORD(i16z);		//	Result
				}
			}else{
				S_BE_WORD(i16x);		//	Result
				S_BE_WORD(i16y);		//	Result
				S_BE_WORD(i16z);		//	Result
			}
		}
		break;

	case PKT_ID_TSL2561:
		_C {
			uint8 u8batt = G_OCTET();

			uint16	u16adc1 = G_BE_WORD();
			uint16	u16adc2 = G_BE_WORD();
			uint32	u32lux = G_BE_DWORD();

			S_OCTET(u8batt); // batt
			S_BE_WORD(u16adc1);
			S_BE_WORD(u16adc2);
			S_BE_DWORD(u32lux);		//	Result
		}
		break;

	case PKT_ID_S1105902:
		_C {
			uint8 u8batt = G_OCTET();

			uint16 u16adc1 = G_BE_WORD();
			uint16 u16adc2 = G_BE_WORD();
			int16 u16R = G_BE_WORD();
			int16 u16G = G_BE_WORD();
			int16 u16B = G_BE_WORD();
			int16 u16I = G_BE_WORD();

			S_OCTET(u8batt); // batt
			S_BE_WORD(u16adc1);
			S_BE_WORD(u16adc2);
			S_BE_WORD(u16R);		//	Result
			S_BE_WORD(u16G);		//	Result
			S_BE_WORD(u16B);
			S_BE_WORD(u16I);
		}
		break;

	//	磁気スイッチ
	case PKT_ID_IO_TIMER:
		_C {
			uint8	u8batt = G_OCTET();
			uint8	u8stat = G_OCTET();
			uint32	u32dur = G_BE_DWORD();

			S_OCTET(u8batt); // batt
			S_OCTET(u8stat); // stat
			S_BE_DWORD(u32dur); // dur
		}
		break;

	case PKT_ID_UART:
		_C {
			uint8 u8len = G_OCTET();
			S_OCTET(u8len);

			uint8	tmp;
			while (u8len--) {
				tmp = G_OCTET();
				S_OCTET(tmp);
			}
		}
		break;

	//	押しボタン
	case PKT_ID_BUTTON:
		_C {
			uint8	u8batt = G_OCTET();
			uint16	u16adc0 = G_BE_WORD();
			uint16	u16adc1 = G_BE_WORD();
			uint8	u8mode = G_OCTET();
			uint8	u8bitmap = G_OCTET();

			bool_t bRegularTransmit = ((u8mode&0x80)>>7);

			if( !bRegularTransmit ){
				if( (u8mode&0x04) || (u8mode&0x02) ){
					if(u8bitmap){
						vPortSetLo(PORT_OUT1);
						sAppData.u8DO_State = 1;
						if(IS_APPCONF_OPT_DIO_AUTO_HI()){
							sAppData.u32LedCt = 250;
						}
					}else{
						vPortSetHi(PORT_OUT1);
						sAppData.u8DO_State = 0;
						sAppData.u32LedCt = 0;
					}
				}else{
					if( IS_APPCONF_OPT_DIO_AUTO_HI() ){
						// Turn on LED
						sAppData.u32LedCt = 250;
						sAppData.u8DO_State = 1;
					}else{
						vLED_Toggle();
					}
				}
			}

			S_OCTET(u8batt);		// batt
			S_BE_WORD(u16adc0);
			S_BE_WORD(u16adc1);
			S_OCTET( u8mode );
			S_OCTET( u8bitmap );
			S_OCTET( sAppData.u8DO_State );
		}
		break;

	case PKT_ID_MULTISENSOR:
		_C {
			uint8 u8batt = G_OCTET();
			uint16 u16adc1 = G_BE_WORD();
			uint16 u16adc2 = G_BE_WORD();
			uint8 u8SnsNum = G_OCTET();

			S_OCTET(u8batt); // batt
			S_BE_WORD(u16adc1);
			S_BE_WORD(u16adc2);
			S_OCTET(u8SnsNum);

			uint8 u8Sensor, i;
			for( i=0; i<u8SnsNum; i++ ){
				u8Sensor = G_OCTET();
				S_OCTET(u8Sensor); // batt
				switch( u8Sensor){
					case PKT_ID_SHT21:
					case PKT_ID_SHT31:
					case PKT_ID_SHTC3:
					{
						int16 i16temp = G_BE_WORD();
						int16 i16humd = G_BE_WORD();
						S_BE_WORD(i16temp);
						S_BE_WORD(i16humd);
					}
					break;
					case PKT_ID_ADT7410:
					{
						int16 i16temp = G_BE_WORD();
						S_BE_WORD(i16temp);
					}
					break;
					case PKT_ID_MPL115A2:
					{
						int16 i16atmo = G_BE_WORD();
						S_BE_WORD(i16atmo);
					}
					break;
					case PKT_ID_LIS3DH:
					case PKT_ID_L3GD20:
					{
						int16 i16x = G_BE_WORD();
						int16 i16y = G_BE_WORD();
						int16 i16z = G_BE_WORD();
						S_BE_WORD(i16x);
						S_BE_WORD(i16y);
						S_BE_WORD(i16z);
					}
					break;
					case PKT_ID_ADXL345:
					{
						uint8 u8mode = G_OCTET();
						S_OCTET(u8mode);
						int16 i16x, i16y, i16z;
						if( u8mode == 0xfa ){
							uint8 u8num = G_OCTET();
							uint8 j;
							for( j=0; j<u8num; j++ ){
								i16x = G_BE_WORD();
								i16y = G_BE_WORD();
								i16z = G_BE_WORD();
								S_BE_WORD(i16x);
								S_BE_WORD(i16y);
								S_BE_WORD(i16z);
							}
						}else{
							i16x = G_BE_WORD();
							i16y = G_BE_WORD();
							i16z = G_BE_WORD();
							S_BE_WORD(i16x);
							S_BE_WORD(i16y);
							S_BE_WORD(i16z);
						}
					}
					break;
					case PKT_ID_TSL2561:
					{
						uint32	u32lux = G_BE_DWORD();
						S_BE_DWORD(u32lux);
					}
					break;
					case PKT_ID_S1105902:
					{
						int16 u16R = G_BE_WORD();
						int16 u16G = G_BE_WORD();
						int16 u16B = G_BE_WORD();
						int16 u16I = G_BE_WORD();
						S_BE_WORD(u16R);
						S_BE_WORD(u16G);
						S_BE_WORD(u16B);
						S_BE_WORD(u16I);
					}
					break;
					case PKT_ID_BME280:
					{
						int16	i16temp = G_BE_WORD();
						uint16	u16hum = G_BE_WORD();
						uint16	u16atmo = G_BE_WORD();
						S_BE_WORD(i16temp);
						S_BE_WORD(u16hum);
						S_BE_WORD(u16atmo);
					}
					break;
					default:
						break;
				}
			}
		}
		break;
	default:
		break;
	}

	sSerCmdOut.u16len = q - u8buff;
	sSerCmdOut.au8data = u8buff;

	sSerCmdOut.vOutput(&sSerCmdOut, &sSerStream);

	sSerCmdOut.au8data = NULL;
}


void vSerOutput_Secondary() {
	//	オプションビットで設定されていたら表示しない
	if(!IS_APPCONF_OPT_PARENT_OUTPUT()){
		// 出力用の関数を呼び出す
		if (IS_APPCONF_OPT_SmplTag()) {
			A_PRINTF(";%d;"LB, u32sec);
		} else if(IS_APPCONF_OPT_UART_CSV()){
			A_PRINTF(",%d,"LB, u32sec);
		} else if (IS_APPCONF_OPT_UART()) {
			// 無し
		} else {
			A_PRINTF("::ts=%d"LB, u32sec);
		}
	}
}
/**
 * コマンド受け取り時の処理
 * @param pCmd
 */
void vProcessSerialCmd(tsSerCmd_Context *pCmd) {
	V_PRINTF(LB "! cmd len=%d data=", pCmd->u16len);
	int i;
	for (i = 0; i < pCmd->u16len && i < 8; i++) {
		V_PRINTF("%02X", pCmd->au8data[i]);
	}
	if (i < pCmd->u16len) {
		V_PRINTF("...");
	}

	return;
}

/**
 * DO1をトグル動作させる
 */
void vLED_Toggle( void )
{
	if( u32TickCount_ms-u32TempCount_ms > 500 ||	//	前回切り替わってから500ms以上たっていた場合
		u32TempCount_ms == 0 ){						//	初めてここに入った場合( u32TickTimer_msが前回切り替わった場合はごめんなさい )
		sAppData.u8DO_State = !sAppData.u8DO_State;
		//	DO1のLEDがトグル動作する
		vPortSet_TrueAsLo( PORT_OUT1, sAppData.u8DO_State );
		u32TempCount_ms = u32TickCount_ms;
	}
}

#ifdef USE_LCD
/**
 * LCDの初期化
 */
static void vLcdInit(void) {
	/* Initisalise the LCD */
	vLcdReset(3, 0);

	/* register for vfPrintf() */
	sLcdStream.bPutChar = LCD_bTxChar;
	sLcdStream.u8Device = 0xFF;
	sLcdStreamBtm.bPutChar = LCD_bTxBottom;
	sLcdStreamBtm.u8Device = 0xFF;
}

/**
 * LCD を描画する
 */
static void vLcdRefresh(void) {
	vLcdClear();
	vDrawLcdDisplay(0, TRUE); /* write to lcd module */
	vLcdRefreshAll(); /* display new data */
}
#endif
/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/
