/* The original file utf8.h was taken from the source of the
 * GNU LIBICONV Library and slightly modified. */

/*
 * Copyright (C) 1999-2001 Free Software Foundation, Inc. 
 * Copyright (C) 2003 Adrian Daerr 
 * This file is part of xletters. It is distributed under the terms of
 * the GNU Public License, a copy of which should be with this
 * software.
 *
 * xletters is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * xletters is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with xletters; see the file COPYING.txt. If not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * UNICODE
 */

typedef u_int32_t ucs4_t;
typedef ucs4_t wchar;
#define UCS4_NULL (ucs4_t)0
#define UCS4_LF (ucs4_t)0xa
#define UCS4_SPC (ucs4_t)0x20
#define UCS4_POINT (ucs4_t)0x2e
#define UCS4_SLASH (ucs4_t)0x2f 
#define UCS4_LT (ucs4_t)0x3c
#define UCS4_GT (ucs4_t)0x3e 
#define UCS4_LSQUAREBRACKET (ucs4_t)0x5b
#define UCS4_RSQUAREBRACKET (ucs4_t)0x5d
/* following is no good if machine uses e.g. EBDIC instead of ASCII */
#define UCS4(x) ((ucs4_t)(unsigned char)x)
#define ASCII(x) ((char)(x & 0xff))

/*
 * UTF-8
 */

#define RET_TOOFEW -1 /* to few chars in input stream */
#define RET_TOOSMALL -2 /* not enough space in output buffer */
#define RET_ILSEQ -3 /* not a permissible UTF-8 sequence */
#define RET_ILUNI -4 /* not a permissible UCS-4 character */

/* Specification: RFC 2279 */

/* The following function converts a UTF-8 coded Unicode character
   (pointed to by s) into a UCS-4 coded character (pointed to by pwc).
   The argument n indicates how many chars are available in the input
   stream s. The return value indicates exactly how many chars were
   consumed from the UTF-8 stream at s. If the end of stream is
   reached before the UCS-4 character is complete, the function
   returns RET_TOOFEW. If the stream does not comply with RFC 2279,
   RET_ILSEQ is returned. */
static int utf8_mbtowc (ucs4_t *pwc, const unsigned char *s, int n)
{
  unsigned char c = s[0];

  if (c < 0x80) {
    *pwc = c;
    return 1;
  } else if (c < 0xc2) {
    return RET_ILSEQ;
  } else if (c < 0xe0) {
    if (n < 2)
      return RET_TOOFEW;
    if (!((s[1] ^ 0x80) < 0x40))
      return RET_ILSEQ;
    *pwc = ((ucs4_t) (c & 0x1f) << 6)
           | (ucs4_t) (s[1] ^ 0x80);
    return 2;
  } else if (c < 0xf0) {
    if (n < 3)
      return RET_TOOFEW;
    if (!((s[1] ^ 0x80) < 0x40 && (s[2] ^ 0x80) < 0x40
          && (c >= 0xe1 || s[1] >= 0xa0)))
      return RET_ILSEQ;
    *pwc = ((ucs4_t) (c & 0x0f) << 12)
           | ((ucs4_t) (s[1] ^ 0x80) << 6)
           | (ucs4_t) (s[2] ^ 0x80);
    return 3;
  } else if (c < 0xf8 && sizeof(ucs4_t)*8 >= 32) {
    if (n < 4)
      return RET_TOOFEW;
    if (!((s[1] ^ 0x80) < 0x40 && (s[2] ^ 0x80) < 0x40
          && (s[3] ^ 0x80) < 0x40
          && (c >= 0xf1 || s[1] >= 0x90)))
      return RET_ILSEQ;
    *pwc = ((ucs4_t) (c & 0x07) << 18)
           | ((ucs4_t) (s[1] ^ 0x80) << 12)
           | ((ucs4_t) (s[2] ^ 0x80) << 6)
           | (ucs4_t) (s[3] ^ 0x80);
    return 4;
  } else if (c < 0xfc && sizeof(ucs4_t)*8 >= 32) {
    if (n < 5)
      return RET_TOOFEW;
    if (!((s[1] ^ 0x80) < 0x40 && (s[2] ^ 0x80) < 0x40
          && (s[3] ^ 0x80) < 0x40 && (s[4] ^ 0x80) < 0x40
          && (c >= 0xf9 || s[1] >= 0x88)))
      return RET_ILSEQ;
    *pwc = ((ucs4_t) (c & 0x03) << 24)
           | ((ucs4_t) (s[1] ^ 0x80) << 18)
           | ((ucs4_t) (s[2] ^ 0x80) << 12)
           | ((ucs4_t) (s[3] ^ 0x80) << 6)
           | (ucs4_t) (s[4] ^ 0x80);
    return 5;
  } else if (c < 0xfe && sizeof(ucs4_t)*8 >= 32) {
    if (n < 6)
      return RET_TOOFEW;
    if (!((s[1] ^ 0x80) < 0x40 && (s[2] ^ 0x80) < 0x40
          && (s[3] ^ 0x80) < 0x40 && (s[4] ^ 0x80) < 0x40
          && (s[5] ^ 0x80) < 0x40
          && (c >= 0xfd || s[1] >= 0x84)))
      return RET_ILSEQ;
    *pwc = ((ucs4_t) (c & 0x01) << 30)
           | ((ucs4_t) (s[1] ^ 0x80) << 24)
           | ((ucs4_t) (s[2] ^ 0x80) << 18)
           | ((ucs4_t) (s[3] ^ 0x80) << 12)
           | ((ucs4_t) (s[4] ^ 0x80) << 6)
           | (ucs4_t) (s[5] ^ 0x80);
    return 6;
  } else
    return RET_ILSEQ;
}

/* The following function converts the UCS-4 Unicode character wc into
   a UTF-8 sequence (stored in memory beginning at r). The argument n
   indicates how many chars may be output at most (n == 0 is
   acceptable). The return value indicates exactly how many chars were
   produced at r. There is not enough room in the output buffer at r
   (i.e. wc needs more than n chars in UTF-8), the function returns
   RET_TOOSMALL. If wc is not a valid Unicode character, the function
   returns RET_ILUNI. */
static int utf8_wctomb (unsigned char *r, ucs4_t wc, int n)
{
  int count;
  if (wc < 0x80)
    count = 1;
  else if (wc < 0x800)
    count = 2;
  else if (wc < 0x10000)
    count = 3;
  else if (wc < 0x200000)
    count = 4;
  else if (wc < 0x4000000)
    count = 5;
  else if (wc <= 0x7fffffff)
    count = 6;
  else
    return RET_ILUNI;
  if (n < count)
    return RET_TOOSMALL;
  switch (count) { /* note: code falls through cases! */
    case 6: r[5] = 0x80 | (wc & 0x3f); wc = wc >> 6; wc |= 0x4000000;
    case 5: r[4] = 0x80 | (wc & 0x3f); wc = wc >> 6; wc |= 0x200000;
    case 4: r[3] = 0x80 | (wc & 0x3f); wc = wc >> 6; wc |= 0x10000;
    case 3: r[2] = 0x80 | (wc & 0x3f); wc = wc >> 6; wc |= 0x800;
    case 2: r[1] = 0x80 | (wc & 0x3f); wc = wc >> 6; wc |= 0xc0;
    case 1: r[0] = wc;
  }
  return count;
}
