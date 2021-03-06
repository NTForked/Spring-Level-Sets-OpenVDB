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

#include "DamBreakSimulation.h"
#include <openvdb/tools/DenseSparseTools.h>
namespace imagesci {
using namespace openvdb::tools;
using namespace imagesci::fluid;
DamBreakSimulation::DamBreakSimulation(const std::string& fileName,int gridSize,MotionScheme scheme):FluidSimulation(Coord(gridSize,gridSize,gridSize),1.0f/gridSize,scheme),mSourceFileName(fileName),mGridSize(gridSize) {
}

bool DamBreakSimulation::init(){

	bool ret=FluidSimulation::init();
	mIsMeshDirty=true;
	return ret;
}
void DamBreakSimulation::cleanup(){
	mAdvect.reset();
	FluidSimulation::cleanup();
}
void DamBreakSimulation::addFluid(){
	//replace with level set for falling object
	BoxObject* obj=new BoxObject;
	Coord dims=mLabel.dimensions();
	obj->mType = ObjectType::FLUID;
	obj->mVisible = true;
	obj->mMin = Vec3f(0.2, mWallThickness, 0.2);
	obj->mMax = Vec3f(
			0.4*mVoxelSize*dims[0],
			0.4*mVoxelSize*dims[1],
			0.8*mVoxelSize*dims[2]);
	obj->mVoxelSize=mVoxelSize;

	addSimulationObject(static_cast<SimulationObject*>(obj));
	obj=new BoxObject;
	obj->mType = ObjectType::FLUID;
	obj->mVisible = true;
	obj->mMin = Vec3f(mWallThickness, mWallThickness, mWallThickness);
	obj->mMax = Vec3f(
			mVoxelSize*dims[0]- mWallThickness,
			0.06*mVoxelSize*dims[1],
			mVoxelSize*dims[2]- mWallThickness);
	obj->mVoxelSize=mVoxelSize;

	addSimulationObject(static_cast<SimulationObject*>(obj));

}
bool DamBreakSimulation::step(){
	Clock::time_point t0 = Clock::now();
	bool ret=FluidSimulation::step();
	Clock::time_point t1 = Clock::now();
	mComputeTimeSeconds= 1E-6*std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
	mIsMeshDirty=true;
	return ret;
}
} /* namespace imagesci */
