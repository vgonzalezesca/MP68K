/*
 * imdct_ref — IMDCT de bloques largos de libmad como referencia en C del kernel asm (src/synth_kernels.h).
 * GENERADO por tools/gen_imdct_ref.py desde third_party/libmad/layer3.c: no editar a mano.
 * libmad: Copyright (C) 2000-2004 Underbit Technologies, Inc. (GPL v2 o posterior).
 * Las cuatro funciones de abajo (fastsdct, sdctII, dctIV, imdct36) son el texto de libmad. Aritmética de 32 bits con
 * envoltura; mad_f_mul es la de FPM_64BIT: (int32)(((int64)x*y) >> 28). Compilar con -fwrapv.
 */
#include "synth_kernels.h"

/* fastsdct escribe en X + 1 (y[0..16] pares): correcto, pero GCC lo ve como acceso fuera del arreglo */
#pragma GCC diagnostic ignored "-Wstringop-overflow"

typedef int32_t mad_fixed_t;
#define MAD_F(x) ((mad_fixed_t)(x))
#define mad_f_mul(x, y) ((mad_fixed_t)(((int64_t)(x) * (y)) >> 28))

/* window_l[i] = sin((PI / 36) * (i + 1/2)) */
static const mad_fixed_t window_l[36] = {
    11708990, 35037858, 58100066, 80720098, 102725802, 123949700,
    144230265, 163413152, 181352365, 197911378, 212964166, 226396167,
    238105157, 248002024, 256011445, 262072464, 266138953, 268179965,
    268179965, 266138953, 262072464, 256011445, 248002024, 238105157,
    226396167, 212964166, 197911378, 181352365, 163413152, 144230265,
    123949700, 102725802, 80720098, 58100066, 35037858, 11708990
};

static
void fastsdct(mad_fixed_t const x[9], mad_fixed_t y[18])
{
  mad_fixed_t a0,  a1,  a2,  a3,  a4,  a5,  a6,  a7,  a8,  a9,  a10, a11, a12;
  mad_fixed_t a13, a14, a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25;
  mad_fixed_t m0,  m1,  m2,  m3,  m4,  m5,  m6,  m7;

  enum {
    c0 =  MAD_F(0x1f838b8d),  /* 2 * cos( 1 * PI / 18) */
    c1 =  MAD_F(0x1bb67ae8),  /* 2 * cos( 3 * PI / 18) */
    c2 =  MAD_F(0x18836fa3),  /* 2 * cos( 4 * PI / 18) */
    c3 =  MAD_F(0x1491b752),  /* 2 * cos( 5 * PI / 18) */
    c4 =  MAD_F(0x0af1d43a),  /* 2 * cos( 7 * PI / 18) */
    c5 =  MAD_F(0x058e86a0),  /* 2 * cos( 8 * PI / 18) */
    c6 = -MAD_F(0x1e11f642)   /* 2 * cos(16 * PI / 18) */
  };

  a0 = x[3] + x[5];
  a1 = x[3] - x[5];
  a2 = x[6] + x[2];
  a3 = x[6] - x[2];
  a4 = x[1] + x[7];
  a5 = x[1] - x[7];
  a6 = x[8] + x[0];
  a7 = x[8] - x[0];

  a8  = a0  + a2;
  a9  = a0  - a2;
  a10 = a0  - a6;
  a11 = a2  - a6;
  a12 = a8  + a6;
  a13 = a1  - a3;
  a14 = a13 + a7;
  a15 = a3  + a7;
  a16 = a1  - a7;
  a17 = a1  + a3;

  m0 = mad_f_mul(a17, -c3);
  m1 = mad_f_mul(a16, -c0);
  m2 = mad_f_mul(a15, -c4);
  m3 = mad_f_mul(a14, -c1);
  m4 = mad_f_mul(a5,  -c1);
  m5 = mad_f_mul(a11, -c6);
  m6 = mad_f_mul(a10, -c5);
  m7 = mad_f_mul(a9,  -c2);

  a18 =     x[4] + a4;
  a19 = 2 * x[4] - a4;
  a20 = a19 + m5;
  a21 = a19 - m5;
  a22 = a19 + m6;
  a23 = m4  + m2;
  a24 = m4  - m2;
  a25 = m4  + m1;

  /* output to every other slot for convenience */

  y[ 0] = a18 + a12;
  y[ 2] = m0  - a25;
  y[ 4] = m7  - a20;
  y[ 6] = m3;
  y[ 8] = a21 - m6;
  y[10] = a24 - m1;
  y[12] = a12 - 2 * a18;
  y[14] = a23 + m0;
  y[16] = a22 + m7;
}

