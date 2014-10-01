#version 330
layout (triangles) in;
layout (triangle_strip, max_vertices=3) out;
out vec3 v0, v1, v2;
out vec3 normal, vert;
uniform mat4 P,V,M;
void main() {
  mat4 PVM=P*V*M;
  mat4 VM=V*M;
  
  vec3 v01 = gl_in[1].gl_Position.xyz - gl_in[0].gl_Position.xyz;
  vec3 v02 = gl_in[2].gl_Position.xyz - gl_in[0].gl_Position.xyz;
  vec3 fn =  normalize(cross( v01, v02 ));
  
  v0 = (VM*gl_in[0].gl_Position).xyz;
  v1 = (VM*gl_in[1].gl_Position).xyz;
  v2 = (VM*gl_in[2].gl_Position).xyz;
  
  
  gl_Position=PVM*gl_in[0].gl_Position;  
  vert = v0;
  normal = (VM*vec4(fn,0.0)).xyz;
  EmitVertex();
  
  
  gl_Position=PVM*gl_in[1].gl_Position;  
  vert = v1;
  normal = (VM*vec4(fn,0.0)).xyz;
  EmitVertex();
  
   
  gl_Position=PVM*gl_in[2].gl_Position;  
  vert = v2;
  normal = (VM*vec4(fn,0.0)).xyz;
  EmitVertex();
  
  EndPrimitive();
 }