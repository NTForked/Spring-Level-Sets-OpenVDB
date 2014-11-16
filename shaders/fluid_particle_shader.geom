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
#version 330
out vec2 uv;
layout (points) in;
layout (triangle_strip, max_vertices=4) out;
uniform mat4 P,V,M;
void main() {
  mat4 PVM=P*V*M;
  mat4 VM=V*M;
  vec4 pt=gl_in[0].gl_Position;
  float r=0.5f*pt.w;
  pt.w=1.0;
  vec4 v = VM*pt;
  r=length(VM*vec4(0,0,r,0));
  gl_Position=P*(v+vec4(-r,-r,0,0));  
  uv=vec2(-1.0,-1.0);
  EmitVertex();
  
  gl_Position=P*(v+vec4(+r,-r,0,0));  
  uv=vec2(1.0,-1.0);
  EmitVertex();
  
  gl_Position=P*(v+vec4(-r,+r,0,0));  
  uv=vec2(-1.0,1.0);
  EmitVertex();
  
  gl_Position=P*(v+vec4(+r,+r,0,0));  
  uv=vec2(1.0,1.0);
  EmitVertex();
  EndPrimitive();

 }
