///////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2012-2013 DreamWorks Animation LLC
//
// All rights reserved. This software is distributed under the
// Mozilla Public License 2.0 ( http://www.mozilla.org/MPL/2.0/ )
//
// Redistributions of source code must retain the above copyright
// and license notice and the following restrictions and disclaimer.
//
// *     Neither the name of DreamWorks Animation nor the names of
// its contributors may be used to endorse or promote products derived
// from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// IN NO EVENT SHALL THE COPYRIGHT HOLDERS' AND CONTRIBUTORS' AGGREGATE
// LIABILITY FOR ALL CLAIMS REGARDLESS OF THEIR BASIS EXCEED US$250.00.
//
///////////////////////////////////////////////////////////////////////////

#include "EnrightSpringls.h"

#include <openvdb/util/Formats.h> // for formattedInt()
#include <stdio.h>
#include <stdlib.h>
#include <iomanip>
#include <openvdb/openvdb.h>

#include <tbb/mutex.h>
#include <iomanip> // for std::setprecision()
#include <iostream>
#include <sstream>
#include <vector>
#include <limits>
#include <boost/smart_ptr.hpp>
#if defined(__APPLE__) || defined(MACOSX)
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#else
#include <GL/gl.h>
#include <GL/glu.h>
#endif

#include <GL/glfw.h>

#include <chrono>
#include <thread>

