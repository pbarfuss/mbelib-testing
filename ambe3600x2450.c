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
#include <string.h>
#include <math.h>
#include "mbelib.h"
#include "ambe3600x2450_const.h"

int mbe_decodeAmbe2450Parms (char *ambe_d, mbe_parms * cur_mp, mbe_parms * prev_mp)
{
  int ji, i, j, k, l, L, m, am, ak;
  int intkl[57];
  int b0, b1, b2, b3, b4, b5, b6, b7, b8;
  float f0, Cik[5][18], flokl[57], deltal[57];
  float Sum42, Sum43, Tl[57], Gm[9], Ri[9], sum, c1, c2;
  int silence;
  int Ji[5], jl;
  float deltaGamma, BigGamma;
  float unvc, rconst;

  silence = 0;

#ifdef AMBE_DEBUG
  printf ("\n");
#endif

  // copy repeat from prev_mp
  cur_mp->repeat = prev_mp->repeat;

  // decode fundamental frequency w0 from b0
  b0 = 0;
  b0 |= ambe_d[0]<<6;
  b0 |= ambe_d[1]<<5;
  b0 |= ambe_d[2]<<4;
  b0 |= ambe_d[3]<<3;
  b0 |= ambe_d[37]<<2;
  b0 |= ambe_d[38]<<1;
  b0 |= ambe_d[39];
  if ((b0 >= 120) && (b0 <= 123)) // if w0 bits are 1111000, 1111001, 1111010 or 1111011, frame is erasure
    {
#ifdef AMBE_DEBUG
      printf ("Erasure Frame\n");
#endif
      return (2);
    }
  else if ((b0 == 124) || (b0 == 125)) // if w0 bits are 1111100 or 1111101, frame is silence
    {
#ifdef AMBE_DEBUG
      printf ("Silence Frame\n");
#endif
      silence = 1;
      cur_mp->w0 = (1.0f / 16.0f);
      f0 = (float) 1 / (float) 32;
      L = 14;
      cur_mp->L = 14;
      for (l = 1; l <= L; l++)
        {
          cur_mp->Vl[l] = 0;
        }
    }
  else if ((b0 == 126) || (b0 == 127)) // if w0 bits are 1111110 or 1111111, frame is tone
    {
#ifdef AMBE_DEBUG
      printf ("Tone Frame\n");
#endif
      return (3);
    }

  if (silence == 0)
    {
      // w0 from specification document
      //f0 = AmbeW0table[b0];
      // w0 using an interpolation of the above table
      //f0 = 1.0f / mbe_expf ((float)M_LN2 * ((float)b0 + 195.75f) / 45.31f);
      f0 = 1.0f / mbe_expf ((float)M_LN2 * ((float)b0 + 195.75f) * 0.02207f);
      cur_mp->w0 = f0 * 2.0f;
    }

  unvc = (float) 0.2046 / mbe_sqrtf ((float)M_PI * cur_mp->w0);
  //unvc = (float) 1;
  //unvc = (float) 0.2046 / sqrtf (f0);

  // decode L
  if (silence == 0)
    {
      // L from specification document
      // lookup L in tabl3
      L = AmbeLtable[b0];
      // L formula from patent filings
      //L=(int)((float)0.4627 / f0);
      cur_mp->L = L;
    }

  // decode V/UV parameters
  // load b1 from ambe_d
  b1 = 0;
  b1 |= ambe_d[4]<<4;
  b1 |= ambe_d[5]<<3;
  b1 |= ambe_d[6]<<2;
  b1 |= ambe_d[7]<<1;
  b1 |= ambe_d[35];

  for (l = 1; l <= L; l++)
    {
      // jl from specification document
      jl = (int) ((float) l * (float) 16.0 * f0);
      // jl from patent filings?
      //jl = (int)(((float)l * (float)16.0 * f0) + 0.25);

      if (silence == 0)
        {
          cur_mp->Vl[l] = AmbeVuv[b1][jl];
        }
#ifdef AMBE_DEBUG
      printf ("jl[%i]:%i Vl[%i]:%i\n", l, jl, l, cur_mp->Vl[l]);
#endif
    }
#ifdef AMBE_DEBUG
  printf ("\nb0:%i w0:%f L:%i b1:%i\n", b0, cur_mp->w0, L, b1);
#endif

  // decode gain vector
  // load b2 from ambe_d
  b2 = 0;
  b2 |= ambe_d[8]<<4;
  b2 |= ambe_d[9]<<3;
  b2 |= ambe_d[10]<<2;
  b2 |= ambe_d[11]<<1;
  b2 |= ambe_d[36];

  deltaGamma = AmbeDg[b2];
  cur_mp->gamma = deltaGamma + ((float) 0.5 * prev_mp->gamma);
#ifdef AMBE_DEBUG
  printf ("b2: %i, deltaGamma: %f gamma: %f gamma-1: %f\n", b2, deltaGamma, cur_mp->gamma, prev_mp->gamma);
#endif

  // decode PRBA vectors
  Gm[1] = 0;

  // load b3 from ambe_d
  b3 = 0;
  b3 |= ambe_d[12]<<8;
  b3 |= ambe_d[13]<<7;
  b3 |= ambe_d[14]<<6;
  b3 |= ambe_d[15]<<5;
  b3 |= ambe_d[16]<<4;
  b3 |= ambe_d[17]<<3;
  b3 |= ambe_d[18]<<2;
  b3 |= ambe_d[19]<<1;
  b3 |= ambe_d[40];
  Gm[2] = AmbePRBA24[b3][0];
  Gm[3] = AmbePRBA24[b3][1];
  Gm[4] = AmbePRBA24[b3][2];

  // load b4 from ambe_d
  b4 = 0;
  b4 |= ambe_d[20]<<6;
  b4 |= ambe_d[21]<<5;
  b4 |= ambe_d[22]<<4;
  b4 |= ambe_d[23]<<3;
  b4 |= ambe_d[41]<<2;
  b4 |= ambe_d[42]<<1;
  b4 |= ambe_d[43];
  Gm[5] = AmbePRBA58[b4][0];
  Gm[6] = AmbePRBA58[b4][1];
  Gm[7] = AmbePRBA58[b4][2];
  Gm[8] = AmbePRBA58[b4][3];

#ifdef AMBE_DEBUG
  printf ("b3: %i Gm[2]: %f Gm[3]: %f Gm[4]: %f b4: %i Gm[5]: %f Gm[6]: %f Gm[7]: %f Gm[8]: %f\n", b3, Gm[2], Gm[3], Gm[4], b4, Gm[5], Gm[6], Gm[7], Gm[8]);
#endif

  // compute Ri
  for (i = 1; i <= 8; i++) {
      sum = 0;
      for (m = 1; m <= 8; m++) {
          if (m == 1) { am = 1; }
          else { am = 2; }
          sum = sum + ((float) am * Gm[m] * mbe_cosf (((float)M_PI * (float) (m - 1) * ((float) i - (float) 0.5)) / (float) 8));
        }
      Ri[i] = sum;
#ifdef AMBE_DEBUG
      printf ("R%i: %f ", i, Ri[i]);
#endif
    }
#ifdef AMBE_DEBUG
  printf ("\n");
#endif

  // generate first to elements of each Ci,k block from PRBA vector
  rconst = ((float) 1 / ((float) 2 * (float)M_SQRT2));
  Cik[1][1] = 0.5f *(Ri[1] + Ri[2]);
  Cik[1][2] = rconst * (Ri[1] - Ri[2]);
  Cik[2][1] = 0.5f *(Ri[3] + Ri[4]);
  Cik[2][2] = rconst * (Ri[3] - Ri[4]);
  Cik[3][1] = 0.5f *(Ri[5] + Ri[6]);
  Cik[3][2] = rconst * (Ri[5] - Ri[6]);
  Cik[4][1] = 0.5f *(Ri[7] + Ri[8]);
  Cik[4][2] = rconst * (Ri[7] - Ri[8]);

  // decode HOC

  // load b5 from ambe_d
  b5 = 0;
  b5 |= ambe_d[24]<<4;
  b5 |= ambe_d[25]<<3;
  b5 |= ambe_d[26]<<2;
  b5 |= ambe_d[27]<<1;
  b5 |= ambe_d[44];

  // load b6 from ambe_d
  b6 = 0;
  b6 |= ambe_d[28]<<3;
  b6 |= ambe_d[29]<<2;
  b6 |= ambe_d[30]<<1;
  b6 |= ambe_d[45];

  // load b7 from ambe_d
  b7 = 0;
  b7 |= ambe_d[31]<<3;
  b7 |= ambe_d[32]<<2;
  b7 |= ambe_d[33]<<1;
  b7 |= ambe_d[46];

  // load b8 from ambe_d
  b8 = 0;
  b8 |= ambe_d[34]<<2;
  b8 |= ambe_d[47]<<1;
  b8 |= ambe_d[48];

  // lookup Ji
  Ji[1] = AmbeLmprbl[L][0];
  Ji[2] = AmbeLmprbl[L][1];
  Ji[3] = AmbeLmprbl[L][2];
  Ji[4] = AmbeLmprbl[L][3];
#ifdef AMBE_DEBUG
  printf ("Ji[1]: %i Ji[2]: %i Ji[3]: %i Ji[4]: %i\n", Ji[1], Ji[2], Ji[3], Ji[4]);
  printf ("b5: %i b6: %i b7: %i b8: %i\n", b5, b6, b7, b8);
#endif

  // Load Ci,k with the values from the HOC tables
  // there appear to be a couple typos in eq. 37 so we will just do what makes sense
  // (3 <= k <= Ji and k<=6)
  for (k = 3; k <= Ji[1]; k++)
    {
      if (k > 6)
        {
          Cik[1][k] = 0;
        }
      else
        {
          Cik[1][k] = AmbeHOCb5[b5][k - 3];
#ifdef AMBE_DEBUG
          printf ("C1,%i: %f ", k, Cik[1][k]);
#endif
        }
    }
  for (k = 3; k <= Ji[2]; k++)
    {
      if (k > 6)
        {
          Cik[2][k] = 0;
        }
      else
        {
          Cik[2][k] = AmbeHOCb6[b6][k - 3];
#ifdef AMBE_DEBUG
          printf ("C2,%i: %f ", k, Cik[2][k]);
#endif
        }
    }
  for (k = 3; k <= Ji[3]; k++)
    {
      if (k > 6)
        {
          Cik[3][k] = 0;
        }
      else
        {
          Cik[3][k] = AmbeHOCb7[b7][k - 3];
#ifdef AMBE_DEBUG
          printf ("C3,%i: %f ", k, Cik[3][k]);
#endif
        }
    }
  for (k = 3; k <= Ji[4]; k++)
    {
      if (k > 6)
        {
          Cik[4][k] = 0;
        }
      else
        {
          Cik[4][k] = AmbeHOCb8[b8][k - 3];
#ifdef AMBE_DEBUG
          printf ("C4,%i: %f ", k, Cik[4][k]);
#endif
        }
    }
#ifdef AMBE_DEBUG
  printf ("\n");
#endif

  // inverse DCT each Ci,k to give ci,j (Tl)
  l = 1;
  for (i = 1; i <= 4; i++)
    {
      ji = Ji[i];
      for (j = 1; j <= ji; j++)
        {
          sum = 0;
          for (k = 1; k <= ji; k++)
            {
              if (k == 1)
                {
                  ak = 1;
                }
              else
                {
                  ak = 2;
                }
#ifdef AMBE_DEBUG
              printf ("j: %i Cik[%i][%i]: %f ", j, i, k, Cik[i][k]);
#endif
              sum = sum + ((float) ak * Cik[i][k] * mbe_cosf (((float)M_PI * (float) (k - 1) * ((float) j - (float) 0.5)) / (float) ji));
            }
          Tl[l] = sum;
#ifdef AMBE_DEBUG
          printf ("Tl[%i]: %f\n", l, Tl[l]);
#endif
          l++;
        }
    }

  // determine log2Ml by applying ci,j to previous log2Ml

  // fix for when L > L(-1)
  if (cur_mp->L > prev_mp->L)
    {
      for (l = (prev_mp->L) + 1; l <= cur_mp->L; l++)
        {
          prev_mp->Ml[l] = prev_mp->Ml[prev_mp->L];
          prev_mp->log2Ml[l] = prev_mp->log2Ml[prev_mp->L];
        }
    }
  prev_mp->log2Ml[0] = prev_mp->log2Ml[1];
  prev_mp->Ml[0] = prev_mp->Ml[1];

  // Part 1
  Sum43 = 0;
  for (l = 1; l <= cur_mp->L; l++)
    {
      // eq. 40
      flokl[l] = ((float) prev_mp->L / (float) cur_mp->L) * (float) l;
      intkl[l] = (int) (flokl[l]);
#ifdef AMBE_DEBUG
      printf ("flok%i: %f, intk%i: %i ", l, flokl[l], l, intkl[l]);
#endif
      // eq. 41
      deltal[l] = flokl[l] - (float) intkl[l];
#ifdef AMBE_DEBUG
      printf ("delta%i: %f ", l, deltal[l]);
#endif
      // eq 43
      Sum43 = Sum43 + ((((float) 1 - deltal[l]) * prev_mp->log2Ml[intkl[l]]) + (deltal[l] * prev_mp->log2Ml[intkl[l] + 1]));
    }
  Sum43 = (((float) 0.65 / (float) cur_mp->L) * Sum43);
#ifdef AMBE_DEBUG
  printf ("\n");
  printf ("Sum43: %f\n", Sum43);
#endif

  // Part 2
  Sum42 = 0;
  for (l = 1; l <= cur_mp->L; l++)
    {
      Sum42 += Tl[l];
    }
  Sum42 = Sum42 / (float) cur_mp->L;
  //BigGamma = cur_mp->gamma - ((float) 0.5 * (logf((float) cur_mp->L) / logf((float) 2))) - Sum42;
  BigGamma = cur_mp->gamma - (0.5f * AmbeLog2f[(unsigned int)cur_mp->L]) - Sum42;

  // Part 3
  for (l = 1; l <= cur_mp->L; l++) {
      float tmp;
      c1 = ((float) 0.65 * ((float) 1 - deltal[l]) * prev_mp->log2Ml[intkl[l]]);
      c2 = ((float) 0.65 * deltal[l] * prev_mp->log2Ml[intkl[l] + 1]);
      cur_mp->log2Ml[l] = Tl[l] + c1 + c2 - Sum43 + BigGamma;
      tmp = mbe_expf ((float) 0.693 * cur_mp->log2Ml[l]);
      // inverse log to generate spectral amplitudes
      if (cur_mp->Vl[l] == 1) {
          cur_mp->Ml[l] = tmp;
      } else {
          cur_mp->Ml[l] = unvc * tmp;
      }
#ifdef AMBE_DEBUG
      printf ("flokl[%i]: %f, intkl[%i]: %i ", l, flokl[l], l, intkl[l]);
      printf ("deltal[%i]: %f ", l, deltal[l]);
      printf ("prev_mp->log2Ml[%i]: %f\n", l, prev_mp->log2Ml[intkl[l]]);
      printf ("BigGamma: %f c1: %f c2: %f Sum43: %f Tl[%i]: %f log2Ml[%i]: %f Ml[%i]: %f\n", BigGamma, c1, c2, Sum43, l, Tl[l], l, cur_mp->log2Ml[l], l, cur_mp->Ml[l]);
#endif
  }

  return (0);
}

