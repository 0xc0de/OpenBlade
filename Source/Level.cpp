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

#include "Level.h"
#include "Utils/FileDump.h"
#include "Utils/ConversionUtils.h"
#include "DataFormats/BW.h"

#include <Hork/Runtime/GameApplication/GameApplication.h>
#include <Hork/Runtime/World/Modules/Render/Components/MeshComponent.h>
#include <Hork/Runtime/World/DebugRenderer.h>
#include <Hork/Geometry/Triangulator.h>
#include <Hork/Geometry/ConvexHull.h>
#include <Hork/Geometry/TangentSpace.h>

using namespace Hk;

namespace
{
    enum TEXTURE_TYPE
    {
        TT_PALETTE      = 1,
        TT_GRAYSCALED   = 2,
        TT_TRUECOLOR    = 4
    };
}

void BladeLevel::Load(World* world, StringView name)
{
    char str[256];
    char key[256];
    char value[256];

    m_World = world;

    File file = File::sOpenRead(name);
    if (!file)
        return;

    StringView fileLocation = PathUtils::sGetFilePath(name);
    bool skydomeSpecified = false;
    String bwfile;

    //UnloadTextures();
    //FreeWorld(); // TODO

    m_SkyColorAvg.Clear();

    while (file.Gets(str, sizeof(str) - 1))
    {
        key[0] = 0;
        value[0] = 0;

        if (sscanf(str, "%s -> %s", key, value) != 2)
            continue;
        if (!key[0] || !value[0])
            continue;

        String fileName = fileLocation / value;

        PathUtils::sFixPathInplace(fileName);

        if (!Core::Stricmp(key, "Bitmaps"))
        {
            LoadTextures(fileName);
        }
        else if (!Core::Stricmp(key, "WorldDome"))
        {
            LoadDome(fileName);
            skydomeSpecified = true;
        }
        else if (!Core::Stricmp(key, "World"))
        {
            bwfile = fileName;
        }
        else
        {
            LOG("LoadLevel: Unknown key {}\n", key);
        }
    }

    if (!skydomeSpecified)
    {
        // Try to load default dome
        String domeFileName = String(PathUtils::sGetFilenameNoExt(name)) + "_d.mmp";
        LoadDome(domeFileName);
    }

    if (!bwfile.IsEmpty())
        LoadWorld(bwfile);
}

// Load Skydome from .MMP file
void BladeLevel::LoadDome(StringView fileName)
{
    constexpr StringView domeNames[6] =
    {
        "DomeRight",
        "DomeLeft",
        "DomeUp",
        "DomeDown",
        "DomeBack",
        "DomeFront"
    };

    auto& resourceMngr = GameApplication::sGetResourceManager();

    File file = File::sOpenRead(fileName);
    if (!file)
        return;

    int32_t texCount = file.ReadInt32();

    HeapBlob trueColorData;
    TextureRef texture;
    
    for (int i = 0; i < texCount; i++)
    {
        file.ReadInt16(); // unknown
        file.ReadInt32(); // checksum

        int32_t size = file.ReadInt32();
        String textureName = file.ReadString();
        int32_t type = file.ReadInt32();
        int32_t width = file.ReadInt32();
        int32_t height = file.ReadInt32();

        int faceNum;
        for (faceNum = 0; faceNum < 6; ++faceNum)
        {
            if (!textureName.Icmp(domeNames[faceNum]))
                break;
        }

        if (faceNum == 6 || width != height)
        {
            LOG("Invalid dome face\n");
            return;
        }

        if (!texture)
        {
            texture = resourceMngr.Acquire<Texture>("internal_skybox");
            texture->AllocateCubemap(TEXTURE_FORMAT_SRGBA8_UNORM, 1, width);
            trueColorData.Reset(width * height * 4);
        }

        if (texture->GetWidth() != static_cast<uint32_t>(width))
        {
            LOG("Invalid dome face\n");
            return;
        }

        int32_t textureDataSize = size - 12;
        HeapBlob textureData = file.ReadBlob(textureDataSize);

        switch (type)
        {
            case TT_PALETTE:
            {
                const uint8_t* data = reinterpret_cast<const uint8_t*>(textureData.GetData());
                const uint8_t* palette = reinterpret_cast<const uint8_t*>(textureData.GetData()) + width * height;
                uint8_t* trueColor = reinterpret_cast<uint8_t*>(trueColorData.GetData());

                for (int j = 0; j < height; ++j)
                {
                    for (int k = j * width; k < (j + 1) * width; ++k)
                    {
                        trueColor[k * 4    ] = palette[data[k] * 3    ] << 2;
                        trueColor[k * 4 + 1] = palette[data[k] * 3 + 1] << 2;
                        trueColor[k * 4 + 2] = palette[data[k] * 3 + 2] << 2;
                        trueColor[k * 4 + 3] = 255;
                    }
                }
                break;
            }
            case TT_GRAYSCALED:
            {
                const uint8_t* data = reinterpret_cast<const uint8_t*>(textureData.GetData());
                uint8_t* trueColor = reinterpret_cast<uint8_t*>(trueColorData.GetData());
                for (int j = 0; j < height; ++j)
                {
                    for (int k = j * width; k < (j + 1) * width; ++k)
                    {
                        trueColor[k * 4    ] = data[k];
                        trueColor[k * 4 + 1] = data[k];
                        trueColor[k * 4 + 2] = data[k];
                        trueColor[k * 4 + 3] = 255;
                    }
                }
                break;
            }
            case TT_TRUECOLOR:
            {
                const uint8_t* data = reinterpret_cast<const uint8_t*>(textureData.GetData());
                uint8_t* trueColor = reinterpret_cast<uint8_t*>(trueColorData.GetData());
                int count = width * height * 3;
                for (int j = 0, k = 0; j < count; j += 3, k += 4)
                {
                    trueColor[k    ] = data[j    ];
                    trueColor[k + 1] = data[j + 1];
                    trueColor[k + 2] = data[j + 2];
                    trueColor[k + 3] = 255;
                }
                break;
            }
            default:
                LOG("Unknown texture type\n");
                break;
        }

        if (faceNum == 2) // Up
        {
            FlipImageY(trueColorData.GetData(), width, height, 4, width * 4);

            m_SkyColorAvg = Float3(0.0f);
            int count = width * height * 4;
            uint8_t* trueColor = reinterpret_cast<uint8_t*>(trueColorData.GetData());
            for (int j = 0; j < count; j += 4)
            {
                m_SkyColorAvg.X += trueColor[j + 0];
                m_SkyColorAvg.Y += trueColor[j + 1];
                m_SkyColorAvg.Z += trueColor[j + 2];
            }
            m_SkyColorAvg /= (width * height * 255);
        }
        else
        {
            FlipImageX(trueColorData.GetData(), width, height, 4, width * 4);
        }

        texture->WriteDataCubemap(0, 0, width, height, faceNum, 0, trueColorData.GetData());
    }
}

