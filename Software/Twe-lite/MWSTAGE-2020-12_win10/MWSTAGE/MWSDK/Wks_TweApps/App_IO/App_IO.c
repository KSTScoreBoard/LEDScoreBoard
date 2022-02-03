/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE *
 * AGREEMENT).                                                    */

/****************************************************************************/
/***        Include files                                                 ***/
/****************************************************************************/
#include <string.h>
#include <AppHardwareApi.h>

#include "App_IO.h"

#include "ccitt8.h"
#include "Interrupt.h"

#include "utils.h"
#include "flash.h"

#include "common.h"
#include "config.h"

#include "Pairing.h"

// IO Read Options
#include "btnMgr.h"

// 重複チェッカ
#include "duplicate_checker.h"

// Serial options
#include <serial.h>
#include <fprintf.h>
#include <sprintf.h>

#include "modbus_ascii.h"
#include "input_string.h"

/****************************************************************************/
/***        ToCoNet Definitions                                           ***/
/****************************************************************************/
// Select Modules (define befor include "ToCoNet.h")
// #define ToCoNet_USE_MOD_RXQUEUE_BIG
#define ToCoNet_USE_MOD_CHANNEL_MGR

// includes
#include "ToCoNet.h"
#include "ToCoNet_mod_prototype.h"

#include "app_event.h"

/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/
#undef QUICK_TRANSFER_ON_MODE7
#ifdef QUICK_TRANSFER_ON_MODE7
// 実験的なモード。
// MODE7 で電源が入った時に、IO状態を見ず HoldMask にあるポートを押し下げとしてパケット送信する
# warning "QUICK_TRANSFER_ON_MODE7"
#endif

/* i16TransmitIoData() のパラメータ */
#define TX_OPT_NODELAY_BIT 1
#define TX_OPT_SMALLDELAY_BIT 2
#define TX_OPT_QUICK_BIT 4
#define TX_OPT_RESP_BIT 8
#define TX_OPT_BY_INT 0x10 // 割り込みによる送信を明示する (sleep 時は bWakeUpByButton フラグも利用される)

/****************************************************************************/
/***        Type Definitions                                              ***/
/****************************************************************************/

/****************************************************************************/
/***        Local Function Prototypes                                     ***/
/****************************************************************************/
static void vProcessEvCore(tsEvent *pEv, teEvent eEvent, uint32 u32evarg);
static void vProcessEvCoreSlp(tsEvent *pEv, teEvent eEvent, uint32 u32evarg);
static void vProcessEvCorePwr(tsEvent *pEv, teEvent eEvent, uint32 u32evarg);

static void vInitHardware(int f_warm_start);
static void vInitHardware_IOs(int f_warm_start);

static void vChangeChannelPreset(uint8 u8preset);
static void vChangeChannelPresetByPorts(uint32);

static void vSerialInit(uint32, tsUartOpt *);
static void vProcessSerialCmd(tsModbusCmd *pSer);
static void vHandleSerialInput();
void vSerInitMessage();

static void vReceiveIoData(tsRxDataApp *pRx);
static void vReceiveIoSettingRequest(tsRxDataApp *pRx);
static void vReceiveSerialMsg(tsRxDataApp *pRx);

static bool_t bCheckDupPacket(tsDupChk_Context *pc, uint32 u32Addr, uint16 u16TimeStamp);

static int16 i16TransmitIoData(uint8 u8Quick);
static int16 i16TransmitIoSettingRequest(uint8 u8DstAddr, tsIOSetReq *pReq);
static int16 i16TransmitSerMsg(uint8 u8DstAddr, uint8 u8Cmd, uint8 *pDat, uint8 u8len);
static int16 i16TransmitRepeat(tsRxDataApp *pRx);

static void vSleep(uint32 u32SleepDur_ms, bool_t bPeriodic, bool_t bDeep, bool_t bNoIoInt);

void cbSaveDstAppID( tsPairingInfo* psPairingInfo );
/****************************************************************************/
/***        Exported Variables                                            ***/
/****************************************************************************/

/****************************************************************************/
/***        Local Variables                                               ***/
/****************************************************************************/
tsAppData sAppData; //!< アプリケーションデータ  @ingroup MASTER
static uint32 u32WakeBtnStatus; //!< 起床時のボタンの状態 @ingroup MASTER
static uint16 u16OnPrsTx_Count; //!< リモコンモードにおける余剰送信回数のカウント @ingroup MASTER
static bool_t bChatteringSleep; //!< TRUEなら、チャタリング抑制を目的としたスリープを実施した @ingroup MASTER

tsFILE sSerStream; //!< シリアル出力ストリーム  @ingroup MASTER
tsSerialPortSetup sSerPort; //!< シリアルポートの設定  @ingroup MASTER

tsModbusCmd sSerCmdIn; //!< シリアル入力系列のパーサー (modbus もどき)  @ingroup MASTER
tsInpStr_Context sSerInpStr; //!< 文字列入力  @ingroup MASTER

uint16 u16HoldUpdateScreen = 0; //!< スクリーンアップデートを行う遅延カウンタ  @ingroup MASTER
tsTimerContext sTimerApp; //!< タイマー管理構造体  @ingroup MASTER

uint8 au8SerInBuff[128]; //!< シリアルの入力書式のための暫定バッファ  @ingroup MASTER
uint8 au8SerOutBuff[256]; //!< シリアルの出力書式のための暫定バッファ  @ingroup MASTER

tsDupChk_Context sDupChk_IoData; //!< 重複チェック(IO関連のデータ転送)  @ingroup MASTER
tsDupChk_Context sDupChk_SerMsg; //!< 重複チェック(シリアル関連のデータ転送)  @ingroup MASTER

/****************************************************************************/
/***        FUNCTIONS                                                     ***/
/****************************************************************************/

/** @ingroup MASTER
 * アプリケーションの基本制御状態マシン。
 * - 特別な処理は無い。
 *
 * @param pEv
 * @param eEvent
 * @param u32evarg
 */
static void vProcessEvCore(tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	switch (pEv->eState) {
	case E_STATE_IDLE:
		if (eEvent == E_EVENT_START_UP) {
			if (IS_APPCONF_ROLE_SILENT_MODE()) {
				vfPrintf(&sSerStream, LB"!Note: launch silent mode."LB);
				ToCoNet_Event_SetState(pEv, E_STATE_RUNNING);
			} else {
				// LayerNetwork で無ければ、特別な動作は不要。

				// 暗号化鍵の設定
				if (IS_CRYPT_MODE()) {
					ToCoNet_bRegisterAesKey((void*)(sAppData.sFlash.sData.au8AesKey), NULL);
				}

				// 始動メッセージの表示
				if (!(u32evarg & EVARG_START_UP_WAKEUP_MASK)) {
					vSerInitMessage();
				}

				// RUNNING 状態へ遷移
				ToCoNet_Event_SetState(pEv, E_STATE_RUNNING);
			}

			break;
		}

		break;

	case E_STATE_RUNNING:
		break;
	default:
		break;
	}
}

/** @ingroup MASTER
 * アプリケーション制御（電源常時 ON モード）
 * - 機能概要
 *   - 起動時にランダムで処理を保留する（同時起動による送信パケットの競合回避のため）
 *   - 初回のDI/AD状態確定まで待つ
 *   - 実行状態では E_EVENT_APP_TICK_A (64fps タイマーイベント) を起点に処理する。
 *     - 32fps のタイミングで送信判定を行う
 *     - 定期パケット送信後は、次回のタイミングを乱数によってブレを作る。
 *
 * - 状態一覧
 *   - E_STATE_IDLE\n
 *     起動直後に呼び出される状態で、同時起動によるパケット衝突を避けるためランダムなウェイトを置き、次の状態に遷移する。
 *   - E_STATE_APP_WAIT_IO_FIRST_CAPTURE\n
 *     初回に DI および AI の入力値が確定するまでの待ちを行い、E_STATE_RUNNING に遷移する。
 *   - E_STATE_RUNNING
 *     秒６４回のタイマー割り込み (E_EVENT_TICK_A) を受けて、入力状態の変化のチェックを行い、無線パケットの送信要求を
 *     発行する。各種判定条件があるので、詳細はコード中のコメント参照。
 *
 * @param pEv
 * @param eEvent
 * @param u32evarg
 */
