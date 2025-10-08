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

#include "CSV.h"
#include <Hork/Core/IO.h>

using namespace Hk;

void BladeCSV::Load(StringView fileName)
{
    char str[256];
    char model[256];
    char name[256];
    char nature[256];

    Entries.Clear();

    File f = File::sOpenRead(fileName);
    if (!f)
        return;

    while (f.Gets(str, sizeof(str) - 1))
    { 
        float unknownValue1;
        int unknownValue2;

        if (sscanf(str, "%s %s %f %d %s", model, name, &unknownValue1, &unknownValue2, nature) != 5)
        {
            LOG("BladeCSV::Load: Not enough parameters\n");
            continue;
        }

        auto& entry = Entries.EmplaceBack();
        entry.Model = model;
        entry.Name = name;
        entry.UnknownValue1 = unknownValue1;
        entry.UnknownValue2 = unknownValue2;
        entry.Nature = nature;
    }
}
