#include <Engine/Renderer/Public/TextureResource.h>
#include <Engine/Renderer/Public/StaticMeshResource.h>
#include <Engine/Renderer/Public/SkinnedMeshResource.h>
#include <Engine/Renderer/Public/SkinnedAnimResource.h>
#include <Engine/Resource/Public/ResourceManager.h>
#include <Engine/Utilites/Public/ImageUtils.h>
#include <Engine/GHI/GHIExt.h>

#include <unordered_set>

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

FTextureResource * CreateCubemapTexture( const char * _Names[6], bool _SimulateHDRI ) {
    FTextureResource * Texture = GResourceManager->CreateUnnamedResource< FTextureResource >();

    FTextureDesc Desc;
    FTextureLodDesc Lods[6];
    Desc.ByteLength = 0;
    Desc.NumLods = 1;
    Desc.Dimension = SAMPLER_DIM_CUBEMAP;
    Desc.ColorSpace = SAMPLER_RGBA;
    Desc.NumLayers = 6;
    Desc.Lods = Lods;

    FImage Image[6];
    float *HDRI[6];

    if ( _SimulateHDRI ) {
        memset( HDRI, 0, sizeof( HDRI ) );
        Desc.InternalPixelFormat = GHI_IPF_RGB16F;
        Desc.PixelFormat = GHI_PF_Float_BGR;
    } else {
        Desc.InternalPixelFormat = GHI_IPF_SRGB8;
        Desc.PixelFormat = GHI_PF_UByte_BGR;
    }

    const float Normalize = 1.0f / 255.0f;
    const float HDRI_Scale = 4.0f;
    const float HDRI_Pow = 1.1f;

    for ( int i = 0 ; i < 6 ; i++ ) {
        Image[i].CreateFromFile( _Names[i], true, true );
        if ( !Image[i].IsValid() ) {
            return Texture;
        }

        int w = Image[i].GetWidth();
        int h = Image[i].GetHeight();

        Desc.Width = w;
        Desc.Height = h;

        byte * Pixels = Image[i].GetData();
        int Count = w * h * 3;

        if ( _SimulateHDRI ) {
            HDRI[i] = new float[ Count ];

            for ( int j = 0; j < Count ; j += 3 ) {
                HDRI[i][ j     ] = pow( ConvertToRGB( Pixels[ j + 0 ] * Normalize ) * HDRI_Scale, HDRI_Pow );
                HDRI[i][ j + 1 ] = pow( ConvertToRGB( Pixels[ j + 1 ] * Normalize ) * HDRI_Scale, HDRI_Pow );
                HDRI[i][ j + 2 ] = pow( ConvertToRGB( Pixels[ j + 2 ] * Normalize ) * HDRI_Scale, HDRI_Pow );
            }

            Lods[i].ByteLength = Lods[i].HWMemUsage = Count * sizeof( float );
            Lods[i].Pixels = HDRI[i];
        } else {
            Lods[i].ByteLength = Lods[i].HWMemUsage = Count;
            Lods[i].Pixels = Image[i].GetData();
        }         

        Desc.ByteLength += Lods[i].ByteLength;
    };

    Texture->UploadImage( Desc );

    if ( _SimulateHDRI ) {
        for ( int i = 0 ; i < 6 ; i++ ) {
            delete [] HDRI[i];
        }
    }

    return Texture;
}


#include <assimp/scene.h>
#include <assimp/cimport.h>
#include <assimp/postprocess.h>

