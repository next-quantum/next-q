#pragma once

// headers

#include <arm_neon.h>

#include <math.h>
#include <stdlib.h>

#ifdef __cplusplus
using namespace std;
#endif

// typdef
#if defined(__GNUC__) || defined(__clang__)

#pragma push_macro("FORCE_INLINE")
#pragma push_macro("ALIGN_STRUCT")
#define FORCE_INLINE static inline __attribute__((always_inline))
#define ALIGN_STRUCT(x) __attribute__((aligned(x)))

#else

#error "Macro name collisions may happens with unknown compiler"
#ifdef FORCE_INLINE
#undef FORCE_INLINE
#endif

#define FORCE_INLINE static inline
#ifndef ALIGN_STRUCT
#define ALIGN_STRUCT(x) __declspec(align(x))
#endif

#endif

#define likely(x)     __builtin_expect(!!(x), 1)
#define unlikely(x)   __builtin_expect(!!(x), 0)

#ifndef __int32
#define __int32 int
#endif

#ifndef __int64
#define __int64 long long
#endif

#ifndef __mmask64
#define __mmask64 unsigned long long
#endif

#ifndef __mmask16
#define __mmask16 unsigned short
#endif

#ifndef __mmask8
#define __mmask8 unsigned char
#endif

// emmintrin

#define  SET64x2(res, e0, e1)                            \
    __asm__ __volatile__ (                                  \
        "mov %[r].d[0], %[x]         \n\t"                  \
        "mov %[r].d[1], %[y]         \n\t"                  \
        :[r]"=w"(res)                                       \
        :[x]"r"(e0), [y]"r"(e1)                             \
    );


typedef float64x2_t __m128d;

FORCE_INLINE __m128d _mm_load_pd (double const* mem_addr)
{
    __m128d res;
    res = vld1q_f64((const double *)mem_addr);
    return res;
}

FORCE_INLINE void _mm_store_pd (double* mem_addr, __m128d a)
{
    vst1q_f64(mem_addr, a);
}

FORCE_INLINE __m128d _mm_set_pd(double e1, double e0)
{
    __m128d res_m128d;
    SET64x2(res_m128d, e0, e1);
    return res_m128d;

    // double ALIGN_STRUCT(16) data[2] = {e0, e1};
    // return vld1q_f64((float64_t *) data);
}

// avxintrin

typedef struct {
    float64x2_t vect_f64[2];
} __m256d;

FORCE_INLINE __m256d _mm256_load_pd (double const * mem_addr)
{
    __m256d res;
    res.vect_f64[0] = vld1q_f64((const double *)mem_addr);
    res.vect_f64[1] = vld1q_f64((const double *)mem_addr + 2);
    return res;
}

FORCE_INLINE __m256d _mm256_set1_pd(double a)
{
    __m256d ret;
    ret.vect_f64[0] = ret.vect_f64[1] = vdupq_n_f64(a);
    return ret;
}

FORCE_INLINE void _mm256_store_pd (double * mem_addr, __m256d a)
{
    vst1q_f64(mem_addr, a.vect_f64[0]);
    vst1q_f64(mem_addr + 2, a.vect_f64[1]);
}

FORCE_INLINE __m256d _mm256_mul_pd(__m256d a, __m256d b)
{
    __m256d res_m256d;
    res_m256d.vect_f64[0] = vmulq_f64(a.vect_f64[0], b.vect_f64[0]);
    res_m256d.vect_f64[1] = vmulq_f64(a.vect_f64[1], b.vect_f64[1]);
    return res_m256d;
}

FORCE_INLINE __m256d _mm256_add_pd(__m256d a, __m256d b)
{
    __m256d res_m256d;
    res_m256d.vect_f64[0] = vaddq_f64(a.vect_f64[0], b.vect_f64[0]);
    res_m256d.vect_f64[1] = vaddq_f64(a.vect_f64[1], b.vect_f64[1]);
    return res_m256d;
}


// [self provided]
FORCE_INLINE __m256d _mm256_set_m128d(__m128d hi, __m128d lo)
{
    __m256d res_m256d;
    res_m256d.vect_f64[0] = lo;
    res_m256d.vect_f64[1] = hi;
    return res_m256d;
}

