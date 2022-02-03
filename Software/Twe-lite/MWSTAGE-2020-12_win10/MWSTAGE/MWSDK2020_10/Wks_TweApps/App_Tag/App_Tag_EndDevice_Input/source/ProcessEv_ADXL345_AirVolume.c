/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */

#include <jendefs.h>

#include "utils.h"

#include "ccitt8.h"

#include <math.h>

#include "Interactive.h"
#include "EndDevice_Input.h"

#include "sensor_driver.h"
#include "ADXL345.h"

#define AVE_NUM 5		// 平均する回数 1～5で設定すること

static void vProcessEvCore(tsEvent *pEv, teEvent eEvent, uint32 u32evarg);
static void vStoreSensorValue();
static void vProcessADXL345_AirVolume(teEvent eEvent);
static bool_t bSendToAppTweLiteReq();
static bool_t bSendToSampMonitor();

static uint8 u8sns_cmplt = 0;

static tsSnsObj sSnsObj;
static tsObjData_ADXL345 sObjADXL345;
static uint16 deg = 0;
static uint16 u16v = 0;
static bool_t bFIFOflag = FALSE;

//	Cカーブ
const uint16 au16_Ccurve[1025] = { 0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40,
		44, 47, 51, 55, 58, 62, 66, 69, 73, 76, 80, 83, 87, 90, 93, 97, 100,
		103, 106, 110, 113, 116, 119, 122, 126, 129, 132, 135, 138, 141, 144,
		147, 150, 153, 156, 158, 161, 164, 167, 170, 173, 175, 178, 181, 184,
		186, 189, 192, 194, 197, 200, 202, 205, 207, 210, 212, 215, 217, 220,
		222, 225, 227, 230, 232, 235, 237, 239, 242, 244, 247, 249, 251, 254,
		256, 258, 260, 263, 265, 267, 269, 272, 274, 276, 278, 280, 282, 285,
		287, 289, 291, 293, 295, 297, 299, 301, 303, 305, 308, 310, 312, 314,
		316, 318, 320, 321, 323, 325, 327, 329, 331, 333, 335, 337, 339, 341,
		343, 344, 346, 348, 350, 352, 354, 355, 357, 359, 361, 363, 364, 366,
		368, 370, 371, 373, 375, 377, 378, 380, 382, 384, 385, 387, 389, 390,
		392, 394, 395, 397, 399, 400, 402, 403, 405, 407, 408, 410, 411, 413,
		415, 416, 418, 419, 421, 422, 424, 426, 427, 429, 430, 432, 433, 435,
		436, 438, 439, 441, 442, 444, 445, 447, 448, 450, 451, 452, 454, 455,
		457, 458, 460, 461, 462, 464, 465, 467, 468, 469, 471, 472, 474, 475,
		476, 478, 479, 480, 482, 483, 484, 486, 487, 488, 490, 491, 492, 494,
		495, 496, 498, 499, 500, 502, 503, 504, 505, 507, 508, 509, 510, 512,
		513, 514, 515, 517, 518, 519, 520, 522, 523, 524, 525, 527, 528, 529,
		530, 531, 533, 534, 535, 536, 537, 539, 540, 541, 542, 543, 544, 546,
		547, 548, 549, 550, 551, 552, 554, 555, 556, 557, 558, 559, 560, 562,
		563, 564, 565, 566, 567, 568, 569, 570, 572, 573, 574, 575, 576, 577,
		578, 579, 580, 581, 582, 583, 584, 585, 587, 588, 589, 590, 591, 592,
		593, 594, 595, 596, 597, 598, 599, 600, 601, 602, 603, 604, 605, 606,
		607, 608, 609, 610, 611, 612, 613, 614, 615, 616, 617, 618, 619, 620,
		621, 622, 623, 624, 625, 626, 627, 628, 629, 630, 631, 632, 633, 634,
		634, 635, 636, 637, 638, 639, 640, 641, 642, 643, 644, 645, 646, 647,
		647, 648, 649, 650, 651, 652, 653, 654, 655, 656, 657, 657, 658, 659,
		660, 661, 662, 663, 664, 665, 665, 666, 667, 668, 669, 670, 671, 671,
		672, 673, 674, 675, 676, 677, 678, 678, 679, 680, 681, 682, 683, 683,
		684, 685, 686, 687, 688, 688, 689, 690, 691, 692, 693, 693, 694, 695,
		696, 697, 698, 698, 699, 700, 701, 702, 702, 703, 704, 705, 706, 706,
		707, 708, 709, 710, 710, 711, 712, 713, 713, 714, 715, 716, 717, 717,
		718, 719, 720, 721, 721, 722, 723, 724, 724, 725, 726, 727, 727, 728,
		729, 730, 730, 731, 732, 733, 733, 734, 735, 736, 736, 737, 738, 739,
		739, 740, 741, 742, 742, 743, 744, 745, 745, 746, 747, 747, 748, 749,
		750, 750, 751, 752, 752, 753, 754, 755, 755, 756, 757, 757, 758, 759,
		760, 760, 761, 762, 762, 763, 764, 764, 765, 766, 767, 767, 768, 769,
		769, 770, 771, 771, 772, 773, 773, 774, 775, 775, 776, 777, 777, 778,
		779, 780, 780, 781, 782, 782, 783, 784, 784, 785, 786, 786, 787, 787,
		788, 789, 789, 790, 791, 791, 792, 793, 793, 794, 795, 795, 796, 797,
		797, 798, 799, 799, 800, 800, 801, 802, 802, 803, 804, 804, 805, 806,
		806, 807, 807, 808, 809, 809, 810, 811, 811, 812, 812, 813, 814, 814,
		815, 816, 816, 817, 817, 818, 819, 819, 820, 820, 821, 822, 822, 823,
		824, 824, 825, 825, 826, 827, 827, 828, 828, 829, 830, 830, 831, 831,
		832, 833, 833, 834, 834, 835, 835, 836, 837, 837, 838, 838, 839, 840,
		840, 841, 841, 842, 843, 843, 844, 844, 845, 845, 846, 847, 847, 848,
		848, 849, 849, 850, 851, 851, 852, 852, 853, 853, 854, 855, 855, 856,
		856, 857, 857, 858, 858, 859, 860, 860, 861, 861, 862, 862, 863, 863,
		864, 865, 865, 866, 866, 867, 867, 868, 868, 869, 869, 870, 871, 871,
		872, 872, 873, 873, 874, 874, 875, 875, 876, 876, 877, 878, 878, 879,
		879, 880, 880, 881, 881, 882, 882, 883, 883, 884, 884, 885, 885, 886,
		887, 887, 888, 888, 889, 889, 890, 890, 891, 891, 892, 892, 893, 893,
		894, 894, 895, 895, 896, 896, 897, 897, 898, 898, 899, 899, 900, 900,
		901, 901, 902, 902, 903, 903, 904, 904, 905, 905, 906, 906, 907, 907,
		908, 908, 909, 909, 910, 910, 911, 911, 912, 912, 913, 913, 914, 914,
		915, 915, 916, 916, 917, 917, 918, 918, 919, 919, 920, 920, 921, 921,
		922, 922, 923, 923, 924, 924, 925, 925, 926, 926, 926, 927, 927, 928,
		928, 929, 929, 930, 930, 931, 931, 932, 932, 933, 933, 934, 934, 934,
		935, 935, 936, 936, 937, 937, 938, 938, 939, 939, 940, 940, 941, 941,
		941, 942, 942, 943, 943, 944, 944, 945, 945, 946, 946, 946, 947, 947,
		948, 948, 949, 949, 950, 950, 951, 951, 951, 952, 952, 953, 953, 954,
		954, 955, 955, 955, 956, 956, 957, 957, 958, 958, 959, 959, 959, 960,
		960, 961, 961, 962, 962, 963, 963, 963, 964, 964, 965, 965, 966, 966,
		966, 967, 967, 968, 968, 969, 969, 969, 970, 970, 971, 971, 972, 972,
		972, 973, 973, 974, 974, 975, 975, 975, 976, 976, 977, 977, 978, 978,
		978, 979, 979, 980, 980, 981, 981, 981, 982, 982, 983, 983, 983, 984,
		984, 985, 985, 986, 986, 986, 987, 987, 988, 988, 988, 989, 989, 990,
		990, 990, 991, 991, 992, 992, 993, 993, 993, 994, 994, 995, 995, 995,
		996, 996, 997, 997, 997, 998, 998, 999, 999, 999, 1000, 1000, 1001,
		1001, 1001, 1002, 1002, 1003, 1003, 1003, 1004, 1004, 1005, 1005, 1005,
		1006, 1006, 1007, 1007, 1007, 1008, 1008, 1009, 1009, 1009, 1010, 1010,
		1011, 1011, 1011, 1012, 1012, 1012, 1013, 1013, 1014, 1014, 1014, 1015,
		1015, 1016, 1016, 1016, 1017, 1017, 1018, 1018, 1018, 1019, 1019, 1019,
		1020, 1020, 1021, 1021, 1021, 1022, 1022, 1022, 1023, 1023, 1024, 1024 };