static const aiScene * LoadAssimpScene( const char * _FileName, bool _Skinned ) {
    unsigned int Flags =
        aiProcess_CalcTangentSpace
//        | aiProcess_JoinIdenticalVertices
        | aiProcess_Triangulate

        //| aiProcess_RemoveComponent

        | aiProcess_GenSmoothNormals

//        | aiProcess_OptimizeMeshes

        | aiProcess_LimitBoneWeights

        | aiProcess_ValidateDataStructure

//        | aiProcess_ImproveCacheLocality

        //| aiProcess_RemoveRedundantMaterials

//        | aiProcess_FixInfacingNormals

//        | aiProcess_FindDegenerates

        | aiProcess_FindInvalidData

        //| aiProcess_GenUVCoords

        //| aiProcess_TransformUVCoords

        | aiProcess_FlipUVs
        //| aiProcess_SplitByBoneCount

        //| aiProcess_Debone
            ;

    if ( _Skinned ) {
//        Flags |= aiProcess_OptimizeGraph;
    } else {
//        Flags |= aiProcess_PreTransformVertices;
    }

    const aiScene * ImportScene;

    aiPropertyStore * Property = aiCreatePropertyStore();

    int RemoveComponents = 0;
    /*
    int RemoveComponents =
          aiComponent_COLORS
        | aiComponent_BONEWEIGHTS
        | aiComponent_ANIMATIONS
        | aiComponent_TEXTURES
        | aiComponent_LIGHTS
        | aiComponent_CAMERAS
        | aiComponent_MATERIALS;
    */

    if ( RemoveComponents && ( Flags &  aiProcess_RemoveComponent ) ) {
        aiSetImportPropertyInteger( Property, AI_CONFIG_PP_RVC_FLAGS, RemoveComponents );
    }
    aiSetImportPropertyInteger( Property, AI_CONFIG_PP_SBP_REMOVE, aiPrimitiveType_LINE|aiPrimitiveType_POINT );
    aiSetImportPropertyInteger( Property, AI_CONFIG_PP_LBW_MAX_WEIGHTS, 4 );
    aiSetImportPropertyInteger( Property, AI_CONFIG_PP_ICL_PTCACHE_SIZE, 24 );

    if ( _Skinned ) {
        aiSetImportPropertyInteger( Property, AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, false );
    }

    ImportScene = aiImportFileExWithProperties( _FileName, Flags, NULL, Property );
    aiReleasePropertyStore( Property );

    return ImportScene;
}

static int FindJoint( const char * _Name, const FJoint * _Joints, int _NumJoints ) {
    for ( int i = 0 ; i < _NumJoints ; i++ ) {
        if ( !FString::Cmp( _Joints[i].Name, _Name ) ) {
            return i;
        }
    }
    return -1;
}

static int JointIndexGen;

static void ReadJointsHeirarchy_r( const aiNode * _Node,
                                   const std::unordered_set< std::string > & _Bones,
                                   int _ParentJoint,
                                   FJoint * _Joints ) {
    std::string NodeName( _Node->mName.C_Str() );

    int JointIndex; // joint index of current node

    auto it = _Bones.find( NodeName );
    if ( it != _Bones.end() ) {
        JointIndex = JointIndexGen;
        JointIndexGen++;

        _Joints[ JointIndex ].Parent = _ParentJoint;
        FString::CopySafe( _Joints[ JointIndex ].Name, NodeName.c_str(), sizeof( _Joints[ JointIndex ].Name ) );

        _ParentJoint = JointIndex;
    } else  {
        Out() << "Node " << NodeName.c_str() << " has no joint (keep " << Int(_ParentJoint) << ")\n";
    }

    for ( unsigned int i = 0 ; i < _Node->mNumChildren ; i++ ) {
        ReadJointsHeirarchy_r( _Node->mChildren[i], _Bones, _ParentJoint, _Joints );
    }
}