void BladeLevel::LoadTextures(StringView fileName)
{
    auto& resourceMngr = GameApplication::sGetResourceManager();

    File file = File::sOpenRead(fileName);
    if (!file)
        return;

    RawImage image;

    int32_t texCount = file.ReadInt32();
    for (int i = 0; i < texCount; i++)
    {
        int16_t unknown = file.ReadInt16();
        if (unknown != 2)
        {
            LOG("Invalid MMP {}\n", fileName);
            return;
        }

        file.ReadInt32(); // checksum

        int32_t size = file.ReadInt32();
        String textureName = file.ReadString();
        int32_t type = file.ReadInt32();
        int32_t width = file.ReadInt32();
        int32_t height = file.ReadInt32();

        int32_t textureDataSize = size - 12;
        HeapBlob textureData = file.ReadBlob(textureDataSize);

        image.Reset(width, height, RAW_IMAGE_FORMAT_RGBA8);

        switch (type)
        {
            case TT_PALETTE:
            {
                const uint8_t* data = reinterpret_cast<const uint8_t*>(textureData.GetData());
                const uint8_t* palette = reinterpret_cast<const uint8_t*>(textureData.GetData()) + width * height;
                uint8_t* trueColor = reinterpret_cast<uint8_t*>(image.GetData());

                for (int j = 0; j < height; ++j)
                {
                    for (int k = j * width; k < (j + 1) * width; ++k)
                    {
                        trueColor[k * 4    ] = palette[data[k] * 3    ] << 2;
                        trueColor[k * 4 + 1] = palette[data[k] * 3 + 1] << 2;
                        trueColor[k * 4 + 2] = palette[data[k] * 3 + 2] << 2;
                        trueColor[k * 4 + 3] = 255;
                    }
                }
                break;
            }
            case TT_GRAYSCALED:
            {
                const uint8_t* data = reinterpret_cast<const uint8_t*>(textureData.GetData());
                uint8_t* trueColor = reinterpret_cast<uint8_t*>(image.GetData());
                for (int j = 0; j < height; ++j)
                {
                    for (int k = j * width; k < (j + 1) * width; ++k)
                    {
                        trueColor[k * 4    ] = data[k];
                        trueColor[k * 4 + 1] = data[k];
                        trueColor[k * 4 + 2] = data[k];
                        trueColor[k * 4 + 3] = 255;
                    }
                }
                break;
            }
            case TT_TRUECOLOR:
            {
                Core::Memcpy(image.GetData(), textureData.GetData(), textureData.Size());
                break;
            }
            default:
                LOG("Unknown texture type\n");
                break;
        }

        ImageMipmapConfig mipmapConfig; // use default params
        ImageStorage imageStorage = CreateImage(image, &mipmapConfig, IMAGE_STORAGE_NO_ALPHA, IMAGE_IMPORT_FLAGS_DEFAULT);

        auto texture = resourceMngr.Acquire<Texture>(textureName);
        texture->CreateFromImage(std::move(imageStorage));

        m_Textures.EmplaceBack(std::move(texture));
    }
}

void BladeLevel::UnloadTextures()
{
    for (auto texture : m_Textures)
        texture->Purge();

    m_Textures.Clear();
}

namespace Hk
{
    namespace TriangulatorTraits
    {
        template<>
        void ContourVertexPosition(Double3& dst, Double2 const& src)
        {
            dst.X = src.X;
            dst.Y = src.Y;
            dst.Z = 0;
        }

        template<>
        void TriangleVertexPosition(Double3& dst, Double2 const& src)
        {
            dst.X = src.X;
            dst.Y = src.Y;
            dst.Z = 0;
        }

        template<>
        void CombineVertex(Double2& outputVertex,
            Double3 const& position,
            float const* weights,
            Double2 const& v0,
            Double2 const& v1,
            Double2 const& v2,
            Double2 const& v3)
        {
            outputVertex.X = position.X;
            outputVertex.Y = position.Y;
        }

        template<>
        void CopyVertex(Double2& dst, Double2 const& src)
        {
            dst = src;
        }
    }
}

namespace
{
#define USE_TEXCOORD_CORRECTION

