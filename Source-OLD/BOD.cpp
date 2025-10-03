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

#include "BOD.h"
#include "BladeWorld.h"
#include "BladeLevel.h"
#include "BladeTextures.h"
#include "BladeCSV.h"
#include "BladeSF.h"
#include "BladeCAM.h"
#include "BladeTunes.h"
#include "BladeModel.h"
#include "World.h"
#include "ImGuiAPI.h"
#include "Common.h"

#include <Engine/IO/Public/FileUrl.h>
#include <Engine/Core/Public/Color.h>
#include <Engine/Utilites/Public/CmdManager.h>

#include <Engine/EntryDecl.h>

#include <Engine/Renderer/Public/StaticMeshComponent.h>
#include <Engine/Renderer/Public/LightComponent.h>
#include <Engine/Renderer/Public/EnvCaptureComponent.h>

#include <Engine/Audio/Public/AudioResource.h>
#include <Engine/Audio/Public/AudioComponent.h>
#include <Engine/Audio/Public/AudioSourceComponent.h>
#include <Engine/Audio/Public/AudioListenerComponent.h>
#include <Engine/Audio/Public/AudioSystem.h>

#include <Engine/Physics/Public/PhysicsComponent.h>

#include <Engine/Resource/Public/ResourceManager.h>

//#define UNLIT
//#define DEBUG_BLADE_MP_SECTORS
//#define DEBUG_BLADE_FACE_PORTAL
//#define DEBUG_BLADE_GHOST_SECTORS
//#define DEBUG_BLADE_CAMERA_RECORDS
//#define DEBUG_CHARACTER_SELECTION
//#define DEBUG_WORLD_PICKING
//#define DEBUG_PORTALS
//#define DEBUG_MONITOR_GAMMA

// Config variables
static FCVarInt     demo_width( "demo_width", "1024" );
static FCVarInt     demo_height( "demo_height", "768" );
static FCVarBool    demo_fullscreen( "demo_fullscreen", "0" );
static FCVarBool    demo_ssao( "demo_ssao", "1" );
static FCVarBool    demo_colorgrading( "demo_colorgrading", "0" );
static FVariable    demo_gamepath( "demo_gamepath", "E:\\Games\\Blade Of Darkness" );
static FVariable    demo_gamelevel( "demo_gamelevel", "Maps/Mine_M5/mine.lvl" );
static FVariable    demo_music( "demo_music", "Sounds/MAPA2.mp3" );

// Common objects
static FWindow *                Window;             // Primary game window
static TSharedPtr< FScene >     Scene;              // Level scene
static FSceneNode *             CameraNode;         // View camera
static FCameraComponent *       Camera;             // View camera
static FTextureResource *       RenderTexture;      // Render target texture
static FRenderTarget *          RenderTarget;       // Render target owner
static FChunkedMeshComponent *  ChunkedMesh;        // Optimized world mesh storage for fast world-ray intersection
static FBladeTunes              Tunes;
static FBladeModel              Model;

// For camera record debugging
static FCameraRecord amazona_barbaro[2];        // amazona   -> barbaro,   barbaro   -> amazona
static FCameraRecord barbaro_caballero[ 2 ];    // barbaro   -> caballero, caballero -> barbaro
static FCameraRecord caballero_enano[ 2 ];      // caballero -> enano,     enano     -> caballero
static FCameraRecord enano_amazona[ 2 ];        // enano     -> amazona,   amazona   -> enano
enum { amazona, barbario, caballero, enano };

static const char * MakePath( const char * _Path ) {
    static FString Tmp;
    Tmp = FString( demo_gamepath.GetString() ) + "/" + _Path;
    return Tmp.Str();
}

static void LoadConfigFile() {
    FFileAbstract * File = FFiles::OpenFileFromUrl( "blade.cfg", FFileAbstract::M_Read );
    if ( File ) {
        long Length = File->Length();
        char * Text = new char[Length+1];
        File->Read( Text, Length );
        Text[Length] = '\0';
        GCmdManager->BufferOp( ECmdBufferOperation::ExecuteText, Text );
        delete [] Text;
        FFiles::CloseFile( File );
    }
}

static void CreateAudioForSector( const FBladeSF::FGhostSector & _Sector ) {
    Float3 Center(0);

    // Compute sector center
    int VerticesCount = _Sector.Vertices.Length();
    if ( VerticesCount > 0 ) {
        for ( int j = 0 ; j < VerticesCount ; j++ ) {
            const Float2 & v = _Sector.Vertices[ j ];
            Center.X += v.X;
            Center.Z += v.Y;
        }
        Center.X /= VerticesCount;
        Center.Z /= VerticesCount;
        Center.Y = ( _Sector.FloorHeight + _Sector.RoofHeight ) * 0.5f;
    }

#if 0
    // Create trigger based on sector convex hull
    TPodArray<Float3> VertexSoup;
    for ( int j = 0 ; j < VerticesCount ; j++ ) {
        const Float2 & v = Sector.Vertices[ j ];
        VertexSoup.Append( Float3( v.X, Sector.FloorHeight, v.Y ) );
    }
    for ( int j = 0 ; j < VerticesCount ; j++ ) {
        const Float2 & v = Sector.Vertices[ j ];
        VertexSoup.Append( Float3( v.X, Sector.RoofHeight, v.Y ) );
    }
    FSceneNode * TriggerNode = Scene->CreateChild( "TriggerNode" );
    FRigidBodyComponent * RigidBody = TriggerNode->CreateComponent< FRigidBodyComponent >();
    RigidBody->SetTrigger( true );
    FCollisionShapeComponent * CollisionShape = TriggerNode->CreateComponent< FCollisionShapeComponent >();
    CollisionShape->SetConvexHull( VertexSoup.ToPtr(), VertexSoup.Length() );
#endif

    // Load audio
    FAudioResource * Resource = GResourceManager->GetResource< FAudioResource >( _Sector.Sound.Str() );
    FAudioResource::FLoadParameters LoadParameters;
    LoadParameters.StreamType = SST_NonStreamed;
    Resource->SetLoadParameters( LoadParameters );
    Resource->Load();

    // Create audio object
    FSceneNode * AudioNode = Scene->CreateChild( "SectorAudio" );

    // Locate audio node at center of ghost sector
    AudioNode->SetPosition( Center );

    // Create audio player
    FAudioSourceComponent * Audio = AudioNode->CreateComponent< FAudioSourceComponent >();
    Audio->SetAudio( Resource );
    Audio->SetTag( SND_SOUND );
    Audio->SetLooping( true );
    Audio->SetRelativeToListener( false );
    Audio->SetGain( _Sector.Volume * 10.0f );
    Audio->SetRolloffFactor( 1 );
    Audio->SetReferenceDistance( _Sector.MinDist * 0.001f );
    Audio->Play();

    // TODO: adapt ghost sector audio parameters to Angie Engine AudioSource parameters

    // distance = clamp(distance,MaxDistance,ReferenceDistance);
    // gain = ReferenceDistance / (ReferenceDistance +  RolloffFactor * (distance Ц ReferenceDistance) )
    //Audio->SetMaxDistance( _Sector.MinDist * 0.001f );
    //Audio->SetReferenceDistance( _Sector.MaxDist * 0.001f );
    //Audio->SetReferenceDistance( _Sector.MinDist * 0.001f );

    Out() << "AUDIO GROUP:" << _Sector.Group << _Sector.Sound;
}

static void LoadGhostSectors( const char * _FileName ) {
    FBladeSF SF;

    SF.LoadSF( _FileName );

    for ( int i = 0 ; i < SF.GhostSectors.Length() ; i++ ) {
        CreateAudioForSector( SF.GhostSectors[i] );
    }
}

static void LoadMusic() {
    FAudioResource::FLoadParameters LoadParameters;

    // Play and stream from file
    LoadParameters.StreamType = SST_FileStreamed;

    // Play and stream from memory
    //LoadParameters.StreamType = SST_MemoryStreamed;

    // Play from memory without streaming
    //LoadParameters.StreamType = SST_NonStreamed;

    // Load music resource
    FAudioResource * Snd = GResourceManager->GetResource< FAudioResource >( MakePath( demo_music.GetString() ) );
    Snd->SetLoadParameters( LoadParameters );
    Snd->Load();

    // Create music object
    FSceneNode * N = Scene->CreateChild( "Music" );

    // Create music player
    FAudioSourceComponent * Source = N->CreateComponent< FAudioSourceComponent >();
    Source->SetAudio( Snd );
    Source->SetLooping( true );
    Source->SetTag( SND_MUSIC );
    Source->SetRelativeToListener( true );
    Source->SetGain( 0.4f );
    Source->Play();
}

static void CreateAreasAndPortals() {
    TPodArray< FSpatialAreaComponent * > Areas;

    Areas.Resize( World.Sectors.Length() );
    for ( int i = 0 ; i < Areas.Length() ; i++ ) {
        Areas[i] = Scene->CreateComponent< FSpatialAreaComponent >();
        Areas[i]->SetPosition( World.Sectors[i].Bounds.Center() );
        Areas[i]->SetBox( World.Sectors[i].Bounds.Size() + Float3(0.01f) );
        Areas[i]->SetUseReferencePoint( true );
        Areas[i]->SetReferencePoint( World.Sectors[i].Centroid );
    }

    for ( int i = 0 ; i < World.Sectors.Length() ; i++ ) {
        FBladeWorld::FSector & Sector = World.Sectors[i];
        for ( int j = 0 ; j < Sector.Portals.Length() ; j++ ) {
            Sector.Portals[j]->Marked = false;
        }
    }

    PolygonF Winding;
    for ( int i = 0 ; i < World.Sectors.Length() ; i++ ) {
        FBladeWorld::FSector & Sector = World.Sectors[i];

        for ( int j = 0 ; j < Sector.Portals.Length() ; j++ ) {
            FBladeWorld::FPortal * Portal = Sector.Portals[j];

            if ( Portal->Marked ) {
                continue;
            }

            FBladeWorld::FSector & Next = World.Sectors[Portal->ToSector];
            for ( int k = 0 ; k < Next.Portals.Length() ; k++ ) {
                FBladeWorld::FPortal * NextPortal = Next.Portals[k];
                if ( NextPortal->ToSector != i ) {
                    continue;
                }
                NextPortal->Marked = true;
            }

            Winding.Resize( Portal->Winding.Length() );
            for ( int k = 0 ; k < Winding.Length() ; k++ ) {
                Winding[k] = Float3( Portal->Winding[k] * BLADE_COORD_SCALE_D );
            }

            FSpatialPortalComponent * SpatialPortal = Scene->CreateComponent< FSpatialPortalComponent >();
            SpatialPortal->SetAreas( Areas[i], Areas[Portal->ToSector] );
            SpatialPortal->SetWinding( Winding.ToPtr(), Winding.Length() );
        }
    }
}

