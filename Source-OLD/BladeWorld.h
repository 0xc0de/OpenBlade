/*

Blade Of Darkness Remake GPL Source Code

Copyright (C) 2017 Alexander Samusev.

This file is part of the Blade Of Darkness Remake GPL Source Code (BladeRemake Source Code).  

BladeRemake is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#pragma once

#include "BladeMap.h"

#include <Engine/Utilites/Public/Polygon.h>
#include <Engine/Utilites/Public/PolygonClipper.h>
#include <Engine/Renderer/Public/StaticMeshResource.h>

// Blade .BW file loader

extern const Double3 BLADE_COORD_SCALE_D;
extern const Float3 BLADE_COORD_SCALE_F;

struct FBladeWorld {
    enum EFaceType {
        FT_SimpleFace = 0x00001B59,    // 7001 Face without holes/portals
        FT_Portal     = 0x00001B5A,    // 7002 Transparent wall (hole/portal)
        FT_Face       = 0x00001B5B,    // 7003 Face with one hole/protal
        FT_FaceBSP    = 0x00001B5C,    // 7004 Face with several holes/protals and BSP nodes
        FT_Skydome    = 0x00001B5D,    // 7005 Sky
        FT_Subface    = 0xffffffff     // Used to mark subfaces
    };

    enum ENodeType {
        NT_Node       = 0x00001F41,    // 8001
        NT_TexInfo    = 0x00001F42,    // 8002
        NT_Leaf       = 0x00001F43,    // 8003
    };

    struct FLeafIndices {
        uint32_t UnknownIndex;
        TPodArray< uint32_t > Indices;
    };

    struct FBSPNode {
        ENodeType Type;

        FBSPNode * Children[ 2 ];  // NULL for leafs

        // Only for nodes
        PlaneD   Plane;

        // Only for NT_TexInfo
        uint64_t UnknownSignature;
        FString TextureName;
        Double3 TexCoordAxis[ 2 ];
        float TexCoordOffset[ 2 ];

        // Only for leafs
        TArray< FLeafIndices > Unknown;

        // Leaf triangles
        TPodArray< Double3 > Vertices;
        TPodArray< unsigned int > Indices;
    };

    struct FFace {
        int Type;
        PlaneD Plane;
        uint64_t UnknownSignature;    // Только для фейсов с текстурой
        FString TextureName;        // Только для фейсов с текстурой
        Double3 TexCoordAxis[2];     // Только для фейсов с текстурой
        float TexCoordOffset[2];    // Только для фейсов с текстурой
        bool CastShadows;

        // result mesh
        TPodArray< Double3 > Vertices;
        TPodArray< unsigned int > Indices;

        int SectorIndex;

        TArray< FFace * > SubFaces;

        FBSPNode * Root;
    };

    struct FPortal {
        FFace * Face;
        int32_t ToSector;
        PolygonD Winding;

        // Some planes. What they mean?
        TPodArray< PlaneD > Planes;

        bool Marked;
    };

    struct FSector {
        // TODO: We can use this for color grading or light manipulation within the sector
        byte AmbientColor[ 3 ];
        float AmbientIntensity;

        Float3 LightDir;

        TArray< FFace * > Faces;
        TArray< FPortal * > Portals;

        BvAxisAlignedBox Bounds;
        Float3 Centroid;
    };

    TArray< FBladeMap::FAtmosphereEntry > Atmospheres;
    TArray< Double3 > Vertices;
    TArray< FSector > Sectors;
    TArray< FMeshOffset > MeshOffsets;
    TArray< FMeshVertex > MeshVertices;
    TArray< unsigned int > MeshIndices;
    TArray< FFace * > MeshFaces;
    FMeshOffset ShadowCasterMeshOffset;

    TPodArray< FPortal * > Portals;
    TPodArray< FFace * > Faces;
    TPodArray< FBSPNode * > BSPNodes;
    TPodArray< FBSPNode * > Leafs; // Temporary used on loading

    BvAxisAlignedBox Bounds;
    bool HasSky;

    ~FBladeWorld();

    void LoadWorld( const char * _FileName );
    void FreeWorld();

private:
    FFace * CreateFace();
    FPortal * CreatePortal();
    FBSPNode * CreateBSPNode();

    void LoadFace( FFace * _Face );
    void LoadSimpleFace( FFace * _Face );
    void LoadPortalFace( FFace * _Face );
    void LoadFaceWithHole( FFace * _Face );
    void LoadFaceBSP( FFace * _Face );
    void LoadSkydomeFace( FFace * _Face );
    void ReadIndices( TPodArray< unsigned int > & _Indices );
    void ReadWinding( PolygonD & _Winding );
    FBSPNode * ReadBSPNode_r( FFace * _Face );
    void CreateWindings_r( FBladeWorld::FFace * _Face, const TArray< FClipperContour > & _Holes, PolygonD * _Winding, FBSPNode * _Node );
    void FilterWinding_r( FBladeWorld::FFace * _Face, FBSPNode * _Node, FBSPNode * _Leaf );
    void WorldGeometryPostProcess();

    FFileAbstract * File;
};

extern FBladeWorld World;
