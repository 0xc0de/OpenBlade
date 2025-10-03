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

#include "BladeCSV.h"

#include <Engine/IO/Public/FileUrl.h>

void FBladeCSV::LoadCSV( const char * _FileName ) {
    char Str[256];
    char BOD[256];
    char Name[256];
    char UnknownString[256];
    FEntry Entry;

    FFileAbstract * File = FFiles::OpenFileFromUrl( _FileName, FFileAbstract::M_Read );
    if ( !File ) {
        return;
    }

    Entries.Clear();

    while ( File->Gets( Str, sizeof( Str ) - 1 ) ) {
        
        if ( sscanf( Str, "%s %s %f %d %s", BOD, Name, &Entry.UnknownValue1, &Entry.UnknownValue2, UnknownString ) != 5 ) {
            Out() << "LoadCSV: Not enough parameters";
            continue;
        }

        Entry.BOD = BOD;
        Entry.Name = Name;
        Entry.UnknownString = UnknownString;

        Entries.Append( Entry );
    }

    FFiles::CloseFile( File );
}