void vProcessEvCorePwr(tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {

	switch(pEv->eState) {
	case E_STATE_IDLE:
		if (eEvent == E_EVENT_START_UP) {
			sAppData.u16CtRndCt = 0;
			sAppData.bWakeupByButton = 0x80; // 初回起床フラグとする
		}

		if (eEvent == E_EVENT_TICK_TIMER) {
			static bool_t bStarted = FALSE;

			if (!sAppData.u16CtRndCt) {
				bStarted = TRUE;
				sAppData.u16CtRndCt = (ToCoNet_u16GetRand() & 0xFF) + 10; // 始動時にランダムで少し待つ（同時電源投入でぶつからないように）
			}
		}

		// 始動時ランダムな待ちを置く
		if (sAppData.u16CtRndCt && PRSEV_u32TickFrNewState(pEv) > sAppData.u16CtRndCt) {
			if (IS_LOGICAL_ID_PARENT(sAppData.u8AppLogicalId)) {
				if (u8_PORT_INPUT_COUNT > 0) {
					ToCoNet_Event_SetState(pEv, E_STATE_APP_WAIT_IO_FIRST_CAPTURE);
				} else {
					ToCoNet_Event_SetState(pEv, E_STATE_APP_RUNNING_PARENT);
				}
			} else if (IS_LOGICAL_ID_CHILD(sAppData.u8AppLogicalId)) {
				ToCoNet_Event_SetState(pEv, E_STATE_APP_WAIT_IO_FIRST_CAPTURE);
			} else {
				ToCoNet_Event_SetState(pEv, E_STATE_APP_RUNNING_ROUTER);
			}
			sAppData.u16CtRndCt = 32; // この変数は定期送信のタイミング用に再利用する。
		}

		break;

	case E_STATE_APP_WAIT_IO_FIRST_CAPTURE:
		// 起動直後の未確定状態
		if (eEvent == E_EVENT_APP_TICK_A) {
			if (sAppData.u8IOFixState == E_IO_FIX_STATE_READY) {
				ToCoNet_Event_SetState(pEv, E_STATE_APP_RUNNING_CHILD);
			} else {
				;
			}
		}
		break;

	case E_STATE_APP_RUNNING_ROUTER: // 中継機
		break;

	case E_STATE_APP_RUNNING_PARENT: // 親機
		break;

	case E_STATE_APP_RUNNING_CHILD: // 子機
		if (eEvent == E_EVENT_APP_TICK_A // 秒64回のタイマー割り込み
			&& (sAppData.u32CtTimer0 & 1) // 秒32回にする
			) {
			// 変更が有った場合は送信する
			//int i;

			if (sAppData.u16CtRndCt) sAppData.u16CtRndCt--; // 定期パケット送信までのカウントダウン

			bool_t bTxCond = FALSE; // 送信条件

			// リモコンモードにおける送信条件(長押しの判定)
			if (   IS_APPCONF_OPT_ON_PRESS_TRANSMIT()
				&& ((sAppData.u32CtTimer0 & sAppData.u8FpsBitMask) == sAppData.u8FpsBitMask)
				&& (sAppData.u8IOFixState == E_IO_FIX_STATE_READY)
			) {
				if (sAppData.sIOData_now.u32BtmBitmap & sAppData.u32BtnMask_Special) {
					// 長押し対象のボタンを検出した

					static uint16 u16CountMax = 0;
					if (!u16CountMax) {
						// 4fps: 1111           = 256ms
						// 8fps:  111 (64/8 -1) = 128ms
						// 16pfs:  11 (64/16-1) = 64ms
						// 32fps:   1 (64/32-1) = 32ms
						u16CountMax = sAppData.sFlash.sData.u16HoldDur_ms / ((sAppData.u8FpsBitMask + 1) * 16) + 1;
					}

					u16OnPrsTx_Count = u16CountMax;
				}

				if (u16OnPrsTx_Count) {
					u16OnPrsTx_Count--;
				}

				if (u16OnPrsTx_Count) {
					// 送信する
					bTxCond |= TRUE;
				}
			}

			// リモコンモード時に長押しを検知した場合は、他のボタンの入力を無視する
			if (u16OnPrsTx_Count
				&& (IS_APPCONF_OPT_LOW_LATENCY_INPUT() // 低レイテンシの場合
					|| (((sAppData.sIOData_now.u32BtmChanged & u32_PORT_INPUT_MASK) & (~sAppData.u32BtnMask_Special)) == 0)) // 非長押しボタン変化なし
			) {
				// 関係ないポートをクリア
				int i;
				for (i = 0; i < u8_PORT_INPUT_COUNT; i++) {
					if (!(sAppData.sFlash.sData.u32HoldMask & (1UL << i))) { // 押し下げを検知したポート
						sAppData.sIOData_now.au8Input[i] = 0;
					}
				}
				sAppData.sIOData_now.u32BtmBitmap &= sAppData.u32BtnMask_Special;
				sAppData.sIOData_now.u32BtmUsed &= sAppData.u32BtnMask_Special;
			}

			// IOの変化により送信する
			if (IS_APPCONF_OPT_ON_PRESS_TRANSMIT() && IS_APPCONF_OPT_LOW_LATENCY_INPUT()) {
				// 低レイテンシとリモコンモードでの通常ボタンは割り込み検出のみで送信する
			} else {
				// IO変化あり
				bTxCond |= sAppData.sIOData_now.u32BtmChanged ? TRUE : FALSE;
			}

			// 定期送信
			if ((sAppData.u16CtRndCt == 0) && !IS_APPCONF_OPT_ON_PRESS_TRANSMIT()) {
				if (!IS_APPCONF_OPT_NO_REGULAR_TX()) {
					bTxCond |= TRUE;
				}
			}

			// 連続送信モード
			if (sAppData.u8Mode == E_IO_MODE_CHILD_CONT_TX
						&& ((sAppData.u32CtTimer0 & sAppData.u8FpsBitMask) == sAppData.u8FpsBitMask)) {
				bTxCond |= TRUE;
			}

			// 条件が整ったので送信する
			if (bTxCond) {
				// デバッグ出力
				DBGOUT(5, "A(%02d/%04d) %d: B=%d%d%d%d %08x"LB,
					sAppData.u32CtTimer0,
					u32TickCount_ms & 8191,
					sAppData.sIOData_now.u32BtmChanged ? 1 : 0,
					sAppData.sIOData_now.au8Input[0] & 1,
					sAppData.sIOData_now.au8Input[1] & 1,
					sAppData.sIOData_now.au8Input[2] & 1,
					sAppData.sIOData_now.au8Input[3] & 1,
					sAppData.sIOData_now.u32BtmBitmap
				);

				// 低遅延送信が必要かどうかの判定
				bool_t bQuick = FALSE;
				if (sAppData.sIOData_now.u32BtmChanged && (sAppData.sFlash.sData.u32Opt & E_APPCONF_OPT_LOW_LATENCY_INPUT)) {
					bQuick = TRUE;
				}

				// 送信要求
				sAppData.sIOData_now.i16TxCbId = i16TransmitIoData(bQuick);

				// 変更フラグのクリア
				sAppData.sIOData_now.u32BtmChanged = 0;

				// 次の定期パケットのタイミングを仕込む
				sAppData.u16CtRndCt = (ToCoNet_u16GetRand() & 0xF) + 24;
			}
		}
		break;

	default:
		break;
	}
}

/**  @ingroup MASTER
 * アプリケーション制御（スリープ稼動モード）\n
 * 本状態遷移マシンは、mode=4, mode=7 で起動したときに登録され、測定完了待ち⇒送信⇒
 * 送信完了待ちを実施し、その後、再びスリープを実行する。
 *
 * - 機能概要
 *   - ADやDIの状態が確定するまで待つ。
 *   - 送信する。
 *   - 送信完了を待つ。
 *   - スリープする。
 *
 * - 状態一覧
 *   - E_STATE_IDLE\n
 *     起動直後またはスリープ復帰後の状態。UARTにメッセージを表示し、最初の TickTimer で
 *     E_STATE_RUNNING に遷移する。
 *   - E_STATE_RUNNING\n
 *     IO状態の確定を待って、無線送信要求、E_STATE_WAIT_TX へ遷移。
 *   - E_STATE_WAIT_TX\n
 *     送信完了イベントを待つ。実際は cbToCoNet_TxEvent() よりコールされる。
 *   - E_STATE_FINISHED\n
 *     スリープ条件が成立するまでの待ちを行う。具体的にはボタン駆動した時にチャタリングの
 *     影響が去るまでの時間待ちである。
 *   - E_STATE_APP_SLEEPING\n
 *     スリープ処理を行う。
 *
 * @param pEv
 * @param eEvent
 * @param u32evarg
 */
void vProcessEvCoreSlp(tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	switch(pEv->eState) {
	case E_STATE_IDLE:
		if (eEvent == E_EVENT_START_UP) {
			if (u32evarg & EVARG_START_UP_WAKEUP_MASK) {
				// ウェイクアップ時
				vfPrintf(&sSerStream, LB"!INF %s:", sAppData.bWakeupByButton ? "DI" : "TM");
			} else {
				// 電源投入起動(POR)
				if (IS_APPCONF_OPT_LOW_LATENCY_INPUT_SLEEP_TX_BY_INT()) {
					// 低レイテンシモードの場合は電源投入時の IO 状態を元に、即送信
					vfPrintf(&sSerStream, LB"!INF PO:");
				}

				sAppData.bWakeupByButton = 0x80; // 起床時のフラグ
			}
		}

		ToCoNet_Event_SetState(pEv, E_STATE_RUNNING);
		break;

	case E_STATE_RUNNING:
		// 割り込み要因に対して送信する。
		if (eEvent == E_EVENT_NEW_STATE
				&& sAppData.bWakeupByButton // IO割り込みで起床
				&& IS_APPCONF_OPT_LOW_LATENCY_INPUT_SLEEP_TX_BY_INT()) { // オプションビットの設定
			int i;

			uint32 u32BtmStatus =
				((IS_APPCONF_OPT_DI_INVERT() ? ~sAppData.u32DIO_startup : sAppData.u32DIO_startup) & u32_PORT_INPUT_MASK)
				| u32WakeBtnStatus; // 割り込み要因が消えているかもしれないので念のためビットを上書きする

			// メッセージの出力
			for (i = 0; i < u8_PORT_INPUT_COUNT; i++) {
				uint8 c;

				c = (u32BtmStatus & (1UL << au8PortTbl_DIn[i])) ? 0x01 : 0x00;
				c += (u32WakeBtnStatus & (1UL << au8PortTbl_DIn[i])) ? 0x02 : 0x00;

				const char d[] = { '0', '1', '*', '*' };
				vPutChar(&sSerStream, d[c]);

				// 入力状態の情報を構築
				sAppData.sIOData_now.au8Input[i] = u32BtmStatus & (1UL << au8PortTbl_DIn[i]) ? 1 : 0;
			}
			vfPrintf(&sSerStream, LB);

			// ビットマップなどの情報を更新
			sAppData.sIOData_now.u32BtmBitmap = u32BtmStatus & u32_PORT_INPUT_MASK;
			if (sAppData.sIOData_now.u32BtmUsed == 0xFFFFFFFF) {
				sAppData.sIOData_now.u32BtmUsed = sAppData.sIOData_now.u32BtmBitmap;
			} else {
				sAppData.sIOData_now.u32BtmUsed |= sAppData.sIOData_now.u32BtmBitmap;
			}
			sAppData.sIOData_now.u32BtmChanged =
					(sAppData.bWakeupByButton & 0x80) ?
							u32_PORT_INPUT_MASK : // POR 時は全部変化とする
							u32WakeBtnStatus;     // 起床時は、割り込み起床ピン

			// DBGOUT(0, "!%08X %08X %08X"LB, sAppData.sIOData_now.u32BtmBitmap, sAppData.sIOData_now.u32BtmUsed, sAppData.sIOData_now.u32BtmChanged);

			ToCoNet_Event_SetState(pEv, E_STATE_APP_TX);
			break;
		}

#ifdef QUICK_TRANSFER_ON_MODE7
		// HOLD マスクにあるポートを押されたとして速やかに送信する
		if (eEvent == E_EVENT_NEW_STATE) {
			int i;
			sAppData.sIOData_now.u32BtmBitmap = 0;
			for (i = 0; i < u8_PORT_INPUT_COUNT; i++) {
				if (sAppData.sFlash.sData.u32HoldMask & (1UL << i)) { // 押し下げを検知したポート
					sAppData.sIOData_now.au8Input[i] = 1;
					sAppData.sIOData_now.u32BtmBitmap |= au8PortTbl_DIn[i];
				} else {
					sAppData.sIOData_now.au8Input[i] = 0;
				}
			}
			sAppData.sIOData_now.u32BtmUsed = sAppData.sIOData_now.u32BtmBitmap;

			if (sAppData.sIOData_now.u32BtmBitmap) {
				ToCoNet_Event_SetState(pEv, E_STATE_APP_TX);
				break;
			}
		}
#endif

		// IO状態が確定すれば送信する。
		if (sAppData.u8IOFixState == E_IO_FIX_STATE_READY) {
			int i;
			for (i = 0; i < u8_PORT_INPUT_COUNT; i++) {
				vPutChar(&sSerStream, sAppData.sIOData_now.au8Input[i] & 1 ? '1' : '0');
				//vfPrintf(&sSerStream, "%d", sAppData.sIOData_now.au8Input[i] & 1);
			}

			// 割り込み起床時(または低レイテンシ始動時)は変更ビットは割り込み要因とする
			if (sAppData.bWakeupByButton) {
				sAppData.sIOData_now.u32BtmChanged = u32WakeBtnStatus;
			}

			ToCoNet_Event_SetState(pEv, E_STATE_APP_TX);
			break;
		}

		// 恐らくここに迷い込むことはないのだが念のためタイムアウトを設定する
		if (PRSEV_u32TickFrNewState(pEv) > 100) {
			vfPrintf(&sSerStream, "!INF TimeOut (E_STATE_RUNNING)");
			vWait(10000);
			vAHI_SwReset();
		}
		break;

	case E_STATE_APP_TX:
		if (eEvent == E_EVENT_NEW_STATE) {
			bool_t bRemoteMode = FALSE;

			// 長押しボタンのいずれかが押されている
			if (IS_APPCONF_OPT_ON_PRESS_TRANSMIT()
				&& (sAppData.sIOData_now.u32BtmBitmap & sAppData.u32BtnMask_Special)) {
				bRemoteMode = TRUE;

				u16OnPrsTx_Count = REMOTE_MODE_ADDITIONAL_TX_COUNT; // 長押しボタンがリリースされてからの検出回数
			}

			// 直前のスリープで長押しボタンが押された
			if (u16OnPrsTx_Count) {
				bRemoteMode = TRUE;

				// 長押し中のスリープ回復時は他通常ボタンの状態はクリアする
				if (!(sAppData.bWakeupByButton && IS_APPCONF_OPT_LOW_LATENCY_INPUT_SLEEP_TX_BY_INT())) {
					int i;
					for (i = 0; i < u8_PORT_INPUT_COUNT; i++) {
						if (!(sAppData.sFlash.sData.u32HoldMask & (1UL << i))) { // 押し下げを検知したポート
							sAppData.sIOData_now.au8Input[i] = 0;
						}
					}
					sAppData.sIOData_now.u32BtmBitmap &= sAppData.u32BtnMask_Special;
					sAppData.sIOData_now.u32BtmUsed &= sAppData.u32BtnMask_Special;
				}

			}

			// クイックで送信。完了待ちをするため CbId を保存する。
			sAppData.u32CtTimer0++;
			if (!bRemoteMode && u8_PORT_OUTPUT_COUNT > 0) {
				// 親機からのデータ要求設定を行う
				sAppData.sIOData_now.i16TxCbId = i16TransmitIoData(TX_OPT_NODELAY_BIT | TX_OPT_QUICK_BIT | TX_OPT_RESP_BIT);
			} else {
				// QUICK 送信
				sAppData.sIOData_now.i16TxCbId = i16TransmitIoData(TX_OPT_NODELAY_BIT | TX_OPT_QUICK_BIT);
			}

			if (bRemoteMode) {
				// RX は停止させておく
				if (!(sAppData.u8RxSetting & RX_STATE_CONTINUOUS)) {
					sToCoNet_AppContext.bRxOnIdle = FALSE;
					ToCoNet_vRfConfig();
				}

				// 長押し処理の専用シーケンスへ移行
				ToCoNet_Event_SetState(pEv, E_STATE_APP_REMOTE_WAIT_TX);
			} else {
				ToCoNet_Event_SetState(pEv, E_STATE_WAIT_TX);
			}
		}
		break;

	case E_STATE_APP_REMOTE_WAIT_TX:
		if (eEvent == E_EVENT_APP_TX_COMPLETE || PRSEV_u32TickFrNewState(pEv) > 100) {
			// タイムアウトは100msだがブロードキャスト送信ではタイムアウトする事はないはず。
			ToCoNet_Event_SetState(pEv, E_STATE_APP_REMOTE_FINISH);
		}
		break;

	case E_STATE_APP_REMOTE_FINISH:
		// 長押し検知時のスリープ (完了後短いスリープに入る)
		if (eEvent == E_EVENT_NEW_STATE) {
			if (!(sAppData.sIOData_now.u32BtmBitmap & sAppData.u32BtnMask_Special)) {
				if (u16OnPrsTx_Count) {
					u16OnPrsTx_Count--;
				}
			}

			if (u16OnPrsTx_Count == 0) {
				ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEPING); // これでおしまいなので、通常の長いスリープに入る
			} else {
				ToCoNet_Event_SetState(pEv, E_STATE_APP_REMOTE_SMALL_SLEEP); // 短いスリープに入る (長押し検知のため)
			}
		}
		break;

	case E_STATE_APP_REMOTE_SMALL_SLEEP:
		if (eEvent == E_EVENT_NEW_STATE) {
			// 短いスリープ実施
			vSleep(sAppData.sFlash.sData.u16HoldDur_ms, FALSE, FALSE, FALSE);
		}
		break;

	case E_STATE_WAIT_TX:
		_C {
			static uint8 u8cond; // bit0: tx, bit1: rx

			switch (eEvent) {
			case E_EVENT_NEW_STATE:
				u8cond = 0;
				break;
			case E_EVENT_APP_TX_COMPLETE:
				if (!(u8cond & 1)) {
					if (IS_APPCONF_OPT_ACK_MODE() && u32evarg && sAppData.u8Mode == E_IO_MODE_CHILD_SLP_10SEC) {
						// MODE7 & ACK モード時の IO 制御。Ack 受信が成功すれば PORT_BAUD をLOに設定する
						// その後、所定期間スリープした後、Hi に戻す
						PORT_SET_LO(PORT_BAUD);
					}
				}
				u8cond |= 1;
				break;
			case E_EVENT_APP_RX_COMPLETE:
				u8cond |= 2;
				break;
			default:
				break;
			}

			// 脱出条件
			if (   ((u8_PORT_OUTPUT_COUNT == 0) && (u8cond & 1))  // 入力のみで受信は不要
				|| ((u8_PORT_OUTPUT_COUNT >  0) && (u8cond == 3)) // 親機からのデータを待つ
			){
				ToCoNet_Event_SetState(pEv, E_STATE_FINISHED);
			}

			// タイムアウト (64ms)
			if (PRSEV_u32TickFrNewState(pEv) > 64) {
				ToCoNet_Event_SetState(pEv, E_STATE_FINISHED);
			}
		}
		break;

	case E_STATE_FINISHED:
		if (eEvent == E_EVENT_NEW_STATE) {
			SERIAL_vFlush(sSerStream.u8Device);

			pEv->bKeepStateOnSetAll = TRUE;

			// チャタリングが落ち着くまで待つ（スリープ直後に再起床すると面倒なので）
			bChatteringSleep = TRUE;
			vSleep(100, FALSE, FALSE, TRUE);
		} else {
			// 定期スリープを実行する
			//vfPrintf(&sSerStream, "!INF SLEEP %dms."LB, sAppData.u32SleepDur);
			//SERIAL_vFlush(sSerStream.u8Device);
			//vfPrintf(&sSerStream, "!S"LB);
			ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEPING);
		}
		break;

	case E_STATE_APP_SLEEPING:
		if (eEvent == E_EVENT_NEW_STATE) {
			pEv->bKeepStateOnSetAll = FALSE;
			vSleep(sAppData.u32SleepDur, TRUE, FALSE, FALSE);
		}
		break;

	default:
		break;
	}
}
/** @ingroup MASTER
 * フラッシュからのロードを行う。
 *
 * - sAppData.sConfig_UnSaved の初期化
 * - sAppData.bCustomDefaults カスタムデフォルトが採用された場合は TRUE
 * - sAppData.bFlashLoaded    フラッシュ(EEPROM)からのロードした場合は TRUE
 */
static void vLoadFlashOnBoot() {
	// configuration
	Config_vUnSetAll(&sAppData.sConfig_UnSaved);

	// デフォルト値を格納する
	sAppData.bCustomDefaults = Config_u8SetDefaults(&(sAppData.sFlash.sData));

	// load flash value
	tsFlash sSettingFlash;
	sAppData.bFlashLoaded = bFlash_Read(&sSettingFlash, FLASH_SECTOR_NUMBER - 1, 0);
	if (sAppData.bFlashLoaded &&
		(   sSettingFlash.sData.u32appkey != APP_ID
		 || (sSettingFlash.sData.u32ver !=
				((VERSION_MAIN<<16) | (VERSION_SUB<<8) | (VERSION_VAR))))
		) {
		sAppData.bFlashLoaded = FALSE;
	}

	if (sAppData.bFlashLoaded) {
		sAppData.sFlash = sSettingFlash; // フラッシュからロードした設定を採用
	}
}

