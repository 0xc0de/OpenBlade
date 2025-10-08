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

#include "BMV.h"
#include <Hork/Core/IO.h>

using namespace Hk;

void BladeAnimation::Load(StringView fileName)
{
    Clear();

    File f = File::sOpenRead(fileName);
    if (!f)
        return;

    String name = f.ReadString();

    Nodes.Resize(f.ReadInt32());
    for (auto& node : Nodes)
    {
        node.Keyframes.Resize(f.ReadInt32());

        for (auto& q : node.Keyframes)
        {
            q.W = f.ReadFloat();
            q.X = f.ReadFloat();
            q.Y = f.ReadFloat();
            q.Z = f.ReadFloat();
        }
    }

    Keyframes.Resize(f.ReadInt32());
    for (auto& position : Keyframes)
    {
        position = f.ReadObject<Double3>();
    }
}

void BladeAnimation::Clear()
{
    Nodes.Clear();
    Keyframes.Clear();
}