// Aカーブ
const uint16 au16_Acurve[1025] = { 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3,
		3, 4, 4, 4, 5, 5, 5, 6, 6, 6, 7, 7, 7, 8, 8, 8, 9, 9, 9, 10, 10, 10, 11,
		11, 12, 12, 12, 13, 13, 13, 14, 14, 15, 15, 15, 16, 16, 17, 17, 17, 18,
		18, 19, 19, 19, 20, 20, 21, 21, 21, 22, 22, 23, 23, 23, 24, 24, 25, 25,
		26, 26, 26, 27, 27, 28, 28, 29, 29, 30, 30, 30, 31, 31, 32, 32, 33, 33,
		34, 34, 34, 35, 35, 36, 36, 37, 37, 38, 38, 39, 39, 40, 40, 41, 41, 41,
		42, 42, 43, 43, 44, 44, 45, 45, 46, 46, 47, 47, 48, 48, 49, 49, 50, 50,
		51, 51, 52, 52, 53, 53, 54, 54, 55, 55, 56, 56, 57, 57, 58, 58, 59, 59,
		60, 60, 61, 61, 62, 63, 63, 64, 64, 65, 65, 66, 66, 67, 67, 68, 68, 69,
		69, 70, 71, 71, 72, 72, 73, 73, 74, 74, 75, 75, 76, 77, 77, 78, 78, 79,
		79, 80, 80, 81, 82, 82, 83, 83, 84, 84, 85, 86, 86, 87, 87, 88, 88, 89,
		90, 90, 91, 91, 92, 92, 93, 94, 94, 95, 95, 96, 97, 97, 98, 98, 99, 100,
		100, 101, 101, 102, 103, 103, 104, 104, 105, 106, 106, 107, 107, 108,
		109, 109, 110, 111, 111, 112, 112, 113, 114, 114, 115, 115, 116, 117,
		117, 118, 119, 119, 120, 120, 121, 122, 122, 123, 124, 124, 125, 126,
		126, 127, 128, 128, 129, 129, 130, 131, 131, 132, 133, 133, 134, 135,
		135, 136, 137, 137, 138, 139, 139, 140, 141, 141, 142, 143, 143, 144,
		145, 145, 146, 147, 147, 148, 149, 149, 150, 151, 151, 152, 153, 153,
		154, 155, 156, 156, 157, 158, 158, 159, 160, 160, 161, 162, 163, 163,
		164, 165, 165, 166, 167, 167, 168, 169, 170, 170, 171, 172, 172, 173,
		174, 175, 175, 176, 177, 177, 178, 179, 180, 180, 181, 182, 183, 183,
		184, 185, 186, 186, 187, 188, 188, 189, 190, 191, 191, 192, 193, 194,
		194, 195, 196, 197, 197, 198, 199, 200, 200, 201, 202, 203, 204, 204,
		205, 206, 207, 207, 208, 209, 210, 210, 211, 212, 213, 214, 214, 215,
		216, 217, 217, 218, 219, 220, 221, 221, 222, 223, 224, 225, 225, 226,
		227, 228, 229, 229, 230, 231, 232, 233, 233, 234, 235, 236, 237, 237,
		238, 239, 240, 241, 242, 242, 243, 244, 245, 246, 246, 247, 248, 249,
		250, 251, 251, 252, 253, 254, 255, 256, 256, 257, 258, 259, 260, 261,
		262, 262, 263, 264, 265, 266, 267, 268, 268, 269, 270, 271, 272, 273,
		274, 274, 275, 276, 277, 278, 279, 280, 281, 281, 282, 283, 284, 285,
		286, 287, 288, 288, 289, 290, 291, 292, 293, 294, 295, 296, 297, 297,
		298, 299, 300, 301, 302, 303, 304, 305, 306, 306, 307, 308, 309, 310,
		311, 312, 313, 314, 315, 316, 317, 318, 318, 319, 320, 321, 322, 323,
		324, 325, 326, 327, 328, 329, 330, 331, 332, 333, 334, 334, 335, 336,
		337, 338, 339, 340, 341, 342, 343, 344, 345, 346, 347, 348, 349, 350,
		351, 352, 353, 354, 355, 356, 357, 358, 359, 360, 361, 362, 363, 364,
		365, 366, 367, 368, 369, 370, 371, 372, 373, 374, 375, 376, 377, 378,
		379, 380, 381, 382, 383, 384, 385, 386, 387, 388, 389, 390, 391, 392,
		393, 394, 395, 396, 397, 398, 399, 400, 401, 402, 404, 405, 406, 407,
		408, 409, 410, 411, 412, 413, 414, 415, 416, 417, 418, 419, 420, 422,
		423, 424, 425, 426, 427, 428, 429, 430, 431, 432, 433, 435, 436, 437,
		438, 439, 440, 441, 442, 443, 444, 446, 447, 448, 449, 450, 451, 452,
		453, 454, 456, 457, 458, 459, 460, 461, 462, 463, 465, 466, 467, 468,
		469, 470, 471, 473, 474, 475, 476, 477, 478, 479, 481, 482, 483, 484,
		485, 486, 488, 489, 490, 491, 492, 493, 495, 496, 497, 498, 499, 501,
		502, 503, 504, 505, 506, 508, 509, 510, 511, 512, 514, 515, 516, 517,
		518, 520, 521, 522, 523, 525, 526, 527, 528, 529, 531, 532, 533, 534,
		536, 537, 538, 539, 540, 542, 543, 544, 545, 547, 548, 549, 550, 552,
		553, 554, 555, 557, 558, 559, 560, 562, 563, 564, 566, 567, 568, 569,
		571, 572, 573, 575, 576, 577, 578, 580, 581, 582, 584, 585, 586, 587,
		589, 590, 591, 593, 594, 595, 597, 598, 599, 601, 602, 603, 605, 606,
		607, 609, 610, 611, 613, 614, 615, 617, 618, 619, 621, 622, 623, 625,
		626, 627, 629, 630, 631, 633, 634, 636, 637, 638, 640, 641, 642, 644,
		645, 647, 648, 649, 651, 652, 653, 655, 656, 658, 659, 660, 662, 663,
		665, 666, 667, 669, 670, 672, 673, 675, 676, 677, 679, 680, 682, 683,
		685, 686, 687, 689, 690, 692, 693, 695, 696, 698, 699, 700, 702, 703,
		705, 706, 708, 709, 711, 712, 714, 715, 717, 718, 720, 721, 723, 724,
		726, 727, 728, 730, 731, 733, 734, 736, 738, 739, 741, 742, 744, 745,
		747, 748, 750, 751, 753, 754, 756, 757, 759, 760, 762, 763, 765, 767,
		768, 770, 771, 773, 774, 776, 777, 779, 781, 782, 784, 785, 787, 788,
		790, 792, 793, 795, 796, 798, 799, 801, 803, 804, 806, 807, 809, 811,
		812, 814, 815, 817, 819, 820, 822, 824, 825, 827, 828, 830, 832, 833,
		835, 837, 838, 840, 842, 843, 845, 847, 848, 850, 852, 853, 855, 857,
		858, 860, 862, 863, 865, 867, 868, 870, 872, 873, 875, 877, 878, 880,
		882, 884, 885, 887, 889, 890, 892, 894, 896, 897, 899, 901, 902, 904,
		906, 908, 909, 911, 913, 915, 916, 918, 920, 922, 923, 925, 927, 929,
		930, 932, 934, 936, 938, 939, 941, 943, 945, 947, 948, 950, 952, 954,
		956, 957, 959, 961, 963, 965, 966, 968, 970, 972, 974, 976, 977, 979,
		981, 983, 985, 987, 988, 990, 992, 994, 996, 998, 1000, 1001, 1003,
		1005, 1007, 1009, 1011, 1013, 1015, 1016, 1018, 1020, 1022, 1024 };