static void ReadJointsHeirarchy( const aiScene * _ImportScene, const std::unordered_set<std::string> & _Bones, FJoint * _Joints ) {
    JointIndexGen = 0;
    ReadJointsHeirarchy_r( _ImportScene->mRootNode, _Bones, -1, _Joints );
}
//#pragma warning(disable : 4505)
static void ReadSkinnedModel( FSkinnedMeshResource * _Resource, const aiScene * _ImportScene, float _Scale ) {
    aiMatrix4x4 GlobalScaling;
    aiMatrix4x4::Scaling( aiVector3D( _Scale, _Scale, _Scale ), GlobalScaling );
    aiMatrix4x4 PretransformMatrix = _ImportScene->mRootNode->mTransformation;
    PretransformMatrix.Inverse();
    PretransformMatrix = PretransformMatrix * GlobalScaling;

    FJoint * pJoint;

    int FirstVertex = 0;
    int FirstIndex = 0;

    std::unordered_set< std::string > Bones;

    // Build bones set
    for ( unsigned int i = 0 ; i < _ImportScene->mNumMeshes ; i++ ) {
        const aiMesh * pMesh = _ImportScene->mMeshes[ i ];
        for ( unsigned int j = 0 ; j < pMesh->mNumBones ; j++ ) {
            Bones.insert( std::string( pMesh->mBones[ j ]->mName.C_Str() ) );
        }
    }

    TPodArray< FJoint > ModelJoints;
    TPodArray< FSkinnedVertex > ModelVertices;
    TPodArray< unsigned int > ModelIndices;
    TArray< FMeshOffset > MeshOffsets;

    ModelJoints.Resize( Bones.size() );
    ReadJointsHeirarchy( _ImportScene, Bones, ModelJoints.ToPtr() );

    aiString Path;

    // Read vertices, indices and joints
    for ( unsigned int i = 0 ; i < _ImportScene->mNumMeshes ; i++ ) {
        const aiMesh * pMesh = _ImportScene->mMeshes[ i ];

        if ( !pMesh->mNumBones ) {
            // ignore static meshes
            continue;
        }

        aiMaterial * Material = _ImportScene->mMaterials[ pMesh->mMaterialIndex ];
        Material->GetTexture( aiTextureType_DIFFUSE, 0, &Path );

        MeshOffsets.EmplaceBack( pMesh->mNumFaces * 3,
                                 FirstIndex,
                                 FirstVertex,
                                 Path.C_Str() );

        for ( unsigned int j = 0 ; j < pMesh->mNumFaces ; j++ ) {
            const aiFace & Face = pMesh->mFaces[ j ];

            for ( unsigned int k = 0 ; k < Face.mNumIndices ; k++ ) {
                const unsigned int & ind = Face.mIndices[ k ];

                ModelIndices.Append( /*FirstVertex + */ind );
            }
        }

        int NumUVChannels = pMesh->GetNumUVChannels();

        for ( unsigned int j = 0 ; j < pMesh->mNumVertices ; j++ ) {

            FSkinnedVertex & v = ModelVertices.Append();

            v.Position[0] = pMesh->mVertices[ j ].x;
            v.Position[1] = pMesh->mVertices[ j ].y;
            v.Position[2] = pMesh->mVertices[ j ].z;

            assert( pMesh->mNormals != NULL );
            v.Normal[0] = pMesh->mNormals[ j ].x;
            v.Normal[1] = pMesh->mNormals[ j ].y;
            v.Normal[2] = pMesh->mNormals[ j ].z;

            assert( pMesh->mTangents != NULL );
            v.Tangent[0] = pMesh->mTangents[ j ].x;
            v.Tangent[1] = pMesh->mTangents[ j ].y;
            v.Tangent[2] = pMesh->mTangents[ j ].z;
            v.Handedness = FMath::Dot( FMath::Cross( v.Normal, v.Tangent ), Float3( pMesh->mBitangents[ j ].x, pMesh->mBitangents[ j ].y, pMesh->mBitangents[ j ].z ) ) < 0 ? -1.0f : 1.0f;

            memset( v.JointIndices, 0, sizeof( v.JointIndices ) );
            memset( v.JointWeights, 0, sizeof( v.JointWeights ) );

            if ( NumUVChannels > 0 ) {
                v.TexCoord[0] = pMesh->mTextureCoords[ 0 ][ j ].x;
                v.TexCoord[1] = pMesh->mTextureCoords[ 0 ][ j ].y;
            }
        }

        for ( unsigned int j = 0 ; j < pMesh->mNumBones ; j++ ) {
            aiBone * Bone = pMesh->mBones[ j ];

            int JointIndex = FindJoint( Bone->mName.C_Str(), ModelJoints.ToPtr(), ModelJoints.Length() ) ;
            if ( JointIndex == -1 ) {
                // “акого не должно произойти!
                Out() << "Joint " << Bone->mName.C_Str() << " not found\n";
                continue;
            }

            pJoint = &ModelJoints[ JointIndex ];
            memcpy( &pJoint->JointOffsetMatrix[0][0], &Bone->mOffsetMatrix[0][0], sizeof( Float3x4 ) );

            for ( unsigned int w = 0 ; w < Bone->mNumWeights ; w++ ) {
                aiVertexWeight * VertexWeight = &Bone->mWeights[ w ];

                FSkinnedVertex & v = ModelVertices[ FirstVertex + VertexWeight->mVertexId ];

                // find unused joint
                for ( int t = 0 ; t < 4 ; t++ ) {
#if 0
                    if ( v.JointWeights[ t ] < 0.00001f ) {
                        v.JointWeights[ t ] = VertexWeight->mWeight;
                        v.JointIndices[ t ] = JointIndex;
                        break;
                    }
#else
                    if ( v.JointWeights[ t ] == 0/* && v.JointIndices[ t ] != JointIndex*/ ) {
                        v.JointWeights[ t ] = Float( VertexWeight->mWeight * 255.0f ).Clamp( 0.0f, 255.0f );
                        v.JointIndices[ t ] = JointIndex;
                        if ( t > 0 ) {
                            Out() << "Num Weights > 1\n";
                        }
                        break;
                    }
#endif
                }
            }
        }
        FirstVertex += pMesh->mNumVertices;
        FirstIndex += pMesh->mNumFaces * 3;
    }

    _Resource->SetPretransformMatrix( *reinterpret_cast< Float3x4 * >( &PretransformMatrix[0][0] ) );
    _Resource->SetVertexData( ModelVertices.ToPtr(), ModelVertices.Length(), ModelIndices.ToPtr(), ModelIndices.Length(), ModelJoints.ToPtr(), ModelJoints.Length(), true );
    _Resource->SetMeshOffsets( MeshOffsets.ToPtr(), MeshOffsets.Length() );
}

