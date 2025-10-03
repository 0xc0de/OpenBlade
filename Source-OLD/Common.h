#pragma once

#include <Engine/Renderer/Public/TextureResource.h>
#include <Engine/Renderer/Public/StaticMeshResource.h>
#include <Engine/Renderer/Public/SkinnedMeshResource.h>
#include <Engine/Renderer/Public/SkinnedAnimResource.h>

FTextureResource * CreateCubemapTexture( const char * _Names[6], bool _SimulateHDRI );
FSkinnedAnimResource * LoadSkinnedAnimEx( const char * _FileName, int _AnimIndex, const FSkinnedMeshResource * _Model );
FStaticMeshResource * LoadStaticMesh( const char * _FileName );
FSkinnedMeshResource * LoadSkinnedMesh( const char * _FileName, float _Scale );
