/*
 * Copyright (c) 2012, Code Aurora Forum. All rights reserved.

 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.
    * Neither the name of Code Aurora Forum, Inc. nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdint.h>

extern "C" {
  float sqrtf(float);
};

float sqrt(float f) {
  return sqrtf(f);
}

typedef struct {
  float m[16];
} rs_matrix4x4;

typedef struct {
  float m[9];
} rs_matrix3x3;

typedef struct {
  float m[4];
} rs_matrix2x2;

extern float sin(float);
extern float cos(float);
extern float fabs(float);
extern float tan(float);
extern void* memcpy(void*, const void*, unsigned int);
#define M_PI        3.14159265358979323846264338327950288f   /* pi */

typedef float F4 __attribute__((ext_vector_type(4)));
typedef float F3 __attribute__((ext_vector_type(3)));
typedef float F2 __attribute__((ext_vector_type(2)));

static inline float m4_get(rs_matrix4x4 const *m, int r, int l) {return m->m[r*4+l];}
static inline float m3_get(rs_matrix3x3 const *m, int r, int l) {return m->m[r*3+l];}
static inline float m2_get(rs_matrix2x2 const *m, int r, int l) {return m->m[r*2+l];}
static inline void m3_set(rs_matrix3x3 *m, int r, int l, float v) {m->m[r*3+l]=v;}
static inline void m2_set(rs_matrix2x2 *m, int r, int l, float v) {m->m[r*2+l]=v;}

#define VEC(x) x
#define vmovq_n_f32(x) initF4(x,x,x,x)

inline static  F4 initF4(float x, float y, float z, float w) {
  F4 t1 = {x, y, z, w};
  return t1;
}

inline static F4 init0() {
  return initF4(0.f, 0.f, 0.f, 0.f);
}


void rsMatrixLoadIdentity(rs_matrix4x4 *m);
void rsMatrixLoadIdentity(rs_matrix3x3 *m);
void rsMatrixLoadIdentity(rs_matrix2x2 *m);
void rsMatrixLoad(rs_matrix4x4 *m, const float *v);
void rsMatrixLoad(rs_matrix3x3 *m, const float *f);
void rsMatrixLoad(rs_matrix2x2 *m, const float *f);
void rsMatrixLoad(rs_matrix4x4 *m, const rs_matrix4x4 *v);
void rsMatrixLoad(rs_matrix4x4 *m, const rs_matrix3x3 *v);
void rsMatrixLoad(rs_matrix4x4 *m, const rs_matrix2x2 *s);
void rsMatrixLoad(rs_matrix3x3 *m, const rs_matrix3x3 *v);
void rsMatrixLoad(rs_matrix2x2 *m, const rs_matrix2x2 *s);
void rsMatrixLoadRotate(rs_matrix4x4 *m, float rot, float x, float y, float z);
void rsMatrixLoadScale(rs_matrix4x4 *m, float x, float y, float z);
void rsMatrixLoadTranslate(rs_matrix4x4 *m, float x, float y, float z);
void rsMatrixRotate(rs_matrix4x4 *m, float rot, float x, float y, float z);
void rsMatrixScale(rs_matrix4x4 *m, float x, float y, float z);
void rsMatrixTranslate(rs_matrix4x4 *m, float x, float y, float z);
void rsMatrixLoadMultiply(rs_matrix4x4 *m, const rs_matrix4x4 *lhs, const rs_matrix4x4 *rhs);
void rsMatrixLoadMultiply(rs_matrix3x3 *m, const rs_matrix3x3 *lhs, const rs_matrix3x3 *rhs);
void rsMatrixLoadMultiply(rs_matrix2x2 *m, const rs_matrix2x2 *lhs, const rs_matrix2x2 *rhs);
void rsMatrixMultiply(rs_matrix4x4 *m, const rs_matrix4x4 *rhs);
void rsMatrixMultiply(rs_matrix3x3 *m, const rs_matrix3x3 *rhs);
void rsMatrixMultiply(rs_matrix2x2 *m, const rs_matrix2x2 *rhs);
void rsMatrixLoadOrtho(rs_matrix4x4 *m, float l, float r, float b, float t, float n, float f);
void rsMatrixLoadFrustum(rs_matrix4x4 *m, float l, float r, float b, float t, float n, float f);
void rsMatrixLoadPerspective(rs_matrix4x4 *m, float fovy, float aspect, float near, float far);
bool rsMatrixInverse(rs_matrix4x4 *m);
bool rsMatrixInverseTranspose(rs_matrix4x4 *m);
void rsMatrixTranspose(rs_matrix4x4 *m);
void rsMatrixTranspose(rs_matrix3x3 *m);
void rsMatrixTranspose(rs_matrix2x2 *m);

void rsMatrixLoadIdentity(rs_matrix4x4 *m) {
    F2* M = (F2 *) m->m;
    union {
       F2 v;
       struct {float x; float y;};
    } t00, t01, t10;
    t00.x = t00.y = t01.x = t10.y = 0.f;
    t01.y = t10.x = 1.0;
    M[0] = t10.v;
    M[1] = t00.v;
    M[2] = t01.v;
    M[3] = t00.v;
    M[4] = t00.v;
    M[5] = t10.v;
    M[6] = t00.v;
    M[7] = t01.v;
}

