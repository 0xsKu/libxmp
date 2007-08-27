/* nomarch 1.3 - extract old `.arc' archives.
 * Copyright (C) 2001,2002 Russell Marks. See main.c for license details.
 *
 * Modified for xmp by Claudio Matsuoka, Aug 2007
 *
 * readlzw.h
 */

#define ALIGN4(x) ((((x) + 3) >> 2) << 2)

#define NOMARCH_QUIRK_DSYM	1	/* Digital Symphony LZW quirk */

extern int nomarch_input_size;		/* Hack for xmp/dsym */

extern unsigned char *convert_lzw_dynamic(unsigned char *data_in,
                                          int bits,int use_rle,
                                          unsigned long in_len,
                                          unsigned long orig_len,
					  int q);