static void CreateCamera() {
    FMonitor * PrimaryMonitor = GPlatformPort->GetPrimaryMonitor();

    // Create camera object
    CameraNode = Scene->CreateChild( "Camera" );
    CameraNode->SetPosition( World.Bounds.Center() );

    // Attach audio listener to camera
    CameraNode->CreateComponent< FAudioListenerComponent >();

    // Create camera
    Camera = CameraNode->CreateComponent< FCameraComponent >();
    //Camera->SetFovX( 100 );
    Camera->SetFovX( 90 );

    Camera->SetPerspectiveAdjust( FCameraComponent::ADJ_FOV_X_ASPECT_RATIO );

    if ( demo_fullscreen.GetBool() ) {
        Camera->SetMonitorAspectRatio( PrimaryMonitor );
    } else {
        Camera->SetWindowAspectRatio( Window->GetPlatformWindow() );
    }

    //FLightComponent * CameraLight = CameraNode->CreateComponent< FLightComponent >();
    //CameraLight->SetType( FLightComponent::T_Point );
    //CameraLight->SetOuterRadius( 10 );
    //CameraLight->SetColor(1,1,1);

    LoadTextures( "E:/Games/Blade of Darkness/3DObjs/3dObjs.mmp" );
    LoadTextures( "E:/Games/Blade of Darkness/3DObjs/bolarayos.mmp" );
    LoadTextures( "E:/Games/Blade of Darkness/3DObjs/CilindroMagico.mmp" );
    LoadTextures( "E:/Games/Blade of Darkness/3DObjs/CilindroMagico2.mmp" );
    LoadTextures( "E:/Games/Blade of Darkness/3DObjs/CilindroMagico3.mmp" );
    LoadTextures( "E:/Games/Blade of Darkness/3DObjs/conos.mmp" );
    LoadTextures( "E:/Games/Blade of Darkness/3DObjs/dalblade.mmp" );
    LoadTextures( "E:/Games/Blade of Darkness/3DObjs/esferagemaazul.mmp" );
    LoadTextures( "E:/Games/Blade of Darkness/3DObjs/esferagemaroja.mmp" );
    LoadTextures( "E:/Games/Blade of Darkness/3DObjs/esferagemaverde.mmp" );
    LoadTextures( "E:/Games/Blade of Darkness/3DObjs/esferanegra.mmp" );
    LoadTextures( "E:/Games/Blade of Darkness/3DObjs/esferaorbital.mmp" );
    LoadTextures( "E:/Games/Blade of Darkness/3DObjs/espectro.mmp" );
    LoadTextures( "E:/Games/Blade of Darkness/3DObjs/firering.mmp" );
    LoadTextures( "E:/Games/Blade of Darkness/3DObjs/genericos.mmp" );
    LoadTextures( "E:/Games/Blade of Darkness/3DObjs/halfmoontrail.mmp" );
    LoadTextures( "E:/Games/Blade of Darkness/3DObjs/luzdivina.mmp" );
    LoadTextures( "E:/Games/Blade of Darkness/3DObjs/magicshield.mmp" );
    LoadTextures( "E:/Games/Blade of Darkness/3DObjs/nube.mmp" );
    LoadTextures( "E:/Games/Blade of Darkness/3DObjs/objetos_p.mmp" );
    LoadTextures( "E:/Games/Blade of Darkness/3DObjs/ondaexpansiva.mmp" );
    LoadTextures( "E:/Games/Blade of Darkness/3DObjs/Pfern.mmp" );
    LoadTextures( "E:/Games/Blade of Darkness/3DObjs/pmiguel.mmp" );
    LoadTextures( "E:/Games/Blade of Darkness/3DObjs/rail.mmp" );
    LoadTextures( "E:/Games/Blade of Darkness/3DObjs/telaranya.mmp" );
    LoadTextures( "E:/Games/Blade of Darkness/3DObjs/vortice.mmp" );
    LoadTextures( "E:/Games/Blade of Darkness/3DObjs/weapons.mmp" );

    LoadTextures( "E:/Games/Blade of Darkness/3DChars/Actors.mmp" );
    LoadTextures( "E:/Games/Blade of Darkness/3DChars/actors_javi.mmp" );

    LoadTextures( "E:/Games/Blade of Darkness/3DChars/ork.mmp" );
    LoadTextures( "E:/Games/Blade of Darkness/3DChars/Bar.mmp" );

    LoadTextures( "E:/Games/Blade of Darkness/3DChars/Kgt.mmp" );
    LoadTextures( "E:/Games/Blade of Darkness/3DChars/Kgtskin1.mmp" );
    LoadTextures( "E:/Games/Blade of Darkness/3DChars/Kgtskin2.mmp" );
    
    

    FSceneNode * Node = Scene->CreateChild( "test_model" );
    //Node->SetPosition(0,0,-3);
    //Node->SetPosition(-2,10,1);
    Node->SetPosition(0,2,0);
    Node->SetScale(1.0f);
    Node->SetAngles(90,0,0);

    FStaticMeshResource * Mesh = Model.Resource;

    FMaterialResource * Material = GResourceManager->GetResource< FMaterialResource >( "Blade/StandardMaterial.json" );
    Material->Load();

    BvAxisAlignedBox Bounds;
    for ( int i = 0 ; i < Mesh->GetMeshOffsets().Length() ; i++ ) {
        FSceneNode * Part = Node->CreateChild( "part" );

        Part->SetPosition( float(i), 0, 0 );

        const FMeshOffset & Offset = Mesh->GetMeshOffsets()[i];

        FStaticMeshComponent * Renderable = Part->CreateComponent< FStaticMeshComponent >();
    
        Renderable->SetMesh( Mesh );
        Renderable->EnableShadowCast( true );
        Renderable->EnableLightPass( true );
        Renderable->SetDrawRange( Offset.IndexCount, Offset.StartIndexLocation, Offset.BaseVertexLocation );

        Bounds.Clear();
        for ( int k = 0 ; k < Offset.IndexCount ; k++ ) {
            Bounds.AddPoint( Mesh->GetVertices()[ Offset.BaseVertexLocation + Mesh->GetIndices()[ Offset.StartIndexLocation + k ] ].Position );
        }
        Renderable->SetBounds( Bounds );
        Renderable->SetUseCustomBounds( true );

        FTextureResource * Texture = GResourceManager->GetResource< FTextureResource >( Offset.Abstract.Str() );

        FMaterialInstance * MaterialInstance = Material->CreateInstance();
        MaterialInstance->Set( MaterialInstance->AddressOf( "SmpBaseColor" ), Texture );

        Renderable->SetMaterialInstance( MaterialInstance );
        MaterialInstance->Set( MaterialInstance->AddressOf( "AmbientColor" ), 0.0f, 0.0f, 0.0f );
    }

#if 0
    FSceneNode * Node = CameraNode->CreateChild( "Cube" );
    Node->SetPosition(0,-1.6f,-4.0f);
    //Node->SetAngles(45,45,0);
    Node->SetScale(0.001f);

    // Create mesh
    {
        FStaticMeshResource * MeshResource = LoadStaticMesh( "Samples/PBRTest/knight-artorias/source/Artorias.fbx" );

        FTextureResource::FLoadParameters LoadParameters;
        memset( &LoadParameters, 0, sizeof( LoadParameters ) );
        LoadParameters.BuildMipmaps = true;

        const char * BaseColorMap[] = {
            "Samples/PBRTest/knight-artorias/textures/Mat_Circle_Base_Color.png",
            "Samples/PBRTest/knight-artorias/textures/Mat_Helmet_Base_Color.png",
            "Samples/PBRTest/knight-artorias/textures/Mat_Skirt_Base_Color.png",
            "Samples/PBRTest/knight-artorias/textures/Sword_albedo.jpg",
            "Samples/PBRTest/knight-artorias/textures/Mat_Chainmail_Base_Color.png",
            "Samples/PBRTest/knight-artorias/textures/Mat_PlateArmor_Base_Color.png"
        };
        const char * NormalMap[] = {
            "*normal*",
            "Samples/PBRTest/knight-artorias/textures/Mat_Helmet_Normal_OpenGL.png",
            "Samples/PBRTest/knight-artorias/textures/Mat_Skirt_Normal_OpenGL.png",
            "Samples/PBRTest/knight-artorias/textures/Sword_normal.jpg",
            "Samples/PBRTest/knight-artorias/textures/Mat_Chainmail_Normal_OpenGL.png",
            "Samples/PBRTest/knight-artorias/textures/Mat_PlateArmor_Normal_OpenGL.png"
        };
        const char * MetallicMap[] = {
            "*black*",
            "Samples/PBRTest/knight-artorias/textures/Mat_Helmet_Metallic.png",
            "Samples/PBRTest/knight-artorias/textures/Mat_Skirt_Metallic.png",
            "Samples/PBRTest/knight-artorias/textures/Sword_metallic.jpg",
            "Samples/PBRTest/knight-artorias/textures/Mat_Chainmail_Metallic.png",
            "Samples/PBRTest/knight-artorias/textures/Mat_PlateArmor_Metallic.png"
        };
        const char * RoughnessMap[] = {             
             "Samples/PBRTest/knight-artorias/textures/Mat_Circle_Roughness.png",
             "Samples/PBRTest/knight-artorias/textures/Mat_Helmet_Roughness.png",
             "Samples/PBRTest/knight-artorias/textures/Mat_Skirt_Roughness.png",
             "Samples/PBRTest/knight-artorias/textures/Sword_roughness.jpg",
             "Samples/PBRTest/knight-artorias/textures/Mat_Chainmail_Roughness.png",
             "Samples/PBRTest/knight-artorias/textures/Mat_PlateArmor_Roughness.png"
        };

        FMaterialResource * Material = GResourceManager->GetResource< FMaterialResource >( "Samples/PBRTest/knight-artorias.json" );
        Material->Load();

        // Init meshes
        const TArray< FMeshOffset > & _Meshes = MeshResource->GetMeshOffsets();
        for ( int i = 0 ; i < _Meshes.Length() ; i++ ) {
            const FMeshOffset & Offset = _Meshes[ i ];
            FStaticMeshComponent * Mesh = Node->CreateComponent< FStaticMeshComponent >();

            FTextureResource * BaseColor = GResourceManager->GetResource< FTextureResource >( ( BaseColorMap[i] ) );
            LoadParameters.SRGB = true;
            LoadParameters.BuildMipmaps = true;
            BaseColor->SetLoadParameters( LoadParameters );

            FTextureResource * Normal = GResourceManager->GetResource< FTextureResource >( ( NormalMap[i] ) );
            LoadParameters.SRGB = false;
            Normal->SetLoadParameters( LoadParameters );

            FTextureResource * Metallic = GResourceManager->GetResource< FTextureResource >( ( MetallicMap[i] ) );
            LoadParameters.SRGB = false;
            Metallic->SetLoadParameters( LoadParameters );

            FTextureResource * Roughness = GResourceManager->GetResource< FTextureResource >( ( RoughnessMap[i] ) );
            LoadParameters.SRGB = false;
            Roughness->SetLoadParameters( LoadParameters );

            FMaterialInstance * MaterialInstance = Material->CreateInstance();
            MaterialInstance->Set( MaterialInstance->AddressOf( "SmpBaseColor" ), BaseColor );
            MaterialInstance->Set( MaterialInstance->AddressOf( "SmpNormal" ), Normal );
            MaterialInstance->Set( MaterialInstance->AddressOf( "SmpMetallic" ), Metallic );
            MaterialInstance->Set( MaterialInstance->AddressOf( "SmpRoughness" ), Roughness );
            
            Mesh->SetMaterialInstance( MaterialInstance );
            Mesh->SetMesh( MeshResource );
            Mesh->SetDrawRange( Offset.IndexCount, Offset.StartIndexLocation, Offset.BaseVertexLocation );
        }
    }
#endif

#if 0
    FStaticMeshComponent * Renderable = Node->CreateComponent< FStaticMeshComponent >();
    FStaticMeshResource * Mesh = GResourceManager->GetResource< FStaticMeshResource >( "*box*" );
    FMaterialResource * Material = GResourceManager->GetResource< FMaterialResource >( "Blade/StandardMaterial.json" );
    Material->Load();

    Renderable->SetMesh( Mesh );
    Renderable->EnableShadowCast( true );

    FTextureResource * Texture = GResourceManager->GetResource< FTextureResource >( "Blade/mipmapchecker.png" );//World.Faces[ 0 ]->TextureName.Str() );
    //if ( !Texture->Load() ) {
    //    Texture = DefaultTexture;
    //}

    FMaterialInstance * MaterialInstance = Material->CreateInstance();
    MaterialInstance->Set( MaterialInstance->AddressOf( "SmpBaseColor" ), Texture );

    Renderable->SetMaterialInstance( MaterialInstance );

        //    Float4 AmbientColor;
        //    const float ColorNormalizer = 1.0f / 255.0f;
        //    //AmbientColor.X = World.Sectors[ Face->SectorIndex ].AmbientColor[0] * ColorNormalizer;
        //    //AmbientColor.Y = World.Sectors[ Face->SectorIndex ].AmbientColor[1] * ColorNormalizer;
        //    //AmbientColor.Z = World.Sectors[ Face->SectorIndex ].AmbientColor[2] * ColorNormalizer;
        //    AmbientColor.w = World.Sectors[ Face->SectorIndex ].AmbientIntensity;// * 5;

        //    AmbientColor.X = ConvertToRGB( World.Sectors[ Face->SectorIndex ].AmbientColor[ 0 ] * ColorNormalizer );
        //    AmbientColor.Y = ConvertToRGB( World.Sectors[ Face->SectorIndex ].AmbientColor[ 1 ] * ColorNormalizer );
        //    AmbientColor.Z = ConvertToRGB( World.Sectors[ Face->SectorIndex ].AmbientColor[ 2 ] * ColorNormalizer );

        //    #ifndef UNLIT
        //    AmbientColor.w *= 5;
        //    #endif

            MaterialInstance->Set( MaterialInstance->AddressOf( "AmbientColor" ), 0.0f, 0.0f, 0.0f );
        //    WorldRenderable->SetMaterialInstance( MaterialInstance );
        //}
#endif
}

