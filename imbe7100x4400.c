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

static unsigned int Hamming15113Gen[11] = {
    0x4009, 0x200d, 0x100f, 0x080e, 0x0407, 0x020a, 0x0105, 0x008b, 0x004c, 0x0026, 0x0013
};

static unsigned int Hamming15113Table[16] = {
    0x0000, 0x0001, 0x0002, 0x0013, 0x0004, 0x0105, 0x0026, 0x0407,
    0x0008, 0x4009, 0x020A, 0x008b, 0x004C, 0x200D, 0x080E, 0x100F
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

static unsigned int mbe_7100x4400hamming1511 (unsigned int codeword, unsigned int *out)
{
    unsigned int i, errs = 0, ecc = 0, syndrome;
    for(i = 0; i < 11; i++) {
        if((codeword & Hamming15113Gen[i]) > 0xf)
            ecc ^= Hamming15113Gen[i];
    }
    syndrome = ecc ^ codeword;

    if (syndrome > 0) {
      errs++;
      codeword ^= Hamming15113Table[syndrome & 0x0f];
    }

    *out = (codeword >> 4);
    return (errs);
}

int mbe_eccImbe7100x4400Data (char imbe_fr[7][24], char *imbe_d)
{
  int i, j, errs;
  unsigned int hin = 0;
  char *imbe, gin[23], gout[23], hout[15];
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
          hin <<= 1;
          hin |= imbe_fr[i][14-j];
      }
      errs += mbe_7100x4400hamming1511 (hin, hout);
      for (j = 10; j >= 0; j--) {
          *imbe++ = (hout >> j);
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
  unsigned short pr[115], seed = 0;

  // create pseudo-random modulator
  for (i = 11; i < 18; i++) {
      seed <<= 1;
      seed |= imbe[0][i];
  }
  pr[0] = (16 * seed);
  for (i = 1; i < 101; i++) {
      pr[i] = (173 * pr[i - 1]) + 13849 - (65536 * (((173 * pr[i - 1]) + 13849) >> 16));
  }
  seed = pr[100];
  for (i = 1; i < 101; i++) {
      pr[i] >>= 15;
  }

  // demodulate imbe with pr
  k = 1;
  for (j = 23; j >= 0; j--) {
      imbe[1][j] ^= pr[k++];
  }

  for (j = 22; j >= 0; j--) {
      imbe[2][j] ^= pr[k++];
  }

  for (j = 22; j >= 0; j--) {
      imbe[3][j] ^= pr[k++];
  }

  for (j = 14; j >= 0; j--) {
      imbe[4][j] ^= pr[k++];
  }

  for (j = 14; j >= 0; j--) {
      imbe[5][j] ^= pr[k++];
  }
}

void
mbe_convertImbe7100to7200 (char *imbe_d)
{
  int i, j, k, b0 = 0;
  unsigned int K, L;
  char tmp_imbe[88];
  float w0;

  // decode fundamental frequency w0 from b0
  for (i = 0; i < 6; i++) {
    b0 <<= 1;
    b0 |= imbe_d[6-i];
  }
  b0 |= ((imbe_d[87] << 7) | (imbe_d[86] << 6));
  w0 = ((4.0f * (float)M_PI) / ((float) b0 + 39.5f));

  // decode L from w0
  //L = (int) (0.9254 * (int) (((float)M_PI / w0) + 0.25f));
  L = (int) (0.9254f * 0.25f * ((float)b0 + 39.5f));

  // decode K from L
  if (L < 37) {
      K = ((L + 2) / 3);
  } else {
      K = 12;
  }

  // rearrange bits from imbe7100x4400 format to imbe7200x4400 format
  tmp_imbe[87] = imbe_d[0];     // "status"/zero bit
  tmp_imbe[48 + K] = imbe_d[42];        // b2.2
  tmp_imbe[49 + K] = imbe_d[43];        // b2.1

  for (i = 0; i < K; i++) {
      tmp_imbe[i+48] = imbe_d[i+44];  // b1
  }

  j = 0;
  k = 1;
  while (j < 87) {
      tmp_imbe[j] = imbe_d[k];
      if (++j == 48) {
          j += (K + 2);         // skip over b1, b2.2, b2.1 on dest
      }
      if (++k == 42) {
          k += (K + 2);         // skip over b2.2, b2.1, b1 on src
      }
  }

  //copy new format back to imbe_d
  for (i = 0; i < 88; i++) {
      imbe_d[i] = tmp_imbe[i];
  }
}

void mbe_processImbe7100x4400Framef (float *aout_buf, int *errs, int *errs2, char *err_str, char imbe_fr[7][24], char imbe_d[88],
                                     mbe_parms * cur_mp, mbe_parms * prev_mp, mbe_parms * prev_mp_enhanced, int uvquality)
{
  *errs2 = mbe_eccImbe7100x4400C0 (imbe_fr);
  mbe_demodulateImbe7100x4400Data (imbe_fr);
  *errs2 += mbe_eccImbe7100x4400Data (imbe_fr, imbe_d);
  mbe_convertImbe7100to7200 (imbe_d);
  mbe_processImbe4400Dataf (aout_buf, errs, errs2, err_str, imbe_d, cur_mp, prev_mp, prev_mp_enhanced, uvquality);
}