    void CalcTextureCoorinates(const Double3 texCoordAxis[2], const float texCoordOffset[2], MeshVertex* vertices, int numVertices, int texWidth, int texHeight)
    {
        double sx = 1.0 / texWidth;
        double sy = 1.0 / texHeight;

#ifdef USE_TEXCOORD_CORRECTION
        Double2 mins(std::numeric_limits<double>::max());

        Vector<Double2> tempTexcoords(numVertices);

        for (int k = 0; k < numVertices; ++k)
        {
            MeshVertex& v = vertices[k];

            tempTexcoords[k].X = (Math::Dot(texCoordAxis[0], Double3(v.Position)) - double(texCoordOffset[0])) * sx;
            tempTexcoords[k].Y = (Math::Dot(texCoordAxis[1], Double3(v.Position)) - double(texCoordOffset[1])) * sy;

            mins.X = Math::Min(tempTexcoords[k].X, mins.X);
            mins.Y = Math::Min(tempTexcoords[k].Y, mins.Y);
        }

        // Скорректировать текстурные координаты, чтобы они были ближе к нулю
        mins.X = Math::Floor(mins.X);
        mins.Y = Math::Floor(mins.Y);
        for (int k = 0; k < numVertices; ++k)
        {
            vertices[k].SetTexCoord(Float2(tempTexcoords[k] - mins));
        }
#else
        for (int k = 0; k < numVertices; ++k)
        {
            MeshVertex& v = vertices[k];

            Float2 texCoord = Float2((Math::Dot(texCoordAxis[0], Double3(v.Position)) - double(texCoordOffset[0])) * sx,
                                     (Math::Dot(texCoordAxis[1], Double3(v.Position)) - double(texCoordOffset[1])) * sy);

            v.SetTexCoord(texCoord);
        }
#endif
    }

    HK_NODISCARD Vector<Double3> CreateWinding(Vector<Double3> const& vertices, Vector<uint32_t> const& windingIndices)
    {
        int windingSize = windingIndices.Size();

        Vector<Double3> winding(windingSize);
        for (int k = 0; k < windingSize; k++)
            winding[windingSize - k - 1] = vertices[windingIndices[k]];
        return winding;
    }

}