FORCE_INLINE __m256d _mm256_hsub_pd(__m256d a, __m256d b)
{
    __m256d res_m256d;
    res_m256d.vect_f64[0] = vsubq_f64(vuzp1q_f64(a.vect_f64[0], b.vect_f64[0]), vuzp2q_f64(a.vect_f64[0], b.vect_f64[0]));
    res_m256d.vect_f64[1] = vsubq_f64(vuzp1q_f64(a.vect_f64[1], b.vect_f64[1]), vuzp2q_f64(a.vect_f64[1], b.vect_f64[1]));
    return res_m256d;
}

FORCE_INLINE void _mm_storeu_pd(double *mem_addr, __m128d a)
{
    _mm_store_pd(mem_addr, a);
}

FORCE_INLINE void _mm256_storeu2_m128d(double* hiaddr, double* loaddr, __m256d a)
{
    _mm_storeu_pd(loaddr, a.vect_f64[0]);
    _mm_storeu_pd(hiaddr, a.vect_f64[1]);
}

FORCE_INLINE __m128d _mm_loadu_pd(const double *p)
{
    return _mm_load_pd(p);
}

FORCE_INLINE __m256d _mm256_loadu2_m128d (double const* hiaddr, double const* loaddr)
{
    __m256d res_m256d;
    res_m256d.vect_f64[0] = _mm_loadu_pd(loaddr);
    res_m256d.vect_f64[1] = _mm_loadu_pd(hiaddr);
    return res_m256d;
}

FORCE_INLINE __m256d _mm256_broadcast_pd (__m128d const * mem_addr)
{
    __m256d res_m256d;
    res_m256d.vect_f64[0] = vld1q_f64((float64_t const *)mem_addr);
    res_m256d.vect_f64[1] = res_m256d.vect_f64[0];
    return res_m256d;
}

FORCE_INLINE __m128d _mm_setr_pd(double e1, double e0)
{
    return _mm_set_pd(e0, e1);
}

FORCE_INLINE __m256d _mm256_setr_pd (double e3, double e2, double e1, double e0)
{
    __m256d res_m256d;
    res_m256d.vect_f64[0] = _mm_setr_pd(e3, e2);
    res_m256d.vect_f64[1] = _mm_setr_pd(e1, e0);
    return res_m256d;
}

FORCE_INLINE __m128d _mm_permute_pd (__m128d a, int imm8)
{
    __m128d res_m128d{0.0, 0.0};

    if (imm8 & 0x1) {
        res_m128d=vsetq_lane_f64(vgetq_lane_f64(a, 1), res_m128d, 0);
    } else { 
        res_m128d=vsetq_lane_f64(vgetq_lane_f64(a, 0), res_m128d, 0);
    }

    if (imm8 & 0x2) {
        res_m128d=vsetq_lane_f64(vgetq_lane_f64(a, 1), res_m128d, 1);
    } else { 
        res_m128d=vsetq_lane_f64(vgetq_lane_f64(a, 0), res_m128d, 1);
    }
    
    return res_m128d;
}

FORCE_INLINE __m256d _mm256_permute_pd (__m256d a, int imm8)
{
    __m256d res_m256d;
    res_m256d.vect_f64[0] = _mm_permute_pd(a.vect_f64[0], imm8 & 0x03);
    res_m256d.vect_f64[1] = _mm_permute_pd(a.vect_f64[1], (imm8 >> 2) & 0x03);
    return res_m256d;
}

// [AVX2 to AVX]
// FORCE_INLINE __m256d _mm256_insertf128_pd(__m256d a, __m128d b, int imm8)
// {
//     assert(imm8 == 0 || imm8 == 1);
//     __m256d res;
//     uint64x2_t vmask = vceqq_s64(vdupq_n_s64(imm8), vdupq_n_s64(0));
//     res.vect_f64[0] = vbslq_f64(vmask, b, a.vect_f64[0]);
//     res.vect_f64[1] = vbslq_f64(vmask, a.vect_f64[1], b);
//     return res;
// }

// FORCE_INLINE __m256d _mm256_castpd128_pd256(__m128d a)
// {
//     __m256d res;
//     res.vect_f64[0] = a;
//     return res;
// }

// FORCE_INLINE __m128d _mm256_castpd256_pd128(__m256d a)
// {
//     return a.vect_f64[0];
// }

// FORCE_INLINE __m128d _mm256_extractf128_pd (__m256d a, const int imm8)
// {
//     assert(imm8 >= 0 && imm8 <= 1);
//     return a.vect_f64[imm8];
// }

// avx512intrin

// immintrin

