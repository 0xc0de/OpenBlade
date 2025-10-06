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
    File file = File::sOpenRead(fileName);
    if (!file)
        return false;

    m_Atmospheres.Resize(file.ReadInt32());
    for (auto& atmo : m_Atmospheres)
    {
        atmo.Name = file.ReadString();
        file.Read(&atmo.Color[0], 3);
        atmo.Intensity = file.ReadFloat();
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
    return true;
}

bool BladeWorld::ReadSector(File& file, uint32_t sectorIndex)
{
    Sector& sector = m_Sectors[sectorIndex];

    SetDumpLog(true);

    DumpFileOffset(file);

    String unknownName = file.ReadString();

    LOG("Loading sector: {}\n", unknownName);

    file.Read(&sector.AmbientColor[0], 3);
    sector.AmbientIntensity = file.ReadFloat();

    SetDumpLog(false);

    DumpFloat(file);

    for (int i = 0; i < 24; i++) {
        if (DumpByte(file) != 0) {
            LOG("not zero\n");
        }
    }

    for (int i = 0; i < 8; i++) {
        if (DumpByte(file) != 0xCD) {
            LOG("not CD\n");
        }
    }

    for (int i = 0; i < 4; i++) {
        if (DumpByte(file) != 0) {
            LOG("not zero\n");
        }
    }

    byte r, g, b;
    r = DumpByte(file);
    g = DumpByte(file);
    b = DumpByte(file);

    LOG("RGB: {} {} {}\n", r, g, b);

    DumpFloat(file);
    DumpFloat(file);

    for (int i = 0; i < 24; i++)
    {
        if (DumpByte(file) != 0)
            LOG("not zero\n");
    }

    for (int i = 0; i < 8; i++)
    {
        if (DumpByte(file) != 0xCD)
            LOG("not CD\n");
    }

    for (int i = 0; i < 4; i++)
    {
        if (DumpByte(file) != 0)
            LOG("not zero\n");
    }

    // Light direction?
    sector.LightDir.X = DumpDouble(file);
    sector.LightDir.Y = -DumpDouble(file);
    sector.LightDir.Z = -DumpDouble(file);
    sector.LightDir.NormalizeSelf();

    SetDumpLog(true);

    int32_t faceCount = file.ReadInt32();

    if (faceCount < 4 || faceCount > 100)
    {
        LOG("WARNING: FILE READ ERROR.. SOMETHING GO WRONG!\n");
        HK_ASSERT(0);
        return false;
    }

    sector.FirstFace = m_Faces.Size();
    sector.FaceCount = faceCount;

    for (int faceIndex = 0; faceIndex < faceCount; ++faceIndex)
    {
        Face& face = m_Faces.EmplaceBack();
        face.SectorIndex = sectorIndex;
        ReadFace(file, &face);
    }

    return true;
}

BladeWorld::BSPNode* BladeWorld::AllocateBSPNode()
{
    return m_BSPNodes.EmplaceBack(MakeUnique<BSPNode>()).RawPtr();
}

void BladeWorld::ReadFace(File& file, Face* face)
{
    face->Type = file.ReadInt32();

    switch (face->Type)
    {
        case FT_OPAQUE:
            LOG("OpaqueFace\n");
            ReadOpaqueFace(file, face);
            break;
        case FT_TRANSPARENT:
            LOG("Transparent\n");
            ReadTransparentFace(file, face);
            break;
        case FT_SINGLE_PORTAL:
            LOG("SinglePortal\n");
            ReadSinglePortalFace(file, face);
            break;
        case FT_MULTIPLE_PORTALS:
            LOG("MultiplePortals\n");
            ReadMultiplePortalsFace(file, face);
            break;
        case FT_SKYDOME:
            LOG("Skydome\n");
            ReadSkydomeFace(file, face);
            break;
        default:
            HK_ASSERT(0);
            break;
    }
}

int BladeWorld::ReadPlane(File& file)
{
    PlaneD plane = file.ReadObject<PlaneD>();

    m_Planes.Add(plane); // TODO: Add if unique
    return m_Planes.Size() - 1;
}

int BladeWorld::ReadTextureName(File& file)
{
    String name = file.ReadString();

    for (int texNum = 0; texNum < m_TextureNames.Size(); ++texNum) {
        if (!m_TextureNames[texNum].Icmp(name))
            return texNum;
    }

    m_TextureNames.Add(std::move(name));
    return m_TextureNames.Size() - 1;
}

void BladeWorld::ReadIndices(File& file, Vector< unsigned int >& indices)
{
    indices.Resize(file.ReadInt32());
    for (auto& index : indices)
        index = file.ReadUInt32();
}

void BladeWorld::ReadOpaqueFace(File& file, Face* face)
{
    // Face plane
    face->PlaneNum = ReadPlane(file);

    // FIXME: What is it?
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
    for (int k = 0; k < 8; k++)
    {
        if (DumpByte(file) != 0)
            LOG("not zero!\n");
    }
    SetDumpLog(true);

    // Winding
    ReadIndices(file, face->Winding);
}

