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

#include "BladeWorld.h"
#include "FileDump.h"

#include <Engine/Utilites/Public/PolygonClipper.h>
#include <Engine/IO/Public/FileUrl.h>
#include <Engine/Core/Public/Sort.h>

FBladeWorld World;

const Double3 BLADE_COORD_SCALE_D( 0.001, -0.001, -0.001 );
const Float3 BLADE_COORD_SCALE_F( 0.001f, -0.001f, -0.001f );

template<>
void TTriangulatorHelper_ContourVertexPosition( Double3 & _Dst, const Double2 & _Src ) {
    _Dst.X = _Src.X;
    _Dst.Y = _Src.Y;
    _Dst.Z = 0;
}

template<>
void TTriangulatorHelper_TriangleVertexPosition( Double3 & _Dst, const Double2 & _Src ) {
    _Dst.X = _Src.X;
    _Dst.Y = _Src.Y;
    _Dst.Z = 0;
}

template<>
void TTriangulatorHelper_CombineVertex( Double2 & _OutputVertex,
                                        const Double3 & _Position,
                                        const float * _Weights,
                                        const Double2 & _V0,
                                        const Double2 & _V1,
                                        const Double2 & _V2,
                                        const Double2 & _V3 ) {
    _OutputVertex.X = _Position.X;
    _OutputVertex.Y = _Position.Y;
}

template<>
void TTriangulatorHelper_CopyVertex( Double2 & _Dst, const Double2 & _Src ) {
    _Dst = _Src;
}

#if 0
static void PrintContour( const FClipperContour & _Contour ) {
    for ( int i = 0 ; i < _Contour.Length() ; i++ ) {
        Out() << _Contour[i];
    }
}

static void PrintPolygon( const FClipperPolygon & _Polygon ) {
    Out() << "OUTER:";
    PrintContour( _Polygon.Outer );

    if ( _Polygon.Outer.Length() == 0 ) {
        Out() << "EMPTY!!!";
    }

    for ( int i = 0 ; i < _Polygon.Holes.Length() ; i++ ) {
        Out() << "HOLE:";
        PrintContour( _Polygon.Holes[i] );
    }
}

static void PrintPolygons( const TArray< FClipperPolygon > & _Polygons ) {
    for ( int i = 0 ; i < _Polygons.Length() ; i++ ) {
        Out() << "--------- Poly" << i << "----------";
        PrintPolygon( _Polygons[i] );
    }
}
#endif

FBladeWorld::~FBladeWorld() {
    FreeWorld();
}