enum {
	E_SNS_ADC_CMP_MASK = 1,
	E_SNS_ADXL345_CMP = 2,
	E_SNS_ALL_CMP = 3
};

#define ENABLE_COUNTER 10

#define ABS(c) (c>0 ? c : -c)

/*
 * ADC 計測をしてデータ送信するアプリケーション制御
 */
PRSEV_HANDLER_DEF(E_STATE_IDLE, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	static bool_t bFirst = TRUE;
	static bool_t bOk = TRUE;
	if (eEvent == E_EVENT_START_UP) {
		if (u32evarg & EVARG_START_UP_WAKEUP_RAMHOLD_MASK) {
			// Warm start message
			V_PRINTF("%c[2J%c[H", 27, 27); // CLEAR SCREEN
			V_PRINTF(LB "*** Warm starting woke by %s. ***", sAppData.bWakeupByButton ? "DIO" : "WakeTimer");
		} else {
			// 開始する
			// start up message
			vSerInitMessage();

			V_PRINTF(LB "*** Cold starting");
			V_PRINTF(LB "* start end device[%d]", u32TickCount_ms & 0xFFFF);

#ifdef LITE2525A
			vPortSetLo(PORT_INPUT1);
			vPortAsOutput(PORT_INPUT1);
#endif
		}

		// RC クロックのキャリブレーションを行う
		ToCoNet_u16RcCalib(sAppData.sFlash.sData.u16RcClock);

		// センサーがらみの変数の初期化
		u8sns_cmplt = 0;

		vADXL345_AirVolume_Init( &sObjADXL345, &sSnsObj );
		if( bFirst ){
			V_PRINTF(LB "*** ADXL345 AirVolume Setting...");
			bOk &= bADXL345reset();
			bOk &= bADXL345_AirVolume_Setting();
			if(bOk) bFirst = FALSE;
		}
		vSnsObj_Process(&sSnsObj, E_ORDER_KICK);
		if (bSnsObj_isComplete(&sSnsObj)) {
			// 即座に完了した時はセンサーが接続されていない、通信エラー等
			u8sns_cmplt |= E_SNS_ADXL345_CMP;
			V_PRINTF(LB "*** ADXL345 comm err?");
			ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // スリープ状態へ遷移
			return;
		}

		// ADC の取得
		vADC_WaitInit();
		vSnsObj_Process(&sAppData.sADC, E_ORDER_KICK);

		// RUNNING 状態
		ToCoNet_Event_SetState(pEv, E_STATE_RUNNING);
	} else {
		V_PRINTF(LB "*** unexpected state.");
		ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // スリープ状態へ遷移
	}
}

