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

#include "World.h"
#include "BladeWorld.h"

AN_SCENE_COMPONENT_DECL( FWorldComponent, CCF_ROOT | CCF_HIDDEN_IN_EDITOR )

int FWorldComponent::FindSpatialArea( const Float3 & _Position ) {
    bool Inside;
    int SectorIndex = -1;
    EPlaneSide Offset;
    Double3 Pos(_Position);
    for ( int i = 0 ; i < World.Sectors.Length() ; i++ ) {
        FBladeWorld::FSector & Sector = World.Sectors[i];

        Inside = true;
        for ( int f = 0 ; f < Sector.Faces.Length() ; f++ ) {
            FBladeWorld::FFace * Face = Sector.Faces[f];

            Offset = Face->Plane.SideOffset( Pos, 0.0 );
            if ( Offset != EPlaneSide::Front ) {
                Inside = false;
                break;
            }
        }

        if ( Inside ) {
            SectorIndex = i;
            break;
        }
    }
    return SectorIndex;
}
