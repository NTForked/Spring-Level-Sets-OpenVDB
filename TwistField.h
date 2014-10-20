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

#ifndef TWISTFIELD_H_
#define TWISTFIELD_H_
#include <openvdb/openvdb.h>

#undef OPENVDB_REQUIRE_VERSION_NAME
namespace imagesci {

template <typename ScalarT = float>
class TwistField
{
protected:
	openvdb::Mat4s mPose;
	openvdb::Mat4s mPoseInverse;
	ScalarT mTwistPosition;
public:
    typedef ScalarT             ScalarType;
    typedef openvdb::math::Vec3<ScalarT> VectorType;
   TwistField(void):mPose(openvdb::Mat4s::identity()),mPoseInverse(openvdb::Mat4s::identity()),mTwistPosition(0.0){

    }

    TwistField(const openvdb::Mat4s& pose,float twistPosition):mPose(pose),mPoseInverse(pose.inverse()),mTwistPosition(twistPosition){
    	mPoseInverse.setCol(3,openvdb::Vec4s(0.0,0.0,0.0,1.0));
    	std::cout<<"Forward Pose "<<mPose<<std::endl;
    	std::cout<<"Inverse Pose "<<mPoseInverse<<std::endl;
    }

    /// @return const reference to the identity transfrom between world and index space
    /// @note Use this method to determine if a client grid is
    /// aligned with the coordinate space of this velocity field
    openvdb::math::Transform transform() const { return openvdb::math::Transform(); }

    /// @return the velocity in world units, evaluated at the world
    /// position xyz and at the specified time
    inline VectorType operator()(const openvdb::Vec3d& pt, ScalarType time) const{
       openvdb::Vec3d lt=mPose*pt;
       openvdb::Vec3d vel(0.0);
       if(lt[1]>mTwistPosition){
    	  vel[0]=-lt[2];
    	  vel[1]=0.0;
    	  vel[2]=lt[0];
    	  vel=mPoseInverse*vel;
       }
       return vel;
    }
    /// @return the velocity at the coordinate space position ijk
    inline VectorType operator() (const openvdb::Coord& ijk, ScalarType time) const
    {
        return (*this)(ijk.asVec3d(), time);
    }
}; // end of EnrightField

} /* namespace imagesci */

#endif /* TWISTFIELD_H_ */