namespace imagesci{
EnrightSpringls* viewer=NULL;

const float EnrightSpringls::dt=0.005f;
using namespace imagesci;
using namespace openvdb_viewer;
EnrightSpringls* EnrightSpringls::GetInstance(){
	if(viewer==NULL)viewer=new EnrightSpringls();
	return viewer;
}
void UpdateView(EnrightSpringls* v){
	while(v->update()){
		std::this_thread::yield();
		//std::this_thread::sleep_for(std::chrono::milliseconds());
	}
}


void
keyCB(int key, int action)
{
	if (viewer) viewer->keyCallback(key, action);
}


void
mouseButtonCB(int button, int action)
{
	if (viewer) viewer->mouseButtonCallback(button, action);
}


void
mousePosCB(int x, int y)
{
	if (viewer) viewer->mousePosCallback(x, y);
}


void
mouseWheelCB(int pos)
{
	if (viewer) viewer->mouseWheelCallback(pos);
}


void
windowSizeCB(int width, int height)
{
	if (viewer) viewer->windowSizeCallback(width, height);
}


void
windowRefreshCB()
{
	if (viewer) viewer->windowRefreshCallback();
}

using namespace openvdb;
using namespace openvdb::tools;

void EnrightSpringls::windowRefreshCallback(){
	setNeedsDisplay();
}

EnrightSpringls::EnrightSpringls()
    : mCamera(new LuxCamera())
    , mClipBox(new openvdb_viewer::ClipBox)
    , mWheelPos(0)
    , mShiftIsDown(false)
    , mCtrlIsDown(false)
    , mShowInfo(true)
	, meshDirty(false)
	, simTime(0.0f)
	, mUpdates(1)
	,simulationIteration(0)
	,playbackMode(false)
	, simulationRunning(false)
{
	renderBBox=BBoxd(Vec3s(-50,-50,-50),Vec3s(50,50,50));
	Pose.setIdentity();
}
void EnrightSpringls::start(){
	simTime=0.0f;
	simulationIteration=0;
	advect=boost::shared_ptr<AdvectT>(new AdvectT(springlGrid,field));
	advect->setTemporalScheme(SpringlTemporalIntegrationScheme::RK4b);
	simulationRunning=true;
	simThread=std::thread(UpdateView,this);
}
void EnrightSpringls::resume(){
	if(advect.get()==nullptr){
		simTime=0.0f;
		simulationIteration=0;
		advect=boost::shared_ptr<AdvectT>(new AdvectT(springlGrid,field));
		advect->setTemporalScheme(SpringlTemporalIntegrationScheme::RK4b);
	}
	simulationRunning=true;
	simThread=std::thread(UpdateView,this);
}
EnrightSpringls::~EnrightSpringls(){
	stop();
}
void EnrightSpringls::stop(){
	simulationRunning=false;
	if(simThread.joinable()){
		simThread.join();
	}
}
bool EnrightSpringls::openMesh(const std::string& fileName){
	Mesh* mesh=new Mesh();
	mesh->openMesh(fileName);
	std::cout<<"Opened mesh "<<mesh->vertexes.size()<<" "<<mesh->faces.size()<<" "<<mesh->quadIndexes.size()<<" "<<mesh->triIndexes.size()<<std::endl;
	if(mesh==NULL)return false;
	boost::shared_ptr<imagesci::Mesh> originalMesh=std::unique_ptr<Mesh>(mesh);
	originalMesh->mapIntoBoundingBox(originalMesh->EstimateVoxelSize());
    openvdb::math::Transform::Ptr trans=openvdb::math::Transform::createLinearTransform();
    springlGrid.create(mesh);
    mClipBox->set(*(springlGrid.signedLevelSet));
	BBoxd bbox=mClipBox->GetBBox();
	trans=springlGrid.signedLevelSet->transformPtr();
    Vec3d extents=bbox.extents();
	double max_extent = std::max(extents[0], std::max(extents[1], extents[2]));
	double scale=1.0/max_extent;
	const float radius = 0.15f;
    const openvdb::Vec3f center(0.35f,0.35f,0.35f);
	Vec3s t=-0.5f*(bbox.min()+bbox.max());
	trans=springlGrid.transformPtr();
	trans->postTranslate(t);
	trans->postScale(scale*2*radius);
	trans->postTranslate(center);
	meshDirty=true;
	setNeedsDisplay();
	rootFile=GetFileWithoutExtension(fileName);
    return true;
}
bool EnrightSpringls::openGrid(FloatGrid& signedLevelSet){
    openvdb::math::Transform::Ptr trans=openvdb::math::Transform::createLinearTransform();
    springlGrid.create(signedLevelSet);
    mClipBox->set(*(springlGrid.signedLevelSet));
	BBoxd bbox=mClipBox->GetBBox();
	rootFile="/home/blake/tmp/enright";
	meshDirty=true;
	setNeedsDisplay();
	return true;
}
bool EnrightSpringls::openGrid(const std::string& fileName){
	openvdb::io::File file(fileName);
	file.open();
	openvdb::GridPtrVecPtr grids = file.getGrids();
	openvdb::GridPtrVec allGrids;
	allGrids.insert(allGrids.end(), grids->begin(), grids->end());
	GridBase::Ptr ptr = allGrids[0];
	Mesh* mesh = new Mesh();
	FloatGrid::Ptr signedLevelSet=boost::static_pointer_cast<FloatGrid>(ptr);
    openvdb::math::Transform::Ptr trans=openvdb::math::Transform::createLinearTransform();
    springlGrid.create(*signedLevelSet);
    mClipBox->set(*(springlGrid.signedLevelSet));
	BBoxd bbox=mClipBox->GetBBox();
	trans=springlGrid.signedLevelSet->transformPtr();
    Vec3d extents=bbox.extents();
	double max_extent = std::max(extents[0], std::max(extents[1], extents[2]));
	double scale=1.0/max_extent;
	const float radius = 0.15f;
    const openvdb::Vec3f center(0.35f,0.35f,0.35f);
	Vec3s t=-0.5f*(bbox.min()+bbox.max());
	trans=springlGrid.transformPtr();
	trans->postTranslate(t);
	trans->postScale(scale*2*radius);
	trans->postTranslate(center);
	meshDirty=true;
	rootFile=GetFileWithoutExtension(fileName);
	setNeedsDisplay();
	return true;
}
bool EnrightSpringls::init(int width,int height){


    if (glfwInit() != GL_TRUE) {
        std::cout<<"GLFW Initialization Failed.";
        return false;
    }
    mGridName.clear();
    // Create window
    if (!glfwOpenWindow(width, height,  // Window size
                       8, 8, 8, 8,      // # of R,G,B, & A bits
                       32, 0,           // # of depth & stencil buffer bits
                       GLFW_WINDOW))    // Window mode
    {
        glfwTerminate();
        return false;
    }
    glfwSetWindowTitle(mProgName.c_str());
    glfwSwapBuffers();

    BitmapFont13::initialize();

    openvdb::BBoxd bbox=renderBBox;
    openvdb::Vec3d extents = bbox.extents();
    double max_extent = std::max(extents[0], std::max(extents[1], extents[2]));

    mCamera->setTarget(bbox.getCenter(), max_extent);
    mCamera->lookAtTarget();
    mCamera->setSpeed(/*zoom=*/0.1, /*strafe=*/0.002, /*tumbling=*/0.02);


    glfwSetKeyCallback(keyCB);
    glfwSetMouseButtonCallback(mouseButtonCB);
    glfwSetMousePosCallback(mousePosCB);
    glfwSetMouseWheelCallback(mouseWheelCB);
    glfwSetWindowSizeCallback(windowSizeCB);
    glfwSetWindowRefreshCallback(windowRefreshCB);
    glClearColor(0.0f,0.0f,0.0f,1.0f);
    glDepthFunc(GL_LESS);
    glEnable(GL_DEPTH_TEST);
    glPointSize(4);
    glLineWidth(2);
	const float ambient[]={0.2f,0.2f,0.2f,1.0f};
	const float diffuse[]={0.8f,0.8f,0.8f,1.0f};
	const float specular[]={0.9f,0.9f,0.9f,1.0f};
	const float position[]={0.3f,0.5f,1.0f,0.0f};
	glEnable(GL_POLYGON_SMOOTH);
	glEnable( GL_BLEND );
	glEnable(GL_NORMALIZE);
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
	glShadeModel(GL_SMOOTH);
	glEnable (GL_LINE_SMOOTH);
	glEnable( GL_COLOR_MATERIAL );
	glEnable(GL_LIGHT0);
	glEnable(GL_LIGHTING);
	glLightfv(GL_LIGHT0, GL_AMBIENT,(GLfloat*)&ambient);
	glLightfv(GL_LIGHT0, GL_SPECULAR,(GLfloat*)&specular);
	glLightfv(GL_LIGHT0, GL_DIFFUSE,(GLfloat*)&diffuse);
	glLightfv(GL_LIGHT0, GL_POSITION,(GLfloat*)&position);
	glMaterialf(GL_FRONT, GL_SHININESS, 5.0f);
    size_t frame = 0;
    double time = glfwGetTime();
    glfwSwapInterval(1);

    stash();
    do {
       if(meshDirty){
    	   meshLock.lock();
    	   try {
				springlGrid.isoSurface.updateGL();
				springlGrid.constellation.updateGL();
    	   } catch(std::exception* e){
    		   std::cerr<<"UpdateGL Error: "<<e->what()<<std::endl;
    	   }
			meshDirty=false;
			meshLock.unlock();
			render();
        } else {
    		if(needsDisplay())render();
    	}
        ++frame;
        double elapsed = glfwGetTime() - time;
        if (elapsed > 1.0) {
            time = glfwGetTime();
            setWindowTitle(double(frame) / elapsed);
            frame = 0;
        }
        // Swap front and back buffers
        glfwSwapBuffers();
    // exit if the esc key is pressed or the window is closed.
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
    } while (!glfwGetKey(GLFW_KEY_ESC) && glfwGetWindowParam(GLFW_OPENED));
    glfwTerminate();
    return true;
}
bool EnrightSpringls::openRecording(const std::string& dirName){
	isoSurfaceFiles.clear();
	constellationFiles.clear();
	signedDistanceFiles.clear();
	int n1=GetDirectoryListing(dirName,isoSurfaceFiles,"_iso",".ply");
	int n2=GetDirectoryListing(dirName,constellationFiles,"_sls",".ply");
	int n3=GetDirectoryListing(dirName,signedDistanceFiles,"",".vdb");
	if(!(n1==n2&&n2==n3)||n1==0)return false;
	playbackMode=true;
	openGrid(signedDistanceFiles[0]);
	Mesh c;
	c.openMesh(constellationFiles[0]);
	springlGrid.constellation.create(&c);
	springlGrid.isoSurface.openMesh(isoSurfaceFiles[0]);
	springlGrid.isoSurface.updateVertexNormals();
	springlGrid.constellation.updateVertexNormals();
	meshDirty=true;
	setNeedsDisplay();
	return true;
}
bool EnrightSpringls::update(){
	if(meshDirty){
		std::this_thread::sleep_for(std::chrono::milliseconds());
		return true;
	}

	if(playbackMode){
		if(simulationIteration>=constellationFiles.size()){
			simulationIteration=0;
			simTime=0;
		}
		Mesh c;
		c.openMesh(constellationFiles[simulationIteration]);
		springlGrid.constellation.create(&c);
		springlGrid.isoSurface.openMesh(isoSurfaceFiles[simulationIteration]);
		springlGrid.isoSurface.updateVertexNormals();
		springlGrid.constellation.updateVertexNormals();
		openvdb::io::File file(signedDistanceFiles[simulationIteration]);
		file.open();
		openvdb::GridPtrVecPtr grids =file.getGrids();
		openvdb::GridPtrVec allGrids;
		allGrids.insert(allGrids.end(), grids->begin(), grids->end());
		GridBase::Ptr ptr = allGrids[0];
		springlGrid.signedLevelSet=boost::static_pointer_cast<FloatGrid>(ptr);
	} else {
		advect->advect(simTime,simTime+dt);
	}
	stash();
	simTime+=dt;
	//openvdb::BBoxd bbox = worldSpaceBBox(springlGrid.signedLevelSet->transform(),springlGrid.signedLevelSet->evalActiveVoxelBoundingBox());
	//mClipBox->setBBox(bbox);
	meshDirty=true;
	setNeedsDisplay();
	simulationIteration++;
	return (simTime<=3.0f&&simulationRunning);
}
void EnrightSpringls::stash(){

	std::ostringstream ostr1,ostr2,ostr3,ostr4;
	ostr4 << rootFile <<std::setw(4)<<std::setfill('0')<< simulationIteration << ".lxs";
	mCamera->setMaterialFile("/home/blake/materials/white_chess.lbm2");
	if(playbackMode){
		mCamera->setGeometryFile(isoSurfaceFiles[simulationIteration],Pose);
	} else {
		ostr1 << rootFile<<"_sls" <<std::setw(4)<<std::setfill('0')<< simulationIteration << ".ply";
		springlGrid.constellation.save(ostr1.str());
		ostr2 <<  rootFile<<"_iso" <<std::setw(4)<<std::setfill('0')<< simulationIteration << ".ply";
		springlGrid.isoSurface.save(ostr2.str());
		ostr3 <<  rootFile<<std::setw(4)<<std::setfill('0')<< simulationIteration << ".vdb";
		mCamera->setGeometryFile(ostr2.str(),Pose);
		openvdb::io::File file(ostr3.str());
		openvdb::GridPtrVec grids;
		grids.push_back(springlGrid.signedLevelSet);
		file.write(grids);
	}
	mCamera->write(ostr4.str(),640,640);
}
void
EnrightSpringls::setWindowTitle(double fps)
{
    std::ostringstream ss;
    ss  << mProgName << ": "
        << (mGridName.empty() ? std::string("OpenVDB ") : mGridName)
        << std::setprecision(1) << std::fixed << fps << " fps";
    glfwSetWindowTitle(ss.str().c_str());
}


////////////////////////////////////////


void
EnrightSpringls::render()
{



    openvdb::BBoxd bbox=mClipBox->GetBBox();
    openvdb::Vec3d extents = bbox.extents();
    openvdb::Vec3d rextents=renderBBox.extents();

    /*
    double scale = std::max(rextents[0], std::max(rextents[1], rextents[2]))/std::max(extents[0], std::max(extents[1], extents[2]));
    Vec3s minPt=bbox.getCenter();
    Vec3s rminPt=renderBBox.getCenter();
*/

    double scale = std::max(rextents[0], std::max(rextents[1], rextents[2]))/std::max(extents[0], std::max(extents[1], extents[2]));
    Vec3s minPt=bbox.getCenter();
    Vec3s rminPt=renderBBox.getCenter();

    Pose.setIdentity();
    Pose.postTranslate(-minPt);
	Pose.postScale(Vec3s(scale,scale,scale));
	Pose.postTranslate(rminPt);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    int width,height;
    glfwGetWindowSize(&width, &height);
    mCamera->aim(0,0,width/2,height);
    glPushMatrix();
    glMultMatrixf(Pose.asPointer());
/*
    glTranslatef(rminPt[0],rminPt[1],rminPt[2]);
    glScalef(scale,scale,scale);
    glTranslatef(-minPt[0],-minPt[1],-minPt[2]);
*/

 //   mClipBox->render();
  //  mClipBox->enableClipping();
	glColor3f(0.8f,0.8f,0.8f);
	springlGrid.draw(false,true,false,false);
//	mClipBox->disableClipping();
    glPopMatrix();

    mCamera->aim(width/2,0,width/2,height);
    glPushMatrix();
    glMultMatrixf(Pose.asPointer());
    mClipBox->render();
 	glColor3f(0.8f,0.3f,0.3f);
	springlGrid.isoSurface.draw(false,false,false,false);
    glPopMatrix();
    //
    // Render text
    /*
        BitmapFont13::enableFontRendering();

        glColor3f (0.2, 0.2, 0.2);

        int width, height;
        glfwGetWindowSize(&width, &height);

        BitmapFont13::print(10, height - 13 - 10, mGridInfo);
        BitmapFont13::print(10, height - 13 - 30, mTransformInfo);
        BitmapFont13::print(10, height - 13 - 50, mTreeInfo);

        BitmapFont13::disableFontRendering();
        */
}





////////////////////////////////////////


void
EnrightSpringls::updateCutPlanes(int wheelPos)
{
    double speed = std::abs(mWheelPos - wheelPos);
    if (mWheelPos < wheelPos) mClipBox->update(speed);
    else mClipBox->update(-speed);
    setNeedsDisplay();
}


////////////////////////////////////////


void
EnrightSpringls::keyCallback(int key, int action)
{
    OPENVDB_START_THREADSAFE_STATIC_WRITE
    mCamera->keyCallback(key, action);
    const bool keyPress = glfwGetKey(key) == GLFW_PRESS;
    mShiftIsDown = glfwGetKey(GLFW_KEY_LSHIFT);
    mCtrlIsDown = glfwGetKey(GLFW_KEY_LCTRL);
    if(keyPress){
		if(key==' '){
			if(simulationRunning){
				stop();
			} else {
				resume();
			}
		}
    }
    switch (key) {
    case 'x': case 'X':
        mClipBox->activateXPlanes() = keyPress;
        break;
    case 'y': case 'Y':
        mClipBox->activateYPlanes() = keyPress;
        break;
    case 'z': case 'Z':
        mClipBox->activateZPlanes() = keyPress;
        break;
    }

    mClipBox->shiftIsDown() = mShiftIsDown;
    mClipBox->ctrlIsDown() = mCtrlIsDown;

    setNeedsDisplay();

    OPENVDB_FINISH_THREADSAFE_STATIC_WRITE
}


void
EnrightSpringls::mouseButtonCallback(int button, int action)
{
    mCamera->mouseButtonCallback(button, action);
    mClipBox->mouseButtonCallback(button, action);
    if (mCamera->needsDisplay()) setNeedsDisplay();
}


void
EnrightSpringls::mousePosCallback(int x, int y)
{
    bool handled = mClipBox->mousePosCallback(x, y);
    if (!handled) mCamera->mousePosCallback(x, y);
    if (mCamera->needsDisplay()) setNeedsDisplay();
}


void
EnrightSpringls::mouseWheelCallback(int pos)
{
    if (mClipBox->isActive()) {
        updateCutPlanes(pos);
    } else {
        mCamera->mouseWheelCallback(pos, mWheelPos);
        if (mCamera->needsDisplay()) setNeedsDisplay();
    }

    mWheelPos = pos;
}


void
EnrightSpringls::windowSizeCallback(int, int)
{
    setNeedsDisplay();
}

////////////////////////////////////////


bool
EnrightSpringls::needsDisplay()
{
    if (mUpdates < 2) {
        mUpdates += 1;
        return true;
    }
    return false;
}


void
EnrightSpringls::setNeedsDisplay()
{
    mUpdates = 0;
}
}