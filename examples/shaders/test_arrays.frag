uniform vec2 u_resolution;
uniform vec3 u_colors[4];

void main() {
	vec2 p = gl_FragCoord.xy / u_resolution;

	vec3 colors01 = mix(u_colors[0], u_colors[1], p.x);
	vec3 colors23 = mix(u_colors[2], u_colors[3], p.x);

	gl_FragColor = vec4(mix(colors01, colors23, p.y), 1.0);
}
