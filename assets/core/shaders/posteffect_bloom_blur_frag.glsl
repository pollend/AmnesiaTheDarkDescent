uniform sampler2D unit0;
@define sampler_unit0 0

void main() {
	gl_Color = texture2D(unit0, gl_TexCoord[0]);
}

