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
#version 330 core
uniform sampler2D colormapTexture;
in vec2 uv;
in float mag;
uniform float MIN_DEPTH;
uniform float MAX_DEPTH;
uniform mat4 P,V,M;
uniform float SCALE;
uniform float maxVelocity;
uniform float minVelocity;
uniform float colorMapValue;
void main(void) {
	float radius=length(uv);
	if(radius>1.0){
		discard;
	} else {
		vec4 lum=mix(vec4(0.8,0.8,0.8,1.0),vec4(0.3,0.3,0.3,1.0),radius);
		   if(maxVelocity>0.0&&maxVelocity-minVelocity>1E-6f){

    }
		if(maxVelocity>0.0&&maxVelocity-minVelocity>1E-6f){
      float hue=clamp(mag,0.0,1.0);
      vec4 colormap=texture2D(colormapTexture,vec2(((colorMapValue<0)?1.0-hue:hue),abs(colorMapValue)));
			gl_FragColor=lum*colormap;
		} else {
			gl_FragColor=lum;
		}
	}
}
