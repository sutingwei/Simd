/*
* Simd Library (http://ermig1979.github.io/Simd).
*
* Copyright (c) 2011-2021 Yermalayeu Ihar.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/
#include "Simd/SimdMemory.h"
#include "Simd/SimdImageSave.h"
#include "Simd/SimdImageSavePng.h"
#include "Simd/SimdBase.h"
#include "Simd/SimdNeon.h"
#include "Simd/SimdSet.h"
#include "Simd/SimdExtract.h"
#include "Simd/SimdStore.h"

namespace Simd
{        
#ifdef SIMD_NEON_ENABLE    
    namespace Neon
    {
        uint32_t ZlibAdler32(uint8_t* data, int size)
        {
            int32x4_t _i0 = SetI32(0, -1, -2, -3), _4 = vdupq_n_s32(4);
            uint32_t lo = 1, hi = 0;
            for (int b = 0, n = (int)(size % 5552); b < size;)
            {
                int n8 = n & (~7), i = 0;
                int32x4_t _i = vaddq_s32(_i0, vdupq_n_s32(n));
                int32x4_t _l = vdupq_n_s32(0), _h = vdupq_n_s32(0);
                for (; i < n8; i += 8)
                {
                    uint8x8_t d8 = LoadHalf<false>(data + b + i);
                    int16x8_t d16 = (int16x8_t)vmovl_u8(d8);
                    int32x4_t d0 = vmovl_s16(Half<0>(d16));
                    _l = vaddq_s32(_l, d0);
                    _h = vmlaq_s32(_h, d0, _i);
                    _i = vsubq_s32(_i, _4);
                    int32x4_t d1 = vmovl_s16((int16x4_t)Half<1>(d16));
                    _l = vaddq_s32(_l, d1);
                    _h = vmlaq_s32(_h, d1, _i);
                    _i = vsubq_s32(_i, _4);
                }
                int l = ExtractSum32s(_l), h = ExtractSum32s(_h);
                for (; i < n; ++i)
                {
                    l += data[b + i];
                    h += data[b + i]*(n - i);
                }
                hi = (hi + h + lo*n) % 65521;
                lo = (lo + l) % 65521;
                b += n;
                n = 5552;
            }
            return (hi << 16) | lo;
        }

        void ZlibCompress(uint8_t* data, int size, int quality, OutputMemoryStream& stream)
        {
            const int ZHASH = 16384;
            if (quality < 5)
                quality = 5;
            const int basket = quality * 2;
            Array32i hashTable(ZHASH * basket);
            memset(hashTable.data, -1, hashTable.RawSize());

            stream.Write(uint8_t(0x78));
            stream.Write(uint8_t(0x5e));
            stream.WriteBits(1, 1);
            stream.WriteBits(1, 2);

            int i = 0, j;
            while (i < size - 3)
            {
                int h = Base::ZlibHash(data + i) & (ZHASH - 1), best = 3;
                uint8_t* bestLoc = 0;
                int* hList = hashTable.data + h * basket;
                for (j = 0; hList[j] != -1 && j < basket; ++j)
                {
                    if (hList[j] > i - 32768)
                    {
                        int d = Base::ZlibCount(data + hList[j], data + i, size - i);
                        if (d >= best)
                        {
                            best = d;
                            bestLoc = data + hList[j];
                        }
                    }
                }
                if (j == basket)
                {
                    memcpy(hList, hList + quality, quality * sizeof(int));
                    memset(hList + quality, -1, quality * sizeof(int));
                    j = quality;
                }
                hList[j] = i;

                if (bestLoc)
                {
                    h = Base::ZlibHash(data + i + 1) & (ZHASH - 1);
                    int* hList = hashTable.data + h * basket;
                    for (j = 0; hList[j] != -1 && j < basket; ++j)
                    {
                        if (hList[j] > i - 32767)
                        {
                            int e = Base::ZlibCount(data + hList[j], data + i + 1, size - i - 1);
                            if (e > best)
                            {
                                bestLoc = NULL;
                                break;
                            }
                        }
                    }
                }

                if (bestLoc)
                {
                    int d = (int)(data + i - bestLoc);
                    assert(d <= 32767 && best <= 258);
                    for (j = 0; best > Base::ZlibLenC[j + 1] - 1; ++j);
                    Base::ZlibHuff(j + 257, stream);
                    if (Base::ZlibLenEb[j])
                        stream.WriteBits(best - Base::ZlibLenC[j], Base::ZlibLenEb[j]);
                    for (j = 0; d > Base::ZlibDistC[j + 1] - 1; ++j);
                    stream.WriteBits(Base::ZlibBitRev(j, 5), 5);
                    if (Base::ZlibDistEb[j])
                        stream.WriteBits(d - Base::ZlibDistC[j], Base::ZlibDistEb[j]);
                    i += best;
                }
                else
                {
                    Base::ZlibHuffB(data[i], stream);
                    ++i;
                }
            }
            for (; i < size; ++i)
                Base::ZlibHuffB(data[i], stream);
            Base::ZlibHuff(256, stream);
            stream.FlushBits(true);
            stream.WriteBe32(ZlibAdler32(data, size));
        }

        uint32_t EncodeLine0(const uint8_t* src, size_t stride, size_t n, size_t size, int8_t* dst)
        {
            size_t i = 0, sizeA = AlignLo(size, A), bS = A << 7, bC = (sizeA >> 7) + 1;
            uint32x4_t _sum = vdupq_n_u32(0);
            for (size_t b = 0; b < bC; ++b)
            {
                uint16x8_t bSum = vdupq_n_u16(0);
                for (size_t end = Min(i + bS, sizeA); i < end; i += A)
                {
                    int8x16_t _src = (int8x16_t)Load<false>(src + i);
                    Store<false>(dst + i, _src);
                    bSum = vaddq_u16(bSum, vpaddlq_u8((uint8x16_t)vabsq_s8(_src)));
                }
                _sum = vaddq_u32(_sum, vpaddlq_u16(bSum));
            }
            uint32_t sum = ExtractSum32u(_sum);
            for (; i < size; ++i)
            {
                dst[i] = src[i];
                sum += ::abs(dst[i]);
            }
            return sum;
        }

        uint32_t EncodeLine1(const uint8_t* src, size_t stride, size_t n, size_t size, int8_t* dst)
        {
            size_t i = 0, sizeA = AlignLo(size - n, A) + n, bS = A << 7, bC = (sizeA >> 7) + 1;;
            uint32_t sum = 0;
            for (; i < n; ++i)
            {
                dst[i] = src[i];
                sum += ::abs(dst[i]);
            }
            uint32x4_t _sum = vdupq_n_u32(0);
            for (size_t b = 0; b < bC; ++b)
            {
                uint16x8_t bSum = vdupq_n_u16(0);
                for (size_t end = Min(i + bS, sizeA); i < end; i += A)
                {
                    int8x16_t _src0 = (int8x16_t)Load<false>(src + i);
                    int8x16_t _src1 = (int8x16_t)Load<false>(src + i - n);
                    int8x16_t _dst = vsubq_s8(_src0, _src1);
                    Store<false>(dst + i, _dst);
                    bSum = vaddq_u16(bSum, vpaddlq_u8((uint8x16_t)vabsq_s8(_dst)));
                }
                _sum = vaddq_u32(_sum, vpaddlq_u16(bSum));
            }
            sum += ExtractSum32u(_sum);
            for (; i < size; ++i)
            {
                dst[i] = src[i] - src[i - n];
                sum += ::abs(dst[i]);
            }
            return sum;
        }
#if 0
        uint32_t EncodeLine2(const uint8_t* src, size_t stride, size_t n, size_t size, int8_t* dst)
        {
            size_t i = 0, sizeA = AlignLo(size - n, A) + n;
            uint32_t sum = 0;
            for (; i < n; ++i)
            {
                dst[i] = src[i] - src[i - stride];
                sum += ::abs(dst[i]);
            }
            __m128i _sum = _mm_setzero_si128();
            for (; i < sizeA; i += A)
            {
                __m128i _src0 = _mm_loadu_si128((__m128i*)(src + i));
                __m128i _src1 = _mm_loadu_si128((__m128i*)(src + i - stride));
                __m128i _dst = _mm_sub_epi8(_src0, _src1);
                _mm_storeu_si128((__m128i*)(dst + i), _dst);
                _sum = _mm_add_epi32(_sum, _mm_sad_epu8(_mm_setzero_si128(), _mm_abs_epi8(_dst)));
            }
            sum += Sse2::ExtractInt32Sum(_sum);
            for (; i < size; ++i)
            {
                dst[i] = src[i] - src[i - stride];
                sum += ::abs(dst[i]);
            }
            return sum;
        }

        uint32_t EncodeLine3(const uint8_t* src, size_t stride, size_t n, size_t size, int8_t* dst)
        {
            size_t i = 0, sizeA = AlignLo(size - n, A) + n;
            uint32_t sum = 0;
            for (; i < n; ++i)
            {
                dst[i] = src[i] - (src[i - stride] >> 1);
                sum += ::abs(dst[i]);
            }
            __m128i _sum = _mm_setzero_si128();
            for (; i < sizeA; i += A)
            {
                __m128i _src0 = _mm_loadu_si128((__m128i*)(src + i));
                __m128i _src1 = _mm_loadu_si128((__m128i*)(src + i - n));
                __m128i _src2 = _mm_loadu_si128((__m128i*)(src + i - stride));
                __m128i lo = _mm_srli_epi16(_mm_add_epi16(UnpackU8<0>(_src1), UnpackU8<0>(_src2)), 1);
                __m128i hi = _mm_srli_epi16(_mm_add_epi16(UnpackU8<1>(_src1), UnpackU8<1>(_src2)), 1);
                __m128i _dst = _mm_sub_epi8(_src0, _mm_packus_epi16(lo, hi));
                _mm_storeu_si128((__m128i*)(dst + i), _dst);
                _sum = _mm_add_epi32(_sum, _mm_sad_epu8(_mm_setzero_si128(), _mm_abs_epi8(_dst)));
            }
            sum += Sse2::ExtractInt32Sum(_sum);
            for (; i < size; ++i)
            {
                dst[i] = src[i] - ((src[i - n] + src[i - stride]) >> 1);
                sum += ::abs(dst[i]);
            }
            return sum;
        }

        SIMD_INLINE __m128i Paeth(__m128i a, __m128i b, __m128i c)
        {
            __m128i p = _mm_sub_epi16(_mm_add_epi16(a, b), c);
            __m128i pa = _mm_abs_epi16(_mm_sub_epi16(p, a));
            __m128i pb = _mm_abs_epi16(_mm_sub_epi16(p, b));
            __m128i pc = _mm_abs_epi16(_mm_sub_epi16(p, c));
            __m128i mbc = _mm_or_si128(_mm_cmpgt_epi16(pa, pb), _mm_cmpgt_epi16(pa, pc));
            __m128i mc = _mm_cmpgt_epi16(pb, pc);
            return _mm_blendv_epi8(a, _mm_blendv_epi8(b, c, mc), mbc);
        }

        uint32_t EncodeLine4(const uint8_t* src, size_t stride, size_t n, size_t size, int8_t* dst)
        {
            size_t i = 0, sizeA = AlignLo(size - n, A) + n;
            uint32_t sum = 0;
            for (; i < n; ++i)
            {
                dst[i] = (int8_t)(src[i] - src[i - stride]);
                sum += ::abs(dst[i]);
            }
            __m128i _sum = _mm_setzero_si128();
            for (; i < sizeA; i += A)
            {
                __m128i _src0 = _mm_loadu_si128((__m128i*)(src + i));
                __m128i _src1 = _mm_loadu_si128((__m128i*)(src + i - n));
                __m128i _src2 = _mm_loadu_si128((__m128i*)(src + i - stride));
                __m128i _src3 = _mm_loadu_si128((__m128i*)(src + i - stride - n));
                __m128i lo = Paeth(UnpackU8<0>(_src1), UnpackU8<0>(_src2), UnpackU8<0>(_src3));
                __m128i hi = Paeth(UnpackU8<1>(_src1), UnpackU8<1>(_src2), UnpackU8<1>(_src3));
                __m128i _dst = _mm_sub_epi8(_src0, _mm_packus_epi16(lo, hi));
                _mm_storeu_si128((__m128i*)(dst + i), _dst);
                _sum = _mm_add_epi32(_sum, _mm_sad_epu8(_mm_setzero_si128(), _mm_abs_epi8(_dst)));
            }
            sum += Sse2::ExtractInt32Sum(_sum);
            for (; i < size; ++i)
            {
                dst[i] = src[i] - Base::Paeth(src[i - n], src[i - stride], src[i - stride - n]);
                sum += ::abs(dst[i]);
            }
            return sum;
        }

        uint32_t EncodeLine5(const uint8_t* src, size_t stride, size_t n, size_t size, int8_t* dst)
        {
            size_t i = 0, sizeA = AlignLo(size - n, A) + n;
            uint32_t sum = 0;
            for (; i < n; ++i)
            {
                dst[i] = src[i];
                sum += ::abs(dst[i]);
            }
            __m128i _sum = _mm_setzero_si128();
            for (; i < sizeA; i += A)
            {
                __m128i _src0 = _mm_loadu_si128((__m128i*)(src + i));
                __m128i _src1 = _mm_loadu_si128((__m128i*)(src + i - n));
                __m128i lo = _mm_srli_epi16(UnpackU8<0>(_src1), 1);
                __m128i hi = _mm_srli_epi16(UnpackU8<1>(_src1), 1);
                __m128i _dst = _mm_sub_epi8(_src0, _mm_packus_epi16(lo, hi));
                _mm_storeu_si128((__m128i*)(dst + i), _dst);
                _sum = _mm_add_epi32(_sum, _mm_sad_epu8(_mm_setzero_si128(), _mm_abs_epi8(_dst)));
            }
            sum += Sse2::ExtractInt32Sum(_sum);
            for (; i < size; ++i)
            {
                dst[i] = src[i] - (src[i - n] >> 1);
                sum += ::abs(dst[i]);
            }
            return sum;
        }

        uint32_t EncodeLine6(const uint8_t* src, size_t stride, size_t n, size_t size, int8_t* dst)
        {
            size_t i = 0, sizeA = AlignLo(size - n, A) + n;
            uint32_t sum = 0;
            for (; i < n; ++i)
            {
                dst[i] = src[i];
                sum += ::abs(dst[i]);
            }
            __m128i _sum = _mm_setzero_si128();
            for (; i < sizeA; i += A)
            {
                __m128i _src0 = _mm_loadu_si128((__m128i*)(src + i));
                __m128i _src1 = _mm_loadu_si128((__m128i*)(src + i - n));
                __m128i _dst = _mm_sub_epi8(_src0, _src1);
                _mm_storeu_si128((__m128i*)(dst + i), _dst);
                _sum = _mm_add_epi32(_sum, _mm_sad_epu8(_mm_setzero_si128(), _mm_abs_epi8(_dst)));
            }
            sum += Sse2::ExtractInt32Sum(_sum);
            for (; i < size; ++i)
            {
                dst[i] = src[i] - src[i - n];
                sum += ::abs(dst[i]);
            }
            return sum;
        }
#endif
        ImagePngSaver::ImagePngSaver(const ImageSaverParam& param)
            : Base::ImagePngSaver(param)
        {
            if (_param.format == SimdPixelFormatBgr24)
                _convert = Neon::BgrToRgb;
            else if (_param.format == SimdPixelFormatBgra32)
                _convert = Neon::BgraToRgba;
            _encode[0] = Neon::EncodeLine0;
            _encode[1] = Neon::EncodeLine1;
            //_encode[2] = Sse41::EncodeLine2;
            //_encode[3] = Sse41::EncodeLine3;
            //_encode[4] = Sse41::EncodeLine4;
            //_encode[5] = Sse41::EncodeLine5;
            //_encode[6] = Sse41::EncodeLine6;
            _compress = Neon::ZlibCompress;
        }
    }
#endif// SIMD_NEON_ENABLE
}