PRSEV_HANDLER_DEF(E_STATE_RUNNING, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
		// 短期間スリープからの起床をしたので、センサーの値をとる
	if ((eEvent == E_EVENT_START_UP) && (u32evarg & EVARG_START_UP_WAKEUP_RAMHOLD_MASK)) {
		V_PRINTF("#");
		vProcessADXL345_AirVolume(E_EVENT_START_UP);
	}

	// 送信処理に移行
	if (u8sns_cmplt == E_SNS_ALL_CMP) {
		ToCoNet_Event_SetState(pEv, E_STATE_APP_WAIT_TX);
	}

	// タイムアウト
	if (ToCoNet_Event_u32TickFrNewState(pEv) > 100) {
		V_PRINTF(LB"! TIME OUT (E_STATE_RUNNING)");
		ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // スリープ状態へ遷移
	}
}

PRSEV_HANDLER_DEF(E_STATE_APP_WAIT_TX, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	// 過去4回分保存(現在含めて5回分)
	static int16 x[5] = {0,0,0,0,0};
	static int16 y[5] = {0,0,0,0,0};
	static int16 z[5] = {0,0,0,0,0};

	int16 deg_now = 0;

	static uint8 count = 0;
	if (eEvent == E_EVENT_NEW_STATE) {
		// ネットワークの初期化
		if (!sAppData.pContextNwk) {
			// 初回のみ
			sAppData.sNwkLayerTreeConfig.u8Role = TOCONET_NWK_ROLE_ENDDEVICE;
			sAppData.pContextNwk = ToCoNet_NwkLyTr_psConfig_MiniNodes(&sAppData.sNwkLayerTreeConfig);
			if (sAppData.pContextNwk) {
				ToCoNet_Nwk_bInit(sAppData.pContextNwk);
				ToCoNet_Nwk_bStart(sAppData.pContextNwk);
			} else {
				ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // スリープ状態へ遷移
				return;
			}
		} else {
			// 一度初期化したら RESUME
			ToCoNet_Nwk_bResume(sAppData.pContextNwk);
		}

		V_PRINTF( LB"Interrupt : %02X", sObjADXL345.u8Interrupt );
		if(  (sObjADXL345.u8Interrupt&0x10) != 0 ){
			bFIFOflag = TRUE;
			bSetFIFO_Air();
			ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // スリープ状態へ遷移
		}else if( (sObjADXL345.u8Interrupt&0x02) == 0 ){		//	FIFOで起きなかった場合
			bFIFOflag = FALSE;
			V_PRINTF( LB"SLEEP" );
			bSetActive();
			ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // スリープ状態へ遷移
		}

		//	センサから読み込んだ加速度を代入
		x[AVE_NUM-1] = sObjADXL345.ai16Result[ADXL345_IDX_X];
		y[AVE_NUM-1] = sObjADXL345.ai16Result[ADXL345_IDX_Y];
		z[AVE_NUM-1] = sObjADXL345.ai16Result[ADXL345_IDX_Z];

		int16 avex=0;
		int16 avey=0;
		int16 avez=0;

		//	5回分の平均を取る
		uint8 i;
		for( i=0; i<AVE_NUM; i++){
			avex += x[i];
			avey += y[i];
			avez += z[i];
			x[i] = x[i+1];
			y[i] = y[i+1];
			z[i] = z[i+1];
		}
		avex /= AVE_NUM;
		avey /= AVE_NUM;
		avez /= AVE_NUM;

		//	送信先の制御を行うか否かの判定
		if( ABS(avez) < 400 ){			// 現在のZ軸にかかっている加速度が0.4g以下になった回数が(実質)連続でENABLE_COUNTER回続いたら計測開始
			if( count < ENABLE_COUNTER ){
				count++;
			}
		}else if( ABS(avez) > 700 ){		// 現在のZ軸に0.7g以上かかったら計測をやめる
			count = 0;
			if( (sObjADXL345.u8Interrupt&0x10) == 0 ){
				bFIFOflag = FALSE;
				bSetActive();
			}
		}

		static int16 i16diff = 0;
		static uint16 u16bfDeg = 0;
		static uint16 u16deg = 0;
		static bool_t OverFlag = FALSE;
		static bool_t UnderFlag = FALSE;
		static uint32 OverCounter = 0;

		if( count == ENABLE_COUNTER ){
			//	角度の計算(小数以下1ケタまで)
			deg_now = (int16)(atan2f( (float)(avey), (float)(avex) )*1800/3.14);		//	x,y軸の角度
			u16deg = (deg_now >= 0 ? deg_now : 3600+deg_now);							//	値域を0.1～360.0度へ変換
			if( u16deg == 3600 ){														//	値域を0～359.9度へ変換
				u16deg = 0;
			}
			u16deg = u16deg+1800;														//	アンテナロゴが上の時に900にする
			if( u16deg >= 3600  ){
				u16deg -= 3600;
			}

			//	振り切ったときの処理
			if( u16bfDeg > 3400 && u16deg < 200 && i16diff > 0 && UnderFlag == FALSE ){
				OverFlag = TRUE;
			}else if( u16bfDeg < 200 && u16deg > 3400 && i16diff < 0 && OverFlag == FALSE ){
				UnderFlag = TRUE;
			}else{
				//	前回動いていなければフラグをへし折らない
				if( OverFlag == TRUE && i16diff < 0 && u16deg > 3400 ){
					OverFlag = FALSE;
				}
				if( UnderFlag == TRUE && i16diff > 0 && u16deg < 200 ){
					UnderFlag = FALSE;
				}
			}

			if( OverFlag == TRUE ){			//	360度を越えたときは359.9度に固定
				deg = 3599;
			}else if( UnderFlag == TRUE ){	//	0度を下回ったときは0度に固定
				deg = 0;
			}else{
				deg = u16deg;
			}
			i16diff = u16deg-u16bfDeg;
			u16bfDeg = u16deg;

			uint16 u16DegTemp = deg;

			if( u16DegTemp <= 900 ){		//	90度より下は無視
				u16DegTemp = 0;
			}else if( u16DegTemp > 2700 ) {	//	270度より上も無視
				u16DegTemp = 2700;
			}

			//	Duty比に変換
			if( u16DegTemp == 0 ){
				u16v = 0;
			}else{
				u16v = ((u16DegTemp-900)*1024)/1800;
				if(u16v > 1024){
					u16v = 1024;
				}
			}

			// カーブの変更
			if( (sAppData.sFlash.sData.i16param&0x0003) == 1){			//	Aカーブ
				u16v = au16_Acurve[u16v];
			}else if( (sAppData.sFlash.sData.i16param&0x0003) == 2){		//	Cカーブ
				u16v = au16_Ccurve[u16v];
			}
			V_PRINTF( LB"deg = %d : Duty = %d", deg, u16v );

			sAppData.u16frame_count++;

			bool_t bOk;
			if( IS_APPCONF_OPT_APP_TWELITE() ){		//	App_Twelites
				bOk = bSendToAppTweLiteReq();
			}else{									//	Samp_Monitor
				bOk = bSendToSampMonitor();
			}

			if(bOk){
				ToCoNet_Tx_vProcessQueue(); // 送信処理をタイマーを待たずに実行する
				V_PRINTF(LB"TxOk");
			}else{
				V_PRINTF(LB"TxFl");
			}


			//	点滅用カウンタ
			if( OverFlag || UnderFlag ){
				OverCounter++;
			}else{
				OverCounter = 0;
			}

			//	0度を下回ればゆっくり点滅、359.9度を超えれば速く点滅、それ以外なら点灯
			bool_t PortState = TRUE;
			if( (OverFlag && (OverCounter&0x2) != 0) || (UnderFlag && (OverCounter&0x4) != 0) ){
				PortState = FALSE;
			}

			vPortSet_TrueAsLo( PORT_INPUT1, PortState );
			vPortSet_TrueAsLo( LED, PortState );
		}else{
			LED_OFF(LED);

			i16diff = 0;
			u16bfDeg = 0;
			OverFlag = FALSE;
			UnderFlag = FALSE;
			ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP);
		}

		V_PRINTF(" FR=%04X", sAppData.u16frame_count);
	}

	if (eEvent == E_ORDER_KICK) { // 送信完了イベントが来たのでスリープする
		ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // スリープ状態へ遷移
	}

	// タイムアウト
	if (ToCoNet_Event_u32TickFrNewState(pEv) > 100) {
		V_PRINTF(LB"! TIME OUT (E_STATE_APP_WAIT_TX)");
		ToCoNet_Event_SetState(pEv, E_STATE_APP_SLEEP); // スリープ状態へ遷移
	}
}