AN_FORCEINLINE float ConvertToRGB( const float & _sRGB ) {
#ifdef SRGB_GAMMA_APPROX
    return pow( _sRGB, 2.2f );
#else
    if ( _sRGB < 0.0f ) return 0.0f;
    if ( _sRGB > 1.0f ) return 1.0f;
    if ( _sRGB <= 0.04045 ) {
        return _sRGB / 12.92f;
    } else {
        return pow( ( _sRGB + 0.055f ) / 1.055f, 2.4f );
    }
#endif
}

AN_FORCEINLINE float ComputeGrayscaleColor( const float & _Red, const float & _Green, const float & _Blue ) {
    // Assume color is in linear space

    return _Red * 0.212639005871510 + _Green * 0.715168678767756 + _Blue * 0.072192315360734;
//  return _Red * 0.212671f         + _Green * 0.715160f         + _Blue * 0.072169f;
}

static void CreateSunLight() {
    if ( !World.HasSky ) {
        return;
    }

    // Create light object
    FSceneNode * Node = Scene->CreateChild( "SunLight" );
    Node->SetPosition( 0, 5, 0 );

    // Attach directional light
    FLightComponent * Light = Node->CreateComponent< FLightComponent >();
    Light->SetType( FLightComponent::T_Direction );

    //Light->SetColor( World.Atmospheres[2].Color[0]/* / 255.0f*/ * World.Atmospheres[2].Intensity,
    //                 World.Atmospheres[2].Color[1]/* / 255.0f*/ * World.Atmospheres[2].Intensity,
    //                 World.Atmospheres[2].Color[2]/* / 255.0f*/ * World.Atmospheres[2].Intensity );
    //Light->SetColor( Float3(ComputeGrayscaleColor(SkyColorAvg.r,SkyColorAvg.g,SkyColorAvg.b) * Tunes.SunBrightness) );
    Light->SetColor( SkyColorAvg * Tunes.SunBrightness );
    Light->SetDirection( Float3( 0.707107f, -0.707107f, 0.0f ) );
    Light->SetRenderMask( 1 );

    //Node = Scene->CreateChild( "EnvMap" );
    //Node->SetPosition( World.Bounds.Center() );
    //FEnvCaptureComponent * EnvCapture = Node->CreateComponent< FEnvCaptureComponent >();
    //EnvCapture->SetBox( World.Bounds.Size() + 1.0f );
    //EnvCapture->SetColor( Float3(0.5f) );
    //EnvCapture->SetWeight( 1.0f );
}

//#define LABYR

static void CreateEnvCaptures() {
    Float4 AmbientColor;
    const float ColorNormalizer = 1.0f / 255.0f;

    for ( int i = 0 ; i < World.Sectors.Length() ; i++ ) {
        BvAxisAlignedBox & AABB = World.Sectors[i].Bounds;

        Float3 Position = AABB.Center();

        AmbientColor.X = ConvertToRGB( World.Sectors[ i ].AmbientColor[ 0 ] * ColorNormalizer );
        AmbientColor.Y = ConvertToRGB( World.Sectors[ i ].AmbientColor[ 1 ] * ColorNormalizer );
        AmbientColor.Z = ConvertToRGB( World.Sectors[ i ].AmbientColor[ 2 ] * ColorNormalizer );

#define SIMULATE_HDRI
#ifdef SIMULATE_HDRI
        const float HDRI_Scale = 4.0f;
        const float HDRI_Pow = 1.1f;
#else
        const float HDRI_Scale = 1.0f;
        const float HDRI_Pow = 1.0f;
#endif

        //AmbientColor *= 0.1f;

        //AmbientColor *= World.Sectors[ i ].AmbientIntensity;
        float Lum = ComputeGrayscaleColor( AmbientColor.X, AmbientColor.Y, AmbientColor.Z );
        if ( Lum < 0.01f ) {
            // too dark
            continue;
        }

        FSceneNode * Node = Scene->CreateChild( "Env" );
        Node->SetPosition( Position );

        bool HasSky = false;
        for ( int f = 0 ; f < World.Sectors[i].Faces.Length() && !HasSky ; f++ ) {
            if ( World.Sectors[i].Faces[f]->Type == FBladeWorld::FT_Skydome ) {
                HasSky = true;
            }
        }

#ifndef LABYR
        // Create IBL
        {
            FEnvCaptureComponent * EnvCapture = Node->CreateComponent< FEnvCaptureComponent >();
            EnvCapture->SetBox( AABB.Size() + 0.011f );
            //EnvCapture->SetBox( AABB.Size() + 0.1f );
            //EnvCapture->SetSphere( Radius+0.011f );
            EnvCapture->SetColor( Float3( AmbientColor * Tunes.AmbientScale ) );
            EnvCapture->SetInnerRange( 0.1f );
            EnvCapture->SetWeight( 1.0f );

            EnvCapture->SetProbeIndex( 0 );
        }
#endif

#if 1

        if ( HasSky ) {
            continue;
        }
        
        float Radius = AABB.Radius();

        if ( Radius < 4.0f ) {
            continue; // too little light
        }

        bool LittleDistance = false;
        Double3 PositionD(Position);
        for ( int f = 0 ; f < World.Sectors[i].Faces.Length() && !LittleDistance ; f++ ) {
            const PlaneD & Plane = World.Sectors[i].Faces[f]->Plane;

            if ( Plane.Dist( PositionD ).Abs() < 0.3 ) {
                LittleDistance = true;
            }
        }

        if ( LittleDistance ) {
            continue;
        }

        // Create point light
        FLightComponent * Light = Node->CreateComponent< FLightComponent >();
        Light->SetType( FLightComponent::T_Point );
        Light->SetOuterRadius( Radius );
        Light->SetInnerRadius( Radius*0.9f );// Radius*0.01f );
        Light->SetColor( Float3( AmbientColor * Tunes.LightScale ) );
#endif
    }

#ifdef LABYR
    FSceneNode * Node = Scene->CreateChild( "EnvMap" );
    Node->SetPosition( World.Bounds.Center() );
    FEnvCaptureComponent * EnvCapture = Node->CreateComponent< FEnvCaptureComponent >();
    EnvCapture->SetBox( World.Bounds.Size() + 1.0f );
    EnvCapture->SetColor( SkyColorAvg * Tunes.SunBrightness * 0.1f );
    EnvCapture->SetWeight( 1.0f );
#endif
}