void rsMatrixLoadIdentity(rs_matrix3x3 *m) {
    F4 t0;
    VEC(t0) = initF4(1.f, 0.f, 0.f, 0.f);

    F4* M = (F4*) m->m;
    VEC(M[0]) = VEC(t0);
    VEC(M[1]) = VEC(t0);
    m->m[8] = 1.f;
}

//Scalar
void rsMatrixLoadIdentity(rs_matrix2x2 *m) {
    m->m[0] = 1.f;
    m->m[1] = 0.f;
    m->m[2] = 0.f;
    m->m[3] = 1.f;
    //m->loadIdentity();
}

void rsMatrixLoad(rs_matrix4x4 *m, const float *v) {
    F4* M = (F4*) m->m;
    F4* V = (F4*) v;

    VEC(M[0]) = VEC(V[0]);
    VEC(M[1]) = VEC(V[1]);
    VEC(M[2]) = VEC(V[2]);
    VEC(M[3]) = VEC(V[3]);
}

void rsMatrixLoad(rs_matrix3x3 *m, const float *f) {
    F4* M = (F4*) m->m;
    F4* F = (F4*) f;

    VEC(M[0]) = VEC(F[0]);
    VEC(M[1]) = VEC(F[1]);
    m->m[8] = f[8];
}

//Scalar
void rsMatrixLoad(rs_matrix2x2 *m, const float *f) {
    memcpy(m->m, f, sizeof(m->m));
    //m->load(f);
}

void rsMatrixLoad(rs_matrix4x4 *m, const rs_matrix4x4 *v) {
    F4* M = (F4*) m->m;
    F4* V = (F4*) v->m;

    VEC(M[0]) = VEC(V[0]);
    VEC(M[1]) = VEC(V[1]);
    VEC(M[2]) = VEC(V[2]);
    VEC(M[3]) = VEC(V[3]);
}

void rsMatrixLoad(rs_matrix4x4 *m, const rs_matrix3x3 *v) {
    F2 *M = (F2 *) m->m;
    F2 *V = (F2 *) v->m;
    F2 *V1 = (F2 *) &v->m[1];
    union {
       F2 v;
       struct {float x; float y;};
    } t00, t01;
    t01.x = t00.x = t00.y = 0.f;
    t01.y = 1.0f;
    M[0] = V[0];
    m->m[2] = v->m[2];
    m->m[3] = 0.f;
    M[2] = V1[1];
    m->m[6] = v->m[5];
    m->m[7] = 0.f;
    M[4] = V[3];
    m->m[10] = v->m[8];
    m->m[11] = 0.f;

    m->m[12] = 0.f;
    m->m[13] = 0.f;
    m->m[14] = 0.f;
    m->m[15] = 1.f;
}

//Scalar
void rsMatrixLoad(rs_matrix4x4 *m, const rs_matrix2x2 *s) {
    m->m[0] = s->m[0];
    m->m[1] = s->m[1];
    m->m[2] = 0.f;
    m->m[3] = 0.f;
    m->m[4] = s->m[2];
    m->m[5] = s->m[3];
    m->m[6] = 0.f;
    m->m[7] = 0.f;
    m->m[8] = 0.f;
    m->m[9] = 0.f;
    m->m[10] = 1.f;
    m->m[11] = 0.f;
    m->m[12] = 0.f;
    m->m[13] = 0.f;
    m->m[14] = 0.f;
    m->m[15] = 1.f;

    //m->load(s);
}

void rsMatrixLoad(rs_matrix3x3 *m, const rs_matrix3x3 *v) {

    F4* M = (F4*) m->m;
    F4* V = (F4*) v->m;

    VEC(M[0]) = VEC(V[0]);
    VEC(M[1]) = VEC(V[1]);
    m->m[8] = v->m[8];
}

//Scalar
void rsMatrixLoad(rs_matrix2x2 *m, const rs_matrix2x2 *s) {
    memcpy(m->m, s->m, sizeof(m->m));
    //m->load(s);
}

//Scalar
void rsMatrixLoadRotate(rs_matrix4x4 *m, float rot, float x, float y, float z) {
    //m->loadRotate(rot, x, y, z);
    float c, s;
    m->m[3] = 0;
    m->m[7] = 0;
    m->m[11]= 0;
    m->m[12]= 0;
    m->m[13]= 0;
    m->m[14]= 0;
    m->m[15]= 1;
    rot *= float(M_PI / 180.0f);
    c = cos(rot);
    s = sin(rot);

    const float len = x*x + y*y + z*z;
    if (len != 1) {
        const float recipLen = 1.f / sqrt(len);
        x *= recipLen;
        y *= recipLen;
        z *= recipLen;
    }
    const float nc = 1.0f - c;
    const float xy = x * y;
    const float yz = y * z;
    const float zx = z * x;
    const float xs = x * s;
    const float ys = y * s;
    const float zs = z * s;
    m->m[ 0] = x*x*nc +  c;
    m->m[ 4] =  xy*nc - zs;
    m->m[ 8] =  zx*nc + ys;
    m->m[ 1] =  xy*nc + zs;
    m->m[ 5] = y*y*nc +  c;
    m->m[ 9] =  yz*nc - xs;
    m->m[ 2] =  zx*nc - ys;
    m->m[ 6] =  yz*nc + xs;
    m->m[10] = z*z*nc +  c;

}

