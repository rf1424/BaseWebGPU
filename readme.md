### Implementation Log

This is a personal project to build a rendering engine in WebGPU from scratch.
After taking a few computer graphics courses in OpenGL, I wanted to learn a more modern graphics API and 
implement graphics techniques I was interested in.

So far I have implemented physical based lighting with point lights, and
I plan to add:
- Environment maps/image based lighting
- Glass / transparent PBR material


##### BSDF / Physically based Rendering with Point Lights
<img src="readmeImages/sneakers.png" width="400"/>

<img src="readmeImages/dino.png" width="400"/>

<img src="readmeImages/r0.1m0.1.png" width="200"/> <img src="readmeImages/r0.1m0.9.png" width="200"/></br>
<img src="readmeImages/r0.9m0.1.png" width="200"/> <img src="readmeImages/r0.9m0.9.png" width="200"/>

##### Camera control with Mouse
##### Window Resizing

##### Tiny Obj Loading + Texturing
<img src="readmeImages/liddedEwer.png" width="400"/>

##### Basic Shading
<img src="readmeImages/normals.png" width="200"/> <img src="readmeImages/basicLighting.png" width="200"/>
##### 2D to 3D
<img src="readmeImages/rotate.gif" width="200"/>

##### Buffers and Uniforms

<img src="readmeImages/buffers.png" width="200"/>

##### Render Pipeline configuration
- Vertex pipeline, primitive pipeline, vertex/fragment shaders

 
<img src="readmeImages/firstTriangles.png" width="200"/>

##### Environment Setup
- Conigure with CMake, Visual Studio, and C++, Dawn.
- Set up adapter, device, command queue.
- Create a window with GLFW.
- Refactor with C++ Wrapper Configuration: https://github.com/eliemichel/WebGPU-Cpp 

##### Dependencies
- `wgpu`
- `glfw`
- `glm`
- `tiny_obj_loader`
- stb

##### References

- https://eliemichel.github.io/LearnWebGPU/index.html
- https://learnopengl.com/PBR/Lighting

##### Models and textures
- https://sketchfab.com/3d-models/lidded-ritual-ewer-guang-c2898500387d40678d26d15d12809608
- https://3d.si.edu/object/3d/triceratops-horridus-marsh-1889:d8c623be-4ebc-11ea-b77f-2e728ce88125
- https://3d.si.edu/object/3d/pair-blue-sneakers-worn-wellington-webb-while-campaigning:3ab80a3a-6099-48f7-9861-c454daa048ef

