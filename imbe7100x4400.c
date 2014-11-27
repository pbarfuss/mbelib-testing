/*
 * Copyright (C) 2010 mbelib Author
 * GPG Key ID: 0xEA5EFE2C (9E7A 5527 9CDC EBF7 BF1B  D772 4F98 E863 EA5E FE2C)
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include "mbelib.h"

static const int hammingGenerator[4] = {
  0x7f08, 0x78e4, 0x66d2, 0x55b1
};

static const int imbe7100x4400hammingGenerator[4] = {
  0x7ac8, 0x3d64, 0x1eb2, 0x7591
};

static const int hammingMatrix[16] = {
  0x0, 0x1, 0x2, 0x4, 0x8, 0x10, 0x20, 0x40, 0x80, 0x100, 0x200, 0x400, 0x800, 0x1000, 0x2000, 0x4000
};

int mbe_eccImbe7100x4400C0 (char imbe_fr[7][24])
{
  int j, errs;
  char in[23], out[23];

  for (j = 0; j < 18; j++) {
      in[j] = imbe_fr[0][j + 1];
  }
  for (j = 18; j < 23; j++) {
      in[j] = 0;
  }

  errs = mbe_golay2312 (in, out);
  for (j = 0; j < 18; j++) {
      imbe_fr[0][j + 1] = out[j];
  }

  return (errs);
}

static int mbe_7100x4400hamming1511 (char *in, char *out)
{
  int i, j, errs = 0, block = 0, syndrome = 0, stmp, stmp2;

  for (i = 14; i >= 0; i--) {
      block <<= 1;
      block |= in[i];
  }

  for (i = 0; i < 4; i++) {
      syndrome <<= 1;
      stmp = block;
      stmp &= imbe7100x4400hammingGenerator[i];
      stmp2 = (stmp % 2);
      for (j = 0; j < 14; j++) {
          stmp >>= 1;
          stmp2 ^= (stmp % 2);
      }
      syndrome |= stmp2;
  }

  if (syndrome > 0) {
      errs++;
      block ^= hammingMatrix[syndrome];
  }

  for (i = 14; i >= 0; i--) {
      out[i] = (block & 0x4000) >> 14;
      block = block << 1;
  }
  return (errs);
}

int mbe_eccImbe7100x4400Data (char imbe_fr[7][24], char *imbe_d)
{
  int i, j, errs;
  char *imbe, gin[23], gout[23], hin[15], hout[15];
  errs = 0;
  imbe = imbe_d;

  // just copy C0
  for (j = 18; j > 11; j--) {
      *imbe = imbe_fr[0][j];
      imbe++;
  }

  // ecc and copy C1
  for (j = 0; j < 23; j++) {
      gin[j] = imbe_fr[1][j + 1];
  }
  errs = mbe_golay2312 (gin, gout);
  for (j = 22; j > 10; j--) {
      *imbe = gout[j];
      imbe++;
  }

  // ecc and copy C2, C3
  for (i = 2; i < 4; i++) {
      for (j = 0; j < 23; j++) {
          gin[j] = imbe_fr[i][j];
      }
      errs += mbe_golay2312 (gin, gout);
      for (j = 22; j > 10; j--) {
          *imbe = gout[j];
          imbe++;
      }
  }
  // ecc and copy C4, C5
  for (i = 4; i < 6; i++) {
      for (j = 0; j < 15; j++) {
          hin[j] = imbe_fr[i][j];
      }
      errs += mbe_7100x4400hamming1511 (hin, hout);
      for (j = 14; j >= 4; j--) {
          *imbe = hout[j];
          imbe++;
      }
  }

  // just copy C6
  for (j = 22; j >= 0; j--) {
      *imbe = imbe_fr[6][j];
      imbe++;
  }

  return (errs);
}

void mbe_demodulateImbe7100x4400Data (char imbe[7][24])
{
  int i, j, k;
  unsigned short pr[115];
  unsigned short seed;
  char tmpstr[24];

  // create pseudo-random modulator
  j = 0;
  tmpstr[7] = 0;
  for (i = 18; i > 11; i--)
    {
      tmpstr[j] = (imbe[0][i] + 48);
      j++;
    }
  seed = strtol (tmpstr, NULL, 2);
  pr[0] = (16 * seed);
  for (i = 1; i < 101; i++)
    {
      pr[i] = (173 * pr[i - 1]) + 13849 - (65536 * (((173 * pr[i - 1]) + 13849) / 65536));
    }
  seed = pr[100];
  for (i = 1; i < 101; i++)
    {
      pr[i] = pr[i] / 32768;
    }

  // demodulate imbe with pr
  k = 1;
  for (j = 23; j >= 0; j--)
    {
      imbe[1][j] = ((imbe[1][j]) ^ pr[k]);
      k++;
    }

  for (i = 2; i < 4; i++)
    {
      for (j = 22; j >= 0; j--)
        {
          imbe[i][j] = ((imbe[i][j]) ^ pr[k]);
          k++;
        }
    }

  for (i = 4; i < 6; i++)
    {
      for (j = 14; j >= 0; j--)
        {
          imbe[i][j] = ((imbe[i][j]) ^ pr[k]);
          k++;
        }
    }
}

void
mbe_convertImbe7100to7200 (char *imbe_d)
{

  int i, j, k, K, L, b0;
  char tmpstr[9];
  char tmp_imbe[88];
  float w0;

  // decode fundamental frequency w0 from b0
  tmpstr[8] = 0;
  tmpstr[0] = imbe_d[1] + 48;
  tmpstr[1] = imbe_d[2] + 48;
  tmpstr[2] = imbe_d[3] + 48;
  tmpstr[3] = imbe_d[4] + 48;
  tmpstr[4] = imbe_d[5] + 48;
  tmpstr[5] = imbe_d[6] + 48;
  tmpstr[6] = imbe_d[86] + 48;
  tmpstr[7] = imbe_d[87] + 48;
  b0 = strtol (tmpstr, NULL, 2);
  w0 = ((float) (4 * M_PI) / (float) ((float) b0 + 39.5));

  // decode L from w0
  L = (int) (0.9254 * (int) ((M_PI / w0) + 0.25));

  // decode K from L
  if (L < 37)
    {
      K = (int) ((float) (L + 2) / (float) 3);
    }
  else
    {
      K = 12;
    }

  // rearrange bits from imbe7100x4400 format to imbe7200x4400 format
  tmp_imbe[87] = imbe_d[0];     // "status"/zero bit
  tmp_imbe[48 + K] = imbe_d[42];        // b2.2
  tmp_imbe[49 + K] = imbe_d[43];        // b2.1

  k = 44;
  j = 48;
  for (i = 0; i < K; i++)
    {
      tmp_imbe[j] = imbe_d[k];  // b1
      j++;
      k++;
    }

  j = 0;
  k = 1;
  while (j < 87)
    {
      tmp_imbe[j] = imbe_d[k];
      if (++j == 48)
        {
          j += (K + 2);         // skip over b1, b2.2, b2.1 on dest
        }
      if (++k == 42)
        {
          k += (K + 2);         // skip over b2.2, b2.1, b1 on src
        }
    }

  //copy new format back to imbe_d

  for (i = 0; i < 88; i++)
    {
      imbe_d[i] = tmp_imbe[i];
    }
}

void mbe_processImbe7100x4400Framef (float *aout_buf, int *errs, int *errs2, char *err_str, char imbe_fr[7][24], char imbe_d[88],
                                     mbe_parms * cur_mp, mbe_parms * prev_mp, mbe_parms * prev_mp_enhanced, int uvquality)
{
  *errs = 0;
  *errs2 = 0;
  *errs = mbe_eccImbe7100x4400C0 (imbe_fr);
  mbe_demodulateImbe7100x4400Data (imbe_fr);
  *errs2 = *errs;
  *errs2 += mbe_eccImbe7100x4400Data (imbe_fr, imbe_d);
  mbe_convertImbe7100to7200 (imbe_d);
  mbe_processImbe4400Dataf (aout_buf, errs, errs2, err_str, imbe_d, cur_mp, prev_mp, prev_mp_enhanced, uvquality);
}

