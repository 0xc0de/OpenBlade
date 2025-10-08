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

#include <Hork/Geometry/BV/BvAxisAlignedBox.h>
#include <Hork/Math/Plane.h>
#include <Hork/Core/Containers/Vector.h>

using namespace Hk;

class BladeModel
{
public:
    struct Vertex
    {
        Double3 Position;
        Double3 Normal;
        //Vector<int> Faces;
    };

    struct Face
    {
        int Indices[3];
        Float2 TexCoords[3];
        int Unknown;
        int TextureNum;
    };

    struct Bone
    {
        String Name;
        int ParentIndex;
        Float4x4 Matrix;
        int FirstVertex;
        int VertexCount;
    };

    struct Anchor
    {
        String Name;
        int ParentIndex;
        Float4x4 Matrix;
    };

    struct Fire
    {
        struct Vertex
        {
            Double3 Position;
        };

        Vector<Vertex> Vertices;

        int ParentIndex;
    };

    struct Omni
    {
        float Intensity;
        float Unknown;
        Double3 Position;
        int ParentIndex;
    };
    
    struct Edge
    {
        int ParentIndex;
        Double3 P0;
        Double3 P1;
        Double3 P2;
    };

    struct Spike
    {
        int ParentIndex;
        Double3 P0;
        Double3 P1;
    };

    struct Trail
    {
        int ParentIndex;
        Double3 Position;
        Double3 Dir;
    };

    String Name;
    Vector<Vertex> Vertices;
    Vector<Face> Faces;
    Vector<Bone> Bones;
    Vector<Anchor> Anchors;
    Vector<Fire> FireLights;
    Vector<Omni> OmniLights;
    Vector<Edge> Edges;
    Vector<Spike> Spikes;
    Vector<int32_t> Mutilations;
    Vector<Trail> Trails;
    Vector<String> Textures;

    double UnknownDbl0;
    double UnknownDbl1;
    double UnknownDbl2;
    double UnknownDbl3;

    void Load(StringView fileName);

    void Clear();
};
