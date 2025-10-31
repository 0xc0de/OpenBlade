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

#include <Hork/Core/UniqueRef.h>
#include <Hork/Core/Containers/Vector.h>
#include <Hork/Math/Plane.h>
#include <Hork/Geometry/BV/BvAxisAlignedBox.h>

using namespace Hk;

class BladeWorld
{
public:
    enum FACE_TYPE : int32_t
    {
        FT_OPAQUE           = 0x00001B59,    // 7001 Face without holes/portals
        FT_TRANSPARENT      = 0x00001B5A,    // 7002 Transparent wall (hole/portal)
        FT_SINGLE_PORTAL    = 0x00001B5B,    // 7003 Face with one hole/protal
        FT_MULTIPLE_PORTALS = 0x00001B5C,    // 7004 Face with several holes/protals and BSP nodes
        FT_SKYDOME          = 0x00001B5D     // 7005 Sky
    };

    enum NODE_TYPE : int32_t
    {
        NT_NODE             = 0x00001F41,    // 8001
        NT_TEXINFO          = 0x00001F42,    // 8002
        NT_LEAF             = 0x00001F43     // 8003
    };

    enum LIGHT_TYPE : int32_t
    {
        LT_POINT            = 15001,
        LT_DIRECTIONAL      = 15002
    };

    struct AtmosphereEntry
    {
        String              Name;
        byte                Color[3];
        float               Opacity;
    };

    struct LeafIndices
    {
        uint32_t            UnknownIndex;
        Vector<uint32_t>    Indices;
    };

    struct BSPNode
    {
        NODE_TYPE           Type;

        BSPNode*            Children[2];  // nullptr for leafs

        // Only for nodes
        int32_t             PlaneNum;

        // Only for NT_TEXINFO
        uint64_t            UnknownSignature;
        int32_t             TextureNum = 0;
        Double3             TexCoordAxis[2];
        float               TexCoordOffset[2];

        // Only for leafs
        Vector<LeafIndices> Unknown;
    };

    struct Face
    {
        FACE_TYPE           Type;
        int32_t             PlaneNum;
        uint64_t            UnknownSignature;
        int32_t             TextureNum = 0;
        Double3             TexCoordAxis[2];
        float               TexCoordOffset[2];

        Vector<uint32_t>            Winding;
        Vector<Vector<uint32_t>>    Holes;

        int32_t             SectorIndex;

        BSPNode*            pRoot = nullptr;
    };

    struct Portal
    {
        //int32_t           FaceNum;
        int32_t             ToSector;
        Vector<PlaneD>      TangentPlanes;
    };

    struct Sector
    {
        int32_t             AtmosphereNum;

        // TODO: We can use this for color grading or light manipulation within the sector
        uint8_t             AmbientColor[3];
        float               AmbientIntensity;
        float               AmbientUnknown;

        uint8_t             IlluminationColor[3];
        float               IlluminationIntensity;
        float               IlluminationUnknown;
        Double3             IlluminationVector;

        uint32_t            FirstFace;
        uint32_t            FaceCount;

        uint32_t            FirstPortal;
        uint32_t            PortalCount;

        int32_t             Group;
    };

    struct Light
    {
        LIGHT_TYPE          Type;

        // Common properties
        uint8_t             Color[3];
        float               Intensity;
        float               Unknown;

        // for LT_POINT
        Double3             Position;
        int32_t             Sector;

        // for LT_DIRECTIONAL
        Double3             Direction;
        Vector<int32_t>     Sectors;
    };

    bool                    Load(StringView name);

    void                    Clear();

private:
    bool                    ReadSector(File& file, uint32_t sectorIndex);
    void                    ReadFace(File& file, Face* face);
    void                    ReadOpaqueFace(File& file, Face* face);
    void                    ReadTransparentFace(File& file, Face* face);
    void                    ReadSinglePortalFace(File& file, Face* face);
    void                    ReadMultiplePortalsFace(File& file, Face* face);
    void                    ReadSkydomeFace(File& file, Face* face);
    BSPNode*                ReadBSPNode_r(File& file, Face* face);
    int32_t                 ReadPlane(File& file);
    int32_t                 ReadTextureName(File& file);
    void                    ReadIndices(File& file, Vector<uint32_t>& indices);

public:
    Vector<AtmosphereEntry>     m_Atmospheres;
    Vector<Double3>             m_Vertices;
    Vector<Sector>              m_Sectors;
    Vector<Face>                m_Faces;
    Vector<Portal>              m_Portals;
    Vector<UniqueRef<BSPNode>>  m_BSPNodes;
    Vector<PlaneD>              m_Planes;
    Vector<String>              m_TextureNames;
    Vector<Light>               m_Lights;
};
