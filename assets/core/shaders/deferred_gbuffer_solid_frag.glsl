uniform sampler2D unit0;
@define sampler_unit0 0

void main() {
	gl_FragData[0] = texture2D(unit0, gl_TexCoord[0].xy);
	gl_FragData[1] = vec4(1.0);
	gl_FragData[2] = vec4(1.0);
	gl_FragData[3] = vec4(1.0);
}

