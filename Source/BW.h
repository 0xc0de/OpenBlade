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

#include <Hork/Runtime/World/World.h>
#include <Hork/Geometry/BV/BvAxisAlignedBox.h>
#include <Hork/Math/Plane.h>

using namespace Hk;

class BladeWorld
{
public:
    enum FACE_TYPE
    {
        FT_OPAQUE           = 0x00001B59,    // 7001 Face without holes/portals
        FT_TRANSPARENT      = 0x00001B5A,    // 7002 Transparent wall (hole/portal)
        FT_SINGLE_PORTAL    = 0x00001B5B,    // 7003 Face with one hole/protal
        FT_MULTIPLE_PORTALS = 0x00001B5C,    // 7004 Face with several holes/protals and BSP nodes
        FT_SKYDOME          = 0x00001B5D     // 7005 Sky
    };

    enum NODE_TYPE
    {
        NT_NODE       = 0x00001F41,    // 8001
        NT_TEXINFO    = 0x00001F42,    // 8002
        NT_LEAF       = 0x00001F43     // 8003
    };

    struct AtmosphereEntry
    {
        String  Name;
        byte    Color[3];
        float   Intensity;
    };

    struct LeafIndices
    {
        uint32_t UnknownIndex;
        Vector<uint32_t> Indices;
    };

    struct BSPNode
    {
        int32_t Type;

        BSPNode* Children[2];  // nullptr for leafs

        // Only for nodes
        int   PlaneNum;

        // Only for NT_TEXINFO
        uint64_t UnknownSignature;
        int TextureNum = 0;
        Double3 TexCoordAxis[2];
        float TexCoordOffset[2];

        // Only for leafs
        Vector<LeafIndices> Unknown;

        // Leaf triangles
        //Vector<Double3> Vertices;
        //Vector<unsigned int> Indices;
    };

    struct Face
    {
        int Type;
        int PlaneNum;
        uint64_t UnknownSignature;
        int TextureNum = 0;
        Double3 TexCoordAxis[2];
        float TexCoordOffset[2];
        //bool CastShadows;

        Vector<uint32_t> Winding;
        Vector<Vector<uint32_t>> Holes;

        // result mesh
        //Vector<Double3> Vertices;
        //Vector<unsigned int> Indices;

        int SectorIndex;

        //Vector<Face*> SubFaces;

        BSPNode* pRoot = nullptr;
    };

    struct Portal
    {
        //Face* pFace;
        int32_t ToSector;
        //Vector<Double3> Winding;

        // Some planes. What they mean?
        Vector<PlaneD> Planes;

        //bool Marked;
    };

    struct Sector
    {
        // TODO: We can use this for color grading or light manipulation within the sector
        byte AmbientColor[3];
        float AmbientIntensity;

        Float3 LightDir;

        uint32_t FirstFace;
        uint32_t FaceCount;

        Vector<int> Portals;

        //BvAxisAlignedBox Bounds;
        //Float3 Centroid;
    };

public:
    bool Load(StringView name);

private:
    bool ReadSector(File& file, uint32_t sectorIndex);
    BSPNode* AllocateBSPNode();
    void ReadFace(File& file, Face* face);
    void ReadOpaqueFace(File& file, Face* face);
    void ReadTransparentFace(File& file, Face* face);
    void ReadSinglePortalFace(File& file, Face* face);
    void ReadMultiplePortalsFace(File& file, Face* face);
    void ReadSkydomeFace(File& file, Face* face);
    BSPNode* ReadBSPNode_r(File& file, Face* face);
    int ReadPlane(File& file);
    int ReadTextureName(File& file);
    void ReadIndices(File& file, Vector<uint32_t>& indices);

public:
    Vector<AtmosphereEntry> m_Atmospheres;
    Vector<Double3> m_Vertices;    
    Vector<Sector> m_Sectors;
    Vector<Face> m_Faces;
    Vector<Portal> m_Portals;
    Vector<UniqueRef<BSPNode>> m_BSPNodes;
    Vector<PlaneD> m_Planes;
    Vector<String> m_TextureNames;
};