static const aiNodeAnim * FindNodeAnim( const aiAnimation * _Animation, const std::string & _NodeName ) {
    for ( unsigned int Channel = 0 ; Channel < _Animation->mNumChannels ; Channel++ ) {
        const aiNodeAnim * NodeAnim = _Animation->mChannels[ Channel ];
        if ( std::string( NodeAnim->mNodeName.C_Str() ) == _NodeName ) {
            return NodeAnim;
        }
    }
    return NULL;
}

//static int FindJointAnim( const char * _Name, const FJoint * _Joints, const FJointAnimation * _AnimatedJoints, int _NumAnimatedJoints ) {
//    for ( int i = 0 ; i < _NumAnimatedJoints ; i++ ) {
//        if ( !FString::Cmp( _Joints[ _AnimatedJoints[i].JointIndex ].Name, _Name ) ) {
//            return i;
//        }
//    }
//    return -1;
//}

static int FindJointAnim( int _JointIndex, const FJointAnimation * _AnimatedJoints, int _NumAnimatedJoints ) {
    for ( int i = 0 ; i < _NumAnimatedJoints ; i++ ) {
        if ( _AnimatedJoints[i].JointIndex == _JointIndex ) {
            return i;
        }
    }
    return -1;
}

static void CalcInterpolatedRotation( const aiNodeAnim * _NodeAnim, float _AnimationTime, aiQuaternion & _Out ) {
    // дл€ интерполировани€ требуетс€ не менее 2 значений...
    if ( _NodeAnim->mNumRotationKeys == 1 ) {
        _Out = _NodeAnim->mRotationKeys[0].mValue;
        return;
    }

    int Current = -1;

    if ( _NodeAnim->mNumRotationKeys > 0 ) {
         for ( int i = 0 ; i < (int)_NodeAnim->mNumRotationKeys - 1 ; i++ ) {
            if ( _AnimationTime < (float)_NodeAnim->mRotationKeys[i + 1].mTime ) {

                if ( _AnimationTime >= (float)_NodeAnim->mRotationKeys[i].mTime ) {
                    Current = i;
                    break;
                }

                _Out = _NodeAnim->mRotationKeys[ FMath::Max( 0, i - 1 ) ].mValue;
                return;
            }
        }
    }
    if ( Current < 0 ) {
        _Out.x = 0;
        _Out.y = 0;
        _Out.z = 0;
        _Out.w = 1;
        return;
    }

    int Next = Current + 1;
    assert( Next < (int)_NodeAnim->mNumRotationKeys );

    float DeltaTime = _NodeAnim->mRotationKeys[Next].mTime - _NodeAnim->mRotationKeys[Current].mTime;

    float Factor = (_AnimationTime - (float)_NodeAnim->mRotationKeys[Current].mTime) / DeltaTime;
    assert( Factor >= 0.0f && Factor <= 1.0f );

    const aiQuaternion & StartRotation = _NodeAnim->mRotationKeys[ Current ].mValue;
    const aiQuaternion & EndRotation   = _NodeAnim->mRotationKeys[ Next    ].mValue;

    aiQuaternion::Interpolate( _Out, StartRotation, EndRotation, Factor );
    _Out = _Out.Normalize();
}

