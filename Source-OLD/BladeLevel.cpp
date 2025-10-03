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

#include "BladeLevel.h"
#include "BladeWorld.h"
#include "BladeTextures.h"

#include <Engine/IO/Public/FileUrl.h>

FTextureResource * SkyboxTexture;
Float3 SkyColorAvg;

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

void LoadLevel( const char * _FileName ) {
    char Str[256];
    char Key[256];
    char Value[256];
    FString FinalFileName;

    FString FileLocation = FString( _FileName ).StripFilename();

    FFileAbstract * File = FFiles::OpenFileFromUrl( _FileName, FFileAbstract::M_Read );
    if ( !File ) {
        return;
    }

    while ( File->Gets( Str, sizeof( Str ) - 1 ) ) {
        Key[0] = 0;
        Value[0] = 0;

        if ( sscanf( Str, "%s -> %s", Key, Value ) != 2 ) {
            continue;
        }

        if ( !Key[0] || !Value[0] ) {
            continue;
        }

        Out() << "KEY:" << Key << "!";
        Out() << "VALUE:" << Value << "!";

        FString::UpdateSeparator( Value );
        FixPath( Value );

        FinalFileName = ( FileLocation + "/" + Value ).OptimizePath();

        Out() << "OPTIMIZED:" << FinalFileName;

        if ( !FString::CmpCase( Key, "Bitmaps" ) ) {
            LoadTextures( FinalFileName.Str() );
        } else if ( !FString::CmpCase( Key, "WorldDome" ) ) {
            SkyboxTexture = LoadDome( FinalFileName.Str(), &SkyColorAvg );
        } else if ( !FString::CmpCase( Key, "World" ) ) {
            World.LoadWorld( FinalFileName.Str() );
        } else {
            Out() << "LoadLevel: Unknown key" << Key;
        }
    }

    FFiles::CloseFile( File );

    if ( !SkyboxTexture ) {
        // Try to load default dome
        FString DomeName = _FileName;
        DomeName.StripExt().Concat( "_d.mmp" );
        SkyboxTexture = LoadDome( DomeName.Str(), &SkyColorAvg );
    }
}
