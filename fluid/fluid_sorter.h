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
 *  This implementation of a PIC/FLIP fluid simulator is derived from:
 *
 *  Ando, R., Thurey, N., & Tsuruno, R. (2012). Preserving fluid sheets with adaptively sampled anisotropic particles.
 *  Visualization and Computer Graphics, IEEE Transactions on, 18(8), 1202-1214.
 */
#include "fluid_common.h"
#include "../ImageSciUtil.h"
#include <vector>
#ifndef _SORTER_H
#define _SORTER_H
namespace imagesci{
namespace fluid{
class ParticleLocator {
public:
	ParticleLocator(openvdb::Coord dims,float voxelSize);
	~ParticleLocator();
	
	void update( std::vector<ParticlePtr>& particles );
	std::vector<FluidParticle*> getNeigboringWallParticles( int i, int j, int k, int w=1, int h=1, int d=1 );
	std::vector<FluidParticle*> getNeigboringCellParticles( int i, int j, int k, int w=1, int h=1, int d=1 );
	float getLevelSetValue( int i, int j, int k, RegularGrid<float>& halfwall, float density );
	const openvdb::Coord& getGridSize(){ return mGridSize; }
	float getVoxelSize(){return mVoxelSize;}
	int	 getParticleCount( int i, int j, int k );
	void markAsWater(RegularGrid<char>& A, RegularGrid<float>& halfwall, float density );
	void deleteAllParticles();
	
protected:
	openvdb::Coord mGridSize;
	float mVoxelSize;
	RegularGrid<std::vector<FluidParticle*> > cells;
};
}}
#endif
