

dependencies
- wgpu
- glfw
- glm
- tiny_obj_loader



notes

CREATING TEXTURE DATA
0. create texture on the GPU/VRAM using TextureDescriptor 
1. create the data for the texture on the CPU
2. use queue.writeTexture(...) to upload the data from CPU->GPU texture
	- specify source & destination
VIEWING TEXTURE DATA: need to bind the texture to the render pipeline & access it from shader
0. Create layouts (binding entry layout & binding group layout) similar to uniforms
	- NOTE: binding layout specification is part of the render pipeline! tells the render pipeline what resources
	  it has
1. Create the binding itself (connect texture to the binding)
	- we must use a Texture View here!
2. On the fragment shader side, access the texture & sample it
	- @group(...) @binding(...) var someTexture: texture_2d<f32>;
	- textureLoad(someTexture, vec2i(texelUV), 0);

UV coords notes
- READING OBJ: coordinate system 0-1 up for obj -> 0 to 1 down for webgpu
- IN SHADER:  UV (0-1) -> TexelUV (0 - texture's integer coords) before sampling



Qs
- what is VRAM?
- look at TODO notes
- make sure to release everything
- cpp initialization

@group(0) @binding(0) var<uniform> u_Uniforms: Uniforms;
@group(0) @binding(1) var gradientTexture: texture_2d<f32>;
what is var<uniform> why not for texture?

tex size and uv and 0 to 1 on the window surface???
if tex size and screen size is different where would 0.5 map to on the texture???

It is important that the conversion to integers (vec2i) is
done in the fragment shader rather than in the vertex shader, 
because integer vertex output does not get interpolated by the rasterizer. 
- what happens then?