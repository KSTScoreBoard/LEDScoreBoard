/* Copyright (C) 2017 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */

/* MISAKI font is used for character bitmap.
 *   http://www.geocities.jp/littlimi/misaki.htm
 */

/****************************************************************************/
/***        Include Files                                                 ***/
/****************************************************************************/
#include "jendefs.h"
#include "LcdFont.h"

/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/

/****************************************************************************/
/***        Type Definitions                                              ***/
/****************************************************************************/

/****************************************************************************/
/***        Local Function Prototypes                                     ***/
/****************************************************************************/

/****************************************************************************/
/***        Exported Variables                                            ***/
/****************************************************************************/
//
const uint8 au8TocosHan[] =
{ 0xc4, 0xb3, 0xb7, 0xae, 0xb3, 0xba, 0xbd, 0xd3, 0xbd, 0x00 };

const uint8 au8TocosZen[] =
{ 0x90, 0x91, 0x92, 0x93, 0x94, 0x93, 0x95, 0x96, 0x97, 0x00 };

const uint8 au8Tocos[] =
{ 0x90, 0x91, 0xba, 0xbd, 0xd3, 0xbd, 0x95, 0x96, 0x00 };

/****************************************************************************/
/***        Local Variables                                               ***/
/****************************************************************************/
//
PRIVATE const uint8 au8LcdFont[][4] =
{
{ 3, 0x00<<1, 0x00<<1, 0x00<<1 }, /*   (20) */
{ 3, 0x00<<1, 0x2f<<1, 0x00<<1 }, /* ! (21) */
{ 3, 0x03<<1, 0x00<<1, 0x03<<1 }, /* " (22) */
{ 3, 0x3f<<1, 0x12<<1, 0x3f<<1 }, /* # (23) */
{ 3, 0x16<<1, 0x3f<<1, 0x1a<<1 }, /* $ (24) */
{ 3, 0x12<<1, 0x08<<1, 0x24<<1 }, /* % (25) */
{ 3, 0x32<<1, 0x3d<<1, 0x2a<<1 }, /* & (26) */
{ 3, 0x02<<1, 0x01<<1, 0x00<<1 }, /* ' (27) */
{ 3, 0x00<<1, 0x3e<<1, 0x41<<1 }, /* ( (28) */
{ 3, 0x41<<1, 0x3e<<1, 0x00<<1 }, /* ) (29) */
{ 3, 0x0a<<1, 0x07<<1, 0x0a<<1 }, /* * (2a) */
{ 3, 0x08<<1, 0x3e<<1, 0x08<<1 }, /* + (2b) */
{ 3, 0x40<<1, 0x20<<1, 0x00<<1 }, /* , (2c) */
{ 3, 0x08<<1, 0x08<<1, 0x08<<1 }, /* - (2d) */
{ 3, 0x00<<1, 0x20<<1, 0x00<<1 }, /* . (2e) */
{ 3, 0x10<<1, 0x08<<1, 0x04<<1 }, /* / (2f) */
{ 3, 0x1c<<1, 0x2a<<1, 0x1c<<1 }, /* 0 (30) */
{ 3, 0x24<<1, 0x3e<<1, 0x20<<1 }, /* 1 (31) */
{ 3, 0x32<<1, 0x2a<<1, 0x24<<1 }, /* 2 (32) */
{ 3, 0x22<<1, 0x2a<<1, 0x14<<1 }, /* 3 (33) */
{ 3, 0x18<<1, 0x14<<1, 0x3e<<1 }, /* 4 (34) */
{ 3, 0x2e<<1, 0x2a<<1, 0x12<<1 }, /* 5 (35) */
{ 3, 0x1c<<1, 0x2a<<1, 0x12<<1 }, /* 6 (36) */
{ 3, 0x02<<1, 0x3a<<1, 0x06<<1 }, /* 7 (37) */
{ 3, 0x14<<1, 0x2a<<1, 0x14<<1 }, /* 8 (38) */
{ 3, 0x24<<1, 0x2a<<1, 0x1c<<1 }, /* 9 (39) */
{ 3, 0x00<<1, 0x24<<1, 0x00<<1 }, /* : (3a) */
{ 3, 0x40<<1, 0x24<<1, 0x00<<1 }, /* ; (3b) */
{ 3, 0x08<<1, 0x14<<1, 0x22<<1 }, /* < (3c) */
{ 3, 0x14<<1, 0x14<<1, 0x14<<1 }, /* = (3d) */
{ 3, 0x22<<1, 0x14<<1, 0x08<<1 }, /* > (3e) */
{ 3, 0x02<<1, 0x29<<1, 0x06<<1 }, /* ? (3f) */
{ 3, 0x12<<1, 0x29<<1, 0x1e<<1 }, /* @ (40) */
{ 3, 0x3e<<1, 0x09<<1, 0x3e<<1 }, /* A (41) */
{ 3, 0x3f<<1, 0x25<<1, 0x1a<<1 }, /* B (42) */
{ 3, 0x1e<<1, 0x21<<1, 0x21<<1 }, /* C (43) */
{ 3, 0x3f<<1, 0x21<<1, 0x1e<<1 }, /* D (44) */
{ 3, 0x3f<<1, 0x25<<1, 0x21<<1 }, /* E (45) */
{ 3, 0x3f<<1, 0x05<<1, 0x01<<1 }, /* F (46) */
{ 3, 0x1e<<1, 0x21<<1, 0x39<<1 }, /* G (47) */
{ 3, 0x3f<<1, 0x08<<1, 0x3f<<1 }, /* H (48) */
{ 3, 0x21<<1, 0x3f<<1, 0x21<<1 }, /* I (49) */
{ 3, 0x10<<1, 0x20<<1, 0x1f<<1 }, /* J (4a) */
{ 3, 0x3f<<1, 0x04<<1, 0x3b<<1 }, /* K (4b) */
{ 3, 0x3f<<1, 0x20<<1, 0x20<<1 }, /* L (4c) */
{ 3, 0x3f<<1, 0x06<<1, 0x3f<<1 }, /* M (4d) */
{ 3, 0x3f<<1, 0x01<<1, 0x3e<<1 }, /* N (4e) */
{ 3, 0x1e<<1, 0x21<<1, 0x1e<<1 }, /* O (4f) */
{ 3, 0x3f<<1, 0x09<<1, 0x06<<1 }, /* P (50) */
{ 3, 0x1e<<1, 0x21<<1, 0x5e<<1 }, /* Q (51) */
{ 3, 0x3f<<1, 0x09<<1, 0x36<<1 }, /* R (52) */
{ 3, 0x22<<1, 0x25<<1, 0x19<<1 }, /* S (53) */
{ 3, 0x01<<1, 0x3f<<1, 0x01<<1 }, /* T (54) */
{ 3, 0x3f<<1, 0x20<<1, 0x3f<<1 }, /* U (55) */
{ 3, 0x3f<<1, 0x10<<1, 0x0f<<1 }, /* V (56) */
{ 3, 0x3f<<1, 0x18<<1, 0x3f<<1 }, /* W (57) */
{ 3, 0x33<<1, 0x0c<<1, 0x33<<1 }, /* X (58) */
{ 3, 0x03<<1, 0x3c<<1, 0x03<<1 }, /* Y (59) */
{ 3, 0x31<<1, 0x2d<<1, 0x23<<1 }, /* Z (5a) */
{ 3, 0x00<<1, 0x7f<<1, 0x41<<1 }, /* [ (5b) */
{ 3, 0x15<<1, 0x3e<<1, 0x15<<1 }, /* ¥ (5c) */
{ 3, 0x41<<1, 0x7f<<1, 0x00<<1 }, /* ] (5d) */
{ 3, 0x02<<1, 0x01<<1, 0x02<<1 }, /* ^ (5e) */
{ 3, 0x40<<1, 0x40<<1, 0x40<<1 }, /* _ (5f) */
{ 3, 0x00<<1, 0x01<<1, 0x02<<1 }, /* ` (60) */
{ 3, 0x18<<1, 0x24<<1, 0x3c<<1 }, /* a (61) */
{ 3, 0x3f<<1, 0x24<<1, 0x18<<1 }, /* b (62) */
{ 3, 0x18<<1, 0x24<<1, 0x24<<1 }, /* c (63) */
{ 3, 0x18<<1, 0x24<<1, 0x3f<<1 }, /* d (64) */
{ 3, 0x18<<1, 0x2c<<1, 0x2c<<1 }, /* e (65) */
{ 3, 0x04<<1, 0x3f<<1, 0x05<<1 }, /* f (66) */
{ 3, 0x48<<1, 0x54<<1, 0x3c<<1 }, /* g (67) */
{ 3, 0x3f<<1, 0x04<<1, 0x38<<1 }, /* h (68) */
{ 3, 0x00<<1, 0x3d<<1, 0x00<<1 }, /* i (69) */
{ 3, 0x40<<1, 0x3d<<1, 0x00<<1 }, /* j (6a) */
{ 3, 0x3f<<1, 0x08<<1, 0x34<<1 }, /* k (6b) */
{ 3, 0x01<<1, 0x3f<<1, 0x00<<1 }, /* l (6c) */
{ 3, 0x3c<<1, 0x1c<<1, 0x38<<1 }, /* m (6d) */
{ 3, 0x3c<<1, 0x04<<1, 0x38<<1 }, /* n (6e) */
{ 3, 0x18<<1, 0x24<<1, 0x18<<1 }, /* o (6f) */
{ 3, 0x7c<<1, 0x24<<1, 0x18<<1 }, /* p (70) */
{ 3, 0x18<<1, 0x24<<1, 0x7c<<1 }, /* q (71) */
{ 3, 0x3c<<1, 0x08<<1, 0x04<<1 }, /* r (72) */
{ 3, 0x28<<1, 0x3c<<1, 0x14<<1 }, /* s (73) */
{ 3, 0x04<<1, 0x3e<<1, 0x24<<1 }, /* t (74) */
{ 3, 0x3c<<1, 0x20<<1, 0x3c<<1 }, /* u (75) */
{ 3, 0x3c<<1, 0x10<<1, 0x0c<<1 }, /* v (76) */
{ 3, 0x3c<<1, 0x30<<1, 0x3c<<1 }, /* w (77) */
{ 3, 0x24<<1, 0x18<<1, 0x24<<1 }, /* x (78) */
{ 3, 0x4c<<1, 0x50<<1, 0x3c<<1 }, /* y (79) */
{ 3, 0x24<<1, 0x34<<1, 0x2c<<1 }, /* z (7a) */
{ 3, 0x08<<1, 0x36<<1, 0x41<<1 }, /* { (7b) */
{ 3, 0x00<<1, 0x7f<<1, 0x00<<1 }, /* | (7c) */
{ 3, 0x41<<1, 0x36<<1, 0x08<<1 }, /* } (7d) */
{ 3, 0x01<<1, 0x01<<1, 0x01<<1 }, /* ~ (7e) */
{ 3, 0x7E, 0x7E, 0x7E }, /* BLOCK (7f) */
{ 3, 0x6A, 0x54, 0x6A }, /* BLOCK SHADOW (80) */
};

