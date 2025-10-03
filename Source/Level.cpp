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

#include <Hork/Runtime/GameApplication/GameApplication.h>

using namespace Hk;

namespace
{
    enum TEXTURE_TYPE
    {
        TT_PALETTE = 1,
        TT_GRAYSCALED = 2,
        TT_TRUECOLOR = 4
    };
}

void BladeLevel::Load(StringView name)
{
    char str[256];
    char key[256];
    char value[256];

    File file = File::sOpenRead(name);
    if (!file)
        return;

    StringView fileLocation = PathUtils::sGetFilePath(name);
    bool skydomeSpecified = false;

    UnloadTextures();

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
            LoadWorld(fileName);
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
    UniqueRef<TextureResource> textureResource;
    
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

        if (!textureResource)
        {
            textureResource = MakeUnique<TextureResource>();
            textureResource->AllocateCubemap(GameApplication::sGetRenderDevice(), TEXTURE_FORMAT_RGBA16_FLOAT, 1, width);
        }

        if (textureResource->GetWidth() != static_cast<uint32_t>(width))
        {
            LOG("Invalid dome face\n");
            return;
        }

        int32_t textureDataSize = size - 12;
        HeapBlob textureData = file.ReadBlob(textureDataSize);

        const float Normalize = 1.0f / 255.0f;

//#define SIMULATE_HDRI
#ifdef SIMULATE_HDRI
        const float HDRI_Scale = 4.0f;
        const float HDRI_Pow = 1.1f;
#else
        const float HDRI_Scale = 1.0f;
        const float HDRI_Pow = 1.0f;
#endif
        trueColorData.Reset(sizeof(uint16_t) * width * height * 4);
        
        switch (type)
        {
            case TT_PALETTE:
            {
                const uint8_t* data = reinterpret_cast<const uint8_t*>(textureData.GetData());
                const uint8_t* palette = reinterpret_cast<const uint8_t*>(textureData.GetData()) + width * height;
                uint16_t* trueColor = reinterpret_cast<uint16_t*>(trueColorData.GetData());

                for (int j = 0; j < height; ++j)
                {
                    for (int k = j * width; k < (j + 1) * width; ++k)
                    {
                        trueColor[k * 4    ] = f32tof16(Math::Pow(ColorUtils::LinearFromSRGB((palette[data[k] * 3    ] << 2) * Normalize) * HDRI_Scale, HDRI_Pow));
                        trueColor[k * 4 + 1] = f32tof16(Math::Pow(ColorUtils::LinearFromSRGB((palette[data[k] * 3 + 1] << 2) * Normalize) * HDRI_Scale, HDRI_Pow));
                        trueColor[k * 4 + 2] = f32tof16(Math::Pow(ColorUtils::LinearFromSRGB((palette[data[k] * 3 + 2] << 2) * Normalize) * HDRI_Scale, HDRI_Pow));
                    }
                }
                break;
            }
            case TT_GRAYSCALED:
            {
                const uint8_t* data = reinterpret_cast<const uint8_t*>(textureData.GetData());
                uint16_t* trueColor = reinterpret_cast<uint16_t*>(trueColorData.GetData());
                for (int j = 0; j < height; ++j)
                {
                    for (int k = j * width; k < (j + 1) * width; ++k)
                    {
                        trueColor[k * 4    ] = f32tof16(Math::Pow(ColorUtils::LinearFromSRGB(data[k] * Normalize) * HDRI_Scale, HDRI_Pow));
                        trueColor[k * 4 + 1] = f32tof16(Math::Pow(ColorUtils::LinearFromSRGB(data[k] * Normalize) * HDRI_Scale, HDRI_Pow));
                        trueColor[k * 4 + 2] = f32tof16(Math::Pow(ColorUtils::LinearFromSRGB(data[k] * Normalize) * HDRI_Scale, HDRI_Pow));
                    }
                }
                break;
            }
            case TT_TRUECOLOR:
            {
                const uint8_t* data = reinterpret_cast<const uint8_t*>(textureData.GetData());
                uint16_t* trueColor = reinterpret_cast<uint16_t*>(trueColorData.GetData());
                int count = width * height * 3;
                for (int j = 0, k = 0; j < count; j += 3, k += 4)
                {
                    trueColor[k    ] = f32tof16(Math::Pow(ColorUtils::LinearFromSRGB(data[j    ] * Normalize) * HDRI_Scale, HDRI_Pow));
                    trueColor[k + 1] = f32tof16(Math::Pow(ColorUtils::LinearFromSRGB(data[j + 1] * Normalize) * HDRI_Scale, HDRI_Pow));
                    trueColor[k + 2] = f32tof16(Math::Pow(ColorUtils::LinearFromSRGB(data[j + 2] * Normalize) * HDRI_Scale, HDRI_Pow));
                }
                break;
            }
            default:
                LOG("Unknown texture type\n");
                break;
        }

        if (faceNum == 2) // Up
        {
            FlipImageY(trueColorData.GetData(), width, height, sizeof(uint16_t) * 4, sizeof(uint16_t) * width * 4);

            m_SkyColorAvg = Float3(0.0f);
            int count = width * height * 4;
            uint16_t* trueColor = reinterpret_cast<uint16_t*>(trueColorData.GetData());
            for (int j = 0; j < count; j += 4)
            {
                m_SkyColorAvg.X += f16tof32(trueColor[j + 0]);
                m_SkyColorAvg.Y += f16tof32(trueColor[j + 1]);
                m_SkyColorAvg.Z += f16tof32(trueColor[j + 2]);
            }
            m_SkyColorAvg /= (width * height);
        }
        else
        {
            FlipImageX(trueColorData.GetData(), width, height, sizeof(uint16_t) * 4, sizeof(uint16_t) * width * 4);
        }

        textureResource->WriteDataCubemap(0, 0, width, height, faceNum, 0, trueColorData.GetData());
    }

    resourceMngr.CreateResourceWithData("internal_skybox", std::move(textureResource));
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
        file.ReadInt16(); // unknown
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

        UniqueRef<TextureResource> textureResource = MakeUnique<TextureResource>(std::move(imageStorage));
        textureResource->Upload(GameApplication::sGetRenderDevice());

        m_LevelTextures.Add(resourceMngr.CreateResourceWithData(textureName, std::move(textureResource)));
    }
}

void BladeLevel::UnloadTextures()
{
    auto& resourceMngr = GameApplication::sGetResourceManager();

    for (auto textureHandle : m_LevelTextures)
        resourceMngr.PurgeResourceData(textureHandle);

    m_LevelTextures.Clear();
}

void BladeLevel::LoadWorld(StringView fileName)
{
}
