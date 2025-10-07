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

#include "BW.h"
#include "FileDump.h"

using namespace Hk;

bool BladeWorld::Load(StringView fileName)
{
    Clear();

    File file = File::sOpenRead(fileName);
    if (!file)
        return false;

    m_Atmospheres.Resize(file.ReadInt32());
    for (auto& atmo : m_Atmospheres)
    {
        atmo.Name = file.ReadString();
        file.Read(&atmo.Color[0], 3);
        atmo.Opacity = file.ReadFloat();
    }

    m_Vertices.Resize(file.ReadInt32());
    for (auto& vertex : m_Vertices)
    {
        vertex.X = file.ReadDouble();
        vertex.Y = file.ReadDouble();
        vertex.Z = file.ReadDouble();
    }

    m_Sectors.Resize(file.ReadInt32());

    for (uint32_t sectorIndex = 0; sectorIndex < m_Sectors.Size(); ++sectorIndex)
    {
        if (!ReadSector(file, sectorIndex))
        {
            m_Sectors.Resize(sectorIndex);
            break;
        }
    }

    m_Lights.Resize(file.ReadInt32());
    for (auto& light : m_Lights)
    {
        light.Type = static_cast<LIGHT_TYPE>(file.ReadInt32());

        file.Read(light.Color, 3);
        light.Intensity = file.ReadFloat();
        light.Unknown = file.ReadFloat();

        switch (light.Type)
        {
            case LT_POINT:
                light.Position = file.ReadObject<Double3>();
                light.Sector = file.ReadInt32();
                break;
            case LT_DIRECTIONAL:
                file.SeekCur(36);
                light.Direction = file.ReadObject<Double3>();
                light.Sectors.Resize(file.ReadInt32());
                for (int32_t& sectorIndex : light.Sectors)
                    sectorIndex = file.ReadInt32();
                break;
            default:
                HK_ASSERT(0);
                break;
        }
    }

    file.ReadObject<Double3>();
    file.ReadObject<Double3>();

    for (auto& sector : m_Sectors)
        sector.Group = file.ReadInt32();

    SetDumpLog(false);
    int32_t strCount = file.ReadInt32();
    for (int32_t i = 0; i < strCount; ++i)
        DumpString(file);
    SetDumpLog(true);

    return true;
}

void BladeWorld::Clear()
{
    m_Atmospheres.Clear();
    m_Vertices.Clear();
    m_Sectors.Clear();
    m_Faces.Clear();
    m_Portals.Clear();
    m_BSPNodes.Clear();
    m_Planes.Clear();
    m_TextureNames.Clear();
    m_Lights.Clear();
}

bool BladeWorld::ReadSector(File& file, uint32_t sectorIndex)
{
    Sector& sector = m_Sectors[sectorIndex];

    SetDumpLog(true);

    //LOG("-------Sector-------\n");
    //DumpFileOffset(file);

    sector.AtmosphereNum = [this](File& file) -> int32_t
        {
            String name = file.ReadString();
            for (int32_t i = 0, count = m_Atmospheres.Size(); i < count; ++i)
            {
                if (!m_Atmospheres[i].Name.Icmp(name))
                    return i;
            }
            return -1;
        }(file);

    file.Read(sector.AmbientColor, 3);
    sector.AmbientIntensity = file.ReadFloat();
    sector.AmbientUnknown = file.ReadFloat();

    SetDumpLog(false);

    for (int i = 0; i < 24; ++i) { if (DumpByte(file) != 0) LOG("not zero\n"); }
    for (int i = 0; i < 8; ++i) { if (DumpByte(file) != 0xCD) LOG("not CD\n"); }
    for (int i = 0; i < 4; ++i) { if (DumpByte(file) != 0) LOG("not zero\n"); }

    file.Read(sector.IlluminationColor, 3);
    sector.IlluminationIntensity = file.ReadFloat();
    sector.IllumintationUnknown = file.ReadFloat();

    for (int i = 0; i < 24; ++i) { if (DumpByte(file) != 0) LOG("not zero\n"); }
    for (int i = 0; i < 8; ++i) { if (DumpByte(file) != 0xCD) LOG("not CD\n"); }
    for (int i = 0; i < 4; ++i) { if (DumpByte(file) != 0) LOG("not zero\n"); }

    // Light direction?
    sector.IllumintationVector = file.ReadObject<Double3>();

    int32_t faceCount = file.ReadInt32();
    if (faceCount < 4 || faceCount > 100)
    {
        LOG("WARNING: FILE READ ERROR.. SOMETHING GO WRONG!\n");
        HK_ASSERT(0);
        return false;
    }

    sector.FirstFace = m_Faces.Size();
    sector.FaceCount = faceCount;

    sector.FirstPortal = m_Portals.Size();

    SetDumpLog(true);

    for (int32_t faceIndex = 0; faceIndex < faceCount; ++faceIndex)
    {
        Face& face = m_Faces.EmplaceBack();
        face.SectorIndex = sectorIndex;
        ReadFace(file, &face);
    }

    sector.PortalCount = m_Portals.Size() - sector.FirstPortal;

    return true;
}