PRSEV_HANDLER_DEF(E_STATE_APP_SLEEP, tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	if (eEvent == E_EVENT_NEW_STATE) {
		// Sleep は必ず E_EVENT_NEW_STATE 内など１回のみ呼び出される場所で呼び出す。
		V_PRINTF(LB"! Sleeping...");
		V_FLUSH();

		// Mininode の場合、特別な処理は無いのだが、ポーズ処理を行う
		if( sAppData.pContextNwk ){
			ToCoNet_Nwk_bPause(sAppData.pContextNwk);
		}

		//	割り込みの設定
		vAHI_DioSetDirection(PORT_INPUT_MASK_AIRVOLUME, 0); // set as input
		(void)u32AHI_DioInterruptStatus(); // clear interrupt register
		vAHI_DioWakeEnable(PORT_INPUT_MASK_AIRVOLUME, 0); // also use as DIO WAKE SOURCE
		vAHI_DioWakeEdge(PORT_INPUT_MASK_AIRVOLUME, 0); // 割り込みエッジ(立上がりに設定)
		u8Read_Interrupt();

		ToCoNet_vSleep( E_AHI_WAKE_TIMER_0, 0, FALSE, FALSE);
	}
}

/**
 * イベント処理関数リスト
 */
static const tsToCoNet_Event_StateHandler asStateFuncTbl[] = {
	PRSEV_HANDLER_TBL_DEF(E_STATE_IDLE),
	PRSEV_HANDLER_TBL_DEF(E_STATE_RUNNING),
	PRSEV_HANDLER_TBL_DEF(E_STATE_APP_WAIT_TX),
	PRSEV_HANDLER_TBL_DEF(E_STATE_APP_SLEEP),
	PRSEV_HANDLER_TBL_TRM
};

