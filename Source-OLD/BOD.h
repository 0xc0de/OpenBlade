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

#include <Engine/Runnable.h>
#include <Engine/Window.h>

#include <Engine/Renderer/Public/MaterialResource.h>
#include <Engine/Renderer/Public/MaterialInstance.h>

#include <Engine/Renderer/Public/StaticMeshResource.h>

#include <Engine/Scene/Public/Scene.h>
#include <Engine/Renderer/Public/SpatialTreeComponent.h>

#include <Engine/AI/Public/ChunkedMeshComponent.h>

class FGame : public FRunnable {
public:
    void OnBeforeCommandLineProcessing( TPodArray< char * > & _Arguments ) override;
    void OnSetPrimaryWindowDefs( FWindowDefs & _WindowDefs ) override;
    void OnInitialize() override;
    void OnShutdown() override;
    void OnUpdate( float _TimeStep ) override;

    void OnKeyPress( FKeyPressEvent & _Event );
    void OnKeyRelease( FKeyReleaseEvent & _Event );
    void OnChar( FCharEvent & _Event );
    void OnMouseMove( FMouseMoveEvent & _Event );
    void OnUpdateGui( FUpdateGuiEvent & _Event );
};