void FBladeWorld::LoadWorld( const char * _FileName ) {
    File = FFiles::OpenFileFromUrl( _FileName, FFileAbstract::M_Read );

    if ( !File ) {
        return;
    }

    FreeWorld();

    int32_t AtmospheresCount;
    File->ReadSwapInt32( AtmospheresCount );
    Atmospheres.Resize( AtmospheresCount );
    for ( int i = 0 ; i < AtmospheresCount ; i++ ) {
        FBladeMap::FAtmosphereEntry & Atmo = Atmospheres[ i ];
        File->ReadString( Atmo.Name );
        File->Read( &Atmo.Color[ 0 ], 3 );
        File->ReadSwapFloat( Atmo.Intensity );
    }

    int32_t VerticesCount;
    File->ReadSwapInt32( VerticesCount );
    Vertices.Resize( VerticesCount );
    for ( int i = 0 ; i < VerticesCount ; i++ ) {
        File->ReadSwapDouble( Vertices[i].X.Value );
        File->ReadSwapDouble( Vertices[i].Y.Value );
        File->ReadSwapDouble( Vertices[i].Z.Value );
    }

    int32_t SectorsCount;
    File->ReadSwapInt32( SectorsCount );
    Sectors.Resize( SectorsCount );

    for ( int SectorIndex = 0 ; SectorIndex < Sectors.Length() ; SectorIndex++ ) {
        FSector & Sector = Sectors[ SectorIndex ];

        SetDumpLog( true );

        DumpFileOffset( File );

        FString UnknownName;
        File->ReadString( UnknownName );

        Out() << "Loading sector:" << UnknownName;

        File->Read( &Sector.AmbientColor[ 0 ], 3 );
        File->ReadSwapFloat( Sector.AmbientIntensity );        

        SetDumpLog( false );

        DumpFloat( File );

        for ( int i = 0 ; i < 24 ; i++ ) {
            if ( DumpByte( File ) != 0 ) {
                Out() << "not zero";
            }
        }

        for ( int i = 0 ; i < 8 ; i++ ) {
            if ( DumpByte( File ) != 0xCD ) {
                Out() << "not CD";
            }
        }

        for ( int i = 0 ; i < 4 ; i++ ) {
            if ( DumpByte( File ) != 0 ) {
                Out() << "not zero";
            }
        }

        byte r,g,b;
        r = DumpByte( File );
        g = DumpByte( File );
        b = DumpByte( File );

        Out() << r << g << b;

        DumpFloat( File );
        DumpFloat( File );

        for ( int i = 0 ; i < 24 ; i++ ) {
            if ( DumpByte( File ) != 0 ) {
                Out() << "not zero";
            }
        }

        for ( int i = 0 ; i < 8 ; i++ ) {
            if ( DumpByte( File ) != 0xCD ) {
                Out() << "not CD";
            }
        }

        for ( int i = 0 ; i < 4 ; i++ ) {
            if ( DumpByte( File ) != 0 ) {
                Out() << "not zero";
            }
        }

        // Light direction?
        Sector.LightDir.X = DumpDouble( File );
        Sector.LightDir.Y = -DumpDouble( File );
        Sector.LightDir.Z = -DumpDouble( File );
        Sector.LightDir.NormalizeSelf();

        SetDumpLog( true );

        int32_t FaceCount;
        File->ReadSwapInt32( FaceCount );

        if (FaceCount<4||FaceCount>100 ) {
            Out() << "WARNING: FILE READ ERROR.. SOMETHING GO WRONG!";
            Sectors.Resize(SectorIndex);
            assert(0);
            break;
        }

        Sector.Faces.Resize( FaceCount );

        for ( int FaceIndex = 0 ; FaceIndex < FaceCount ; FaceIndex++ ) {
            Sector.Faces[FaceIndex] = CreateFace();
            Sector.Faces[FaceIndex]->SectorIndex = SectorIndex;
            LoadFace( Sector.Faces[FaceIndex] );
        }
    }

#if 0
    typedef enum FileEndChanks {
        FEC_Unk1 = 0x00003A99, //15001
        FEC_Unk2 = 0x00003A9A, //15002
    } FileEndChanks;


    int elemCount = DumpInt( File );
    for (int i = 0; i < elemCount; i++)
    {
        int Type = DumpInt( File );
        switch ( Type ) {
            case FEC_Unk1: { // pos += __addStructureAt(pos, "FileEndUnk1", "") ;
                DumpUnknownBytes( File, 11 );   // blob Unk1[ 11 ];

                // Vertex Unk2;
                DumpDouble( File );
                DumpDouble( File );
                DumpDouble( File );

                DumpInt( File );

                break;
            }
            case FEC_Unk2: { // pos += __addStructureAt(pos, "FileEndUnk2", "") ;
                DumpUnknownBytes( File, 47 );   // blob Unk1[ 47 ];

                DumpDouble( File );
                DumpDouble( File );
                DumpDouble( File );

                int NumIndices = DumpInt( File );
                for ( int j = 0 ; j < NumIndices ; j++ ) {
                    DumpInt( File );
                }
                
                break;
            }
            default:
                break;
        }
    }

    //pos += __addStructureAt(pos, "Vertex", "");
    Bounds.mins[0] = DumpDouble( File );
    Bounds.mins[1] = DumpDouble( File );
    Bounds.mins[2] = DumpDouble( File );
    Bounds.mins *= BLADE_COORD_SCALE;

    //pos += __addStructureAt(pos, "Vertex", "");
    Bounds.maxs[0] = DumpDouble( File );
    Bounds.maxs[1] = DumpDouble( File );
    Bounds.maxs[2] = DumpDouble( File );
    Bounds.maxs *= BLADE_COORD_SCALE;

    //pos += secCount * 4;
    File->Seek( SectorsCount * 4, FFileAbstract::SeekCur );

    //__addStructureAt(pos, "StringArray", "");
    TArray< FString > Strings;
    Strings.Resize( DumpInt( File ) );
    for ( int i = 0 ; i < Strings.Length() ; i++ ) {
        Strings[i] = DumpString( File );
    }
#endif
    FFiles::CloseFile( File );

    WorldGeometryPostProcess();
}

FBladeWorld::FFace * FBladeWorld::CreateFace() {
    Faces.Append( new FFace );
    return Faces.Last();
}
FBladeWorld::FPortal * FBladeWorld::CreatePortal() {
    Portals.Append( new FPortal );
    return Portals.Last();
}

FBladeWorld::FBSPNode * FBladeWorld::CreateBSPNode() {
    BSPNodes.Append( new FBSPNode );
    return BSPNodes.Last();
}

void FBladeWorld::LoadFace( FFace * _Face ) {
    File->ReadSwapInt32( _Face->Type );

    switch ( _Face->Type ) {
        case FT_SimpleFace:
            Out() << "SimpleFace";
            LoadSimpleFace( _Face );
            break;
        case FT_Portal:
            Out() << "Portal";
            LoadPortalFace( _Face );
            break;
        case FT_Face:
            Out() << "Face";
            LoadFaceWithHole( _Face );
            break;
        case FT_FaceBSP:
            Out() << "FaceBSP";
            LoadFaceBSP( _Face );
            break;
        case FT_Skydome:
            Out() << "Skydome";
            LoadSkydomeFace( _Face );
            break;
        default:
            assert(0);
            break;
    }
}

void FBladeWorld::ReadIndices( TPodArray< unsigned int > & _Indices ) {
    int32_t NumIndices;
    File->ReadSwapInt32( NumIndices );
    _Indices.Resize( NumIndices );
    for ( int k = 0 ; k < NumIndices ; k++ ) {
        File->ReadSwapUInt32( _Indices[k] );
    }
}

void FBladeWorld::ReadWinding( PolygonD & _Winding ) {
    int32_t NumVertices;
    int Index;
    File->ReadSwapInt32( NumVertices );
    if ( NumVertices == 0 || NumVertices > 32 ) {
        Out() << "WARNING" << NumVertices << "SOMETHING GO WRONG!";
        assert( 0 );
    }
    _Winding.Resize( NumVertices );
    for ( int k = 0 ; k < NumVertices ; k++ ) {
        File->ReadSwapInt32( Index );
        _Winding[ NumVertices - k - 1 ] = Vertices[ Index ];
    }
}

