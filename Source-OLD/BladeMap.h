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

#include <Engine/Core/Public/String.h>
#include <Engine/Core/Public/Array.h>

// Blade .MP file loader

struct FBladeMap {
    struct FTextureEntry {
        FString Name;
        FString Path;
    };

    struct FAtmosphereEntry {
        FString Name;

        // TODO: we can use this for directional light
        byte Color[ 3 ];
        float Intensity;
    };

    struct FMapLimits {
        double Left, Right, Upper, Lower;
    };

    struct FVertex {
        double X, Y;
    };

    struct FWall {
        byte Color[ 4 ];
        int32_t IncludeFloor;
        int32_t IncludeCeil;
        FString TextureName;
        float Scale;
        float CornerOfTurn;
        float X;
        float Y;
    };

    enum EFloorType {
        FT_Elliptic = 0,
        FT_Spherical = 1,
        FT_Flat = 2
    };

    struct FSectorEntry {
        FString Name;

        double FloorHeight0;
        FString FloorTexture;
        float FloorScale;
        float FloorCornerOfTurn;
        float FloorX;
        float FloorY;
        double FloorHeight;
        byte FloorType;
        double FloorSphericalRadius;
        int32_t FloorSegmentsCount;
        double FloorEllipseXParameter;
        double FloorEllipseYParameter;

        double CeilHeight0;
        FString CeilTexture;
        float CeilScale;
        float CeilCornerOfTurn;
        float CeilX;
        float CeilY;
        double CeilHeight;
        byte CeilType;
        double CeilSphericalRadius;
        int32_t CeilSegmentsCount;
        double CeilEllipseXParameter;
        double CeilEllipseYParameter;

        // Illumination
        byte EnvironmentIllum[ 3 ];
        byte InternalIllum[ 3 ];
        byte ExternalIllum[ 3 ];
        byte FinalEnvironmentIllum[ 3 ];
        byte FinalInternalIllum[ 3 ];

        double InternalIllumCoord[ 3 ];
        double ExternalIllumCoord[ 3 ];

        FString AtmosphereName;

        FString AttributeName;

        TArray< FVertex > Vertices;
        TArray< FWall > Walls;
    };

    TArray< FTextureEntry > Textures;
    TArray< FAtmosphereEntry > Atmospheres;
    TArray< FSectorEntry > Sectors;

    void LoadMap( const char * _FileName );
    void FreeMap();
};
