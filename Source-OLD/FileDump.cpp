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

#include "FileDump.h"

static bool DumpLogEnabled = false;

void SetDumpLog( bool _Enable ) {
    DumpLogEnabled = _Enable;
}

void DumpUnknownBytes( FFileAbstract * _File, int _BytesCount ) {
    Byte * Bytes = new Byte[ _BytesCount ];
    Long Position = _File->Tell();
    _File->Read( Bytes, _BytesCount );
    if ( DumpLogEnabled ) {
        for ( Byte * pByte = Bytes ; pByte < &Bytes[_BytesCount] ; pByte++ ) {
            Out() << ( Position++ ).ToHexString( true ) << ":" << ( *pByte ).ToHexString( true ) << char( *pByte );
        }
    }
    delete [] Bytes;
}

void DumpIntOrFloat( FFileAbstract * _File ) {
    int32_t Unknown;
    Byte * pUnknown = (Byte *)&Unknown;
    _File->ReadSwapInt32( Unknown );
    if ( DumpLogEnabled ) {
        Out() << Long( _File->Tell() - 4 ).ToHexString( true ) << ":" << Unknown << *(float *)&Unknown << "hex :" << pUnknown[0].ToHexString( true ) << pUnknown[1].ToHexString( true ) << pUnknown[2].ToHexString( true ) << pUnknown[3].ToHexString( true );
    }
}

int32_t DumpInt( FFileAbstract * _File ) {
    int32_t Unknown;
    Byte * pUnknown = (Byte *)&Unknown;
    _File->ReadSwapInt32( Unknown );
    if ( DumpLogEnabled ) {
        Out() << Long( _File->Tell() - 4 ).ToHexString( true ) << ":" << Unknown << "hex :" << pUnknown[0].ToHexString( true ) << pUnknown[1].ToHexString( true ) << pUnknown[2].ToHexString( true ) << pUnknown[3].ToHexString( true );
    }
    return Unknown;
}

int16_t DumpShort( FFileAbstract * _File ) {
    int16_t Unknown;
    Byte * pUnknown = (Byte *)&Unknown;
    _File->ReadSwapInt16( Unknown );
    if ( DumpLogEnabled ) {
        Out() << Long( _File->Tell() - 2 ).ToHexString( true ) << ":" << Unknown << "hex :" << pUnknown[0].ToHexString( true ) << pUnknown[1].ToHexString( true );
    }
    return Unknown;
}

int32_t DumpIntNotSeek( FFileAbstract * _File ) {
    int32_t Unknown;
    Byte * pUnknown = (Byte *)&Unknown;
    _File->ReadSwapInt32( Unknown );
    if ( DumpLogEnabled ) {
        Out() << Long( _File->Tell() - 4 ).ToHexString( true ) << ":" << Unknown << "hex :" << pUnknown[0].ToHexString( true ) << pUnknown[1].ToHexString( true ) << pUnknown[2].ToHexString( true ) << pUnknown[3].ToHexString( true );
    }
    _File->Seek( -4, FFileAbstract::SeekCur );
    return Unknown;
}

int32_t DumpByte( FFileAbstract * _File ) {
    Byte Unknown;
    _File->Read( &Unknown, 1 );
    if ( DumpLogEnabled ) {
        Out() << Long( _File->Tell() - 1 ).ToHexString( true ) << ":" << Unknown << "hex :" << Unknown.ToHexString( true );
    }
    return Unknown;
}

float DumpFloat( FFileAbstract * _File ) {
    float Unknown;
    Byte * pUnknown = (Byte *)&Unknown;
    _File->ReadSwapFloat( Unknown );
    if ( DumpLogEnabled ) {
        Out() << Long( _File->Tell() - 4 ).ToHexString( true ) << ":" << Unknown << "hex :" << pUnknown[0].ToHexString( true ) << pUnknown[1].ToHexString( true ) << pUnknown[2].ToHexString( true ) << pUnknown[3].ToHexString( true );
    }
    return Unknown;
}

double DumpDouble( FFileAbstract * _File ) {
    double Unknown;
    Byte * pUnknown = (Byte *)&Unknown;
    _File->ReadSwapDouble( Unknown );
    if ( DumpLogEnabled ) {
        Out() << Long( _File->Tell() - 8 ).ToHexString( true ) << ":" << Unknown << "hex :" << pUnknown[0].ToHexString( true ) << pUnknown[1].ToHexString( true ) << pUnknown[2].ToHexString( true ) << pUnknown[3].ToHexString( true ) << pUnknown[4].ToHexString( true ) << pUnknown[5].ToHexString( true ) << pUnknown[6].ToHexString( true ) << pUnknown[7].ToHexString( true );
    }
    return Unknown;
}

FString DumpString( FFileAbstract * _File ) {
    FString Unknown;
    Long FileOffset = _File->Tell();
    _File->ReadString( Unknown );
    if ( DumpLogEnabled ) {
        Out() << FileOffset.ToHexString( true ) << ":" << Unknown;
    }
    return Unknown;
}

int DumpFileOffset( FFileAbstract * _File ) {
    Long FileOffset = _File->Tell();
    if ( DumpLogEnabled ) {
        Out() << "FileOffset :" << FileOffset.ToHexString( true );
    }
    return FileOffset;
}