void mbe_processAmbe2450Dataf (float *aout_buf, int *errs2, char *err_str, char ambe_d[49],
                               mbe_parms * cur_mp, mbe_parms * prev_mp, mbe_parms * prev_mp_enhanced, unsigned int uvquality)
{
  unsigned int i, bad;
  for (i = 0; i < *errs2; i++) {
      *err_str++ = '=';
  }
  bad = mbe_decodeAmbe2450Parms (ambe_d, cur_mp, prev_mp);
  if (bad == 0) {
      if (*errs2 > 3) {
          mbe_useLastMbeParms (cur_mp, prev_mp);
          cur_mp->repeat++;
          *err_str++ = 'R';
      } else {
          cur_mp->repeat = 0;
      }
      if (cur_mp->repeat <= 3) {
          mbe_moveMbeParms (cur_mp, prev_mp);
          mbe_spectralAmpEnhance (cur_mp);
          mbe_synthesizeSpeechf (aout_buf, cur_mp, prev_mp_enhanced, uvquality);
          mbe_moveMbeParms (cur_mp, prev_mp_enhanced);
      } else {
          *err_str++ = 'M';
          mbe_synthesizeSilencef (aout_buf);
          mbe_initMbeParms (cur_mp, prev_mp, prev_mp_enhanced);
      }
      *err_str = 0;
  } else {
    if (bad == 2) {
      // Erasure frame
      *err_str++ = 'E';
      cur_mp->repeat = 0;
    } else if (bad == 3) {
      // Tone Frame
      *err_str++ = 'T';
      cur_mp->repeat = 0;
    }
    *err_str = 0;
    mbe_synthesizeSilencef (aout_buf);
    mbe_initMbeParms (cur_mp, prev_mp, prev_mp_enhanced);
  }
}