PRIVATE const uint8 au8LcdFontHankaku[][4] =
{
{ 3, 0x10<<1, 0x28<<1, 0x10<<1 }, /* ｡ (a1) */
{ 3, 0x1f<<1, 0x01<<1, 0x01<<1 }, /* ｢ (a2) */
{ 3, 0x20<<1, 0x20<<1, 0x3e<<1 }, /* ｣ (a3) */
{ 3, 0x10<<1, 0x20<<1, 0x00<<1 }, /* ､ (a4) */
{ 3, 0x00<<1, 0x08<<1, 0x00<<1 }, /* ･ (a5) */
{ 3, 0x25<<1, 0x15<<1, 0x0f<<1 }, /* ｦ (a6) */
{ 3, 0x24<<1, 0x1c<<1, 0x0c<<1 }, /* ｧ (a7) */
{ 3, 0x10<<1, 0x38<<1, 0x04<<1 }, /* ｨ (a8) */
{ 3, 0x18<<1, 0x0c<<1, 0x38<<1 }, /* ｩ (a9) */
{ 3, 0x28<<1, 0x38<<1, 0x28<<1 }, /* ｪ (aa) */
{ 3, 0x28<<1, 0x18<<1, 0x3c<<1 }, /* ｫ (ab) */
{ 3, 0x08<<1, 0x3c<<1, 0x18<<1 }, /* ｬ (ac) */
{ 3, 0x28<<1, 0x38<<1, 0x20<<1 }, /* ｭ (ad) */
{ 3, 0x24<<1, 0x2c<<1, 0x3c<<1 }, /* ｮ (ae) */
{ 3, 0x2c<<1, 0x2c<<1, 0x1c<<1 }, /* ｯ (af) */
{ 3, 0x04<<1, 0x08<<1, 0x08<<1 }, /* ｰ (b0) */
{ 3, 0x21<<1, 0x1d<<1, 0x07<<1 }, /* ｱ (b1) */
{ 3, 0x08<<1, 0x3c<<1, 0x03<<1 }, /* ｲ (b2) */
{ 3, 0x06<<1, 0x23<<1, 0x1e<<1 }, /* ｳ (b3) */
{ 3, 0x22<<1, 0x3e<<1, 0x22<<1 }, /* ｴ (b4) */
{ 3, 0x12<<1, 0x0a<<1, 0x3f<<1 }, /* ｵ (b5) */
{ 3, 0x32<<1, 0x0f<<1, 0x3e<<1 }, /* ｶ (b6) */
{ 3, 0x0a<<1, 0x3f<<1, 0x0a<<1 }, /* ｷ (b7) */
{ 3, 0x24<<1, 0x13<<1, 0x0e<<1 }, /* ｸ (b8) */
{ 3, 0x27<<1, 0x1e<<1, 0x02<<1 }, /* ｹ (b9) */
{ 3, 0x22<<1, 0x22<<1, 0x3e<<1 }, /* ｺ (ba) */
{ 3, 0x27<<1, 0x12<<1, 0x0f<<1 }, /* ｻ (bb) */
{ 3, 0x25<<1, 0x25<<1, 0x10<<1 }, /* ｼ (bc) */
{ 3, 0x21<<1, 0x19<<1, 0x27<<1 }, /* ｽ (bd) */
{ 3, 0x3f<<1, 0x22<<1, 0x2e<<1 }, /* ｾ (be) */
{ 3, 0x21<<1, 0x16<<1, 0x0f<<1 }, /* ｿ (bf) */
{ 3, 0x24<<1, 0x1b<<1, 0x0e<<1 }, /* ﾀ (c0) */
{ 3, 0x25<<1, 0x1f<<1, 0x05<<1 }, /* ﾁ (c1) */
{ 3, 0x26<<1, 0x26<<1, 0x1e<<1 }, /* ﾂ (c2) */
{ 3, 0x25<<1, 0x1d<<1, 0x05<<1 }, /* ﾃ (c3) */
{ 3, 0x3f<<1, 0x04<<1, 0x08<<1 }, /* ﾄ (c4) */
{ 3, 0x24<<1, 0x1f<<1, 0x04<<1 }, /* ﾅ (c5) */
{ 3, 0x20<<1, 0x22<<1, 0x22<<1 }, /* ﾆ (c6) */
{ 3, 0x25<<1, 0x19<<1, 0x27<<1 }, /* ﾇ (c7) */
{ 3, 0x12<<1, 0x3b<<1, 0x16<<1 }, /* ﾈ (c8) */
{ 3, 0x20<<1, 0x10<<1, 0x0f<<1 }, /* ﾉ (c9) */
{ 3, 0x3c<<1, 0x01<<1, 0x3e<<1 }, /* ﾊ (ca) */
{ 3, 0x1f<<1, 0x24<<1, 0x24<<1 }, /* ﾋ (cb) */
{ 3, 0x21<<1, 0x11<<1, 0x0f<<1 }, /* ﾌ (cc) */
{ 3, 0x0c<<1, 0x03<<1, 0x1c<<1 }, /* ﾍ (cd) */
{ 3, 0x1a<<1, 0x3f<<1, 0x1a<<1 }, /* ﾎ (ce) */
{ 3, 0x09<<1, 0x19<<1, 0x27<<1 }, /* ﾏ (cf) */
{ 3, 0x22<<1, 0x2a<<1, 0x2a<<1 }, /* ﾐ (d0) */
{ 3, 0x38<<1, 0x27<<1, 0x30<<1 }, /* ﾑ (d1) */
{ 3, 0x32<<1, 0x0c<<1, 0x13<<1 }, /* ﾒ (d2) */
{ 3, 0x05<<1, 0x3f<<1, 0x25<<1 }, /* ﾓ (d3) */
{ 3, 0x02<<1, 0x3f<<1, 0x0e<<1 }, /* ﾔ (d4) */
{ 3, 0x21<<1, 0x3f<<1, 0x20<<1 }, /* ﾕ (d5) */
{ 3, 0x25<<1, 0x25<<1, 0x3f<<1 }, /* ﾖ (d6) */
{ 3, 0x25<<1, 0x25<<1, 0x1d<<1 }, /* ﾗ (d7) */
{ 3, 0x07<<1, 0x20<<1, 0x1f<<1 }, /* ﾘ (d8) */
{ 3, 0x3c<<1, 0x3f<<1, 0x20<<1 }, /* ﾙ (d9) */
{ 3, 0x3f<<1, 0x20<<1, 0x10<<1 }, /* ﾚ (da) */
{ 3, 0x3e<<1, 0x22<<1, 0x3e<<1 }, /* ﾛ (db) */
{ 3, 0x23<<1, 0x11<<1, 0x0f<<1 }, /* ﾜ (dc) */
{ 3, 0x21<<1, 0x21<<1, 0x18<<1 }, /* ﾝ (dd) */
{ 3, 0x01<<1, 0x00<<1, 0x01<<1 }, /* ﾞ (de) */
{ 3, 0x02<<1, 0x05<<1, 0x02<<1 }, /* ﾟ (df) */
};