void BladeWorld::ReadFace(File& file, Face* face)
{
    face->Type = static_cast<FACE_TYPE>(file.ReadInt32());

    switch (face->Type)
    {
        case FT_OPAQUE:
            ReadOpaqueFace(file, face);
            break;
        case FT_TRANSPARENT:
            ReadTransparentFace(file, face);
            break;
        case FT_SINGLE_PORTAL:
            ReadSinglePortalFace(file, face);
            break;
        case FT_MULTIPLE_PORTALS:
            ReadMultiplePortalsFace(file, face);
            break;
        case FT_SKYDOME:
            ReadSkydomeFace(file, face);
            break;
        default:
            HK_ASSERT(0);
            break;
    }
}

int32_t BladeWorld::ReadPlane(File& file)
{
    PlaneD plane = file.ReadObject<PlaneD>();

    m_Planes.Add(plane);
    return m_Planes.Size() - 1;
}

int32_t BladeWorld::ReadTextureName(File& file)
{
    String name = file.ReadString();

    for (int32_t texNum = 0; texNum < m_TextureNames.Size(); ++texNum) {
        if (!m_TextureNames[texNum].Icmp(name))
            return texNum;
    }

    m_TextureNames.Add(std::move(name));
    return m_TextureNames.Size() - 1;
}

void BladeWorld::ReadIndices(File& file, Vector<uint32_t>& indices)
{
    indices.Resize(file.ReadInt32());
    for (auto& index : indices)
        index = file.ReadUInt32();
}

void BladeWorld::ReadOpaqueFace(File& file, Face* face)
{
    face->PlaneNum = ReadPlane(file);

    face->UnknownSignature = file.ReadUInt64();
    if (face->UnknownSignature != 3)
        LOG("Face signature {}\n", face->UnknownSignature);

    face->TextureNum = ReadTextureName(file);
    face->TexCoordAxis[0] = file.ReadObject<Double3>();
    face->TexCoordAxis[1] = file.ReadObject<Double3>();
    face->TexCoordOffset[0] = file.ReadFloat();
    face->TexCoordOffset[1] = file.ReadFloat();

    // 8 zero bytes?
    SetDumpLog(false);
    for (int i = 0; i < 8; ++i) { if (DumpByte(file) != 0) LOG("not zero!\n"); }
    SetDumpLog(true);

    ReadIndices(file, face->Winding);
}

void BladeWorld::ReadTransparentFace(File& file, Face* face)
{
    face->PlaneNum = ReadPlane(file);

    ReadIndices(file, face->Winding);

    Portal& portal = m_Portals.EmplaceBack();

    //portal.FaceNum = face;
    portal.ToSector = file.ReadInt32();

    face->UnknownSignature = file.ReadUInt64();
    if (face->UnknownSignature != 3)
        LOG("Face signature {}\n", face->UnknownSignature);

    // FIXME: wtf portal has texture properties? Is it doors?

    face->TextureNum = ReadTextureName(file);
    face->TexCoordAxis[0] = file.ReadObject<Double3>();
    face->TexCoordAxis[1] = file.ReadObject<Double3>();
    face->TexCoordOffset[0] = file.ReadFloat();
    face->TexCoordOffset[1] = file.ReadFloat();

    // 8 zero bytes?
    SetDumpLog(false);
    for (int i = 0; i < 8; ++i) { if (DumpByte(file) != 0) LOG("not zero!\n"); }
    SetDumpLog(true);
}

