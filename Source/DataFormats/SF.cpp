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

#include "SF.h"
#include <Hork/Core/IO.h>

using namespace Hk;

void BladeSF::Load(StringView fileName)
{
    char str[256];
    char property[256];
    char value[256];

    GhostSectors.Clear();

    File f = File::sOpenRead(fileName);
    if (!f)
        return;

    if (!f.Gets(str, sizeof(str) - 1))
        return;

    int numSectors = 0;
    sscanf(str, "%s => %d", property, &numSectors);
    if (!Core::Stricmp(property, "NumGhostSectors"))
        GhostSectors.Reserve(numSectors);

    StringView fileLocation = PathUtils::sGetFilePath(fileName);
    bool beginSector = false;

    while (f.Gets(str, sizeof(str) - 1))
    {
        property[0] = 0;
        value[0] = 0;

        sscanf(str, "%s", property);

        if (!property[0])
            continue;

        if (!Core::Stricmp(property, "BeginGhostSector"))
        {
            if (beginSector)
            {
                LOG("BladeSF::Load: Unexpected begin of the sector\n");
                continue;
            }

            GhostSectors.EmplaceBack();

            beginSector = true;
            continue;
        }
        if (!Core::Stricmp(property, "EndGhostSector"))
        {
            if (!beginSector)
            {
                LOG("BladeSF::Load: Unexpected end of the sector\n");
            }

            beginSector = false;
            continue;
        }

        if (!beginSector)
        {
            // ignore all stuff outside of sector section
            continue;
        }

        if (sscanf(str, "%s => %s", property, value) != 2 || !property[0] || !value[0])
            continue;

        auto& sector = GhostSectors.Last();

        if (!Core::Stricmp(property, "Name"))
        {
            sector.Name = value;
        }
        else if (!Core::Stricmp(property, "FloorHeight"))
        {
            sector.FloorHeight = atof(value);
        }
        else if ( !Core::Stricmp(property, "RoofHeight"))
        {
            sector.RoofHeight = atof(value);
        }
        else if (!Core::Stricmp(property, "Vertex"))
        {
            Float2 v;
            if (sscanf(str, "%s => %f %f", value, &v.X, &v.Y) == 3)
                sector.Vertices.Add(v);
        }
        else if (!Core::Stricmp(property, "Grupo"))
        {
            sector.Group = value;
        }
        else if (!Core::Stricmp(property, "Sonido"))
        {
            sector.Sound = fileLocation  / value;
            PathUtils::sFixPathInplace(sector.Sound);
        }
        else if (!Core::Stricmp(property, "Volumen"))
        {
            sector.Volume = atof(value);
        }
        else if (!Core::Stricmp(property, "VolumenBase"))
        {
            sector.VolumeBase = atof(value);
        }
        else if (!Core::Stricmp(property, "DistanciaMinima"))
        {
            sector.MinDist = atof(value);
        }
        else if (!Core::Stricmp(property, "DistanciaMaxima"))
        {
            sector.MaxDist = atof(value);
        }
        else if (!Core::Stricmp(property, "DistMaximaVertical"))
        {
            sector.MaxVerticalDist = atof(value);
        }
        else if (!Core::Stricmp(property, "Escala"))
        {
            sector.Scale = atof(value);
        }
        else
            LOG("BladeSF::Load: Unknown property {}\n", property);
    }
}