void FBladeWorld::LoadSimpleFace( FFace * _Face ) {
    // Face plane
    File->ReadSwapVector( _Face->Plane.Normal );
    File->ReadSwapDouble( _Face->Plane.D.Value );

    // FIXME: What is it?
    File->ReadSwapUInt64( _Face->UnknownSignature );
    if ( _Face->UnknownSignature != 3 ) {
        Out() << "Face signature" << _Face->UnknownSignature;
    }

    File->ReadString( _Face->TextureName );

    File->ReadSwapVector( _Face->TexCoordAxis[0] );
    File->ReadSwapVector( _Face->TexCoordAxis[1] );
    File->ReadSwapFloat( _Face->TexCoordOffset[0] );
    File->ReadSwapFloat( _Face->TexCoordOffset[1] );

    _Face->TexCoordOffset[0] = -_Face->TexCoordOffset[0];
    _Face->TexCoordOffset[1] = -_Face->TexCoordOffset[1];

    // 8 zero bytes?
    SetDumpLog( false );
    for ( int k = 0 ; k < 8 ; k++ ) {
        if ( DumpByte( File ) != 0 ) {
            Out() << "not zero!";
        }
    }
    SetDumpLog( true );

    // Winding
    ReadIndices( _Face->Indices );
}

void FBladeWorld::LoadPortalFace( FFace * _Face ) {
    // Face plane
    File->ReadSwapVector( _Face->Plane.Normal );
    File->ReadSwapDouble( _Face->Plane.D.Value );

    // Winding
    ReadIndices( _Face->Indices );

    FPortal * Portal = CreatePortal();

    Portal->Face = _Face;
    File->ReadSwapInt32( Portal->ToSector );
    Portal->Winding.Resize( _Face->Indices.Length() );
    for ( int k = 0 ; k < _Face->Indices.Length() ; k++ ) {
        Portal->Winding[ _Face->Indices.Length() - k - 1 ] = Vertices[ _Face->Indices[k] ];
    }

    Sectors[ _Face->SectorIndex ].Portals.Append( Portal );

    // FIXME: What is it?
    SetDumpLog( false );
    DumpUnknownBytes( File, 8 );
    SetDumpLog( true );

    File->ReadString( _Face->TextureName );

    File->ReadSwapVector( _Face->TexCoordAxis[0] );
    File->ReadSwapVector( _Face->TexCoordAxis[1] );
    File->ReadSwapFloat( _Face->TexCoordOffset[0] );
    File->ReadSwapFloat( _Face->TexCoordOffset[1] );

    _Face->TexCoordOffset[0] = -_Face->TexCoordOffset[0];
    _Face->TexCoordOffset[1] = -_Face->TexCoordOffset[1];

    // 8 zero bytes?
    SetDumpLog( false );
    for ( int k = 0 ; k < 8 ; k++ ) {
        if ( DumpByte( File ) != 0 ) {
            Out() << "not zero!";
        }
    }
    SetDumpLog( true );
}