void BladeLevel::LoadWorld(StringView fileName)
{
    //BladeWorld bw;

    if (!bw.Load(fileName))
        return;

    auto& resourceMngr = GameApplication::sGetResourceManager();
    auto& materialMngr = GameApplication::sGetMaterialManager();

    // Create materials
    m_Materials.Clear();
    auto materialResource = resourceMngr.Acquire<Material>("/Root/materials/compiled/wall.mat");
    for (auto& texture : m_Textures)
    {
        IntrusiveRef<MatInstance> matInstance(new MatInstance);

        matInstance->SetResource(materialResource);
        matInstance->SetTexture(0, texture);

        m_Materials[texture->GetName()] = std::move(matInstance);
    }

    Vector<Vector<MeshVertex>> vertexBatches(bw.m_TextureNames.Size());
    Vector<Vector<uint32_t>> indexBatches(bw.m_TextureNames.Size());

    Vector<MeshVertex> vertexBuffer;
    Vector<uint32_t> indexBuffer;

    Vector<MeshVertex> skydomeVertexBuffer;
    Vector<uint32_t> skydomeIndexBuffer;

    Vector<MeshVertex> shadowVertexBuffer;
    Vector<uint32_t> shadowIndexBuffer;

    GameObjectDesc desc;
    GameObject* object;
    m_World->CreateObject(desc, object);

    MatInstanceHandle skyMaterial = materialMngr.FindMaterial("skywall");

    for (auto const& face : bw.m_Faces)
    {
        vertexBuffer.Clear();
        indexBuffer.Clear();

        PlaneD facePlane = ConvertPlane(bw.m_Planes[face.PlaneNum]);
        Float3 faceNormal = Float3(facePlane.Normal);

        if (face.Type == BladeWorld::FT_OPAQUE || face.Type == BladeWorld::FT_SKYDOME)
        {
            for (uint32_t index : face.Winding)
            {
                auto& v = vertexBuffer.EmplaceBack();

                v.Position = Float3(bw.m_Vertices[index]);
                v.SetNormal(faceNormal);
            }

            // triangle fan -> triangles
            for (int j = 0; j < face.Winding.Size() - 2; ++j)
            {
                indexBuffer.Add(0);
                indexBuffer.Add(face.Winding.Size() - j - 2);
                indexBuffer.Add(face.Winding.Size() - j - 1);
            }

            CalcTextureCoorinates(face.TexCoordAxis, face.TexCoordOffset, vertexBuffer.ToPtr(), vertexBuffer.Size(), 256, 256);

            for (auto& v : vertexBuffer)
                v.Position = ConvertCoord(v.Position);

            //Geometry::CalcTangentSpace(vertexBuffer.ToPtr(), indexBuffer.ToPtr(), indexBuffer.Size());
        }
        else if (face.Type == BladeWorld::FT_TRANSPARENT)
        {
            // Do nothing
            continue;
        }
        else if (face.Type == BladeWorld::FT_SINGLE_PORTAL)
        {
            PlaneD plane = bw.m_Planes[face.PlaneNum];

            Vector<Double3> winding = CreateWinding(bw.m_Vertices, face.Winding);
            Vector<Double3> hole = CreateWinding(bw.m_Vertices, face.Holes[0]);

            PolyClipper clipper;
            clipper.SetTransformFromNormal(Float3(plane.Normal));
            clipper.AddSubj3D(winding.ToPtr(), winding.Size());
            clipper.AddClip3D(hole.ToPtr(), hole.Size());

            Vector<ClipperPolygon> resultPolygons;
            clipper.MakeDiff(resultPolygons);

            using MyTriangulator = Triangulator<Double2, Double2>;
            Vector<Double2> resultVertices;
            MyTriangulator triangulator(&resultVertices, &indexBuffer);
            MyTriangulator::Polygon polygon;
            polygon.Normal.X = 0;
            polygon.Normal.Y = 0;
            polygon.Normal.Z = 1;
            for (int i = 0; i < resultPolygons.Size(); i++)
            {
                polygon.OuterContour = resultPolygons[i].Outer.ToPtr();
                polygon.OuterContourVertexCount = resultPolygons[i].Outer.Size();

                polygon.HoleContours.Resize(resultPolygons[i].Holes.Size());
                for (int j = 0; j < resultPolygons[i].Holes.Size(); j++)
                    polygon.HoleContours[j] = std::make_pair(resultPolygons[i].Holes[j].ToPtr(), resultPolygons[i].Holes[j].Size());

                triangulator.Triangulate(&polygon);
            }    

            const Float3x3& transformMatrix = clipper.GetTransform();

            vertexBuffer.Resize(resultVertices.Size());
            for (int k = 0; k < resultVertices.Size(); k++)
            {
                auto& v = vertexBuffer[k];
                v.Position = transformMatrix * Float3(resultVertices[k].X, resultVertices[k].Y, plane.GetDist());
                v.SetNormal(faceNormal);
            }

            CalcTextureCoorinates(face.TexCoordAxis, face.TexCoordOffset, vertexBuffer.ToPtr(), vertexBuffer.Size(), 256, 256);

            for (auto& v : vertexBuffer)
                v.Position = ConvertCoord(v.Position);

            //Geometry::CalcTangentSpace(vertexBuffer.ToPtr(), indexBuffer.ToPtr(), indexBuffer.Size());
        }
        else if (face.Type == BladeWorld::FT_MULTIPLE_PORTALS)
        {
            PlaneD plane = bw.m_Planes[face.PlaneNum];

            Vector<Double3> winding = CreateWinding(bw.m_Vertices, face.Winding);

            CreateWindings_r(vertexBuffer, indexBuffer, face, winding, face.pRoot, nullptr);

#if 0
            PolyClipper clipper;
            clipper.SetTransformFromNormal(Float3(plane.Normal));
            clipper.AddSubj3D(winding.ToPtr(), winding.Size());

            for (auto& h : face.Holes)
            {
                Vector<Double3> hole = CreateWinding(bw.m_Vertices, h);
                clipper.AddClip3D(hole.ToPtr(), hole.Size());
            }

            Vector<ClipperPolygon> resultPolygons;
            clipper.MakeDiff(resultPolygons);

            using MyTriangulator = Triangulator<Double2, Double2>;
            Vector<Double2> resultVertices;
            MyTriangulator triangulator(&resultVertices, &indexBuffer);
            MyTriangulator::Polygon polygon;
            polygon.Normal.X = 0;
            polygon.Normal.Y = 0;
            polygon.Normal.Z = 1;
            for (int i = 0; i < resultPolygons.Size(); i++)
            {
                polygon.OuterContour = resultPolygons[i].Outer.ToPtr();
                polygon.OuterContourVertexCount = resultPolygons[i].Outer.Size();

                polygon.HoleContours.Resize(resultPolygons[i].Holes.Size());
                for (int j = 0; j < resultPolygons[i].Holes.Size(); j++)
                    polygon.HoleContours[j] = std::make_pair(resultPolygons[i].Holes[j].ToPtr(), resultPolygons[i].Holes[j].Size());

                triangulator.Triangulate(&polygon);
            }    

            const Float3x3& transformMatrix = clipper.GetTransform();

            vertexBuffer.Resize(resultVertices.Size());
            for (int k = 0; k < resultVertices.Size(); k++)
            {
                auto& v = vertexBuffer[k];
                v.Position = transformMatrix * Float3(resultVertices[k].X, resultVertices[k].Y, plane.GetDist());
                v.SetNormal(faceNormal);
            }

            CalcTextureCoorinates(&face, vertexBuffer.ToPtr(), vertexBuffer.Size(), 256, 256);

            for (auto& v : vertexBuffer)
                v.Position = ConvertCoord(v.Position);

            //Geometry::CalcTangentSpace(vertexBuffer.ToPtr(), indexBuffer.ToPtr(), indexBuffer.Size());
#endif


            //Vector<ClipperContour> Holes;

            //int numHoles = face.Holes.Size();
            //if (numHoles > 0)
            //{
            //    PolyClipper holesUnion;

            //    holesUnion.SetTransformFromNormal(Float3(plane.Normal));
            //    for (int c = 0; c < numHoles; c++)
            //    {
            //        Vector<Double3> hole = CreateWinding(bw.m_Vertices, face.Holes[c]);
            //        holesUnion.AddSubj3D(hole.ToPtr(), hole.Size());
            //    }

            //    // FIXME: Union of hulls may produce inner holes!
            //    holesUnion.MakeUnion(Holes);
            //}

            //// Create windings and fill Leafs
            //CreateWindings_r(face, Holes, winding, face->pRoot);

            //// Create subfaces
            //for (int i = 0; i < m_Leafs.Size(); i++)
            //{
            //    if (m_Leafs[i]->Vertices.Size() > 0 && m_Leafs[i]->Indices.Size() > 0)
            //    {
            //        m_Leafs[i]->TextureNum = face->TextureNum;
            //        m_Leafs[i]->TexCoordAxis[0] = face->TexCoordAxis[0];
            //        m_Leafs[i]->TexCoordAxis[1] = face->TexCoordAxis[1];
            //        m_Leafs[i]->TexCoordOffset[0] = face->TexCoordOffset[0];
            //        m_Leafs[i]->TexCoordOffset[1] = face->TexCoordOffset[1];

            //        FilterWinding_r(face, face->pRoot, m_Leafs[i]);

            //        Face* SubFace = m_Faces[AllocateFaces(1)];
            //        SubFace->Type = FT_SUBFACE;
            //        SubFace->TextureNum = m_Leafs[i]->TextureNum;
            //        SubFace->TexCoordAxis[0] = m_Leafs[i]->TexCoordAxis[0];
            //        SubFace->TexCoordAxis[1] = m_Leafs[i]->TexCoordAxis[1];
            //        SubFace->TexCoordOffset[0] = m_Leafs[i]->TexCoordOffset[0];
            //        SubFace->TexCoordOffset[1] = m_Leafs[i]->TexCoordOffset[1];
            //        SubFace->SectorIndex = face->SectorIndex;
            //        SubFace->PlaneNum = face->PlaneNum;
            //        SubFace->Vertices = m_Leafs[i]->Vertices;
            //        SubFace->Indices = m_Leafs[i]->Indices;

            //        face->SubFaces.Add(SubFace);
            //    }
            //    else
            //    {
            //        LOG("Leaf with no vertices\n");
            //    }
            //}
            //m_Leafs.Clear();
        }
        else
        {
            continue;
        }

        if (face.Type != BladeWorld::FT_SKYDOME && bw.m_TextureNames[face.TextureNum] != "blanca")
        {
            // add vertexbuffer, indexbuffer to shadow caster

            uint32_t firstVertex = shadowVertexBuffer.Size();

            shadowVertexBuffer.Add(vertexBuffer);

            for (uint32_t i = 0; i < indexBuffer.Size(); i += 3)
            {
                shadowIndexBuffer.Add(firstVertex + indexBuffer[i  ]);
                shadowIndexBuffer.Add(firstVertex + indexBuffer[i+1]);
                shadowIndexBuffer.Add(firstVertex + indexBuffer[i+2]);
            }
        }

        if (face.Type == BladeWorld::FT_SKYDOME)
        {
            Vector<MeshVertex>& vertexBatch = skydomeVertexBuffer;
            Vector<uint32_t>& indexBatch = skydomeIndexBuffer;

            uint32_t firstVertex = vertexBatch.Size();
            vertexBatch.Add(vertexBuffer);
            for (uint32_t i = 0; i < indexBuffer.Size(); i += 3)
            {
                indexBatch.Add(firstVertex + indexBuffer[i  ]);
                indexBatch.Add(firstVertex + indexBuffer[i+1]);
                indexBatch.Add(firstVertex + indexBuffer[i+2]);
            }
        }
        else
        {
            Vector<MeshVertex>& vertexBatch = vertexBatches[face.TextureNum];
            Vector<uint32_t>& indexBatch = indexBatches[face.TextureNum];

            uint32_t firstVertex = vertexBatch.Size();
            vertexBatch.Add(vertexBuffer);
            for (uint32_t i = 0; i < indexBuffer.Size(); i += 3)
            {
                indexBatch.Add(firstVertex + indexBuffer[i  ]);
                indexBatch.Add(firstVertex + indexBuffer[i+1]);
                indexBatch.Add(firstVertex + indexBuffer[i+2]);
            }
        }
    }

    // TODO:
    // Разбить пространство на кубы, распределить треугольники каждого батча по кубам, получим чанки.
    // Если треугольник попадает в несколько кубов, то определить его в тот куб, AABB которого будет меньше с учетом этого треугольника.
    // Далее для каждого куба составить список чанков, AABB которых пересекается с кубом.
    // Далее сделать препроцесс для проверки какой куб из какого куба будет виден, составить PVS.
    // При рендере, получаем куб в котором находится камера, извлекаем PVS, из PVS получаем список видимых кубов, рисуем чанки, которые
    // относятся к этим кубам.
    // Для динамики, в рантайме определяем к какому кубу относится динамический меш, рисуем в зависимости от PVS.

    // TODO:
    // Использовать meshoptimizer для оптимизации геометрии

    for (int textureNum = 0; textureNum < bw.m_TextureNames.Size(); ++textureNum)
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
        mesh->SetLocalBoundingBox(bounds);
        mesh->SetCastShadow(false);

        mesh->SetMaterial(FindMaterial(bw.m_TextureNames[textureNum]));
    }

    // Skydome
    if (!skydomeVertexBuffer.IsEmpty())
    {
        const Vector<MeshVertex>& vertexBatch = skydomeVertexBuffer;
        const Vector<uint32_t>& indexBatch = skydomeIndexBuffer;

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
        mesh->SetLocalBoundingBox(bounds);
        mesh->SetCastShadow(false);

        mesh->SetMaterial(skyMaterial);
    }

    // Create level shadow caster
    {
        MeshRef surface(new Mesh);

        BvAxisAlignedBox bounds;
        bounds.Clear();
        for (auto& v : shadowVertexBuffer)
            bounds.AddPoint(v.Position);

        MeshAllocateDesc alloc;
        alloc.SurfaceCount = 1;
        alloc.VertexCount = shadowVertexBuffer.Size();
        alloc.IndexCount = shadowIndexBuffer.Size();

        surface->Allocate(alloc);
        surface->WriteVertexData(shadowVertexBuffer.ToPtr(), shadowVertexBuffer.Size(), 0);
        surface->WriteIndexData(shadowIndexBuffer.ToPtr(), shadowIndexBuffer.Size(), 0);
        surface->SetBoundingBox(bounds);

        MeshSurface& meshSurface = surface->LockSurface(0);
        meshSurface.BoundingBox = bounds;

        StaticMeshComponent* mesh;
        object->CreateComponent(mesh);
        mesh->SetMesh(surface);
        mesh->SetLocalBoundingBox(bounds);
        mesh->SetShadowMode(ShadowMode::CastOnlyShadow);

        mesh->SetMaterial(materialMngr.FindMaterial("shadow_caster"));
    }
}
#if 0
void BladeLevel::ReadMultiplePortalsFace(File& file, Face* face)
{
    // Face plane
    face->PlaneNum = ReadPlane(file);

    // FIXME: What is it?
    face->UnknownSignature = file.ReadUInt64();
    if (face->UnknownSignature != 3)
        LOG("Face signature {}\n", face->UnknownSignature);

    face->TextureNum = ReadTextureName(file);
    face->TexCoordAxis[0] = file.ReadObject<Double3>();
    face->TexCoordAxis[1] = file.ReadObject<Double3>();
    face->TexCoordOffset[0] = file.ReadFloat();
    face->TexCoordOffset[1] = file.ReadFloat();

    // 8 zero bytes?
    SetDumpLog(false);
    for (int k = 0; k < 8; k++)
    {
        if (DumpByte(file) != 0)
            LOG("not zero!\n");
    }
    SetDumpLog(true);

    // Winding
    ReadIndices(file, face->Indices);

    Vector<Double3> winding;

    winding.Resize(face->Indices.Size());
    for (int k = 0; k < face->Indices.Size(); k++)
        winding[k] = m_Vertices[face->Indices[k]];
    winding.Reverse();

    PlaneD facePlane = m_Planes[face->PlaneNum];

    Vector<ClipperContour> Holes;

    int numHoles = file.ReadInt32();
    if (numHoles > 0)
    {
        PolyClipper HolesUnion;
        Vector<Double3> Hole;

        HolesUnion.SetTransformFromNormal(Float3(facePlane.Normal));

        for (int c = 0; c < numHoles; c++)
        {
            ReadWinding(file, Hole);

            HolesUnion.AddSubj3D(Hole.ToPtr(), Hole.Size());

            Portal* portal = AllocatePortal();

            portal->pFace = face;
            portal->ToSector = file.ReadInt32();
            portal->Winding = Hole;

            m_Sectors[face->SectorIndex].Portals.Add(portal);

            int32_t Count = file.ReadInt32();
            portal->Planes.Resize(Count);
            for (int k = 0; k < Count; k++)
                portal->Planes[k] = file.ReadObject<PlaneD>();
        }

        // FIXME: Union of hulls may produce inner holes!
        HolesUnion.MakeUnion(Holes);
    }

    face->pRoot = ReadBSPNode_r(file, face);

    // Create windings and fill Leafs
    CreateWindings_r(face, Holes, winding, face->pRoot);

    // Create subfaces
    for (int i = 0; i < m_Leafs.Size(); i++)
    {
        if (m_Leafs[i]->Vertices.Size() > 0 && m_Leafs[i]->Indices.Size() > 0)
        {
            m_Leafs[i]->TextureNum = face->TextureNum;
            m_Leafs[i]->TexCoordAxis[0] = face->TexCoordAxis[0];
            m_Leafs[i]->TexCoordAxis[1] = face->TexCoordAxis[1];
            m_Leafs[i]->TexCoordOffset[0] = face->TexCoordOffset[0];
            m_Leafs[i]->TexCoordOffset[1] = face->TexCoordOffset[1];

            FilterWinding_r(face, face->pRoot, m_Leafs[i]);

            Face* SubFace = m_Faces[AllocateFaces(1)];
            SubFace->Type = FT_SUBFACE;
            SubFace->TextureNum = m_Leafs[i]->TextureNum;
            SubFace->TexCoordAxis[0] = m_Leafs[i]->TexCoordAxis[0];
            SubFace->TexCoordAxis[1] = m_Leafs[i]->TexCoordAxis[1];
            SubFace->TexCoordOffset[0] = m_Leafs[i]->TexCoordOffset[0];
            SubFace->TexCoordOffset[1] = m_Leafs[i]->TexCoordOffset[1];
            SubFace->SectorIndex = face->SectorIndex;
            SubFace->PlaneNum = face->PlaneNum;
            SubFace->Vertices = m_Leafs[i]->Vertices;
            SubFace->Indices = m_Leafs[i]->Indices;

            face->SubFaces.Add(SubFace);
        }
        else
        {
            LOG("Leaf with no vertices\n");
        }
    }
    m_Leafs.Clear();
}
#endif
Double3 CalcNormal(Vector<Double3> const& winding)
{
    if (winding.Size() < 3)
        return Double3(0);

    Double3 center = winding[0];
    for (size_t i = 1, count = winding.Size(); i < count; ++i)
        center += winding[i];
    center /= winding.Size();

#ifdef CONVEX_HULL_CW
    // CW
    return Math::Cross(winding[1] - center, winding[0] - center).NormalizeFix();
#else
    // CCW
    return Math::Cross(winding[0] - center, winding[1] - center).NormalizeFix();
#endif
}