/** @ingroup MASTER
 * 電源投入時・リセット時に最初に実行される処理。本関数は２回呼び出される。初回は u32AHI_Init()前、
 * ２回目は AHI 初期化後である。
 *
 * - 各種初期化
 * - ToCoNet ネットワーク設定
 * - 設定IO読み取り
 * - 緊急フラッシュ消去処理
 * - 設定値の計算
 * - ハードウェア初期化
 * - イベントマシンの登録
 * - 本関数終了後は登録したイベントマシン、および cbToCoNet_vMain() など各種コールバック関数が
 *   呼び出される。
 *
 * @param bStart TRUE:u32AHI_Init() 前の呼び出し FALSE: 後
 */
void cbAppColdStart(bool_t bStart) {
	if (!bStart) {
		// before AHI initialization (very first of code)

		// Module Registration
		ToCoNet_REG_MOD_ALL();
	} else {
		// メモリのクリア
		memset(&sAppData, 0x00, sizeof(sAppData));
		memset(&(sAppData.sIOData_now), 0xFF, sizeof(tsIOData));
		memset(&(sAppData.sIOData_reserve), 0xFF, sizeof(tsIOData));
		psCbHandlerPair = NULL;

		// デフォルトのチャネル (チャネルマネージャで決定するので意味が無いが、そのままにしておく)
		sToCoNet_AppContext.u8Channel = CHANNEL;

		// デフォルトのネットワーク指定値
		sToCoNet_AppContext.u8TxMacRetry = 3; // MAC再送回数（JN516x では変更できない）

		// CCA 関連の設定
		sToCoNet_AppContext.u8CCA_Level = 1; // CCA は最小レベルで設定 (Level=1, Retry=0 が最小）
		sToCoNet_AppContext.u8CCA_Retry = 0; // 再試行は１回のみ

		// ToCoNet の制御 Tick [ms]
		sAppData.u16ToCoNetTickDelta_ms = 1000 / sToCoNet_AppContext.u16TickHz;

		// フラッシュの読み出し
		vLoadFlashOnBoot();

		// その他ハードウェアの初期化
		vInitHardware(FALSE);

		if( LOGICAL_ID_PAIRING == au8IoModeTbl_To_LogicalID[sAppData.u8Mode] ){
			tsPairingConf sConfig;
			memset(&sConfig, 0x00, sizeof(tsPairingConf));
			sConfig.u8DataLength = 0;										//	汎用データのバイト数
			sConfig.au8Data = NULL;											//	汎用データ
			sConfig.u32AppID = PAIR_ID;										//	ペアリングするときのAppID
			sConfig.u8PairCh = PAIR_CHANNEL;								//	ペアリングするときのチャネル
			sConfig.u32PairKey = APP_ID;									//	ペアリング時の照合ID
			sConfig.u8LEDPort = 0x80;										//	ペアリングの状態を示すLEDを接続するDIOポート
			sConfig.u8ThLQI = 150;											//	ペアリング時のLQIの閾値
			sConfig.u32AppVer = ((VERSION_MAIN<<16) | (VERSION_SUB<<8) | (VERSION_VAR));	//	アプリケーションバージョン
			sConfig.sSerStream = sSerStream;								//	UARTの設定

			sConfig.pf_cbSavePair = cbSaveDstAppID;							//	コールバックする関数ポインタ

			bAHI_DoEnableOutputs(TRUE);

			vInitAppPairing(&sConfig);
			ToCoNet_Event_Register_State_Machine(pvProcessEvPair);
		} else {
			// 設定の反映
			sToCoNet_AppContext.u32AppId = sAppData.sFlash.sData.u32appid;
			sToCoNet_AppContext.u32ChMask = sAppData.sFlash.sData.u32chmask;
			sToCoNet_AppContext.u8TxPower = (sAppData.sFlash.sData.u8pow & 0x0F); // 出力の設定

			// 標準再送回数の計算
			uint8 u8retry = (sAppData.sFlash.sData.u8pow & 0xF0) >> 4;
			sAppData.u8StandardTxRetry = 0x82;
			sAppData.u8StandardTxAckRetry = 1;

			switch (u8retry) {
				case   0: break;
				case 0xF:
					sAppData.u8StandardTxRetry = 0;
					sAppData.u8StandardTxAckRetry = 0;
					break;
				default:
					sAppData.u8StandardTxRetry = 0x80 + u8retry;
					sAppData.u8StandardTxAckRetry = u8retry;
					break;
			}

			// eNwkMode の計算
			if (sAppData.sFlash.sData.u8role == E_APPCONF_ROLE_MAC_NODE) {
				sAppData.eNwkMode = E_NWKMODE_MAC_DIRECT;
			} else
			if (sAppData.sFlash.sData.u8role == E_APPCONF_ROLE_SILENT) {
				sAppData.eNwkMode = E_NWKMODE_MAC_DIRECT;
			} else {
				sAppData.bFlashLoaded = 0;
			}

			// 低レイテンシモードでは 1000Hz タイマーとする
			if (sAppData.sFlash.sData.u32Opt & E_APPCONF_OPT_LOW_LATENCY_INPUT) {
				sToCoNet_AppContext.u16TickHz = HZ_LOW_LATENCY; // 低レイテンシモードでは 1KHz 動作
				sAppData.u16ToCoNetTickDelta_ms = 1000/HZ_LOW_LATENCY;
			}

			// ToCoNet の制御 Tick [ms]
			sAppData.u16ToCoNetTickDelta_ms = 1000 / sToCoNet_AppContext.u16TickHz;

			// ヘッダの１バイト識別子を AppID から計算
			sAppData.u8AppIdentifier = u8CCITT8((uint8*)&sToCoNet_AppContext.u32AppId, 4); // APP ID の CRC8

			// IOより状態を読み取る (DI反転設定はは反映しない)
			sAppData.u32DIO_startup = ~u32PortReadBitmap(); // この時点では全部入力ポート

			// version info
			sAppData.u32ToCoNetVersion = ToCoNet_u32GetVersion();

			// 論理IDの設定チェック、その他設定値のチェック
			sAppData.u8AppLogicalId = 255; // 無効な値で初期化しておく

			// 子機動作モードの場合の Logical ID は、ここで設定しておく
			if (IS_LOGICAL_ID_CHILD(au8IoModeTbl_To_LogicalID[sAppData.u8Mode])) {
				// 子機IDはフラッシュ値・カスタムデフォルト値が設定されていれば、これを採用
				if (sAppData.bFlashLoaded || sAppData.bCustomDefaults) {
					sAppData.u8AppLogicalId = sAppData.sFlash.sData.u8id;
				}

				// デフォルト値の設定
				if (!IS_LOGICAL_ID_CHILD(sAppData.u8AppLogicalId )) {
					sAppData.u8AppLogicalId = au8IoModeTbl_To_LogicalID[sAppData.u8Mode];
				}
			}

			// 論理IDを121,122に保存した場合、親機・中継器で起動する
			if (sAppData.bFlashLoaded || sAppData.bCustomDefaults) {
				if (sAppData.sFlash.sData.u8id == LOGICAL_ID_CHILDREN + E_IO_MODE_PARNET /* 121 */) {
					sAppData.u8Mode = E_IO_MODE_PARNET; // 親機のモード番号
				} else
				if (sAppData.sFlash.sData.u8id == LOGICAL_ID_CHILDREN + E_IO_MODE_ROUTER /* 122 */) {
					sAppData.u8Mode = E_IO_MODE_ROUTER; //中継器のモード番号
				}
			}

#if 0
#warning "DEBUG"
			if (sAppData.u8Mode == E_IO_MODE_CHILD_SLP_1SEC) {
				sAppData.u8Mode = E_IO_MODE_CHILD_SLP_10SEC;
			}
#endif

			// 各モード依存の初期値の設定など
			switch(sAppData.u8Mode) {
			case E_IO_MODE_PARNET:
				sAppData.u8AppLogicalId = LOGICAL_ID_PARENT;
				break;

			case E_IO_MODE_ROUTER:
				sAppData.u8AppLogicalId = LOGICAL_ID_REPEATER;
				break;

			case E_IO_MODE_CHILD_SLP_1SEC:
				if (!sAppData.u32SleepDur) {
					if (sAppData.bFlashLoaded || sAppData.bCustomDefaults) {
						sAppData.u32SleepDur = sAppData.sFlash.sData.u16SleepDur_ms;
					} else {
						sAppData.u32SleepDur = MODE4_SLEEP_DUR_ms;
					}
				}
				break;

			case E_IO_MODE_CHILD_SLP_10SEC:
				if (!sAppData.u32SleepDur) {
					if (sAppData.bFlashLoaded || sAppData.bCustomDefaults) {
						sAppData.u32SleepDur = sAppData.sFlash.sData.u16SleepDur_s * 1000L;
					} else {
						sAppData.u32SleepDur = MODE7_SLEEP_DUR_ms;
					}
				}
				break;

			case E_IO_MODE_CHILD_CONT_TX:
				break;

			case E_IO_MODE_CHILD:
				break;

			default: // 未定義機能なので、SILENT モードにする。
				sAppData.u8AppLogicalId = 255;
				sAppData.sFlash.sData.u8role = E_APPCONF_ROLE_SILENT;
				break;
			}

			// ポートの入出力を決定
			sAppData.u8IoTbl = IS_APPCONF_OPT_PORT_TBL1() ? 1 : 0;
			sAppData.u8IoTbl += IS_APPCONF_OPT_PORT_TBL2() ? 2 : 0;
			bPortTblInit(sAppData.u8IoTbl, IS_LOGICAL_ID_PARENT(sAppData.u8AppLogicalId));

			// FPS のビットマスク
//			sAppData.u8FpsBitMask = 1;
//			if (sAppData.bFlashLoaded || sAppData.bCustomDefaults) {
				// 4fps: 1111
				// 8fps:  111 (64/8 -1)
				// 16pfs:  11 (64/16-1)
				// 32fps:   1 (64/32-1)
				sAppData.u8FpsBitMask = 64 / sAppData.sFlash.sData.u8Fps - 1;
				// DBGOUT(0, "fps mask = %x"LB, sAppData.u8FpsBitMask);
//			}

			// IO設定に基づきチャネルを設定する
			vChangeChannelPresetByPorts(sAppData.u32DIO_startup);

			// 親機子機が決まったので IO 設定を行う
			vInitHardware_IOs(TRUE);

			// ショートアドレスの設定(決めうち)
			sToCoNet_AppContext.u16ShortAddress = SERCMD_ADDR_CONV_TO_SHORT_ADDR(sAppData.u8AppLogicalId);

			// UART の初期化
			ToCoNet_vDebugInit(&sSerStream);
			ToCoNet_vDebugLevel(0);

			// その他の初期化
			DUPCHK_vInit(&sDupChk_IoData); // 重複チェック用
			DUPCHK_vInit(&sDupChk_SerMsg); // 重複チェック用

			// 主状態遷移マシンの登録
			ToCoNet_Event_Register_State_Machine(vProcessEvCore);

			if (!(IS_APPCONF_ROLE_SILENT_MODE())) {
				// 状態遷移マシン、RxOnIdle の設定
				switch(sAppData.u8Mode) {
				case E_IO_MODE_PARNET:
				case E_IO_MODE_ROUTER:
					ToCoNet_Event_Register_State_Machine(vProcessEvCorePwr); // 常時通電用の処理
					sAppData.prPrsEv = (void*)vProcessEvCorePwr;

					// App_IO では親機と中継機のみ受信できる
					sAppData.u8RxSetting = RX_STATE_BOOT_ON | RX_STATE_CONTINUOUS;
					break;

				case E_IO_MODE_CHILD:
				case E_IO_MODE_CHILD_CONT_TX:
					ToCoNet_Event_Register_State_Machine(vProcessEvCorePwr); // 常時通電用の処理
					sAppData.prPrsEv = (void*)vProcessEvCorePwr;

					if (IS_APPCONF_OPT_CHILD_RECV_OTHER_NODES() || u8_PORT_OUTPUT_COUNT > 0) {
						// 子機を受信可能モードに設定する
						sAppData.u8RxSetting = RX_STATE_BOOT_ON | RX_STATE_CONTINUOUS;
					}
					break;

				case E_IO_MODE_CHILD_SLP_1SEC:
				case E_IO_MODE_CHILD_SLP_10SEC:
					ToCoNet_Event_Register_State_Machine(vProcessEvCoreSlp); // スリープ用の処理
					sAppData.prPrsEv = (void*)vProcessEvCoreSlp;

					if (u8_PORT_OUTPUT_COUNT > 0) {
						sAppData.u8RxSetting = RX_STATE_BOOT_ON;
					}
					break;
				default: // 未定義機能なので、SILENT モードにする。
					break;
				}

				// RX 設定
				if (sAppData.u8RxSetting & RX_STATE_BOOT_ON) {
					sToCoNet_AppContext.bRxOnIdle = TRUE;
				}

				// MAC の初期化
				ToCoNet_vMacStart();
			}
		}
	}
}

/** @ingroup MASTER
 * スリープ復帰後に呼び出される関数。\n
 * 本関数も cbAppColdStart() と同様に２回呼び出され、u32AHI_Init() 前の
 * 初回呼び出しに於いて、スリープ復帰要因を判定している。u32AHI_Init() 関数は
 * これらのレジスタを初期化してしまう。
 *
 * - 変数の初期化（必要なもののみ）
 * - ハードウェアの初期化（スリープ後は基本的に再初期化が必要）
 * - イベントマシンは登録済み。
 *
 * @param bStart TRUE:u32AHI_Init() 前の呼び出し FALSE: 後
 */