static void CalcInterpolatedScaling( const aiNodeAnim * _NodeAnim, float _AnimationTime, aiVector3D & _Out ) {
    // дл€ интерполировани€ требуетс€ не менее 2 значений...
    if ( _NodeAnim->mNumScalingKeys == 1 ) {
        _Out = _NodeAnim->mScalingKeys[0].mValue;
        return;
    }

    int Current = -1;

    if ( _NodeAnim->mNumScalingKeys > 0 ) {
         for ( int i = 0 ; i < (int)_NodeAnim->mNumScalingKeys - 1 ; i++ ) {
            if ( _AnimationTime < (float)_NodeAnim->mScalingKeys[i + 1].mTime ) {

                if ( _AnimationTime >= (float)_NodeAnim->mScalingKeys[i].mTime ) {
                    Current = i;
                    break;
                }

                _Out = _NodeAnim->mScalingKeys[ FMath::Max( 0, i - 1 ) ].mValue;
                return;
            }
        }
    }
    if ( Current < 0 ) {
        _Out.x = _Out.y = _Out.z = 1;
        return;
    }

    int Next = Current + 1;
    assert( Next < (int)_NodeAnim->mNumScalingKeys );

    float DeltaTime = _NodeAnim->mScalingKeys[Next].mTime - _NodeAnim->mScalingKeys[Current].mTime;

    float Factor = (_AnimationTime - (float)_NodeAnim->mScalingKeys[Current].mTime) / DeltaTime;
    assert( Factor >= 0.0f && Factor <= 1.0f );

    const aiVector3D & StartScaling = _NodeAnim->mScalingKeys[ Current ].mValue;
    const aiVector3D & EndScaling   = _NodeAnim->mScalingKeys[ Next    ].mValue;

    _Out = StartScaling + Factor * ( EndScaling - StartScaling );
}

static void CalcInterpolatedPosition( const aiNodeAnim * _NodeAnim, float _AnimationTime, aiVector3D & _Out ) {
    // дл€ интерполировани€ требуетс€ не менее 2 значений...
    if ( _NodeAnim->mNumPositionKeys == 1 ) {
        _Out = _NodeAnim->mPositionKeys[0].mValue;
        return;
    }

    int Current = -1;

    if ( _NodeAnim->mNumPositionKeys > 0 ) {
         for ( int i = 0 ; i < (int)_NodeAnim->mNumPositionKeys - 1 ; i++ ) {
            if ( _AnimationTime < (float)_NodeAnim->mPositionKeys[i + 1].mTime ) {

                if ( _AnimationTime >= (float)_NodeAnim->mPositionKeys[i].mTime ) {
                    Current = i;
                    break;
                }

                _Out = _NodeAnim->mPositionKeys[ FMath::Max( 0, i - 1 ) ].mValue;
                return;
            }
        }
    }
    if ( Current < 0 ) {
        _Out.x = _Out.y = _Out.z = 0;
        return;
    }

    int Next = Current + 1;
    assert( Next < (int)_NodeAnim->mNumPositionKeys );

    float DeltaTime = _NodeAnim->mPositionKeys[Next].mTime - _NodeAnim->mPositionKeys[Current].mTime;

    float Factor = (_AnimationTime - (float)_NodeAnim->mPositionKeys[Current].mTime) / DeltaTime;

    if ( Factor < 0 || Factor > 1 ) {
        Out() << "Factor:" << Factor << " KeyIndex " << Current << "\n";
        for ( unsigned int i = 0 ; i < _NodeAnim->mNumPositionKeys ; i++ ) {
            Out() << "i: "<<i<< " time " << _NodeAnim->mPositionKeys[i].mTime << "\n";
       }
    }

    assert( Factor >= 0.0f && Factor <= 1.0f );

    const aiVector3D & StartPosition = _NodeAnim->mPositionKeys[ Current ].mValue;
    const aiVector3D & EndPosition   = _NodeAnim->mPositionKeys[ Next    ].mValue;

    _Out = StartPosition + Factor * ( EndPosition - StartPosition );
}