PlaneSide SplitWinding(Vector<Double3> const& winding, PlaneD const& plane, double epsilon, Vector<Double3>& frontHull, Vector<Double3>& backHull)
{
    size_t i;
    size_t count = winding.Size();

    int front = 0;
    int back = 0;

    constexpr size_t MaxHullVerts = 128;

    SmallVector<double, MaxHullVerts> distances(count + 1);
    SmallVector<PlaneSide, MaxHullVerts> sides(count + 1);

    frontHull.Clear();
    backHull.Clear();

    // Classify each point of the hull
    for (i = 0; i < count; ++i)
    {
        double dist = Math::Dot(winding[i], plane);

        distances[i] = dist;

        if (dist > epsilon)
        {
            sides[i] = PlaneSide::Front;
            front++;
        }
        else if (dist < -epsilon)
        {
            sides[i] = PlaneSide::Back;
            back++;
        }
        else
        {
            sides[i] = PlaneSide::On;
        }
    }

    sides[i] = sides[0];
    distances[i] = distances[0];

    // If all points on the plane
    if (!front && !back)
    {
        Double3 hullNormal = CalcNormal(winding);

        if (Math::Dot(hullNormal, plane.Normal) > 0)
        {
            frontHull = winding;
            return PlaneSide::Front;
        }
        else
        {
            backHull = winding;
            return PlaneSide::Back;
        }
    }

    if (!front)
    {
        // All poins at the back side of the plane
        backHull = winding;
        return PlaneSide::Back;
    }

    if (!back)
    {
        // All poins at the front side of the plane
        frontHull = winding;
        return PlaneSide::Front;
    }

    frontHull.Reserve(count + 4);
    backHull.Reserve(count + 4);

    for (i = 0; i < count; ++i)
    {
        Double3 const& p = winding[i];

        switch (sides[i])
        {
        case PlaneSide::On:
            frontHull.Add(p);
            backHull.Add(p);
            continue;
        case PlaneSide::Front:
            frontHull.Add(p);
            break;
        case PlaneSide::Back:
            backHull.Add(p);
            break;
        }

        auto nextSide = sides[i + 1];

        if (nextSide == PlaneSide::On || nextSide == sides[i])
        {
            continue;
        }

        Double3 newVertex = winding[(i + 1) % count];

        if (sides[i] == PlaneSide::Front)
        {
            double dist = distances[i] / (distances[i] - distances[i + 1]);
            for (int j = 0; j < 3; j++)
            {
                if (plane.Normal[j] == 1)
                    newVertex[j] = -plane.D;
                else if (plane.Normal[j] == -1)
                    newVertex[j] = plane.D;
                else
                    newVertex[j] = p[j] + dist * (newVertex[j] - p[j]);
            }
        }
        else
        {
            double dist = distances[i + 1] / (distances[i + 1] - distances[i]);
            for (int j = 0; j < 3; j++)
            {
                if (plane.Normal[j] == 1)
                    newVertex[j] = -plane.D;
                else if (plane.Normal[j] == -1)
                    newVertex[j] = plane.D;
                else
                    newVertex[j] = newVertex[j] + dist * (p[j] - newVertex[j]);
            }
        }

        frontHull.Add(newVertex);
        backHull.Add(newVertex);
    }

    return PlaneSide::Cross;
}