void FBladeWorld::LoadFaceWithHole( FFace * _Face ) {
    // Face plane
    File->ReadSwapVector( _Face->Plane.Normal );
    File->ReadSwapDouble( _Face->Plane.D.Value );

    // FIXME: What is it?
    File->ReadSwapUInt64( _Face->UnknownSignature );
    if ( _Face->UnknownSignature != 3 ) {
        Out() << "Face signature" << _Face->UnknownSignature;
    }

    File->ReadString( _Face->TextureName );

    File->ReadSwapVector( _Face->TexCoordAxis[0] );
    File->ReadSwapVector( _Face->TexCoordAxis[1] );
    File->ReadSwapFloat( _Face->TexCoordOffset[0] );
    File->ReadSwapFloat( _Face->TexCoordOffset[1] );

    _Face->TexCoordOffset[0] = -_Face->TexCoordOffset[0];
    _Face->TexCoordOffset[1] = -_Face->TexCoordOffset[1];

    // 8 zero bytes?
    SetDumpLog( false );
    for ( int k = 0 ; k < 8 ; k++ ) {
        if ( DumpByte( File ) != 0 ) {
            Out() << "not zero!";
        }
    }
    SetDumpLog( true );

    // Winding
    PolygonD Winding;
    ReadWinding( Winding );

    // Winding hole
    PolygonD Hole;
    ReadWinding( Hole );

    FClipper Clipper;

    PlaneD Plane = _Face->Plane;//Winding.CalcPlane();

    Clipper.SetNormal( Plane.Normal );
    Clipper.AddContour3D( Winding.ToPtr(), Winding.Length(), true );
    Clipper.AddContour3D( Hole.ToPtr(), Hole.Length(), false );

    TArray< FClipperPolygon > ResultPolygons;
    Clipper.Execute( FClipper::CLIP_DIFF, ResultPolygons );

    //PrintPolygons( ResultPolygons );

    typedef TTriangulator< Double2, Double2 > FTriangulator;
    FTriangulator Triangulator;
    TPodArray< FTriangulator::FPolygon * > Polygons;
    TArray< Double2 > ResultVertices;
    for ( int i = 0 ; i < ResultPolygons.Length() ; i++ ) {
        FTriangulator::FPolygon * Polygon = new FTriangulator::FPolygon;

        Polygon->OuterContour = ResultPolygons[i].Outer.ToPtr();
        Polygon->OuterContourEnd = ResultPolygons[i].Outer.ToPtr() + ResultPolygons[i].Outer.Length();

        Polygon->HoleContours.Resize( ResultPolygons[i].Holes.Length() );
        Polygon->HoleContoursEnd.Resize( ResultPolygons[i].Holes.Length() );
        for ( int j = 0 ; j < ResultPolygons[i].Holes.Length() ; j++ ) {
            Polygon->HoleContours[j] = ResultPolygons[i].Holes[j].ToPtr();
            Polygon->HoleContoursEnd[j] = ResultPolygons[i].Holes[j].ToPtr() + ResultPolygons[i].Holes[j].Length();
        }

        Polygon->Normal.X = 0;
        Polygon->Normal.Y = 0;
        Polygon->Normal.Z = 1;

        Polygons.Append( Polygon );
    }
    Triangulator.TriangulatePolygons( Polygons, ResultVertices, _Face->Indices );

    // free polygons
    for ( int i = 0 ; i < Polygons.Length() ; i++ ) {
        FTriangulator::FPolygon * Polygon = Polygons[ i ];
        delete Polygon;
    }

    const Double3x3 & TransformMatrix = Clipper.GetTransform3D();

    _Face->Vertices.Resize( ResultVertices.Length() );
    for ( int k = 0 ; k < ResultVertices.Length() ; k++ ) {
        _Face->Vertices[k] = TransformMatrix * Double3( ResultVertices[k], Plane.Dist() );
    }

    FPortal * Portal = CreatePortal();

    Portal->Face = _Face;
    File->ReadSwapInt32( Portal->ToSector );
    Portal->Winding = Hole;

    Sectors[ _Face->SectorIndex ].Portals.Append( Portal );

    int32_t Count;
    File->ReadSwapInt32( Count );
    Portal->Planes.Resize( Count );
    for ( int k = 0 ; k < Count ; k++ ) {
        File->ReadSwapVector( Portal->Planes[ k ].Normal );
        File->ReadSwapDouble( Portal->Planes[ k ].D.Value );
    }
}

static int VerticesOffset( const PlaneD & _Plane, const TPodArray< Double3 > & _Vertices ) {
    int Front = 0;
    int Back = 0;

    for ( int i = 0 ; i < _Vertices.Length() ; i++ ) {
        EPlaneSide Side = _Plane.SideOffset( _Vertices[i], 0.0 );

        if ( Side == EPlaneSide::Front ) {
            Front++;
        }
        if ( Side == EPlaneSide::Back ) {
            Back++;
        }
    }

    if ( Front > Back ) {
        return 0;
    }

    return 1;
}

void FBladeWorld::FilterWinding_r( FBladeWorld::FFace * _Face, FBSPNode * _Node, FBSPNode * _Leaf ) {
    if ( _Node->Type == NT_Leaf ) {
        return;
    }

    int Offset = VerticesOffset( _Node->Plane, _Leaf->Vertices );

    if ( Offset == 1 ) {        
        if ( _Node->Type == NT_TexInfo ) {
            _Leaf->TextureName = _Node->TextureName;
            _Leaf->TexCoordAxis[ 0 ] = _Node->TexCoordAxis[ 0 ];
            _Leaf->TexCoordAxis[ 1 ] = _Node->TexCoordAxis[ 1 ];
            _Leaf->TexCoordOffset[ 0 ] = _Node->TexCoordOffset[ 0 ];
            _Leaf->TexCoordOffset[ 1 ] = _Node->TexCoordOffset[ 1 ];
        }
        FilterWinding_r( _Face, _Node->Children[1], _Leaf );
    }    
}

