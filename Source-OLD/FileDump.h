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

#include <Engine/IO/Public/FileAbstract.h>

void SetDumpLog( bool _Enable );
void DumpUnknownBytes( FFileAbstract * _File, int _BytesCount );
void DumpIntOrFloat( FFileAbstract * _File );
int32_t DumpInt( FFileAbstract * _File );
int16_t DumpShort( FFileAbstract * _File );
int32_t DumpIntNotSeek( FFileAbstract * _File );
int32_t DumpByte( FFileAbstract * _File );
float DumpFloat( FFileAbstract * _File );
double DumpDouble( FFileAbstract * _File );
FString DumpString( FFileAbstract * _File );
int DumpFileOffset( FFileAbstract * _File );
