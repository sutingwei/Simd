/*
* Simd Library (http://simd.sourceforge.net).
*
* Copyright (c) 2011-2016 Yermalayeu Ihar.
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
#ifndef __SimdSse_h__
#define __SimdSse_h__

#include "Simd/SimdDefs.h"

namespace Simd
{
#ifdef SIMD_SSE_ENABLE    
	namespace Sse
	{
		void AnnProductSum(const float * a, const float * b, size_t size, float * sum);

		void AnnRoughSigmoid(const float * src, size_t size, const float * slope, float * dst);

        void AnnDerivativeSigmoid(const float * src, size_t size, const float * slope, float * dst);

        void AnnRoughTanh(const float * src, size_t size, const float * slope, float * dst);

        void AnnDerivativeTanh(const float * src, size_t size, const float * slope, float * dst);

        void AnnUpdateWeights(const float * x, size_t size, const float * a, const float * b, float * d, float * w);

        void AnnAddConvolution3x3(const float * src, size_t srcStride, size_t width, size_t height, const float * weights, float * dst, size_t dstStride);

        void AnnAddConvolution5x5(const float * src, size_t srcStride, size_t width, size_t height, const float * weights, float * dst, size_t dstStride);

        void AnnMax2x2(const float * src, size_t srcStride, size_t width, size_t height, float * dst, size_t dstStride);

        void SquaredDifferenceSum32f(const float * a, const float * b, size_t size, float * sum);

        void SquaredDifferenceKahanSum32f(const float * a, const float * b, size_t size, float * sum);

        void SvmSumLinear(const float * x, const float * svs, const float * weights, size_t length, size_t count, float * sum);
    }
#endif// SIMD_SSE_ENABLE
}
#endif//__SimdSse_h__