/**
 * イベント処理関数
 * @param pEv
 * @param eEvent
 * @param u32evarg
 */
void vProcessEvCore(tsEvent *pEv, teEvent eEvent, uint32 u32evarg) {
	ToCoNet_Event_StateExec(asStateFuncTbl, pEv, eEvent, u32evarg);
}

#if 0
/**
 * ハードウェア割り込み
 * @param u32DeviceId
 * @param u32ItemBitmap
 * @return
 */
static uint8 cbAppToCoNet_u8HwInt(uint32 u32DeviceId, uint32 u32ItemBitmap) {
	uint8 u8handled = FALSE;

	switch (u32DeviceId) {
	case E_AHI_DEVICE_ANALOGUE:
		break;

	case E_AHI_DEVICE_SYSCTRL:
		break;

	case E_AHI_DEVICE_TIMER0:
		break;

	case E_AHI_DEVICE_TICK_TIMER:
		break;

	default:
		break;
	}

	return u8handled;
}
#endif

/**
 * ハードウェアイベント（遅延実行）
 * @param u32DeviceId
 * @param u32ItemBitmap
 */
static void cbAppToCoNet_vHwEvent(uint32 u32DeviceId, uint32 u32ItemBitmap) {
	switch (u32DeviceId) {
	case E_AHI_DEVICE_TICK_TIMER:
		vProcessADXL345_AirVolume(E_EVENT_TICK_TIMER);
		break;

	case E_AHI_DEVICE_ANALOGUE:
		/*
		 * ADC完了割り込み
		 */
		V_PUTCHAR('@');
		vSnsObj_Process(&sAppData.sADC, E_ORDER_KICK);
		if (bSnsObj_isComplete(&sAppData.sADC)) {
			u8sns_cmplt |= E_SNS_ADC_CMP_MASK;
			vStoreSensorValue();
		}
		break;

	case E_AHI_DEVICE_SYSCTRL:
		break;

	case E_AHI_DEVICE_TIMER0:
		break;

	default:
		break;
	}
}

