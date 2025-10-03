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

#include "ImGuiAPI.h"

#include <Engine/Resource/Public/ResourceManager.h>

static ImGuiContext * GUIContext = NULL;
static FTextureResource * GFontTexture = NULL;
static ImFontAtlas * GFontAtlas = NULL;
static FWindow * GWindow = NULL;

void ImGui_Create( FWindow * _Window ) {
    GWindow = _Window;
    GUIContext = ImGui::CreateContext();

    GFontAtlas = new ImFontAtlas;
    //GFontAtlas->AddFontFromFileTTF( "DejaVuSansMono.ttf", 16, NULL, GFontAtlas->GetGlyphRangesCyrillic() );
    unsigned char * Pixels;
    int AtlasWidth, AtlasHeight;
    GFontAtlas->GetTexDataAsRGBA32( &Pixels, &AtlasWidth, &AtlasHeight );

    GFontTexture = GResourceManager->CreateUnnamedResource< FTextureResource >();
    GFontTexture->UploadImage2D( Pixels, AtlasWidth, AtlasHeight, 4, false, false );
    
    GFontAtlas->TexID = GFontTexture;

    ImGuiIO & IO = ImGui::GetIO();
    IO.Fonts = GFontAtlas;
    IO.RenderDrawListsFn = NULL;
    IO.SetClipboardTextFn = []( void *, const char * _Text ) { GPlatformPort->SetClipboard( _Text ); };
    IO.GetClipboardTextFn = []( void * ) { return GPlatformPort->GetClipboard(); };
    IO.ClipboardUserData = NULL;
    //IO.UserData = this;
    IO.ImeWindowHandle = _Window->GetPlatformWindow()->GetHWnd();

    IO.KeyMap[ImGuiKey_Tab] = Key_TAB;
    IO.KeyMap[ImGuiKey_LeftArrow] = Key_LEFTARROW;
    IO.KeyMap[ImGuiKey_RightArrow] = Key_RIGHTARROW;
    IO.KeyMap[ImGuiKey_UpArrow] = Key_UPARROW;
    IO.KeyMap[ImGuiKey_DownArrow] = Key_DOWNARROW;
    IO.KeyMap[ImGuiKey_PageUp] = Key_PGUP;
    IO.KeyMap[ImGuiKey_PageDown] = Key_PGDN;
    IO.KeyMap[ImGuiKey_Home] = Key_HOME;
    IO.KeyMap[ImGuiKey_End] = Key_END;
    IO.KeyMap[ImGuiKey_Delete] = Key_DEL;
    IO.KeyMap[ImGuiKey_Backspace] = Key_BACKSPACE;
    IO.KeyMap[ImGuiKey_Enter] = Key_ENTER;
    IO.KeyMap[ImGuiKey_Escape] = Key_ESCAPE;
    IO.KeyMap[ImGuiKey_A] = Key_A;
    IO.KeyMap[ImGuiKey_C] = Key_C;
    IO.KeyMap[ImGuiKey_V] = Key_V;
    IO.KeyMap[ImGuiKey_X] = Key_X;
    IO.KeyMap[ImGuiKey_Y] = Key_Y;
    IO.KeyMap[ImGuiKey_Z] = Key_Z;

    ImVec2 FramebufferSize(640,480);
    IO.DisplaySize.x = 640;
    IO.DisplaySize.y = 480;
    IO.DisplayFramebufferScale = Float2((float)FramebufferSize.x / IO.DisplaySize.x, (float)FramebufferSize.y / IO.DisplaySize.y);
    IO.DeltaTime = 1.0f / 60.0f;
    IO.MousePos.x = -1;
    IO.MousePos.y = -1;
    for ( int i = 0 ; i < 5 ; i++ ) {
        IO.MouseDown[i] = false;
    }
    IO.MouseWheel = 0;

    ImGui::NewFrame();
}

void ImGui_Release() {
    ImGui::GetIO().Fonts = NULL;

    ImGui::Shutdown();

    ImGui::DestroyContext( GUIContext );
    GUIContext = NULL;

    delete GFontAtlas;
    GFontAtlas = NULL;

    GWindow = NULL;
}

void ImGui_KeyPress( FKeyPressEvent & _Event ) {
    ImGuiIO & IO = ImGui::GetIO();

    if ( _Event.KeyCode == Key_MWHEELUP ) {
        IO.MouseWheel += 1;
    } else if ( _Event.KeyCode == Key_MWHEELDOWN ) {
        IO.MouseWheel -= 1;
    }

    if ( _Event.KeyCode >= 0 && _Event.KeyCode < AN_ARRAY_LENGTH(IO.KeysDown) ) {
        IO.KeysDown[_Event.KeyCode] = true;
    }

    IO.KeyCtrl = IO.KeysDown[Key_LCTRL] || IO.KeysDown[Key_RCTRL];
    IO.KeyShift = IO.KeysDown[Key_LSHIFT] || IO.KeysDown[Key_RSHIFT];
    IO.KeyAlt = IO.KeysDown[Key_LALT] || IO.KeysDown[Key_RALT];
}

