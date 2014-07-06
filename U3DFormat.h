/*======================================================================

    	Unreal Vertex Animation data structure details
	Copyright 1997-2003 Epic Games, Inc. All Rights Reserved.

========================================================================*/
#pragma pack(push,1)

/*

 Vertex animation

The classic vertex animation raw data format hasn't changed since Unreal 1 
- though in the engine, rendering has been sped up since, and now works 
with continuous level-of-detail, unlike skeletal meshes, which - 
since UT2003 - uses static LOD levels.

Overal CPU overhead for vertex animated meshes is still much larger than 
for skeletal meshes, so vertex animation should never be your first choice 
if you can achieve the same animation with a skeletal setup and a reasonable 
number of bones. Also, the vertex mesh animation code does not support any 
blending features.

Tools used to generate vertex animated meshes include the Max and Maya ActorX 
exporter plugins and the older 3DS2UNR tool.

Like the skeletal animation raw data, we have both a 'reference' mesh and the 
animation data, though unlike skeletal, these two have to be imported together 
and are inextricably linked; Vertex animation cannot be shared between multiple 
meshes.

By convention, the mesh's face UV mapping and vertex index data resides in the 
"Data" file with the extension *_d.3d, while the animation "Aniv" data file has 
the extension *_a.3d.

The relevant C++ structures mentioned are defined in detail in 
https://udn.epicgames.com/pub/Two/BinaryFormatSpecifications/UnrealVertexAnimation.h

The files contain the following data:

Data file format

Order of appearance 	Description
File Header 	type FJSDataHeader
NumPolygons : number of faces, as present in the array below.
NumVertices : number of vertices.
Data 	Array of [] elements of type FJSMeshTri

One important clarification: since UT2003, the Flags byte in each FJSMeshTri 
is no longer relevant since properties like translucency and double-sidedness are now 
derived from the assigned materials, rather than the intrinsic mesh-face properties 
they used to be in Unreal 1.

Animation file format

Order of appearance 	Description
File Header 	type FJSAnivHeader
NumFrames : number of animation frames, as present in the array below.
FrameSize : size in BYTES of each frame; i.e. number of vertices * 4.
Data 	Array of ( NumFrames * NumVertices ) 32-bit packed XYZ vectors.

When importing - which can only be done by recompiling the .u scripts, not through the 
editor as with skeletal animation - it is up to script code to designate sequence names 
to subsections of the frame sequence. 

*/



//
// Note: WORD is an unsigned 16 bit word.
//       DWORD is an unsigned 32 bit word.
//         INT is a signed 32 bit integer.
//

typedef unsigned short U_WORD;
typedef unsigned int U_DWORD;
typedef int U_INT;
typedef unsigned char U_BYTE;
typedef float U_FLOAT;

//#define RoundUV(x) ((x)>=0?(long)((x)+0.1):(long)((x)-0.1))
static inline U_BYTE ConvertUV( float f )
{
    return f*256.0f;
}

class FVector
{
public:
	U_FLOAT X,Y,Z;

	FVector( const U_FLOAT x, const U_FLOAT y, const U_FLOAT z )
	: X(x), Y(y), Z(z)
	{}

	FVector( const Point3& p )
	: X(p.x), Y(p.y), Z(p.z)
	{}

	FVector( const U_INT x, const U_INT y, const U_INT z )
	: X((U_FLOAT)x), Y((U_FLOAT)y), Z((U_FLOAT)z)
	{}
};



struct FJSDataHeader
{
	U_WORD	NumPolys;
	U_WORD	NumVertices;
	U_WORD	BogusRot;  		 //(unused)
	U_WORD	BogusFrame;		 //(unused)
	U_DWORD	BogusNormX,BogusNormY,BogusNormZ; //(unused)
	U_DWORD	FixScale; 		 //(unused)
	U_DWORD	Unused1,Unused2,Unused3; //(unused)
	U_DWORD Unknown1,Unknown2,Unknown3; // ??

