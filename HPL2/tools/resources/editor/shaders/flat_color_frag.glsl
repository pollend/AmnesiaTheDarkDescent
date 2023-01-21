@ifdef OS_OSX
	#version 120
@endif
////////////////////////////////////////////////////////
// Flat color - Fragment Shader
//
// A flat fragment shader - draws stuff using a given color
////////////////////////////////////////////////////////

uniform vec4 gvColor;

void main()
{
	gl_FragColor.xyz = gvColor.xyz;
	gl_FragColor.w = 0.5f;
}