void BladeLevel::CreateWindings_r(Vector<MeshVertex>& vertexBuffer, Vector<uint32_t>& indexBuffer,
    BladeWorld::Face const& face, Vector<Double3> const& winding, BladeWorld::BSPNode const* node, BladeWorld::BSPNode const* texInfo)
{
    if (node->Type == BladeWorld::NT_TEXINFO)
    {
        texInfo = node;
#if 0
        PlaneD plane = bw.m_Planes[face.PlaneNum];

        PlaneD facePlane = ConvertPlane(plane);
        Float3 faceNormal = Float3(facePlane.Normal);

        PolyClipper clipper;
        clipper.SetTransformFromNormal(Float3(plane.Normal));
        clipper.AddSubj3D(winding.ToPtr(), winding.Size());

        for (auto& h : face.Holes)
        {
            Vector<Double3> hole = CreateWinding(bw.m_Vertices, h);
            clipper.AddClip3D(hole.ToPtr(), hole.Size());
        }

        Vector<ClipperPolygon> resultPolygons;
        clipper.MakeDiff(resultPolygons);

        Vector<uint32_t> tempIndexBuffer;

        using MyTriangulator = Triangulator<Double2, Double2>;
        Vector<Double2> resultVertices;
        MyTriangulator triangulator(&resultVertices, &tempIndexBuffer);
        MyTriangulator::Polygon polygon;
        polygon.Normal.X = 0;
        polygon.Normal.Y = 0;
        polygon.Normal.Z = 1;
        for (int i = 0; i < resultPolygons.Size(); i++)
        {
            polygon.OuterContour = resultPolygons[i].Outer.ToPtr();
            polygon.OuterContourVertexCount = resultPolygons[i].Outer.Size();

            polygon.HoleContours.Resize(resultPolygons[i].Holes.Size());
            for (int j = 0; j < resultPolygons[i].Holes.Size(); j++)
                polygon.HoleContours[j] = std::make_pair(resultPolygons[i].Holes[j].ToPtr(), resultPolygons[i].Holes[j].Size());

            triangulator.Triangulate(&polygon);
        }

        const Float3x3& transformMatrix = clipper.GetTransform();

        uint32_t firstVertex = vertexBuffer.Size();

        for (int k = 0; k < resultVertices.Size(); k++)
        {
            auto& v = vertexBuffer.EmplaceBack();
            v.Position = transformMatrix * Float3(resultVertices[k].X, resultVertices[k].Y, plane.GetDist());
            v.SetNormal(faceNormal);
        }

        CalcTextureCoorinates(node->TexCoordAxis, node->TexCoordOffset, vertexBuffer.ToPtr() + firstVertex, resultVertices.Size(), 256, 256);

        for (uint32_t i = 0; i < resultVertices.Size(); ++i)
            vertexBuffer[firstVertex + i].Position = ConvertCoord(vertexBuffer[firstVertex + i].Position);

        //Geometry::CalcTangentSpace(vertexBuffer.ToPtr() + firstVertex, tempIndexBuffer.ToPtr(), tempIndexBuffer.Size());

        for (uint32_t index : tempIndexBuffer)
            indexBuffer.Add(firstVertex + index);

#endif
    }
    else if (node->Type == BladeWorld::NT_LEAF)
    {
#if 1
        PlaneD plane = bw.m_Planes[face.PlaneNum];

        PlaneD facePlane = ConvertPlane(plane);
        Float3 faceNormal = Float3(facePlane.Normal);

        PolyClipper clipper;
        clipper.SetTransformFromNormal(Float3(plane.Normal));
        clipper.AddSubj3D(winding.ToPtr(), winding.Size());

        for (auto& h : face.Holes)
        {
            Vector<Double3> hole = CreateWinding(bw.m_Vertices, h);
            clipper.AddClip3D(hole.ToPtr(), hole.Size());
        }

        Vector<ClipperPolygon> resultPolygons;
        clipper.MakeDiff(resultPolygons);

        Vector<uint32_t> tempIndexBuffer;

        using MyTriangulator = Triangulator<Double2, Double2>;
        Vector<Double2> resultVertices;
        MyTriangulator triangulator(&resultVertices, &tempIndexBuffer);
        MyTriangulator::Polygon polygon;
        polygon.Normal.X = 0;
        polygon.Normal.Y = 0;
        polygon.Normal.Z = 1;
        for (int i = 0; i < resultPolygons.Size(); i++)
        {
            polygon.OuterContour = resultPolygons[i].Outer.ToPtr();
            polygon.OuterContourVertexCount = resultPolygons[i].Outer.Size();

            polygon.HoleContours.Resize(resultPolygons[i].Holes.Size());
            for (int j = 0; j < resultPolygons[i].Holes.Size(); j++)
                polygon.HoleContours[j] = std::make_pair(resultPolygons[i].Holes[j].ToPtr(), resultPolygons[i].Holes[j].Size());

            triangulator.Triangulate(&polygon);
        }

        const Float3x3& transformMatrix = clipper.GetTransform();

        uint32_t firstVertex = vertexBuffer.Size();

        for (int k = 0; k < resultVertices.Size(); k++)
        {
            auto& v = vertexBuffer.EmplaceBack();
            v.Position = transformMatrix * Float3(resultVertices[k].X, resultVertices[k].Y, plane.GetDist());
            v.SetNormal(faceNormal);
        }

        if (texInfo)
        {
            CalcTextureCoorinates(texInfo->TexCoordAxis, texInfo->TexCoordOffset, vertexBuffer.ToPtr() + firstVertex, resultVertices.Size(), 256, 256);
        }
        else
        {
            CalcTextureCoorinates(face.TexCoordAxis, face.TexCoordOffset, vertexBuffer.ToPtr() + firstVertex, resultVertices.Size(), 256, 256);
        }

        for (uint32_t i = 0; i < resultVertices.Size(); ++i)
            vertexBuffer[firstVertex + i].Position = ConvertCoord(vertexBuffer[firstVertex + i].Position);

        for (uint32_t index : tempIndexBuffer)
            indexBuffer.Add(firstVertex + index);

        //Geometry::CalcTangentSpace(vertexBuffer.ToPtr() + firstVertex, tempIndexBuffer.ToPtr(), tempIndexBuffer.Size());

#endif
        //HK_ASSERT(!winding.IsEmpty());

        //PolyClipper clipper;
        //clipper.SetTransformFromNormal(Float3(m_Planes[face->PlaneNum].Normal));
        //clipper.AddSubj3D(winding.ToPtr(), winding.Size());

        //for (int i = 0; i < holes.Size(); i++) {
        //    clipper.AddClip2D(holes[i].ToPtr(), holes[i].Size());
        //}

        //Vector<ClipperPolygon> resultPolygons;
        //clipper.MakeDiff(resultPolygons);

        //using MyTriangulator = Triangulator<Double2, Double2>;
        //Vector<MyTriangulator::Polygon*> Polygons;
        //Vector<Double2> resultVertices;
        //Vector<unsigned int> resultIndices;
        //MyTriangulator triangulator(&resultVertices, &resultIndices);
        //MyTriangulator::Polygon polygon;
        //polygon.Normal.X = 0;
        //polygon.Normal.Y = 0;
        //polygon.Normal.Z = 1;
        //for (int i = 0; i < resultPolygons.Size(); i++)
        //{
        //    polygon.OuterContour = resultPolygons[i].Outer.ToPtr();
        //    polygon.OuterContourVertexCount = resultPolygons[i].Outer.Size();

        //    polygon.HoleContours.Resize(resultPolygons[i].Holes.Size());
        //    for (int j = 0; j < resultPolygons[i].Holes.Size(); j++)
        //        polygon.HoleContours[j] = std::make_pair(resultPolygons[i].Holes[j].ToPtr(), resultPolygons[i].Holes[j].Size());

        //    triangulator.Triangulate(&polygon);
        //}        

        //const Float3x3& transformMatrix = clipper.GetTransform();

        //node->Vertices.Resize(resultVertices.Size());
        //for (int k = 0; k < resultVertices.Size(); k++) {
        //    node->Vertices[k] = Double3(transformMatrix * Float3(resultVertices[k].X, resultVertices[k].Y, m_Planes[face->PlaneNum].GetDist()));
        //}
        //node->Indices = resultIndices;

        //m_Leafs.Add(node);

        return;
    }

    Vector<Double3> front;
    Vector<Double3> back;

    HK_ASSERT(!winding.IsEmpty());
    SplitWinding(winding, bw.m_Planes[node->PlaneNum], 0.0, front, back);

    CreateWindings_r(vertexBuffer, indexBuffer, face, front, node->Children[0], texInfo);
    CreateWindings_r(vertexBuffer, indexBuffer, face, back, node->Children[1], texInfo);
}