    FJSDataHeader()
    : NumPolys(0)
    , NumVertices(0)
    , BogusRot(0)
    , BogusFrame(0)
    , BogusNormX(0)
    , BogusNormY(0)
    , BogusNormZ(0)
    , FixScale(0)
    , Unused1(0)
    , Unused2(0)
    , Unused3(0)
    , Unknown1(0)
    , Unknown2(0)
    , Unknown3(0)
    {
    }
};

//
// Animation info.
//
struct FJSAnivHeader
{
	U_WORD	NumFrames;		// Number of animation frames.
	U_WORD	FrameSize;		// Size of one frame of animation.
    
    FJSAnivHeader()
    : NumFrames(0)
    , FrameSize(0)
    {
    }
};

// Texture coordinates associated with a vertex and one or more mesh triangles.
// All triangles sharing a vertex do not necessarily have the same texture
// coordinates at the vertex.
struct FMeshUV
{
	U_BYTE U;
	U_BYTE V;

	// Constructor.
	FMeshUV(): U(0), V(0)
	{}
	FMeshUV( const U_BYTE u, const U_BYTE v )
	: U(u), V(v)
	{}

	FMeshUV( const Point2& p )
	: U(ConvertUV(p.x)), V(255-ConvertUV(p.y))
	{}

    static void printf( FILE* f, const Point2& p, const FMeshUV& u )
    {
        _ftprintf( f, _T("(U=[%f][%d][%d],V=[%f][%d][%d]) ")
            , p.x, ConvertUV(p.x), u.U
            , p.y, 255-ConvertUV(p.y), u.V );
    }

};

//
// Mesh triangle.
//
struct FJSMeshTri
{
	U_WORD		iVertex[3];		// Vertex indices.
	U_BYTE		Type;			// James' mesh type. (unused)
	U_BYTE		Color;			// Color for flat and Gouraud shaded. (unused)
	FMeshUV		Tex[3];			// Texture UV coordinates.
	U_BYTE		TextureNum;		// Source texture offset.
	U_BYTE		Flags;			// Unreal mesh flags (currently unused).
    
    FJSMeshTri()
    : Type(0)
    , Color(0)
    , TextureNum(0)
    , Flags(0)
    {
        iVertex[0] = 0;
        iVertex[1] = 0;
        iVertex[2] = 0;
    }
};

////
//// One triangular polygon in a mesh, which references three vertices, and various drawing/texturing information.
////
//struct FMeshTri
//{
//	U_WORD		iVertex[3];	// Vertex indices.
//	FMeshUV		Tex[3];		// Texture UV coordinates.
//	U_DWORD		PolyFlags;	// Surface flags.
//	U_INT		MaterialIndex;	// Source texture index.
//};



//
// Animated vertices: 32 bits per vertex: packed 11,11,10 bit 3d vector.
//
//struct FMeshVert
//{
//	// Variables.
//	U_INT X:11; U_INT Y:11; U_INT Z:10;
//
//	// Constructor.
//	FMeshVert() : X(0), Y(0), Z(0)
//	{}
//	FMeshVert( const FVector& In )
//	: X((U_INT)In.X), Y((U_INT)In.Y), Z((U_INT)In.Z)
//	{}
//
//	// Functions.
//	FVector Vector() const
//	{
//		return FVector( X, Y, Z );
//	}
//};

struct FMeshVert
{
	// Variables.
	U_INT V;

	// Constructor.
	FMeshVert() : V(0)
	{}

	FMeshVert( const Point3& p )
    : V ( ( static_cast<U_INT>( p.x ) & 0x7FF ) |
        ( ( static_cast<U_INT>( p.y ) & 0x7FF ) << 11 ) |
        ( ( static_cast<U_INT>( p.z ) & 0x3FF ) << 22 ) )
	{
    }
};


#pragma pack(pop)