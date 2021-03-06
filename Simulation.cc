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

#include "Simulation.h"
#include <boost/filesystem.hpp>
#include <openvdb/openvdb.h>
#include <sstream>
#include <fstream>
#include <ostream>
#include "json/JsonUtil.h"
namespace imagesci {
void ExecuteSimulation(Simulation* sim){
	try {
		sim->fireUpdateEvent();
		while(sim->step()){
			sim->fireUpdateEvent();
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
		}
		if(sim->isRunning())sim->stopRunning();
	} catch (imagesci::Exception& e) {
		std::cout << "ImageSci Error:: "<< e.what() << std::endl;
	} catch (openvdb::Exception& e) {
		std::cout << "OpenVDB Error:: "<< e.what() << std::endl;
	}
}
SimulationListener::~SimulationListener(){

}
Simulation::Simulation(const std::string& name,MotionScheme scheme):mPaused(false),mComputeTimeSeconds(0.0),mName(name),mMotionScheme(scheme),mIsInitialized(false),mIsMeshDirty(false),mRunning(false),mTimeStep(0),mSimulationDuration(0),mSimulationTime(0),mSimulationIteration(0) {
	// TODO Auto-generated constructor stub

}
bool Simulation::stash(const std::string& directory){
	SimulationTimeStepDescription simDesc=getDescription();
	SpringLevelSetDescription springlDesc;
	std::stringstream constFile,isoFile,signedFile,descFile,fluidFile,rawFile;
	constFile<< directory<<mName<<"_sls_" <<std::setw(8)<<std::setfill('0')<< mSimulationIteration << ".ply";
	isoFile<< directory<<mName<<"_iso_" <<std::setw(8)<<std::setfill('0')<< mSimulationIteration << ".ply";
	fluidFile<< directory<<mName<<"_fluid_" <<std::setw(8)<<std::setfill('0')<< mSimulationIteration << ".ply";
	signedFile<< directory<<mName<<"_signed_"<<std::setw(8)<<std::setfill('0')<< mSimulationIteration << ".vdb";
	//rawFile<< directory<<mName<<"_signed_"<<std::setw(8)<<std::setfill('0')<< mSimulationIteration;
	descFile<< directory<<mName<<"_"<<std::setw(8)<<std::setfill('0')<< mSimulationIteration << ".sim";

	springlDesc.mMetricValues["Elements"]=mSource.mConstellation.getNumSpringls();
	springlDesc.mMetricValues["Removed"]=mSource.getLastCleanCount();
	springlDesc.mMetricValues["Added"]=mSource.getLastFillCount();
	if(mSource.mConstellation.save(constFile.str())){
		springlDesc.mConstellationFile=constFile.str();
	}
	//WriteToRawFile(mSource.mSignedLevelSet,rawFile.str());
	try {
		openvdb::io::File file(signedFile.str());
		openvdb::GridPtrVec grids;
		//GridBase::Ptr ptr=mSource.mSignedLevelSet->copyGrid(CopyPolicy::CP_COPY);
		mSource.mSignedLevelSet->transform()=mSource.transform();
		grids.push_back(mSource.mSignedLevelSet);
		file.write(grids);
		mSource.mSignedLevelSet->setTransform(Transform::createLinearTransform(1.0));
		springlDesc.mSignedLevelSetFile=signedFile.str();
	}catch(openvdb::Exception& e){
		std::cout<<"OpenVDB: "<<e.what()<<std::endl;
	} catch(std::exception& e){
		std::cout<<e.what()<<std::endl;
	}
	if(mSource.mIsoSurface.save(isoFile.str())){
		springlDesc.mIsoSurfaceFile=isoFile.str();
	}
	if(mSource.mParticleVolume.save(fluidFile.str())){
		springlDesc.mParticleVolumeFile=fluidFile.str();
	}
	std::ofstream ofs;
	ofs.open(descFile.str(), std::ofstream::out);
	if (ofs.is_open()){
		std::cout << "Saving " << descFile.str() << " ... ";
		std::string output;
		Json::Value serializeRoot;
		Json::Value &root = serializeRoot["Simulation Record"];
		springlDesc.serialize(root);
		simDesc.serialize(root);
		Json::StyledWriter writer;
		ofs<<writer.write( serializeRoot );
		ofs.close();
		std::cout << "Done." << std::endl;
		return true;
	} else return false;
	return true;
}
bool Simulation::updateGL(){
	if(mIsMeshDirty){
		mSource.mParticleVolume.updateGL();
		mSource.mConstellation.updateGL();
		mSource.mIsoSurface.updateGL();
		mIsMeshDirty=false;
		return true;
	} else return false;
}
bool Simulation::setSource(const std::string& fileName){
	std::string ext = boost::filesystem::extension(
			boost::filesystem::path(fileName));
	if (ext == std::string(".ply")) {
		Mesh* mesh=new Mesh();
		if(!mesh->openMesh(fileName)){
			delete mesh;
			return false;
		}
		//Normalize mesh to lie within unit cube, centered at (0.5f,0.5f,0.5f)
		mesh->mapIntoBoundingBox(mesh->estimateVoxelSize());
		BBoxd bbox=mesh->updateBoundingBox();
		mSource.create(mesh);
		openvdb::math::Transform::Ptr trans=mSource.transformPtr();
		Vec3d extents=bbox.extents();
		double max_extent = std::max(extents[0], std::max(extents[1], extents[2]));
		double scale=1.0/max_extent;
		const float radius = 0.15f;
		const openvdb::Vec3f center(0.5f,0.5f,0.5f);
		Vec3s t=-0.5f*(bbox.min()+bbox.max());
		trans->postTranslate(t);
		trans->postScale(scale);
		trans->postTranslate(center);
		mIsMeshDirty=true;
		return true;
	} else if (ext == std::string(".vdb")) {
		openvdb::io::File file(fileName);
		file.open();
		openvdb::GridPtrVecPtr grids = file.getGrids();
		openvdb::GridPtrVec allGrids;
		allGrids.insert(allGrids.end(), grids->begin(), grids->end());
		GridBase::Ptr ptr = allGrids[0];
		Mesh* mesh = new Mesh();
		FloatGrid::Ptr mSignedLevelSet=boost::static_pointer_cast<FloatGrid>(ptr);
	    openvdb::math::Transform::Ptr trans=openvdb::math::Transform::createLinearTransform();
	    mSource.create(*mSignedLevelSet);
	    mSource.mIsoSurface.updateBoundingBox();
	    mSource.transform()=mSource.mSignedLevelSet->transform();
	    mSource.mSignedLevelSet->transform()=*trans;
	    mIsMeshDirty=true;
		return true;
	}
	return false;
}
SimulationTimeStepDescription Simulation::getDescription(){
	SimulationTimeStepDescription desc;
	desc.mSimulationDuration=mSimulationDuration;
	desc.mSimulationIteration=mSimulationIteration;
	desc.mSimulationTime=mSimulationTime;
	desc.mTimeStep=mTimeStep;
	desc.mSimulationName=mName;
	desc.mComputeTimeSeconds=mComputeTimeSeconds;
	return desc;
}
void Simulation::reset(){
	mSimulationTime=0;
	mSimulationIteration=0;
	mRunning=false;
	mPaused=false;
	if(mIsInitialized){
		cleanup();
		mIsInitialized=false;
	}
}
void Simulation::stopRunning(){
	mRunning=false;
	mPaused=false;
}
bool Simulation::stop(){
	if(mRunning)mPaused=true;
	mRunning=false;
	if(mSimulationThread.joinable()){
		mSimulationThread.join();
	} else {
		return false;
	}
	return true;
}
bool Simulation::start(){
	if(mPaused){
		mPaused=false;
		mRunning=true;
		mSimulationThread=std::thread(ExecuteSimulation,this);
	} else {
		stop();
		if(mIsInitialized)cleanup();
		mIsInitialized=false;
		if(!init()){
			return false;
		}
		mIsInitialized=true;
		mRunning=true;
		mSimulationThread=std::thread(ExecuteSimulation,this);
	}
	return true;
}
Simulation::~Simulation() {
	stop();
}
void SimulationTimeStepDescription::serialize(Json::Value& root_in)
{
	Json::Value &root = root_in["SimulationTimeStep"];
	root["Name"] = mSimulationName;
	root["MotionScheme"] = EncodeMotionScheme(mMotionScheme);
	root["Iteration"] = (int)mSimulationIteration;
	root["Time"] = mSimulationTime;
	root["TimeStep"] = mTimeStep;
	root["Duration"] = mSimulationDuration;
	root["ComputeTimeSeconds"]=mComputeTimeSeconds;

}
void SimulationTimeStepDescription::deserialize(Json::Value& root_in)
{
	Json::Value &root = root_in["SimulationTimeStep"];
	mSimulationName=root.get("Name","").asString();
	mMotionScheme=DecodeMotionScheme(root.get("MotionScheme","undefined").asString());
	mSimulationIteration=root.get("Iteration",0).asInt();
	mSimulationTime=root.get("Time",0.0).asDouble();
	mTimeStep=root.get("TimeStep",0.0).asDouble();
	mSimulationDuration=root.get("Duration",0.0).asDouble();
	mComputeTimeSeconds=root.get("ComputeTimeSeconds",0.0).asDouble();
}

bool SimulationTimeStepDescription::load(const std::string& file, SimulationTimeStepDescription* out){
	std::ifstream ifs;
	ifs.open(file, std::ifstream::in);
	if (ifs.is_open()){
		std::cout << "Reading " << file << " ... ";
		std::string str((std::istreambuf_iterator<char>(ifs)),
			std::istreambuf_iterator<char>());
		ifs.close();
		std::cout << "Done." << std::endl;
		return JsonUtil::Deserialize(out, str);
	}
	else {
		return false;
	}
}

bool SimulationTimeStepDescription::save(const std::string& file) {
	std::ofstream ofs;
	ofs.open(file, std::ofstream::out);
	if (ofs.is_open()){
		std::cout << "Saving " << file << " ... ";
		std::string output;
		bool ret = JsonUtil::Serialize(this, output);
		if (ret)ofs << output;
		ofs.close();
		std::cout << "Done." << std::endl;
		return ret;
	}
	return false;
}

} /* namespace imagesci */
