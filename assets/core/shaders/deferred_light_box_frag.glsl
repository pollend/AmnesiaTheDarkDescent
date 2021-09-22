#version 120
#extension GL_ARB_texture_rectangle : enable

uniform sampler2DRect unit0;
@define sampler_unit0 0

void main() {
	gl_FragColor = texture2DRect(unit0, gl_FragCoord.xy);
}

