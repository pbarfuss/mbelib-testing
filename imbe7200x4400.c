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
#include "imbe7200x4400_const.h"

int
mbe_decodeImbe4400Parms (char *imbe_d, mbe_parms * cur_mp, mbe_parms * prev_mp)
{
  int Bm, ji, b, i, j, k, l, L, K, L9, m, am, ak;
  int intkl[57];
  int b0, b2, bm;
  float Cik[7][11], rho, flokl[57], deltal[57];
  float Sum77, Tl[57], Gm[7], Ri[7], sum, c1, c2;
  const float *ba1, *ba2;
  char tmpstr[13];
  const int *bo1, *bo2;
  char bb[58][12];


  // copy repeat from prev_mp
  cur_mp->repeat = prev_mp->repeat;

  // decode fundamental frequency w0 from b0
  tmpstr[8] = 0;
  tmpstr[0] = imbe_d[0] + 48;
  tmpstr[1] = imbe_d[1] + 48;
  tmpstr[2] = imbe_d[2] + 48;
  tmpstr[3] = imbe_d[3] + 48;
  tmpstr[4] = imbe_d[4] + 48;
  tmpstr[5] = imbe_d[5] + 48;
  tmpstr[6] = imbe_d[85] + 48;
  tmpstr[7] = imbe_d[86] + 48;
  b0 = strtol (tmpstr, NULL, 2);
  if (b0 > 207) {
      if ((b0 >= 216) && (b0 <= 219)) {
#ifdef IMBE_DEBUG
          printf ("Silence\n");
#endif
      } else {
#ifdef IMBE_DEBUG
          printf ("Invalid fundamental frequency\n");
#endif
      }
      return (1);
  }
  cur_mp->w0 = (4.0f / ((float) b0 + 39.5f));

  // decode L from w0
  L = (int) (0.9254f * (int) ((1.0f / cur_mp->w0) + 0.25f));
  //L = (int) (0.23135f * ((float) b0 + 40.5f));
  //L = (int) ((0.23135f * (float) (b0 + 40)) + 0.115675f);
  if ((L > 56) || (L < 9)) {
#ifdef IMBE_DEBUG
      printf ("invalid L: %i\n", L);
#endif
      return (1);
  }
  cur_mp->L = L;
  L9 = L - 9;

  // decode K from L
  if (L < 37) {
      K = (int) ((float) (L + 2) / (float) 3);
      cur_mp->K = K;
  } else {
      K = 12;
      cur_mp->K = 12;
  }

#ifdef IMBE_DEBUG
  printf ("b0:%i L:%i K:%i\n", b0, L, K);
#endif

  // read bits from imbe_d into b0..bL+1
  bo1 = bo[L9][0];
  bo2 = bo1 + 1;
  for (i = 6; i < 85; i++) {
      bb[*bo1][*bo2] = imbe_d[i];
#ifdef IMBE_DEBUG
      printf ("bo1: %i,bo2: %i, ", *bo1, *bo2);
#endif
      bo1 += 2;
      bo2 += 2;
  }

  // Vl
  j = 1;
  k = (K - 1);
  for (i = 1; i <= L; i++) {
      cur_mp->Vl[i] = bb[1][k];
      if (j == 3) {
          j = 1;
          if (k > 0) {
              k--;
          } else {
              k = 0;
          }
      } else {
          j++;
      }
  }

  //decode G1 from b2
  tmpstr[6] = 0;
  tmpstr[0] = bb[2][5] + 48;
  tmpstr[1] = bb[2][4] + 48;
  tmpstr[2] = bb[2][3] + 48;
  tmpstr[3] = bb[2][2] + 48;
  tmpstr[4] = bb[2][1] + 48;
  tmpstr[5] = bb[2][0] + 48;
  b2 = strtol (tmpstr, NULL, 2);
  Gm[1] = B2[b2];
#ifdef IMBE_DEBUG
  printf ("G1: %e, %s, %i\n", Gm[1], tmpstr, b2);
  printf ("tmpstr: %s b2: %i g1: %e\n", tmpstr, b2, Gm[1]);
#endif

  // decode G2..G6 (from b3..b7) with annex E
  // equation 68
  ba1 = ba[L9][0];
  ba2 = ba1 + 1;

  for (i = 2; i < 7; i++) {
      tmpstr[(int) *ba1] = 0;
      k = 0;
      for (j = ((int) *ba1 - 1); j >= 0; j--) {
          tmpstr[k] = bb[i + 1][j] + 48;
          k++;
      }
      bm = strtol (tmpstr, NULL, 2);
      Gm[i] = (*ba2 * ((float) bm - mbe_expf((float)M_LN2 * (*ba1 - 1)) + (float) 0.5f));
#ifdef IMBE_DEBUG
      printf ("G%i: %e, %s, %i, ba1: %e, ba2: %e\n", i, Gm[i], tmpstr, bm, *ba1, *ba2);
#endif
      ba1 += 2;
      ba2 += 2;
  }

  // inverse DCT Gi to give Ri (also known as Ci,1)
  for (i = 1; i <= 6; i++) {
      sum = 0;
      for (m = 1; m <= 6; m++) {
          if (m == 1) {
              am = 1;
          } else {
              am = 2;
          }
          sum = sum + ((float) am * Gm[m] * mbe_cosf (((float)M_PI * (float) (m - 1) * ((float) i - 0.5f)) / (float) 6));
#ifdef IMBE_DEBUG
          printf ("sum: %e ", sum);
#endif
      }
      Ri[i] = sum;
#ifdef IMBE_DEBUG
      printf ("R%i: %e\n", i, Ri[i]);
#endif
  }
#ifdef IMBE_DEBUG
  printf ("R1: %e\n", Ri[1]);
#endif

  // load b8..bL+1 into Ci,k
  m = 8;
  for (i = 1; i <= 6; i++) {
      Cik[i][1] = Ri[i];
      for (k = 2; k <= ImbeJi[L9][i - 1]; k++) {
          Bm = hoba[L9][m - 8];
          for (b = 0; b < Bm; b++) {
              tmpstr[b] = bb[m][(Bm - b) - 1] + 48;
          }
          if (Bm == 0) {
              Cik[i][k] = 0;
          } else {
              tmpstr[Bm] = 0;
              bm = strtol (tmpstr, NULL, 2);
              Cik[i][k] = ((quantstep[Bm - 1] * standdev[k - 2]) * (((float) bm - mbe_expf((float)M_LN2 * (Bm - 1))) + 0.5f));
          }
          m++;
      }
  }

  // inverse DCT each Ci,k to give ci,j (Tl)
  l = 1;
  for (i = 1; i <= 6; i++) {
      ji = ImbeJi[L9][i - 1];
      for (j = 1; j <= ji; j++) {
          sum = 0;
          for (k = 1; k <= ji; k++) {
              if (k == 1) {
                  ak = 1;
              } else {
                  ak = 2;
              }
              sum = sum + ((float) ak * Cik[i][k] * mbe_cosf (((float)M_PI * (float) (k - 1) * ((float) j - 0.5f)) / (float) ji));
          }
          Tl[l] = sum;
          l++;
      }
  }
#ifdef IMBE_DEBUG
  printf ("T1: %e\n", Tl[1]);
#endif

  // determine log2Ml by applying ci,j to previous log2Ml 
  if (cur_mp->L <= 15) {
      rho = 0.4f;
  } else if (cur_mp->L <= 24) {
      rho = (0.03f * (float) cur_mp->L) - 0.05f;
  } else {
      rho = 0.7f;
  }

  // fix for when L > L(-1)
  if (cur_mp->L > prev_mp->L) {
      for (l = prev_mp->L + 1; l <= cur_mp->L; l++) {
          prev_mp->Ml[l] = prev_mp->Ml[prev_mp->L];
          prev_mp->log2Ml[l] = prev_mp->log2Ml[prev_mp->L];
      }
  }

  // Part 1
  Sum77 = 0;
  for (l = 1; l <= cur_mp->L; l++) {
      // eq. 75
      flokl[l] = ((float) prev_mp->L / (float) cur_mp->L) * (float) l;
      intkl[l] = (int) (flokl[l]);
#ifdef IMBE_DEBUG
      printf ("flokl: %e, intkl: %i ", flokl[l], intkl[l]);
#endif
      // eq. 76
      deltal[l] = flokl[l] - (float) intkl[l];
#ifdef IMBE_DEBUG
      printf ("deltal: %e ", deltal[l]);
#endif
      // eq 77
      Sum77 = Sum77 + ((((float) 1 - deltal[l]) * prev_mp->log2Ml[intkl[l]]) + (deltal[l] * prev_mp->log2Ml[intkl[l] + 1]));
  }
  Sum77 = ((rho / (float) cur_mp->L) * Sum77);

#ifdef IMBE_DEBUG
  printf ("Sum77: %e\n", Sum77);
#endif

  // Part 2
  for (l = 1; l <= cur_mp->L; l++) {
      c1 = (rho * ((float) 1 - deltal[l]) * prev_mp->log2Ml[intkl[l]]);
      c2 = (rho * deltal[l] * prev_mp->log2Ml[intkl[l] + 1]);
      cur_mp->log2Ml[l] = Tl[l] + c1 + c2 - Sum77;
      cur_mp->Ml[l] = mbe_expf((float)M_LN2 * cur_mp->log2Ml[l]);
#ifdef IMBE_DEBUG
      printf ("rho: %e c1: %e c2: %e Sum77: %e T%i: %e log2M%i: %e M%i: %e\n", rho, c1, c2, Sum77, l, Tl[l], l, cur_mp->log2Ml[l], l, cur_mp->Ml[l]);
#endif
  }
  return (0);
}