void rsMatrixLoadScale(rs_matrix4x4 *m, float x, float y, float z) {
    F2 *M = (F2 *) m->m;
    union {
      F2 v;
      struct { float x; float y;};
    } t00, t01;
    t00.x = t00.y= t01.x = 0.f;
    t01.y = 1.f;
    m->m[0] = x;
    m->m[1] = 0.f;
    M[1] = t00.v;
    m->m[4] = 0.f;
    m->m[5] = y;
    M[3] = t00.v;
    M[4] = t00.v;
    m->m[10] = z;
    m->m[11] = 0.f;
    M[6] = t00.v;
    M[7] = t01.v;
}

void rsMatrixLoadTranslate(rs_matrix4x4 *m, float x, float y, float z) {
    F4 t0, t1, t2;

    VEC(t1) = VEC(t2) = VEC(t0) = init0();
    t0.x = 1.f;
    t1.y = 1.f;
    t2.z = 1.f;

    F4* M = (F4 *) m->m;
    VEC(M[0]) = VEC(t0);
    VEC(M[1]) = VEC(t1);
    VEC(M[2]) = VEC(t2);
    m->m[12] = x;
    m->m[13] = y;
    m->m[14] = z;
    m->m[15] = 1.f;
}

void rsMatrixRotate(rs_matrix4x4 *m, float rot, float x, float y, float z) {
    rs_matrix4x4 m1;
    rsMatrixLoadRotate(&m1, rot, x, y, z);
    rsMatrixMultiply(m, &m1);
}

void rsMatrixScale(rs_matrix4x4 *m, float x, float y, float z) {
    rs_matrix4x4 m1;
    rsMatrixLoadScale(&m1, x, y, z);
    rsMatrixMultiply(m, &m1);
}

void rsMatrixTranslate(rs_matrix4x4 *m, float x, float y, float z) {
    rs_matrix4x4 tmp;
    rsMatrixLoadTranslate(&tmp, x, y, z);
    rsMatrixMultiply(m, &tmp);

    //m->translate(x, y, z);
}

void rsMatrixLoadMultiply(rs_matrix4x4 *m, const rs_matrix4x4 *lhs, const rs_matrix4x4 *rhs) {
     F4 r0, r1, r2, r3;
    F4* M = (F4 *) m->m;
    F4* LHS = (F4 *) lhs->m;
    F4* RHS = (F4 *) rhs->m;
    F4 rhs1, rhs2;

    VEC(rhs2) = VEC(RHS[0]);
    VEC(rhs1) = __builtin_shufflevector(VEC(rhs2), VEC(rhs2), 0,0,0,0);
    VEC(r0) = VEC(LHS[0]) * VEC(rhs1);
    VEC(rhs1) = __builtin_shufflevector(VEC(rhs2), VEC(rhs2), 1,1,1,1);
    VEC(r0) += VEC(LHS[1]) * VEC(rhs1);
    VEC(rhs1) = __builtin_shufflevector(VEC(rhs2), VEC(rhs2), 2,2,2,2);
    VEC(r0) += VEC(LHS[2]) * VEC(rhs1);
    VEC(rhs1) = __builtin_shufflevector(VEC(rhs2), VEC(rhs2), 3,3,3,3);
    VEC(r0) += VEC(LHS[3]) * VEC(rhs1);

    VEC(rhs2) = VEC(RHS[1]);
    VEC(rhs1) = __builtin_shufflevector(VEC(rhs2), VEC(rhs2), 0,0,0,0);
    VEC(r1) = VEC(LHS[0]) * VEC(rhs1);
    VEC(rhs1) = __builtin_shufflevector(VEC(rhs2), VEC(rhs2), 1,1,1,1);
    VEC(r1) += VEC(LHS[1]) * VEC(rhs1);
    VEC(rhs1) = __builtin_shufflevector(VEC(rhs2), VEC(rhs2), 2,2,2,2);
    VEC(r1) += VEC(LHS[2]) * VEC(rhs1);
    VEC(rhs1) = __builtin_shufflevector(VEC(rhs2), VEC(rhs2), 3,3,3,3);
    VEC(r1) += VEC(LHS[3]) * VEC(rhs1);

    VEC(rhs2) = VEC(RHS[2]);
    VEC(rhs1) = __builtin_shufflevector(VEC(rhs2), VEC(rhs2), 0,0,0,0);
    VEC(r2) = VEC(LHS[0]) * VEC(rhs1);
    VEC(rhs1) = __builtin_shufflevector(VEC(rhs2), VEC(rhs2), 1,1,1,1);
    VEC(r2) += VEC(LHS[1]) * VEC(rhs1);
    VEC(rhs1) = __builtin_shufflevector(VEC(rhs2), VEC(rhs2), 2,2,2,2);
    VEC(r2) += VEC(LHS[2]) * VEC(rhs1);
    VEC(rhs1) = __builtin_shufflevector(VEC(rhs2), VEC(rhs2), 3,3,3,3);
    VEC(r2) += VEC(LHS[3]) * VEC(rhs1);

    VEC(rhs2) = VEC(RHS[3]);
    VEC(rhs1) = __builtin_shufflevector(VEC(rhs2), VEC(rhs2), 0,0,0,0);
    VEC(r3) = VEC(LHS[0]) * VEC(rhs1);
    VEC(rhs1) = __builtin_shufflevector(VEC(rhs2), VEC(rhs2), 1,1,1,1);
    VEC(r3) += VEC(LHS[1]) * VEC(rhs1);
    VEC(rhs1) = __builtin_shufflevector(VEC(rhs2), VEC(rhs2), 2,2,2,2);
    VEC(r3) += VEC(LHS[2]) * VEC(rhs1);
    VEC(rhs1) = __builtin_shufflevector(VEC(rhs2), VEC(rhs2), 3,3,3,3);
    VEC(r3) += VEC(LHS[3]) * VEC(rhs1);
    VEC(M[0]) = VEC(r0);    VEC(M[1]) = VEC(r1);    VEC(M[2]) = VEC(r2);    VEC(M[3]) = VEC(r3);
}

