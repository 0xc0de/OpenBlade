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

#include "BladeSF.h"

#include <Engine/IO/Public/FileUrl.h>

void FBladeSF::ResetToDefaults( FGhostSector & _Sector ) {
    _Sector.Name.Clear();
    _Sector.FloorHeight = 0;
    _Sector.RoofHeight = 0;
    _Sector.Vertices.Clear();
    _Sector.Group.Clear();
    _Sector.Sound.Clear();
    _Sector.Volume = 0;
    _Sector.VolumeBase = 0;
    _Sector.MinDist = 0;
    _Sector.MaxDist = 0;
    _Sector.MaxVerticalDist = 0;
    _Sector.Scale = 0;
}

// Skip sequences of //
static void FixPath( char * _Path ) {
    int Length = strlen( _Path );
    while ( *_Path ) {
        if ( *_Path == '/' && *( _Path + 1 ) == '/' ) {
            memmove( _Path, _Path + 1, Length );
        } else {
            ++_Path;
        }
        --Length;        
    }
}

void FBladeSF::LoadSF( const char * _FileName ) {
    char Str[256];
    char Key[256];
    char Value[256];
    FGhostSector * Sector = NULL;

    FFileAbstract * File = FFiles::OpenFileFromUrl( _FileName, FFileAbstract::M_Read );
    if ( !File ) {
        return;
    }

    GhostSectors.Clear();

    if ( !File->Gets( Str, sizeof( Str ) - 1 ) ) {
        return;
    }

    int NumSectors = 0;
    sscanf( Str, "%s => %d", Key, &NumSectors );
    if ( !FString::CmpCase( Key, "NumGhostSectors" ) ) {
        GhostSectors.Reserve( NumSectors );
    }

    FString FileLocation = FString( _FileName ).StripFilename();

    while ( File->Gets( Str, sizeof( Str ) - 1 ) ) {
        Key[0] = 0;
        Value[0] = 0;

        sscanf( Str, "%s", Key );

        if ( !Key[0] ) {
            continue;
        }

        if ( !FString::CmpCase( Key, "BeginGhostSector" ) ) {
            // create new ghost sector

            if ( Sector ) {
                Out() << "LoadSF: Unexpected begin of sector";
                continue;
            }

            GhostSectors.Append( FGhostSector() );

            Sector = &GhostSectors.Last();
            ResetToDefaults( *Sector );

            continue;
        } else if ( !FString::CmpCase( Key, "EndGhostSector" ) ) {
            // finish ghost sector

            if ( !Sector ) {
                Out() << "LoadSF: Unexpected end of sector";
            }

            Sector = NULL;

            continue;
        }

        if ( !Sector ) {
            // ignore all stuff outside of sector section
            continue;
        }

        if ( sscanf( Str, "%s => %s", Key, Value ) != 2 ) {
            continue;
        }

        if ( !Key[0] || !Value[0] ) {
            // Empty string
            continue;
        }

        if ( !FString::CmpCase( Key, "Name" ) ) {

            Sector->Name = Value;

        } else if ( !FString::CmpCase( Key, "FloorHeight" ) ) {

            Sector->FloorHeight = atof( Value );

        } else if ( !FString::CmpCase( Key, "RoofHeight" ) ) {

            Sector->RoofHeight = atof( Value );

        } else if ( !FString::CmpCase( Key, "Vertex" ) ) {
            Float2 v;

            if ( sscanf( Str, "%s => %f %f", Value, &v.X.Value, &v.Y.Value ) == 3 ) {
                Sector->Vertices.Append( v );
            }

        } else if ( !FString::CmpCase( Key, "Grupo" ) ) {

            Sector->Group = Value;

        } else if ( !FString::CmpCase( Key, "Sonido" ) ) {

            FString::UpdateSeparator( Value );
            FixPath( Value );

            Sector->Sound = ( FileLocation + "/" + Value ).OptimizePath();

        } else if ( !FString::CmpCase( Key, "Volumen" ) ) {

            Sector->Volume = atof( Value );

        } else if ( !FString::CmpCase( Key, "VolumenBase" ) ) {

            Sector->VolumeBase = atof( Value );

        } else if ( !FString::CmpCase( Key, "DistanciaMinima" ) ) {

            Sector->MinDist = atof( Value );

        } else if ( !FString::CmpCase( Key, "DistanciaMaxima" ) ) {

            Sector->MaxDist = atof( Value );

        } else if ( !FString::CmpCase( Key, "DistMaximaVertical" ) ) {

            Sector->MaxVerticalDist = atof( Value );

        } else if ( !FString::CmpCase( Key, "Escala" ) ) {

            Sector->Scale = atof( Value );

        } else {
            Out() << "LoadSF: Unknown key" << Key;
        }
    }

    FFiles::CloseFile( File );
}