static inline
void sdctII(mad_fixed_t const x[18], mad_fixed_t X[18])
{
  mad_fixed_t tmp[9];
  int i;

  /* scale[i] = 2 * cos(PI * (2 * i + 1) / (2 * 18)) */
  static mad_fixed_t const scale[9] = {
    MAD_F(0x1fe0d3b4), MAD_F(0x1ee8dd47), MAD_F(0x1d007930),
    MAD_F(0x1a367e59), MAD_F(0x16a09e66), MAD_F(0x125abcf8),
    MAD_F(0x0d8616bc), MAD_F(0x08483ee1), MAD_F(0x02c9fad7)
  };

  /* divide the 18-point SDCT-II into two 9-point SDCT-IIs */

  /* even input butterfly */

  for (i = 0; i < 9; i += 3) {
    tmp[i + 0] = x[i + 0] + x[18 - (i + 0) - 1];
    tmp[i + 1] = x[i + 1] + x[18 - (i + 1) - 1];
    tmp[i + 2] = x[i + 2] + x[18 - (i + 2) - 1];
  }

  fastsdct(tmp, &X[0]);

  /* odd input butterfly and scaling */

  for (i = 0; i < 9; i += 3) {
    tmp[i + 0] = mad_f_mul(x[i + 0] - x[18 - (i + 0) - 1], scale[i + 0]);
    tmp[i + 1] = mad_f_mul(x[i + 1] - x[18 - (i + 1) - 1], scale[i + 1]);
    tmp[i + 2] = mad_f_mul(x[i + 2] - x[18 - (i + 2) - 1], scale[i + 2]);
  }

  fastsdct(tmp, &X[1]);

  /* output accumulation */

  for (i = 3; i < 18; i += 8) {
    X[i + 0] -= X[(i + 0) - 2];
    X[i + 2] -= X[(i + 2) - 2];
    X[i + 4] -= X[(i + 4) - 2];
    X[i + 6] -= X[(i + 6) - 2];
  }
}

static inline
void dctIV(mad_fixed_t const y[18], mad_fixed_t X[18])
{
  mad_fixed_t tmp[18];
  int i;

  /* scale[i] = 2 * cos(PI * (2 * i + 1) / (4 * 18)) */
  static mad_fixed_t const scale[18] = {
    MAD_F(0x1ff833fa), MAD_F(0x1fb9ea93), MAD_F(0x1f3dd120),
    MAD_F(0x1e84d969), MAD_F(0x1d906bcf), MAD_F(0x1c62648b),
    MAD_F(0x1afd100f), MAD_F(0x1963268b), MAD_F(0x1797c6a4),
    MAD_F(0x159e6f5b), MAD_F(0x137af940), MAD_F(0x11318ef3),
    MAD_F(0x0ec6a507), MAD_F(0x0c3ef153), MAD_F(0x099f61c5),
    MAD_F(0x06ed12c5), MAD_F(0x042d4544), MAD_F(0x0165547c)
  };

  /* scaling */

  for (i = 0; i < 18; i += 3) {
    tmp[i + 0] = mad_f_mul(y[i + 0], scale[i + 0]);
    tmp[i + 1] = mad_f_mul(y[i + 1], scale[i + 1]);
    tmp[i + 2] = mad_f_mul(y[i + 2], scale[i + 2]);
  }

  /* SDCT-II */

  sdctII(tmp, X);

  /* scale reduction and output accumulation */

  X[0] /= 2;
  for (i = 1; i < 17; i += 4) {
    X[i + 0] = X[i + 0] / 2 - X[(i + 0) - 1];
    X[i + 1] = X[i + 1] / 2 - X[(i + 1) - 1];
    X[i + 2] = X[i + 2] / 2 - X[(i + 2) - 1];
    X[i + 3] = X[i + 3] / 2 - X[(i + 3) - 1];
  }
  X[17] = X[17] / 2 - X[16];
}

/*
 * NAME:	imdct36
 * DESCRIPTION:	perform X[18]->x[36] IMDCT using Szu-Wei Lee's fast algorithm
 */
static inline
void imdct36(mad_fixed_t const x[18], mad_fixed_t y[36])
{
  mad_fixed_t tmp[18];
  int i;

  /* DCT-IV */

  dctIV(x, tmp);

  /* convert 18-point DCT-IV to 36-point IMDCT */

  for (i =  0; i <  9; i += 3) {
    y[i + 0] =  tmp[9 + (i + 0)];
    y[i + 1] =  tmp[9 + (i + 1)];
    y[i + 2] =  tmp[9 + (i + 2)];
  }
  for (i =  9; i < 27; i += 3) {
    y[i + 0] = -tmp[36 - (9 + (i + 0)) - 1];
    y[i + 1] = -tmp[36 - (9 + (i + 1)) - 1];
    y[i + 2] = -tmp[36 - (9 + (i + 2)) - 1];
  }
  for (i = 27; i < 36; i += 3) {
    y[i + 0] = -tmp[(i + 0) - 27];
    y[i + 1] = -tmp[(i + 1) - 27];
    y[i + 2] = -tmp[(i + 2) - 27];
  }
}

void mp368k_imdct_l_ref(const int32_t X[18], int32_t overlap[18], int32_t *sample, int invert)
{
    mad_fixed_t z[36];
    unsigned int i;

    imdct36(X, z);
    for (i = 0; i < 36; i += 4) {
        z[i + 0] = mad_f_mul(z[i + 0], window_l[i + 0]);
        z[i + 1] = mad_f_mul(z[i + 1], window_l[i + 1]);
        z[i + 2] = mad_f_mul(z[i + 2], window_l[i + 2]);
        z[i + 3] = mad_f_mul(z[i + 3], window_l[i + 3]);
    }
    for (i = 0; i < 18; i++) {
        sample[32 * i] = z[i] + overlap[i];
        overlap[i] = z[i + 18];
    }
    if (invert)
        for (i = 1; i < 18; i += 2)
            sample[32 * i] = -sample[32 * i];
}