void FBladeWorld::CreateWindings_r( FBladeWorld::FFace * _Face, const TArray< FClipperContour > & _Holes, PolygonD * _Winding, FBSPNode * _Node ) {
    if ( _Node->Type == NT_Leaf ) {
        assert( _Winding != NULL );

        FClipper Clipper;
        Clipper.SetNormal( _Face->Plane.Normal );
        Clipper.AddContour3D( _Winding->ToPtr(), _Winding->Length(), true );

        for ( int i = 0 ; i < _Holes.Length() ; i++ ) {
            Clipper.AddContour2D( _Holes[ i ].ToPtr(), _Holes[ i ].Length(), false, true );
        }

        TArray< FClipperPolygon > ResultPolygons;
        Clipper.Execute( FClipper::CLIP_DIFF, ResultPolygons );

        //PrintPolygons( ResultPolygons );

        typedef TTriangulator< Double2, Double2 > FTriangulator;
        FTriangulator Triangulator;
        TPodArray< FTriangulator::FPolygon * > Polygons;
        TArray< Double2 > ResultVertices;
        TPodArray< unsigned int > ResultIndices;
        for ( int i = 0 ; i < ResultPolygons.Length() ; i++ ) {
            FTriangulator::FPolygon * Polygon = new FTriangulator::FPolygon;

            Polygon->OuterContour = ResultPolygons[ i ].Outer.ToPtr();
            Polygon->OuterContourEnd = ResultPolygons[ i ].Outer.ToPtr() + ResultPolygons[ i ].Outer.Length();

            Polygon->HoleContours.Resize( ResultPolygons[ i ].Holes.Length() );
            Polygon->HoleContoursEnd.Resize( ResultPolygons[ i ].Holes.Length() );
            for ( int j = 0 ; j < ResultPolygons[ i ].Holes.Length() ; j++ ) {
                Polygon->HoleContours[ j ] = ResultPolygons[ i ].Holes[ j ].ToPtr();
                Polygon->HoleContoursEnd[ j ] = ResultPolygons[ i ].Holes[ j ].ToPtr() + ResultPolygons[ i ].Holes[ j ].Length();
            }

            Polygon->Normal.X = 0;
            Polygon->Normal.Y = 0;
            Polygon->Normal.Z = 1;

            Polygons.Append( Polygon );
        }
        Triangulator.TriangulatePolygons( Polygons, ResultVertices, ResultIndices );

        // free polygons
        for ( int i = 0 ; i < Polygons.Length() ; i++ ) {
            FTriangulator::FPolygon * Polygon = Polygons[ i ];
            delete Polygon;
        }

        const Double3x3 & TransformMatrix = Clipper.GetTransform3D();

        _Node->Vertices.Resize( ResultVertices.Length() );
        for ( int k = 0 ; k < ResultVertices.Length() ; k++ ) {
            _Node->Vertices[ k ] = TransformMatrix * Double3( ResultVertices[ k ], _Face->Plane.Dist() );
        }
        _Node->Indices = ResultIndices;

        Leafs.Append( _Node );

        return;
    }

    PolygonD * Front = NULL;
    PolygonD * Back = NULL;

    assert( _Winding != NULL );
    _Winding->Split( _Node->Plane, &Front, &Back, 0.0 );

    CreateWindings_r( _Face, _Holes, Front, _Node->Children[0] );
    CreateWindings_r( _Face, _Holes, Back, _Node->Children[1] );

    delete Front;
    delete Back;
}

void FBladeWorld::LoadFaceBSP( FFace * _Face ) {
    // Face plane
    File->ReadSwapVector( _Face->Plane.Normal );
    File->ReadSwapDouble( _Face->Plane.D.Value );

    // FIXME: What is it?
    File->ReadSwapUInt64( _Face->UnknownSignature );
    if ( _Face->UnknownSignature != 3 ) {
        Out() << "Face signature" << _Face->UnknownSignature;
    }

    File->ReadString( _Face->TextureName );

    File->ReadSwapVector( _Face->TexCoordAxis[0] );
    File->ReadSwapVector( _Face->TexCoordAxis[1] );
    File->ReadSwapFloat( _Face->TexCoordOffset[0] );
    File->ReadSwapFloat( _Face->TexCoordOffset[1] );

    _Face->TexCoordOffset[0] = -_Face->TexCoordOffset[0];
    _Face->TexCoordOffset[1] = -_Face->TexCoordOffset[1];

    // 8 zero bytes?
    SetDumpLog( false );
    for ( int k = 0 ; k < 8 ; k++ ) {
        if ( DumpByte( File ) != 0 ) {
            Out() << "not zero!";
        }
    }
    SetDumpLog( true );

    // Winding
    ReadIndices( _Face->Indices );

    PolygonD Winding;

    Winding.Resize( _Face->Indices.Length() );
    for ( int k = 0 ; k < _Face->Indices.Length() ; k++ ) {
        Winding[ k ] = Vertices[ _Face->Indices[ k ] ];
    }
    Winding.Reverse();

    PlaneD Plane = _Face->Plane;

    TArray< FClipperContour > Holes;

    int NumHoles;
    File->ReadSwapInt32( NumHoles );
    if ( NumHoles > 0 ) {
        FClipper HolesUnion;
        PolygonD Hole;

        HolesUnion.SetNormal( Plane.Normal );

        for ( int c = 0 ; c < NumHoles ; c++ ) {
            ReadWinding( Hole );

            HolesUnion.AddContour3D( Hole.ToPtr(), Hole.Length(), true );

            FPortal * Portal = CreatePortal();

            Portal->Face = _Face;
            File->ReadSwapInt32( Portal->ToSector );
            Portal->Winding = Hole;

            Sectors[ _Face->SectorIndex ].Portals.Append( Portal );

            int32_t Count;
            File->ReadSwapInt32( Count );
            Portal->Planes.Resize( Count );
            for ( int k = 0 ; k < Count ; k++ ) {
                File->ReadSwapVector( Portal->Planes[k].Normal );
                File->ReadSwapDouble( Portal->Planes[k].D.Value );
            }
        }

        // FIXME: Union of hulls may produce inner holes!
        HolesUnion.Execute( FClipper::CLIP_UNION, Holes );
    }

    _Face->Root = ReadBSPNode_r( _Face );

    // Create windings and fill Leafs
    CreateWindings_r( _Face, Holes, &Winding, _Face->Root );

    // Create subfaces
    for ( int i = 0 ; i < Leafs.Length() ; i++ ) {
        if ( Leafs[i]->Vertices.Length() > 0 && Leafs[i]->Indices.Length() > 0 ) {
            Leafs[i]->TextureName = _Face->TextureName;
            Leafs[i]->TexCoordAxis[0] = _Face->TexCoordAxis[0];
            Leafs[i]->TexCoordAxis[1] = _Face->TexCoordAxis[1];
            Leafs[i]->TexCoordOffset[0] = _Face->TexCoordOffset[0];
            Leafs[i]->TexCoordOffset[1] = _Face->TexCoordOffset[1];

            FilterWinding_r( _Face, _Face->Root, Leafs[i] );

            FBladeWorld::FFace * SubFace = CreateFace();
            SubFace->Type = FT_Subface;
            SubFace->TextureName = Leafs[i]->TextureName;
            SubFace->TexCoordAxis[ 0 ] = Leafs[i]->TexCoordAxis[ 0 ];
            SubFace->TexCoordAxis[ 1 ] = Leafs[i]->TexCoordAxis[ 1 ];
            SubFace->TexCoordOffset[ 0 ] = Leafs[i]->TexCoordOffset[ 0 ];
            SubFace->TexCoordOffset[ 1 ] = Leafs[i]->TexCoordOffset[ 1 ];
            SubFace->SectorIndex = _Face->SectorIndex;
            SubFace->Plane = _Face->Plane;
            SubFace->Vertices = Leafs[i]->Vertices;
            SubFace->Indices = Leafs[i]->Indices;

            _Face->SubFaces.Append( SubFace );
        } else {
            Out() << "Leaf with no vertices";
        }
    }
    Leafs.Clear();

    //assert( _Face->SubFaces.Length() > 0 );
}