void BladeWorld::ReadTransparentFace(File& file, Face* face)
{
    // Face plane
    face->PlaneNum = ReadPlane(file);

    // Winding
    ReadIndices(file, face->Winding);

    int portalNum = m_Portals.Size();
    Portal& portal = m_Portals.EmplaceBack();

    //portal.pFace = face;
    portal.ToSector = file.ReadInt32();
    //portal->Winding.Resize(face->Indices.Size());
    //for (int k = 0; k < face->Indices.Size(); k++) {
    //    portal->Winding[face->Indices.Size() - k - 1] = m_Vertices[face->Indices[k]];
    //}

    m_Sectors[face->SectorIndex].Portals.Add(portalNum);

    // FIXME: What is it?
    SetDumpLog(false);
    DumpUnknownBytes(file, 8);
    SetDumpLog(true);

    // FIXME: wtf portal has texture properties? Is this doors?

    face->TextureNum = ReadTextureName(file);
    face->TexCoordAxis[0] = file.ReadObject<Double3>();
    face->TexCoordAxis[1] = file.ReadObject<Double3>();
    face->TexCoordOffset[0] = file.ReadFloat();
    face->TexCoordOffset[1] = file.ReadFloat();

    // 8 zero bytes?
    SetDumpLog(false);
    for (int k = 0; k < 8; k++)
    {
        if (DumpByte(file) != 0)
            LOG("not zero!\n");
    }
    SetDumpLog(true);
}

void BladeWorld::ReadSinglePortalFace(File& file, Face* face)
{
    // Face plane
    face->PlaneNum = ReadPlane(file);

    // FIXME: What is it?
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
    for (int k = 0; k < 8; k++)
    {
        if (DumpByte(file) != 0)
            LOG("not zero!\n");
    }
    SetDumpLog(true);

    // Winding
    ReadIndices(file, face->Winding);

    // Winding hole
    face->Holes.Resize(1);
    ReadIndices(file, face->Holes[0]);

    int portalNum = m_Portals.Size();
    Portal& portal = m_Portals.EmplaceBack();

    //portal.pFace = face;
    portal.ToSector = file.ReadInt32();
    //portal->Winding = hole;

    m_Sectors[face->SectorIndex].Portals.Add(portalNum);

    int32_t count = file.ReadInt32();
    portal.Planes.Resize(count);
    for (auto& portalPlane : portal.Planes)
        portalPlane = file.ReadObject<PlaneD>();
}

void BladeWorld::ReadMultiplePortalsFace(File& file, Face* face)
{
    // Face plane
    face->PlaneNum = ReadPlane(file);

    // FIXME: What is it?
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
    for (int k = 0; k < 8; k++)
    {
        if (DumpByte(file) != 0)
            LOG("not zero!\n");
    }
    SetDumpLog(true);

    // Winding
    ReadIndices(file, face->Winding);

    //Vector<Double3> winding;

    //winding.Resize(face->Indices.Size());
    //for (int k = 0; k < face->Indices.Size(); k++)
    //    winding[k] = m_Vertices[face->Indices[k]];
    //winding.Reverse();

    //PlaneD facePlane = m_Planes[face->PlaneNum];

    //Vector<ClipperContour> Holes;

    int numHoles = file.ReadInt32();
    if (numHoles > 0)
    {
        //PolyClipper HolesUnion;
        //Vector<Double3> Hole;

        //HolesUnion.SetTransformFromNormal(Float3(facePlane.Normal));

        face->Holes.Resize(numHoles);

        for (int c = 0; c < numHoles; c++)
        {
            ReadIndices(file, face->Holes[c]);

            //HolesUnion.AddSubj3D(Hole.ToPtr(), Hole.Size());

            int portalNum = m_Portals.Size();
            Portal& portal = m_Portals.EmplaceBack();

            //portal.pFace = face;
            portal.ToSector = file.ReadInt32();
            //portal->Winding = Hole;

            m_Sectors[face->SectorIndex].Portals.Add(portalNum);

            int32_t Count = file.ReadInt32();
            portal.Planes.Resize(Count);
            for (int k = 0; k < Count; k++)
                portal.Planes[k] = file.ReadObject<PlaneD>();
        }

        // FIXME: Union of hulls may produce inner holes!
        //HolesUnion.MakeUnion(Holes);
    }

    face->pRoot = ReadBSPNode_r(file, face);
}

BladeWorld::BSPNode* BladeWorld::ReadBSPNode_r(File& file, Face* face)
{
    BSPNode* node = AllocateBSPNode();

    node->Type = file.ReadInt32();

    if (node->Type == NT_LEAF)
    {
        node->Children[0] = nullptr;
        node->Children[1] = nullptr;

        int Count = DumpInt(file);

        node->Unknown.Resize(Count);

        for (int i = 0; i < Count; i++)
        {
            LeafIndices& Unknown = node->Unknown[i];

            Unknown.UnknownIndex = DumpInt(file);

            uint32_t IndicesCount = DumpInt(file);
            Unknown.Indices.Resize(IndicesCount);
            for (int j = 0; j < IndicesCount; j++)
                Unknown.Indices[j] = DumpInt(file);
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
        for (int k = 0; k < 8; k++)
        {
            if (DumpByte(file) != 0)
                LOG("not zero!\n");
        }
    }

    return node;
}

void BladeWorld::ReadSkydomeFace(File& file, Face* face)
{
    // Face plane
    face->PlaneNum = ReadPlane(file);

    // Winding
    ReadIndices(file, face->Winding);
}