void rsMatrixLoadMultiply(rs_matrix3x3 *m, const rs_matrix3x3 *lhs, const rs_matrix3x3 *rhs) {
    F3 r0;
    F3 mj;
    F2 *M2 = (F2 *) m->m;

    r0 = 0.f; // r0 = 0 will trigger assertion failure
    for(int j=0; j<3 ; j ++){
      const float rhs_ij = m3_get(rhs,0,j);
      mj.x = m3_get(lhs,j,0);
      mj.y = m3_get(lhs,j,1);
      mj.z = m3_get(lhs,j,2);
      r0 += mj * rhs_ij;
    }
    M2[0]= r0.xy; m->m[2] = r0.z;

    r0 = 0.f;
    for (int j=0 ; j<3 ; j++) {
      const float rhs_ij = m3_get(rhs,1,j);
      mj.x = m3_get(lhs,j,0);
      mj.y = m3_get(lhs,j,1);
      mj.z = m3_get(lhs,j,2);
      r0 += mj * rhs_ij;
    }
    m->m[3] = r0.x; m->m[4] = r0.y; m->m[5] = r0.z;
    r0 = 0.f;
    for (int j=0 ; j<3 ; j++) {
      const float rhs_ij = m3_get(rhs,2,j);
      mj.x = m3_get(lhs,j,0);
      mj.y = m3_get(lhs,j,1);
      mj.z = m3_get(lhs,j,2);
      r0 += mj * rhs_ij;
    }
    M2[3] = r0.xy; m->m[8] = r0.z;
}

//Scalar
void rsMatrixLoadMultiply(rs_matrix2x2 *m, const rs_matrix2x2 *lhs, const rs_matrix2x2 *rhs) {
    for (int i=0 ; i<2 ; i++) {
        float ri0 = 0;
        float ri1 = 0;
        for (int j=0 ; j<2 ; j++) {
            const float rhs_ij = m2_get(rhs, i, j);
            ri0 += m2_get(lhs, j, 0) * rhs_ij;
            ri1 += m2_get(lhs, j, 1) * rhs_ij;
        }
        m2_set(m, i, 0, ri0);
        m2_set(m, i, 1, ri1);
    }

    //m->loadMultiply(lhs, rhs);
}

void rsMatrixMultiply(rs_matrix4x4 *m, const rs_matrix4x4 *rhs) {
    F4* M = (F4*) m->m;
    F4* RHS = (F4*) rhs->m;
    F4 r0, r1, r2, r3;
    F4 mj;
    F4 rhs1,rhs2;

    VEC(rhs2) = VEC(RHS[0]);
    VEC(rhs1) = __builtin_shufflevector(VEC(rhs2), VEC(rhs2), 0,0,0,0);
    VEC(r0) = VEC(M[0]) * VEC(rhs1);
    VEC(rhs1) = __builtin_shufflevector(VEC(rhs2), VEC(rhs2), 1,1,1,1);
    VEC(r0) += VEC(M[1]) * VEC(rhs1);
    VEC(rhs1) = __builtin_shufflevector(VEC(rhs2), VEC(rhs2), 2,2,2,2);
    VEC(r0) += VEC(M[2]) * VEC(rhs1);
    VEC(rhs1) = __builtin_shufflevector(VEC(rhs2), VEC(rhs2), 3,3,3,3);
    VEC(r0) += VEC(M[3]) * VEC(rhs1);

    VEC(rhs2) = VEC(RHS[1]);
    VEC(rhs1) = __builtin_shufflevector(VEC(rhs2), VEC(rhs2), 0,0,0,0);
    VEC(r1) = VEC(M[0]) * VEC(rhs1);
    VEC(rhs1) = __builtin_shufflevector(VEC(rhs2), VEC(rhs2), 1,1,1,1);
    VEC(r1) += VEC(M[1]) * VEC(rhs1);
    VEC(rhs1) = __builtin_shufflevector(VEC(rhs2), VEC(rhs2), 2,2,2,2);
    VEC(r1) += VEC(M[2]) * VEC(rhs1);
    VEC(rhs1) = __builtin_shufflevector(VEC(rhs2), VEC(rhs2), 3,3,3,3);
    VEC(r1) += VEC(M[3]) * VEC(rhs1);

    VEC(rhs2) = VEC(RHS[2]);
    VEC(rhs1) = __builtin_shufflevector(VEC(rhs2), VEC(rhs2), 0,0,0,0);
    VEC(r2) = VEC(M[0]) * VEC(rhs1);
    VEC(rhs1) = __builtin_shufflevector(VEC(rhs2), VEC(rhs2), 1,1,1,1);
    VEC(r2) += VEC(M[1]) * VEC(rhs1);
    VEC(rhs1) = __builtin_shufflevector(VEC(rhs2), VEC(rhs2), 2,2,2,2);
    VEC(r2) += VEC(M[2]) * VEC(rhs1);
    VEC(rhs1) = __builtin_shufflevector(VEC(rhs2), VEC(rhs2), 3,3,3,3);
    VEC(r2) += VEC(M[3]) * VEC(rhs1);

    VEC(rhs2) = VEC(RHS[3]);
    VEC(rhs1) = __builtin_shufflevector(VEC(rhs2), VEC(rhs2), 0,0,0,0);
    VEC(r3) = VEC(M[0]) * VEC(rhs1);
    VEC(rhs1) = __builtin_shufflevector(VEC(rhs2), VEC(rhs2), 1,1,1,1);
    VEC(r3) += VEC(M[1]) * VEC(rhs1);
    VEC(rhs1) = __builtin_shufflevector(VEC(rhs2), VEC(rhs2), 2,2,2,2);
    VEC(r3) += VEC(M[2]) * VEC(rhs1);
    VEC(rhs1) = __builtin_shufflevector(VEC(rhs2), VEC(rhs2), 3,3,3,3);
    VEC(r3) += VEC(M[3]) * VEC(rhs1);
    VEC(M[0]) = VEC(r0);    VEC(M[1]) = VEC(r1);    VEC(M[2]) = VEC(r2);    VEC(M[3]) = VEC(r3);
}

