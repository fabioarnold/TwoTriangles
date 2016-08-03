uniform vec2 u_resolution;
uniform float u_time;
uniform samplerCube cubemap;

void main() {
	float aspect_ratio = u_resolution.x / u_resolution.y;
	
	// rotation around y axis
	mat3 rotation = mat3(
		 cos(u_time), 0.0, sin(u_time),
		         0.0, 1.0,         0.0,
		-sin(u_time), 0.0, cos(u_time)
	);
	
	// normalized device coordinates
	vec2 ndc = 2.0 * gl_FragCoord.xy / u_resolution.xy - 1.0;
	vec2 uv = ndc * vec2(aspect_ratio, 1.0);
	
	// camera
	vec3 rd = normalize(vec3(0.7 * uv, 1.0));
	rd = rotation * rd;
	
	float sphereRadius = 0.7;
	if (dot(uv, uv) < sphereRadius * sphereRadius) {
		vec3 sphereNormal = vec3(uv / sphereRadius, 0.0);
		sphereNormal.z = sqrt(1.0 - sphereNormal.x * sphereNormal.x - sphereNormal.y * sphereNormal.y);
		gl_FragColor = textureCube(cubemap, rotation * reflect(sphereNormal, vec3(0.7 * uv, -1.0)));
	} else
		gl_FragColor = textureCube(cubemap, rd);
}