void ImGui_KeyRelease( FKeyReleaseEvent & _Event ) {
    ImGuiIO & IO = ImGui::GetIO();

    if ( _Event.KeyCode >= 0 && _Event.KeyCode < AN_ARRAY_LENGTH(IO.KeysDown) ) {
        IO.KeysDown[_Event.KeyCode] = false;
    }

    IO.KeyCtrl = IO.KeysDown[Key_LCTRL] || IO.KeysDown[Key_RCTRL];
    IO.KeyShift = IO.KeysDown[Key_LSHIFT] || IO.KeysDown[Key_RSHIFT];
    IO.KeyAlt = IO.KeysDown[Key_LALT] || IO.KeysDown[Key_RALT];
}

void ImGui_Char( FCharEvent & _Event ) {
    ImGuiIO & IO = ImGui::GetIO();

    if ( _Event.UnicodeCharacter < 0x10000 ) {
        IO.AddInputCharacter( (unsigned short)_Event.UnicodeCharacter );
    }
}

void ImGui_MouseMove( FMouseMoveEvent & _Event ) {
    FMonitor * Monitor = GPlatformPort->GetPrimaryMonitor();

    // Simulate ballistics
    const float RefreshRate = Monitor->Modes[Monitor->CurrentVideoMode].RefreshRate;
    const float MM_To_Inch = 0.0393701f;
    const float DPI_X = (float)Monitor->Modes[Monitor->CurrentVideoMode].Width / (Monitor->PhysicalWidthMM*MM_To_Inch);
    const float DPI_Y = (float)Monitor->Modes[Monitor->CurrentVideoMode].Height / (Monitor->PhysicalHeightMM*MM_To_Inch);

    ImGuiIO & IO = ImGui::GetIO();
    IO.MousePos.x += _Event.DeltaX / RefreshRate * DPI_X;
    IO.MousePos.y += _Event.DeltaY / RefreshRate * DPI_Y;
}

static void RenderDrawLists() {
    ImDrawData * DrawData = ImGui::GetDrawData();
    if ( DrawData->CmdListsCount == 0 ) {
        return;
    }

    // Avoid rendering when minimized, scale coordinates for retina displays (screen coordinates != framebuffer coordinates)
    ImGuiIO & IO = ImGui::GetIO();
    int fb_width = IO.DisplaySize.x * IO.DisplayFramebufferScale.x;
    int fb_height = IO.DisplaySize.y * IO.DisplayFramebufferScale.y;
    if ( fb_width == 0 || fb_height == 0 ) {
        return;
    }

    if ( IO.DisplayFramebufferScale.x != 1.0f || IO.DisplayFramebufferScale.y != 1.0f ) {
        DrawData->ScaleClipRects( IO.DisplayFramebufferScale );
    }

    TArray< FDrawList > & DrawLists = GWindow->GetDrawLists();
    DrawLists.Resize( DrawData->CmdListsCount );

    for ( int n = 0; n < DrawData->CmdListsCount ; n++ ) {
        const ImDrawList * Src = DrawData->CmdLists[ n ];
        FDrawList & Dst = DrawLists[ n ];
        Dst.VerticesCount = Src->VtxBuffer.size();
        Dst.IndicesCount = Src->IdxBuffer.size();
        Dst.Vertices = reinterpret_cast< FGuiVertex * >( Src->VtxBuffer.Data );
        Dst.Indices = Src->IdxBuffer.Data;
        Dst.Cmds.Resize( Src->CmdBuffer.size() );

        FDrawCmd * DstCmd = Dst.Cmds.ToPtr();
        for ( const ImDrawCmd * pCmd = Src->CmdBuffer.begin() ; pCmd != Src->CmdBuffer.end() ; pCmd++, DstCmd++ ) {
            DstCmd->ClipRect = pCmd->ClipRect;
            DstCmd->IndicesCount = pCmd->ElemCount;
            DstCmd->Texture = ( FTextureResource * )pCmd->TextureId;
        }
    }
}

void ImGui_BeginUpdate() {
    Int2 FramebufferSize = GWindow->GetPlatformWindow()->GetFramebufferSize();
    Int2 DisplaySize = GWindow->GetPlatformWindow()->GetSize();

    ImGuiIO & IO = ImGui::GetIO();
   
    IO.DisplaySize.x = DisplaySize.X;
    IO.DisplaySize.y = DisplaySize.Y;
    IO.DisplayFramebufferScale = Float2((float)FramebufferSize.X / IO.DisplaySize.x, (float)FramebufferSize.Y / IO.DisplaySize.y);

    static float PrevTime = GPlatformPort->GetTimeSec();
    float CurTime = GPlatformPort->GetTimeSec();
    IO.DeltaTime = CurTime - PrevTime;
    PrevTime = CurTime;

    IO.MousePos.x = Float( IO.MousePos.x ).Clamp( 0.0f, (float)FramebufferSize.X );
    IO.MousePos.y = Float( IO.MousePos.y ).Clamp( 0.0f, (float)FramebufferSize.Y );
    IO.MouseDrawCursor = true;

    for ( int i = 0 ; i < 5 ; i++ ) {
        IO.MouseDown[ i ] = GWindow->IsKeyDown( Key_MOUSE1 + i );
    }

    ImGui::NewFrame();
}

void ImGui_EndUpdate() {
    ImGui::Render();

    RenderDrawLists();
}