void rsMatrixMultiply(rs_matrix3x3 *m, const rs_matrix3x3 *rhs) {
    float r00 = 0;
    float r01 = 0;
    float r02 = 0;
    for (int j=0 ; j<3 ; j++) {
            const float rhs_ij = m3_get(rhs,0,j);
            r00 += m3_get(m,j,0) * rhs_ij;
            r01 += m3_get(m,j,1) * rhs_ij;
            r02 += m3_get(m,j,2) * rhs_ij;
    }

    float r10 = 0;
    float r11 = 0;
    float r12 = 0;
    for (int j=0 ; j<3 ; j++) {
            const float rhs_ij = m3_get(rhs,1,j);
            r10 += m3_get(m,j,0) * rhs_ij;
            r11 += m3_get(m,j,1) * rhs_ij;
            r12 += m3_get(m,j,2) * rhs_ij;
    }

    float r20 = 0;
    float r21 = 0;
    float r22 = 0;
    for (int j=0 ; j<3 ; j++) {
            const float rhs_ij = m3_get(rhs,2,j);
            r20 += m3_get(m,j,0) * rhs_ij;
            r21 += m3_get(m,j,1) * rhs_ij;
            r22 += m3_get(m,j,2) * rhs_ij;
    }

    m->m[0] = r00; m->m[1] = r01; m->m[2] = r02;
    m->m[3] = r10; m->m[4] = r11; m->m[5] = r12;
    m->m[6] = r20; m->m[7] = r21; m->m[8] = r22;
}

void rsMatrixMultiply(rs_matrix2x2 *m, const rs_matrix2x2 *rhs) {
    float r00 = 0;
    float r01 = 0;
    for (int j=0 ; j<2 ; j++) {
            const float rhs_ij = m2_get(rhs,0,j);
            r00 += m2_get(m,j,0) * rhs_ij;
            r01 += m2_get(m,j,1) * rhs_ij;
    }

    float r10 = 0;
    float r11 = 0;
    for (int j=0 ; j<2 ; j++) {
            const float rhs_ij = m2_get(rhs,1,j);
            r10 += m2_get(m,j,0) * rhs_ij;
            r11 += m2_get(m,j,1) * rhs_ij;
    }

    m->m[0] = r00; m->m[1] = r01;
    m->m[2] = r10; m->m[3] = r11;
}

//Scalar
void rsMatrixLoadOrtho(rs_matrix4x4 *m, float left, float right, float bottom, float top, float near, float far) {
    rsMatrixLoadIdentity(m);
    m->m[0] = 2.f / (right - left);
    m->m[5] = 2.f / (top - bottom);
    m->m[10]= -2.f / (far - near);
    m->m[12]= -(right + left) / (right - left);
    m->m[13]= -(top + bottom) / (top - bottom);
    m->m[14]= -(far + near) / (far - near);

    //m->loadOrtho(l, r, b, t, n, f);
}

//Scalar
void rsMatrixLoadFrustum(rs_matrix4x4 *m, float left, float right, float bottom, float top, float near, float far) {
    rsMatrixLoadIdentity(m);
    m->m[0] = 2.f * near / (right - left);
    m->m[5] = 2.f * near / (top - bottom);
    m->m[8] = (right + left) / (right - left);
    m->m[9] = (top + bottom) / (top - bottom);
    m->m[10]= -(far + near) / (far - near);
    m->m[11]= -1.f;
    m->m[14]= -2.f * far * near / (far - near);
    m->m[15]= 0.f;

    //m->loadFrustum(l, r, b, t, n, f);
}

//Scalar
void rsMatrixLoadPerspective(rs_matrix4x4 *m, float fovy, float aspect, float near, float far) {
    float top = near * tan((float) (fovy * M_PI / 360.0f));
    float bottom = -top;
    float left = bottom * aspect;
    float right = top * aspect;
    rsMatrixLoadFrustum(m, left, right, bottom, top, near, far);

    //m->loadPerspective(fovy, aspect, near, far);
}