void cbAppWarmStart(bool_t bStart) {
	if (!bStart) {
		// before AHI init, very first of code.
		//  to check interrupt source, etc.

		vAHI_DioSetPullup( (1<<PORT_EI1)|(1<<PORT_EI2),0);

		sAppData.bWakeupByButton = FALSE;
		u32WakeBtnStatus = u32AHI_DioWakeStatus();

		if(u8AHI_WakeTimerFiredStatus()) {
			;
		} else
		if(u32WakeBtnStatus & u32_PORT_INPUT_MASK) {
			// woke up from DIO events
			sAppData.bWakeupByButton = TRUE;
		}
	} else {
		int i;

		// データ領域の初期化
		memset(&(sAppData.sIOData_now), 0xFF, sizeof(tsIOData));

		// いくつかのデータは復元
		sAppData.sIOData_now.u32BtmUsed = sAppData.sIOData_reserve.u32BtmUsed;
		for (i = 0; i < MAX_IO_TBL; i++) {
			sAppData.sIOData_now.au8Input[i] = sAppData.sIOData_reserve.au8Input[i];
			sAppData.sIOData_now.au8Output[i] = sAppData.sIOData_reserve.au8Output[i];
		}

		// 変数の初期化（必要なものだけ）
		sAppData.u16CtTimer0 = 0; // このカウンタは、起動時からのカウントとする
		sAppData.u8IOFixState = E_IO_FIX_STATE_NOT_READY; // IO読み取り状態の確定待ちフラグ

		// スリープ条件の判定
		// チャタリング抑制期間からのスリープ復帰
		if (bChatteringSleep) {
			bChatteringSleep = FALSE;
			if (!u16OnPrsTx_Count) {
				// IO 状態を基に戻す
				if (IS_APPCONF_OPT_ACK_MODE() && sAppData.u8Mode == E_IO_MODE_CHILD_SLP_10SEC) {
					PORT_SET_HI(PORT_BAUD);
				}

				// abAppWarmStart を抜けると MAC 層の初期化など大きな処理が続くため、ここでスリープ
				vSleep(sAppData.u32SleepDur, FALSE, FALSE, FALSE);
			}
		}

		// その他ハードウェアの初期化（基本、起動時に毎回実行する）
		vInitHardware(TRUE);
		vInitHardware_IOs(TRUE);

		// 起床時のポート状態 (DI 反転は考慮しない)
		sAppData.u32DIO_startup = ~u32PortReadBitmap();

		// IO設定に基づきチャネルを設定する
		vChangeChannelPresetByPorts(sAppData.u32DIO_startup);

		// UART の初期化
		ToCoNet_vDebugInit(&sSerStream);
		ToCoNet_vDebugLevel(0);

		// その他の初期化
		DUPCHK_vInit(&sDupChk_IoData);
		DUPCHK_vInit(&sDupChk_SerMsg);

		// RX 設定
		if (sAppData.u8RxSetting & RX_STATE_BOOT_ON) {
			sToCoNet_AppContext.bRxOnIdle = TRUE;
		}

		// MAC の開始
		ToCoNet_vMacStart();
	}
}

/** @ingroup MASTER
 * 本関数は ToCoNet のメインループ内で必ず１回は呼び出される。
 * ToCoNet のメインループでは、CPU DOZE 命令を発行しているため、割り込みなどが発生した時に
 * 呼び出されるが、処理が無い時には呼び出されない。
 * しかし TICK TIMER の割り込みは定期的に発生しているため、定期処理としても使用可能である。
 *
 * - シリアルの入力チェック
 */
void cbToCoNet_vMain(void) {
	/* handle uart input */
	vHandleSerialInput();
}

/** @ingroup MASTER
 * パケットの受信完了時に呼び出されるコールバック関数。\n
 * パケットの種別によって具体的な処理関数にディスパッチしている。
 * データ種別は psRx->u8Cmd (ToCoNet のパケットヘッダに含まれます) により識別される。
 *
 * - パケット種別
 *   - TOCONET_PACKET_CMD_APP_DATA : シリアル電文パケット
 *   - TOCONET_PACKET_CMD_APP_USER_IO_DATA : IO状態の伝送
 *   - TOCONET_PACKET_CMD_APP_USER_IO_DATA_EXT : シリアル電文による IO 状態の伝送
 *
 * @param psRx 受信パケット
 */
void cbToCoNet_vRxEvent(tsRxDataApp *psRx) {
	if (psCbHandlerPair && psCbHandlerPair->pf_cbToCoNet_vRxEvent) {
		(*psCbHandlerPair->pf_cbToCoNet_vRxEvent)(psRx);
	}else{
		//uint8 *p = pRx->auData;

		DBGOUT(3, "Rx packet (cm:%02x, fr:%08x, to:%08x)"LB, psRx->u8Cmd, psRx->u32SrcAddr, psRx->u32DstAddr);

		if (IS_APPCONF_ROLE_SILENT_MODE()) {
			// SILENT では受信処理をしない
			return;
		}

		// 暗号化モードで平文は無視する
		if (IS_CRYPT_MODE()) {
			if (!psRx->bSecurePkt) {
				DBGOUT(5, LB"Recv Plain Pkt!");
				return;
			}
		}

		switch (psRx->u8Cmd) {
		case TOCONET_PACKET_CMD_APP_USER_IO_DATA: // IO状態の伝送
			vReceiveIoData(psRx);
			break;
		case TOCONET_PACKET_CMD_APP_USER_IO_DATA_EXT: // IO状態の伝送(UART経由)
			vReceiveIoSettingRequest(psRx);
			break;
		case TOCONET_PACKET_CMD_APP_USER_SERIAL_MSG: // IO状態の伝送(UART経由)
			vReceiveSerialMsg(psRx);
			break;
		}
	}
}


/** @ingroup MASTER
 * 送信完了時に呼び出されるコールバック関数。
 *
 * - IO 送信完了イベントはイベントマシンにE_EVENT_APP_TX_COMPLETEイベントを伝達する。
 * - シリアルメッセージの一連のパケット群の送信完了も検出しsSerSeqTx.bWaitComplete
 *   フラグをリセットする。
 *
 * @param u8CbId 送信時に設定したコールバックID
 * @param bStatus 送信ステータス
 */
void cbToCoNet_vTxEvent(uint8 u8CbId, uint8 bStatus) {
	if (psCbHandlerPair && psCbHandlerPair->pf_cbToCoNet_vTxEvent) {
		(*psCbHandlerPair->pf_cbToCoNet_vTxEvent)(u8CbId, bStatus);
	}else{
		//uint8 *q = au8SerOutBuff;
		if (IS_APPCONF_ROLE_SILENT_MODE()) {
			return;
		}

		// IO 関連の送信が完了した
		if (sAppData.sIOData_now.i16TxCbId >= 0
				&& u8CbId == sAppData.sIOData_now.i16TxCbId) {
			// スリープを行う場合は、このイベントを持ってスリープ遷移
			ToCoNet_Event_Process(E_EVENT_APP_TX_COMPLETE, bStatus, sAppData.prPrsEv);
		}
	}
	return;
}

/** @ingroup MASTER
 * ネットワーク層などのイベントが通達される。\n
 * 本アプリケーションでは特別な処理は行っていない。
 *
 * @param ev
 * @param u32evarg
 */
void cbToCoNet_vNwkEvent(teEvent ev, uint32 u32evarg) {
	if (IS_APPCONF_ROLE_SILENT_MODE()) {
		return;
	}

	switch(ev) {
	case E_EVENT_TOCONET_NWK_START:
		break;

	case E_EVENT_TOCONET_NWK_DISCONNECT:
		break;

	default:
		break;
	}
}

/** @ingroup MASTER
 * ハードウェア割り込み時に呼び出される。本処理は割り込みハンドラではなく、割り込みハンドラに登録された遅延実行部による処理で、長い処理が記述可能である。
 * 本アプリケーションに於いては、ADC/DIの入力状態のチェック、64fps のタイマーイベントの処理などを行っている。
 *
 * - E_AHI_DEVICE_SYSCTRL
 *   - DI割り込みの処理を行う。これは、低レイテンシモードでの処理である。
 *
 * - E_AHI_DEVICE_TICK_TIMER : このイベントは ToCoNet 組み込みで、ToCoNet の制御周期 (sToCoNet_AppContext.u16TickHz) を
 *   実現するためのタイマーです。ユーザが TickTimer の制御を変更したりすると ToCoNet は動作しなくなります。
 *
 *   - Di入力の変化の確認。変化が有れば、sAppData.sIOData_now 構造体に結果を格納する。
 *     低レイテンシモードの時は、この判定を起点として送信を行う。
 *
 * - E_AHI_DEVICE_TIMER0 : TICK_TIMER から分周して制御周期を作っても良いのですが、TIMER_0 を使用しています。
 *   - カウンタのインクリメント処理
 *   - ADC の完了確認
 *   - パケット重複チェックアルゴリズムのタイムアウト処理
 *   - DIのカウンタ処理 (インタラクティブモードでカウンタ終了時にもパケットを送信する処理を行う）
 *   - イベントマシンに TIMER0 イベントを発行
 *   - インタラクティブモード時の画面再描画
 *
 * @param u32DeviceId
 * @param u32ItemBitmap
 */
void cbToCoNet_vHwEvent(uint32 u32DeviceId, uint32 u32ItemBitmap) {
	if (psCbHandlerPair && psCbHandlerPair->pf_cbToCoNet_vHwEvent) {
		(*psCbHandlerPair->pf_cbToCoNet_vHwEvent)(u32DeviceId, u32ItemBitmap);
	}else{
	switch (u32DeviceId) {
	case E_AHI_DEVICE_SYSCTRL:
		// 低レイテンシモードでは割り込みが入る。
		// 割り込みハンドラを抜けた後のアプリケーション処理がこの部分。
		if (sAppData.u8IOFixState == E_IO_FIX_STATE_READY
				&& sAppData.u8Mode != E_IO_MODE_CHILD_SLP_1SEC   // SLEEP モードでない
				&& sAppData.u8Mode != E_IO_MODE_CHILD_SLP_10SEC  // SLEEP モードでない
		) {
			int i;
			bool_t bTransmit = FALSE;

			/* DIの入力ピンの番号を調べる。
			 *
			 *  ボタンを猿みたいに押してみたが DIO の割り込みは同時に２ビット報告されることは無く、
			 *  順 次処理されるようだ。しかしながら、同時処理されても良いようなコードにしておく。
			 */
			DBGOUT(1, LB"BTN: ");
			for (i = 0; i < u8_PORT_INPUT_COUNT; i++) {
				/* DI検知したポートについては sAppData.sIOData_now.au8Input[] に値を設定する。
				 *
				 * この値の下位１ビットが、１になると Lo 判定したことを意味する。
				 * この値の上位４ビットは、再度の割り込みを検知しても無視するためのカウンタとして
				 * 用いる。このカウンタは E_AHI_DEVICE_TIMER0 イベントで処理する。
				 */
				DBGOUT(1, "%c", u32ItemBitmap & (1UL << au8PortTbl_DIn[i]) ? '1' : '0');

				if (u32ItemBitmap & (1UL << au8PortTbl_DIn[i])) { // 押し下げを検知したポート
					uint8 u8stat = sAppData.sIOData_now.au8Input[i]; // 元の値を取り出す。
					uint8 u8ct = (u8stat & 0xF0) >> 4; // 上位４ビットは、前回の同様の割り込み受信からの 64fps カウンタ
					// カウンタ値が無い場合は、割り込みを有効とする。
					if (u8ct == 0) {
						sAppData.sIOData_now.au8Input[i] = (LOW_LATENCY_DELAYED_TRANSMIT_COUNTER * 0x10) + 1;
						bTransmit = TRUE;
					} else {
						// カウンタ値が有る場合は、直前に押されたためチャタリングなどが考えられ処理しない
						;
					}
				}
			}

			// いずれかのポートの割り込みが有効であった場合。
			if (bTransmit) {
				/* 速やかに送信する
				 *   ポートの変更対象フラグ(u32BtmUsed)を、この割り込みで入力検知したものとする。
				 *   そうしないと、関係ないビットが変更されてしまう
				 */
				uint32 u32used = sAppData.sIOData_now.u32BtmUsed; // 関数呼び出し中だけ値を変更する
				uint32 u32changed = sAppData.sIOData_now.u32BtmChanged;

				sAppData.sIOData_now.u32BtmUsed = u32ItemBitmap & u32_PORT_INPUT_MASK; // 割り込みでLoになったDINだけ変更対照として送信する
				sAppData.sIOData_now.u32BtmChanged = u32ItemBitmap & u32_PORT_INPUT_MASK; // 割り込みでLoになったDINだけ変更対照として送信する

				sAppData.sIOData_now.i16TxCbId = i16TransmitIoData(TX_OPT_NODELAY_BIT | TX_OPT_QUICK_BIT | TX_OPT_BY_INT); // 送信処理を行う

				sAppData.sIOData_now.u32BtmUsed = u32used | (u32ItemBitmap & u32_PORT_INPUT_MASK); //値を復元する
				sAppData.sIOData_now.u32BtmChanged = u32changed;
			}
		}
		break;

	case E_AHI_DEVICE_ANALOGUE: //ADC完了時にこのイベントが発生する。
		break;

	case E_AHI_DEVICE_TICK_TIMER: //比較的頻繁な処理
		// ボタンの判定を行う。
		_C {
			uint32 bmPorts, bmChanged, i;

			if (bBTM_GetState(&bmPorts, &bmChanged)) {
				// DI 反転対応
				if (IS_APPCONF_OPT_DI_INVERT()) {
					bmPorts = ~bmPorts & u32_PORT_INPUT_MASK;
				}

				// 読み出し値を格納する
				for (i = 0; i < u8_PORT_INPUT_COUNT; i++) {
					uint8 u8stat = (sAppData.sIOData_now.au8Input[i] == 0xFF) ? 0 : sAppData.sIOData_now.au8Input[i];
					// LSBを設定する
					if (bmPorts & (1UL << au8PortTbl_DIn[i])) {
						u8stat |= 0x01;
					} else {
						u8stat &= 0xFE;
					}
					sAppData.sIOData_now.au8Input[i] = u8stat;
				}
				sAppData.sIOData_now.u32BtmBitmap = bmPorts; // au8Input と冗長だが両方管理する。

				// EI関連のポートが変化した
				if (bmChanged & ((1UL << PORT_EI1) | (1UL << PORT_EI2))) {
					// チャネルを変更する
					vChangeChannelPresetByPorts(bmPorts);
				}

				if (bmChanged) { // 入力ポートの値が確定したか、変化があった
					// この２ビットをクリアしておく(入力に関係ない EI ポートのため)
					bmChanged &= ~((1UL << PORT_EI1) | (1UL << PORT_EI2));

					// 利用入力ポートの判定
					if (sAppData.sIOData_now.u32BtmUsed == 0xFFFFFFFF) {
						sAppData.sIOData_now.u32BtmUsed = bmPorts; // 一番最初の確定時に Lo のポートは利用ポート
					} else {
						sAppData.sIOData_now.u32BtmUsed |= bmPorts; // 利用ポートのビットマップは一度でもLo確定したポート
					}

					// 変化ポートの判定
					if (sAppData.u8IOFixState == E_IO_FIX_STATE_READY) {
						// 初回確定後
						sAppData.sIOData_now.u32BtmChanged |= bmChanged; // 変化フラグはアプリケーションタスクに変更させる
					} else {
						// 初回確定時(スリープ復帰後も含む)
						sAppData.sIOData_now.u32BtmChanged = bmChanged; // 初回は変化を報告
					}

					// IO確定状態とする
					sAppData.u8IOFixState = E_IO_FIX_STATE_READY;
				}
			}

			// 低レイテンシモードならここで送信を行う。
			static bool_t bLowLatencyTxCond = 0xFF; // 判定回数を減らすため、このフラグを利用する
			if (bLowLatencyTxCond == 0xFF) {
				if (sAppData.u8IOFixState == E_IO_FIX_STATE_READY) {
					if ((sAppData.sFlash.sData.u32Opt & E_APPCONF_OPT_LOW_LATENCY_INPUT)
						&& (sAppData.u8Mode != E_IO_MODE_CHILD_SLP_10SEC)
						&& (sAppData.u8Mode != E_IO_MODE_CHILD_SLP_1SEC)
						&& !IS_APPCONF_OPT_ON_PRESS_TRANSMIT() // リモコンモード＆低レイテンシの場合はここでは送信しない
					) {
						bLowLatencyTxCond = TRUE;
					} else {
						bLowLatencyTxCond = FALSE;
					}
				}
			}
			if (bLowLatencyTxCond == TRUE && sAppData.sIOData_now.u32BtmChanged) {
				sAppData.sIOData_now.i16TxCbId = i16TransmitIoData(TX_OPT_NODELAY_BIT | TX_OPT_QUICK_BIT);
				sAppData.sIOData_now.u32BtmChanged = 0;
			}
		}

		// 一定期間受信が無い場合、IO状態をHIに自動で戻す処理
		if (IS_APPCONF_OPT_ON_PRESS_TRANSMIT()) {
			if (u32TickCount_ms - sAppData.sIOData_now.u32RxLastTick > sAppData.sFlash.sData.u16HoldDur_ms) { // 一定時間で自動停止
				int i = 0, j = 1;

				// ポートの値を設定する（変更フラグのあるものだけ）
				for (; i < u8_PORT_OUTPUT_COUNT; i++, j <<= 1) {
					if (j & sAppData.sFlash.sData.u32HoldMask) {
						PORT_SET_HI(au8PortTbl_DOut[i]);
						sAppData.sIOData_now.au8Output[i] = FALSE;
					}
				}
			}
		}
		break;

	case E_AHI_DEVICE_TIMER0:
		// ウォッチドッグの設定
		if (IS_APPCONF_OPT_WATCHDOG_OUTPUT()) {
			PORT_SET_TRUEASLO(PORT_EI2, sAppData.u32CtTimer0 & 1);
		}

		// タイマーカウンタをインクリメントする (64fps なので 64カウントごとに１秒)
		sAppData.u32CtTimer0++;
		sAppData.u16CtTimer0++;

		// 重複チェックのタイムアウト処理
		if ((sAppData.u32CtTimer0 & 0xF) == 0) {
			DUPCHK_bFind(&sDupChk_IoData, 0, NULL);
			DUPCHK_bFind(&sDupChk_SerMsg, 0, NULL);
		}

		/* インタラクティブモードのカウンタ処理。
		 * カウンタが0になったときに送信フラグを立てる。
		 * 1.3.4 カウンタが0までは押し下げフラグを維持
		 */
		{
			int i;

			// Input ビットのカウンタビットをカウントダウンする。
			bool_t bUpdated = FALSE;
			for(i = 0; i < u8_PORT_INPUT_COUNT; i++) {
				uint8 u8stat = sAppData.sIOData_now.au8Input[i];
				if (u8stat == 0xFF) continue; // 初期化直後（まだ値が入っていない）

				uint8 u8ct = u8stat >> 4;

				if (u8ct) {
					u8ct--;
					if (u8ct == 0) {
						// 送信要求
						bUpdated = TRUE;
					}
				}

				u8stat = (u8ct << 4) + (u8stat & 0x0F);
				sAppData.sIOData_now.au8Input[i] = u8stat;
			}
			if (bUpdated) {
				sAppData.sIOData_now.u32BtmChanged |= 0x80000000;
			}
		}

		// イベント処理部分にイベントを送信
		if (sAppData.prPrsEv && (sAppData.u32CtTimer0 & 1)) {
			ToCoNet_Event_Process(E_EVENT_APP_TICK_A, 0, sAppData.prPrsEv);
		}

		// シリアル画面制御のためのカウンタ
		if (!(--u16HoldUpdateScreen)) {
			if (sSerCmdIn.bverbose) {
				vSerUpdateScreen();
			}
		}
		break;

	default:
		break;
	}
	}
}

