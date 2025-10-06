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

#include "FileDump.h"

#include <Hork/Core/Containers/Vector.h>

static bool DumpLogEnabled = false;
static bool LeadingZeros = true;

void SetDumpLog(bool enable)
{
    DumpLogEnabled = enable;
}

void DumpUnknownBytes(File& file, int bytesCount)
{
    Vector<uint8_t> bytes(bytesCount);

    size_t position = file.GetOffset();
    file.Read(bytes.ToPtr(), bytesCount);

    if (DumpLogEnabled)
    {
        for (uint8_t b : bytes)
        {
            LOG("{}: {} {}\n",
                Core::ToHexString(position++, LeadingZeros),
                Core::ToHexString(b, LeadingZeros),
                char(b));
        }
    }
}

void DumpIntOrFloat(File& file)
{
    int32_t unknown = file.ReadInt32();
    if (DumpLogEnabled)
    {
        uint8_t* pUnknown = (uint8_t*)&unknown;
        LOG("{}: {} {} hex : {} {} {} {}\n",
            Core::ToHexString(file.GetOffset() - 4, LeadingZeros),
            unknown,
            *(float*)&unknown,
            Core::ToHexString(pUnknown[0], LeadingZeros),
            Core::ToHexString(pUnknown[1], LeadingZeros),
            Core::ToHexString(pUnknown[2], LeadingZeros),
            Core::ToHexString(pUnknown[3], LeadingZeros));
    }
}

int32_t DumpInt(File& file)
{
    int32_t unknown = file.ReadInt32();
    if (DumpLogEnabled)
    {
        uint8_t* pUnknown = (uint8_t*)&unknown;
        LOG("{}: {} hex : {} {} {} {}\n",
            Core::ToHexString(file.GetOffset() - 4, LeadingZeros),
            unknown,
            Core::ToHexString(pUnknown[0], LeadingZeros),
            Core::ToHexString(pUnknown[1], LeadingZeros),
            Core::ToHexString(pUnknown[2], LeadingZeros),
            Core::ToHexString(pUnknown[3], LeadingZeros));
    }
    return unknown;
}

int16_t DumpShort(File& file)
{
    int16_t unknown = file.ReadInt16();
    if (DumpLogEnabled)
    {
        uint8_t* pUnknown = (uint8_t*)&unknown;
        LOG("{}: {} hex : {} {}\n",
            Core::ToHexString(file.GetOffset() - 2, LeadingZeros),
            unknown,
            Core::ToHexString(pUnknown[0], LeadingZeros),
            Core::ToHexString(pUnknown[1], LeadingZeros));
    }
    return unknown;
}

int32_t DumpIntNotSeek(File& file)
{
    int32_t unknown = file.ReadInt32();
    if (DumpLogEnabled)
    {
        uint8_t* pUnknown = (uint8_t*)&unknown;
        LOG("{}: {} hex : {} {} {} {}\n",
            Core::ToHexString(file.GetOffset() - 4, LeadingZeros),
            unknown,
            Core::ToHexString(pUnknown[0], LeadingZeros),
            Core::ToHexString(pUnknown[1], LeadingZeros),
            Core::ToHexString(pUnknown[2], LeadingZeros),
            Core::ToHexString(pUnknown[3], LeadingZeros));
    }
    file.SeekCur(-4);
    return unknown;
}

int32_t DumpByte(File& file)
{
    uint8_t unknown;
    file.Read(&unknown, 1);
    if (DumpLogEnabled)
    {
        LOG("{}: {} hex : {}\n",
            Core::ToHexString(file.GetOffset() - 1, LeadingZeros),
            unknown,
            Core::ToHexString(unknown, LeadingZeros));
    }
    return unknown;
}

float DumpFloat(File& file)
{
    float unknown = file.ReadFloat();
    if (DumpLogEnabled)
    {
        uint8_t* pUnknown = (uint8_t*)&unknown;
        LOG("{}: {} hex : {} {} {} {}\n",
            Core::ToHexString(file.GetOffset() - 4, LeadingZeros),
            unknown,
            Core::ToHexString(pUnknown[0], LeadingZeros),
            Core::ToHexString(pUnknown[1], LeadingZeros),
            Core::ToHexString(pUnknown[2], LeadingZeros),
            Core::ToHexString(pUnknown[3], LeadingZeros));
    }
    return unknown;
}

double DumpDouble(File& file)
{
    double unknown = file.ReadDouble();
    if (DumpLogEnabled)
    {
        uint8_t* pUnknown = (uint8_t*)&unknown;
        LOG("{}: {} hex : {} {} {} {} {} {} {} {}\n",
            Core::ToHexString(file.GetOffset() - 8, LeadingZeros),
            unknown,
            Core::ToHexString(pUnknown[0], LeadingZeros),
            Core::ToHexString(pUnknown[1], LeadingZeros),
            Core::ToHexString(pUnknown[2], LeadingZeros),
            Core::ToHexString(pUnknown[3], LeadingZeros),
            Core::ToHexString(pUnknown[4], LeadingZeros),
            Core::ToHexString(pUnknown[5], LeadingZeros),
            Core::ToHexString(pUnknown[6], LeadingZeros),
            Core::ToHexString(pUnknown[7], LeadingZeros));
    }
    return unknown;
}

String DumpString(File& file)
{
    size_t fileOffset = file.GetOffset();
    String unknown = file.ReadString();
    if (DumpLogEnabled)
        LOG("{}: {}\n", Core::ToHexString(fileOffset, LeadingZeros), unknown);
    return unknown;
}

int DumpFileOffset(File& file)
{
    size_t fileOffset = file.GetOffset();
    if (DumpLogEnabled)
        LOG("FileOffset: {}\n", Core::ToHexString(fileOffset, LeadingZeros));
    return fileOffset;
}
