// dear imgui: Renderer Backend for Metal
// This needs to be used along with a Platform Backend (e.g. OSX)

// Implemented features:
//  [X] Renderer: User texture binding. Use 'MTLTexture' as texture identifier.
//  Read the FAQ about ImTextureID/ImTextureRef! [X] Renderer: Large meshes
//  support (64k+ vertices) even with 16-bit indices
//  (ImGuiBackendFlags_RendererHasVtxOffset). [X] Renderer: Texture updates
//  support for dynamic font atlas (ImGuiBackendFlags_RendererHasTextures). [X]
//  Renderer: Multi-viewport support (multiple windows). Enable with
//  'io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable'.

// You can use unmodified imgui_impl_* files in your project. See examples/
// folder for examples of using this. Prefer including the entire imgui/
// repository into your project (either as a copy or as a submodule), and only
// build the backends you need. Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/
// folder).
// - Introduction, links and more at the top of imgui.cpp

#pragma once

#include <imgui.h> // IMGUI_IMPL_API

#ifndef IMGUI_DISABLE

//-----------------------------------------------------------------------------
// ObjC API
//-----------------------------------------------------------------------------

#ifdef __OBJC__

@class MTLRenderPassDescriptor;
@protocol MTLDevice
, MTLCommandBuffer, MTLRenderCommandEncoder;

// Follow "Getting Started" link and check examples/ folder to learn about using
// backends!
IMGUI_IMPL_API bool ImGui_Impl_Mosaic_Metal_Init(id<MTLDevice> device);
IMGUI_IMPL_API void ImGui_Impl_Mosaic_Metal_Shutdown();
IMGUI_IMPL_API void
ImGui_Impl_Mosaic_Metal_NewFrame(MTLRenderPassDescriptor* renderPassDescriptor);
IMGUI_IMPL_API void ImGui_Impl_Mosaic_Metal_RenderDrawData(
    ImDrawData* drawData, id<MTLCommandBuffer> commandBuffer,
    id<MTLRenderCommandEncoder> commandEncoder);

// Called by Init/NewFrame/Shutdown
IMGUI_IMPL_API bool
ImGui_Impl_Mosaic_Metal_CreateDeviceObjects(id<MTLDevice> device);
IMGUI_IMPL_API void ImGui_Impl_Mosaic_Metal_DestroyDeviceObjects();

// (Advanced) Use e.g. if you need to precisely control the timing of texture
// updates (e.g. for staged rendering), by setting ImDrawData::Textures =
// nullptr to handle this manually.
IMGUI_IMPL_API void ImGui_Impl_Mosaic_Metal_UpdateTexture(ImTextureData* tex);

#endif

//-----------------------------------------------------------------------------

#endif // #ifndef IMGUI_DISABLE