void mbe_processImbe4400Dataf (float *aout_buf, int *errs2, char *err_str, char imbe_d[88],
                               mbe_parms * cur_mp, mbe_parms * prev_mp, mbe_parms * prev_mp_enhanced, int uvquality)
{
  int i, bad;

  for (i = 0; i < *errs2; i++) {
      *err_str++ = '=';
  }

  bad = mbe_decodeImbe4400Parms (imbe_d, cur_mp, prev_mp);
  if ((bad == 1) || (*errs2 > 5))
    {
      mbe_useLastMbeParms (cur_mp, prev_mp);
      cur_mp->repeat++;
      *err_str = 'R';
      err_str++;
    }
  else
    {
      cur_mp->repeat = 0;
    }
  if (cur_mp->repeat <= 3)
    {
      mbe_moveMbeParms (cur_mp, prev_mp);
      mbe_spectralAmpEnhance (cur_mp);
      mbe_synthesizeSpeechf (aout_buf, cur_mp, prev_mp_enhanced, uvquality);
      mbe_moveMbeParms (cur_mp, prev_mp_enhanced);
    }
  else
    {
      *err_str = 'M';
      err_str++;
      mbe_synthesizeSilencef (aout_buf);
      mbe_initMbeParms (cur_mp, prev_mp, prev_mp_enhanced);
    }
  *err_str = 0;
}