static void CreateWorldGeometry() {
    BvAxisAlignedBox Bounds;

    // Create default texture
    FTextureResource * DefaultTexture = GResourceManager->GetResource< FTextureResource >( "Blade/mipmapchecker.png" );
    FTextureResource::FLoadParameters LoadParameters;
    LoadParameters.BuildMipmaps = true;
    LoadParameters.SRGB = true;
    DefaultTexture->SetLoadParameters( LoadParameters );
    DefaultTexture->Load();

    // Upload world mesh
    FStaticMeshResource * WorldMesh = GResourceManager->CreateUnnamedResource< FStaticMeshResource >();
    WorldMesh->SetVertexData( World.MeshVertices.ToPtr(), World.MeshVertices.Length(), World.MeshIndices.ToPtr(), World.MeshIndices.Length() );
    WorldMesh->SetMeshOffsets( World.MeshOffsets.ToPtr(), World.MeshOffsets.Length() );

//#define WORLD_AS_ONE_MESH
#ifdef WORLD_AS_ONE_MESH

    // Create materials
    FMaterialResource * Material = GResourceManager->GetResource< FMaterialResource >( "Blade/StandardMaterial.json" );
    Material->Load();

    // Create world object
    FSceneNode * WorldNode = Scene;//Scene->CreateChild( "World" );
    FStaticMeshComponent * WorldRenderable = WorldNode->CreateComponent< FStaticMeshComponent >();
    WorldRenderable->SetMesh( WorldMesh );
    WorldRenderable->SetBounds( World.Bounds );
    WorldRenderable->SetUseCustomBounds( true );
    WorldRenderable->EnableShadowCast( false );

    FMaterialInstance * MaterialInstance = Material->CreateInstance();
    MaterialInstance->Set( MaterialInstance->AddressOf( "SmpBaseColor" ), DefaultTexture );
    WorldRenderable->SetMaterialInstance( MaterialInstance );

#else

    // Create materials
#ifdef UNLIT
    FMaterialResource * Material = GResourceManager->GetResource< FMaterialResource >( "Blade/UnlitMaterial.json" );
#else
    FMaterialResource * Material = GResourceManager->GetResource< FMaterialResource >( "Blade/StandardMaterial.json" );
#endif
    Material->Load();

    FMaterialResource * SkyboxMaterial = GResourceManager->GetResource< FMaterialResource >( "Blade/Skybox.json" );
    SkyboxMaterial->Load();

    FMaterialInstance * SkyboxMaterialInstance = SkyboxMaterial->CreateInstance();
    SkyboxMaterialInstance->Set( SkyboxMaterialInstance->AddressOf( "SmpCubemap" ), SkyboxTexture );

    // Create world object
    FSceneNode * WorldNode = Scene;//Scene->CreateChild( "World" );
    for ( int i = 0 ; i < World.MeshOffsets.Length() ; i++ ) {
        FMeshOffset & Ofs = World.MeshOffsets[i];
        FBladeWorld::FFace * Face = World.MeshFaces[i];

        FStaticMeshComponent * WorldRenderable = WorldNode->CreateComponent< FStaticMeshComponent >();
        WorldRenderable->SetMesh( WorldMesh );
        WorldRenderable->SetDrawRange( Ofs.IndexCount, Ofs.StartIndexLocation, Ofs.BaseVertexLocation );
        Bounds.Clear();
        for ( int k = 0 ; k < Ofs.IndexCount ; k++ ) {
            Bounds.AddPoint( World.MeshVertices[ Ofs.BaseVertexLocation + World.MeshIndices[ Ofs.StartIndexLocation + k ] ].Position );
        }
        WorldRenderable->SetBounds( Bounds );
        WorldRenderable->SetUseCustomBounds( true );
        //WorldRenderable->EnableShadowCast( Face->CastShadows );
        WorldRenderable->EnableShadowCast( false );
        WorldRenderable->SetSurfaceType( SURF_PLANAR );
        WorldRenderable->SetSurfacePlane( PlaneF(Face->Plane) );
        
        if ( Face->Type == FBladeWorld::FT_Skydome || Face->TextureName.Length() == 0 ) {
            WorldRenderable->SetMaterialInstance( SkyboxMaterialInstance );
        } else {

            //if ( Face->TextureName == "blanca" ) {
            //    ...
            //}

            FTextureResource * Texture = GResourceManager->GetResource< FTextureResource >( Face->TextureName.Str() );
            if ( !Texture->Load() ) {
                Texture = DefaultTexture;
            }

            FMaterialInstance * MaterialInstance = Material->CreateInstance();
            MaterialInstance->Set( MaterialInstance->AddressOf( "SmpBaseColor" ), Texture );

            //Float4 AmbientColor;
            //const float ColorNormalizer = 1.0f / 255.0f;
            //AmbientColor.X = ConvertToRGB( World.Sectors[ Face->SectorIndex ].AmbientColor[ 0 ] * ColorNormalizer );
            //AmbientColor.Y = ConvertToRGB( World.Sectors[ Face->SectorIndex ].AmbientColor[ 1 ] * ColorNormalizer );
            //AmbientColor.Z = ConvertToRGB( World.Sectors[ Face->SectorIndex ].AmbientColor[ 2 ] * ColorNormalizer );
            //Float4 Color = AmbientColor * World.Sectors[ Face->SectorIndex ].AmbientIntensity;
            //float Lum = ComputeGrayscaleColor( Color.r,Color.g,Color.b );

            //if ( Lum < 0.01f ) {
            //    MaterialInstance->Set( MaterialInstance->AddressOf( "LightMask" ), (int)0xffffffff & (~1) );
            //} else {
            //    MaterialInstance->Set( MaterialInstance->AddressOf( "LightMask" ), (int)0xffffffff );
            //}

            WorldRenderable->SetMaterialInstance( MaterialInstance );
        }
    }
#endif

    // Shadow caster as single mesh
    if ( World.ShadowCasterMeshOffset.IndexCount > 0 ) {
        FStaticMeshComponent * ShadowCaster = WorldNode->CreateComponent< FStaticMeshComponent >();
        ShadowCaster->SetMesh( WorldMesh );
        ShadowCaster->SetDrawRange( World.ShadowCasterMeshOffset.IndexCount, World.ShadowCasterMeshOffset.StartIndexLocation, World.ShadowCasterMeshOffset.BaseVertexLocation );
        Bounds.Clear();
        for ( int k = 0 ; k < World.ShadowCasterMeshOffset.IndexCount ; k++ ) {
            Bounds.AddPoint( World.MeshVertices[ World.ShadowCasterMeshOffset.BaseVertexLocation + World.MeshIndices[ World.ShadowCasterMeshOffset.StartIndexLocation + k ] ].Position );
        }
        ShadowCaster->SetBounds( Bounds );
        ShadowCaster->SetUseCustomBounds( true );
        ShadowCaster->EnableShadowCast( true );
        ShadowCaster->EnableLightPass( false );
    }

    CreateEnvCaptures();

    // Create chunked mesh for world picking
#ifdef DEBUG_WORLD_PICKING
    ChunkedMesh = WorldNode->CreateComponent< FChunkedMeshComponent >();
    ChunkedMesh->Build();
#endif
}