#if 0
/**
 * メイン処理
 */
static void cbAppToCoNet_vMain() {
	/* handle serial input */
	vHandleSerialInput();
}
#endif

#if 0
/**
 * ネットワークイベント
 * @param eEvent
 * @param u32arg
 */
static void cbAppToCoNet_vNwkEvent(teEvent eEvent, uint32 u32arg) {
	switch(eEvent) {
	case E_EVENT_TOCONET_NWK_START:
		break;

	default:
		break;
	}
}
#endif


#if 0
/**
 * RXイベント
 * @param pRx
 */
static void cbAppToCoNet_vRxEvent(tsRxDataApp *pRx) {

}
#endif

/**
 * TXイベント
 * @param u8CbId
 * @param bStatus
 */
static void cbAppToCoNet_vTxEvent(uint8 u8CbId, uint8 bStatus) {
	// 送信完了
	V_PRINTF(LB"! Tx Cmp = %d", bStatus);
	ToCoNet_Event_Process(E_ORDER_KICK, 0, vProcessEvCore);
}
/**
 * アプリケーションハンドラー定義
 *
 */
static tsCbHandler sCbHandler = {
	NULL, // cbAppToCoNet_u8HwInt,
	cbAppToCoNet_vHwEvent,
	NULL, // cbAppToCoNet_vMain,
	NULL, // cbAppToCoNet_vNwkEvent,
	NULL, // cbAppToCoNet_vRxEvent,
	cbAppToCoNet_vTxEvent
};

/**
 * アプリケーション初期化
 */
void vInitAppADXL345_AirVolume() {
	psCbHandler = &sCbHandler;
	pvProcessEv1 = vProcessEvCore;
}

static void vProcessADXL345_AirVolume(teEvent eEvent) {
	if (bSnsObj_isComplete(&sSnsObj)) {
		 return;
	}

	// イベントの処理
	vSnsObj_Process(&sSnsObj, eEvent); // ポーリングの時間待ち
	if (bSnsObj_isComplete(&sSnsObj)) {
		u8sns_cmplt |= E_SNS_ADXL345_CMP;

		V_PRINTF(LB"!ADXL345: X : %d, Y : %d, Z : %d",
			sObjADXL345.ai16Result[ADXL345_IDX_X],
			sObjADXL345.ai16Result[ADXL345_IDX_Y],
			sObjADXL345.ai16Result[ADXL345_IDX_Z]
		);

		// 完了時の処理
		if (u8sns_cmplt == E_SNS_ALL_CMP) {
			ToCoNet_Event_Process(E_ORDER_KICK, 0, vProcessEvCore);
		}
	}
}

/**
 * センサー値を格納する
 */
