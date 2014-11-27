#include <stdint.h>
#include <math.h>

typedef union _f32 {
    float f;
    uint32_t i;
} _f32;

/* Constants for expf(x) -- single-precision exponential function evaluated at x (i.e. e^x). */
static const float
halF[2] = {0.5,-0.5,},
ln2HI[2]   ={ 6.9313812256e-01,     /* 0x3f317180 */
         -6.9313812256e-01,},   /* 0xbf317180 */
ln2LO[2]   ={ 9.0580006145e-06,     /* 0x3717f7d1 */
         -9.0580006145e-06,},   /* 0xb717f7d1 */
ln2    = 0.69314718246,
invln2 =  1.4426950216,         /* 0x3fb8aa3b */
T_P1   =  9.999999951334e-01,
T_P2   =  5.0000022321103498e-01,
T_P3   =  1.66662832359450473e-01,
T_P4   =  4.16979462133574885e-02,
T_P5   =  8.2031098e-03;

static const float
C1  = -5.0000000000e-01,
C2  =  4.1666667908e-02, /* 0x3d2aaaab */
C3  = -1.3888889225e-03, /* 0xbab60b61 */
C4  =  2.4801587642e-05, /* 0x37d00d01 */
C5  = -2.7557314297e-07, /* 0xb493f27c */
C6  =  2.0875723372e-09, /* 0x310f74f6 */
C7  = -1.1359647598e-11; /* 0xad47d74e */

// Differs from libc cosf on [0, pi/2] by at most 0.0000001229f
// Differs from libc cosf on [0, pi] by at most 0.0000035763f
static inline float k_cosf(float x)
{
    float z;
    z = x*x;
    return (1.0f+z*(C1+z*(C2+z*(C3+z*(C4+z*(C5+z*(C6+z*C7)))))));
}

static const float
S1  = -1.66666666666666324348e-01, /* 0xBFC55555, 0x55555549 */
S2  =  8.33333333332248946124e-03, /* 0x3F811111, 0x1110F8A6 */
S3  = -1.98412698298579493134e-04, /* 0xBF2A01A0, 0x19C161D5 */
S4  =  2.75573137070700676789e-06, /* 0x3EC71DE3, 0x57B1FE7D */
S5  = -2.50507602534068634195e-08, /* 0xBE5AE5E6, 0x8A2B9CEB */
S6  =  1.58969099521155010221e-10; /* 0x3DE5D93A, 0x5ACFD57C */

// Differs from libc sinf on [0, pi/2] by at most 0.0000001192f
// Differs from libc sinf on [0, pi] by at most 0.0000170176f
static inline float k_sinf(float x)
{
    float z;
    z = x*x;
    return x*(1.0f+z*(S1+z*(S2+z*(S3+z*(S4+z*(S5+z*S6))))));
}

float mbe_expf(float x) /* default IEEE double exp */
{
    _f32 tmp;
    float y,hi,lo,c,t;
    int32_t k,xsb;
    uint32_t hx;

    tmp.f = x;
    hx = tmp.i;
    xsb = (hx>>31)&1;       /* sign bit of x */
    hx &= 0x7fffffff;       /* high word of |x| */

    /* filter out non-finite argument */
    if(hx >= 0x42b17218) {              /* if |x|>=88.721... */
        if(hx>0x7f800000)
            return x+x;                 /* NaN */
        if(hx==0x7f800000)
            return (xsb==0)? x:0.0f;    /* exp(+-inf)={inf,0} */
    }

    /* argument reduction */
    if(hx > 0x3eb17218) {       /* if  |x| > 0.5 ln2 */ 
        if(hx < 0x3F851592) {   /* and |x| < 1.5 ln2 */
            hi = x-ln2HI[xsb]; lo=ln2LO[xsb]; k = 1-xsb-xsb;
        } else {
            k  = invln2*x+halF[xsb];
            t  = k;
            hi = x - t*ln2HI[0];    /* t*ln2HI is exact here */
            lo = t*ln2LO[0];
        }
        x  = hi - lo;
    } 
    else k = 0;

    /* x is now in primary range */
    c = x*(T_P1+x*(T_P2+x*(T_P3+x*(T_P4+x*T_P5))));
    y = 1.0f+c;
    {
        _f32 hy;
        hy.f = y;
        hy.i += (k << 23); /* add k to y\s exponent */
        return hy.f;
    }
}

float mbe_sqrtf(float x)
{
    _f32 tmp;
    int32_t ix;
    float y;
 
    tmp.f = x;
    ix = tmp.i;

    /* take care of Inf and NaN */
    if((ix&0x7f800000)==0x7f800000) {
        return x*x+x;       /* sqrt(NaN)=NaN, sqrt(+inf)=+inf, sqrt(-inf)=sNaN */
    }

    /* take care of 0 */
    if (!(ix&0x7fffffff)) return x; /* sqrt(+-0) = +-0 */

    /* take care of -ve values */
    if(ix & 0x80000000) return 0.0f/0.0f;  /* sqrt(-ve) = sNaN */

    ix  = 0x5f3759df - (ix >> 1);
    tmp.i = ix;
    y = tmp.f;
    y  = y * (1.5f - (0.5f * x * y * y));   // 1st iteration
    y  = y * (1.5f - (0.5f * x * y * y));   // 2nd iteration
    y  = y * (1.5f - (0.5f * x * y * y));   // 3rd iteration
    return (x*y);
}

/*
 * invpio2:  24 bits of 2/pi
 * pio2_1:   first  17 bit of pi/2
 * pio2_1t:  pi/2 - pio2_1
 */

static const float 
invpio2 =  6.3661980629e-01, /* 0x3f22f984 */
pio2_1  =  1.5707855225e+00, /* 0x3fc90f80 */
pio2_1t =  1.0804334124e-05; /* 0x37354443 */

uint32_t __ieee754_rem_pio2f(float x, float *y)
{
    float fn, t;
    uint32_t n;

    t = fabsf(x);
    n  = (uint32_t)(t*invpio2);
    fn = (float)n;
    y[0] = t-fn*pio2_1-fn*pio2_1t; /* 1st round good to 40 bit */
    return n;
}

float mbe_cosf(float x) {
    float y[2];
    uint32_t n = __ieee754_rem_pio2f(x,y);
    switch(n&3) {
        case 0: return  k_cosf(y[0]);
        case 1: return -k_sinf(y[0]);
        case 2: return -k_cosf(y[0]);
        default:
                return  k_sinf(y[0]);
    }
}