static aiMatrix4x4 JointPretransformMatrix;
static aiMatrix4x4 PretransformMatrix;
static void ReadNodeHeirarchy_r( const aiNode * _Node,
                                 const aiAnimation * _Animation,
                                 float _AnimationTime,
                                 const aiMatrix4x4 & _ParentTransform,
                                 const FJoint * _Joints,
                                 int _NumJoints,
                                 int _FrameIndex,
                                 int _NumFrames,
                                 TArray< FJointAnimation > & _OutAnimation ) {
    std::string NodeName( _Node->mName.C_Str() );

    aiMatrix4x4 NodeTransformation;

    const aiNodeAnim * pNodeAnim = FindNodeAnim( _Animation, NodeName );

    if ( pNodeAnim ) {
        aiVector3D Scaling;
        aiQuaternion Rotation;
        aiVector3D Translation;

        CalcInterpolatedScaling( pNodeAnim, _AnimationTime, Scaling );
        CalcInterpolatedRotation( pNodeAnim, _AnimationTime, Rotation );
        CalcInterpolatedPosition( pNodeAnim, _AnimationTime, Translation );

        NodeTransformation = aiMatrix4x4( Scaling, Rotation, Translation );
    } else {
        NodeTransformation = _Node->mTransformation;
    }

    aiMatrix4x4 GlobalTransformation = _ParentTransform * NodeTransformation;

    int JointIndex = FindJoint( _Node->mName.C_Str(), _Joints, _NumJoints );
    if ( JointIndex != -1 ) {

        Float3 t,s;
        Float3x3 r;

        FJointAnimation * pJointAnim;
        int AnimJointIndex = FindJointAnim( JointIndex, _OutAnimation.ToPtr(), _OutAnimation.Length() );
        if ( AnimJointIndex == -1 ) {
            _OutAnimation.EmplaceBack();
            pJointAnim = &_OutAnimation.Last();
            pJointAnim->JointIndex = JointIndex;
            pJointAnim->Frames.Resize( _NumFrames );
        } else {
            pJointAnim = &_OutAnimation[AnimJointIndex];
        }

        ( *(Float3x4 *)&NodeTransformation[0][0] ).DecomposeAll( t,r,s );

        pJointAnim->Frames[ _FrameIndex ].Transform.RelPosition = t;
        pJointAnim->Frames[ _FrameIndex ].Transform.RelRotation.FromMatrix( r );
        pJointAnim->Frames[ _FrameIndex ].Transform.RelScale = s;

//        aiMatrix4x4 FinalTransformation = PretransformMatrix * GlobalTransformation * (*(aiMatrix4x4 *)&Float4x4(_Joints[JointIndex].JointOffsetMatrix)[0][0]);
//        FMat3x4_Decompose( *(Float3x4 *)&FinalTransformation[0][0], t,r,s );
//        pJointAnim->Frames[ _FrameIndex ].Transform.Position = t;
//        pJointAnim->Frames[ _FrameIndex ].Transform.Rotation = FQuat( r );
//        pJointAnim->Frames[ _FrameIndex ].Transform.Scale = s;

        if ( _Joints[ JointIndex ].Parent == -1 ) {
            JointPretransformMatrix = PretransformMatrix * _ParentTransform;
        }
    }

    for ( unsigned int i = 0 ; i < _Node->mNumChildren ; i++ ) {
        ReadNodeHeirarchy_r( _Node->mChildren[i], _Animation, _AnimationTime, GlobalTransformation, _Joints, _NumJoints, _FrameIndex, _NumFrames, _OutAnimation );
    }
}

