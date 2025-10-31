/*

Open source re-implementation of Blade Of Darkness.

MIT License

Copyright (C) 2025 Alexander Samusev.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

#include <Hork/Runtime/GameApplication/GameApplication.h>
#include <Hork/Runtime/UI/UIViewport.h>
#include <Hork/Runtime/World/Modules/Render/Components/MeshComponent.h>
#include <Hork/Runtime/World/Modules/Render/Components/DirectionalLightComponent.h>
#include <Hork/Runtime/World/Modules/Render/RenderInterface.h>
#include <Hork/Runtime/World/Modules/Input/InputInterface.h>
#include <Hork/Runtime/World/Modules/Audio/AudioInterface.h>
#include <Hork/Runtime/World/Modules/Audio/Components/SoundSource.h>
#include <Hork/Runtime/World/DebugRenderer.h>
#include <Hork/Resources/Sound.h>
#include <Hork/RenderUtils/Utilites.h>
#include <Hork/Geometry/TangentSpace.h>

#include "Level.h"
#include "DataFormats/SF.h"
#include "DataFormats/BOD.h"
#include "DataFormats/BMV.h"
#include "Utils/ConversionUtils.h"

using namespace Hk;

ConsoleVar demo_gamepath("demo_gamepath"_s, "D:\\Games\\Blade Of Darkness"_s);
//ConsoleVar demo_gamelevel("demo_gamelevel"_s, "Maps/Mine_M5/mine.lvl"_s);
ConsoleVar demo_gamelevel("demo_gamelevel"_s, "Maps/Casa/casa.lvl"_s);
ConsoleVar demo_spectatorMoveSpeed("demo_spectatorMoveSpeed"_s, "10"_s);
ConsoleVar demo_music("demo_music"_s, "Sounds/MAPA2.mp3"_s);

class SpectatorComponent : public Component
{
public:
    static constexpr ComponentMode Mode = ComponentMode::Dynamic;

    void BindInput(InputBindings& input)
    {
        input.BindAxis("MoveForward", this, &SpectatorComponent::MoveForward);
        input.BindAxis("MoveRight", this, &SpectatorComponent::MoveRight);
        input.BindAxis("MoveUp", this, &SpectatorComponent::MoveUp);
        input.BindAxis("MoveDown", this, &SpectatorComponent::MoveDown);
        input.BindAxis("TurnRight", this, &SpectatorComponent::TurnRight);
        input.BindAxis("FreelookVertical", this, &SpectatorComponent::FreelookVertical);
        input.BindAxis("FreelookHorizontal", this, &SpectatorComponent::FreelookHorizontal);
    }

    void MoveForward(float amount)
    {
        amount *= demo_spectatorMoveSpeed.GetFloat();
        GetOwner()->Move(GetOwner()->GetForwardVector() * amount * GetWorld()->GetTick().FrameTimeStep);
    }

    void MoveRight(float amount)
    {
        amount *= demo_spectatorMoveSpeed.GetFloat();
        GetOwner()->Move(GetOwner()->GetRightVector() * amount * GetWorld()->GetTick().FrameTimeStep);
    }

    void MoveUp(float amount)
    {
        amount *= demo_spectatorMoveSpeed.GetFloat();
        GetOwner()->Move(Float3::sAxisY() * amount * GetWorld()->GetTick().FrameTimeStep);
    }

    void MoveDown(float amount)
    {
        amount *= demo_spectatorMoveSpeed.GetFloat();
        GetOwner()->Move(Float3::sAxisY() * (-amount) * GetWorld()->GetTick().FrameTimeStep);
    }

    void TurnRight(float amount)
    {
        GetOwner()->Rotate(-amount * GetWorld()->GetTick().FrameTimeStep, Float3::sAxisY());
    }

    void FreelookVertical(float amount)
    {
        GetOwner()->Rotate(amount, GetOwner()->GetRightVector());
    }

    void FreelookHorizontal(float amount)
    {
        GetOwner()->Rotate(-amount, Float3::sAxisY());
    }
};

class SampleApplication;

class DebugRendererComponent : public Component
{
public:
    static constexpr ComponentMode Mode = ComponentMode::Static;

    SampleApplication* App{};

    void DrawDebug(DebugRenderer& renderer);
};

class SampleApplication final : public GameApplication
{
    World*                      m_World{};
    IntrusiveRef<WorldRenderView>m_WorldRenderView;
    BladeLevel                  m_Level;
    BladeSF                     m_SF;

public:
    SampleApplication(ArgumentPack const& args) :
        GameApplication(args, "Open Blade")
    {}

    void Initialize()
    {
        // Set input mappings
        IntrusiveRef<InputMappings> inputMappings(new InputMappings);
        inputMappings->MapAxis(PlayerController::_1, "MoveForward", VirtualKey::W, 1.0f);
        inputMappings->MapAxis(PlayerController::_1, "MoveForward", VirtualKey::S, -1.0f);
        inputMappings->MapAxis(PlayerController::_1, "MoveForward", VirtualKey::Up, 1.0f);
        inputMappings->MapAxis(PlayerController::_1, "MoveForward", VirtualKey::Down, -1.0f);
        inputMappings->MapAxis(PlayerController::_1, "MoveRight", VirtualKey::A, -1.0f);
        inputMappings->MapAxis(PlayerController::_1, "MoveRight", VirtualKey::D, 1.0f);
        inputMappings->MapAxis(PlayerController::_1, "MoveUp", VirtualKey::Space, 1.0f);
        inputMappings->MapAxis(PlayerController::_1, "MoveDown", VirtualKey::C, 1.0f);
        inputMappings->MapAxis(PlayerController::_1, "FreelookVertical", VirtualAxis::MouseVertical, 1.0f);
        inputMappings->MapAxis(PlayerController::_1, "FreelookHorizontal", VirtualAxis::MouseHorizontal, 1.0f);
        inputMappings->MapAxis(PlayerController::_1, "TurnRight", VirtualKey::Left, -90.0f);
        inputMappings->MapAxis(PlayerController::_1, "TurnRight", VirtualKey::Right, 90.0f);

        sGetInputSystem().SetInputMappings(std::move(inputMappings));

        // Set rendering parameters
        m_WorldRenderView.Reset(new WorldRenderView);
        m_WorldRenderView->bDrawDebug = true;
        //m_WorldRenderView->bWireframe = true;

        // Create UI desktop
        UIDesktop* desktop = UINew(UIDesktop);

        // Add viewport to desktop
        UIViewport* viewport;
        desktop->AddWidget(UINewAssign(viewport, UIViewport)
            .SetWorldRenderView(m_WorldRenderView));

        desktop->SetFullscreenWidget(viewport);
        desktop->SetFocusWidget(viewport);

        // Hide mouse cursor
        sGetUIManager().bCursorVisible = false;

        // Add desktop and set current
        sGetUIManager().AddDesktop(desktop);

        // Add shortcuts
        UIShortcutContainer* shortcuts = UINew(UIShortcutContainer);
        shortcuts->AddShortcut(VirtualKey::Escape, {}, {this, &SampleApplication::Quit});
        shortcuts->AddShortcut(VirtualKey::Pause, {}, {this, &SampleApplication::Pause});
        shortcuts->AddShortcut(VirtualKey::P, {}, {this, &SampleApplication::Pause});
        desktop->SetShortcuts(shortcuts);

        // Create game resources
        CreateResources();

        // Create game world
        m_World = CreateWorld();

        // Spawn player
        GameObject* spectator = CreateSpectator(Float3(0, 2, 0), Quat::sIdentity());

        // Set camera for render view
        m_WorldRenderView->SetCamera(spectator->GetComponentHandle<CameraComponent>());
        m_WorldRenderView->SetWorld(m_World);

        // Bind input to the player
        InputInterface& input = m_World->GetInterface<InputInterface>();
        input.BindInput(spectator->GetComponentHandle<SpectatorComponent>(), PlayerController::_1);
        input.SetActive(true);

        RenderInterface& render = m_World->GetInterface<RenderInterface>();
        //render.SetAmbient(0.1f);
        render.SetAmbient(0);

        AudioInterface& audio = m_World->GetInterface<AudioInterface>();
        audio.SetListener(spectator->GetComponentHandle<AudioListenerComponent>());

        CreateScene();
    }

    void Deinitialize()
    {
        DestroyWorld(m_World);
    }

    const uint32_t BATCH_BASE_RESOURCES = 1;
    Vector<ResourceRef> m_Resources;

    void CreateResources()
    {
        auto& resourceMngr = sGetResourceManager();
        auto& materialMngr = sGetMaterialManager();

        materialMngr.LoadLibrary("/Root/default/materials/default.mlib");
        materialMngr.LoadLibrary("/Root/materials/common.mlib");

        // Load resources asynchronously
        m_Resources.EmplaceBack(resourceMngr.LoadAsync<Mesh>(BATCH_BASE_RESOURCES, "/Root/default/box.mesh"));
        m_Resources.EmplaceBack(resourceMngr.LoadAsync<Material>(BATCH_BASE_RESOURCES, "/Root/materials/compiled/sky.mat"));
        m_Resources.EmplaceBack(resourceMngr.LoadAsync<Material>(BATCH_BASE_RESOURCES, "/Root/materials/compiled/wall.mat"));
        m_Resources.EmplaceBack(resourceMngr.LoadAsync<Material>(BATCH_BASE_RESOURCES, "/Root/materials/compiled/shadow_caster.mat"));
        m_Resources.EmplaceBack(resourceMngr.LoadAsync<Material>(BATCH_BASE_RESOURCES, "/Root/default/materials/compiled/default.mat"));
        m_Resources.EmplaceBack(resourceMngr.LoadAsync<Texture>(BATCH_BASE_RESOURCES, "/Root/grid8.webp"));

        resourceMngr.WaitForBatch(BATCH_BASE_RESOURCES);
    }

    GameObject* CreateSpectator(Float3 const& position, Quat const& rotation)
    {
        GameObject* spectator;

        GameObjectDesc desc;
        desc.Name.FromString("Spectator");
        desc.Position = position;
        desc.Rotation = rotation;
        desc.IsDynamic = true;
        m_World->CreateObject(desc, spectator);

        spectator->CreateComponent<SpectatorComponent>();
        spectator->CreateComponent<CameraComponent>();
        spectator->CreateComponent<AudioListenerComponent>();

        return spectator;
    }

    String MakePath(StringView in)
    {
        return demo_gamepath.GetString() / in;
    }

    BladeModel model;
    BladeAnimation anim;
    void LoadAndSpawnModel(StringView fileName, Float3 const& position, Quat const& rotation)
    {
        auto& materialMngr = sGetMaterialManager();

        
        model.Load(fileName);

        // Build mesh
        Vector<Vector<MeshVertex>> vertexBatches(model.Textures.Size());
        Vector<Vector<uint32_t>> indexBatches(model.Textures.Size());
        for (auto& face : model.Faces)
        {
            Vector<MeshVertex>& vertexBatch = vertexBatches[face.TextureNum];
            Vector<uint32_t>& indexBatch = indexBatches[face.TextureNum];

            auto& v0 = model.Vertices[face.Indices[0]];
            auto& v1 = model.Vertices[face.Indices[1]];
            auto& v2 = model.Vertices[face.Indices[2]];

            Double3 faceNormal = Math::Cross(v1.Position - v0.Position, v2.Position - v0.Position);//.Normalized();

            for (int i = 0; i < 3; ++i)
            {
                auto& v = vertexBatch.EmplaceBack();

                v.Position = ConvertCoord(Float3(model.Vertices[face.Indices[i]].Position));
                
#if 0
                Double3 normal = faceNormal;

                auto& modelVertex = model.Vertices[face.Indices[i]];
                for (auto& adjacentFaceNum : modelVertex.Faces)
                {
                    auto& adjacentFace = model.Faces[adjacentFaceNum];
                    if (&adjacentFace != &face)
                    {
                        if ((face.Group & adjacentFace.Group) != 0)
                        {
                            auto& a0 = model.Vertices[adjacentFace.Indices[0]];
                            auto& a1 = model.Vertices[adjacentFace.Indices[1]];
                            auto& a2 = model.Vertices[adjacentFace.Indices[2]];

                            Double3 adjacentFaceNormal = Math::Cross(a1.Position - a0.Position, a2.Position - a0.Position);//.Normalized();

                            normal += adjacentFaceNormal;
                        }
                    }
                }

                

                LOG("NORMAL {} FACE NORMAL {}\n", normal, faceNormal);

                if (normal.Length() < 0.001)
                    normal = faceNormal;//v.SetNormal(ConvertAxis(Float3(model.Vertices[face.Indices[i]].Normal)).Normalized());
                //else
                    v.SetNormal(ConvertAxis(Float3(normal).Normalized()));
#else

                v.SetNormal(ConvertAxis(Float3(model.Vertices[face.Indices[i]].Normal)).Normalized());
#endif

                v.SetTexCoord(face.TexCoords[i]);

                indexBatch.Add(vertexBatch.Size() - 1); // TODO: use mesh optimizer to generate index buffer for the batch
            }
        }

        for (int textureNum = 0, count = model.Textures.Size(); textureNum < count; ++textureNum)
        {
            Vector<MeshVertex>& vertexBatch = vertexBatches[textureNum];
            Vector<uint32_t>& indexBatch = indexBatches[textureNum];

            Geometry::CalcTangentSpace(vertexBatch.ToPtr(), indexBatch.ToPtr(), indexBatch.Size());
        }

        //for (Vector<MeshVertex>& vertexBatch : vertexBatches)
        {
            

            // TODO: use mesh optimizer to optimize vertexBatch
            //       use mesh optimizer to generate index buffer for the batch
        }

        GameObjectDesc desc;
        desc.Position = position;
        desc.Rotation = rotation;
        GameObject* object;
        m_World->CreateObject(desc, object);

        for (int textureNum = 0; textureNum < model.Textures.Size(); ++textureNum)
        {
            const Vector<MeshVertex>& vertexBatch = vertexBatches[textureNum];
            const Vector<uint32_t>& indexBatch = indexBatches[textureNum];

            if (vertexBatch.IsEmpty())
                continue;

            MeshRef surface(new Mesh);

            BvAxisAlignedBox bounds;
            bounds.Clear();
            for (auto& v : vertexBatch)
                bounds.AddPoint(v.Position);

            MeshAllocateDesc alloc;
            alloc.SurfaceCount = 1;
            alloc.VertexCount = vertexBatch.Size();
            alloc.IndexCount = indexBatch.Size();

            surface->Allocate(alloc);
            surface->WriteVertexData(vertexBatch.ToPtr(), vertexBatch.Size(), 0);
            surface->WriteIndexData(indexBatch.ToPtr(), indexBatch.Size(), 0);
            surface->SetBoundingBox(bounds);

            MeshSurface& meshSurface = surface->LockSurface(0);
            meshSurface.BoundingBox = bounds;

            StaticMeshComponent* mesh;
            object->CreateComponent(mesh);
            mesh->SetMesh(surface);
            mesh->SetMaterial(materialMngr.FindMaterial("grid8")); // TODO
            mesh->SetMaterial(m_Level.FindMaterial(model.Textures[textureNum]));
            mesh->SetCastShadow(false);
            mesh->SetLocalBoundingBox(bounds);
        }
    }

    

    void CreateScene()
    {
        m_Level.LoadTextures( MakePath( "3DObjs/3dObjs.mmp" ) );
        m_Level.LoadTextures( MakePath( "3DObjs/bolarayos.mmp" ) );
        m_Level.LoadTextures( MakePath( "3DObjs/CilindroMagico.mmp" ) );
        m_Level.LoadTextures( MakePath( "3DObjs/CilindroMagico2.mmp" ) );
        m_Level.LoadTextures( MakePath( "3DObjs/CilindroMagico3.mmp" ) );
        m_Level.LoadTextures( MakePath( "3DObjs/conos.mmp" ) );
        m_Level.LoadTextures( MakePath( "3DObjs/dalblade.mmp" ) );
        m_Level.LoadTextures( MakePath( "3DObjs/esferagemaazul.mmp" ) );
        m_Level.LoadTextures( MakePath( "3DObjs/esferagemaroja.mmp" ) );
        m_Level.LoadTextures( MakePath( "3DObjs/esferagemaverde.mmp" ) );
        m_Level.LoadTextures( MakePath( "3DObjs/esferanegra.mmp" ) );
        m_Level.LoadTextures( MakePath( "3DObjs/esferaorbital.mmp" ) );
        m_Level.LoadTextures( MakePath( "3DObjs/espectro.mmp" ) );
        m_Level.LoadTextures( MakePath( "3DObjs/firering.mmp" ) );
        m_Level.LoadTextures( MakePath( "3DObjs/genericos.mmp" ) );
        m_Level.LoadTextures( MakePath( "3DObjs/halfmoontrail.mmp" ) );
        m_Level.LoadTextures( MakePath( "3DObjs/luzdivina.mmp" ) );
        m_Level.LoadTextures( MakePath( "3DObjs/magicshield.mmp" ) );
        m_Level.LoadTextures( MakePath( "3DObjs/nube.mmp" ) );
        m_Level.LoadTextures( MakePath( "3DObjs/objetos_p.mmp" ) );
        m_Level.LoadTextures( MakePath( "3DObjs/ondaexpansiva.mmp" ) );
        m_Level.LoadTextures( MakePath( "3DObjs/Pfern.mmp" ) );
        m_Level.LoadTextures( MakePath( "3DObjs/pmiguel.mmp" ) );
        m_Level.LoadTextures( MakePath( "3DObjs/rail.mmp" ) );
        m_Level.LoadTextures( MakePath( "3DObjs/telaranya.mmp" ) );
        m_Level.LoadTextures( MakePath( "3DObjs/vortice.mmp" ) );
        m_Level.LoadTextures( MakePath( "3DObjs/weapons.mmp" ) );
        m_Level.LoadTextures( MakePath( "3DChars/Actors.mmp" ) );
        m_Level.LoadTextures( MakePath( "3DChars/actors_javi.mmp" ) );
        m_Level.LoadTextures( MakePath( "3DChars/ork.mmp" ) );
        m_Level.LoadTextures( MakePath( "3DChars/Bar.mmp" ) );
        m_Level.LoadTextures( MakePath( "3DChars/Kgt.mmp" ) );
        m_Level.LoadTextures( MakePath( "3DChars/Kgtskin1.mmp" ) );
        m_Level.LoadTextures( MakePath( "3DChars/Kgtskin2.mmp" ) );

        m_Level.Load(m_World, MakePath(demo_gamelevel.GetString()));

        

        String ghostSectors = demo_gamelevel.GetString();
        PathUtils::sSetExtensionInplace(ghostSectors, "sf", true);
        
        m_SF.Load(MakePath(ghostSectors));

        //LoadAndSpawnModel(MakePath("3DObjs/Dragon_estatua.BOD"), Float3(-2, 12, 4), Quat::sRotationX(Math::_HALF_PI) /*Quat::sIdentity()*/);
        //LoadAndSpawnModel(MakePath("3DObjs/Gargola_estatua.BOD"), Float3(-2, 2, 4), Quat::sRotationX(Math::_HALF_PI) /*Quat::sIdentity()*/);
        //LoadAndSpawnModel(MakePath("3DObjs/Gargola02.BOD"), Float3(-2, 12, 4), Quat::sRotationX(Math::_HALF_PI) /*Quat::sIdentity()*/);
        //LoadAndSpawnModel(MakePath("3DObjs/EstatuaGolem.BOD"), Float3(-2, 12, 4), Quat::sRotationX(Math::_HALF_PI) /*Quat::sIdentity()*/);
        //LoadAndSpawnModel(MakePath("3DObjs/bigsword.BOD"), Float3(-2, 2, 4), Quat::sRotationX(Math::_HALF_PI) /*Quat::sIdentity()*/);
        LoadAndSpawnModel(MakePath("3DChars/Ork.BOD"), Float3(-2, 2, 4), Quat::sRotationX(Math::_HALF_PI) /*Quat::sIdentity()*/);
        //LoadAndSpawnModel(MakePath("3DObjs/sectorvolcan.BOD"), Float3(-2, 2, 4), Quat::sRotationX(Math::_HALF_PI) /*Quat::sIdentity()*/);
        //LoadAndSpawnModel(MakePath("3DObjs/Hacha2hojas.BOD"), Float3(-2, 2, 4), Quat::sRotationX(Math::_HALF_PI) /*Quat::sIdentity()*/);
        //LoadAndSpawnModel(MakePath("3DObjs/lampara.BOD"), Float3(-2, 2, 4), Quat::sRotationX(Math::_HALF_PI) /*Quat::sIdentity()*/);
        //LoadAndSpawnModel(MakePath("3DObjs/corblade.BOD"), Float3(-2, 2, 4), Quat::sRotationX(Math::_HALF_PI) /*Quat::sIdentity()*/);
        //LoadAndSpawnModel(MakePath("3DObjs/rhinoclub.BOD"), Float3(-2, 2, 4), Quat::sRotationX(Math::_HALF_PI) /*Quat::sIdentity()*/);
        //LoadAndSpawnModel(MakePath("3DObjs/escudonpoly.BOD"), Float3(-2, 2, 4), Quat::sRotationX(Math::_HALF_PI) /*Quat::sIdentity()*/);
        //LoadAndSpawnModel(MakePath("3DObjs/tapizesc.BOD"), Float3(-2, 2, 4), Quat::sRotationX(Math::_HALF_PI) /*Quat::sIdentity()*/);
        //LoadAndSpawnModel(MakePath("3DObjs/espadonPoly.BOD"), Float3(-2, 2, 4), Quat::sRotationX(Math::_HALF_PI) /*Quat::sIdentity()*/);
    
        //anim.Load(MakePath("Anm/Ork_patrol1.BMV"));
        anim.Load(MakePath("Anm/Ork_wlk_1h.BMV"));
        
        //auto sound = sGetResourceManager().CreateResourceFromFile<SoundResource>("/FS/" + MakePath(demo_music.GetString()));

        auto sound = sGetResourceManager().Load<Sound>("/FS/" + MakePath(demo_music.GetString()));

        {
            GameObject* musicPlayer;
            GameObjectDesc desc;
            desc.Name.FromString("MusicPlayer");
            desc.IsDynamic = false;
            m_World->CreateObject(desc, musicPlayer);

            SoundSource* soundSource;
            musicPlayer->CreateComponent(soundSource);
            soundSource->SetSourceType(SoundSourceType::Background);
            soundSource->SetVolume(0.1f);
            soundSource->PlaySound(sound, 0, 0);
        }

        {
            GameObject* debugRenderer;

            GameObjectDesc desc;
            desc.Name.FromString("DebugRenderer");
            desc.IsDynamic = false;
            m_World->CreateObject(desc, debugRenderer);

            DebugRendererComponent* component;
            debugRenderer->CreateComponent(component);
            component->App = this;
        }

        // Spawn directional light
        {
            GameObjectDesc desc;
            desc.IsDynamic = true;

            GameObject* object;
            m_World->CreateObject(desc, object);
            object->SetDirection({1, -1, -1});
            //object->SetDirection({0, -1, 0});

            DirectionalLightComponent* dirlight;
            object->CreateComponent(dirlight);

            dirlight->SetIlluminance(20000.0f);
            dirlight->SetShadowMaxDistance(40);
            dirlight->SetShadowCascadeResolution(2048);
            dirlight->SetShadowCascadeOffset(0.0f);
            dirlight->SetShadowCascadeSplitLambda(0.8f);
        }
    }

    void Pause()
    {
        m_World->SetPaused(!m_World->GetTick().IsPaused);
    }

    void Quit()
    {
        PostTerminateEvent();
    }

    Vector<Float3> m_TempPoints;

    void DrawDebug(DebugRenderer& renderer)
    {
        m_Level.DrawDebug(renderer);

#if 0
        for (auto& sector : m_SF.GhostSectors)
        {
            m_TempPoints.Clear();
            for (auto& v : sector.Vertices)
                m_TempPoints.EmplaceBack(v.X, sector.FloorHeight, v.Y);
            renderer.DrawLine(m_TempPoints, true);

            m_TempPoints.Clear();
            for (auto& v : sector.Vertices)
                m_TempPoints.EmplaceBack(v.X, sector.RoofHeight, v.Y);
            renderer.DrawLine(m_TempPoints, true);

            for (auto& v : sector.Vertices)
                renderer.DrawLine(Float3(v.X, sector.FloorHeight, v.Y), Float3(v.X, sector.RoofHeight, v.Y));
        }
#endif

        // Draw skeleton:

        Vector<Float3x4> localMatrices;
        Vector<Float3x4> absoluteMatrices;

        for (auto& bone : model.Bones)
            localMatrices.Add(ConvertMatrix3x4(bone.Matrix));

        int frameCount = anim.RootMotion.Size();
        int frameNum = int(m_World->GetTick().RunningTime * 10) % frameCount;

        absoluteMatrices.Resize(model.Bones.Size());
        for (int i = 0; i < model.Bones.Size(); ++i)
        {
            auto& node = anim.BoneTransforms[i];

            Quat rotation = node.Keyframes[frameNum];

            Float3 translation = localMatrices[i].DecomposeTranslation();

            if (i == 0)
                translation += Float3(anim.RootMotion[frameNum]);

            localMatrices[i].Compose(translation, rotation.ToMatrix3x3());

            auto& bone = model.Bones[i];
            if (bone.ParentIndex != -1)
            {
                HK_ASSERT(bone.ParentIndex < i);
                absoluteMatrices[i] = absoluteMatrices[bone.ParentIndex] * localMatrices[i];
            }
            else
                absoluteMatrices[i] = localMatrices[i];
        }

        Float3x4 objectMat;
        objectMat.Compose(Float3(0,2,-2), Quat::sRotationX(Math::_HALF_PI).ToMatrix3x3()/*, Float3(0.001)*/);
        for (int i = 0; i < model.Bones.Size(); ++i)
        {
            auto& bone = model.Bones[i];

            if (bone.ParentIndex != -1)
            {
                Float3 p0 = ConvertCoord(absoluteMatrices[i].DecomposeTranslation());
                Float3 p1 = ConvertCoord(absoluteMatrices[bone.ParentIndex].DecomposeTranslation());

                renderer.DrawLine(objectMat*p0, objectMat*p1);
            }
        }
    }
};

void DebugRendererComponent::DrawDebug(DebugRenderer& renderer)
{
    if (App)
        App->DrawDebug(renderer);
}

using ApplicationClass = SampleApplication;
#include "Samples/Source/Common/EntryPoint.h"