bool rsMatrixInverse(rs_matrix4x4 *m) {
  F4 * M = (F4 *) m->m;
  F4  m0, m1, m2, m3;
  F4  r0, r1, r2, r3;
  F4  det, t1;


  VEC(r0) = VEC(M[0]);
  VEC(r1) = VEC(M[1]);
  VEC(r2) = VEC(M[2]);
  VEC(r3) = VEC(M[3]);

  VEC(m0) = __builtin_shufflevector(VEC(r0), VEC(r2), 0, 4, 1, 5);
  VEC(m1) = __builtin_shufflevector(VEC(r0), VEC(r2), 2, 6, 3, 7);
  VEC(m2) = __builtin_shufflevector(VEC(r1), VEC(r3), 0, 4, 1, 5);
  VEC(m3) = __builtin_shufflevector(VEC(r1), VEC(r3), 2, 6, 3, 7);

  VEC(r0) = __builtin_shufflevector(VEC(m0), VEC(m2), 0, 4, 1, 5);
  VEC(r1) = __builtin_shufflevector(VEC(m0), VEC(m2), 2, 6, 3, 7);
  VEC(r2) = __builtin_shufflevector(VEC(m1), VEC(m3), 0, 4, 1, 5);
  VEC(r3) = __builtin_shufflevector(VEC(m1), VEC(m3), 2, 6, 3, 7);

  VEC(r1) = __builtin_shufflevector(VEC(r1), VEC(r1), 2, 3, 0, 1);
  VEC(r3) = __builtin_shufflevector(VEC(r3), VEC(r3), 2, 3, 0, 1);


  VEC(t1) = VEC(r2) * VEC(r3);
  VEC(t1) = __builtin_shufflevector(VEC(t1), VEC(t1), 1, 0, 3, 2);
  VEC(m0) = VEC(r1) * VEC(t1);
  VEC(m1) = VEC(r0) * VEC(t1);
  VEC(t1) = __builtin_shufflevector(VEC(t1), VEC(t1), 2, 3, 0, 1);
  VEC(m0) = (VEC(r1) * VEC(t1)) - VEC(m0);
  VEC(m1) = (VEC(r0) * VEC(t1)) - VEC(m1);
  VEC(m1) = __builtin_shufflevector(VEC(m1), VEC(m1), 2, 3, 0, 1);

  VEC(t1) = VEC(r1) * VEC(r2);
  VEC(t1) = __builtin_shufflevector(VEC(t1), VEC(t1), 1, 0, 3, 2);
  VEC(m0) += (VEC(r3) * VEC(t1));
  VEC(m3) = (VEC(r0) * VEC(t1));

  VEC(t1) = __builtin_shufflevector(VEC(t1), VEC(t1), 2, 3, 0, 1);
  VEC(m0) = VEC(m0) - (VEC(r3) * VEC(t1));
  VEC(m3) = (VEC(r0) * VEC(t1)) - VEC(m3);
  VEC(m3) = __builtin_shufflevector(VEC(m3), VEC(m3), 2, 3, 0, 1);

  VEC(t1) = __builtin_shufflevector(VEC(r1), VEC(r1), 2, 3, 0, 1);
  VEC(t1) = VEC(t1) * VEC(r3);
  VEC(t1) = __builtin_shufflevector(VEC(t1), VEC(t1), 1, 0, 3, 2);
  VEC(r2) = __builtin_shufflevector(VEC(r2), VEC(r2), 2, 3, 0, 1);
  VEC(m0) += VEC(r2) * VEC(t1);
  VEC(m2) = VEC(r0) * VEC(t1);

  VEC(t1) = __builtin_shufflevector(VEC(t1), VEC(t1), 2, 3, 0, 1);
  VEC(m0) = VEC(m0) - (VEC(r2) * VEC(t1));
  VEC(m2) = (VEC(r0) * VEC(t1)) - VEC(m2);
  VEC(m2) = __builtin_shufflevector(VEC(m2), VEC(m2), 2, 3, 0, 1);

  VEC(t1) = VEC(r0) * VEC(r1);
  VEC(t1) = __builtin_shufflevector(VEC(t1), VEC(t1), 1, 0, 3, 2);
  VEC(m2) += VEC(r3)*VEC(t1);
  VEC(m3) = (VEC(r2)*VEC(t1)) - VEC(m3);
  VEC(t1) = __builtin_shufflevector(VEC(t1), VEC(t1), 2, 3, 0, 1);
  VEC(m2) = (VEC(r3)*VEC(t1)) - VEC(m2);
  VEC(m3) = VEC(m3) - (VEC(r2)*VEC(t1));

  VEC(t1) = VEC(r0) * VEC(r3);
  VEC(t1) = __builtin_shufflevector(VEC(t1), VEC(t1), 1, 0, 3, 2);
  VEC(m1) = VEC(m1) - (VEC(r2)*VEC(t1));
  VEC(m2) += VEC(r1)*VEC(t1);
  VEC(t1) = __builtin_shufflevector(VEC(t1), VEC(t1), 2, 3, 0, 1);
  VEC(m1) += VEC(r2)*VEC(t1);
  VEC(m2) = VEC(m2) - (VEC(r1)*VEC(t1));

  VEC(t1) = VEC(r0)*VEC(r2);
  VEC(t1) = __builtin_shufflevector(VEC(t1), VEC(t1), 1, 0, 3, 2);

  VEC(m1) += VEC(r3)*VEC(t1);
  VEC(m3) = VEC(m3) - (VEC(r1)*VEC(t1));
  VEC(t1) = __builtin_shufflevector(VEC(t1), VEC(t1), 2, 3, 0, 1);
  VEC(m1) = VEC(m1) - (VEC(r3)*VEC(t1));
  VEC(m3) += VEC(r1)*VEC(t1);

  // after computing VEC(m0)..3, compute determinant

  VEC(det) = VEC(r0) * VEC(m0);
  VEC(det) += __builtin_shufflevector(VEC(det), VEC(det), 2, 3, 0, 1);
  det.x += det.y;
  if (fabs(det.x) < 1e-6f) {
    return false;
  }

  det.x = 1.0f/det.x;

  VEC(det) = initF4(det.x, det.x, det.x, det.x);
  VEC(m0) *= VEC(det);
  VEC(m1) *= VEC(det);
  VEC(m2) *= VEC(det);
  VEC(m3) *= VEC(det);
  VEC(M[0]) = VEC(m0);
  VEC(M[1]) = VEC(m1);
  VEC(M[2]) = VEC(m2);
  VEC(M[3]) = VEC(m3);
  return true;
}

