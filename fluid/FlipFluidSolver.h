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

#ifndef FLIPFLUIDSOLVER_H_
#define FLIPFLUIDSOLVER_H_
#include <openvdb/openvdb.h>
#include <openvdb/tools/Dense.h>
#include <openvdb/tools/DenseSparseTools.h>
#include "fluid_common.h"
#include "fluid_sorter.h"
#undef OPENVDB_REQUIRE_VERSION_NAME


namespace imagesci {
namespace fluid{
/*
 *
 */
class FlipFluidSolver {

	protected:
		int pourTime = -1;
		openvdb::Vec2f pourPos;
		float pourRad;
		double max_dens;
		int step;
		int MAX_STEP;
		const static float ALPHA ;
		const static float DT   ;
		const static float DENSITY;
		const static float GRAVITY ;
		MACGrid<float> mVol;
		MACGrid<float> mVolSave;
		RegularGrid<char> mA;
		RegularGrid<float> mL;
		RegularGrid<float> div;
		RegularGrid<float> mPress;
		RegularGrid<openvdb::Vec3f> mWallNormal;
		RegularGrid<float> mHalfWall;
		RegularGrid<float> mLevelSet;
		int mGridSize;
		int gNumStuck;
		float WALL_THICK;
		std::unique_ptr<sorter> sort;
		std::vector<Object> objects;
		void save_grid();
		void subtract_grid();
		void placeObjects();
		void placeWalls();
		void damBreakTest();
		void computeDensity();
		void compute_wall_normal();
		void simulateStep();
		void advect_particle();
		void solve_picflip();
		void add_ExtForce();
		void pourWater(int limit);
		void extrapolate_velocity();
		void reposition(std::vector<int>& indices, std::vector<particlePtr> particles ) ;
		void pushParticle( double x, double y, double z, char type );
		void project();
		void createLevelSet();
		void enforce_boundary();
		inline char wall_chk( char a ) {
			return a == WALL ? 1.0 : -1.0;
		}

	public:
		std::vector<particlePtr> particles;
		FlipFluidSolver(int gridSize);
		void init();
		virtual ~FlipFluidSolver();
	};
}
} /* namespace imagesci */

#endif /* FLIPFLUIDSOLVER_H_ */
