uniform float u_time;
uniform vec2 u_resolution;
uniform mat4 u_view_to_world;

const vec3 LIGHT_DIR = normalize(vec3(1,2,-5));

// http://mercury.sexy/hg_sdf/
void pR(inout vec2 p, float a) {
	p = cos(a)*p + sin(a)*vec2(p.y, -p.x);
}

float sdScene(vec3 p) {
	float t = mod(0.5*u_time,1);
	t = abs(2*t-1);
	t = 2*t-t*t;

	p.z += 2;
	pR(p.xz, u_time);

	p.xy += 0.5+0.1*t;
	float t0 = max(max(max(abs(p.z)-0.04, -p.y), -p.x), p.x+p.y-1);
	p.xy -= 0.2*t;
	p = vec3(-p.x+1, -p.y + 1, p.z);
	float t1 = max(max(max(abs(p.z)-0.04, -p.y), -p.x), p.x+p.y-1);
	return min(t0, t1);
}

vec3 sceneNormal(vec3 p) {
	vec2 e = vec2(1e-3,0);
	return normalize(vec3(
		sdScene(p+e.xyy) - sdScene(p-e.xyy),
		sdScene(p+e.yxy) - sdScene(p-e.yxy),
		sdScene(p+e.yyx) - sdScene(p-e.yyx)));
}

void main() {
	vec2 uv = gl_FragCoord / u_resolution.xy;

	vec2 ndc = 2*uv-1;
	ndc.x *= u_resolution.x/u_resolution.y;
	vec3 ro = u_view_to_world[3].xyz;
	vec3 rd = mat3(u_view_to_world)*normalize(vec3(ndc,-1.3));

	vec3 col;

	float t = 0, d = 0;
	for (int i = 0; i < 80; i++) {
		d = sdScene(ro + t * rd);
		if (d<1e-4) break;
		if (t>10) break;
		t += 0.7 * d;
	}
	if (t > 9.999) discard;

	vec3 p = ro + t * rd;
	vec3 n = sceneNormal(p);

	vec3 diffuse = 0.5 + 0.5*dot(-LIGHT_DIR, n);

	col += diffuse;

	gl_FragColor = vec4(pow(col, vec3(1.0/2.2)), 1.0);
}