#if 0
namespace
{
    int AreVerticesMostlyOnFront(PlaneD const& plane, Vector<Double3> const& vertices)
    {
        int front = 0;
        int back = 0;
        for (auto& v : vertices)
        {
            double d = plane.DistanceToPoint(v);
            if (d > 0.0) front++;
            else if (d < -0.0) back++;
        }
        return front > back;
    }
}

void BladeLevel::FilterWinding_r(Face* face, BSPNode* node, BSPNode* leaf)
{
    if (node->Type == NT_LEAF)
        return;

    if (!AreVerticesMostlyOnFront(m_Planes[node->PlaneNum], leaf->Vertices))
    {
        if (node->Type == NT_TEXINFO)
        {
            leaf->TextureNum = node->TextureNum;
            leaf->TexCoordAxis[0] = node->TexCoordAxis[0];
            leaf->TexCoordAxis[1] = node->TexCoordAxis[1];
            leaf->TexCoordOffset[0] = node->TexCoordOffset[0];
            leaf->TexCoordOffset[1] = node->TexCoordOffset[1];
        }
        FilterWinding_r(face, node->Children[1], leaf);
    }
}
#endif

MatInstanceHandle BladeLevel::FindMaterial(StringView name)
{
    auto it = m_Materials.Find(name);
    if (it != m_Materials.End())
        return it->second;

    //HK_ASSERT(0);

    auto& materialMngr = GameApplication::sGetMaterialManager();
    return materialMngr.FindMaterial("grid8");
}

void BladeLevel::DrawDebug(DebugRenderer& renderer)
{
#if 0
    Vector<Float3> contour;

    for (auto& sector : bw.m_Sectors)
    {
        for (int faceIndex=0;faceIndex<sector.FaceCount;++faceIndex)
        {
            contour.Clear();
            for (uint32_t index : bw.m_Faces[sector.FirstFace + faceIndex].Winding)
            {
                contour.Add(ConvertCoord(Float3(bw.m_Vertices[index])));
            }

            renderer.SetColor(Color4::sBlue());
            renderer.DrawLine(contour);
        }        
    }
#endif
}
