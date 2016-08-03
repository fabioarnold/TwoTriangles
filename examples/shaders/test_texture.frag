uniform vec2 u_resolution;
uniform float u_time;
uniform sampler2D u_texture;

void main() {
	vec2 uv = gl_FragCoord.xy / u_resolution.xy;
	uv.y = -uv.y; // FragCoords are vertically inverse of window coords
	gl_FragColor = texture2D(u_texture, uv);
}
