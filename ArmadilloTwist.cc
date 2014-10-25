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
 */

#include "ArmadilloTwist.h"

namespace imagesci {

ArmadilloTwist::ArmadilloTwist(const std::string& fileName):Simulation("Twist"),mSourceFileName(fileName) {
}

ArmadilloTwist::~ArmadilloTwist() {
}
bool ArmadilloTwist::init(){
	Mesh mesh;
	if(!mesh.openMesh(mSourceFileName))return false;
	mesh.mapIntoBoundingBox(mesh.estimateVoxelSize());
	mesh.updateBoundingBox();
    openvdb::math::Transform::Ptr trans=openvdb::math::Transform::createLinearTransform();
    mSource.create(&mesh);
    BBoxd bbox=mSource.mIsoSurface.updateBoundingBox();
	trans=mSource.mSignedLevelSet->transformPtr();
    Vec3d extents=bbox.extents();
	double max_extent = std::max(extents[0], std::max(extents[1], extents[2]));
	double scale=1.0/max_extent;
    const openvdb::Vec3f center(0.0f,0.0f,0.0f);
	Vec3s t=-0.5f*(bbox.min()+bbox.max());
	trans=mSource.transformPtr();
	trans->postTranslate(t);
	trans->postScale(scale);
	trans->postTranslate(center);
	//trans->indexToWorld()
	mField=std::unique_ptr<FieldT>(new TwistField<float>(Mat4s::identity(),0.0));
	mAdvect=std::unique_ptr<AdvectT>(new AdvectT(mSource,*mField));
	mAdvect->setTemporalScheme(imagesci::TemporalIntegrationScheme::RK4b);
	mAdvect->setMotionScheme(MotionScheme::EXPLICIT);
	mAdvect->setResampleEnabled(true);
	mSimulationDuration=2*M_PI;
	mTimeStep=mSimulationDuration/180.0f;
	mIsMeshDirty=true;
	return true;
}
bool ArmadilloTwist::step(){
	mAdvect->advect(mSimulationTime,mSimulationTime+mTimeStep);
	mIsMeshDirty=true;
	mSimulationIteration++;
	mSimulationTime=mTimeStep*mSimulationIteration;
	if(mSimulationTime<=mSimulationDuration&&mRunning){
		return true;
	} else {
		mSimulationIteration--;
		mSimulationTime=mSimulationDuration;
		return false;
	}
}
void ArmadilloTwist::cleanup(){
	mAdvect.reset();
}
} /* namespace imagesci */