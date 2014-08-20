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

#ifndef SPRINGLS_VIEWER_VIEWER_HAS_BEEN_INCLUDED
#define SPRINGLS_VIEWER_VIEWER_HAS_BEEN_INCLUDED
#undef OPENVDB_REQUIRE_VERSION_NAME
#include <GL/glew.h>
#include <GL/glfw.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <openvdb/openvdb.h>
#include <openvdb/tools/LevelSetUtil.h>
#include <openvdb/tools/LevelSetSphere.h>
#include <openvdb/tools/LevelSetAdvect.h>
#include <openvdb/tools/LevelSetMeasure.h>
#include <openvdb/tools/LevelSetMorph.h>
#include <openvdb/tools/Morphology.h>
#include <openvdb/tools/PointAdvect.h>
#include <openvdb/tools/PointScatter.h>
#include <openvdb/tools/ValueTransformer.h>
#include <openvdb/tools/VectorTransformer.h>
#include <string>
#include <chrono>
#include <thread>
#include <mutex>
#include <memory>
#include "Camera.h"
#include "ClipBox.h"
#include "Font.h"
#include "Mesh.h"
#include "SpringlGrid.h"
#include "Constellation.h"
namespace imagesci {
typedef openvdb::tools::EnrightField<float> FieldT;
typedef openvdb::tools::LevelSetAdvection<openvdb::FloatGrid, FieldT> AdvectT;
class SpringlsViewer {
protected:
	static const float dt;
	float simTime;
	bool meshDirty;
	bool simulationRunning;
	std::mutex meshLock;
	int mUpdates;
	boost::shared_ptr<Mesh> originalMesh;
	SpringlGrid springlGrid;
	std::unique_ptr<Constellation> constellation;

	FieldT field;
	boost::shared_ptr<AdvectT> advect;
	std::thread simThread;

public:
	typedef std::unique_ptr<openvdb_viewer::Camera> CameraPtr;
	typedef std::unique_ptr<openvdb_viewer::ClipBox> ClipBoxPtr;

	SpringlsViewer();
	~SpringlsViewer();
	bool update();

	bool openMesh(const std::string& fileName);
	bool openGrid(const std::string& fileName);

	bool needsDisplay();
	void setNeedsDisplay();
	void setWindowTitle(double fps = 0.0);
	void render();
	void updateCutPlanes(int wheelPos);
	bool init(int width, int height);
	void keyCallback(int key, int action);
	void mouseButtonCallback(int button, int action);
	void mousePosCallback(int x, int y);
	void mouseWheelCallback(int pos);
	void windowSizeCallback(int width, int height);
	void windowRefreshCallback();
	void start();
	void stop();
private:
	CameraPtr mCamera;
	ClipBoxPtr mClipBox;
	std::string mGridName, mProgName, mGridInfo, mTransformInfo, mTreeInfo;
	int mWheelPos;
	bool mShiftIsDown, mCtrlIsDown, mShowInfo;
};
}
#endif