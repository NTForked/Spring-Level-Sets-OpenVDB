/*
 * Mesh.cc
 *
 *  Created on: Aug 17, 2014
 *      Author: blake
 */

#include "Mesh.h"
#include <openvdb/openvdb.h>
#include <openvdb/tools/VolumeToMesh.h>
#include <openvdb/tree/LeafManager.h>
#include <openvdb/math/Operators.h>
#include "ply_io.h"
namespace imagesci {
template<typename T> T clamp(T a,T min,T max){
	return std::max(std::min(a,max),min);
}
using namespace openvdb;
typedef struct _plyVertex {
	float x[3];             // the usual 3-space position of a vertex
	unsigned char red;
	unsigned char green;
	unsigned char blue;
	unsigned char alpha;
	_plyVertex(){
		x[0] = 0;
		x[1] = 0;
		x[2] = 0;
		red = 0;
		green = 0;
		blue = 0;
		alpha = 0;
	}
} plyVertex;

typedef struct _plyFace {
	unsigned char nverts;    // number of vertex indices in list
	int *verts;              // vertex index list
	unsigned char red;
	unsigned char green;
	unsigned char blue;
	_plyFace(){
		nverts = 0;
		verts = NULL;
		red = 0;
		green = 0;
		blue = 0;
	}
} plyFace;

char* elemNames[] = { "vertex", "face","normal" };

PlyProperty vertProps[] = { // property information for a vertex
	{ "x", Float32, Float32, static_cast<int>(offsetof(plyVertex, x)),0, 0, 0, 0 },
	{ "y", Float32, Float32, static_cast<int>(offsetof(plyVertex, x) + sizeof(float)),0, 0, 0, 0 },
	{ "z", Float32, Float32, static_cast<int>(offsetof(plyVertex, x) + sizeof(float)+sizeof(float)),0, 0, 0, 0 },
	{ "red", Uint8, Uint8, static_cast<int>(offsetof(plyVertex, red)),0, 0, 0, 0 },
	{ "green", Uint8, Uint8,static_cast<int>(offsetof(plyVertex, green)), 0, 0, 0, 0 },
	{ "blue", Uint8, Uint8, static_cast<int>(offsetof(plyVertex, blue)),0, 0, 0, 0 },
	{ "alpha", Uint8, Uint8, static_cast<int>(offsetof(plyVertex,alpha)),0, 0, 0, 0 },
};

PlyProperty faceProps[] = { // property information for a face
	{ "vertex_indices", Int32, Int32,static_cast<int>(offsetof(plyFace, verts)),1, Uint8, Uint8, static_cast<int>(offsetof(plyFace, nverts)) },
	{ "red", Uint8, Uint8, static_cast<int>(offsetof(plyFace, red)),0, 0, 0, 0 },
	{ "green", Uint8, Uint8, static_cast<int>(offsetof(plyFace, green)),0, 0, 0, 0 },
	{ "blue", Uint8, Uint8, static_cast<int>(offsetof(plyFace, blue)),0, 0, 0, 0 },
};


Mesh::Mesh() :
		mVertexBuffer(0), mNormalBuffer(0), mColorBuffer(0), mIndexBuffer(0),primType(PrimitiveType::TRIANGLES) {
}
bool Mesh::Save(const std::string& f){
	int i, j, idx;
	const char* fileName=f.c_str();
	unsigned char  *pointColors=NULL;
	PlyFile *ply;

	// Get input and check data

	ply = open_for_writing_ply(fileName, 2, elemNames,PLY_BINARY_LE);

	if(ply==NULL)return false;


	// compute colors, if any
	int numPts=points.size();

	int npts=(primType==PrimitiveType::TRIANGLES)?3:4;
	int numPolys=indexes.size()/npts;
	unsigned char* c;
	if(colors.size()>0){
		pointColors = c = new unsigned char[3*numPts];
		for (i=0; i<numPts; i++){
			Vec3s d=colors[i];
			unsigned char r=(unsigned char)clamp(d[0]*255.0f,0.0f,255.0f);
			unsigned char g=(unsigned char)clamp(d[1]*255.0f,0.0f,255.0f);
			unsigned char b=(unsigned char)clamp(d[2]*255.0f,0.0f,255.0f);
			*c++ = r;
			*c++ = g;
			*c++ = b;
		}
	}
	// describe what properties go into the vertex and face elements
	element_count_ply(ply, "vertex", numPts);
	ply_describe_property (ply, "vertex", &vertProps[0]);
	ply_describe_property (ply, "vertex", &vertProps[1]);
	ply_describe_property (ply, "vertex", &vertProps[2]);
	if(colors.size()>0){
		ply_describe_property (ply, "vertex", &vertProps[3]);
		ply_describe_property (ply, "vertex", &vertProps[4]);
		ply_describe_property (ply, "vertex", &vertProps[5]);
	}
	element_count_ply(ply, "face", numPolys);
	ply_describe_property (ply, "face", &faceProps[0]);

	// write a comment and an object information field
	append_comment_ply(ply, "PLY File");
	append_obj_info_ply(ply, "ImageSci");

	// complete the header
	header_complete_ply(ply);

	// set up and write the vertex elements
	plyVertex vert;
	put_element_setup_ply(ply, "vertex");


	for (i = 0; i < numPts; i++)
	{
		Vec3s pt=points[i];
		vert.x[0] = pt[0];
		vert.x[1] = pt[1];
		vert.x[2] = pt[2];
		if ( pointColors )
		{
			idx = 3*i;
			vert.red = *(pointColors + idx);
			vert.green = *(pointColors + idx + 1);
			vert.blue = *(pointColors + idx + 2);
		}
		put_element_ply (ply, (void *) &vert);
	}
	// set up and write the face elements
	plyFace face;
	int verts[256];
	face.verts = verts;
	put_element_setup_ply(ply, "face");


	if(indexes.size()<numPolys){
		for (int i=0;i<numPolys;i++){
			for (j=0; j<npts; j++)
			{
				face.nverts = npts;
				verts[j] = 3*i+j;
			}
			put_element_ply (ply, (void *) &face);
		}
	} else {
		for (int i=0;i<numPolys;i++){
			for (j=0; j<npts; j++)
			{
				face.nverts = npts;
				verts[j] = indexes[3*i+j];
			}
			put_element_ply (ply, (void *) &face);
		}
	}
	if(pointColors!=NULL)delete[] pointColors;
	// close the PLY file
	close_ply(ply);
	return true;
}
Mesh* Mesh::Open(const std::string& file){
			int i, j, k;
			int numPts = 0, numPolys = 0;

			// open a PLY file for reading
			PlyFile *ply;
			int nelems = 3, fileType = PLY_BINARY_LE, numElems, nprops;
			char** elist=NULL;
			char* elemName;
			float version;
			if (!(ply = ply_open_for_reading(file.c_str(), &nelems, &elist, &fileType,&version)))
			{
				std::cerr << "Could not open ply file." << std::endl;
				return NULL;
			}

			// Check to make sure that we can read geometry
			PlyElement *elem;
			int index;
			if ((elem =find_element(ply, "vertex")) == NULL ||
				find_property(elem, "x", &index) == NULL ||
				find_property(elem, "y", &index) == NULL ||
				find_property(elem, "z", &index) == NULL ||
				(elem = find_element(ply, "face")) == NULL ||
				find_property(elem, "vertex_indices", &index) == NULL)
			{
				std::cerr<< "Cannot read geometry"<<std::endl;
				close_ply(ply);
				return NULL;
			}
			Mesh* mesh=new Mesh();

			// Check for optional attribute data. We can handle intensity; and the
			// triplet red, green, blue.
			unsigned char intensityAvailable = false, RGBCellsAvailable = false, RGBPointsAvailable = false;
			std::vector<Vec3s>& colors=mesh->colors;
			std::vector<Vec3s>& points=mesh->points;
			std::vector<Vec3s>& normals=mesh->normals;
			std::vector<Index32>& indexes=mesh->indexes;

			if ((elem = find_element(ply, "face")) != NULL &&
				find_property(elem, "intensity", &index) != NULL)
			{
				intensityAvailable = true;
			}

			if ((elem = find_element(ply, "face")) != NULL &&
				find_property(elem, "red", &index) != NULL &&
				find_property(elem, "green", &index) != NULL &&
				find_property(elem, "blue", &index) != NULL)
			{
				RGBCellsAvailable = true;
			}

			if ((elem = find_element(ply, "vertex")) != NULL &&
				find_property(elem, "red", &index) != NULL &&
				find_property(elem, "green", &index) != NULL &&
				find_property(elem, "blue", &index) != NULL)
			{
				RGBPointsAvailable = true;
			}

			int verts[256];
			plyFace face;
			plyVertex vertex;
			memset(verts, 0, sizeof(verts));
			// Okay, now we can grab the data
			for (i = 0; i < nelems; i++)
			{
				//get the description of the first element */
				elemName = elist[i];
				get_element_description_ply(ply, elemName, &numElems, &nprops);
				// if we're on vertex elements, read them in
				if (elemName && !strcmp("vertex", elemName))
				{
					// Create a list of points
					numPts = numElems;
					points.resize(numPts);
					// Setup to read the PLY elements
					ply_get_property(ply, elemName, &vertProps[0]);
					ply_get_property(ply, elemName, &vertProps[1]);
					ply_get_property(ply, elemName, &vertProps[2]);

					if (RGBPointsAvailable)
					{
						colors.resize(numPts);
						ply_get_property(ply, elemName, &vertProps[3]);
						ply_get_property(ply, elemName, &vertProps[4]);
						ply_get_property(ply, elemName, &vertProps[5]);
					}
					for (j = 0; j < numPts; j++)
					{
						get_element_ply(ply, &vertex);
						points[j] = Vec3s(vertex.x[0], vertex.x[1], vertex.x[2]);
						if (RGBPointsAvailable)
						{
							colors[j] = Vec3s(vertex.red / 255.0f, vertex.green / 255.0f, vertex.blue / 255.0f);
						}
					}
				}//if vertex
				else if (elemName && !strcmp("face", elemName))
				{
					// Create a polygonal array
					numPolys = numElems;

					// Get the face properties
					ply_get_property(ply, elemName, &faceProps[0]);
					if (intensityAvailable)
					{
						ply_get_property(ply, elemName, &faceProps[1]);
					}
					if (RGBCellsAvailable)
					{
						ply_get_property(ply, elemName, &faceProps[2]);
						ply_get_property(ply, elemName, &faceProps[3]);
						ply_get_property(ply, elemName, &faceProps[4]);
					}
					indexes.clear();
					indexes.reserve(numPolys *3);
					for (j = 0; j < numPolys; j++)
					{

						//grab and element from the file
						face.verts = &verts[0];
						get_element_ply(ply, &face);
						for (k = 0; k<face.nverts; k++)
						{
							indexes.push_back(face.verts[k]);
						}
						if(face.nverts==3){
							primType=PrimitiveType::TRIANGLES;
						} else {
							primType=PrimitiveType::QUADS;
						}
						if (intensityAvailable)
						{
							//Not used!
							//face.intensity;
						}
						if (RGBCellsAvailable)
						{
							//Not used!
							//face.red;
							//face.green;
							//face.blue;
						}
					}
				}//if face

				free(elist[i]); //allocated by ply_open_for_reading

			}//for all elements of the PLY file
			free(elist); //allocated by ply_open_for_reading
			close_ply(ply);
			if (points.size() > 0 && indexes.size() > 0){
				return mesh;
			} else {
				return NULL;
			}
}
template<typename GridType> void Mesh::Create(typename GridType::ConstPtr grid,PrimitiveType primType) {
	using openvdb::Index64;
	openvdb::tools::VolumeToMesh mesher(
			grid->getGridClass() == openvdb::GRID_LEVEL_SET ? 0.0 : 0.01);
	mesher(*grid);
	// Copy points and generate point normals.
	points.resize(mesher.pointListSize() * 3);
	normals.resize(mesher.pointListSize() * 3);
	openvdb::tree::ValueAccessor<const typename GridType::TreeType> acc(
			grid->tree());

	openvdb::math::GenericMap map(grid->transform());
	openvdb::Coord ijk;
	this->points = mesher.pointList();
	// Copy primitives
	openvdb::tools::PolygonPoolList& polygonPoolList = mesher.polygonPoolList();
	Index64 numQuads = 0;
	for (Index64 n = 0, N = mesher.polygonPoolListSize(); n < N; ++n) {
		numQuads += polygonPoolList[n].numQuads();
	}
	indexes.reserve(numQuads * 4);
	openvdb::Vec3d normal, e1, e2;
	for (Index64 n = 0, N = mesher.polygonPoolListSize(); n < N; ++n) {
		const openvdb::tools::PolygonPool& polygons = polygonPoolList[n];
		std::cout << "Polygon " << polygons.numTriangles() << " "
				<< polygons.numQuads() << std::endl;
		for (Index64 i = 0, I = polygons.numQuads(); i < I; ++i) {
			const openvdb::Vec4I& quad = polygons.quad(i);
			indexes.push_back(quad[0]);
			indexes.push_back(quad[1]);
			indexes.push_back(quad[2]);
			indexes.push_back(quad[3]);
			e1 = mesher.pointList()[quad[1]];
			e1 -= mesher.pointList()[quad[0]];
			e2 = mesher.pointList()[quad[2]];
			e2 -= mesher.pointList()[quad[1]];
			normal = e1.cross(e2);
			const double length = normal.length();
			if (length > 1.0e-7)
				normal *= (1.0 / length);
			for (Index64 v = 0; v < 4; ++v) {
				normals[quad[v]] = -normal;
			}
		}
	}

	if (points.size() > 0) {
		if (glIsBuffer(mVertexBuffer) == GL_TRUE)
			glDeleteBuffers(1, &mVertexBuffer);
		glGenBuffers(1, &mVertexBuffer);
		glBindBuffer(GL_ARRAY_BUFFER, mVertexBuffer);
		if (glIsBuffer(mVertexBuffer) == GL_FALSE)
			throw "Error: Unable to create vertex buffer";
		glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 3 * points.size(),
				&points[0], GL_STATIC_DRAW);
		if (GL_NO_ERROR != glGetError())
			throw "Error: Unable to upload vertex buffer data";
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	if (colors.size() > 0) {
		if (glIsBuffer(mColorBuffer) == GL_TRUE)
			glDeleteBuffers(1, &mColorBuffer);

		glGenBuffers(1, &mColorBuffer);
		glBindBuffer(GL_ARRAY_BUFFER, mColorBuffer);
		if (glIsBuffer(mColorBuffer) == GL_FALSE)
			throw "Error: Unable to create color buffer";

		glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 3 * colors.size(),
				&colors[0], GL_STATIC_DRAW);
		if (GL_NO_ERROR != glGetError())
			throw "Error: Unable to upload color buffer data";

		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	if (indexes.size() > 0) {
		// clear old buffer
		if (glIsBuffer(mIndexBuffer) == GL_TRUE)
			glDeleteBuffers(1, &mIndexBuffer);

		// gen new buffer
		glGenBuffers(1, &mIndexBuffer);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mIndexBuffer);
		if (glIsBuffer(mIndexBuffer) == GL_FALSE)
			throw "Error: Unable to create index buffer";

		// upload data
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLuint) * indexes.size(),
				&indexes[0], GL_STATIC_DRAW); // upload data
		if (GL_NO_ERROR != glGetError())
			throw "Error: Unable to upload index buffer data";

		// release buffer
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	}

	if (normals.size() > 0) {
		if (glIsBuffer(mNormalBuffer) == GL_TRUE)
			glDeleteBuffers(1, &mNormalBuffer);

		glGenBuffers(1, &mNormalBuffer);
		glBindBuffer(GL_ARRAY_BUFFER, mNormalBuffer);
		if (glIsBuffer(mNormalBuffer) == GL_FALSE)
			throw "Error: Unable to create normal buffer";

		glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 3 * normals.size(),
				&normals[0], GL_STATIC_DRAW);
		if (GL_NO_ERROR != glGetError())
			throw "Error: Unable to upload normal buffer data";

		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}
}

Mesh::~Mesh() {
	// TODO Auto-generated destructor stub
}

} /* namespace imagesci */
