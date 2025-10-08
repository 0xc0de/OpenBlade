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

#include <Hork/Core/String.h>
#include <Hork/Core/Containers/Vector.h>
#include <Hork/Math/VectorMath.h>

using namespace Hk;

struct BladeSF
{
    struct GhostSector
    {
        String Name;
        float FloorHeight = 0;
        float RoofHeight = 0;
        Vector<Float2> Vertices;
        String Group;
        String Sound;
        float Volume = 0;
        float VolumeBase = 0;
        float MinDist = 0;
        float MaxDist = 0;
        float MaxVerticalDist = 0;
        float Scale = 0;
    };

    Vector<GhostSector> GhostSectors;

    void Load(StringView fileName);
};
