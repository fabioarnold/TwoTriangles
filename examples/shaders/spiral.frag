uniform vec2 u_resolution;
uniform float u_time;
uniform int u_stripe_count;
uniform float u_speed;
uniform float u_curl;
uniform float u_blur;
uniform vec3 u_color0;
uniform vec3 u_color1;

void main() {
	float aspect_x = max(1.0, u_resolution.x / u_resolution.y);
	float aspect_y = max(1.0, u_resolution.y / u_resolution.x);
	vec2 p = 2.0 * gl_FragCoord.xy / min(u_resolution.x, u_resolution.y)
		- vec2(aspect_x, aspect_y);

	float distSqr = 0.25*dot(p, p);
	float vignette = 1.0 - distSqr;
	float angle = atan(p.y, p.x);
	float stripes = smoothstep(-u_blur, u_blur,
		sin(float(u_stripe_count)*angle + u_speed*u_time - u_curl*sqrt(distSqr)));
	gl_FragColor = vec4(vignette * mix(u_color0, u_color1, stripes), 1.0);
}
