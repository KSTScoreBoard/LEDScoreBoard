/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */

// 本ファイルは、Interactive.h からインクルードされる

/*************************************************************************
 * VERSION
 *************************************************************************/

#if defined(PARENT)
# define VERSION_CODE 1
#elif defined(ROUTER)
# define VERSION_CODE 2
#elif defined(ENDDEVICE)
# define VERSION_CODE 3
#else
# error "VERSION_CODE is not defined"
#endif

/*************************************************************************
 * OPTION 設定
 *************************************************************************/
/**
 * 定義済みの場合、各ルータまたは親機宛に送信し、ルータから親機宛のパケットを
 * 送信する受信したルータすべての情報が親機に転送されることになる。
 * この場合、複数の受信パケットを分析する事で一番近くで受信したルータを特定
 * できる。
 *
 * 未定義の場合、直接親機宛の送信が行われる。原則として受信パケットを一つだけ
 * 親機が表示することになり、近接ルータの特定はできない。
 */
#define E_APPCONF_OPT_TO_ROUTER 0x00000001UL
#define IS_APPCONF_OPT_TO_ROUTER() ((sAppData.sFlash.sData.u32Opt & E_APPCONF_OPT_TO_ROUTER) != 0) //!< E_APPCONF_OPT_TO_ROUTER 判定

/**
 * NOTICE PALに対して送信するモードに強制する
 */
#define E_APPCONF_OPT_TO_NOTICE 0x00000010UL
#define IS_APPCONF_OPT_TO_NOTICE() ((sAppData.sFlash.sData.u32Opt & E_APPCONF_OPT_TO_NOTICE) != 0)

/**
 * 起床時間のランダマイズを無効にする
 */
#define E_APPCONF_OPT_WAKE_RANDOM 0x00000040UL
#define IS_APPCONF_OPT_WAKE_RANDOM() ((sAppData.sFlash.sData.u32Opt & E_APPCONF_OPT_WAKE_RANDOM) != 0) //!< E_APPCONF_OPT_WAKE_RANDOM 判定

/**
 * OTAを無効にする
 */
#define E_APPCONF_OPT_DISABLE_OTA 0x00000040UL
#define IS_APPCONF_OPT_DISABLE_OTA() ((sAppData.sFlash.sData.u32Opt & E_APPCONF_OPT_DISABLE_OTA) != 0) //!< E_APPCONF_OPT_WAKE_RANDOM 判定

/**
 * 書式モード(バイナリ)
 */
#define E_APPCONF_OPT_UART_BIN 0x00000200UL
#define IS_APPCONF_OPT_UART_BIN() ((sAppData.sFlash.sData.u32Opt & E_APPCONF_OPT_UART_BIN) != 0) //!< E_APPCONF_OPT_UART_BIN 判定

/**
 * パケット通信に暗号化を行う
 */
#define E_APPCONF_OPT_SECURE 0x00001000UL
#define IS_APPCONF_OPT_SECURE() ((sAppData.sFlash.sData.u32Opt & E_APPCONF_OPT_SECURE) != 0) //!< E_APPCONF_OPT_SECURE 判定

/**
 * 子機のデバッグ出力を有効にする
 */
#define E_APPCONF_OPT_VERBOSE 0x00010000UL
#define IS_APPCONF_OPT_VERBOSE() ((sAppData.sFlash.sData.u32Opt & E_APPCONF_OPT_VERBOSE) != 0) //!< E_APPCONF_OPT_VERBOSE 判定

/**
 * UART ボーレート設定の強制
 */
#define E_APPCONF_OPT_UART_FORCE_SETTINGS 0x00040000UL
#define IS_APPCONF_OPT_UART_FORCE_SETTINGS() (sAppData.sFlash.sData.u32Opt & E_APPCONF_OPT_UART_FORCE_SETTINGS) //!< E_APPCONF_OPT_UART_FORCE_SETTINGS 判定
