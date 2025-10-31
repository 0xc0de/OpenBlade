/*

Open source re-implementation of Blade Of Darkness.

MIT License

Copyright (C) 2025 Alexander Samusev.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

#pragma once

#include <Hork/Math/VectorMath.h>
#include <Hork/Math/Plane.h>

using namespace Hk;

HK_FORCEINLINE HK_NODISCARD Float3 ConvertCoord(Float3 const& coord)
{
    return coord * Float3(0.001f, -0.001f, -0.001f);
}

HK_FORCEINLINE HK_NODISCARD Float3 ConvertAxis(Float3 const& coord)
{
    return coord * Float3(1, -1, -1);
}

HK_FORCEINLINE HK_NODISCARD PlaneD ConvertPlane(PlaneD const& plane)
{
    PlaneD newPlane = plane;
    newPlane.Normal.Y = -newPlane.Normal.Y;
    newPlane.Normal.Z = -newPlane.Normal.Z;
    newPlane.D *= 0.001;
    return newPlane;
}

HK_FORCEINLINE HK_NODISCARD Float3x4 ConvertMatrix3x4(Float4x4 const& matrix)
{
    Float4x4 m = matrix;
    m.TransposeSelf();

    Float3x4 m3x4;
    m3x4[0] = m[0];
    m3x4[1] = m[1];
    m3x4[2] = m[2];
    return m3x4;
}