static void ConvertFacePlane( PlaneD & _Plane ) {
    _Plane.Normal.Y = -_Plane.Normal.Y;
    _Plane.Normal.Z = -_Plane.Normal.Z;
    _Plane.D *= 0.001;
}

FBladeWorld::FBSPNode * FBladeWorld::ReadBSPNode_r( FFace * _Face ) {
    // Special thanks to ASP for node parsing

    FBSPNode * Node = CreateBSPNode();

    File->ReadSwapInt32( ( int32_t & )Node->Type );

    if ( Node->Type == NT_Leaf ) {

        Node->Children[0] = NULL;
        Node->Children[1] = NULL;

        int Count = DumpInt( File );

        Node->Unknown.Resize( Count );

        for ( int i = 0; i < Count; i++ ) {
            FLeafIndices & Unknown = Node->Unknown[ i ];

            Unknown.UnknownIndex = DumpInt( File );

            uint32_t IndicesCount = DumpInt( File );
            Unknown.Indices.Resize( IndicesCount );
            for ( int j = 0; j < IndicesCount; j++ ) {
                Unknown.Indices[ j ] = DumpInt( File );
            }
        }

        return Node;
    }    

    Node->Children[0] = ReadBSPNode_r( _Face );
    Node->Children[1] = ReadBSPNode_r( _Face );

    File->ReadSwapVector( Node->Plane.Normal );
    File->ReadSwapDouble( Node->Plane.D.Value );

    if ( Node->Type == NT_TexInfo ) {
        File->ReadSwapUInt64( Node->UnknownSignature );
        File->ReadString( Node->TextureName );

        File->ReadSwapVector( Node->TexCoordAxis[0] );
        File->ReadSwapVector( Node->TexCoordAxis[1] );
        File->ReadSwapFloat( Node->TexCoordOffset[0] );
        File->ReadSwapFloat( Node->TexCoordOffset[1] );

        Node->TexCoordOffset[ 0 ] = -Node->TexCoordOffset[ 0 ];
        Node->TexCoordOffset[ 1 ] = -Node->TexCoordOffset[ 1 ];

        // 8 zero bytes?
        for ( int k = 0 ; k < 8 ; k++ ) {
            if ( DumpByte( File ) != 0 ) {
                Out() << "not zero!";
            }
        }
    }

    return Node;
}

void FBladeWorld::LoadSkydomeFace( FFace * _Face ) {
    // Face plane
    File->ReadSwapVector( _Face->Plane.Normal );
    File->ReadSwapDouble( _Face->Plane.D.Value );

    // Winding
    ReadIndices( _Face->Indices );
}

#define USE_TEXCOORD_CORRECTION

static void RecalcTextureCoords( FBladeWorld::FFace * _Face, FMeshVertex * _Vertices, int _NumVertices, int _TexWidth, int _TexHeight ) {
#ifdef USE_TEXCOORD_CORRECTION
    Float2 Mins( std::numeric_limits< float >::max() );
#endif

    double tx, ty;
    double sx = 1.0 / _TexWidth;
    double sy = 1.0 / _TexHeight;

    for ( int k = 0 ; k < _NumVertices ; k++ ) {
        FMeshVertex & Vert = _Vertices[ k ];

        tx = FMath::Dot( _Face->TexCoordAxis[ 0 ], Double3( Vert.Position ) ) + double(_Face->TexCoordOffset[ 0 ]);
        ty = FMath::Dot( _Face->TexCoordAxis[ 1 ], Double3( Vert.Position ) ) + double(_Face->TexCoordOffset[ 1 ]);
        tx *= sx;
        ty *= sy;

        Vert.TexCoord.X = tx;
        Vert.TexCoord.Y = ty;

#ifdef USE_TEXCOORD_CORRECTION
        Mins.X = FMath::Min( Vert.TexCoord.X, Mins.X );
        Mins.Y = FMath::Min( Vert.TexCoord.Y, Mins.Y );
#endif
    }

#ifdef USE_TEXCOORD_CORRECTION
    // Скорректировать текстурные координаты, чтобы они были ближе к нулю
    Mins.X = Mins.X.Floor();
    Mins.Y = Mins.Y.Floor();
    for ( int k = 0 ; k < _NumVertices ; k++ ) {
        FMeshVertex & Vert = _Vertices[ k ];
        Vert.TexCoord -= Mins;
    }
#endif
}

