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

struct VertexInput {
    @location(0) position: vec3f,
    @location(1) color: vec3f,
    @location(2) normal: vec3f,
    @location(3) uv : vec2f
};
struct VertexOutput {
    @builtin(position) position: vec4f,
    @location(0) color: vec3f,
    @location(1) normal: vec3f,
    @location(2) uv: vec2f
};
struct Uniforms {
    // match order, type, and memory layout
    // wgsl -> C++ utility: 
    // https://eliemichel.github.io/WebGPU-AutoLayout/
    projMatrix: mat4x4f,
    viewMatrix: mat4x4f,
    modelMatrix: mat4x4f,
    time: f32
}

@group(0) @binding(0) var<uniform> u_Uniforms: Uniforms;
@group(0) @binding(1) var gradientTexture: texture_2d<f32>;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    var o : VertexOutput;

    let AR =  640.0 / 480.0;
    o.position = vec4f(in.position, 1.0);
    
    var mvp : mat4x4<f32> = u_Uniforms.projMatrix * u_Uniforms.viewMatrix * u_Uniforms.modelMatrix;
    o.position = mvp * o.position;
    o.color = in.color;
    o.normal = (u_Uniforms.modelMatrix * vec4(in.normal, 1.0)).xyz;
    o.uv = in.uv;
    return o;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    // for visual debugging
    // var nor : vec3f = in.normal * 0.5 + 0.5;

    let lightDirection = vec3f(0.5, 0.9, 0.1);
    let shading = dot(lightDirection, in.normal);
    var color = vec3(0.5) * shading + vec3(0.5, 0.5, 0.1);
    let texelUV = vec2i(in.uv * vec2f(textureDimensions(gradientTexture))); // 0-1 -> 0-texture size
    color = textureLoad(gradientTexture, texelUV, 0).rgb;
	return vec4f(color, 1.0);
}   