void BladeWorld::ReadSinglePortalFace(File& file, Face* face)
{
    face->PlaneNum = ReadPlane(file);

    face->UnknownSignature = file.ReadUInt64();
    if (face->UnknownSignature != 3)
        LOG("Face signature {}\n", face->UnknownSignature);

    face->TextureNum = ReadTextureName(file);
    face->TexCoordAxis[0] = file.ReadObject<Double3>();
    face->TexCoordAxis[1] = file.ReadObject<Double3>();
    face->TexCoordOffset[0] = file.ReadFloat();
    face->TexCoordOffset[1] = file.ReadFloat();

    // 8 zero bytes?
    SetDumpLog(false);
    for (int i = 0; i < 8; ++i) { if (DumpByte(file) != 0) LOG("not zero!\n"); }
    SetDumpLog(true);

    // Winding
    ReadIndices(file, face->Winding);

    // Winding hole
    face->Holes.Resize(1);
    ReadIndices(file, face->Holes[0]);

    Portal& portal = m_Portals.EmplaceBack();

    //portal.FaceNum = face;
    portal.ToSector = file.ReadInt32();

    int32_t count = file.ReadInt32();
    portal.TangentPlanes.Resize(count);
    for (auto& tangentPlane : portal.TangentPlanes)
        tangentPlane = file.ReadObject<PlaneD>();
}

void BladeWorld::ReadMultiplePortalsFace(File& file, Face* face)
{
    face->PlaneNum = ReadPlane(file);

    face->UnknownSignature = file.ReadUInt64();
    if (face->UnknownSignature != 3)
        LOG("Face signature {}\n", face->UnknownSignature);

    face->TextureNum = ReadTextureName(file);
    face->TexCoordAxis[0] = file.ReadObject<Double3>();
    face->TexCoordAxis[1] = file.ReadObject<Double3>();
    face->TexCoordOffset[0] = file.ReadFloat();
    face->TexCoordOffset[1] = file.ReadFloat();

    // 8 zero bytes?
    SetDumpLog(false);
    for (int i = 0; i < 8; ++i) { if (DumpByte(file) != 0) LOG("not zero!\n"); }
    SetDumpLog(true);

    // Winding
    ReadIndices(file, face->Winding);

    face->Holes.Resize(file.ReadInt32());
    for (auto& hole : face->Holes)
    {
        ReadIndices(file, hole);

        Portal& portal = m_Portals.EmplaceBack();

        //portal.FaceNum = face;
        portal.ToSector = file.ReadInt32();

        portal.TangentPlanes.Resize(file.ReadInt32());
        for (auto& tangentPlane : portal.TangentPlanes)
            tangentPlane = file.ReadObject<PlaneD>();
    }

    face->pRoot = ReadBSPNode_r(file, face);
}

BladeWorld::BSPNode* BladeWorld::ReadBSPNode_r(File& file, Face* face)
{
    BSPNode* node = m_BSPNodes.EmplaceBack(MakeUnique<BSPNode>()).RawPtr();

    node->Type = static_cast<NODE_TYPE>(file.ReadInt32());

    if (node->Type == NT_LEAF)
    {
        node->Children[0] = nullptr;
        node->Children[1] = nullptr;

        node->Unknown.Resize(file.ReadInt32());
        for (LeafIndices& unknown : node->Unknown)
        {
            unknown.UnknownIndex = file.ReadInt32();

            unknown.Indices.Resize(file.ReadInt32());
            for (uint32_t& index : unknown.Indices)
                index = file.ReadInt32();
        }

        return node;
    }

    node->Children[0] = ReadBSPNode_r(file, face);
    node->Children[1] = ReadBSPNode_r(file, face);

    node->PlaneNum = ReadPlane(file);

    if (node->Type == NT_TEXINFO)
    {
        node->UnknownSignature = file.ReadUInt64();

        node->TextureNum = ReadTextureName(file);
        node->TexCoordAxis[0] = file.ReadObject<Double3>();
        node->TexCoordAxis[1] = file.ReadObject<Double3>();
        node->TexCoordOffset[0] = file.ReadFloat();
        node->TexCoordOffset[1] = file.ReadFloat();

        // 8 zero bytes?
        SetDumpLog(false);
        for (int i = 0; i < 8; ++i) { if (DumpByte(file) != 0) LOG("not zero!\n"); }
        SetDumpLog(true);
    }

    return node;
}

void BladeWorld::ReadSkydomeFace(File& file, Face* face)
{
    face->PlaneNum = ReadPlane(file);

    ReadIndices(file, face->Winding);
}
