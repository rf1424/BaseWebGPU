const PI: f32 = 3.141592653589793;
// Rotation around X-axis
fn rotateX(angle: f32) -> mat4x4<f32> {
    let c = cos(angle);
    let s = sin(angle);
    return mat4x4<f32>(
        1.0, 0.0, 0.0, 0.0,
        0.0, c,   -s,  0.0,
        0.0, s,    c,   0.0,
        0.0, 0.0, 0.0, 1.0
    );
}


@group(0) @binding(0) var<uniform> uTime: f32;

struct VertexInput {
    @location(0) position: vec3f,
    @location(1) color: vec3f
};
struct VertexOutput {
    @builtin(position) position: vec4f,
    @location(0) color: vec3f
};

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    var o : VertexOutput;

    let AR =  640.0 / 480.0;
    o.position = vec4f(in.position, 1.0);
    // o.position.x += sin(uTime) * 0.5;
    o.position = rotateX(uTime) * o.position;
    o.position.z = o.position.z * 0.5 + 0.5;
    // o.position.x /= AR;
    o.color = in.color;
    return o;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
	return vec4f(in.color, 1.0);
}   