/** @ingroup MASTER
 * 割り込みハンドラ。ここでは長い処理は記述してはいけない。
 * - TICK_TIMER\n
 *   - ADCの実行管理
 *   - ボタン入力判定管理
 */
uint8 cbToCoNet_u8HwInt(uint32 u32DeviceId, uint32 u32ItemBitmap) {
	uint8 u8handled = FALSE;

	switch (u32DeviceId) {
	case E_AHI_DEVICE_TIMER0:
		break;

	case E_AHI_DEVICE_TICK_TIMER:
		// ボタンハンドラの駆動
		if (sAppData.pr_BTM_handler) {
			// ハンドラを稼働させる
			(*sAppData.pr_BTM_handler)(sAppData.u16ToCoNetTickDelta_ms);
		}

		// ホールドモードの制御
		if (sAppData.sFlash.sData.u32HoldMask) {
			int i = 0, j = 1;
			for (; i < u8_PORT_OUTPUT_COUNT; i++, j <<= 1) {
				if (sAppData.au16HoldBtn[i]) {
					// カウントダウンしていく
					if (sAppData.au16HoldBtn[i] < sAppData.u16ToCoNetTickDelta_ms) {
						sAppData.au16HoldBtn[i] = 0;
					} else {
						sAppData.au16HoldBtn[i] -= sAppData.u16ToCoNetTickDelta_ms;
					}

					// カウント値が 0 になったら IO を Hi に戻す
					if (sAppData.au16HoldBtn[i] == 0) {
						PORT_SET_HI(au8PortTbl_DOut[i]);
					}
				}
			}
		}
		break;

	default:
		break;
	}

	return u8handled;
}

/** @ingroup MASTER
 * ハードウェアの初期化を行う。スリープ復帰時も原則同じ初期化手続きを行う。
 *
 * - 管理構造体のメモリ領域の初期化
 * - DO出力設定
 * - DI入力設定
 * - DI割り込み設定 (低レイテンシモード)
 * - M1-M3 の読み込み
 * - UARTの初期化
 * - ADC3/4 のプルアップ停止
 * - タイマー用の未使用ポートを汎用IOに解放する宣言
 * - 秒64回のTIMER0の初期化と稼働
 *
 * @param f_warm_start TRUE:スリープ復帰時
 */
