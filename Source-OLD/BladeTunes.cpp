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

#include "BladeTunes.h"

#include <Engine/IO/Public/FileUrl.h>

void FBladeTunes::SetDefaults() {
    AmbientScale = 0.2f;
    LightScale = 8.0f;
    SunBrightness = 1.0f;
}

void FBladeTunes::LoadTunes( const char * _FileName ) {
    char Str[256];
    char Key[256];
    char Value[256];

    SetDefaults();

    FFileAbstract * File = FFiles::OpenFileFromUrl( _FileName, FFileAbstract::M_Read );
    if ( !File ) {
        return;
    }

    while ( File->Gets( Str, sizeof( Str ) - 1 ) ) {
        Key[0] = 0;
        Value[0] = 0;

        sscanf( Str, "%s", Key );

        if ( !Key[0] ) {
            continue;
        }

        if ( sscanf( Str, "%s %s", Key, Value ) != 2 ) {
            continue;
        }

        if ( !Key[0] || !Value[0] ) {
            // Empty string
            continue;
        }

        if ( !FString::CmpCase( Key, "AmbientScale" ) ) {

            AmbientScale = FString::ToFloat( Value );

        } else if ( !FString::CmpCase( Key, "LightScale" ) ) {

            LightScale = FString::ToFloat( Value );

        } else if ( !FString::CmpCase( Key, "SunBrightness" ) ) {

            SunBrightness = FString::ToFloat( Value );

        } else {
            Out() << "LoadTunes: Unknown key" << Key;
        }
    }

    FFiles::CloseFile( File );
}
