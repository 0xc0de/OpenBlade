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

#include "BladeTextures.h"

#include <Engine/IO/Public/FileUrl.h>
#include <Engine/Utilites/Public/ImageUtils.h>
#include <Engine/GHI/GHIExt.h>

#include <Engine/Resource/Public/ResourceManager.h>

enum EBladeTextureType {
    TT_Palette = 1,
    TT_Grayscaled = 2,
    TT_TrueColor = 4
};

static const char * DomeNames[ 6 ] = {
    "DomeRight",
    "DomeLeft",
    "DomeUp",
    "DomeDown",
    "DomeBack",
    "DomeFront"
};

// Load texture from memory
FTextureResource * LoadTexture( const char * _TextureName, const byte * _TrueColor, int _Width, int _Height ) {
    FTextureResource * Texture = GResourceManager->GetResource< FTextureResource >( _TextureName );

    Texture->UploadImage2D( _TrueColor, _Width, _Height, 3, true, true );

    Out() << "TEX_SIZE" << _Width << _Height << _TextureName;

    return Texture;
}

// Load textures from .MMP file
void LoadTextures( const char * _FileName ) {
    FFileAbstract * File = FFiles::OpenFileFromUrl( _FileName, FFileAbstract::M_Read );

    if ( !File ) {
        return;
    }

    int32_t TexturesCount;
    File->ReadSwapInt32( TexturesCount );
    for ( int i = 0 ; i < TexturesCount ; i++ ) {
        int16_t UnknownInt16;

        File->ReadSwapInt16( UnknownInt16 );

        int32_t Checksum;
        File->ReadSwapInt32( Checksum );

        int32_t Size;
        File->ReadSwapInt32( Size );

        FString TextureName;
        File->ReadString( TextureName );

        int32_t Type;
        File->ReadSwapInt32( Type );

        int32_t Width;
        File->ReadSwapInt32( Width );

        int32_t Height;
        File->ReadSwapInt32( Height );

        int32_t TextureDataLength = Size - 12;

        byte * TextureData = new byte[ TextureDataLength ];

        File->Read( TextureData, TextureDataLength );

        switch ( Type ) {
        case TT_Palette:
        {
            byte * Palette = TextureData + Width * Height;
            byte * TrueColor = new byte[ Width * Height * 3 ];

            for ( int j = 0; j < Height; ++j ) {
                for ( int k = j*Width; k < ( j + 1 )*Width; ++k ) {
                    TrueColor[ k * 3 + 2 ] = Palette[ TextureData[ k ] * 3     ] << 2;
                    TrueColor[ k * 3 + 1 ] = Palette[ TextureData[ k ] * 3 + 1 ] << 2;
                    TrueColor[ k * 3     ] = Palette[ TextureData[ k ] * 3 + 2 ] << 2;
                }
            }

            LoadTexture( TextureName.Str(), TrueColor, Width, Height );

            delete[] TrueColor;
            break;
        }
        case TT_Grayscaled:
        {
            byte * TrueColor = new byte[ Width * Height * 3 ];
            for ( int j = 0; j < Height; ++j ) {
                for ( int k = j*Width; k < ( j + 1 )*Width; ++k ) {
                    TrueColor[ k * 3     ] = TextureData[ k ];
                    TrueColor[ k * 3 + 1 ] = TextureData[ k ];
                    TrueColor[ k * 3 + 2 ] = TextureData[ k ];
                }
            }

            LoadTexture( TextureName.Str(), TrueColor, Width, Height );

            delete[] TrueColor;
            break;
        }
        case TT_TrueColor:
        {
            // Swap to bgr
            int Count = Width * Height * 3;
            for ( int j = 0; j < Count ; j += 3 ) {
                FCore::SwapArgs( TextureData[ j ], TextureData[ j + 2 ] );
            }
            LoadTexture( TextureName.Str(), TextureData, Width, Height );
            break;
        }
        default:
            Out() << "Unknown texture type";
        }

        delete[] TextureData;
    }
    FFiles::CloseFile( File );
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

// Load Skydome from .MMP file
FTextureResource * LoadDome( const char * _FileName, Float3 * _SkyColorAvg ) {
    FFileAbstract * File = FFiles::OpenFileFromUrl( _FileName, FFileAbstract::M_Read );

    if ( !File ) {
        return NULL;
    }

    FTextureDesc Desc;
    FTextureLodDesc Lods[ 6 ];
    Desc.ByteLength = 0;
    Desc.NumLods = 1;
    Desc.Dimension = SAMPLER_DIM_CUBEMAP;
    Desc.ColorSpace = SAMPLER_RGBA;
    Desc.InternalPixelFormat = GHI_IPF_RGB16F;
    Desc.PixelFormat = GHI_PF_Float_BGR;
    Desc.NumLayers = 6;
    Desc.Lods = Lods;

    memset( Lods, 0, sizeof( Lods ) );

    int32_t TexturesCount;
    File->ReadSwapInt32( TexturesCount );
    for ( int i = 0 ; i < TexturesCount ; i++ ) {
        int16_t UnknownInt16;

        File->ReadSwapInt16( UnknownInt16 );

        int32_t Checksum;
        File->ReadSwapInt32( Checksum );

        int32_t Size;
        File->ReadSwapInt32( Size );

        FString TextureName;
        File->ReadString( TextureName );

        int32_t Type;
        File->ReadSwapInt32( Type );

        int32_t Width;
        File->ReadSwapInt32( Width );

        int32_t Height;
        File->ReadSwapInt32( Height );

        Desc.Width = Width;
        Desc.Height = Height;

        int DomeFace;
        for ( DomeFace = 0 ; DomeFace < 6 ; DomeFace++ ) {
            if ( !FString::CmpCase( TextureName.Str(), DomeNames[DomeFace] ) ) {
                break;
            }
        }

        if ( DomeFace == 6 ) {
            // Unknown dome face
            continue;
        }

        Lods[ DomeFace ].ByteLength = Width * Height * 3 * sizeof( float );
        Lods[ DomeFace ].HWMemUsage = Lods[ DomeFace ].ByteLength >> 1;

        Desc.ByteLength += Lods[ DomeFace ].ByteLength;

        int32_t TextureDataLength = Size - 12;

        byte * TextureData = new byte[ TextureDataLength ];

        File->Read( TextureData, TextureDataLength );

        enum ETextureType {
            TT_Palette = 1,
            TT_Grayscaled = 2,
            TT_TrueColor = 4
        };

        const float Normalize = 1.0f / 255.0f;

#define SIMULATE_HDRI
#ifdef SIMULATE_HDRI
        const float HDRI_Scale = 4.0f;
        const float HDRI_Pow = 1.1f;
#else
        const float HDRI_Scale = 1.0f;
        const float HDRI_Pow = 1.0f;
#endif

        switch ( Type ) {
        case TT_Palette:
        {
            byte * Palette = TextureData + Width * Height;
            float * TrueColor = new float[ Width * Height * 3 ];

            for ( int j = 0; j < Height; ++j ) {
                for ( int k = j*Width; k < ( j + 1 )*Width; ++k ) {
                    TrueColor[ k * 3 + 2 ] = pow( ConvertToRGB( ( Palette[ TextureData[ k ] * 3     ] << 2 ) * Normalize ) * HDRI_Scale, HDRI_Pow );
                    TrueColor[ k * 3 + 1 ] = pow( ConvertToRGB( ( Palette[ TextureData[ k ] * 3 + 1 ] << 2 ) * Normalize ) * HDRI_Scale, HDRI_Pow );
                    TrueColor[ k * 3     ] = pow( ConvertToRGB( ( Palette[ TextureData[ k ] * 3 + 2 ] << 2 ) * Normalize ) * HDRI_Scale, HDRI_Pow );
                }
            }

            Lods[ DomeFace ].Pixels = TrueColor;
            break;
        }
        case TT_Grayscaled:
        {
            float * TrueColor = new float[ Width * Height * 3 ];
            for ( int j = 0; j < Height; ++j ) {
                for ( int k = j*Width; k < ( j + 1 )*Width; ++k ) {
                    TrueColor[ k * 3     ] = pow( ConvertToRGB( TextureData[ k ] * Normalize ) * HDRI_Scale, HDRI_Pow );
                    TrueColor[ k * 3 + 1 ] = pow( ConvertToRGB( TextureData[ k ] * Normalize ) * HDRI_Scale, HDRI_Pow );
                    TrueColor[ k * 3 + 2 ] = pow( ConvertToRGB( TextureData[ k ] * Normalize ) * HDRI_Scale, HDRI_Pow );
                }
            }

            Lods[ DomeFace ].Pixels = TrueColor;
            break;
        }
        case TT_TrueColor:
        {
            float * TrueColor = new float[ Width * Height * 3 ];
            int Count = Width * Height * 3;
            for ( int j = 0; j < Count ; j += 3 ) {
                TrueColor[ j     ] = pow( ConvertToRGB( TextureData[ j + 2 ] * Normalize ) * HDRI_Scale, HDRI_Pow );
                TrueColor[ j + 1 ] = pow( ConvertToRGB( TextureData[ j + 1 ] * Normalize ) * HDRI_Scale, HDRI_Pow );
                TrueColor[ j + 2 ] = pow( ConvertToRGB( TextureData[ j     ] * Normalize ) * HDRI_Scale, HDRI_Pow );
            }
            Lods[ DomeFace ].Pixels = TrueColor;
            break;
        }
        default:
            Lods[ DomeFace ].Pixels = NULL;
            Out() << "Unknown texture type";
            break;
        }

        delete[] TextureData;

        if ( DomeFace == 2 ) {  // Up
            FImageUtils::FlipBuffer( Desc.Lods[DomeFace].Pixels, Width, Height, 3 * 4, Width*3 * 4, true, false );

            if ( _SkyColorAvg ) {
                *_SkyColorAvg = Float3(0.0f);
                int Count = Width * Height * 3;
                float * TrueColor = (float *)Lods[ DomeFace ].Pixels;
                for ( int j = 0; j < Count ; j += 3 ) {
                    _SkyColorAvg->X += TrueColor[ j + 2 ];
                    _SkyColorAvg->Y += TrueColor[ j + 1 ];
                    _SkyColorAvg->Z += TrueColor[ j + 0 ];
                }
                *_SkyColorAvg /= Count;
            }
        } else {
            FImageUtils::FlipBuffer( Desc.Lods[DomeFace].Pixels, Width, Height, 3 * 4, Width*3 * 4, false, true );
        }
    }
    FFiles::CloseFile( File );

    FTextureResource * Texture = GResourceManager->CreateUnnamedResource< FTextureResource >();
    Texture->UploadImage( Desc );

    for ( int DomeFace = 0 ; DomeFace < 6 ; DomeFace++ ) {
        delete [] (float *)Desc.Lods[DomeFace].Pixels;
    }

    return Texture;
}

FTextureResource * CreateWhiteCubemap() {
    FTextureDesc Desc;
    FTextureLodDesc Lods[ 6 ];
    Desc.ByteLength = 0;
    Desc.NumLods = 1;
    Desc.Dimension = SAMPLER_DIM_CUBEMAP;
    Desc.ColorSpace = SAMPLER_RGBA;
    Desc.InternalPixelFormat = GHI_IPF_RGB8;
    Desc.PixelFormat = GHI_PF_UByte_BGR;
    Desc.NumLayers = 6;
    Desc.Width = 1;
    Desc.Height = 1;
    Desc.Lods = Lods;
    Desc.ByteLength = 6 * 3;

    memset( Lods, 0, sizeof( Lods ) );

    const byte Color[3] = { 255,255,255 };

    for ( int i = 0 ; i < 6 ; i++ ) {
        Lods[ i ].ByteLength = 3;
        Lods[ i ].HWMemUsage = 3;
        Lods[ i ].Pixels = (void *)&Color[0];
    }

    FTextureResource * Texture = GResourceManager->CreateUnnamedResource< FTextureResource >();
    Texture->UploadImage( Desc );
    return Texture;
}