// Generate world mesh
void FBladeWorld::WorldGeometryPostProcess() {
    FMeshOffset MeshOffset;
    FMeshVertex Vertex;
    int FirstVertex;

    memset( &Vertex, 0, sizeof( Vertex ) );
    FirstVertex = 0;

    MeshOffset.BaseVertexLocation = 0;
    MeshOffset.StartIndexLocation = 0;
    MeshOffset.IndexCount = 0;

    ShadowCasterMeshOffset.BaseVertexLocation = 0;
    ShadowCasterMeshOffset.StartIndexLocation = 0;
    ShadowCasterMeshOffset.IndexCount = 0;

    TPodArray< int > VerticesCounts;
    VerticesCounts.Resize( Sectors.Length() );

    Bounds.Clear();
    HasSky = false;

    for ( int SectorIndex = 0 ; SectorIndex < Sectors.Length() ; SectorIndex++ ) {
        FSector & Sector = Sectors[ SectorIndex ];

        Sector.Bounds.Clear();
        Sector.Centroid = Float3(0);

        VerticesCounts[ SectorIndex ] = 0;
    }

    for ( int FaceIndex = 0 ; FaceIndex < Faces.Length() ; FaceIndex++ ) {
        FFace * Face = Faces[ FaceIndex ];
        FSector & Sector = Sectors[ Face->SectorIndex ];

        if ( Face->Type == FT_Skydome ) {
            Face->CastShadows = false;
            HasSky = true;
        } else if ( Face->TextureName.Length() == 0 ) {
            Face->CastShadows = false;
        } else if ( Face->TextureName == "blanca" ) {
            Face->CastShadows = false;
            HasSky = true;
        } else if ( Sector.Portals.Length() == 0 ) {
            Face->CastShadows = false;
        } else {
            Face->CastShadows = true;
        }
    }

    class FaceSort : public TQuickSort< FFace *, FaceSort > {
    public:
        bool operator() ( FFace * _First, FFace * _Second ) {
            return _First->CastShadows < _Second->CastShadows;
        }
    };

    FaceSort().Sort( Faces.ToPtr(), Faces.Length() );

    for ( int FaceIndex = 0 ; FaceIndex < Faces.Length() ; FaceIndex++ ) {

        FFace * Face = Faces[ FaceIndex ];
        FSector & Sector = Sectors[ Face->SectorIndex ];
        int & VerticesCount = VerticesCounts[ Face->SectorIndex ];

        ConvertFacePlane( Face->Plane );

        if ( Face->Type == FT_Portal || Face->Type == FT_FaceBSP ) {
            continue;
        }

        if ( Face->Indices.Length() < 3 ) {
            continue;
        }

        MeshOffset.StartIndexLocation = MeshIndices.Length();

        if ( Face->Vertices.Length() > 0 ) {
            for ( int v = 0 ; v < Face->Vertices.Length() ; v++ ) {
                Vertex.Position.X = Face->Vertices[ v ].X;
                Vertex.Position.Y = Face->Vertices[ v ].Y;
                Vertex.Position.Z = Face->Vertices[ v ].Z;
                Vertex.Normal = Float3( Face->Plane.Normal );

                MeshVertices.Append( Vertex );
            }

            for ( int j = 0 ; j < Face->Indices.Length() ; j++ ) {
                MeshIndices.Append( FirstVertex + Face->Indices[ j ] );
            }

            MeshOffset.IndexCount = Face->Indices.Length();

            RecalcTextureCoords( Face, &MeshVertices[ FirstVertex ], Face->Vertices.Length(), 256, 256 );

            for ( int v = 0 ; v < Face->Vertices.Length() ; v++ ) {
                FMeshVertex & Vert = MeshVertices[ v + FirstVertex ];

                Vert.Position *= BLADE_COORD_SCALE_F;

                Sector.Bounds.AddPoint( Vert.Position );
                Sector.Centroid += Vert.Position;
                VerticesCount++;
            }

            FirstVertex += Face->Vertices.Length();

        } else {
            for ( int j = 0 ; j < Face->Indices.Length() ; j++ ) {
                int Index = Face->Indices[ j ];

                Vertex.Position = Float3( Vertices[ Index ] );
                Vertex.Normal = Float3( Face->Plane.Normal );

                MeshVertices.Append( Vertex );
            }

            // triangle fan -> triangles
            for ( int j = 0 ; j < Face->Indices.Length() - 2 ; j++ ) {
                MeshIndices.Append( FirstVertex + 0 );
                MeshIndices.Append( FirstVertex + Face->Indices.Length() - j - 2 );
                MeshIndices.Append( FirstVertex + Face->Indices.Length() - j - 1 );
            }

            MeshOffset.IndexCount = ( Face->Indices.Length() - 2 ) * 3;

            RecalcTextureCoords( Face, &MeshVertices[ FirstVertex ], Face->Indices.Length(), 256, 256 );

            for ( int v = 0 ; v < Face->Indices.Length() ; v++ ) {
                FMeshVertex & Vert = MeshVertices[ v + FirstVertex ];

                Vert.Position *= BLADE_COORD_SCALE_F;

                Sector.Bounds.AddPoint( Vert.Position );
                Sector.Centroid += Vert.Position;
                VerticesCount++;
            }

            FirstVertex += Face->Indices.Length();
        }

        if ( Face->CastShadows ) {
            if ( ShadowCasterMeshOffset.IndexCount == 0 ) {
                ShadowCasterMeshOffset.StartIndexLocation = MeshOffset.StartIndexLocation;
            }

            ShadowCasterMeshOffset.IndexCount += MeshOffset.IndexCount;
        }

        MeshOffsets.Append( MeshOffset );
        MeshFaces.Append( Face );
    }

    CalcTangentSpace( MeshVertices.ToPtr(), MeshVertices.Length(), MeshIndices.ToPtr(), MeshIndices.Length() );

    for ( int SectorIndex = 0 ; SectorIndex < Sectors.Length() ; SectorIndex++ ) {
        Sectors[ SectorIndex ].Centroid *= 1.0f / VerticesCounts[ SectorIndex ];

        Bounds.AddAABB( Sectors[ SectorIndex ].Bounds );
    }
}

