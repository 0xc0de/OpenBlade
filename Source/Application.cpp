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
#include <Hork/RenderUtils/Utilites.h>

#include "Level.h"

using namespace Hk;

ConsoleVar demo_gamepath("demo_gamepath"_s, "D:\\Games\\Blade Of Darkness"_s);
//ConsoleVar demo_gamelevel("demo_gamelevel"_s, "Maps/Mine_M5/mine.lvl"_s);
ConsoleVar demo_gamelevel("demo_gamelevel"_s, "Maps/Casa/casa.lvl"_s);
ConsoleVar demo_spectatorMoveSpeed("demo_spectatorMoveSpeed"_s, "10"_s);


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

class SampleApplication final : public GameApplication
{
    World*                      m_World{};
    Ref<WorldRenderView>        m_WorldRenderView;
    BladeLevel                  m_Level;

public:
    SampleApplication(ArgumentPack const& args) :
        GameApplication(args, "Open Blade")
    {}

    void Initialize()
    {
        // Set input mappings
        Ref<InputMappings> inputMappings = MakeRef<InputMappings>();
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

        sGetInputSystem().SetInputMappings(inputMappings);

        // Set rendering parameters
        m_WorldRenderView = MakeRef<WorldRenderView>();
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
        render.SetAmbient(0.1f);

        CreateScene();
    }

    void Deinitialize()
    {
        DestroyWorld(m_World);
    }

    void CreateResources()
    {
        auto& resourceMngr = sGetResourceManager();
        auto& materialMngr = sGetMaterialManager();

        materialMngr.LoadLibrary("/Root/default/materials/default.mlib");
        materialMngr.LoadLibrary("/Root/materials/common.mlib");

        // List of resources used in scene
        ResourceID sceneResources[] = {
            resourceMngr.GetResource<MeshResource>("/Root/default/box.mesh"),
            resourceMngr.GetResource<MaterialResource>("/Root/materials/compiled/sky.mat"),
            resourceMngr.GetResource<MaterialResource>("/Root/materials/compiled/wall.mat"),
            resourceMngr.GetResource<MaterialResource>("/Root/materials/compiled/shadow_caster.mat"),
            resourceMngr.GetResource<MaterialResource>("/Root/default/materials/compiled/default.mat"),
            resourceMngr.GetResource<TextureResource>("/Root/grid8.webp")
        };

        // Load resources asynchronously
        ResourceAreaID resources = resourceMngr.CreateResourceArea(sceneResources);
        resourceMngr.LoadArea(resources);

        // Wait for the resources to load
        resourceMngr.MainThread_WaitResourceArea(resources);
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

        return spectator;
    }

    String MakePath(StringView in)
    {
        return demo_gamepath.GetString() / in;
    }

    void CreateScene()
    {
        m_Level.Load(m_World, MakePath(demo_gamelevel.GetString()));

        // Spawn directional light
        {
            GameObjectDesc desc;
            desc.IsDynamic = true;

            GameObject* object;
            m_World->CreateObject(desc, object);
            object->SetDirection({1, -1, -1});

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
};

using ApplicationClass = SampleApplication;
#include "Samples/Source/Common/EntryPoint.h"