bool rsMatrixInverseTranspose(rs_matrix4x4 *m) {
  F4 * M = (F4 *) m->m;
  F4  m0, m1, m2, m3;
  F4  r0, r1, r2, r3;
  F4  det, t1;

  VEC(r0) = VEC(M[0]);
  VEC(r1) = VEC(M[1]);
  VEC(r2) = VEC(M[2]);
  VEC(r3) = VEC(M[3]);

  VEC(m0) = __builtin_shufflevector(VEC(r0), VEC(r2), 0, 4, 1, 5);
  VEC(m1) = __builtin_shufflevector(VEC(r0), VEC(r2), 2, 6, 3, 7);
  VEC(m2) = __builtin_shufflevector(VEC(r1), VEC(r3), 0, 4, 1, 5);
  VEC(m3) = __builtin_shufflevector(VEC(r1), VEC(r3), 2, 6, 3, 7);

  VEC(r0) = __builtin_shufflevector(VEC(m0), VEC(m2), 0, 4, 1, 5);
  VEC(r1) = __builtin_shufflevector(VEC(m0), VEC(m2), 2, 6, 3, 7);
  VEC(r2) = __builtin_shufflevector(VEC(m1), VEC(m3), 0, 4, 1, 5);
  VEC(r3) = __builtin_shufflevector(VEC(m1), VEC(m3), 2, 6, 3, 7);

  VEC(r1) = __builtin_shufflevector(VEC(r1), VEC(r1), 2, 3, 0, 1);
  VEC(r3) = __builtin_shufflevector(VEC(r3), VEC(r3), 2, 3, 0, 1);

  VEC(t1) = VEC(r2) * VEC(r3);
  VEC(t1) = __builtin_shufflevector(VEC(t1), VEC(t1), 1, 0, 3, 2);
  VEC(m0) = VEC(r1) * VEC(t1);
  VEC(m1) = VEC(r0) * VEC(t1);
  VEC(t1) = __builtin_shufflevector(VEC(t1), VEC(t1), 2, 3, 0, 1);
  VEC(m0) = (VEC(r1) * VEC(t1)) - VEC(m0);
  VEC(m1) = (VEC(r0) * VEC(t1)) - VEC(m1);
  VEC(m1) = __builtin_shufflevector(VEC(m1), VEC(m1), 2, 3, 0, 1);

  VEC(t1) = VEC(r1) * VEC(r2);
  VEC(t1) = __builtin_shufflevector(VEC(t1), VEC(t1), 1, 0, 3, 2);
  VEC(m0) += (VEC(r3) * VEC(t1));
  VEC(m3) = (VEC(r0) * VEC(t1));

  VEC(t1) = __builtin_shufflevector(VEC(t1), VEC(t1), 2, 3, 0, 1);
  VEC(m0) = VEC(m0) - (VEC(r3) * VEC(t1));
  VEC(m3) = (VEC(r0) * VEC(t1)) - VEC(m3);
  VEC(m3) = __builtin_shufflevector(VEC(m3), VEC(m3), 2, 3, 0, 1);
  VEC(t1) = __builtin_shufflevector(VEC(r1), VEC(r1), 2, 3, 0, 1);
  VEC(t1) = VEC(t1) * VEC(r3);
  VEC(t1) = __builtin_shufflevector(VEC(t1), VEC(t1), 1, 0, 3, 2);
  VEC(r2) = __builtin_shufflevector(VEC(r2), VEC(r2), 2, 3, 0, 1);
  VEC(m0) += VEC(r2) * VEC(t1);
  VEC(m2) = VEC(r0) * VEC(t1);

  VEC(t1) = __builtin_shufflevector(VEC(t1), VEC(t1), 2, 3, 0, 1);
  VEC(m0) = VEC(m0) - (VEC(r2) * VEC(t1));
  VEC(m2) = (VEC(r0) * VEC(t1)) - VEC(m2);
  VEC(m2) = __builtin_shufflevector(VEC(m2), VEC(m2), 2, 3, 0, 1);
  VEC(t1) = VEC(r0) * VEC(r1);
  VEC(t1) = __builtin_shufflevector(VEC(t1), VEC(t1), 1, 0, 3, 2);
  VEC(m2) += VEC(r3)*VEC(t1);
  VEC(m3) = (VEC(r2)*VEC(t1)) - VEC(m3);
  VEC(t1) = __builtin_shufflevector(VEC(t1), VEC(t1), 2, 3, 0, 1);
  VEC(m2) = (VEC(r3)*VEC(t1)) - VEC(m2);
  VEC(m3) = VEC(m3) - (VEC(r2)*VEC(t1));

  VEC(t1) = VEC(r0) * VEC(r3);
  VEC(t1) = __builtin_shufflevector(VEC(t1), VEC(t1), 1, 0, 3, 2);
  VEC(m1) = VEC(m1) - (VEC(r2)*VEC(t1));
  VEC(m2) += VEC(r1)*VEC(t1);
  VEC(t1) = __builtin_shufflevector(VEC(t1), VEC(t1), 2, 3, 0, 1);
  VEC(m1) += VEC(r2)*VEC(t1);
  VEC(m2) = VEC(m2) - (VEC(r1)*VEC(t1));

  VEC(t1) = VEC(r0)*VEC(r2);
  VEC(t1) = __builtin_shufflevector(VEC(t1), VEC(t1), 1, 0, 3, 2);
  VEC(m1) += VEC(r3)*VEC(t1);
  VEC(m3) = VEC(m3) - (VEC(r1)*VEC(t1));
  VEC(t1) = __builtin_shufflevector(VEC(t1), VEC(t1), 2, 3, 0, 1);
  VEC(m1) = VEC(m1) - (VEC(r3)*VEC(t1));
  VEC(m3) += VEC(r1)*VEC(t1);

  // after computing m0..3, compute determinant

  VEC(det) = VEC(r0) * VEC(m0);
  VEC(det) += __builtin_shufflevector(VEC(det), VEC(det), 2, 3, 0, 1);
  det.x += det.y;
  if (fabs(det.x) < 1e-6f) {
    return false;
  }

  det.x = 1.0f/det.x;

  VEC(det) = initF4(det.x, det.x, det.x, det.x);
  VEC(m0) *= VEC(det);
  VEC(m1) *= VEC(det);
  VEC(m2) *= VEC(det);
  VEC(m3) *= VEC(det);

  //  Transpose the matrix

  VEC(r0) = __builtin_shufflevector(VEC(m0), VEC(m2), 0, 4, 1, 5);
  VEC(r1) = __builtin_shufflevector(VEC(m0), VEC(m2), 2, 6, 3, 7);
  VEC(r2) = __builtin_shufflevector(VEC(m1), VEC(m3), 0, 4, 1, 5);
  VEC(r3) = __builtin_shufflevector(VEC(m1), VEC(m3), 2, 6, 3, 7);

  VEC(M[0]) = __builtin_shufflevector(VEC(r0), VEC(r2), 0, 4, 1, 5);
  VEC(M[1]) = __builtin_shufflevector(VEC(r0), VEC(r2), 2, 6, 3, 7);
  VEC(M[2]) = __builtin_shufflevector(VEC(r1), VEC(r3), 0, 4, 1, 5);
  VEC(M[3]) = __builtin_shufflevector(VEC(r1), VEC(r3), 2, 6, 3, 7);

  return true;
}

