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

#pragma once

#include <Hork/Resources/Resource_Texture.h>
#include <Hork/Runtime/World/World.h>
#include <Hork/Geometry/BV/BvAxisAlignedBox.h>
#include <Hork/Math/Plane.h>
#include <Hork/Geometry/PolyClipper.h>
#include <Hork/Geometry/VertexFormat.h>
#include <Hork/Runtime/Materials/Material.h>
#include "BW.h"

using namespace Hk;

class BladeLevel
{
public:
    void Load(World* world, StringView name);

    void DrawDebug(DebugRenderer& renderer);

private:
    void LoadDome(StringView fileName);
    void LoadTextures(StringView fileName);
    void UnloadTextures();
    void LoadWorld(StringView fileName);
    void CreateWindings_r(Vector<MeshVertex>& vertexBuffer, Vector<uint32_t>& indexBuffer,
        BladeWorld::Face const& face, Vector<Double3> const& winding, BladeWorld::BSPNode const* node, BladeWorld::BSPNode const* texInfo);
    //void BuildGeometry();
    Ref<Material> FindMaterial(StringView name);

    World* m_World;
    Float3 m_SkyColorAvg;
    Vector<TextureHandle> m_Textures;    
    Vector<Ref<Material>> m_Materials;
    //Vector<MeshVertex> m_MeshVertices;
    //Vector<uint32_t> m_MeshIndices;

    //struct Surface
    //{
    //    uint32_t BaseVertexLocation;
    //    uint32_t StartIndexLocation;
    //    uint32_t VertexCount;
    //    uint32_t IndexCount;
    //};
    //Vector<Surface> m_Surfaces;
    //Vector<Face*> m_MeshFaces;

    BladeWorld bw;
};
