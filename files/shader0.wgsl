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
    @location(2) uv: vec2f,
    @location(3) worldPos: vec3f
};
struct Uniforms {
    // PADDING: match order, type, and memory layout
    // wgsl -> C++ utility: 
    // https://eliemichel.github.io/WebGPU-AutoLayout/
    projMatrix: mat4x4f,
    viewMatrix: mat4x4f,
    modelMatrix: mat4x4f,
    modelInvTranspose: mat4x4f,
    time: f32,
    cameraPos : vec3f
}

@group(0) @binding(0) var<uniform> u_Uniforms: Uniforms;
@group(0) @binding(1) var objTexture: texture_2d<f32>;
@group(0) @binding(2) var textureSampler : sampler;
@group(0) @binding(3) var cubemapTexture : texture_cube<f32>;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    var o : VertexOutput;
    o.position = vec4f(in.position, 1.0);
    
    var mvp : mat4x4<f32> = u_Uniforms.projMatrix * u_Uniforms.viewMatrix * u_Uniforms.modelMatrix;
    o.position = mvp * o.position;
    o.color = in.color;
    o.normal = normalize((u_Uniforms.modelInvTranspose * vec4(in.normal, 0.0)).xyz);
    o.uv = in.uv;
    o.worldPos = (u_Uniforms.modelMatrix * vec4(in.position, 1.0)).xyz;
    return o;
}

// cosTheta: viewing angle, R: base color
fn fresnelSchlick(cosTheta: f32, R: vec3f) -> vec3f {
    return R + (vec3f(1.0) - R) * pow(1.0 - cosTheta, 5.0);
}

// probability wh aligns with microfacets
fn distributionMicrofacet(nor: vec3f, wh: vec3f, roughness: f32) -> f32 {
    let a: f32 = roughness * roughness;
    let a2: f32 = a * a;
    let nh: f32 = dot(nor, wh);
    let D: f32 = nh * nh * (a2 - 1.0) + 1.0;
    return a2 / (PI * D * D);
}

fn geometricOcclusion(wo: vec3f, wi: vec3f, nor: vec3f, roughness: f32) -> f32 {
    let k: f32 = pow(roughness + 1.0, 2.0) / 8.0;
    let ndotwi: f32 = max(dot(nor, wi), 0.0);
    let ndotwo: f32 = max(dot(nor, wo), 0.0);
    let G_wi: f32 = ndotwi / max(ndotwi * (1.0 - k) + k, 0.0001);
    let G_wo: f32 = ndotwo / max(ndotwo * (1.0 - k) + k, 0.0001);
    // G_smith
    return G_wo * G_wi; 
}

 // RENDERING LIGHT TRANSPORT EQUATION
    // Lo(p, wo) = integral { f(p, wi, wo) * Li(p, wi) * ndotwi } dwi -> approximate with sums
    // where f = (k_d * f_lambert) + (k_s * f_cooktorrance)
    //         = (k_d * c / PI) + (k_s * DFG / (4 * wo dot n * wi dot n))
fn computeLo(worldPos : vec3f, nor: vec3f, wo: vec3f, baseCol : vec3f, lightPos : vec3f) -> vec3f {

    // temp
    let attenuation : f32 = 1. /  dot(lightPos - worldPos, lightPos - worldPos); // TODO temp
    let lightCol : vec3f = vec3f(1., 1., 1.);
    let roughness : f32 = 0.5;
    let metallicness : f32 = 0.5;
    let ambientOcclusion : f32= 1.0;

    let wi : vec3f = normalize(lightPos - worldPos);
    // half vector
    let wh : vec3f = normalize(wo + wi);

    // 0. radiance Li(p, wi) energy wavelength, in terms of rgb
    let Li : vec3f = lightCol * attenuation;

    // 1. ndotwi
    let cosTheta : f32 = max(dot(nor, wi), 0.);

    // 2. f(p, wi, wo)
    var f_lambert : vec3f = baseCol / PI;

    let D : f32 = distributionMicrofacet(nor, wh, roughness);
    let F0 : vec3f = mix(vec3f(0.04, 0.04, 0.04), baseCol, metallicness); // TODO F0???
    let F : vec3f = fresnelSchlick(max(dot(wh, wo), 0.0f), F0);
    let G : f32 = geometricOcclusion(wo, wi, nor, roughness);
    var f_cooktorrance : vec3f = D * F * G / (4. * max(0.0001, dot(wo, nor) * dot(wi, nor)));
    
    let k_s : vec3f = F;
    var k_d : vec3f = 1. - k_s;
    k_d *= (vec3f(1.0) - metallicness); // TODO

    // 3. combine them all 0-2
    let f : vec3f = k_d * f_lambert + k_s * f_cooktorrance;
    var Lo : vec3f = f * Li * cosTheta;

    Lo += 0.03 * ambientOcclusion * baseCol;  // TODO
    return Lo;
}

fn gammaCorrect(rgb: vec3<f32>) -> vec3f {
    let sRGB: vec3<f32> = rgb / (rgb + 1.0);

    let gCorrected: vec3<f32> = pow(sRGB, vec3<f32>(1.0 / 2.2));
    return gCorrected;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    // simple shading
    let lightDirection = vec3f(1.0, 1.0, 1.0) * 0.6;//0.5, 0., 0.1);
    let shading = max(dot(lightDirection, in.normal), 0.);
    var color = vec3(0.5) * shading + vec3(0.5, 0.5, 0.1);

    // texturing
    let texelUV = vec2i(in.uv * vec2f(textureDimensions(objTexture))); // 0-1 -> 0-texture size
    color = textureSample(objTexture, textureSampler, in.uv).rgb;

    // flat
    color = vec3f(0., 0.5, 0.5);
    

    // pbr
    //let lightPos : vec3f = vec3f(1, 1, 1); // 
    let lightPos : vec3f = vec3f(0, 1, 1);     
    let lightPos2 : vec3f = vec3f(0, -1, 2);

    let wo : vec3f = normalize(u_Uniforms.cameraPos - in.worldPos);
    var test : vec3f = computeLo(in.worldPos, in.normal, wo, color, lightPos);

    test += computeLo(in.worldPos, in.normal, wo, color, lightPos2);
    
    test = gammaCorrect(test);

    // check spherical image map
    let reflectedDir = -reflect(wo, in.normal);
    let ibl_sample = textureSample(cubemapTexture, textureSampler, reflectedDir).rgb;

	return vec4f(ibl_sample,  1.0);
}   