static FSkinnedAnimResource * ReadAnimation( const char * _FileName, const aiScene * _Scene, int _AnimationIndex, const FSkinnedMeshResource * _Model ) {
    assert( _AnimationIndex < (int)_Scene->mNumAnimations );

    FSkinnedAnimResource * Resource = GResourceManager->GetResource< FSkinnedAnimResource >( _FileName );

    aiAnimation * Animation = _Scene->mAnimations[ _AnimationIndex ];

    aiMatrix4x4 Identity;

    float TicksPerSecond = Animation->mTicksPerSecond != 0 ? Animation->mTicksPerSecond : 25.0f;

    Resource->SetFrameRate( 1.0f / TicksPerSecond );
    int FrameCount = floor( Animation->mDuration );
    //AnimationName = Animation->mName.C_Str();

    TPodArray< Float3x4 > PretransformMatrices;
    PretransformMatrices.Resize( FrameCount );

    memcpy( &PretransformMatrix[0][0], &Float4x4( _Model->GetPretransformMatrix() )[0][0], sizeof( float ) * 4 * 4 );

    TArray< FJointAnimation > AnimatedJoints;

    for ( int FrameIndex = 0 ; FrameIndex < FrameCount ; FrameIndex++ ) {
        double AnimationTime = Animation->mDuration / FrameCount * FrameIndex;

        JointPretransformMatrix = PretransformMatrix;

        ReadNodeHeirarchy_r( _Scene->mRootNode,
                             Animation,
                             AnimationTime,
                             Identity,
                             _Model->GetJoints().ToPtr(),
                             _Model->GetJoints().Length(),
                             FrameIndex,
                             FrameCount,
                             AnimatedJoints );

        memcpy( &PretransformMatrices[FrameIndex][0][0], &JointPretransformMatrix[0][0], sizeof( Float3x4 ) );
    }

    Resource->SetAnimationData( PretransformMatrices.ToPtr(), FrameCount, AnimatedJoints.ToPtr(), AnimatedJoints.Length() );

#if 0

    // Compute bounds for each animation frame
    BvAxisAlignedBox Aabb;
    Float3 TmpPoint;

    _SkinnedAnimation.Bounds.resize( _Model.Meshes.size() );
    for ( int MeshIndex = 0 ; MeshIndex < (int)_Model.Meshes.size() ; MeshIndex++ ) {
        FSkinnedAnimation::FBounds & MeshBounds = _SkinnedAnimation.Bounds[ MeshIndex ];
        MeshBounds.Frames.resize( _SkinnedAnimation.FrameCount );
    }

    std::vector< Float3x4 > AbsoluteMatrices( _Model.Joints.size() + 1 );
    std::vector< Float3x4 > BindposeTransforms( _Model.Joints.size() );

    Float3x4 RelativeTransform;

    const FJointAnimation * pAnimJoints = _SkinnedAnimation.AnimatedJoints.data();

    for ( int FrameIndex = 0 ; FrameIndex < _SkinnedAnimation.FrameCount ; FrameIndex++ ) {

        //AbsoluteMatrices[0] = Float3x4(1);
        AbsoluteMatrices[0] = _SkinnedAnimation.JointPretransformMatrix[FrameIndex];

        // Compute bindpose transforms
        for ( unsigned int j = 0 ; j < _Model.Joints.size() ; j++ ) {
            const FJoint & Joint = _Model.Joints[ j ];

            const Float3 & Position = pAnimJoints[j].Frames[FrameIndex].Transform.RelPosition;
            Float3x3 Rotation = Float3x3( pAnimJoints[j].Frames[FrameIndex].Transform.RelRotation );
            const Float3 & Scale = pAnimJoints[j].Frames[FrameIndex].Transform.RelScale;

            // Retrieve relative transform matrix
            FMat3x4_Compose( Position, Rotation, Scale, RelativeTransform );

            // Compute node absolute transform
            FMat3x4_Mult( AbsoluteMatrices[ Joint.Parent + 1 ], RelativeTransform, AbsoluteMatrices[ j + 1 ] );

            // Compute node bindpose transform
            FMat3x4_Mult( AbsoluteMatrices[ j + 1 ], Joint.JointOffsetMatrix, BindposeTransforms[j] );
        }

        for ( int MeshIndex = 0 ; MeshIndex < (int)_Model.Meshes.size() ; MeshIndex++ ) {
            FSkinnedAnimation::FBounds & MeshBounds = _SkinnedAnimation.Bounds[ MeshIndex ];
            const FSkinnedMesh & Mesh = _Model.Meshes[ MeshIndex ];

            Aabb.Clear();

            for ( int v = 0 ; v < Mesh.NumVertices ; v++ ) {
                int VertexId = Mesh.FirstVertex + v;
                const FSkinnedVertex & Vertex = _Model.Vertices[ VertexId ];
                TmpPoint = ComputeVertex( Vertex, BindposeTransforms.data() );
                Aabb.AddPoint( TmpPoint );
            }

            MeshBounds.Frames[ FrameIndex ].Center = Aabb.Center();
            MeshBounds.Frames[ FrameIndex ].HalfSize = Aabb.HalfSize();
        }
    }
#endif

    return Resource;
}

FSkinnedAnimResource * LoadSkinnedAnimEx( const char * _FileName, int _AnimIndex, const FSkinnedMeshResource * _Model ) {

    const aiScene * ImportScene = LoadAssimpScene( _FileName, true );
    if ( !ImportScene ) {
        return NULL;
    }

    FSkinnedAnimResource * Animation = ReadAnimation( _FileName, ImportScene, _AnimIndex, _Model );

    aiReleaseImport( ImportScene );

    return Animation;
}