//Scalar
void rsMatrixTranspose(rs_matrix4x4 *m) {
    F4 * M = (F4 *) m->m;
    F4 L0_2, H0_2, L1_3, H1_3;
    F4 R0, R1, R2, R3;
    VEC(R0) = VEC(M[0]);
    VEC(R1) = VEC(M[1]);
    VEC(R2) = VEC(M[2]);
    VEC(R3) = VEC(M[3]);
    VEC(L0_2) = __builtin_shufflevector(VEC(R0), VEC(R2), 0, 4, 1, 5);
    VEC(H0_2) = __builtin_shufflevector(VEC(R0), VEC(R2), 2, 6, 3, 7);
    VEC(L1_3) = __builtin_shufflevector(VEC(R1), VEC(R3), 0, 4, 1, 5);
    VEC(H1_3) = __builtin_shufflevector(VEC(R1), VEC(R3), 2, 6, 3, 7);

    VEC(M[0]) = __builtin_shufflevector(VEC(L0_2), VEC(L1_3), 0, 4, 1, 5);
    VEC(M[1]) = __builtin_shufflevector(VEC(L0_2), VEC(L1_3), 2, 6, 3, 7);
    VEC(M[2]) = __builtin_shufflevector(VEC(H0_2), VEC(H1_3), 0, 4, 1, 5);
    VEC(M[3]) = __builtin_shufflevector(VEC(H0_2), VEC(H1_3), 2, 6, 3, 7);
}

//Scalar
void rsMatrixTranspose(rs_matrix3x3 *m) {
    int i, j;
    float temp;
    for (i = 0; i < 2; ++i) {
        for (j = i + 1; j < 3; ++j) {
            temp = m3_get(m, i, j);
            m3_set(m, i, j, m3_get(m, j, i));
            m3_set(m, j, i, temp);
        }
    }
}

//Scalar
void rsMatrixTranspose(rs_matrix2x2 *m) {
    float temp = m->m[1];
    m->m[1] = m->m[2];
    m->m[2] = temp;
}