PRIVATE const uint8 au8LcdFontTocos[][8] =
{
{ 7, 0x42, 0x5e, 0x2a, 0x7f, 0x2a, 0x5e, 0x42 }, /* 東 (456c) */
{ 7, 0x42, 0x2e, 0x4a, 0x7b, 0x0a, 0x2e, 0x42 }, /* 京 (357e) */

{ 7, 0x00, 0x42, 0x42, 0x42, 0x42, 0x42, 0x7e }, /* コ (2533) */
{ 7, 0x40, 0x42, 0x22, 0x22, 0x1a, 0x26, 0x40 }, /* ス (2539) */
{ 7, 0x00, 0x08, 0x09, 0x3f, 0x49, 0x49, 0x48 }, /* モ (2562) */

{ 7, 0x06, 0x3a, 0x2f, 0x3f, 0x6b, 0x7e, 0x46 }, /* 電 (4545) */
{ 7, 0x1a, 0x7f, 0x4a, 0x3d, 0x48, 0x3a, 0x55 }, /* 機 (3521) */
{ 7, 0x3e, 0x5b, 0x7e, 0x16, 0x7c, 0x55, 0x3e }, /* (株) (2d6a) */
};

/****************************************************************************/
/***        Exported Functions                                            ***/
/****************************************************************************/
/****************************************************************************
 *
 * NAME: vLcdFontReset
 *
 * DESCRIPTION:
 * Sets up font store. Font needs converting from horizontal to vertical
 * strips.
 *
 ****************************************************************************/
PUBLIC void vLcdFontReset(void)
{
}

/****************************************************************************
 *
 * NAME: pu8LcdFontGetChar
 *
 * DESCRIPTION:
 * Returns pointer to x bytes containing a character map, first byte contains
 * number of valid subsequent bytes.
 *
 * PARAMETERS:      Name            RW  Usage
 *                  u8Char          R   Ascii value of character
 *
 * RETURNS:
 * Pointer to character map.
 *
 ****************************************************************************/
PUBLIC uint8 *pu8LcdFontGetChar(uint8 u8Char)
{
	if ((u8Char >= 0x20) && (u8Char <= 0x80))
	{
		return (uint8 *) au8LcdFont[u8Char - 0x20];
	}

	if ((u8Char >= 0xa1) && (u8Char <= 0xdf))
	{
		return (uint8 *) au8LcdFontHankaku[u8Char - 0xa1];
	}

	if ((u8Char >= 0x90) && (u8Char <= 0x97))
	{
		return (uint8 *) au8LcdFontTocos[u8Char - 0x90];
	}

	/* Default is to return a space character */
	return (uint8 *) au8LcdFont[0];
}

/****************************************************************************/
/***        Local Functions                                               ***/
/****************************************************************************/

/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/

