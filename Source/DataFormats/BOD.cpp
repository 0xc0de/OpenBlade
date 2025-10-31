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

#include "BOD.h"
#include "../Utils/FileDump.h"

using namespace Hk;

void BladeModel::Clear()
{
    Vertices.Clear();
    Faces.Clear();
    Bones.Clear();
    Anchors.Clear();
    Textures.Clear();
}

void BladeModel::Load(StringView fileName)
{
    Clear();

    File f = File::sOpenRead(fileName);
    if (!f)
        return;

    Name = f.ReadString();

    Vertices.Resize(f.ReadInt32());
    for (auto& v : Vertices)
    {
        v.Position = f.ReadObject<Double3>();
        v.Normal = f.ReadObject<Double3>();
    }

    auto ReadTextureName = [this](File& f) -> int
        {
            String name = f.ReadString();
            for (int i = 0, count = Textures.Size(); i < count; ++i)
            {
                if (!Textures[i].Icmp(name))
                    return i;
            }
            Textures.Add(std::move(name));
            return Textures.Size() - 1;
        };

    int faceCount = f.ReadInt32();
    Faces.Resize(faceCount);

    for (int i = 0; i < faceCount; ++i)
    {
        auto& face = Faces[i];

        face.Indices[0] = f.ReadInt32();
        face.Indices[1] = f.ReadInt32();
        face.Indices[2] = f.ReadInt32();

        Vertices[face.Indices[0]].Faces.Add(i);
        Vertices[face.Indices[1]].Faces.Add(i);
        Vertices[face.Indices[2]].Faces.Add(i);

        face.TextureNum = ReadTextureName(f);

        face.TexCoords[0].X = f.ReadFloat();
        face.TexCoords[1].X = f.ReadFloat();
        face.TexCoords[2].X = f.ReadFloat();
        face.TexCoords[0].Y = 1.0f - f.ReadFloat();
        face.TexCoords[1].Y = 1.0f - f.ReadFloat();
        face.TexCoords[2].Y = 1.0f - f.ReadFloat();

        face.Unknown = f.ReadInt32();
        assert(face.Unknown == 0); // ???

        face.Group = 1;
    }

    SetDumpLog(true);

    Bones.Resize(f.ReadInt32());
    for (Bone& bone : Bones)
    {
        if (Bones.Size() != 1)
            bone.Name = f.ReadString();

        bone.ParentIndex = f.ReadInt32();

        for (int i = 0; i < 16; ++i)
            bone.Matrix.ToPtr()[i] = f.ReadDouble();

        bone.VertexCount = f.ReadInt32();
        bone.FirstVertex = f.ReadInt32();

        int count = f.ReadInt32();

        for (int i = 0; i < count; i++)
        {
            // local offset XYZ:
            Double3 localOffset = f.ReadObject<Double3>();

            double distance = DumpDouble(f);
            HK_UNUSED(distance);

            int FirstVertex = DumpInt(f);
            HK_UNUSED(FirstVertex);

            int NumVertices = DumpInt(f);
            HK_UNUSED(NumVertices);
        }
    }


    // Some Position or axis
    UnknownDbl0 = DumpDouble(f);
    UnknownDbl1 = DumpDouble(f);
    UnknownDbl2 = DumpDouble(f);

    // Distance?
    UnknownDbl3 = DumpDouble(f);

    // Fire

    FireLights.Resize(f.ReadInt32());
    for (auto& light : FireLights)
    {
        light.Vertices.Resize(f.ReadInt32());

        for (auto& v : light.Vertices)
        {
            v.Position = f.ReadObject<Double3>();
            DumpInt(f); // unknown
        }

        light.ParentIndex = DumpInt(f);

        DumpInt(f); // ???
    }

    // Omni

    OmniLights.Resize(f.ReadInt32());
    for (auto& light : OmniLights)
    {
        light.Intensity = f.ReadFloat();
        light.Unknown = DumpFloat(f);
        light.Position = f.ReadObject<Double3>();
        light.ParentIndex = f.ReadInt32();
    }

    // Anchors

    Anchors.Resize(f.ReadInt32());
    for (auto& anchor : Anchors)
    {
        anchor.Name = f.ReadString();

        for (int i = 0; i < 16; i++)
            anchor.Matrix.ToPtr()[i] = f.ReadDouble();

        anchor.ParentIndex = f.ReadInt32();
    }


    SetDumpLog(true);

    // 4
    int numDataChunks = f.ReadInt32();

    if (numDataChunks > 0)
    {
        // Edges
        // 0, 8
        Edges.Resize(f.ReadInt32());
        for (auto& edge : Edges)
        {
            DumpInt(f); // zero?
            edge.ParentIndex = f.ReadInt32();

            edge.P0 = f.ReadObject<Double3>();
            edge.P1 = f.ReadObject<Double3>();
            edge.P2 = f.ReadObject<Double3>();
        }
        --numDataChunks;
    }

    if (numDataChunks > 0)
    {
        // Spikes
        // 0, 2
        Spikes.Resize(f.ReadInt32());
        for (auto& spike : Spikes)
        {
            DumpInt(f); // zero?
            spike.ParentIndex = f.ReadInt32();

            // two points
            spike.P0 = f.ReadObject<Double3>();
            spike.P1 = f.ReadObject<Double3>();
        }
        --numDataChunks;
    }

    if (numDataChunks > 0)
    {
        // Groups
        auto count = f.ReadInt32();
        HK_ASSERT(count == faceCount);
        if (count == faceCount)
        {
            for (int n = 0; n < count; ++n)
                Faces[n].Group = DumpByte(f);
        }
        else
            f.SeekCur(count);

        --numDataChunks;
    }

    if (numDataChunks > 0)
    {
        // Mutilations
        Mutilations.Resize(f.ReadInt32()); // face count
        for (auto& index : Mutilations)
            index = f.ReadInt32();

        --numDataChunks;
    }

    if (numDataChunks > 0)
    {
        // Trails
        Trails.Resize(f.ReadInt32());
        for (auto& trail : Trails)
        {
            DumpInt(f); // zero?
            trail.ParentIndex = f.ReadInt32(); // parent index

            trail.Position = f.ReadObject<Double3>();
            trail.Dir = f.ReadObject<Double3>();
        }

        --numDataChunks;
    }


#if 0
    // Build mesh
    Vector<Vector<MeshVertex>> vertexBatches(Textures.Size());
    Vector<Vector<uint32_t>> indexBatches(Textures.Size());
    for (auto& face : Faces)
    {
        Vector<MeshVertex>& vertexBatch = vertexBatches[face.TextureNum];
        Vector<uint32_t>& indexBatch = indexBatches[face.TextureNum];

        for (int i = 0; i < 3; ++i)
        {
            auto& v = vertexBatch.EmplaceBack();

            v.Position = ConvertCoord(Vertices[face.Indices[i]].Position);
            v.Normal = ConvertAxis(Vertices[face.Indices[i]].Normal)
                v.TexCoord = face.TexCoords[i];
            // TODO: clear other members, calculate tangent, binormal

            indexBatch.Add(vertexBatch.Size() - 1); // TODO: use mesh optimizer to generate index buffer for the batch
        }
    }

    for (Vector<MeshVertex>& vertexBatch : vertexBatches)
    {
        // TODO: use mesh optimizer to optimize vertexBatch
        //       use mesh optimizer to generate index buffer for the batch
    }
#endif
}
