/*
 * Copyright(C) 2014, Blake C. Lucas, Ph.D. (img.science@gmail.com)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * This is a re-implementation of the following work:
 *
 * Lucas, Blake C., Michael Kazhdan, and Russell H. Taylor. "Spring level sets: a deformable model
 * representation to provide interoperability between meshes and level sets."
 * Visualization and Computer Graphics, IEEE Transactions on 19.5 (2013): 852-865.
 */
#include "SpringLevelSet.h"
#include "json/JsonUtil.h"
#include <openvdb/Grid.h>
#include <openvdb/util/Util.h>
#include <openvdb/math/Stencils.h>
#include <openvdb/tools/MeshToVolume.h>
#include <openvdb/tools/VolumeToMesh.h>
#include <openvdb/tools/LevelSetAdvect.h>
#include <openvdb/tools/DenseSparseTools.h>
#include <openvdb/openvdb.h>
namespace imagesci {
using namespace openvdb;
using namespace openvdb::math;
using namespace openvdb::tools;

typedef DiscreteField<openvdb::VectorGrid> VelocityField;
typedef LevelSetAdvection<openvdb::FloatGrid, VelocityField> AdvectionTool;
const float SpringLevelSet::NEAREST_NEIGHBOR_RANGE = 1.5f;
const float SpringLevelSet::PARTICLE_RADIUS = 0.05f;
const float SpringLevelSet::MAX_VEXT = 0.5f;
const int SpringLevelSet::MAX_NEAREST_NEIGHBORS = 2;
const float SpringLevelSet::FILL_DISTANCE = 0.3f;
const float SpringLevelSet::CLEAN_DISTANCE = 0.625f;
const float SpringLevelSet::SHARPNESS = 5.0f;
const float SpringLevelSet::SPRING_CONSTANT = 0.3f;
const float SpringLevelSet::RELAX_TIMESTEP = 0.1f;
const float SpringLevelSet::MIN_AREA = 0.05f;
const float SpringLevelSet::MAX_AREA = 2.0 ;
const float SpringLevelSet::MIN_ASPECT_RATIO = 0.1f;
MotionScheme DecodeMotionScheme(const std::string& name) {
	if (name == "implicit" || name == "IMPLICIT") {
		return MotionScheme::IMPLICIT;
	} else if (name == "semi-implicit" || name == "SEMI_IMPLICIT"
			|| name == "semi_implicit") {
		return MotionScheme::SEMI_IMPLICIT;
	} else if (name == "explicit" || name == "EXPLICIT") {
		return MotionScheme::EXPLICIT;
	} else {
		return MotionScheme::UNDEFINED;
	}
}
std::string EncodeMotionScheme(MotionScheme scheme) {
	switch (scheme) {
	case IMPLICIT:
		return "implicit";
	case SEMI_IMPLICIT:
		return "semi-implicit";
	case EXPLICIT:
		return "explicit";
	case UNDEFINED:
	default:
		return "undefined";
	}
}
void SpringLevelSetDescription::serialize(Json::Value& root_in) {
	Json::Value &root = root_in["SpringLevelSet"];
	root["ConstellationFile"] = mConstellationFile;
	root["IsoSurfaceFile"] = mIsoSurfaceFile;
	root["SignedLevelSetFile"] = mSignedLevelSetFile;
	root["ParticleVolumeFile"] = mParticleVolumeFile;
	for (int i = 0; i < mMetricNames.size(); i++) {
		root[mMetricNames[i]] = mMetricValues[mMetricNames[i]];
	}
}
void SpringLevelSetDescription::deserialize(Json::Value& root_in) {
	Json::Value &root = root_in["SpringLevelSet"];
	mConstellationFile = root.get("ConstellationFile", "").asString();
	mIsoSurfaceFile = root.get("IsoSurfaceFile", "").asString();
	mSignedLevelSetFile = root.get("SignedLevelSetFile", "").asString();
	mParticleVolumeFile = root.get("ParticleVolumeFile", "").asString();
	for (int i = 0; i < mMetricNames.size(); i++) {
		mMetricValues[mMetricNames[i]] =
				root.get(mMetricNames[i], 0.0).asDouble();
	}
}

std::vector<std::string> SpringLevelSetDescription::mMetricNames = { "Elements",
		"Added", "Removed" };
SpringLevelSetDescription::SpringLevelSetDescription() {
}
std::ostream& operator<<(std::ostream& ostr, const SpringlNeighbor& classname) {
	ostr << "{" << classname.springlId << "|"
			<< static_cast<int>(classname.edgeId) << ":" << std::setprecision(4)
			<< classname.distance << "}";
	return ostr;
}

int Springl::size() const {
	return ((mesh->mFaces[id][3] == openvdb::util::INVALID_IDX) ? 3 : 4);
}

float Springl::distanceToParticle(const openvdb::Vec3s& pt) {
	return ((particle()) - pt).length();
}

float Springl::distanceToParticleSqr(const openvdb::Vec3s& pt) {
	return ((particle()) - pt).lengthSqr();
}
float Springl::distanceToFace(const openvdb::Vec3s& pt) {
	return std::sqrt(distanceToFaceSqr(pt));
}

float Springl::distanceToFaceSqr(const openvdb::Vec3s& pt) {
	Vec3s closest;
	if (size() == 3) {
		return DistanceToTriangleSqr(pt, (*this)[0], (*this)[1], (*this)[2],
				&closest);
	} else {
		return DistanceToQuadSqr(pt, (*this)[0], (*this)[1], (*this)[2],
				(*this)[3], normal(), &closest);
	}
}
float Springl::signedDistanceToFaceSqr(const openvdb::Vec3s& pt) {
	Vec3s closest;
	if (size() == 3) {
		float d=DistanceToTriangleSqr(pt, (*this)[0], (*this)[1], (*this)[2],
				&closest);
		return d*imagesci::Sign((pt-closest).dot(normal()));

	} else {
		float d=DistanceToQuadSqr(pt, (*this)[0], (*this)[1], (*this)[2],
				(*this)[3], normal(), &closest);
		return d*imagesci::Sign((pt-closest).dot(normal()));
	}
}
openvdb::math::BBox<Vec3s> Springl::getBoundingBox(){
	openvdb::math::BBox<Vec3s> bbox;

	bbox.min()=Vec3s(std::numeric_limits<float>::max());
	bbox.max()=Vec3s(std::numeric_limits<float>::min());

	for(int n=0;n<size();n++){
		Vec3s pt=(*this)[n];
		bbox.min()=openvdb::math::Min(bbox.min(),pt);
		bbox.max()=openvdb::math::Max(bbox.max(),pt);
	}
	return bbox;
}
float Springl::signedDistanceToFace(const openvdb::Vec3s& pt) {
	float d=signedDistanceToFaceSqr(pt);
	return imagesci::Sign(d)*std::sqrt(abs(d));
}
float Springl::distanceToEdgeSqr(const openvdb::Vec3s& pt, int e) {
	return DistanceToEdgeSqr(pt, (*this)[e], (*this)[(e + 1) % size()]);
}
float Springl::distanceToEdge(const openvdb::Vec3s& pt, int e) {
	return std::sqrt(
			DistanceToEdgeSqr(pt, (*this)[e], (*this)[(e + 1) % size()]));
}

openvdb::Vec3s Springl::computeCentroid() const {
	openvdb::Vec3s centroid = openvdb::Vec3s(0.0f, 0.0f, 0.0f);
	int K = size();
	for (int k = 0; k < K; k++) {
		centroid += (*this)[k];
	}
	centroid = (1.0 / K) * centroid;
	return centroid;
}
openvdb::Vec3s Springl::computeNormal(const float eps) const {
	openvdb::Vec3s norm(0.0f);
	int K = size();
	openvdb::Vec3s pt = particle();
	for (int k = 0; k < K; k++) {
		norm += ((*this)[k] - pt).cross((*this)[(k + 1) % K] - pt);
	}

	norm.normalize(eps);
	return norm;
}
float Springl::area() const {
	openvdb::Vec3s norm(0.0f);
	int K = size();
	for (int k = 0; k < K; k++) {
		norm += ((*this)[k] - (particle())).cross(
				(*this)[(k + 1) % K] - (particle()));
	}
	return 0.5f * norm.length();
}
Vec3s Constellation::closestPointOnEdge(const Vec3s& start,
		const SpringlNeighbor& ci) {
	Vec3s pt;
	Springl& nbr = springls[ci.springlId];
	DistanceToEdgeSqr(start, nbr[ci.edgeId], nbr[(ci.edgeId + 1) % nbr.size()],
			&pt);
	return pt;
}
void SpringLevelSet::draw() {
	mIsoSurface.draw();
	mConstellation.draw();
}

openvdb::Vec3s& SpringLevelSet::getParticle(const openvdb::Index32 id) {
	return (mConstellation.springls[id].particle());
}
openvdb::Vec3s& SpringLevelSet::getParticleNormal(const openvdb::Index32 id) {
	return (mConstellation.springls[id].normal());
}
openvdb::Vec3s& SpringLevelSet::getSpringlVertex(const openvdb::Index32 id,
		const int i) {
	return mConstellation.springls[id][i];
}
openvdb::Vec3s& SpringLevelSet::getSpringlVertex(const openvdb::Index32 id) {
	return mConstellation.mVertexes[id];
}
Springl& SpringLevelSet::getSpringl(const openvdb::Index32 id) {
	return mConstellation.springls[id];
}

void RelaxOperation::init(SpringLevelSet& mGrid) {
	mGrid.mConstellation.mVertexAuxBuffer.resize(
			mGrid.mConstellation.getNumVertexes());
}
void RelaxOperation::apply(Springl& springl, SpringLevelSet& mGrid, double dt) {
	int K = springl.size();
	for (int k = 0; k < K; k++) {
		springl[k] = mGrid.mConstellation.mVertexAuxBuffer[springl.offset + k];
	}
}
void RelaxOperation::compute(Springl& springl, SpringLevelSet& mGrid,
		double t) {
	float w, len;
	Vec3s tanget;
	Vec3s dir;
	int K = springl.size();
	std::vector<Vec3s> vertexVelocity(K);
	std::vector<Vec3s> tangets(K);
	std::vector<float> springForce(K);
	std::vector<float> tangetLengths(K);
	Vec3s particlePt = springl.particle();
	Vec3s startVelocity = Vec3s(0);
	Vec3s resultantMoment = Vec3s(0);
	const float MAX_FORCE = 0.999f;
	Vec3s start;
	float dotProd;
	Vec3s pt2;
	for (int k = 0; k < K; k++) {
		std::list<SpringlNeighbor>& map = mGrid.getNearestNeighbors(springl.id,
				k);
		start = springl[k];
		// edge from pivot to magnet
		tanget = (start - particlePt);
		tangetLengths[k] = tanget.length();
		if (tangetLengths[k] > 1E-6f) {
			tanget *= (1.0f / tangetLengths[k]);
		}
		tangets[k] = tanget;
		startVelocity = Vec3s(0);
		// Sum forces
		//unroll loop
		for (SpringlNeighbor ci : map) {
			//Closest point should be recomputed each time and does not need to be stored

			Springl& nbr = mGrid.getSpringl(ci.springlId);
			DistanceToEdgeSqr(start, nbr[ci.edgeId],
					nbr[(ci.edgeId + 1) % nbr.size()], &pt2);
			dir = (pt2 - start);
			len = dir.length();
			w = ((len - 2 * SpringLevelSet::PARTICLE_RADIUS)
					/ (SpringLevelSet::MAX_VEXT
							+ 2 * SpringLevelSet::PARTICLE_RADIUS));

			w = atanh(MAX_FORCE * clamp(w, -1.0f, 1.0f));
			startVelocity += (w * dir);
		}
		if (map.size() > 0)
			startVelocity /= map.size();
		len = std::max(1E-6f, startVelocity.length());
		float t = SpringLevelSet::SHARPNESS * len;
		vertexVelocity[k] = SpringLevelSet::RELAX_TIMESTEP * startVelocity
				* (t / len);
		springForce[k] = SpringLevelSet::RELAX_TIMESTEP
				* SpringLevelSet::SPRING_CONSTANT
				* (2 * SpringLevelSet::PARTICLE_RADIUS - tangetLengths[k]);

		resultantMoment += vertexVelocity[k].cross(tangets[k]);
	}
	//std::cout<<"moment "<<resultantMoment<<" Normal "<<*springl.normal<<std::endl;

	openvdb::math::Mat3<float> rot = CreateAxisAngle(resultantMoment,
			-resultantMoment.length());

	//std::cout<<"Rotation\n"<<rot<<std::endl;

	//std::cout<<springl.id<<springl.offset<<" ROTATION "<<resultantMoment<<" "<<springForce[0]<<" "<<vertexVelocity[0]<<std::endl;
	for (int k = 0; k < K; k++) {
		start = springl[k] - particlePt;
		dotProd = std::max(
				start.length() + vertexVelocity[k].dot(tangets[k])
						+ springForce[k], 0.001f);
		start = dotProd * tangets[k];

		//disable rotation
		start = rot * start;
		mGrid.mConstellation.mVertexAuxBuffer[springl.offset + k] = start
				+ particlePt;
	}
}
void NearestNeighborOperation::init(SpringLevelSet& mGrid) {
	NearestNeighborMap& map = mGrid.mNearestNeighbors;
	map.clear();
	map.resize(mGrid.mConstellation.getNumVertexes(),
			std::list<SpringlNeighbor>());
}
void NearestNeighborOperation::compute(Springl& springl, SpringLevelSet& mGrid,
		double t) {
	const float D2 = SpringLevelSet::NEAREST_NEIGHBOR_RANGE
			* SpringLevelSet::NEAREST_NEIGHBOR_RANGE;

	openvdb::math::DenseStencil<openvdb::Int32Grid> stencil =
			openvdb::math::DenseStencil<openvdb::Int32Grid>(
					*mGrid.mSpringlIndexGrid,
					ceil(SpringLevelSet::NEAREST_NEIGHBOR_RANGE));
	openvdb::Vec3s refPoint = springl.particle();

	stencil.moveTo(
			Coord(std::floor(refPoint[0] + 0.5f),
					std::floor(refPoint[1] + 0.5f),
					std::floor(refPoint[2] + 0.5f)));
	int sz = stencil.size();
	if (sz == 0)
		return;
	Index32 N = mGrid.mConstellation.getNumSpringls();
	std::vector<openvdb::Index32> stencilCopy;
	for (int i = 0; i < sz; i++) {
		openvdb::Index32 id = stencil.getValue(i);
		if (id >= N)
			continue;
		openvdb::Vec3s nbr = mGrid.getParticle(id);
		float d = (refPoint - nbr).lengthSqr();
		if (id != springl.id) {
			stencilCopy.push_back(id);
		}
	}
	if (stencilCopy.size() == 0)
		return;
	std::sort(stencilCopy.begin(), stencilCopy.end());
	sz = stencilCopy.size();

	openvdb::Index32 last = -1;
	SpringlNeighbor bestNbr;
	std::vector<SpringlNeighbor> tmpRange;
	for (int k = 0; k < springl.size(); k++) {
		std::list<SpringlNeighbor>& mapList = mGrid.getNearestNeighbors(
				springl.id, k);
		refPoint = springl[k];
		//
		last = -1;

		tmpRange.clear();
		for (int i = 0; i < sz; i++) {
			openvdb::Index32 nbrId = stencilCopy[i];
			if (nbrId == last)
				continue;
			Springl& snbr = mGrid.getSpringl(nbrId);
			bestNbr = SpringlNeighbor(nbrId, -1, D2);
			for (int8_t n = 0; n < snbr.size(); n++) {
				float d = snbr.distanceToEdgeSqr(refPoint, n);
				if (d <= bestNbr.distance) {
					bestNbr.edgeId = n;
					bestNbr.distance = d;
				}
			}
			if (bestNbr.edgeId >= 0)
				tmpRange.push_back(bestNbr);
			last = nbrId;
		}
		sort(tmpRange.begin(), tmpRange.end());
		for (int nn = 0, nmax = std::min(SpringLevelSet::MAX_NEAREST_NEIGHBORS,
				(int) tmpRange.size()); nn < nmax; nn++) {
			mapList.push_back(tmpRange[nn]);
		}
		//if (bestNbr.springlId >= 0 && bestNbr.springlId < N)mapList.push_back(bestNbr);
	}
}

void SpringLevelSet::updateNearestNeighbors(bool threaded) {
	using namespace openvdb;
	NearestNeighbors<openvdb::util::NullInterrupter> nn(*this);
	nn.process();
}
void SpringLevelSet::updateLines() {
	std::vector<Vec3s>& lines = mConstellation.mLines;
	lines.clear();
	float d;
	Vec3s pt, qt;
	for (Index32 i = 0; i < mConstellation.getNumSpringls(); i++) {

		Springl& springl = mConstellation.springls[i];

		for (int k = 0; k < springl.size(); k++) {
			//std::cout <<i << "={"<<k<<":: ";
			for (SpringlNeighbor nbr : getNearestNeighbors(i, k)) {
				pt = springl[k];
				lines.push_back(pt);
				qt = mConstellation.closestPointOnEdge(pt, nbr);
				lines.push_back(qt);
				//std::cout << nbr << " ";
			}

			//std::cout << "}" << std::endl;
		}
	}
}
void SpringLevelSet::relax(int iters) {
	Relax<openvdb::util::NullInterrupter> relax(*this);
	for (int iter = 0; iter < iters; iter++) {
		relax.process();
	}
}
void SpringLevelSet::evolve() {

	updateGradient();

	VelocityField grad(*mGradient);
	AdvectionTool advect(*mSignedLevelSet, grad);
	advect.setSpatialScheme(openvdb::math::FIRST_BIAS);
	advect.setTemporalScheme(openvdb::math::TVD_RK2);
	advect.setTrackerSpatialScheme(openvdb::math::FIRST_BIAS);
	advect.setTrackerTemporalScheme(openvdb::math::TVD_RK2);
	int steps = advect.advect(0.0, 4.0);

}
void SpringLevelSet::updateUnSignedLevelSet(double distance) {
	openvdb::math::Transform::Ptr trans =
			openvdb::math::Transform::createLinearTransform(1.0f);
	using namespace openvdb::tools;
	using namespace openvdb;
	MeshToVolume<FloatGrid> mtol(trans, GENERATE_PRIM_INDEX_GRID);
	mtol.convertToUnsignedDistanceField(mConstellation.mVertexes,
			mConstellation.mFaces, distance);
	mUnsignedLevelSet = mtol.distGridPtr();
	mUnsignedLevelSet->setBackground(distance);
	mSpringlIndexGrid = mtol.indexGridPtr();
}
double SpringLevelSet::distanceToConstellation(const Vec3s& pt) {
	openvdb::math::DenseStencil<openvdb::Int32Grid> stencil =
			openvdb::math::DenseStencil<openvdb::Int32Grid>(*mSpringlIndexGrid,
					ceil(FILL_DISTANCE));
	stencil.moveTo(
			Coord((int) floor(pt[0] + 0.5f), (int) floor(pt[1] + 0.5f),
					(int) floor(pt[2] + 0.5f)));
	int sz = stencil.size();
	double levelSetValue = std::numeric_limits<float>::max();
	std::vector<Index32> stencilCopy;
	stencilCopy.clear();
	Index32 last = -1;
	Index32 springlsCount = mConstellation.getNumSpringls();
	for (unsigned int nn = 0; nn < sz; nn++) {
		openvdb::Index32 id = stencil.getValue(nn);
		if (id >= springlsCount)
			continue;
		stencilCopy.push_back(id);
	}
	sz = stencilCopy.size();
	sort(stencilCopy.begin(), stencilCopy.end());
	for (Index32 id : stencilCopy) {
		if (last != id) {
			float d = mConstellation.springls[id].distanceToFaceSqr(pt);
			if (d < levelSetValue) {
				levelSetValue = d;
			}
		}
		last = id;
	}
	return std::sqrt(levelSetValue);
}
void SpringLevelSet::updateSignedLevelSet() {
	openvdb::math::Transform::Ptr trans =
			openvdb::math::Transform::createLinearTransform(1.0);
	using namespace openvdb::tools;
	using namespace openvdb;
	MeshToVolume<FloatGrid> mtol(trans);
	mtol.convertToLevelSet(mIsoSurface.mVertexes, mIsoSurface.mFaces,
			float(LEVEL_SET_HALF_WIDTH));
	mSignedLevelSet = mtol.distGridPtr();
}

void SpringLevelSet::updateGradient() {
	//mGradient = openvdb::tools::mGradient(*mUnsignedLevelSet);
	mGradient = advectionForce(*mUnsignedLevelSet);

}
std::list<SpringlNeighbor>& SpringLevelSet::getNearestNeighbors(
		openvdb::Index32 id, int8_t e) {
	return mNearestNeighbors[mConstellation.springls[id].offset + e];
}
void SpringLevelSet::create(Mesh* mesh,
		openvdb::math::Transform::Ptr _transform) {
	this->mTransform = _transform;
	openvdb::math::Transform::Ptr trans =
			openvdb::math::Transform::createLinearTransform(1.0);
	openvdb::tools::MeshToVolume<openvdb::FloatGrid> mtol(trans);
	mtol.convertToLevelSet(mesh->mVertexes, mesh->mFaces);
	mSignedLevelSet = mtol.distGridPtr();
	mIsoSurface.create(mSignedLevelSet);
	mConstellation.create(&mIsoSurface);
	updateIsoSurface();
	for (int iter = 0; iter < 2; iter++) {
		updateUnSignedLevelSet();
		updateNearestNeighbors();
		relax(10);
		updateUnSignedLevelSet(2.5 * openvdb::LEVEL_SET_HALF_WIDTH);
		clean();
		updateUnSignedLevelSet();
		fill();
		fillWithNearestNeighbors();
	}
	updateGradient();

}
void SpringLevelSet::create(FloatGrid& grid) {
	this->mTransform = grid.transformPtr();
	grid.setTransform(openvdb::math::Transform::createLinearTransform(1.0));
	mSignedLevelSet = boost::static_pointer_cast<FloatGrid>(
			grid.copyGrid(CopyPolicy::CP_COPY));
	mIsoSurface.create(mSignedLevelSet);
	updateSignedLevelSet();
	mConstellation.create(&mIsoSurface);
	updateIsoSurface();
	for (int iter = 0; iter < 2; iter++) {
		updateUnSignedLevelSet();
		updateNearestNeighbors();
		relax(10);
		updateUnSignedLevelSet(2.5 * openvdb::LEVEL_SET_HALF_WIDTH);
		clean();
		updateUnSignedLevelSet();
		fill();
		fillWithNearestNeighbors();
	}
	updateGradient();
}
void SpringLevelSet::create(RegularGrid<float>& grid) {
	this->mTransform = grid.transformPtr();
	mSignedLevelSet = std::unique_ptr<FloatGrid>(new FloatGrid());
	mSignedLevelSet->setBackground(openvdb::LEVEL_SET_HALF_WIDTH);
	mSignedLevelSet->setTransform(grid.transformPtr());
	openvdb::tools::copyFromDense(grid, *mSignedLevelSet, 0.25f);
	mSignedLevelSet->setTransform(Transform::createLinearTransform(1.0));
	mIsoSurface.create(mSignedLevelSet);
	updateSignedLevelSet();
	mConstellation.create(&mIsoSurface);
	updateIsoSurface();
	for (int iter = 0; iter < 2; iter++) {
		updateUnSignedLevelSet();
		updateNearestNeighbors();
		relax(10);
		updateUnSignedLevelSet(2.5 * openvdb::LEVEL_SET_HALF_WIDTH);
		clean();
		updateUnSignedLevelSet();
		fill();
		fillWithNearestNeighbors();
	}
	updateGradient();
}
void SpringLevelSet::updateIsoSurface() {

	mVolToMesh(*mSignedLevelSet);
	mIsoSurface.create(mVolToMesh, mSignedLevelSet);

}
int SpringLevelSet::fill() {

	openvdb::tree::ValueAccessor<FloatGrid::TreeType> acc(
			mSignedLevelSet->tree());
	openvdb::math::GenericMap map(mSignedLevelSet->transform());

	openvdb::math::DenseStencil<openvdb::Int32Grid> stencil =
			openvdb::math::DenseStencil<openvdb::Int32Grid>(*mSpringlIndexGrid,
					ceil(FILL_DISTANCE));

	openvdb::tools::PolygonPoolList& polygonPoolList =
			mVolToMesh.polygonPoolList();
	Vec3s p[4];
	Vec3s refPoint;

	Index32 springlsCount = mConstellation.getNumSpringls();
	Index32 pcounter = mConstellation.getNumSpringls();
	Index32 counter = mConstellation.getNumVertexes();
	int added = 0;

	const float D2 = FILL_DISTANCE * FILL_DISTANCE;
	Index64 N = mVolToMesh.polygonPoolListSize();
	Index64 I;
	fillList.clear();
	for (Index64 n = 0; n < N; ++n) {
		const openvdb::tools::PolygonPool& polygons = polygonPoolList[n];
		I = polygons.numQuads();
#pragma omp for
		for (Index64 i = 0; i < I; ++i) {
			std::vector<openvdb::Index32> stencilCopy;
			const openvdb::Vec4I& quad = polygons.quad(i);
			p[0] = mVolToMesh.pointList()[quad[3]];
			p[1] = mVolToMesh.pointList()[quad[2]];
			p[2] = mVolToMesh.pointList()[quad[1]];
			p[3] = mVolToMesh.pointList()[quad[0]];
			refPoint = 0.25f * (p[0] + p[1] + p[2] + p[3]);
			stencil.moveTo(
					Coord(std::floor(refPoint[0] + 0.5f),
							std::floor(refPoint[1] + 0.5f),
							std::floor(refPoint[2] + 0.5f)));
			int sz = stencil.size();
			float levelSetValue = std::numeric_limits<float>::max();
			int last = -1;
			for (unsigned int nn = 0; nn < sz; nn++) {
				openvdb::Index32 id = stencil.getValue(nn);
				if (id >= springlsCount)
					continue;
				stencilCopy.push_back(id);
			}
			sz = stencilCopy.size();
			sort(stencilCopy.begin(), stencilCopy.end());
			for (unsigned int nn = 0; nn < sz; nn++) {
				openvdb::Index32 id = stencilCopy[nn];
				if (last != id) {
					float d = mConstellation.springls[id].distanceToFaceSqr(
							refPoint);
					if (d < levelSetValue) {
						levelSetValue = d;
					}
				}
				last = id;
			}
			if (levelSetValue > D2) {
#pragma omp critical
				{
					added++;
					mConstellation.mQuadIndexes.push_back(counter);
					mConstellation.mVertexes.push_back(p[0]);
					mConstellation.mQuadIndexes.push_back(counter + 1);
					mConstellation.mVertexes.push_back(p[1]);
					mConstellation.mQuadIndexes.push_back(counter + 2);
					mConstellation.mVertexes.push_back(p[2]);
					mConstellation.mQuadIndexes.push_back(counter + 3);
					mConstellation.mVertexes.push_back(p[3]);

					Springl springl(&mConstellation);
					springl.offset = counter;
					springl.id = mConstellation.springls.size();
					mConstellation.mFaces.push_back(
							Vec4I(counter, counter + 1, counter + 2,
									counter + 3));
					mConstellation.mParticles.push_back(
							springl.computeCentroid());
					if (mConstellation.mParticleVelocity.size() > 0) {
						fillList.push_back(springl.id);
						mConstellation.mParticleVelocity.push_back(Vec3s(0.0f));
					}
					if(mConstellation.mVertexVelocity.size()>0){
						mConstellation.mVertexVelocity.push_back(Vec3s(0.0f));
						mConstellation.mVertexVelocity.push_back(Vec3s(0.0f));
						mConstellation.mVertexVelocity.push_back(Vec3s(0.0f));
						mConstellation.mVertexVelocity.push_back(Vec3s(0.0f));
					}
					if(mConstellation.mParticleLabel.size()>0){
						mConstellation.mParticleLabel.push_back(0);
					}
					openvdb::Vec3s norm = springl.computeNormal();
					mConstellation.mParticleNormals.push_back(norm);
					mConstellation.mVertexNormals.push_back(norm);
					mConstellation.mVertexNormals.push_back(norm);
					mConstellation.mVertexNormals.push_back(norm);
					mConstellation.mVertexNormals.push_back(norm);
					mConstellation.springls.push_back(springl);
					pcounter++;
					counter += 4;
				}
			}
		}
		I = polygons.numTriangles();
#pragma omp for
		for (Index64 i = 0; i < I; ++i) {
			std::vector<openvdb::Index32> stencilCopy;
			const openvdb::Vec3I& tri = polygons.triangle(i);
			p[0] = mVolToMesh.pointList()[tri[2]];
			p[1] = mVolToMesh.pointList()[tri[1]];
			p[2] = mVolToMesh.pointList()[tri[0]];
			refPoint = 0.25f * (p[0] + p[1] + p[2]);
			stencil.moveTo(
					Coord(std::floor(refPoint[0] + 0.5f),
							std::floor(refPoint[1] + 0.5f),
							std::floor(refPoint[2] + 0.5f)));
			int sz = stencil.size();
			float levelSetValue = std::numeric_limits<float>::max();
			int last = -1;
			for (unsigned int nn = 0; nn < sz; nn++) {
				openvdb::Index32 id = stencil.getValue(nn);
				if (id >= springlsCount)
					continue;
				stencilCopy.push_back(id);
			}
			sz = stencilCopy.size();
			sort(stencilCopy.begin(), stencilCopy.end());
			for (unsigned int nn = 0; nn < sz; nn++) {
				openvdb::Index32 id = stencilCopy[nn];
				if (last != id) {
					float d = mConstellation.springls[id].distanceToFaceSqr(
							refPoint);
					if (d < levelSetValue) {
						levelSetValue = d;
					}
				}
				last = id;
			}
			if (levelSetValue > D2) {
#pragma omp critical
				{
					added++;
					mConstellation.mTriIndexes.push_back(counter);
					mConstellation.mVertexes.push_back(p[0]);
					mConstellation.mTriIndexes.push_back(counter + 1);
					mConstellation.mVertexes.push_back(p[1]);
					mConstellation.mTriIndexes.push_back(counter + 2);
					mConstellation.mVertexes.push_back(p[2]);
					Springl springl(&mConstellation);
					springl.offset = counter;
					springl.id = mConstellation.springls.size();

					mConstellation.mFaces.push_back(
							Vec4I(counter, counter + 1, counter + 2,
									openvdb::util::INVALID_IDX));
					mConstellation.mParticles.push_back(
							springl.computeCentroid());
					if (mConstellation.mParticleVelocity.size() > 0) {
						fillList.push_back(springl.id);
						mConstellation.mParticleVelocity.push_back(Vec3s(0.0f));
					}
					if(mConstellation.mVertexVelocity.size()>0){
						mConstellation.mVertexVelocity.push_back(Vec3s(0.0f));
						mConstellation.mVertexVelocity.push_back(Vec3s(0.0f));
						mConstellation.mVertexVelocity.push_back(Vec3s(0.0f));
					}
					if(mConstellation.mParticleLabel.size()>0){
						mConstellation.mParticleLabel.push_back(0);
					}
					openvdb::Vec3s norm = springl.computeNormal();

					mConstellation.mParticleNormals.push_back(norm);
					mConstellation.mVertexNormals.push_back(norm);
					mConstellation.mVertexNormals.push_back(norm);
					mConstellation.mVertexNormals.push_back(norm);
					mConstellation.springls.push_back(springl);
					pcounter++;
					counter += 3;
				}
			}
		}
	}

	mFillCount += added;
	return added;
}
void SpringLevelSet::fillWithVelocityField(MACGrid<float>& grid,float radius){
	for (int fid : fillList) {
		Springl& springl = mConstellation.springls[fid];
		//Is particle() in the correct coordinate space?
		Vec3d wpt=transform().indexToWorld(springl.particle());
		mConstellation.mParticleVelocity[fid] = grid.maxInterpolate(Vec3s(wpt),radius);
		for(int n=0;n<springl.size();n++){
			wpt=transform().indexToWorld(springl[n]);
			mConstellation.mVertexVelocity[springl.offset+n]=grid.maxInterpolate(Vec3s(wpt),radius);
		}
	}
	fillList.clear();
}

void SpringLevelSet::fillWithNearestNeighbors(){
	if (fillList.size() > 0) {
		updateUnSignedLevelSet();
		updateNearestNeighbors();
		for (int cycle = 0; cycle < 16; cycle++) {
			int unfilledCount = 0;
			for (int fid : fillList) {
				Springl& springl = mConstellation.springls[fid];
				int K = springl.size();
				Vec3s vel(0.0);
				double wsum = 0.0;
				for (int k = 0; k < K; k++) {
					std::list<SpringlNeighbor>& map = getNearestNeighbors(
							springl.id, k);
					for (SpringlNeighbor ci : map) {
						Springl& nbr = getSpringl(ci.springlId);
						Vec3s v = nbr.particleVelocity();
						if (v.lengthSqr() > 0) {
							vel += v;
							wsum += 1.0f;
						}
					}
				}
				if (wsum > 0.0f) {
					vel *= 1.0 / wsum;
					mConstellation.mParticleVelocity[fid] = vel;
					for(int n=0;n<springl.size();n++){
						mConstellation.mVertexVelocity[springl.offset+n]=vel;
					}
				} else {
					unfilledCount++;
				}
			}
			if (unfilledCount == 0)
				break;
			std::cout << cycle << ":: un-filled " << unfilledCount << std::endl;
		}
		fillList.clear();
	}
}
void SpringLevelSet::computeStatistics(Mesh& mesh) {
	float area;
	float minEdgeLength, maxEdgeLength;
	int K;
	std::vector<Index32> keepList;
	Index32 newVertexCount = 0;
	Index32 newSpringlCount = 0;
	int N = mConstellation.getNumSpringls();
	keepList.reserve(N);
	Index32 index = 0;
	double minls = 1E30, bias = 0, maxls = -1E30, meanls = 0, v, sqrs = 0,
			stdev;
	int count = 0;
	for (Vec3s pt : mesh.mVertexes) {
		float levelSetValue = distanceToConstellation(pt);
		count++;
		v = fabs(levelSetValue);
		sqrs += v * v;
		meanls += v;
		bias += levelSetValue;
		minls = std::min(minls, v);
		maxls = std::max(maxls, v);
		count++;
	}
	meanls /= count;
	bias /= count;
	stdev = std::sqrt(sqrs / count - meanls * meanls);
	std::cout << ">>Constellation Vertex mean=" << meanls << " std dev.="
			<< stdev << " bias=" << bias << " [" << minls << "," << maxls << "]"
			<< std::endl;
	meanls = 0;
	bias = 0;
	sqrs = 0;
	minls = 1E30;
	maxls = -1E30;
	count = 0;

	for (Vec3s pt : mesh.mParticles) {
		float levelSetValue = distanceToConstellation(pt);
		count++;
		v = fabs(levelSetValue);
		sqrs += v * v;
		meanls += v;
		bias += levelSetValue;
		minls = std::min(minls, v);
		maxls = std::max(maxls, v);
		count++;
	}
	meanls /= count;
	bias /= count;

	stdev = std::sqrt(sqrs / count - meanls * meanls);
	if (count > 0)
		std::cout << ">>Constellation Particle mean=" << meanls << " std dev.="
				<< stdev << " bias=" << bias << " [" << minls << "," << maxls
				<< "]" << std::endl;
}
void SpringLevelSet::computeStatistics(Mesh& mesh, FloatGrid& levelSet) {
	openvdb::math::BoxStencil<openvdb::FloatGrid> stencil(levelSet);
	float area;
	float minEdgeLength, maxEdgeLength;
	int K;
	std::vector<Index32> keepList;
	Index32 newVertexCount = 0;
	Index32 newSpringlCount = 0;
	int N = mConstellation.getNumSpringls();
	keepList.reserve(N);
	Index32 index = 0;
	double minls = 1E30, bias = 0, maxls = -1E30, meanls = 0, v, sqrs = 0,
			stdev;
	int count = 0;
	for (Vec3s pt : mesh.mVertexes) {
		stencil.moveTo(
				Coord(std::floor(pt[0]), std::floor(pt[1]), std::floor(pt[2])));
		float levelSetValue = stencil.interpolation(pt);
		count++;
		v = fabs(levelSetValue);
		sqrs += v * v;
		meanls += v;
		bias += levelSetValue;
		minls = std::min(minls, v);
		maxls = std::max(maxls, v);
		count++;
	}
	meanls /= count;
	bias /= count;

	stdev = std::sqrt(sqrs / count - meanls * meanls);
	std::cout << ">>Vertex mean=" << meanls << " std dev.=" << stdev << " bias="
			<< bias << " [" << minls << "," << maxls << "]" << std::endl;

	meanls = 0;
	bias = 0;
	sqrs = 0;
	minls = 1E30;
	maxls = -1E30;
	count = 0;

	for (Vec3s pt : mesh.mParticles) {
		stencil.moveTo(
				Coord(std::floor(pt[0]), std::floor(pt[1]), std::floor(pt[2])));
		float levelSetValue = stencil.interpolation(pt);
		count++;
		v = fabs(levelSetValue);
		sqrs += v * v;
		meanls += v;
		bias += levelSetValue;
		minls = std::min(minls, v);
		maxls = std::max(maxls, v);
		count++;
	}
	meanls /= count;
	bias /= count;

	stdev = std::sqrt(sqrs / count - meanls * meanls);
	if (count > 0)
		std::cout << ">>Particle mean=" << meanls << " std dev.=" << stdev
				<< " bias=" << bias << " [" << minls << "," << maxls << "]"
				<< std::endl;

}
void Constellation::create(Mesh* mesh) {
	size_t faceCount = mesh->mFaces.size();
	size_t counter = 0;
	size_t pcounter = 0;
	springls.clear();
	mFaces.clear();
	mQuadIndexes.clear();
	mTriIndexes.clear();
	mVertexes.clear();
	mVertexes.resize(mesh->mQuadIndexes.size() + mesh->mTriIndexes.size());
	mParticles.resize(faceCount);
	mParticleNormals.resize(faceCount);
	mVertexNormals.resize(mVertexes.size());
	mParticleVelocity=mesh->mParticleVelocity;
	mVertexVelocity=mesh->mVertexVelocity;
	for (openvdb::Vec4I face : mesh->mFaces) {
		Springl springl(this);
		springl.offset = counter;
		springl.id = springls.size();
		if (face[3] != openvdb::util::INVALID_IDX) {
			mFaces.push_back(
					openvdb::Vec4I(counter, counter + 1, counter + 2,
							counter + 3));
			mQuadIndexes.push_back(counter);
			mQuadIndexes.push_back(counter + 1);
			mQuadIndexes.push_back(counter + 2);
			mQuadIndexes.push_back(counter + 3);
			mVertexes[counter++] = mesh->mVertexes[face[0]];
			mVertexes[counter++] = mesh->mVertexes[face[1]];
			mVertexes[counter++] = mesh->mVertexes[face[2]];
			mVertexes[counter++] = mesh->mVertexes[face[3]];
			mParticles[pcounter] = springl.computeCentroid();
			openvdb::Vec3s norm = springl.computeNormal();
			mParticleNormals[pcounter] = norm;
			mVertexNormals[counter - 1] = norm;
			mVertexNormals[counter - 2] = norm;
			mVertexNormals[counter - 3] = norm;
			mVertexNormals[counter - 4] = norm;
			springls.push_back(springl);
		} else {
			mFaces.push_back(
					openvdb::Vec4I(counter, counter + 1, counter + 2,
							openvdb::util::INVALID_IDX));
			mTriIndexes.push_back(counter);
			mTriIndexes.push_back(counter + 1);
			mTriIndexes.push_back(counter + 2);
			mVertexes[counter++] = mesh->mVertexes[face[0]];
			mVertexes[counter++] = mesh->mVertexes[face[1]];
			mVertexes[counter++] = mesh->mVertexes[face[2]];
			mParticles[pcounter] = springl.computeCentroid();
			openvdb::Vec3s norm = springl.computeNormal();
			mParticleNormals[pcounter] = norm;
			mVertexNormals[counter - 1] = norm;
			mVertexNormals[counter - 2] = norm;
			mVertexNormals[counter - 3] = norm;

			springls.push_back(springl);
		}
		pcounter++;
	}
	updateBoundingBox();
}

int SpringLevelSet::clean() {
	openvdb::math::BoxStencil<openvdb::FloatGrid> stencil(*mSignedLevelSet);
	Vec3s pt, pt1, pt2, pt3;
	float area;
	float minEdgeLength, maxEdgeLength;
	std::vector<Index32> keepList;
	Index32 newVertexCount = 0;
	Index32 newSpringlCount = 0;
	int N = mConstellation.getNumSpringls();
	keepList.reserve(N);
	Index32 index = 0;
	double minls = 1E30, bias = 0, maxls = -1E30, meanls = 0;
	int count = 0;
	int removeFarCount = 0;
	int removeSmallCount = 0;
	int removeAspectCount = 0;
	std::vector<float> levelSetValues(mConstellation.springls.size());
#pragma omp for
	for (int nn = 0; nn < mConstellation.springls.size(); nn++) {
		Springl& springl = mConstellation.springls[nn];
		pt = springl.particle();
		stencil.moveTo(
				Coord(std::floor(pt[0]), std::floor(pt[1]), std::floor(pt[2])));
		levelSetValues[nn] = stencil.interpolation(pt);
	}
	for (Springl& springl : mConstellation.springls) {
		float levelSetValue = levelSetValues[count++];
		int K = springl.size();
		double v = std::abs(levelSetValue);
		meanls += v;
		bias += levelSetValue;
		minls = std::min(minls, v);
		maxls = std::max(maxls, v);
		if (v <= CLEAN_DISTANCE) {
			minEdgeLength = 1E30;
			maxEdgeLength = -1E30;
			area = 0.0f;
			for (int i = 0; i < K; i++) {
				pt1 = springl[i];
				pt2 = springl[(i + 1) % K];
				float len = (pt1 - pt2).length();
				minEdgeLength = std::min(minEdgeLength, len);
				maxEdgeLength = std::max(maxEdgeLength, len);
			}
			float aspect = minEdgeLength / maxEdgeLength;
			area = springl.area();
			//std::cout<<"AREA "<<area<<" ASPECT "<<aspect<<std::endl;
			if (area >= MIN_AREA && area < MAX_AREA
					&& aspect >= MIN_ASPECT_RATIO) {
				keepList.push_back(springl.id);
				newSpringlCount++;
				newVertexCount += K;
			} else {
				if (area < MIN_AREA || area >= MAX_AREA) {
					removeSmallCount++;
				}
				if (aspect < MIN_ASPECT_RATIO) {
					removeAspectCount++;
				}
			}
		} else {
			removeFarCount++;
		}
		index++;
	}
	levelSetValues.clear();
	meanls /= count;
	bias /= count;

	std::cout << "Clean mean=" << meanls << " bias=" << bias << " [" << minls<< "," << maxls << "] [far:" << removeFarCount << ", small:"<< removeSmallCount << ", aspect:" << removeAspectCount << "]" << std::endl;

	if (newSpringlCount == N)
		return 0;
	Index32 springlOffset = 0;
	Index32 vertexOffset = 0;
	Index32 quadIndex = 0;
	Index32 triIndex = 0;
	for (int n : keepList) {
		Springl& rspringl = mConstellation.springls[n];
		Springl& springl = mConstellation.springls[springlOffset];
		int K = rspringl.size();
		if (springlOffset != n) {
			mConstellation.mParticles[springlOffset] =
					mConstellation.mParticles[n];

			if (mConstellation.mParticleVelocity.size() > 0) {
				mConstellation.mParticleVelocity[springlOffset]=mConstellation.mParticleVelocity[n];
			}
			if(mConstellation.mVertexVelocity.size()>0){
				for(int n=0;n<springl.size();n++){
					mConstellation.mVertexVelocity[springl.offset+n]=mConstellation.mVertexVelocity[rspringl.offset+n];
				}
			}
			if(mConstellation.mParticleLabel.size()>0){
				mConstellation.mParticleLabel[springlOffset]=mConstellation.mParticleLabel[n];
			}
			mConstellation.mParticleNormals[springlOffset] =
					mConstellation.mParticleNormals[n];
			springl.offset = vertexOffset;
			springl.id = springlOffset;
			Vec4I quad;
			quad[3] = openvdb::util::INVALID_IDX;
			for (int k = 0; k < K; k++) {
				mConstellation.mVertexes[vertexOffset + k] =
						mConstellation.mVertexes[rspringl.offset + k];
				mConstellation.mVertexNormals[vertexOffset + k] =
						mConstellation.mVertexNormals[rspringl.offset + k];
				quad[k] = vertexOffset + k;
			}
			if (K == 4) {
				for (int k = 0; k < K; k++) {
					mConstellation.mQuadIndexes[quadIndex++] = vertexOffset + k;
				}
			} else if (K == 3) {
				for (int k = 0; k < K; k++) {
					mConstellation.mTriIndexes[triIndex++] = vertexOffset + k;
				}
			}
			mConstellation.mFaces[springlOffset] = quad;
		} else {
			if (K == 4) {
				quadIndex += K;
			} else if (K == 3) {
				triIndex += K;
			}
		}
		vertexOffset += K;
		springlOffset++;
	}
	mConstellation.mTriIndexes.erase(
			mConstellation.mTriIndexes.begin() + triIndex,
			mConstellation.mTriIndexes.end());

	mConstellation.mQuadIndexes.erase(
			mConstellation.mQuadIndexes.begin() + quadIndex,
			mConstellation.mQuadIndexes.end());

	mConstellation.springls.erase(
			mConstellation.springls.begin() + springlOffset,
			mConstellation.springls.end());
	mConstellation.mParticles.erase(
			mConstellation.mParticles.begin() + springlOffset,
			mConstellation.mParticles.end());

	if (mConstellation.mParticleVelocity.size() > 0) {
		mConstellation.mParticleVelocity.erase(
				mConstellation.mParticleVelocity.begin() + springlOffset,
				mConstellation.mParticleVelocity.end());
	}
	if (mConstellation.mVertexVelocity.size() > 0) {
		mConstellation.mVertexVelocity.erase(
				mConstellation.mVertexVelocity.begin() + vertexOffset,
				mConstellation.mVertexVelocity.end());
	}
	mConstellation.mParticleNormals.erase(
			mConstellation.mParticleNormals.begin() + springlOffset,
			mConstellation.mParticleNormals.end());
	mConstellation.mFaces.erase(mConstellation.mFaces.begin() + springlOffset,
			mConstellation.mFaces.end());

	mConstellation.mVertexNormals.erase(
			mConstellation.mVertexNormals.begin() + vertexOffset,
			mConstellation.mVertexNormals.end());
	mConstellation.mVertexes.erase(
			mConstellation.mVertexes.begin() + vertexOffset,
			mConstellation.mVertexes.end());
	mCleanCount += (N - newSpringlCount);
	return (N - newSpringlCount);
}

}