static void vStoreSensorValue() {
	// センサー値の保管
	sAppData.sSns.u16Adc1 = sAppData.sObjADC.ai16Result[u8ADCPort[0]];
	sAppData.sSns.u16Adc2 = sAppData.sObjADC.ai16Result[u8ADCPort[1]];
	sAppData.sSns.u8Batt = ENCODE_VOLT(sAppData.sObjADC.ai16Result[TEH_ADC_IDX_VOLT]);

	// ADC1 が 1300mV 以上(SuperCAP が 2600mV 以上)である場合は SUPER CAP の直結を有効にする
	if (sAppData.sSns.u16Adc1 >= VOLT_SUPERCAP_CONTROL) {
		vPortSetLo(DIO_SUPERCAP_CONTROL);
	}
}

static bool_t bSendToAppTweLiteReq(){
	// 初期化後速やかに送信要求
	V_PRINTF(LB"[SNS_COMP/TX]");

	tsTxDataApp sTx;
	memset(&sTx, 0, sizeof(sTx)); // 必ず０クリアしてから使う！

	sTx.u32SrcAddr = ToCoNet_u32GetSerial();

	uint8* q = sTx.auData;
	uint8 crc = u8CCITT8((uint8*) &sToCoNet_AppContext.u32AppId, 4);

	sAppData.u16frame_count++; // シリアル番号を更新する
	S_OCTET(crc);								//
	S_OCTET(0x01);								// プロトコルバージョン
	S_OCTET(0x78);								// アプリケーション論理アドレス
	S_BE_DWORD(ToCoNet_u32GetSerial());			// シリアル番号
	S_OCTET(0x00);								// 宛先
	S_BE_WORD(u32TickCount_ms & 0xFFFF);		// タイムスタンプ
	S_OCTET(0);									// 中継フラグ

	S_OCTET(1);									// パケット形式

	//	DO1～4の制御
	uint8 u8DO = 0x00;
	if( u16v == 0 ){
		u8DO = 0x00;
	}else if( u16v < 256 ){
		u8DO = 0x01;
	}else if( u16v < 512 ){
		u8DO = 0x03;
	}else if( u16v < 768 ){
		u8DO = 0x07;
	}else{
		u8DO = 0x0F;
	}

	if( sAppData.sFlash.sData.u8id == 0 || sAppData.sFlash.sData.u8id == 5 ){
		S_OCTET(u8DO);	//	DO1～4
		S_OCTET(0x0F);	//	DOのマスク
	}else{
		S_OCTET(0x00);	//	DO1～4
		S_OCTET(0x00);	//	DOのマスク
	}

	//	PWM値の設定
	switch( sAppData.sFlash.sData.u8id ){
		case 1:
			S_BE_WORD(u16v);
			S_BE_WORD(0xFFFF);
			S_BE_WORD(0xFFFF);
			S_BE_WORD(0xFFFF);
			break;
		case 2:
			S_BE_WORD(0xFFFF);
			S_BE_WORD(u16v);
			S_BE_WORD(0xFFFF);
			S_BE_WORD(0xFFFF);
			break;
		case 3:
			S_BE_WORD(0xFFFF);
			S_BE_WORD(0xFFFF);
			S_BE_WORD(u16v);
			S_BE_WORD(0xFFFF);
			break;
		case 4:
			S_BE_WORD(0xFFFF);
			S_BE_WORD(0xFFFF);
			S_BE_WORD(0xFFFF);
			S_BE_WORD(u16v);
			break;
		case 5:
			S_BE_WORD(0xFFFF);
			S_BE_WORD(0xFFFF);
			S_BE_WORD(0xFFFF);
			S_BE_WORD(0xFFFF);
			break;
		default:
			S_BE_WORD(u16v);
			S_BE_WORD(0x0000);
			S_BE_WORD(0x0000);
			S_BE_WORD(0x0000);
			break;
	}

	sTx.u8Cmd = 0x02+1; // パケット種別
	// 送信する
	sTx.u32DstAddr = TOCONET_MAC_ADDR_BROADCAST; // ブロードキャスト
	sTx.u32SrcAddr = sToCoNet_AppContext.u16ShortAddress;
	sTx.bAckReq = FALSE;
	sTx.u8Retry = sAppData.u8Retry;

	sTx.u8Len = q - sTx.auData; // パケットのサイズ
	sTx.u8CbId = sAppData.u16frame_count & 0xFF; // TxEvent で通知される番号、送信先には通知されない
	sTx.u8Seq = sAppData.u16frame_count & 0xFF; // シーケンス番号(送信先に通知される)

	return ToCoNet_bMacTxReq(&sTx);
}

bool_t bSendToSampMonitor()
{
	uint8	au8Data[12];
	uint8*	q = au8Data;

	S_OCTET(sAppData.sSns.u8Batt);
	S_BE_WORD(sAppData.sSns.u16Adc1);
	S_BE_WORD(sAppData.sSns.u16Adc2);
	S_BE_WORD(u16v);
	S_BE_WORD(deg);
	S_BE_WORD(0x0000);
	S_OCTET(0xFB);

	return bTransmitToParent( sAppData.pContextNwk, au8Data, q-au8Data );
}
