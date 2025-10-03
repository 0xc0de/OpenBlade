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

#include "BladeMap.h"

#include <Engine/IO/Public/FileUrl.h>

void FBladeMap::LoadMap( const char * _FileName ) {
    FFileAbstract * File = FFiles::OpenFileFromUrl( _FileName, FFileAbstract::M_Read );
    if ( !File ) {
        return;
    }

    int FileOffset;
    FString MapaBlade;
    byte Unknown;
    File->ReadString( MapaBlade );
    File->ReadByte( Unknown );
    File->Seek( 9, FFileAbstract::SeekCur ); // Nine zero bytes?
    int32_t UnknownInt32;
    File->ReadSwapInt32( UnknownInt32 );
    int16_t BitmapsCount;
    File->ReadSwapInt16( BitmapsCount );
    Textures.Resize( BitmapsCount );
    for ( int i = 0 ; i < BitmapsCount ; i++ ) {
        FTextureEntry & Texture = Textures[ i ];
        File->ReadString( Texture.Name );
        File->ReadString( Texture.Path );
        File->ReadByte( Unknown );
        File->Seek( 31, FFileAbstract::SeekCur ); // Thirty one zero bytes?
    };
    int16_t AtmospheresCount;
    File->ReadSwapInt16( AtmospheresCount );
    Atmospheres.Resize( AtmospheresCount );
    for ( int i = 0 ; i < AtmospheresCount ; i++ ) {
        FAtmosphereEntry & Atmo = Atmospheres[ i ];
        File->ReadString( Atmo.Name );
        File->Read( &Atmo.Color[ 0 ], 3 );
        File->ReadSwapFloat( Atmo.Intensity );
    }
    // Initial point
    double InitialX, InitialY, InitialZ;
    double OrientX, OrientY, OrientZ;
    File->ReadSwapDouble( InitialX );
    File->ReadSwapDouble( InitialY );
    File->ReadSwapDouble( InitialZ );
    File->ReadSwapDouble( OrientX );
    File->ReadSwapDouble( OrientY );
    File->ReadSwapDouble( OrientZ );

    double dX, dY;
    File->ReadSwapDouble( dX );
    File->ReadSwapDouble( dY );

    FMapLimits Limits;
    File->ReadSwapDouble( Limits.Left );
    File->ReadSwapDouble( Limits.Right );
    File->ReadSwapDouble( Limits.Upper );
    File->ReadSwapDouble( Limits.Lower );

    int16_t SectorsCount;
    File->ReadSwapInt16( SectorsCount ); // FIXME: int16 or int32_t ???
    int16_t UnknownInt16;
    // The value such as WORD (2 bytes) - is equal 1 (? Why?).
    File->ReadSwapInt16( UnknownInt16 );
    Sectors.Resize( SectorsCount );
    for ( int SectorIndex = 0 ; SectorIndex < SectorsCount ; SectorIndex++ ) {
        FSectorEntry & Sector = Sectors[ SectorIndex ];

        FileOffset = File->Tell();
        File->ReadString( Sector.Name );
        File->Seek( 14, FFileAbstract::SeekCur ); // 14 zero bytes (? What they mean?)
        File->ReadByte( Unknown ); // Byte $F0 (? What it(he) means?).
        File->ReadByte( Unknown ); // Byte $BF (? What it(he) means?).
        File->Seek( 8, FFileAbstract::SeekCur ); // 8 zero bytes (? What they mean?)

        // PARAMETERS of a FLOOR
        File->ReadSwapDouble( Sector.FloorHeight0 );
        File->ReadString( Sector.FloorTexture );
        File->ReadSwapFloat( Sector.FloorScale );
        File->ReadSwapFloat( Sector.FloorCornerOfTurn );
        File->ReadSwapFloat( Sector.FloorX );
        File->ReadSwapFloat( Sector.FloorY );
        File->ReadSwapDouble( Sector.FloorHeight );
        File->ReadByte( Sector.FloorType );
        File->ReadByte( Unknown ); // Byte with value 2. (?)
        File->Seek( 3, FFileAbstract::SeekCur ); // 3 zero bytes (? What they mean?)
        File->ReadSwapDouble( Sector.FloorSphericalRadius );
        File->ReadSwapInt32( Sector.FloorSegmentsCount );
        File->ReadSwapDouble( Sector.FloorEllipseXParameter );
        File->ReadSwapDouble( Sector.FloorEllipseYParameter );

        // Unknown data
        File->Seek( 14, FFileAbstract::SeekCur ); // 14 zero bytes (? What they mean?)
        File->ReadByte( Unknown ); // Byte $F0 (? What it(he) means?).
        File->ReadByte( Unknown ); // Byte $3F (? What it(he) means?).
        File->Seek( 8, FFileAbstract::SeekCur ); // 8 zero bytes (? What they mean?)

        // PARAMETERS of a CEILING
        File->ReadSwapDouble( Sector.CeilHeight0 );
        File->ReadString( Sector.CeilTexture );
        File->ReadSwapFloat( Sector.CeilScale );
        File->ReadSwapFloat( Sector.CeilCornerOfTurn );
        File->ReadSwapFloat( Sector.CeilX );
        File->ReadSwapFloat( Sector.CeilY );
        File->ReadSwapDouble( Sector.CeilHeight );
        File->ReadByte( Sector.CeilType );
        File->ReadByte( Unknown ); // Byte with value 2. (?)
        File->Seek( 3, FFileAbstract::SeekCur ); // 3 zero bytes (? What they mean?)
        File->ReadSwapDouble( Sector.CeilSphericalRadius );
        File->ReadSwapInt32( Sector.CeilSegmentsCount );
        File->ReadSwapDouble( Sector.CeilEllipseXParameter );
        File->ReadSwapDouble( Sector.CeilEllipseYParameter );

        // PARAMETERS of ILLUMINATION
        File->Read( &Sector.EnvironmentIllum[ 0 ], 3 );
        File->Read( &Sector.InternalIllum[ 0 ], 3 );
        File->Read( &Sector.ExternalIllum[ 0 ], 3 );
        File->Read( &Sector.FinalEnvironmentIllum[ 0 ], 3 );
        File->Read( &Sector.FinalInternalIllum[ 0 ], 3 );
        File->ReadSwapDouble( Sector.InternalIllumCoord[ 0 ] );
        File->ReadSwapDouble( Sector.InternalIllumCoord[ 1 ] );
        File->ReadSwapDouble( Sector.InternalIllumCoord[ 2 ] );
        File->ReadSwapDouble( Sector.ExternalIllumCoord[ 0 ] );
        File->ReadSwapDouble( Sector.ExternalIllumCoord[ 1 ] );
        File->ReadSwapDouble( Sector.ExternalIllumCoord[ 2 ] );

        // Parameters of an atmosphere
        File->ReadString( Sector.AtmosphereName );

        // Attributes of sector
        File->ReadSwapInt32( UnknownInt32 ); // equal 2. (?) FIXME: AttributeCount?
        File->ReadString( Sector.AttributeName ); // FIXME: Read Array of Attributes?

        // Parameters of points
        int16_t VerticesCount;
        File->ReadSwapInt16( VerticesCount );
        Sector.Vertices.Resize( VerticesCount );
        for ( int i = 0 ; i < VerticesCount ; i++ ) {
            File->ReadSwapDouble( Sector.Vertices[ i ].X );
            File->ReadSwapDouble( Sector.Vertices[ i ].Y );
        }

        // Parameters of walls
        int16_t WallsCount;
        File->ReadSwapInt16( WallsCount );
        Sector.Walls.Resize( WallsCount );
        for ( int i = 0 ; i < WallsCount ; i++ ) {
            FWall & Wall = Sector.Walls[ i ];
            FileOffset = File->Tell();
            File->Read( Wall.Color, 4 );
            File->ReadSwapInt32( Wall.IncludeFloor );
            File->ReadSwapInt32( Wall.IncludeCeil );
            File->ReadString( Wall.TextureName );
            Out() << Wall.TextureName;
            File->ReadSwapFloat( Wall.Scale );
            File->ReadSwapFloat( Wall.CornerOfTurn );
            File->ReadSwapFloat( Wall.X );
            File->ReadSwapFloat( Wall.Y );
            Out() << Wall.Scale << Wall.CornerOfTurn << Wall.X << Wall.Y;
            int16_t UnknownCount;
            File->ReadSwapInt16( UnknownCount );
            for ( int UnknownEntryIndex = 0 ; UnknownEntryIndex < UnknownCount ; UnknownEntryIndex++ ) {
                File->Seek( 28, FFileAbstract::SeekCur ); // FIXME?
            }
        }

        int16_t UnknownCount;
        File->ReadSwapInt16( UnknownCount );
        for ( int i = 0 ; i < UnknownCount ; i++ ) {
            File->Seek( 43, FFileAbstract::SeekCur ); // FIXME?
        }

        FileOffset = File->Tell();
        File->Seek( 138, FFileAbstract::SeekCur ); // FIXME?
    }

    FFiles::CloseFile( File );
}

void FBladeMap::FreeMap() {
    Textures.Clear();
    Atmospheres.Clear();
    Sectors.Clear();
}