static void CreateDebugMesh() {
    FSceneNode * Node = Scene->CreateChild( "DebugMesh" );
    FPrimitiveBatchComponent * Prim = Node->CreateComponent< FPrimitiveBatchComponent >();

    Prim->SetColor( 1, 0, 0, 1 );
    //Prim->DrawAABB( World.Bounds );

#ifdef DEBUG_BLADE_MP_SECTORS
    FBladeMap Map;

    LoadMP( Map );

    Prim->SetPrimitive( P_LineLoop );

    for ( int SectorIndex = 0 ; SectorIndex < Map.Sectors.Length() ; SectorIndex++ ) {
        BladeMP::SectorEntry & Sector = Map.Sectors[ SectorIndex ];

        for ( int i = 0 ; i < Sector.Vertices.Length() ; i++ ) {
            BladeMP::SectorEntry::Vertex & Vertex = Sector.Vertices[i];

            Prim->EmitPoint( Vertex.X, Sector.FloorHeight, Vertex.Y );
        }

        Prim->Flush();

        for ( int i = 0 ; i < Sector.Vertices.Length() ; i++ ) {
            BladeMP::SectorEntry::Vertex & Vertex = Sector.Vertices[i];

            Prim->EmitPoint( Vertex.X, Sector.CeilHeight, Vertex.Y );
        }

        Prim->Flush();
    }

    Prim->SetPrimitive( P_Points );
    Prim->SetColor( 1,0,0,1 );
    for ( int i = 0 ; i < World.Vertices.Length() ; i++ ) {
        Prim->DrawCircle( Float3( World.Vertices[i].X*0.001f,-World.Vertices[i].Y*0.001f,-World.Vertices[i].Z*0.001f ), 0.1f );
    }
    Prim->Flush();
#endif

#ifdef DEBUG_BLADE_FACE_PORTAL
    Prim->SetPrimitive( P_LineLoop );
    Prim->SetZTest( true );
    for ( int i = 0 ; i < World.Sectors.Length() ; i++ ) {
        for ( int j = 0 ; j < World.Sectors[i].Faces.Length() ; j++ ) {
            FBladeWorld::FFace * f = World.Sectors[i].Faces[j];
            if ( f->Type == FBladeWorld::FT_Portal ) {
                for ( int k = 0 ; k < f->Indices.Length() ; k++ ) {
                    Double3 & v = World.Vertices[ f->Indices[k] ];
                    Prim->EmitPoint( v.X*0.001f, -v.Y*0.001f, -v.Z*0.001f );
                }
                Prim->Flush();
            }
        }
    }
#endif

#ifdef DEBUG_BLADE_GHOST_SECTORS
    Prim->SetPrimitive( P_LineLoop );
    Prim->SetZTest( false );
    for ( int i = 0 ; i < SF.GhostSectors.Length() ; i++ ) {
        FBladeSF::FGhostSector & Sector = SF.GhostSectors[i];

        for ( int j = 0 ; j < Sector.Vertices.Length() ; j++ ) {
            Float2 & v = Sector.Vertices[ j ];
            Prim->EmitPoint( v.X, Sector.FloorHeight, v.Y );
        }
        Prim->Flush();

        for ( int j = 0 ; j < Sector.Vertices.Length() ; j++ ) {
            Float2 & v = Sector.Vertices[ j ];
            Prim->EmitPoint( v.X, Sector.RoofHeight, v.Y );
        }
        Prim->Flush();
    }
#endif

#ifdef DEBUG_BLADE_CAMERA_RECORDS
    FCameraRecord * Record[4];

    Record[0] = amazona_barbaro;
    Record[1] = barbaro_caballero;
    Record[2] = caballero_enano;
    Record[3] = enano_amazona;

    Prim->SetPrimitive( P_LineStrip );
    Prim->SetZTest( false );
    Prim->SetColor(1,1,1,1);
    for ( int r = 0 ; r < 4 ; r++ ) {
        FCameraRecord * Rec = Record[r];
        for ( int i = 0 ; i < Rec->Frames.Length() ; i++ ) {
            FCameraRecord::FFrame & Frame = Rec->Frames[i];
            Prim->EmitPoint( Frame.Position );
        }
        Prim->Flush();
    }
#endif
}