static TArray< FMeshOffset > MeshOffsets;
static int FirstIndex;
static int MeshFirstVertex = 0;
static void LoadAssimpNode( const aiScene * Scene, const aiNode * Node, const aiMatrix4x4 & ParentTransform, TPodArray< FMeshVertex > & Vertices, TPodArray< unsigned int > & Indices ) {
    aiMatrix4x4 Transform = ParentTransform * Node->mTransformation; // FIXME: multiply order
    FMeshVertex v;

    const float Scale = 1.0f;

    for ( int i = 0 ; i < Node->mNumChildren ; i++ ) {
        LoadAssimpNode( Scene, Node->mChildren[i], Transform, Vertices, Indices );
    }

    aiMatrix4x4 Mat = Transform;
    Mat.Inverse().Transpose();

    aiMatrix3x3 NormalMatrix = aiMatrix3x3(Mat);
    
    for ( int i = 0 ; i < Node->mNumMeshes ; i++ ) {
        const aiMesh * pMesh = Scene->mMeshes[ Node->mMeshes[i] ];

        MeshOffsets.EmplaceBack(
            pMesh->mNumFaces * 3,
            FirstIndex,
            MeshFirstVertex );

        FirstIndex += MeshOffsets.Last().IndexCount;

        for ( unsigned int j = 0 ; j < pMesh->mNumFaces ; j++ ) {
            const aiFace & Face = pMesh->mFaces[ j ];

            for ( unsigned int k = 0 ; k < Face.mNumIndices ; k++ ) {
                const unsigned int & ind = Face.mIndices[ k ];

                Indices.Append( /*MeshFirstVertex + */ind );
            }
        }

        int NumUVChannels = pMesh->GetNumUVChannels();

        for ( unsigned int j = 0 ; j < pMesh->mNumVertices ; j++ ) {

            aiVector3D Tmp = Transform * pMesh->mVertices[ j ];

            v.Position[ 0 ] = Tmp.x * Scale;
            v.Position[ 1 ] = Tmp.y * Scale;
            v.Position[ 2 ] = Tmp.z * Scale;

            Tmp = NormalMatrix * pMesh->mNormals[ j ];
            Tmp.NormalizeSafe();
            v.Normal[ 0 ] = Tmp.x;
            v.Normal[ 1 ] = Tmp.y;
            v.Normal[ 2 ] = Tmp.z;

            if ( pMesh->mTangents && pMesh->mBitangents ) {
                Tmp = NormalMatrix * pMesh->mTangents[ j ];
                Tmp.NormalizeSafe();
                v.Tangent[ 0 ] = Tmp.x;
                v.Tangent[ 1 ] = Tmp.y;
                v.Tangent[ 2 ] = Tmp.z;

                Tmp = NormalMatrix * pMesh->mBitangents[ j ];
                Tmp.NormalizeSafe();

                v.Handedness = FMath::Dot( FMath::Cross( v.Normal, v.Tangent ), Float3( Tmp.x, Tmp.y, Tmp.z ) ) < 0 ? -1.0f : 1.0f;
            }

            if ( NumUVChannels > 0 ) {
                v.TexCoord[ 0 ] = pMesh->mTextureCoords[ 0 ][ j ].x;
                v.TexCoord[ 1 ] = pMesh->mTextureCoords[ 0 ][ j ].y;
            }

            Vertices.Append( v );
        }

        MeshFirstVertex += pMesh->mNumVertices;
    }
}

FStaticMeshResource * LoadStaticMesh( const char * _FileName ) {
    FStaticMeshResource * Resource = GResourceManager->GetResource< FStaticMeshResource >( _FileName );
    if ( Resource->IsLoaded() ) {
        return Resource;
    }

    const aiScene * ImportScene = LoadAssimpScene( _FileName, false );
    if ( !ImportScene ) {
        return NULL;
    }

    TPodArray< FMeshVertex > Vertices;
    TPodArray< unsigned int > Indices;

    MeshFirstVertex = 0;

    MeshOffsets.Clear();
    FirstIndex = 0;

    aiMatrix4x4 Identity;
    aiIdentityMatrix4( &Identity );
    LoadAssimpNode( ImportScene, ImportScene->mRootNode, Identity, Vertices, Indices );
    Resource->SetVertexData( Vertices.ToPtr(), Vertices.Length(), Indices.ToPtr(), Indices.Length(), true );
    Resource->SetMeshOffsets( MeshOffsets.ToPtr(), MeshOffsets.Length() );

    aiReleaseImport( ImportScene );

    return Resource;
}

FSkinnedMeshResource * LoadSkinnedMesh( const char * _FileName, float _Scale ) {
    FSkinnedMeshResource * Resource = GResourceManager->GetResource< FSkinnedMeshResource >( _FileName );
    if ( Resource->IsLoaded() ) {
        return Resource;
    }

    const aiScene * ImportScene = LoadAssimpScene( _FileName, true );
    if ( ImportScene ) {
        ReadSkinnedModel( Resource, ImportScene, _Scale );
        aiReleaseImport( ImportScene );
    }

    return Resource;
}
