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

#include "BladeModel.h"
#include "BladeWorld.h"

#include "FileDump.h"

#include <Engine/IO/Public/FileUrl.h>
#include <Engine/Resource/Public/ResourceManager.h>

#include <map>

#pragma warning( disable : 4189 )
#pragma warning( disable : 4101 )

void FBladeModel::LoadModel( const char * _FileName ) {
    struct FVertex {
        Double3 Position;
        Double3 Normal;
        TPodArray< int > Polygons;
    };

    struct FPolygon {
        int Indices[3];
        Float2 TexCoords[3];
        int Unknown;
        FString TextureName;
        int Use;
    };

    TPodArray< FMeshVertex > MeshVertices;
    TPodArray< unsigned int > MeshIndices;
    TArray< FMeshOffset > MeshOffsets;
    TArray< FVertex > Vertices;
    TArray< FPolygon > Polygons;

    FFileAbstract * File = FFiles::OpenFileFromUrl( _FileName, FFileAbstract::M_Read );
    if ( !File ) {
        return;
    }

    SetDumpLog( false );

    FString Name = DumpString( File );

    int VerticesCount = DumpInt( File );
    Vertices.Resize( VerticesCount );
    for ( int i = 0 ; i < VerticesCount ; i++ ) {
        FVertex & v = Vertices[ i ];

        v.Position.X = DumpDouble( File );
        v.Position.Y = DumpDouble( File );
        v.Position.Z = DumpDouble( File );

        v.Position *= BLADE_COORD_SCALE_D;

        v.Normal.X = DumpDouble( File );
        v.Normal.Y = -DumpDouble( File );
        v.Normal.Z = -DumpDouble( File );
    }

    std::map< std::string, TPodArray< int > > TextureMap;

    Out() << "Texture Unknown Ints:";

    int PolygonsCount = DumpInt( File );
    Polygons.Resize( PolygonsCount );

    for ( int i = 0 ; i < PolygonsCount ; i++ ) {

        FPolygon & Polygon = Polygons[i];

        Polygon.Indices[0] = DumpInt( File );
        Polygon.Indices[1] = DumpInt( File );
        Polygon.Indices[2] = DumpInt( File );

        Vertices[ Polygon.Indices[0] ].Polygons.Append( i );
        Vertices[ Polygon.Indices[1] ].Polygons.Append( i );
        Vertices[ Polygon.Indices[2] ].Polygons.Append( i );

        Polygon.TextureName = DumpString( File );

        TextureMap[ Polygon.TextureName.Str() ].Append( i );
    
        Polygon.TexCoords[0].X = DumpFloat( File );
        Polygon.TexCoords[1].X = DumpFloat( File );
        Polygon.TexCoords[2].X = DumpFloat( File );
        Polygon.TexCoords[0].Y = 1.0f - DumpFloat( File );
        Polygon.TexCoords[1].Y = 1.0f - DumpFloat( File );
        Polygon.TexCoords[2].Y = 1.0f - DumpFloat( File );

        SetDumpLog( true );
        Polygon.Unknown = DumpInt( File );
        assert( Polygon.Unknown == 0 ); // FIXME
        SetDumpLog( false );

        Polygon.Use = 0;
    }

    int PartsCount = DumpInt( File );

    Parts.Resize( PartsCount );

    uint32_t StrLen = DumpIntNotSeek( File );
    if ( StrLen != 0xffffffff ) {

        SetDumpLog( true );

        Out() << "Parts";

        int StartIndexLocation = 0;

        for ( int n = 0 ; n < PartsCount  ; n++ ) {
            FPart & Part = Parts[ n ];

            Part.Name = DumpString( File );

            Out() << "Part" << Part.Name;

            Part.UnknownIndex = DumpInt( File );

            SetDumpLog( false );
            for ( int i = 0 ; i < 16 ; i++ ) {
                Part.Matrix.ToPtr()[i] = DumpDouble( File );
            }
            SetDumpLog( true );

            DumpInt( File );
            DumpInt( File );

            int Count = DumpInt( File );

            for ( int i = 0 ; i < Count ; i++ ) {
                DumpDouble( File );
                DumpDouble( File );
                DumpDouble( File );
                DumpDouble( File );
                int FirstVertex = DumpInt( File ); // First vertex
                int NumVertices = DumpInt( File ); // Num vertices

                for ( int p = 0 ; p < Polygons.Length() ; p++ ) {
                    FPolygon & Polygon = Polygons[p];

                    Polygon.Use = 0;
                }

                for ( int v = 0 ; v < NumVertices ; v++ ) {
                    FVertex & Vertex = Vertices[ FirstVertex + v ];
                    for ( int p = 0 ; p < Vertex.Polygons.Length() ; p++ ) {
                        Polygons[ Vertex.Polygons[p] ].Use++;
                    }
                }

                FMeshOffset Offset;

                for ( int p = 0 ; p < Polygons.Length() ; p++ ) {
                    FPolygon & Polygon = Polygons[p];

                    if ( Polygon.Use < 3 ) {
                        continue;
                    }
                    assert( Polygon.Use == 3 );
                    
                    for ( int j = 0 ; j < 3 ; j++ ) {                        

                        FVertex & v = Vertices[ Polygon.Indices[ j ] ];
                        FMeshVertex & Vertex = MeshVertices.Append();

                        Vertex.Clear();
                        Vertex.Position.X = v.Position.X;
                        Vertex.Position.Y = v.Position.Y;
                        Vertex.Position.Z = v.Position.Z;
                        Vertex.Normal.X = v.Normal.X;
                        Vertex.Normal.Y = v.Normal.Y;
                        Vertex.Normal.Z = v.Normal.Z;
                        Vertex.TexCoord = Polygon.TexCoords[ j ];
//Vertex.TexCoord.Y=1.0f-Vertex.TexCoord.Y;

                        MeshIndices.Append( MeshVertices.Length() - 1 );
                    }

                    Offset.IndexCount += 3;
                    Offset.Abstract = Polygon.TextureName;
                }

                //assert( Offset.IndexCount > 0 );

                if ( Offset.IndexCount > 0 ) {

                    Offset.StartIndexLocation = StartIndexLocation;
                    //Offset.Abstract = Polygons[FirstPolygon].TextureName;

                    MeshOffsets.Append( Offset );

                    StartIndexLocation += Offset.IndexCount;
                }

            }
        }

        Out() << "Unknown doubles";
        UnknownDbl0 = DumpDouble( File );
        UnknownDbl1 = DumpDouble( File );
        UnknownDbl2 = DumpDouble( File );
        UnknownDbl3 = DumpDouble( File );

        Out() << "Unknown ints";

        int UnknownI0 = DumpInt( File );

        assert( UnknownI0 == 0 ); // FIXME

        int UnknownI1 = DumpInt( File );

        assert( UnknownI1 == 0 ); // FIXME

        SetDumpLog( false );

        int SocketsCount = DumpInt( File );

        Sockets.Resize( SocketsCount );

        Out() << "Sockets";

        for ( int n = 0 ; n < SocketsCount ; n++ ) {
            FSocket & Socket = Sockets[ n ];

            Socket.Name = DumpString( File );

            for ( int i = 0 ; i < 16 ; i++ ) {
                Socket.Matrix.ToPtr()[i] = DumpDouble( File );
            }

            SetDumpLog( true );
            Socket.UnknownIndex = DumpInt( File );
            SetDumpLog( false );
        }

        SetDumpLog( true );

        Out() << "Unknown ints";
        // 4
        int UnknownI2 = DumpInt( File );  // FIXME
        // 0, 8
        int Count = DumpInt( File );
        for ( int n = 0 ; n < Count ; n++ ) {
            DumpInt( File );
            DumpInt( File );
            for ( int i = 0 ; i < 9 ; i++ ) {
                DumpDouble( File );
            }
        }

        // 0, 2
        Count = DumpInt( File );
        for ( int n = 0 ; n < Count ; n++ ) {
            DumpInt( File );
            DumpInt( File );
            for ( int i = 0 ; i < 6 ; i++ ) {
                DumpDouble( File );
            }
        }

        Out() << "----------";
        Count = DumpInt( File );
        assert( Count == PolygonsCount );
        for ( int n = 0 ; n < Count ; n++ ) {
            DumpByte( File );
        }

        Out() <<"------------";

        Count = DumpInt( File );
        assert( Count == PolygonsCount );
        for ( int n = 0 ; n < Count ; n++ ) {
            DumpInt( File );
        }

        int UnknownI5 = DumpInt( File );
        assert( UnknownI5 == 0 );

        Out() <<"------------";

    } else {

        assert( PartsCount == 1 );

        FPart & Part = Parts[ 0 ];

        File->Seek( 4, FFileAbstract::SeekCur );

        SetDumpLog( true );
        DumpFileOffset( File );
        SetDumpLog( false );

        MeshOffsets.Resize( TextureMap.size() );

        int OffsetIndex = 0;
        int StartIndexLocation = 0;
        for ( auto It : TextureMap ) {

            const TPodArray< int > & PolygonIndices = It.second;

            for ( int i = 0 ; i < PolygonIndices.Length() ; i++ ) {
                for ( int j = 0 ; j < 3 ; j++ ) {
                    FPolygon & Polygon = Polygons[ PolygonIndices[ i ] ];

                    FVertex & v = Vertices[ Polygon.Indices[ j ] ];
                    FMeshVertex & Vertex = MeshVertices.Append();

                    Vertex.Clear();
                    Vertex.Position.X = v.Position.X;
                    Vertex.Position.Y = v.Position.Y;
                    Vertex.Position.Z = v.Position.Z;
                    Vertex.Normal.X = v.Normal.X;
                    Vertex.Normal.Y = v.Normal.Y;
                    Vertex.Normal.Z = v.Normal.Z;
                    Vertex.TexCoord = Polygon.TexCoords[ j ];

                    MeshIndices.Append( MeshVertices.Length() - 1 );
                }
            }

            FMeshOffset & Offset = MeshOffsets[ OffsetIndex ];
            Offset.IndexCount = PolygonIndices.Length() * 3;
            Offset.StartIndexLocation = StartIndexLocation;
            Offset.Abstract = It.first.c_str();

            ++OffsetIndex;

            StartIndexLocation += Offset.IndexCount;
        }

        Part.UnknownIndex = 0;

        // Matrix 4x4
        for ( int i = 0 ; i < 16 ; i++ ) {
            Part.Matrix.ToPtr()[ i ] = DumpDouble( File );
        }
        //Out() << "Matrix:" << Part.Matrix;

        SetDumpLog( true );

        DumpInt( File );
        DumpInt( File );

        int Count = DumpInt( File );

        for ( int i = 0 ; i < Count ; i++ ) {
            DumpDouble( File );
            DumpDouble( File );
            DumpDouble( File );
            DumpDouble( File );

            DumpInt( File );
            DumpInt( File );
        }

        Out() << "Unknown doubles";
        UnknownDbl0 = DumpDouble( File );
        UnknownDbl1 = DumpDouble( File );
        UnknownDbl2 = DumpDouble( File );
        UnknownDbl3 = DumpDouble( File );

        Out() << "Unknown ints";

        // 0
        int UnknownI0 = DumpInt( File ); // FIXME

        Count = DumpInt( File );

        for ( int i = 0 ; i < Count ; i++ ) {
            Out() << "----";
            DumpDouble( File );
            DumpDouble( File );
            DumpDouble( File );

            DumpInt( File );
        }

        Out() << "Unknown ints";

        if ( DumpIntNotSeek( File ) == 0xffffffff ) {
            // ffffffff
            DumpInt( File );
            // 0
            DumpInt( File );
            // 1
            DumpInt( File );
            DumpFloat( File );
            DumpFloat( File );
            DumpDouble( File );
            DumpDouble( File );
            DumpDouble( File );
            // ffffffff
            DumpInt( File );
        }

        Count = DumpInt( File );
        for ( int n = 0 ; n < Count ; n++ ) {
            DumpString( File );
            for ( int i = 0 ; i < 16 ; i++ ) {
                DumpDouble( File );
            }
            DumpInt( File );
        }

        // 4
        int UnknownI3 = DumpInt( File );
        if ( UnknownI3 == 4 ) { // FIXME
            // 0
            Count = DumpInt( File );
            for ( int n = 0 ; n < Count ; n++ ) {
                for ( int i = 0 ; i < 10 ; i++ ) {
                    DumpDouble( File );
                }
            }

            // 0
            Count = DumpInt( File );
            for ( int n = 0 ; n < Count * 7 ; n++ ) {
                DumpDouble( File );
            }

            Out() << "----------";

            Count = DumpInt( File );
            for ( int n = 0 ; n < Count ; n++ ) {
                DumpByte( File );
            }

            Count = DumpInt( File );
            assert( Count == 0 );

            Count = DumpInt( File );
            for ( int n = 0 ; n < Count * 7 ; n++ ) {
                DumpDouble( File );
            }

        } else { // UnknownI2 == 2

            // trono.BOD
            // firybally.BOD
            // leon.BOD
            // reyaure.BOD

            int Unk = DumpInt( File );
            assert( Unk == 0 );

            Unk = DumpInt( File );
            assert( Unk == 0 );

            Count = DumpInt( File );
            for ( int i = 0 ; i < Count ; i++ ) { // FIXME
                DumpByte( File );
            }
        }
    }

    // Check eof
    byte b;
    assert( 0 == File->ReadByte( b ) );

    FFiles::CloseFile( File );

    Resource = GResourceManager->GetResource< FStaticMeshResource >( Name.Str() );
    CalcTangentSpace( MeshVertices.ToPtr(), MeshVertices.Length(), MeshIndices.ToPtr(), MeshIndices.Length() );
    Resource->SetVertexData( MeshVertices.ToPtr(), MeshVertices.Length(), MeshIndices.ToPtr(), MeshIndices.Length(), false );
    Resource->SetMeshOffsets( MeshOffsets.ToPtr(), MeshOffsets.Length() );
}