static void vInitHardware(int f_warm_start) {
	// メモリのクリア
	memset(&sTimerApp, 0, sizeof(tsTimerContext));

	// モード設定
	if (!f_warm_start) {
		vPortAsInput(PORT_CONF1);
		vPortAsInput(PORT_CONF2);
		vPortAsInput(PORT_CONF3);
		sAppData.u8Mode = (bPortRead(PORT_CONF1) | (bPortRead(PORT_CONF2) << 1) | (bPortRead(PORT_CONF3) << 2));
	}

	// チャネル設定 (0 ならデフォルトorフラッシュ値、1, 2, 3 は設定値)
	vPortAsInput(PORT_EI1);
	if (!IS_APPCONF_OPT_WATCHDOG_OUTPUT()) {
		PORT_SET_LO(PORT_EI2);
		vPortAsOutput(PORT_EI2);
	} else {
		vPortAsInput(PORT_EI2);
	}

	// UART 設定
	{
		// ACK モードでは BAUD ピンを出力用に使用する
		uint32 u32baud;
		bool_t bPortBPS;

		if (IS_APPCONF_OPT_ACK_MODE() && sAppData.u8Mode == E_IO_MODE_CHILD_SLP_10SEC) {
			// ACK_MODE かつ 10秒SLEEP モード時は、BPSピンを出力として使用する
			bPortBPS = FALSE;
			u32baud = UART_BAUD;
		} else {
			vPortAsInput(PORT_BAUD);
			bPortBPS = bPortRead(PORT_BAUD);
			u32baud = bPortBPS ? UART_BAUD_SAFE : UART_BAUD;
		}

		tsUartOpt sUartOpt;
		memset(&sUartOpt, 0, sizeof(tsUartOpt));

		// BAUD ピンが GND になっている場合、かつフラッシュの設定が有効な場合は、設定値を採用する (v1.0.3)
		if ((sAppData.bFlashLoaded || sAppData.bCustomDefaults) && bPortBPS) {
			u32baud = sAppData.sFlash.sData.u32baud_safe;
			sUartOpt.bHwFlowEnabled = FALSE;
			sUartOpt.bParityEnabled = UART_PARITY_ENABLE;
			sUartOpt.u8ParityType = UART_PARITY_TYPE;
			sUartOpt.u8StopBit = UART_STOPBITS;

			// 設定されている場合は、設定値を採用する (v1.0.3)
			switch(sAppData.sFlash.sData.u8parity) {
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

			vSerialInit(u32baud, &sUartOpt);
		} else {
			vSerialInit(u32baud, NULL);
		}
	}

	// タイマの未使用ポートの解放（汎用ＩＯとして使用するため）
	vAHI_TimerFineGrainDIOControl(0x7F); // bit 0,1,2 をセット (TIMER0 の各ピンを解放する, PWM1..4 は使用する)

	// 秒64回のTIMER0の初期化と稼働
	if (!(   sAppData.u8Mode == E_IO_MODE_CHILD_SLP_10SEC
		  || sAppData.u8Mode == E_IO_MODE_CHILD_SLP_1SEC)) {
		sTimerApp.u8Device = E_AHI_DEVICE_TIMER0;
		sTimerApp.u16Hz =64;
		sTimerApp.u8PreScale = 4; // 15625ct@2^4

		vTimerConfig(&sTimerApp);
		vTimerStart(&sTimerApp);
	}
}


/** @ingroup MASTER
 * ハードウェアの初期化を行う。スリープ復帰時も原則同じ初期化手続きを行う。
 *
 * - 管理構造体のメモリ領域の初期化
 * - DO出力設定
 * - DI入力設定
 * - DI割り込み設定 (低レイテンシモード)
 * - M1-M3 の読み込み
 * - UARTの初期化
 * - ADC3/4 のプルアップ停止
 * - タイマー用の未使用ポートを汎用IOに解放する宣言
 * - 秒64回のTIMER0の初期化と稼働
 * - I2Cの初期化
 *
 * @param f_warm_start TRUE:スリープ復帰時
 */
static void vInitHardware_IOs(int f_warm_start) {
	int i;

	if (IS_LOGICAL_ID_CHILD(sAppData.u8AppLogicalId) || IS_LOGICAL_ID_PARENT(sAppData.u8AppLogicalId)) {
		// 子機は入力側とする

		// 入力の設定
		for (i = 0; i < u8_PORT_INPUT_COUNT; i++) {
			vPortAsInput(au8PortTbl_DIn[i]);

			// プルアップの停止
			if (IS_APPCONF_OPT_NO_PULLUP_FOR_INPUT()) {
				vPortDisablePullup(au8PortTbl_DIn[i]);
			}
		}

		// 低レイテンシで入力を行う処理(SLEEPモード時は割り込みは無視)
		if ((sAppData.sFlash.sData.u32Opt & E_APPCONF_OPT_LOW_LATENCY_INPUT)
			&& sAppData.u8Mode != E_IO_MODE_CHILD_SLP_10SEC
			&& sAppData.u8Mode != E_IO_MODE_CHILD_SLP_1SEC
		) {
			// 割り込みを有効にする
			vAHI_DioInterruptEnable(u32_PORT_INPUT_MASK, 0); // 割り込みの登録
			if (IS_APPCONF_OPT_DI_INVERT()) {
				vAHI_DioInterruptEdge(u32_PORT_INPUT_MASK, 0); // 割り込みエッジの登録(立ち上がり)
			} else {
				vAHI_DioInterruptEdge(0, u32_PORT_INPUT_MASK); // 割り込みエッジの登録(立ち下がり)
			}
		} else {
			vAHI_DioInterruptEnable(0, u32_PORT_INPUT_MASK); // 割り込みを無効化
		}

		// 送信完了状態の設定
		if (IS_APPCONF_OPT_ACK_MODE() && sAppData.u8Mode == E_IO_MODE_CHILD_SLP_10SEC) {
			// MODE7&ACKモード時には PORT_BAUD は出力ピンとして使用する
			PORT_SET_HI(PORT_BAUD);
			vPortAsOutput(PORT_BAUD);
		}

		// 出力の設定
		for (i = 0; i < u8_PORT_OUTPUT_COUNT; i++) {
			if (sAppData.sIOData_reserve.au8Output[i] == 0xFF) {
				// 始動時
				PORT_SET_HI(au8PortTbl_DOut[i]);
			} else {
				// スリープ回復後
				PORT_SET_TRUEASLO(au8PortTbl_DOut[i], sAppData.sIOData_reserve.au8Output[i]);
			}
			// 出力設定
			vPortAsOutput(au8PortTbl_DOut[i]);
		}

		// button Manager (for Input)
		sAppData.sBTM_Config.bmPortMask = u32_PORT_INPUT_MASK;
	} else {
		// 親機子機以外、何もしない
		sAppData.sBTM_Config.bmPortMask = 0UL;
	}

	// ボタン監視の有効化
	if (IS_APPCONF_OPT_WATCHDOG_OUTPUT()) {
		sAppData.sBTM_Config.bmPortMask = u32_PORT_INPUT_MASK | (1UL << PORT_EI1); // ここでは EI1 の監視も行う
	} else {
		sAppData.sBTM_Config.bmPortMask = u32_PORT_INPUT_MASK | (1UL << PORT_EI1) | (1UL << PORT_EI2); // ここでは EI1,2 の監視も行う
	}
	sAppData.sBTM_Config.u16Tick_ms = sAppData.u16ToCoNetTickDelta_ms; //低レイテンシでは1ms, デフォルトは4ms
	sAppData.sBTM_Config.u8MaxHistory = 5; //連続参照回数
	sAppData.sBTM_Config.u8DeviceTimer = 0xFF; // TickTimer を流用する。
	sAppData.pr_BTM_handler = prBTM_InitExternal(&sAppData.sBTM_Config);
	vBTM_Enable();

	// モード設定ピンで Lo になっているポートはプルアップ停止
	// Lo でない場合は、プルアップ停止をするとリーク電流が発生する
	// ※ 暗電流に神経質な mode4, 7 のみ設定する。
	// スリープ時は設定ピンを途中で変更しない前提。
	if (sAppData.u8Mode == 4) {
		vPortDisablePullup(PORT_CONF3);
	} else if (sAppData.u8Mode == 7) {
		vPortDisablePullup(PORT_CONF1);
		vPortDisablePullup(PORT_CONF2);
		vPortDisablePullup(PORT_CONF3);
	}
	if (sAppData.u8Mode == 4 || sAppData.u8Mode == 7) {
		if (sAppData.u8ChCfg & 1) {
			vPortDisablePullup(PORT_EI1);
		}
		if (sAppData.u8ChCfg & 2) {
			vPortDisablePullup(PORT_EI2);
		}
	}

	// u32HoldMask に対応するマスクを計算しておく
	{
		int i = 0, j = 1;

		sAppData.u32BtnMask_Special = 0;
		for (; i < u8_PORT_INPUT_COUNT; i++, j <<= 1) {
			if (sAppData.sFlash.sData.u32HoldMask & j)
			sAppData.u32BtnMask_Special |= (1UL << au8PortTbl_DIn[i]);
		}
	}
}


static void vChangeChannelPresetByPorts(uint32 u32Ports) {
	sAppData.u8ChCfg =
			  (u32Ports & (1UL << PORT_EI1) ? 1 : 0)
			+ (u32Ports & (1UL << PORT_EI2) ? 2 : 0);

	if (IS_APPCONF_OPT_WATCHDOG_OUTPUT()) {
		sAppData.u8ChCfg &= 1; // ２ビット目をマスクしておく
	}

	vChangeChannelPreset(sAppData.u8ChCfg);
}

/** @ingroup MASTER
 * チャネル設定を行う。
 * - EI1,EI2 の設定に基づきチャネルを変更する
 * @param u8preset
 */
static void vChangeChannelPreset(uint8 u8preset) {
	if (!IS_APPCONF_OPT_NO_C1C2_CONFIG()) {
		if (u8preset == 0) {
			// デフォルト値またはフラッシュ格納値を採用する
			sToCoNet_AppContext.u32ChMask = sAppData.sFlash.sData.u32chmask;
		} else {
			sToCoNet_AppContext.u32ChMask = au32ChMask_Preset[u8preset];
		}

		// 設定を反映する
		ToCoNet_vRfConfig();
	}
}

/** @ingroup MASTER
 * UART を初期化する
 * @param u32Baud ボーレート
 */
void vSerialInit(uint32 u32Baud, tsUartOpt *pUartOpt) {
	/* Create the debug port transmit and receive queues */
	static uint8 au8SerialTxBuffer[512];
	static uint8 au8SerialRxBuffer[512];

	/* Initialise the serial port to be used for debug output */
	sSerPort.pu8SerialRxQueueBuffer = au8SerialRxBuffer;
	sSerPort.pu8SerialTxQueueBuffer = au8SerialTxBuffer;
	sSerPort.u32BaudRate = u32Baud;
	sSerPort.u16AHI_UART_RTS_LOW = 0xffff;
	sSerPort.u16AHI_UART_RTS_HIGH = 0xffff;
	sSerPort.u16SerialRxQueueSize = sizeof(au8SerialRxBuffer);
	sSerPort.u16SerialTxQueueSize = sizeof(au8SerialTxBuffer);
	sSerPort.u8SerialPort = UART_PORT_MASTER;
	sSerPort.u8RX_FIFO_LEVEL = E_AHI_UART_FIFO_LEVEL_1;
	SERIAL_vInitEx(&sSerPort, pUartOpt);

	/* prepare stream for vfPrintf */
	sSerStream.bPutChar = SERIAL_bTxChar;
	sSerStream.u8Device = UART_PORT_MASTER;

	/* other initialization */
	INPSTR_vInit(&sSerInpStr, &sSerStream);
	memset(&sSerCmdIn, 0x00, sizeof(sSerCmdIn));

	sSerCmdIn.au8data = au8SerInBuff;
	sSerCmdIn.u16maxlen = sizeof(au8SerInBuff);
}

/** @ingroup MASTER
 * 始動時メッセージの表示を行う。
 */
void vSerInitMessage() {
	vfPrintf(&sSerStream, LB"!INF "APP_NAME" V%d-%02d-%d, SID=0x%08X, LID=0x%02x"LB,
			VERSION_MAIN, VERSION_SUB, VERSION_VAR, ToCoNet_u32GetSerial(), sAppData.u8AppLogicalId);
	if (sAppData.bFlashLoaded == 0) {
		vfPrintf(&sSerStream, "!INF Default config (no save info). .." LB);
	}
	vfPrintf(&sSerStream, "!INF DIO=%06x"LB, sAppData.u32DIO_startup);
	if (IS_APPCONF_ROLE_SILENT_MODE()) {
		vfPrintf(&sSerStream, "!ERR SILENT MODE" LB);
	}

#if 0
#warning "debug code!"
	{
		int i;
		vfPrintf(&sSerStream, "!INF INPUT(%d)", u8_PORT_INPUT_COUNT);
		for (i = 0; i < u8_PORT_INPUT_COUNT; i++) {
			vfPrintf(&sSerStream, " %02d", au8PortTbl_DIn[i]);
		}
		vfPrintf(&sSerStream, LB);
		vfPrintf(&sSerStream, "!INF OUTPUT(%d)", u8_PORT_OUTPUT_COUNT);
		for (i = 0; i < u8_PORT_OUTPUT_COUNT; i++) {
			vfPrintf(&sSerStream, " %02d", au8PortTbl_DOut[i]);
		}
		vfPrintf(&sSerStream, LB);
	}
#endif

}


/** @ingroup MASTER
 * シリアルポートからの入力を処理します。
 * - シリアルポートからの入力は uart.c/serial.c により管理される FIFO キューに値が格納されます。
 *   このキューから１バイト値を得るのが SERIAL_i16RxChar() です。
 * - 本関数では、入力したバイトに対し、アプリケーションのモードに依存した処理を行います。
 *   - 文字列入力モード時(INPSTR_ API 群、インタラクティブモードの設定値入力中)は、INPSTR_u8InputByte()
 *     API に渡す。文字列が完了したときは vProcessInputString() を呼び出し、設定値の入力処理を
 *     行います。
 *   - 上記文字列入力ではない場合は、ModBusAscii_u8Parse() を呼び出します。この関数は + + + の
 *     入力判定および : で始まる書式を認識します。
 *   - 上記書式解釈中でない場合は、vProcessInputByte() を呼び出します。この関数はインタラクティブ
 *     モードにおける１文字入力コマンドを処理します。
 *
 */
void vHandleSerialInput() {
	// handle UART command
	while (!SERIAL_bRxQueueEmpty(sSerPort.u8SerialPort)) {
		int16 i16Char;

		i16Char = SERIAL_i16RxChar(sSerPort.u8SerialPort);

		// process
		if (i16Char >=0 && i16Char <= 0xFF) {
			//DBGOUT(0, "[%02x]", i16Char);
			if (INPSTR_bActive(&sSerInpStr)) {
				// 文字列入力モード
				uint8 u8res = INPSTR_u8InputByte(&sSerInpStr, (uint8)i16Char);

				if (u8res == E_INPUTSTRING_STATE_COMPLETE) {
					vProcessInputString(&sSerInpStr);
				} else if (u8res == E_INPUTSTRING_STATE_CANCELED) {
					V_PRINT("(canceled)");
					u16HoldUpdateScreen = 64;
				}
				continue;
			}

			{
				// コマンド書式の系列解釈、および verbose モードの判定
				uint8 u8res = ModBusAscii_u8Parse(&sSerCmdIn, (uint8)i16Char);

				if (u8res != E_MODBUS_CMD_EMPTY) {
					V_PUTCHAR(i16Char);
				}

				if (u8res == E_MODBUS_CMD_COMPLETE || u8res == E_MODBUS_CMD_LRCERROR) {
					// 解釈完了

					if (u8res == E_MODBUS_CMD_LRCERROR) {
						// command complete, but CRC error
						V_PRINT(LB "!INF LRC_ERR? (might be %02X)" LB, sSerCmdIn.u8lrc);
					}

					if (u8res == E_MODBUS_CMD_COMPLETE) {
						// process command
						vProcessSerialCmd(&sSerCmdIn);
					}

					continue;
				} else
				if (u8res != E_MODBUS_CMD_EMPTY) {
					if (u8res == E_MODBUS_CMD_VERBOSE_ON) {
						// verbose モードの判定があった
						vSerUpdateScreen();
					}

					if (u8res == E_MODBUS_CMD_VERBOSE_OFF) {
						vfPrintf(&sSerStream, "!INF EXIT INTERACTIVE MODE."LB);
					}

					// still waiting for bytes.
					continue;
				} else {
					; // コマンド解釈モードではない
				}
			}

			// Verbose モードのときは、シングルコマンドを取り扱う
			if (sSerCmdIn.bverbose) {
				vProcessInputByte(i16Char);
			}
		}
	}
}

/** @ingroup MASTER
 * シリアルから入力されたコマンド形式の電文を処理します。
 *
 * - 先頭バイトがアドレス指定で、0xDB 指定の場合、自モジュールに対しての指令となります。
 * - ２バイト目がコマンドで、0x80 以降を指定します。0x7F 以下は特別な処理は割り当てられていません。
 * - コマンド(0xDB向け)
 *   - SERCMD_ID_GET_MODULE_ADDRESS\n
 *     モジュールのシリアル番号を取得する
 * - コマンド(外部アドレス向け)
 *   - SERCMD_ID_REQUEST_IO_DATA\n
 *     IO状態の設定
 *   - それ以外のコマンドID\n
 *     通常送信 (ただし 0x80 以降は今後の機能追加で意味が変わる可能性あり)
 *
 * @param pSer シリアルコマンド入力の管理構造体
 */
void vProcessSerialCmd(tsModbusCmd *pSer) {
	uint8 *p = pSer->au8data;

	uint8 u8addr; // 送信先論理アドレス
	uint8 u8cmd; // コマンド

	uint8 *p_end;
	p_end = &(pSer->au8data[pSer->u16len]); // the end points 1 byte after the data end.

	// COMMON FORMAT
	OCTET(u8addr); // [1] OCTET : 論理ID
	OCTET(u8cmd); // [1] OCTET : 要求番号

	DBGOUT(3, "* UARTCMD ln=%d cmd=%02x req=%02x %02x%0x2%02x%02x..." LB,
			pSer->u16len,
			u8addr,
			u8cmd,
			*p,
			*(p+1),
			*(p+2),
			*(p+3)
			);

	if (u8addr == SERCMD_ADDR_TO_MODULE) {
		/*
		 * モジュール自身へのコマンド (0xDB)
		 */
		switch(u8cmd) {
		case SERCMD_ID_GET_MODULE_ADDRESS:
			vModbOut_MySerial(&sSerStream);
			break;

		default:
			break;
		}
	} else {
		/*
		 * 外部アドレスへの送信(IO情報の設定要求)
		 */
		if (u8cmd == SERCMD_ID_REQUEST_IO_DATA) {
			/*
			 * OCTET: 書式 (0x01)
			 * OCTET: 出力状態
			 * OCTET: 出力状態マスク
			 */
			uint8 u8format = G_OCTET();

			if (u8format == 0x01) {
				tsIOSetReq sIOreq;
				memset(&sIOreq, 0, sizeof(tsIOSetReq));

				sIOreq.u16IOports = G_BE_WORD();
				sIOreq.u16IOports_use_mask = G_BE_WORD();

				int i;

				for (i = 0; i < 4; i++) {
					(void)G_BE_WORD();
				}

				if (p > p_end) return; // v1.1.3 (終端チェック)

				DBGOUT(1, "SERCMD:IO REQ: %02x %02x"LB,
						sIOreq.u16IOports,
						sIOreq.u16IOports_use_mask
						);

				i16TransmitIoSettingRequest(u8addr, &sIOreq);
			}

			return;
		} else {
			int ilen = p_end - p;

			if (ilen > 0 && ilen <= 80) {
				i16TransmitSerMsg(u8addr, u8cmd, p, p_end - p);
			} else {
				DBGOUT(3, "* SERMSG: invalid length (%d)", ilen);
			}
		}
	}
}

/** @ingroup MASTER
 * 重複パケットの判定。タイムスタンプの比較で、ごく最近であって旧いパケットを除外する。
 *
 * - 注意点
 *   - DUPCHK_vInin() の初期化を行うこと
 *   - DUPCHK_bFIND(0,NULL) を一定周期で呼び出すこと
 *
 * @param pc 管理構造体
 * @param u32Addr
 * @param u16TimeStamp
 * @return TRUE:重複している FALSE:新しいパケットが来た
 */
static bool_t bCheckDupPacket(tsDupChk_Context *pc, uint32 u32Addr, uint16 u16TimeStamp) {
	uint32 u32Key;
	if (DUPCHK_bFind(pc, u32Addr, &u32Key)) {
		// 最後に受けたカウンタの値が得られるので、これより新しい
		uint16 u16Delta = ((uint16)u32Key - (u16TimeStamp&0x7FFF)) & 0x7FFF; // 最上位ビットは設定されない
		//vfPrintf(&sSerStream, LB"%d - %d = %d", u32Key, u16TimeStamp&0x7FFF, u16Delta);
		if (u16Delta < 256) { // 32count=500ms, 500ms の遅延は想定外だが。
			// すでに処理したパケット
			return TRUE;
		}
	}

	// 新しいパケットである（時間情報を格納する）
	DUPCHK_vAdd(pc, u32Addr, u16TimeStamp);
	return FALSE;
}

/** @ingroup MASTER
 * IO 情報を送信します。
 *
 * - IO状態の変化、および１秒置きの定期送時に呼び出されます。
 *
 * - Packet 構造
 *   - OCTET: 識別ヘッダ(APP ID より生成)
 *   - OCTET: プロトコルバージョン(バージョン間干渉しないための識別子)
 *   - OCTET: 送信元論理ID
 *   - BE_DWORD: 送信元のシリアル番号
 *   - OCTET: 宛先論理ID
 *   - BE_WORD: 送信タイムスタンプ (64fps カウンタの値の下１６ビット, 約1000秒で１周する)
 *   - OCTET: 中継フラグ(中継したパケットは１、最初の送信なら０を格納）
 *   - BE_WORD: ポート状態 (LSB から順に I1, I2, ... I12, 1=Lo)
 *   - BE_WORD: 設定ポート (LSB から順に I1, I2, ... I12, 1=変化) 本値が１のポートを設定対象とする
 *
 * @param bQuick TRUEなら遅延無しで送信しようと試みる
 * @returns -1:ERROR, 0..255 CBID
 */
static int16 i16TransmitIoData(uint8 u8Quick) {
	if(IS_APPCONF_ROLE_SILENT_MODE()) return -1;

	int16 i16Ret = -1;
	tsTxDataApp sTx;
	memset(&sTx, 0, sizeof(sTx));

	uint8 *q = sTx.auData;

	uint8 u8stat = 0; // 割り込み要因などステータス情報を付加する

	// ペイロードを構成
	S_OCTET(sAppData.u8AppIdentifier);
	S_OCTET(APP_PROTOCOL_VERSION);
	S_OCTET(sAppData.u8AppLogicalId); // アプリケーション論理アドレス
	S_BE_DWORD(ToCoNet_u32GetSerial());  // シリアル番号
	S_OCTET(IS_LOGICAL_ID_PARENT(sAppData.u8AppLogicalId) ? LOGICAL_ID_CHILDREN : LOGICAL_ID_PARENT); // 宛
	uint16 u16ts = (sAppData.u32CtTimer0 & 0x7FFF) + ((u8Quick & TX_OPT_QUICK_BIT) ? 0x8000 : 0);
	S_BE_WORD(u16ts); // タイムスタンプ
		// bQuick 転送する場合は MSB をセットし、優先パケットである処理を行う
	S_OCTET(0); // 中継フラグ

	// 割り込み送信
	bool_t bDiInt = (sAppData.bWakeupByButton & 0x01);
	bool_t bWkFst = (sAppData.bWakeupByButton & 0x80);
	sAppData.bWakeupByButton &= 0x7F; // ここでこのフラグをクリアしておく

	// IOの状態
	{
		int i;

		// ボタン状態ビット
		uint16 u16bm = 0x0000;

		for (i = 0; i < u8_PORT_INPUT_COUNT; i++) {
			uint8 u8ct = sAppData.sIOData_now.au8Input[i] >> 4; // カウンタ値の取り出し

			if (u8ct >= LOW_LATENCY_DELAYED_TRANSMIT_COUNTER - 3) {
				// カウンタ値が残っている間は１とする(チャタリングが起きているかもしれないので、押したと判断する）
				u16bm |= (1 << i);
			} else {
				// カウンタ値残存期間が過ぎたら BTM による値を採用する
				u16bm |= (sAppData.sIOData_now.au8Input[i] & 1) ? (1 << i) : 0;
			}
		}

		// IOの使用フラグ
		//   通常： 一度でも設定したポートは対象とする
		uint16 u16bm_used = u16bm;
		uint16 u16int = 0;
		for (i = 0; i < u8_PORT_INPUT_COUNT; i++) {
			uint32 u32mask = (1UL << au8PortTbl_DIn[i]);
			if (sAppData.sIOData_now.u32BtmUsed & u32mask) {
				u16bm_used |= (1UL << i);
			}
			if (sAppData.sIOData_now.u32BtmChanged & u32mask) {
				u16int |= (1UL << i);
			}
		}

		//vfPrintf(&sSerStream, LB"ts=%d : %08X, %08X, %08X", u16ts, u16bm, u16bm_used, u16int );

		// ビットマスクの計算
#if 0
		uint32 u32mask =
				bDiInt ?
				  sAppData.sIOData_now.u32BtmChanged // 低レイテンシモードで割り込み起床時は割り込み要因を採用する
				: sAppData.sIOData_now.u32BtmUsed;
		for (i = 0; i < u8_PORT_INPUT_COUNT; i++) {
			if (u32mask & (1UL << au8PortTbl_DIn[i])) {
				u16bm_used |= (1UL << i);
			}
		}
#endif

		// ペイロードへの格納
		S_BE_WORD(u16bm);
		S_BE_WORD(u16bm_used);
		S_BE_WORD(u16int);

		// ステータスビット (bit0: 割り込み要因, bit1: 応答要求, bit7: POR)
		u8stat |= bDiInt ? 0x01 : 0;
		u8stat |= (u8Quick & TX_OPT_BY_INT ? 0x01 : 0);
		u8stat |= (u8Quick & TX_OPT_RESP_BIT ? 0x02 : 0);
		u8stat |= bWkFst ? 0x80 : 0;
		S_OCTET(u8stat);
	}


	// 送信情報の構築
	sTx.u8Len = q - sTx.auData; // パケット長
	sTx.u8Cmd = TOCONET_PACKET_CMD_APP_USER_IO_DATA; // パケット種別

	if (IS_APPCONF_OPT_ACK_MODE() && IS_LOGICAL_ID_CHILD(sAppData.u8AppLogicalId)) {
		sTx.u32DstAddr  = SERCMD_ADDR_CONV_TO_SHORT_ADDR(LOGICAL_ID_PARENT); // 親機宛に送信(Ack付き)
		sTx.u8Retry     = sAppData.u8StandardTxAckRetry; // アプリ再送は１回のみ
		sTx.bAckReq = TRUE;
	} else {
		sTx.u32DstAddr  = TOCONET_MAC_ADDR_BROADCAST; // ブロードキャスト
		sTx.u8Retry     = sAppData.u8StandardTxRetry; // 再送回数
		sTx.bAckReq     = FALSE;
	}

	// フレームカウントとコールバック識別子の指定
	sAppData.u16TxFrame++;
	sTx.u8Seq = (sAppData.u16TxFrame & 0xFF);
	sTx.u8CbId = sTx.u8Seq;

	{
		/* MAC モードでは細かい指定が可能 */
		sTx.bSecurePacket = IS_CRYPT_MODE() ? TRUE : FALSE;
		sTx.u32SrcAddr = sToCoNet_AppContext.u16ShortAddress;

		// 送信タイミングの調整
		if (u8Quick & TX_OPT_SMALLDELAY_BIT) {
			sTx.u16RetryDur = 8; // 再送間隔
			sTx.u16DelayMin = 8; // 衝突を抑制するため送信タイミングを遅らせる
			sTx.u16DelayMax = 16; // 衝突を抑制するため送信タイミングを遅らせる
		} else if (u8Quick & TX_OPT_NODELAY_BIT) {
			sTx.u16RetryDur = 0; // 再送間隔
			sTx.u16DelayMin = 0; // すみやかに送信する
			sTx.u16DelayMax = 0; // すみやかに送信する
		} else {
			sTx.u16RetryDur = 4; // 再送間隔
			sTx.u16DelayMin = 0; // 衝突を抑制するため送信タイミングを遅らせる
			sTx.u16DelayMax = 16; // 衝突を抑制するため送信タイミングにブレを作る(最大16ms)
		}

		// 送信API
		if (ToCoNet_bMacTxReq(&sTx)) {
			ToCoNet_Tx_vProcessQueue();

			i16Ret = sTx.u8CbId;
			sAppData.sIOData_now.u32TxLastTick = u32TickCount_ms;
		}
	}

	return i16Ret;
}

/** @ingroup MASTER
 * IOデータを中継送信します。
 *
 * - パケット中の中継フラグのビットは、呼び出し前に設定されています。
 * - 衝突を抑制出来る程度の送信遅延、再送間隔を設定しています。
 *
 * @param pRx 受信したときのデータ
 * @return -1:Error, 0..255:CbId
 */
int16 i16TransmitRepeat(tsRxDataApp *pRx) {
	if(IS_APPCONF_ROLE_SILENT_MODE()) return -1;

	int16 i16Ret = -1;

	tsTxDataApp sTx;
	memset(&sTx, 0, sizeof(sTx));

	// Payload
	memcpy(sTx.auData, pRx->auData, pRx->u8Len);
	sTx.u8Len = pRx->u8Len;

	// コマンド設定
	sTx.u8Cmd = pRx->u8Cmd; // パケット種別

	// 送信する
	sTx.u32DstAddr  = TOCONET_MAC_ADDR_BROADCAST; // ブロードキャスト
	sTx.u8Retry     = sAppData.u8StandardTxRetry; // 再送回数

	// フレームカウントとコールバック識別子の指定
	sAppData.u16TxFrame++;
	sTx.u8Seq = (sAppData.u16TxFrame & 0xFF);
	sTx.u8CbId = sTx.u8Seq;

	// 中継時の送信パラメータ
	sTx.bAckReq = FALSE;
	sTx.bSecurePacket = IS_CRYPT_MODE() ? TRUE : FALSE;
	sTx.u32SrcAddr = sToCoNet_AppContext.u16ShortAddress;

	sTx.u16RetryDur = 8; // 再送間隔
	sTx.u16DelayMin = 8; // 衝突を抑制するため送信タイミングにブレを作る(最大16ms)
	sTx.u16DelayMax = 16; // 衝突を抑制するため送信タイミングにブレを作る(最大16ms)

	// 送信API
	if (ToCoNet_bMacTxReq(&sTx)) {
		i16Ret = sTx.u8CbId;
	}

	return i16Ret;
}


/** @ingroup MASTER
 * IO(DO/PWM)を設定する要求コマンドパケットを送信します。
 *
 * - Packet 構造
 *   - OCTET: 識別ヘッダ(APP ID より生成)
 *   - OCTET: プロトコルバージョン(バージョン間干渉しないための識別子)
 *   - OCTET: 送信元論理ID
 *   - BE_DWORD: 送信元のシリアル番号
 *   - OCTET: 宛先論理ID
 *   - BE_WORD: 送信タイムスタンプ (64fps カウンタの値の下１６ビット, 約1000秒で１周する)
 *   - OCTET: 中継フラグ
 *   - OCTET: 形式 (1固定)
 *   - OCTET: ボタン (LSB から順に SW1 ... SW4, 1=Lo)
 *   - OCTET: ボタン使用フラグ (LSB から順に SW1 ... SW4, 1=このポートを設定する)
 *
 * @param u8DstAddr 送信先
 * @param pReq 設定データ
 * @return -1:Error, 0..255:CbId
 */
int16 i16TransmitIoSettingRequest(uint8 u8DstAddr, tsIOSetReq *pReq) {
	if(IS_APPCONF_ROLE_SILENT_MODE()) return -1;

	int16 i16Ret;

	tsTxDataApp sTx;
	memset(&sTx, 0, sizeof(sTx));

	uint8 *q = sTx.auData;

	// ペイロードの構築
	S_OCTET(sAppData.u8AppIdentifier);
	S_OCTET(APP_PROTOCOL_VERSION);
	S_OCTET(sAppData.u8AppLogicalId); // アプリケーション論理アドレス
	S_BE_DWORD(ToCoNet_u32GetSerial());  // シリアル番号
	S_OCTET(u8DstAddr); // 宛先
	S_BE_WORD(sAppData.u32CtTimer0 & 0xFFFF); // タイムスタンプ
	S_OCTET(0); // 中継フラグ

	S_OCTET(1); // パケット形式

	// DIO の設定
	S_BE_WORD(pReq->u16IOports);
	S_BE_WORD(pReq->u16IOports_use_mask);

	// 各種送信情報
	sTx.u8Len = q - sTx.auData; // パケット長
	sTx.u8Cmd = TOCONET_PACKET_CMD_APP_USER_IO_DATA_EXT; // パケット種別

	// 送信する
	sTx.u32DstAddr  = TOCONET_MAC_ADDR_BROADCAST; // ブロードキャスト
	sTx.u8Retry     = sAppData.u8StandardTxRetry; // 再送

	{
		/* 送信設定 */
		sTx.bAckReq = FALSE;
		sTx.bSecurePacket = IS_CRYPT_MODE() ? TRUE : FALSE;
		sTx.u32SrcAddr = sToCoNet_AppContext.u16ShortAddress;
		sTx.u16RetryDur = 4; // 再送間隔
		sTx.u16DelayMax = 16; // 衝突を抑制するため送信タイミングにブレを作る(最大16ms)

		// 送信API
		if (ToCoNet_bMacTxReq(&sTx)) {
			i16Ret = sTx.u8CbId;
		}
	}

	return i16Ret;
}

/** @ingroup MASTER
 * シリアルコマンドを送信します
 *
 * - Packet 構造
 *   - OCTET: 識別ヘッダ(APP ID より生成)
 *   - OCTET: プロトコルバージョン(バージョン間干渉しないための識別子)
 *   - OCTET: 送信元論理ID
 *   - BE_DWORD: 送信元のシリアル番号
 *   - OCTET: 宛先論理ID
 *   - BE_WORD: 送信タイムスタンプ (64fps カウンタの値の下１６ビット, 約1000秒で１周する)
 *   - OCTET: 中継フラグ
 *   - OCTET: コマンド名
 *   - OCTET[]: データ
 *
 * @param u8DstAddr
 * @param pDat 送信データ
 * @param u8len 送信データ長
 * @return -1:Error, 0..255:CbId
 */
static int16 i16TransmitSerMsg(uint8 u8DstAddr, uint8 u8Cmd, uint8 *pDat, uint8 u8len) {
	if(IS_APPCONF_ROLE_SILENT_MODE()) return -1;
	if (u8len > 80) { return -1; }

	int16 i16Ret;

	tsTxDataApp sTx;
	memset(&sTx, 0, sizeof(sTx));

	uint8 *q = sTx.auData;

	// ペイロードの構築
	S_OCTET(sAppData.u8AppIdentifier);
	S_OCTET(APP_PROTOCOL_VERSION);
	S_OCTET(sAppData.u8AppLogicalId); // アプリケーション論理アドレス
	S_BE_DWORD(ToCoNet_u32GetSerial());  // シリアル番号
	S_OCTET(u8DstAddr); // 宛先
	S_BE_WORD(sAppData.u32CtTimer0 & 0xFFFF); // タイムスタンプ
	S_OCTET(0); // 中継フラグ

	S_OCTET(u8Cmd);
	memcpy(q, pDat, u8len); q += u8len; // データ中身

	// 各種送信情報
	sTx.u8Len = q - sTx.auData; // パケット長
	sTx.u8Cmd = TOCONET_PACKET_CMD_APP_USER_SERIAL_MSG; // パケット種別

	// 送信する
	if (IS_APPCONF_OPT_ACK_MODE() && (u8DstAddr != LOGICAL_ID_CHILDREN)) {
		sTx.bAckReq     = TRUE;
		sTx.u32DstAddr  = SERCMD_ADDR_CONV_TO_SHORT_ADDR(LOGICAL_ID_PARENT); // 指定子機アドレス宛て
		sTx.u8Retry     = sAppData.u8StandardTxAckRetry; // 再送
		sTx.u16RetryDur = (sToCoNet_AppContext.u8TxMacRetry + 1) * 6; // 再送間隔
	} else {
		sTx.bAckReq     = FALSE;
		sTx.u32DstAddr  = TOCONET_MAC_ADDR_BROADCAST; // ブロードキャスト
		sTx.u8Retry     = sAppData.u8StandardTxRetry; // 再送
		sTx.u16RetryDur = 4; // 再送間隔
	}

	{
		/* 送信設定 */
		sTx.bSecurePacket = IS_CRYPT_MODE() ? TRUE : FALSE;
		sTx.u32SrcAddr = sToCoNet_AppContext.u16ShortAddress; // ペイロードにアドレスが含まれるのでショートアドレス指定
		sTx.u16DelayMax = 16; // 衝突を抑制するため送信タイミングにブレを作る(最大16ms)

		// 送信API
		if (ToCoNet_bMacTxReq(&sTx)) {
			i16Ret = sTx.u8CbId;
		}
	}

	return i16Ret;
}

/** @ingroup MASTER
 * IO状態パケットの受信処理を行います。
 *
 * - 受信したデータに格納されるIO設定要求に従いIO値(DO/PWM)を設定します。
 * - 受信したデータを UART に出力します。
 * - 中継機の場合、中継パケットを送信します。
 * - 低遅延で送信されてきたパケットの TimeStamp の MSB には１が設定される。
 *   このパケットは、タイムスタンプによる重複除去アルゴリズムとは独立して
 *   処理される。
 * - IOの設定は本受信関数でのみ行われる。
 * - UART 出力書式
 *   OCTET: 送信元ID
 *   OCTET: 0x81
 *   OCTET: ペイロードの長さ(このバイトも含む)
 *   OCTET: 0x01
 *   OCTET: LQI値
 *   BE_DWORD: 送信元アドレス
 *   OCTET: 送信先アドレス
 *   BW_WORD: タイムスタンプ (MSB 0x8000 が設定されているときはクイック送信)
 *   OCTET: 中継フラグ (中継されていないパケットなら 0)
 *   BE_WORD: ボタン状態 (1=Lo, 0=Hi, LSB から順に DI1...)
 *            ボタンの有効マスクが 1 のビットに限り意味のある情報となる。
 *   BE_WORD: ボタンの有効マスク (1=有効 0=無効, LSBから順に DI1...)
 *
 * @param pRx 受信データ
 */
static void vReceiveIoData(tsRxDataApp *pRx) {
	int i, j;
	uint8 *p = pRx->auData;

	uint8 u8AppIdentifier = G_OCTET();
	if (u8AppIdentifier != sAppData.u8AppIdentifier) return;

	uint8 u8PtclVersion = G_OCTET();
	if (u8PtclVersion != APP_PROTOCOL_VERSION) return;

	uint8 u8AppLogicalId = G_OCTET();

	uint32 u32Addr = G_BE_DWORD();

	uint8 u8AppLogicalId_Dest = G_OCTET(); (void)u8AppLogicalId_Dest;

	uint16 u16TimeStamp = G_BE_WORD();

	/* 重複の確認を行う */
	bool_t bQuick = u16TimeStamp & 0x8000 ? TRUE : FALSE; // 優先パケット（全部処理する）
	u16TimeStamp &= 0x7FFF;
	if (bQuick == FALSE && bCheckDupPacket(&sDupChk_IoData, u32Addr, u16TimeStamp)) {
		return;
	}
	static uint32 u32LastQuick;
	if (bQuick) {
		if ((u32TickCount_ms - u32LastQuick) < 20) {
			// Quickパケットを受けて一定期間未満のパケットは無視する
			return;
		} else {
			u32LastQuick = u32TickCount_ms; // タイムスタンプを巻き戻す
		}
	}

	// 中継フラグ
	uint8 u8TxFlag = G_OCTET();
	// 中継の判定 (このフラグが１なら中継済みのパケット）
	if (u8TxFlag == 0 && sAppData.u8Mode == E_IO_MODE_ROUTER) {

		// リピータの場合はここで中継の判定
		*(p-1) = 1; // 中継済みフラグのセット

		// 中継する
		i16TransmitRepeat(pRx);
		return;
	}

	// 親機子機の判定
	bool_t bSetIo = FALSE;
	if (	(IS_LOGICAL_ID_PARENT(u8AppLogicalId) && IS_LOGICAL_ID_CHILD(sAppData.u8AppLogicalId))
		||	(IS_LOGICAL_ID_CHILD(u8AppLogicalId) && IS_LOGICAL_ID_PARENT(sAppData.u8AppLogicalId)) ) {
		bSetIo = TRUE; // 親機⇒子機、または子機⇒親機への伝送
	}

	/* BUTTON & STATUS */
	uint16 u16ButtonState = G_BE_WORD();
	uint16 u16ButtonChanged = G_BE_WORD();
	uint16 u16ButtonInt = G_BE_WORD(); (void)u16ButtonInt;
	uint8 u8stat = G_OCTET();

	if (bSetIo) {
		// ポートの値を設定する（変更フラグのあるものだけ）
		for (i = 0, j = 1; i < u8_PORT_OUTPUT_COUNT; i++, j <<= 1) {
			if (u16ButtonChanged & j) {
				if (   ((j & sAppData.sFlash.sData.u32HoldMask) && !IS_APPCONF_OPT_ON_PRESS_TRANSMIT()) // 通常モード時は、対象ポートをホールドする
					|| (!(j & sAppData.sFlash.sData.u32HoldMask) && IS_APPCONF_OPT_ON_PRESS_TRANSMIT()) // OnPressTransmit モードでは、対象ポート以外はホールドする
				) {
					// hold モードの処理
					if (u16ButtonState & j) {
						// Hi>Loへの遷移のみ対象とする
						MICRO_INT_STORAGE;
						MICRO_INT_ENABLE_ONLY(0); // 割り込み禁止
						sAppData.au16HoldBtn[i] = IS_APPCONF_OPT_ON_PRESS_TRANSMIT()
								? 50 : sAppData.sFlash.sData.u16HoldDur_ms;
								// ホールド時間を設定(割り込みハンドラでHiの制御を実施)
								// ON_PRESS_TRANSMIT 時で、対象ポートでない場合は 50ms 固定とする(mode7 の親和性を優先)
						PORT_SET_LO(au8PortTbl_DOut[i]); // Lo に落とす
						sAppData.sIOData_now.au8Output[i] = TRUE;
						MICRO_INT_RESTORE_STATE(); // 割り込み解除
					}
				} else {
					bool_t bState = ((u16ButtonState & j) != 0);
					sAppData.sIOData_now.au8Output[i] = bState;
					PORT_SET_TRUEASLO(au8PortTbl_DOut[i], bState);
				}
			}
		}

		// タイムスタンプの保存
		sAppData.sIOData_now.u32RxLastTick = u32TickCount_ms;
	}

	/* 応答処理が必要な場合 */
	if ((u8stat & 0x2) && IS_LOGICAL_ID_PARENT(sAppData.u8AppLogicalId)) { // 応答要求あり
		i16TransmitIoData(TX_OPT_RESP_BIT | TX_OPT_SMALLDELAY_BIT);
	}

	//vfPrintf(&sSerStream, "ts=%d ", u16TimeStamp);
	/* UART 出力 */
	if (bSetIo && !sSerCmdIn.bverbose && !IS_APPCONF_OPT_CHILD_RECV_NO_IO_DATA()) {
		// 以下のようにペイロードを書き換えて UART 出力
		pRx->auData[0] = pRx->u8Len; // １バイト目はバイト数
		pRx->auData[2] = pRx->u8Lqi; // ３バイト目(もともとは送信元の LogicalID) は LQI

		vSerOutput_ModbusAscii(&sSerStream, u8AppLogicalId, SERCMD_ID_INFORM_IO_DATA, pRx->auData, pRx->u8Len);
	}

	/* イベントを伝達 */
	ToCoNet_Event_Process(E_EVENT_APP_RX_COMPLETE, (uint32)(void*)pRx, sAppData.prPrsEv);
}

/** @ingroup MASTER
 * IO状態の設定要求を行う UART メッセージから送信されてきたパケットの処理を行います。
 * vReceiveIoData() と大まかな処理は同じですが、PWMの設定に違いが有るので独立した
 * 関数に記述しています。
 *
 * @param pRx 受信したときのデータ
 */
static void vReceiveIoSettingRequest(tsRxDataApp *pRx) {
	int i, j;
	uint8 *p = pRx->auData;

	uint8 u8AppIdentifier = G_OCTET();
	if (u8AppIdentifier != sAppData.u8AppIdentifier) return;

	uint8 u8PtclVersion = G_OCTET();
	if (u8PtclVersion != APP_PROTOCOL_VERSION) return;

	uint8 u8AppLogicalId = G_OCTET();

	uint32 u32Addr = G_BE_DWORD();

	uint8 u8AppLogicalId_Dest = G_OCTET();

	uint16 u16TimeStamp = G_BE_WORD();

	/* 重複の確認を行う */
	if (bCheckDupPacket(&sDupChk_IoData, u32Addr, u16TimeStamp)) {
		return;
	}

	uint8 u8TxFlag = G_OCTET();

	// 中継の判定 (このフラグが１なら中継済みのパケット）
	if (u8TxFlag == 0 && sAppData.u8Mode == E_IO_MODE_ROUTER) {
		// リピータの場合はここで中継の判定
		*(p-1) = 1; // 中継済みフラグのセット

		i16TransmitRepeat(pRx);
		return;
	}

	// 親機子機の判定
	if (IS_LOGICAL_ID_CHILD(sAppData.u8AppLogicalId)) {
		// 子機の場合は、任意の送り主から受けるが、送り先が CHILDREN(120) またはアドレスが一致している事
		if (!(u8AppLogicalId_Dest == sAppData.u8AppLogicalId || u8AppLogicalId_Dest == LOGICAL_ID_CHILDREN)) {
			return;
		}
	} else if(IS_LOGICAL_ID_PARENT(sAppData.u8AppLogicalId)) {
		// 親機の場合は、子機からの送信である事
		if (!(u8AppLogicalId_Dest == LOGICAL_ID_PARENT && IS_LOGICAL_ID_CHILD(u8AppLogicalId))) {
			return;
		}
	} else {
		// それ以外は処理しない
		return;
	}

	/* 書式 */
	uint8 u8Format = G_OCTET();

	if (u8Format == 1) {
		/* BUTTON */
		uint16 u16ButtonState = G_BE_WORD();
		uint16 u16ButtonChanged = G_BE_WORD();
		// ポートの値を設定する（変更フラグのあるものだけ）
		for (i = 0, j = 1; i < u8_PORT_OUTPUT_COUNT; i++, j <<= 1) {
			if (u16ButtonChanged & j) {
				PORT_SET_TRUEASLO(au8PortTbl_DOut[i], u16ButtonState & j);
				sAppData.sIOData_now.au8Output[i] = ((u16ButtonState & j) != 0);
			}
		}

		DBGOUT(1, "RECV:IO REQ: %02x %02x"LB,
				u16ButtonState,
				u16ButtonChanged
				);
	}
}

/** @ingroup MASTER
 * IO状態の設定要求を行う UART メッセージから送信されてきたパケットの処理を行います。
 * vReceiveIoData() と大まかな処理は同じですが、PWMの設定に違いが有るので独立した
 * 関数に記述しています。
 *
 * @param pRx 受信したときのデータ
 */
static void vReceiveSerialMsg(tsRxDataApp *pRx) {
	uint8 *p = pRx->auData;
	uint8 *p_end = pRx->auData + pRx->u8Len;

	uint8 u8AppIdentifier = G_OCTET();
	if (u8AppIdentifier != sAppData.u8AppIdentifier) return;

	uint8 u8PtclVersion = G_OCTET();
	if (u8PtclVersion != APP_PROTOCOL_VERSION) return;

	uint8 u8AppLogicalId = G_OCTET();

	uint32 u32Addr = G_BE_DWORD();

	uint8 u8AppLogicalId_Dest = G_OCTET();

	uint16 u16TimeStamp = G_BE_WORD();

	/* 重複の確認を行う */
	if (bCheckDupPacket(&sDupChk_IoData, u32Addr, u16TimeStamp)) {
		return;
	}

	uint8 u8TxFlag = G_OCTET();

	// 中継の判定 (このフラグが１なら中継済みのパケット）
	if (u8TxFlag == 0 && sAppData.u8Mode == E_IO_MODE_ROUTER) {
		// リピータの場合はここで中継の判定
		*(p-1) = 1; // 中継済みフラグのセット

		i16TransmitRepeat(pRx);
		return;
	}

	// 親機子機の判定
	if (IS_LOGICAL_ID_CHILD(sAppData.u8AppLogicalId)) {
		// 子機の場合は、任意の送り主から受けるが、送り先が CHILDREN(120) またはアドレスが一致している事
		if (!(u8AppLogicalId_Dest == sAppData.u8AppLogicalId || u8AppLogicalId_Dest == LOGICAL_ID_CHILDREN)) {
			return;
		}
	} else if(IS_LOGICAL_ID_PARENT(sAppData.u8AppLogicalId)) {
		// 親機の場合は、子機からの送信である事
		if (!(u8AppLogicalId_Dest == LOGICAL_ID_PARENT && IS_LOGICAL_ID_CHILD(u8AppLogicalId))) {
			return;
		}
	} else {
		// それ以外は処理しない
		return;
	}

	/* ペイロードの処理 */
	uint8 u8Cmd = G_OCTET();

	if (p < p_end) {
		uint8 u8paylen = p_end - p;
		if (u8Cmd == SERCMD_ID_SERCMD_EX_SIMPLE) {
			uint8 au8buff2[256], *q = au8buff2;

			S_OCTET(0x00); // 応答ID（指定なし）
			S_BE_DWORD(u32Addr); // 送信元アドレス
			S_BE_DWORD(0xFFFFFFFF); // 送信先アドレス（指定無し）
			S_OCTET(pRx->u8Lqi);
			S_BE_WORD(u8paylen);
			memcpy(q, p, u8paylen); q += u8paylen;

			vSerOutput_ModbusAscii(&sSerStream, u8AppLogicalId, SERCMD_ID_SERCMD_EX_SIMPLE, au8buff2, q - au8buff2);
		} else {
			vSerOutput_ModbusAscii(&sSerStream, u8AppLogicalId, u8Cmd, p, p_end - p);
		}
	}
}

/** @ingroup MASTER
 * スリープ状態に遷移します。
 *
 * @param u32SleepDur_ms スリープ時間[ms]
 * @param bPeriodic TRUE:前回の起床時間から次のウェイクアップタイミングを計る
 * @param bDeep TRUE:RAM OFF スリープ
 */
static void vSleep(uint32 u32SleepDur_ms, bool_t bPeriodic, bool_t bDeep, bool_t bNoIoInt) {
	// IO 情報の保存
	memcpy(&sAppData.sIOData_reserve, &sAppData.sIOData_now, sizeof(tsIOData));

	// stop interrupt source, if interrupt source is still running.
	vAHI_DioInterruptEnable(0, u32_PORT_INPUT_MASK); // 割り込みの解除）

	// set UART Rx port as interrupt source
	vAHI_DioSetDirection(u32_PORT_INPUT_MASK, 0); // set as input

	// clear interrupt register
	(void)u32AHI_DioInterruptStatus();

	if (!bNoIoInt) {
		vAHI_DioWakeEnable(u32_PORT_INPUT_MASK, 0); // also use as DIO WAKE SOURCE

		if (IS_APPCONF_OPT_DI_INVERT()) {
			vAHI_DioWakeEdge(u32_PORT_INPUT_MASK, 0); // 割り込みエッジ（立上りに設定）
		} else {
			vAHI_DioWakeEdge(0, u32_PORT_INPUT_MASK); // 割り込みエッジ（立下りに設定）
		}
	} else {
		vAHI_DioWakeEnable(0, u32_PORT_INPUT_MASK); // 割り込み禁止
	}

	// wake up using wakeup timer as well.
	ToCoNet_vSleep(bPeriodic ? E_AHI_WAKE_TIMER_0 : E_AHI_WAKE_TIMER_1, u32SleepDur_ms, bPeriodic, bDeep); // PERIODIC RAM OFF SLEEP USING WK0
}

void cbSaveDstAppID( tsPairingInfo* psPairingInfo )
{
	if( psPairingInfo->bStatus ){
		uint8 au8conf[16], *q = au8conf;

		// 交換した APP ID を書き込む
		S_OCTET(E_APPCONF_APPID);
		S_BE_DWORD(psPairingInfo->u32PairKey);
		Config_bSetModuleParam(au8conf, q - au8conf);

		// 設定情報を書き込む
		Config_vSave();

		vfPrintf(&sSerStream, LB"!EXIT PAIRING MODE" LB);
		vfPrintf(&sSerStream,"!PLEASE RESET SYSTEM..." LB);
	}else{
		vfPrintf(&sSerStream, LB"!PAIRING FAILED" LB);
		vfPrintf(&sSerStream,"!PLEASE RESET SYSTEM..." LB);
	}
	return;
}
/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/