void FBladeWorld::FreeWorld() {
    Atmospheres.Clear();
    Vertices.Clear();
    Sectors.Clear();
    MeshOffsets.Clear();
    MeshVertices.Clear();
    MeshIndices.Clear();
    MeshFaces.Clear();

    for ( int i = 0 ; i < Portals.Length() ; i++ ) {
        delete Portals[i];
    }
    Portals.Clear();

    for ( int i = 0 ; i < Faces.Length() ; i++ ) {
        delete Faces[i];
    }
    Faces.Clear();

    for ( int i = 0 ; i < BSPNodes.Length() ; i++ ) {
        delete BSPNodes[i];
    }
    BSPNodes.Clear();
}


#if 0
        FClipper Clipper;
        PolygonD Hole;

        Clipper.SetNormal( Plane.normal );
        Clipper.AddContour3D( Winding.ToPtr(), Winding.Length(), true );

        for ( int c = 0 ; c < NumHoles ; c++ ) {
            ReadWinding( Hole );

            Clipper.AddContour3D( Hole.ToPtr(), Hole.Length(), false );

            FPortal * Portal = CreatePortal();

            Portal->Face = _Face;
            File->ReadSwapInt32( Portal->ToSector );
            Portal->Winding = Hole;

            Sectors[ _Face->SectorIndex ].Portals.Append( Portal );

            int32_t Count;
            File->ReadSwapInt32( Count );
            Portal->Planes.Resize( Count );
            for ( int k = 0 ; k < Count ; k++ ) {
                File->ReadSwapVector( Portal->Planes[k].normal );
                File->ReadSwapDouble( Portal->Planes[k].d );
            }
        }

        TArray< FClipperPolygon > ResultPolygons;
        Clipper.Execute( FClipper::CLIP_DIFF, ResultPolygons );

        //PrintPolygons( ResultPolygons );

        typedef TTriangulator< Double2, Double2 > FTriangulator;
        FTriangulator Triangulator;
        TPodArray< FTriangulator::FPolygon * > Polygons;
        TArray< Double2 > ResultVertices;
        TPodArray< unsigned int > ResultIndices;
        for ( int i = 0 ; i < ResultPolygons.Length() ; i++ ) {
            FTriangulator::FPolygon * Polygon = new FTriangulator::FPolygon;

            Polygon->OuterContour = ResultPolygons[i].Outer.ToPtr();
            Polygon->OuterContourEnd = ResultPolygons[i].Outer.ToPtr() + ResultPolygons[i].Outer.Length();

            Polygon->HoleContours.Resize( ResultPolygons[i].Holes.Length() );
            Polygon->HoleContoursEnd.Resize( ResultPolygons[i].Holes.Length() );
            for ( int j = 0 ; j < ResultPolygons[i].Holes.Length() ; j++ ) {
                Polygon->HoleContours[j] = ResultPolygons[i].Holes[j].ToPtr();
                Polygon->HoleContoursEnd[j] = ResultPolygons[i].Holes[j].ToPtr() + ResultPolygons[i].Holes[j].Length();
            }

            Polygon->Normal.X = 0;
            Polygon->Normal.Y = 0;
            Polygon->Normal.Z = 1;

            Polygons.Append( Polygon );
        }
        Triangulator.TriangulatePolygons( Polygons, ResultVertices, ResultIndices );

        // free polygons
        for ( int i = 0 ; i < Polygons.Length() ; i++ ) {
            FTriangulator::FPolygon * Polygon = Polygons[ i ];
            delete Polygon;
        }

        const Double3x3 & TransformMatrix = Clipper.GetTransform3D();

        _Face->Vertices.Resize( ResultVertices.Length() );
        for ( int k = 0 ; k < ResultVertices.Length() ; k++ ) {
            _Face->Vertices[k] = TransformMatrix * Double3( ResultVertices[k], Plane.Dist() );
        }
        _Face->Indices = ResultIndices;

#endif
