A simple vulkan rendering project that forked from vblanco20-1/vulkan-guide. Added simple GPU driven features, refer to the engine branch of the original repo.

This simple GPU-driven pipeline consists of the following passes:
- Compute Culling: Utilizes a compute shader to perform culling methods such as frustum culling and occlusion culling using HZB.
- Shadow Pass: Generates shadow map using a single DrawIndirect call.
- Color Pass: Renders the final shaded geometry for the main scene.
- HZB Generation: A compute pass that generates the HZB from the current frame's depth buffer, which will be used for occlusion culling in the next frame.
- UI Pass: Renders the user interface as a final overlay.

Renderdoc capture: ./ToyEngine_rdc.rar
