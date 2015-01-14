#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "mbelib.h"

void mbe_spectralAmpEnhance (mbe_parms * cur_mp)
{
  float tmp, sum, gamma, M, Rm0, Rm1, R2m0, R2m1, Wl[57];
  int l;

  Rm0 = 0;
  Rm1 = 0;
  for (l = 1; l <= cur_mp->L; l++) {
      tmp = cur_mp->Ml[l];
      tmp *= tmp;
      Rm0 = Rm0 + tmp;
      Rm1 = Rm1 + (tmp * mbe_cosf ((float)M_PI * cur_mp->w0 * (float) l)); // mbe_cosf(x) bounded by M_PI at least for AMBE
  }
  R2m0 = (Rm0 * Rm0);
  R2m1 = (Rm1 * Rm1);

  for (l = 1; l <= cur_mp->L; l++) {
      if (cur_mp->Ml[l] != 0) {
          // mbe_cosf(x) here bounded similarly
          tmp = mbe_sqrtf(mbe_sqrtf(((0.96f * M_PI * ((R2m0 + R2m1) - (2.0f * Rm0 * Rm1 * 
                                                                       mbe_cosf ((float)M_PI * cur_mp->w0 * (float) l)))) 
                                    / ((float)M_PI * cur_mp->w0 * Rm0 * (R2m0 - R2m1)))));
          Wl[l] = mbe_sqrtf (cur_mp->Ml[l]) * tmp; 

          if ((8 * l) <= cur_mp->L) {
          } else if (Wl[l] > 1.2f) {
              cur_mp->Ml[l] = 1.2f * cur_mp->Ml[l];
          } else if (Wl[l] < 0.5f) {
              cur_mp->Ml[l] = 0.5f * cur_mp->Ml[l];
          } else {
              cur_mp->Ml[l] = Wl[l] * cur_mp->Ml[l];
          }
      }
  }

  // generate scaling factor
  sum = 0;
  for (l = 1; l <= cur_mp->L; l++) {
      M = cur_mp->Ml[l];
      if (M < 0) {
          M = -M;
      }
      sum += (M * M);
  }
  if (sum == 0) {
      gamma = 1.0f;
  } else {
      gamma = mbe_sqrtf (Rm0 / sum);
  }

  // apply scaling factor
  for (l = 1; l <= cur_mp->L; l++) {
      cur_mp->Ml[l] = gamma * cur_mp->Ml[l];
  }
}