// TODO: ќптимизировать поиск сектора
static int FindSector( const Double3 & Pos ) {
    bool Inside;
    int SectorIndex = -1;
    EPlaneSide Offset;
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

static void DebugCharacterSelection( float _TimeStep ) {
#ifdef DEBUG_CHARACTER_SELECTION
    static FCameraRecord * Record = &barbaro_caballero[0];
    static float Time = 999999.0f;
    static bool PlayState = false;
    static int curplayer = caballero;
    float PlayTimes[4] = {60,50,60,47}; // from selec.py (FrameCamera[])
    static float PlayTime;

    static FSceneNode * TestRecordNode = NULL;
    if ( !TestRecordNode ) {
        TestRecordNode = Scene->CreateChild( "TestRecord" );
    }

    if ( !PlayState ) {
        bool left = Window->IsKeyDown( Key_LEFTARROW );
        bool right = Window->IsKeyDown( Key_RIGHTARROW );
        switch ( curplayer ) {
            case amazona:
                if ( left ) {
                    Record = &enano_amazona[ 1 ]; PlayState = true; Time = 0; curplayer = enano; PlayTime = PlayTimes[enano];
                }
                if ( right ) {
                    Record = &amazona_barbaro[ 0 ]; PlayState = true; Time = 0; curplayer = barbario; PlayTime = PlayTimes[amazona];
                }
                break;
            case barbario:
                if ( left ) {
                    Record = &amazona_barbaro[ 1 ]; PlayState = true; Time = 0; curplayer = amazona; PlayTime = PlayTimes[amazona];
                }
                if ( right ) {
                    Record = &barbaro_caballero[ 0 ]; PlayState = true; Time = 0; curplayer = caballero; PlayTime = PlayTimes[barbario];
                }
                break;
            case caballero:
                if ( left ) {
                    Record = &barbaro_caballero[ 1 ]; PlayState = true; Time = 0; curplayer = barbario; PlayTime = PlayTimes[barbario];
                }
                if ( right ) {
                    Record = &caballero_enano[ 0 ]; PlayState = true; Time = 0; curplayer = enano;  PlayTime = PlayTimes[caballero];
                }
                break;
            case enano:
                if ( left ) {
                    Record = &caballero_enano[ 1 ]; PlayState = true; Time = 0; curplayer = caballero; PlayTime = PlayTimes[caballero];
                }
                if ( right ) {
                    Record = &enano_amazona[ 0 ]; PlayState = true; Time = 0; curplayer = amazona; PlayTime = PlayTimes[enano];
                }
                break;
        }
    }

    if ( PlayState ) {
        int RecFrameIndex = FMath::Floor( Time );

        if ( Time > PlayTime * Record->UnknownFloat ) {
            Time = PlayTime * Record->UnknownFloat;
            RecFrameIndex = FMath::Floor( Time );
            RecFrameIndex = FMath::Min( RecFrameIndex,  Record->Frames.Length() - 1 );
            TestRecordNode->SetPosition( Record->Frames[RecFrameIndex].Position );
            TestRecordNode->SetRotation( Record->Frames[RecFrameIndex].Rotation );
            CameraNode->SetPosition( TestRecordNode->GetPosition() );
            CameraNode->SetRotation( TestRecordNode->GetRotation() );
            PlayState = false;
        } else if ( RecFrameIndex >= Record->Frames.Length()-1 )  {
            RecFrameIndex = Record->Frames.Length() - 1;
            TestRecordNode->SetPosition( Record->Frames[RecFrameIndex].Position );
            TestRecordNode->SetRotation( Record->Frames[RecFrameIndex].Rotation );
            CameraNode->SetPosition( TestRecordNode->GetPosition() );
            CameraNode->SetRotation( TestRecordNode->GetRotation() );
            PlayState = false;
        } else {
            int NextFrameIndex = RecFrameIndex + 1;
            float Lerp = Time - RecFrameIndex;

            const Float3 & P0 = Record->Frames[RecFrameIndex].Position;
            const Float3 & P1 = Record->Frames[NextFrameIndex].Position;

            const FQuat & R0 = Record->Frames[RecFrameIndex].Rotation;
            const FQuat & R1 = Record->Frames[NextFrameIndex].Rotation;

            TestRecordNode->SetPosition( FMath::Mix( P0, P1, Lerp ) );
            TestRecordNode->SetRotation( FMath::Slerp( R0, R1, Lerp ) );

            CameraNode->SetPosition( TestRecordNode->GetPosition() );
            CameraNode->SetRotation( TestRecordNode->GetRotation() );

            float Scale = FMath::Mix(Record->Frames[RecFrameIndex].TimeScale, Record->Frames[NextFrameIndex].TimeScale, Lerp );

            Time += _TimeStep*Scale*0.1f*1000;
        }
    }
#endif
}

static void DebugWorldPicking() {
#ifdef DEBUG_WORLD_PICKING
    ImVec2 MousePos = ImGui::GetMousePos();
    FSegment Segment = Camera->RetrieveRay( MousePos.X, MousePos.Y );
    FChunkedMeshComponent::FRaycastResult Result;

    bool Intersected = ChunkedMesh->Raycast( Segment.start, Segment.end, Result );
    if ( Intersected ) {

        Float3 Vec = ( Segment.end - Segment.start ) * Result.Distance;
        float VecLength = FMath::Length( Vec );
        const float SphereRadius = 0.6f;
        Float3 Pos = Segment.start + ( VecLength > 0.0f ? ( Vec / VecLength ) * ( VecLength - 0.01f ) : Float3(0.0f) ); // Shift little epsilon

        static FMaterialInstance * PickerMaterial = NULL;
        if ( !PickerMaterial ) {
            FMaterialResource * Material = GResourceManager->GetResource< FMaterialResource >( "Blade/StandardMaterial.json" );
            PickerMaterial = Material->CreateInstance();
            //PickerMaterial->Set( PickerMaterial->AddressOf( "SmpBaseColor" ), DefaultTexture );
            Float4 AmbientColor(1.0f);
            PickerMaterial->Set( PickerMaterial->AddressOf( "AmbientColor" ), AmbientColor );
        }

        static FSceneNode * Mover = NULL;
        if ( !Mover ) {
            FStaticMeshResource * Mesh = GResourceManager->GetResource< FStaticMeshResource >( "*sphere*" );
            Mover = Scene->CreateChild( "Mover" );
            Mover->SetScale( SphereRadius );

            FMaterialResource * Material = GResourceManager->GetResource< FMaterialResource >( "Blade/StandardMaterial.json" );
            Material->Load();

            FTextureResource * DefaultTexture = GResourceManager->GetResource< FTextureResource >( "Blade/mipmapchecker.png" );
            FTextureResource::FLoadParameters LoadParameters;
            LoadParameters.SRGB = true;
            LoadParameters.BuildMipmaps = true;
            DefaultTexture->SetLoadParameters( LoadParameters );

            PickerMaterial = Material->CreateInstance();
            PickerMaterial->Set( PickerMaterial->AddressOf( "SmpBaseColor" ), DefaultTexture );

            FStaticMeshComponent * StaticMesh = Mover->CreateComponent< FStaticMeshComponent >();
            StaticMesh->SetMesh( Mesh );
            StaticMesh->SetMaterialInstance( PickerMaterial );
        }
        Mover->SetPosition( Pos );
        int Sector = FindSector( Pos );
        if ( Sector >= 0 ) {
            Float4 AmbientColor;
            AmbientColor.X = World.Sectors[ Sector ].AmbientColor[0] / 255.0f;
            AmbientColor.Y = World.Sectors[ Sector ].AmbientColor[1] / 255.0f;
            AmbientColor.Z = World.Sectors[ Sector ].AmbientColor[2] / 255.0f;
            AmbientColor.w = World.Sectors[ Sector ].AmbientIntensity;// * 5;

#ifndef UNLIT
            AmbientColor.w *= 5;
#endif

            PickerMaterial->Set( PickerMaterial->AddressOf( "AmbientColor" ), AmbientColor );
        }
    }
#endif
}

static void DebugKeypress( float _TimeStep ) {
    if ( Window->IsKeyPressed( Key_F, false ) ) {
        r_faceCull.SetBool( !r_faceCull.GetBool() );
    }
    if ( Window->IsKeyPressed( Key_Y, false ) ) {
        if ( RenderTarget->GetRenderMode() == ERenderMode::SolidTriangles ) {
            RenderTarget->SetRenderMode( ERenderMode::Solid );
        } else {
            RenderTarget->SetRenderMode( ERenderMode::SolidTriangles );
        }
    }
    if ( Window->IsKeyPressed( Key_3, false ) ) {
        if ( RenderTarget->GetAmbientOcclusionMode() != EAmbientOcclusion::OFF ) {
            RenderTarget->SetAmbientOcclusionMode( EAmbientOcclusion::OFF );
        } else {
            RenderTarget->SetAmbientOcclusionMode( EAmbientOcclusion::SSAO16 );
        }
    }
    if ( Window->IsKeyPressed( Key_4, false ) ) {
        RenderTarget->SetColorGradingEnabled( !RenderTarget->IsColorGradingEnabled() );
    }

    if ( Window->IsKeyPressed( Key_5, false ) ) {
        r_spatialCull.ToggleBool();
    }

    if ( World.HasSky ) {
        const float TurnSpeed = _TimeStep;

        if ( Window->IsKeyDown( Key_LEFTARROW ) ) {
            FSceneNode * Node = Scene->FindChild( "SunLight" );
            Node->TurnLeftFPS( TurnSpeed );
        }
        if ( Window->IsKeyDown( Key_RIGHTARROW ) ) {
            FSceneNode * Node = Scene->FindChild( "SunLight" );
            Node->TurnRightFPS( TurnSpeed );
        }
        if ( Window->IsKeyDown( Key_UPARROW ) ) {
            FSceneNode * Node = Scene->FindChild( "SunLight" );
            Node->TurnUpFPS( TurnSpeed );
        }
        if ( Window->IsKeyDown( Key_DOWNARROW ) ) {
            FSceneNode * Node = Scene->FindChild( "SunLight" );
            Node->TurnDownFPS( TurnSpeed );
        }
    }

    if ( Window->IsKeyPressed( Key_0, false ) ) {
        CreateEnvCaptures();
    }
}

static void UpdateCameraAngles( FMouseMoveEvent & _Event ) {
    static Float Yaw = 0.0;
    static Float Pitch = 0.0;
    const Float RotationSpeed = 0.1f;
    Yaw -= _Event.DeltaX * RotationSpeed;
    Pitch -= _Event.DeltaY * RotationSpeed;
    Pitch = Pitch.Clamp( -90.0f, 90.0f );
    CameraNode->SetAngles( Pitch, Yaw, 0 );
}

static void UpdateCameraMovement( float _TimeStep ) {
    const float SpeedScale = 5.0f;
    const float Boost = 8.0f;

    float MoveSpeed = _TimeStep * SpeedScale;

    if ( Window->IsKeyDown( Key_LSHIFT ) ) {
        MoveSpeed *= Boost;
    }

    if ( Window->IsKeyDown( Key_W ) ) {
        CameraNode->SetPosition( CameraNode->GetPosition() + CameraNode->GetForwardVector() * MoveSpeed );
    }

    if ( Window->IsKeyDown( Key_S ) ) {
        CameraNode->SetPosition( CameraNode->GetPosition() + CameraNode->GetBackVector() * MoveSpeed );
    }

    if ( Window->IsKeyDown( Key_A ) ) {
        CameraNode->SetPosition( CameraNode->GetPosition() + CameraNode->GetLeftVector() * MoveSpeed );
    }

    if ( Window->IsKeyDown( Key_D ) ) {
        CameraNode->SetPosition( CameraNode->GetPosition() + CameraNode->GetRightVector() * MoveSpeed );
    }

    if ( Window->IsKeyDown( Key_SPACE ) ) {
        CameraNode->SetPosition( CameraNode->GetPosition() + Float3(0,MoveSpeed,0)  );
    }

    if ( Window->IsKeyDown( Key_C ) ) {
        CameraNode->SetPosition( CameraNode->GetPosition() + Float3(0,-MoveSpeed,0) );
    }
}

static void DebugSectorPortals( const FBladeWorld::FSector & Sector ) {
#ifdef DEBUG_PORTALS
    static FPrimitiveBatchComponent * DebugPortals = NULL;

    if ( !DebugPortals ) {
        FSceneNode * DebugPortalsNode = Scene->CreateChild( "DebugPortals" );
        DebugPortals = DebugPortalsNode->CreateComponent< FPrimitiveBatchComponent >();
    }

    DebugPortals->Clear();
    DebugPortals->SetZTest( false );

    // Draw sector faces
    for ( int j = 0 ; j < Sector.Faces.Length() ; j++ ) {
        FBladeWorld::FFace & f = *Sector.Faces[ j ];
        if ( f.Type == FBladeWorld::FT_Portal ) {
            continue;
        }
        if ( !f.Vertices.Length() ) {
            // Sky or simple face
            DebugPortals->SetColor( 1,1,0,0.2f);
            DebugPortals->SetPrimitive( P_TriangleFan );
            for ( int k = f.Indices.Length()-1 ; k >=0  ; k-- ) {
                Double3 & v = World.Vertices[ f.Indices[ k ] ];

                DebugPortals->EmitPoint( v.X*0.001f, -v.Y*0.001f, -v.Z*0.001f );
            }
        } else {
            // Face with holes
            DebugPortals->SetPrimitive( P_Triangles );
            DebugPortals->SetColor( 1,1,1,0.2f);
            for ( int k = f.Indices.Length()-1 ; k >=0  ; k-- ) {
                Double3 & v = f.Vertices[ f.Indices[ k ] ];

                DebugPortals->EmitPoint( v.X*0.001f, -v.Y*0.001f, -v.Z*0.001f );
            }
        }
        DebugPortals->Flush();
    }

    // Draw sector portals
    for ( int j = 0 ; j < Sector.Portals.Length() ; j++ ) {
        FBladeWorld::FPortal * Portal = Sector.Portals[j];
        DebugPortals->SetColor( 0,0,1,0.2f);
        DebugPortals->SetPrimitive( P_TriangleFan );
        for ( int k = 0 ; k < Portal->Winding.Length() ; k++ ) {
            Double3 & v = Portal->Winding[ k ];
            DebugPortals->EmitPoint( v.X*0.001f, -v.Y*0.001f, -v.Z*0.001f );
        }
        DebugPortals->Flush();
    }

    //DebugPortals->SetPrimitive( P_LineLoop );
    //for ( int j = 0 ; j < Sector.Portals.Length() ; j++ ) {
    //    FBladeWorld::FFace & f = *Sector.Portals[ j ].Face;
    //    for ( int k = 0 ; k < f.Indices.Length() ; k++ ) {
    //        Double3 & v = World.Vertices[ f.Indices[ k ] ];
    //        DebugPortals->EmitPoint( v.X*0.001f, -v.Y*0.001f, -v.Z*0.001f );
    //    }
    //    DebugPortals->Flush();
    //}
#endif
}

void FGame::OnBeforeCommandLineProcessing( TPodArray< char * > & _Arguments ) {
    _Arguments.Append( "-splash" );
    _Arguments.Append( "Blade/splash.png" );
    _Arguments.Append( "-splashOpacity" );
    _Arguments.Append( "0.8" );
}

void FGame::OnSetPrimaryWindowDefs( FWindowDefs & _WindowDefs ) {
    _WindowDefs.Title = "Blade Of Darkness Remake. Powered by Angie Engine";
    _WindowDefs.AutoIconify = true;
    _WindowDefs.Focused = true;
}

void FGame::OnInitialize() {
    LoadConfigFile();

    Window = GetPrimaryWindow();

    Scene = FCore::TNew< FScene >();

    Scene->GetOrCreateComponent< FWorldComponent >();
    Scene->GetOrCreateComponent< FPhysicsComponent >();

    //GAudioSystem->SetMasterGain( 0.0f );
    GAudioSystem->SetMasterGain( 1.0f );

    //FBladeCSV CSV;
    //CSV.LoadCSV( MakePath( "Maps/csv.dat" ) );

    amazona_barbaro[0].LoadRecord( MakePath( "Maps/Casa/Seleccion_Camera_amazona_barbaro.cam" ) );
    amazona_barbaro[1].LoadRecord( MakePath( "Maps/Casa/Seleccion_Camera_barbaro_amazona.cam" ) );

    barbaro_caballero[0].LoadRecord( MakePath( "Maps/Casa/Seleccion_Camera_barbaro_caballero.cam" ) );
    barbaro_caballero[1].LoadRecord( MakePath( "Maps/Casa/Seleccion_Camera_caballero_barbaro.cam" ) );

    caballero_enano[0].LoadRecord( MakePath( "Maps/Casa/Seleccion_Camera_caballero_enano.cam" ) );
    caballero_enano[1].LoadRecord( MakePath( "Maps/Casa/Seleccion_Camera_enano_caballero.cam" ) );

    enano_amazona[0].LoadRecord( MakePath( "Maps/Casa/Seleccion_Camera_enano_amazona.cam" ) );
    enano_amazona[1].LoadRecord( MakePath( "Maps/Casa/Seleccion_Camera_amazona_enano.cam" ) );

    //LoadLevel( MakePath( "Maps/Chaos_M17/chaos.lvl" ) );
    //LoadGhostSectors( MakePath( "Maps/Chaos_M17/ambiente.sf" ) );

    //LoadLevel( MakePath( "Maps/Btomb_M12/btomb.lvl" ) );
    //LoadGhostSectors( MakePath( "Maps/Btomb_M12/ambiente.sf" ) );

    //LoadLevel( MakePath( "Maps/Labyrinth_M6/labyr.lvl" ) );
    //LoadGhostSectors( MakePath( "Maps/Labyrinth_M6/sonidos.sf" ) );

    //LoadLevel( MakePath( "Maps/Orlok_M10/defile.lvl" ) );
    //LoadGhostSectors( MakePath( "Maps/Orlok_M10/exterior.sf" ) );

    //LoadLevel( MakePath( "Maps/Orc_M9/orcst.lvl" ) );
    //LoadGhostSectors( MakePath( "Maps/Orc_M9/orc.sf" ) );

    //LoadLevel( MakePath( "Maps/Ruins_M4/ruins.lvl" ) );
    //LoadGhostSectors( MakePath( "Maps/Ruins_M4/ruinssnd.sf" ) );

    FString SFName = demo_gamelevel.GetString();
    SFName.ReplaceExt( ".sf" );

    FString TunesName = demo_gamelevel.GetString();
    TunesName.StripFilename().Concat( ".txt" );

    Tunes.LoadTunes( ( "Blade/Tunes/" + TunesName ).Str() );

// ok
//Model.LoadModel( "E:/Games/Blade of Darkness/3DObjs/altarPieza3.BOD" );
//Model.LoadModel( "E:/Games/Blade of Darkness/3DObjs/Gargola02.BOD" );
//Model.LoadModel( "E:/Games/Blade of Darkness/3DObjs/lampara.BOD" );
//Model.LoadModel( "E:/Games/Blade of Darkness/3DObjs/longswordpieza1.BOD" );
//Model.LoadModel( "E:/Games/Blade of Darkness/3DObjs/EstatuaGolem.BOD" );
//Model.LoadModel( "E:/Games/Blade of Darkness/3DObjs/Dragon_estatua.BOD" );
//Model.LoadModel( "E:/Games/Blade of Darkness/3DObjs/Gargola_estatua.BOD" );
//Model.LoadModel( "E:/Games/Blade of Darkness/3DObjs/vampweapon.BOD" );
//Model.LoadModel( "E:/Games/Blade of Darkness/3DObjs/globo.BOD" );
//Model.LoadModel( "E:/Games/Blade of Darkness/3DObjs/rastrillo32.BOD" );
//Model.LoadModel( "E:/Games/Blade of Darkness/3DObjs/rastrillo3.BOD" );
//Model.LoadModel( "E:/Games/Blade of Darkness/3DObjs/ArmaduraKF.BOD" );
//Model.LoadModel( "E:/Games/Blade of Darkness/3DObjs/Hacha2hojas.BOD" );
//Model.LoadModel( "E:/Games/Blade of Darkness/3DObjs/Carcaj_Amz_seleccion.BOD" );
//Model.LoadModel( "E:/Games/Blade of Darkness/3DChars/Ork.BOD" );
Model.LoadModel( "E:/Games/Blade of Darkness/3DChars/Skl.BOD" );
//Model.LoadModel( "E:/Games/Blade of Darkness/3DChars/Amz_l.BOD" );
//Model.LoadModel( "E:/Games/Blade of Darkness/3DChars/amz_bng.BOD" );
//Model.LoadModel( "E:/Games/Blade of Darkness/3DChars/Amz_N.BOD" );
//Model.LoadModel( "E:/Games/Blade of Darkness/3DChars/Amzskin1.BOD" );
//Model.LoadModel( "E:/Games/Blade of Darkness/3DChars/Amzskin2.BOD" );
//Model.LoadModel( "E:/Games/Blade of Darkness/3DChars/Ank.BOD" );
//Model.LoadModel( "E:/Games/Blade of Darkness/3DChars/Ank_Bng.BOD" );
//Model.LoadModel( "E:/Games/Blade of Darkness/3DChars/Ank2.BOD" );
//Model.LoadModel( "E:/Games/Blade of Darkness/3DChars/Bar.BOD" );
//Model.LoadModel( "E:/Games/Blade of Darkness/3DChars/Bar_bng.BOD" );
//Model.LoadModel( "E:/Games/Blade of Darkness/3DChars/Bar_L.BOD" );
//Model.LoadModel( "E:/Games/Blade of Darkness/3DChars/Bar_N.BOD" );
//Model.LoadModel( "E:/Games/Blade of Darkness/3DChars/Barskin1.BOD" );
//Model.LoadModel( "E:/Games/Blade of Darkness/3DChars/Barskin1.BOD" );
//Model.LoadModel( "E:/Games/Blade of Darkness/3DChars/bat.BOD" );
//Model.LoadModel( "E:/Games/Blade of Darkness/3DChars/Gargoyle_Stone_Form.BOD" );
//Model.LoadModel( "E:/Games/Blade of Darkness/3DChars/Kgt_F.BOD" );
//Model.LoadModel( "E:/Games/Blade of Darkness/3DObjs/Flecha_Amz_seleccion.BOD" );
//Model.LoadModel( "E:/Games/Blade of Darkness/3DObjs/HacharrajadaPieza3.BOD" );
//Model.LoadModel( "E:/Games/Blade of Darkness/3DObjs/llavedoblada.BOD" );

//Model.LoadModel( "E:/Games/Blade of Darkness/3DObjs/trono.BOD" );
//Model.LoadModel( "E:/Games/Blade of Darkness/3DObjs/firybally.BOD" );
//Model.LoadModel( "E:/Games/Blade of Darkness/3DObjs/leon.BOD" );
//Model.LoadModel( "E:/Games/Blade of Darkness/3DObjs/reyaure.BOD" );

    
    
    
    
    
    
    
    

    
    
    

    
    
    
    LoadLevel( MakePath( demo_gamelevel.GetString() ) );
    LoadGhostSectors( MakePath( SFName.Str() ) );
    LoadMusic();
    CreateAreasAndPortals();
    CreateCamera();
    CreateSunLight();
    CreateWorldGeometry();
    CreateDebugMesh();

    Scene->SetDebugDrawFlags( 0 );// EDebugDrawFlags::DRAW_LIGHTS );
    //Scene->SetDebugDrawFlags( EDebugDrawFlags::DRAW_ENV_CAPTURE );

    RenderTexture = GResourceManager->CreateUnnamedResource< FTextureResource >();
    RenderTexture->CreateRenderTarget();
    RenderTarget = RenderTexture->GetRenderTarget();
    RenderTarget->SetEraseBackground( true );
    RenderTarget->SetBackgroundColor( Float4(0.4f,0.4f,1.0f,1) );
    RenderTarget->SetWireframeColor( Float4(1) );
    RenderTarget->SetWireframeLineWidth( 2.0f );
    RenderTarget->SetRenderMode( ERenderMode::Solid );
    RenderTarget->SetEraseDepth( true );
    RenderTarget->SetViewCamera( Camera );
    RenderTarget->SetCullCamera( Camera );
    RenderTarget->SetColorGradingEnabled( demo_colorgrading.GetBool() );

    RenderTarget->SetAmbientOcclusionMode( demo_ssao.GetBool() ? EAmbientOcclusion::SSAO16 : EAmbientOcclusion::OFF );
    if ( SkyboxTexture ) {
        FTextureResource * WhiteCubemap = CreateWhiteCubemap();
        const FTextureResource * EnvProbes[2] = { WhiteCubemap, SkyboxTexture };

        FTextureResource * EnvProbeArray = GResourceManager->CreateUnnamedResource< FTextureResource >();
        EnvProbeArray->GenerateEnvProbes( 7, 2, EnvProbes );

        RenderTarget->SetEnvironmentProbeArray( EnvProbeArray );
    }

    GPlatformPort->CloseSplashScreen();

    Window->SetTexture( RenderTexture );
    if ( !demo_fullscreen.GetBool() ) {
        FMonitor * Monitor = GPlatformPort->GetPrimaryMonitor();
        int w = Monitor->Modes[Monitor->CurrentVideoMode].Width;
        int h = Monitor->Modes[Monitor->CurrentVideoMode].Height;
        Window->GetPlatformWindow()->SetPosition( (w - demo_width.GetInteger())/2, (h - demo_height.GetInteger())/2 );
        Window->GetPlatformWindow()->Resize( demo_width.GetInteger(), demo_height.GetInteger() );
    } else {
        Window->GetPlatformWindow()->SetMonitor( GPlatformPort->GetPrimaryMonitor(), 0, 0, demo_width.GetInteger(), demo_height.GetInteger(), 120 );
    }
    Window->GetPlatformWindow()->SetVisible(true);
    Window->E_OnKeyPress.Subscribe( this, &FGame::OnKeyPress );
    Window->E_OnKeyRelease.Subscribe( this, &FGame::OnKeyRelease );
    Window->E_OnChar.Subscribe( this, &FGame::OnChar );
    Window->E_OnMouseMove.Subscribe( this, &FGame::OnMouseMove );
    Window->E_OnUpdateGui.Subscribe( this, &FGame::OnUpdateGui );
    Window->SetCursorMode( ECursorMode::Disabled );
    //Window->SetSwapControlMode( ESwapControl::Adaptive );
    Window->SetSwapControlMode( ESwapControl::Synchronized );

    ImGui_Create( Window );
}

void FGame::OnShutdown() {
    Scene.Reset();

    ImGui_Release();
}

void FGame::OnKeyPress( FKeyPressEvent & _Event ) {
    ImGui_KeyPress( _Event );

    if ( _Event.KeyCode == Key_ESCAPE ) {
        Terminate();
    }
}

void FGame::OnKeyRelease( FKeyReleaseEvent & _Event ) {
    ImGui_KeyRelease( _Event );
}

void FGame::OnChar( FCharEvent & _Event ) {
    ImGui_Char( _Event );
}

void FGame::OnMouseMove( FMouseMoveEvent & _Event ) {
    ImGui_MouseMove( _Event );

    UpdateCameraAngles( _Event );
}

void FGame::OnUpdateGui( FUpdateGuiEvent & _Event ) {
    ImGui_BeginUpdate();

    ImGui::SetMouseCursor( ImGuiMouseCursor_None );

#if 0
    ImGui::SetNextWindowPos(ImVec2(0,0));
    ImGui::SetNextWindowSize( ImVec2(RenderTexture->GetWidth(),RenderTexture->GetHeight()) );
    ImGui::PushStyleVar( ImGuiStyleVar_WindowRounding, 0.0f );
    ImGui::PushStyleColor( ImGuiCol_WindowBg, ImVec4(0,0,0,0) );
    if ( ImGui::Begin( "Background", NULL, ImGuiWindowFlags_NoFocusOnAppearing
        | ImGuiWindowFlags_NoCollapse
        | ImGuiWindowFlags_NoInputs
        | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoSavedSettings
        | ImGuiWindowFlags_NoScrollbar
        | ImGuiWindowFlags_NoScrollWithMouse
        | ImGuiWindowFlags_NoTitleBar ) ) {

        ImGui::Text( "FPS %d", RenderTarget->GetFPSAvg() );
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
#endif
#if 1
    ImGui::SetNextWindowPos(ImVec2(0,0));
    ImGui::SetNextWindowSize( ImVec2(650,150) );
    ImGui::PushStyleVar( ImGuiStyleVar_WindowRounding, 0.0f );
    if ( ImGui::Begin( "Background", NULL, ImGuiWindowFlags_NoFocusOnAppearing
        | ImGuiWindowFlags_NoCollapse
        | ImGuiWindowFlags_NoInputs
        | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoSavedSettings
        | ImGuiWindowFlags_NoScrollbar
        | ImGuiWindowFlags_NoScrollWithMouse
        | ImGuiWindowFlags_NoTitleBar ) ) {

        ImGui::SetWindowFontScale( 1.5f );
        ImGui::Text( "Powered by Angie Engine (C)" );
        ImGui::Text( "Programmed by Alexander Samusev (0xc0de)" );
        ImGui::TextColored( ImVec4(0.5f,0.5f,1,1), "http://vk.com/bladeremake" );
        ImGui::TextColored( ImVec4(0.5f,0.5f,1,1), "http://twitter.com/BladeRemake" );
        ImGui::TextColored( ImVec4(0.5f,0.5f,1,1), "http://www.youtube.com/channel/UC7EK28QMYfy1Ke6tan6pTwg" );
        ImGui::TextColored( ImVec4(0.5f,0.5f,1,1), "http://alsamusev-lostlands.blogspot.ru" );
    }
    ImGui::End();
    ImGui::PopStyleVar();
#endif
#if 0
    ImGui::SetNextWindowPos(ImVec2(0,0));
    ImGui::SetNextWindowSize( ImVec2(RenderTexture->GetWidth(),RenderTexture->GetHeight()) );
    ImGui::PushStyleVar( ImGuiStyleVar_WindowRounding, 0.0f );
    ImGui::PushStyleColor( ImGuiCol_WindowBg, ImVec4(0,0,0,0) );
    if ( ImGui::Begin( "Background", NULL, ImGuiWindowFlags_NoFocusOnAppearing
        | ImGuiWindowFlags_NoCollapse
        | ImGuiWindowFlags_NoInputs
        | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoSavedSettings
        | ImGuiWindowFlags_NoScrollbar
        | ImGuiWindowFlags_NoScrollWithMouse
        | ImGuiWindowFlags_NoTitleBar ) ) {

        ImGui::SetWindowFontScale( 4 );
        if ( r_spatialCull.GetBool() ) {
            ImGui::Text( "VSD ON" );
        } else {
            ImGui::Text( "VSD OFF" );
        }        
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
#endif
#if 0
    if ( ImGui::Begin( "Options" ) ) {

        static float MasterGain = GAudioSystem->GetMasterGain();
        if ( ImGui::SliderFloat( "Mater Gain", &MasterGain, 0, 1 ) ) {
            GAudioSystem->SetMasterGain( MasterGain );
        }

        static float MusicVolume = 1.0f;
        static float SoundVolume = 1.0f;

        if ( ImGui::SliderFloat( "Music Volume", &MusicVolume, 0, 1 ) ) {
            FAudioComponent * ac = Scene->GetComponent< FAudioComponent >();
            ac->SetGainScale( SND_MUSIC, MusicVolume );
        }

        if ( ImGui::SliderFloat( "Sound Volume", &SoundVolume, 0, 1 ) ) {
            FAudioComponent * ac = Scene->GetComponent< FAudioComponent >();
            ac->SetGainScale( SND_SOUND, SoundVolume );
        }

        static float Brightness = 0.5f;
        if ( ImGui::SliderFloat( "Brightness", &Brightness, 0.0f, 1.0f ) ) {
            RenderTarget->SetBrightness( Brightness );
        }

        static float Gamma = RenderTarget->GetColorGradingGamma().X;
        if ( ImGui::DragFloat( "Gamma", &Gamma, 0.01f, 0.0f, 1.0f ) ) {
            RenderTarget->SetColorGradingGamma( Float3(Gamma) );
        }

        static float Lift = RenderTarget->GetColorGradingLift().X;
        if ( ImGui::DragFloat( "Lift", &Lift, 0.01f, 0.0f, 1.0f ) ) {
            RenderTarget->SetColorGradingLift( Float3(Lift) );
        }

        static float Presat = RenderTarget->GetColorGradingPresaturation().X;
        if ( ImGui::DragFloat( "Presat", &Presat, 0.01f, 0.0f, 1.0f ) ) {
            RenderTarget->SetColorGradingPresaturation( Float3(Presat) );
        }

        static float Temperature = RenderTarget->GetColorGradingTemperature();
        if ( ImGui::DragFloat( "Temperature", &Temperature, 50.f, 1000.0f, 40000.0f ) ) {
            RenderTarget->SetColorGradingTemperature( Temperature );
        }

        static float Strength = RenderTarget->GetColorGradingTemperatureStrength().X;
        if ( ImGui::DragFloat( "Strength", &Strength, 0.01f, 0.0f, 1.0f ) ) {
            RenderTarget->SetColorGradingTemperatureStrength( Float3(Strength) );
        }

        static float BrightNorm = RenderTarget->GetColorGradingBrightnessNormalization();
        if ( ImGui::DragFloat( "BrightNorm", &BrightNorm, 0.01f, 0.0f, 1.0f ) ) {
            RenderTarget->SetColorGradingBrightnessNormalization( BrightNorm );
        }
    
#ifdef DEBUG_MONITOR_GAMMA
        FGammaRamp Ramp;
        static unsigned short GammaRamp[256];
        Ramp.Red = GammaRamp;
        Ramp.Green = GammaRamp;
        Ramp.Blue = GammaRamp;
        Ramp.Size = 256;

        bool UpdateGamma = false;
        
        static float Gamma = 1.0f;
        if ( ImGui::SliderFloat( "Gamma", &Gamma, 0.5f, 3.0f ) ) {
            UpdateGamma = true;
        }

        static float Brightness = 0.5f;
        if ( ImGui::SliderFloat( "Brightness", &Brightness, 0.5f, 100.0f ) ) {
            UpdateGamma = true;
        }

        static float Overbright = 0.0f;
        if ( ImGui::SliderFloat( "Overbright", &Overbright, 0.0f, 10.0f ) ) {
            UpdateGamma = true;
        }

        for ( int i = 0 ; i < 256 ; i++ ) {
            float f = ( 255.0f * pow( i / 255.0f, 1.0f / Gamma ) + Brightness ) * ( Overbright + 1 );

            f = FMath::Clamp( f, 0.0f, 255.0f );

            GammaRamp[ i ] = ( unsigned short )( (int)( f ) << 8 );
        }

        if ( UpdateGamma ) {
            GPlatformPort->SetGammaRamp( *GPlatformPort->GetPrimaryMonitor(), &Ramp );
        }
#endif
    }
    ImGui::End();
#endif
    ImGui_EndUpdate();
}

void FGame::OnUpdate( float _TimeStep ) {
    static int PrevSectorIndex = -1;

    DebugKeypress( _TimeStep );
    DebugCharacterSelection( _TimeStep );
    DebugWorldPicking();
    UpdateCameraMovement( _TimeStep );

    int SectorIndex = FindSector( Double3( CameraNode->GetPosition() ) );
    if ( SectorIndex >= 0 && SectorIndex != PrevSectorIndex ) {
        FBladeWorld::FSector & Sector = World.Sectors[SectorIndex];

        PrevSectorIndex = SectorIndex;

        // Update color grading inside the sector
#if 1
        Float3 Color;
        byte YCoCg[3], RGB[3];
        FColorSpace::RGBToYCoCg( Sector.AmbientColor, YCoCg );
        YCoCg[0] = 128;//255;
        FColorSpace::YCoCgToRGB( YCoCg, RGB );
        Color.X = RGB[0] / 255.0f;
        Color.Y = RGB[1] / 255.0f;
        Color.Z = RGB[2] / 255.0f;

        RenderTarget->SetColorGradingGamma( Float3( 0.624f ) );
        RenderTarget->SetColorGradingLift( Float3( 0.472f ) );
        RenderTarget->SetColorGradingPresaturation( Float3( 1.0f ) );
        RenderTarget->SetColorGradingTemperature( 4601.678f );
        RenderTarget->SetColorGradingTemperatureStrength( Float3( 1.0f ) );
        RenderTarget->SetColorGradingBrightnessNormalization( 0.0f );
        RenderTarget->SetColorGradingGrain( Color );
        RenderTarget->SetColorGradingLUT( NULL );
#else
        //byte YCoCg[3];
        //FColorSpace::RGBToYCoCg( Sector.AmbientColor, YCoCg );
        Float3 Color( FMath::Clamp( Sector.AmbientColor[0] / 255.0f * 0.3f + Sector.AmbientColor[1] / 255.0f * 0.4f + Sector.AmbientColor[2] / 255.0f * 0.3f - 0.5f, 0.001f, 0.5f ) );

        //RenderTarget->SetTonemappingExposure( pow( (1.0f-Color.r) * 4.0f, 4.0 ) );
        RenderTarget->SetColorGradingGrain( Color );
        RenderTarget->SetColorGradingLUT( NULL );
#endif

        // Draw some debug info
        DebugSectorPortals( Sector );
    }

    Scene->Update( NULL, Camera, _TimeStep );  
}

AN_APP_ENTRY_DECL( FGame )
