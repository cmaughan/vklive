#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include <stdexcept>
#include <vector>

#include <SDL2/SDL.h>
#include <SDL2/SDL_metal.h>

#include <vklive/metal/metal_context.h>
#include <vklive/metal/metal_scene.h>

namespace
{

void release_obj(void*& object)
{
    if (object)
    {
        id releasedObject = CFBridgingRelease(object);
        releasedObject = nil;
        object = nullptr;
    }
}

template <typename T>
T bridge(void* object)
{
    return (__bridge T)object;
}

void retain_obj(void*& storage, id object)
{
    release_obj(storage);
    storage = object ? (__bridge_retained void*)object : nullptr;
}

} // namespace

namespace metal
{

void context_validate_drawable_size(MetalContext& ctx)
{
    if (!ctx.window || !ctx.metalLayer)
    {
        return;
    }

    int width = 0;
    int height = 0;
    SDL_Metal_GetDrawableSize(ctx.window, &width, &height);
    auto layer = bridge<CAMetalLayer*>(ctx.metalLayer);
    layer.drawableSize = CGSizeMake(width, height);
}

void context_init(MetalContext& ctx)
{
    ctx.metalView = SDL_Metal_CreateView(ctx.window);
    if (!ctx.metalView)
    {
        throw std::runtime_error(SDL_GetError());
    }

    auto device = MTLCreateSystemDefaultDevice();
    if (!device)
    {
        throw std::runtime_error("Metal is not available on this Mac");
    }

    auto layer = (__bridge CAMetalLayer*)SDL_Metal_GetLayer((SDL_MetalView)ctx.metalView);
    if (!layer)
    {
        throw std::runtime_error(SDL_GetError());
    }

    layer.device = device;
    layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    layer.framebufferOnly = NO;
    layer.opaque = YES;

    ctx.metalLayer = (__bridge void*)layer;
    retain_obj(ctx.device, device);
    retain_obj(ctx.commandQueue, [device newCommandQueue]);
    if (!ctx.commandQueue)
    {
        throw std::runtime_error("Could not create Metal command queue");
    }

    context_validate_drawable_size(ctx);
}

void context_wait_idle(MetalContext& ctx)
{
    auto frameCommandBuffer = bridge<id<MTLCommandBuffer>>(ctx.frameCommandBuffer);
    if (frameCommandBuffer && frameCommandBuffer.status != MTLCommandBufferStatusNotEnqueued)
    {
        [frameCommandBuffer waitUntilCompleted];
    }

    auto lastCommandBuffer = bridge<id<MTLCommandBuffer>>(ctx.lastCommandBuffer);
    if (lastCommandBuffer)
    {
        [lastCommandBuffer waitUntilCompleted];
    }
}

bool context_begin_frame(MetalContext& ctx)
{
    release_obj(ctx.currentDrawable);
    release_obj(ctx.frameCommandBuffer);

    auto lastCommandBuffer = bridge<id<MTLCommandBuffer>>(ctx.lastCommandBuffer);
    if (lastCommandBuffer && lastCommandBuffer.status >= MTLCommandBufferStatusCompleted)
    {
        release_obj(ctx.lastCommandBuffer);
    }

    context_validate_drawable_size(ctx);

    auto layer = bridge<CAMetalLayer*>(ctx.metalLayer);
    auto commandQueue = bridge<id<MTLCommandQueue>>(ctx.commandQueue);
    if (!layer || !commandQueue)
    {
        return false;
    }

    id<CAMetalDrawable> drawable = [layer nextDrawable];
    if (!drawable)
    {
        return false;
    }

    id<MTLCommandBuffer> commandBuffer = [commandQueue commandBuffer];
    if (!commandBuffer)
    {
        return false;
    }

    retain_obj(ctx.currentDrawable, drawable);
    retain_obj(ctx.frameCommandBuffer, commandBuffer);
    return true;
}

bool context_present(MetalContext& ctx)
{
    auto drawable = bridge<id<CAMetalDrawable>>(ctx.currentDrawable);
    auto commandBuffer = bridge<id<MTLCommandBuffer>>(ctx.frameCommandBuffer);
    if (!drawable || !commandBuffer)
    {
        release_obj(ctx.currentDrawable);
        release_obj(ctx.frameCommandBuffer);
        return false;
    }

    [commandBuffer presentDrawable:drawable];
    [commandBuffer commit];

    release_obj(ctx.lastCommandBuffer);
    ctx.lastCommandBuffer = ctx.frameCommandBuffer;
    ctx.frameCommandBuffer = nullptr;
    release_obj(ctx.currentDrawable);
    return true;
}

void context_destroy(MetalContext& ctx)
{
    context_wait_idle(ctx);
    std::vector<Scene*> scenes;
    {
        std::lock_guard<std::mutex> lock(ctx.metalSceneMutex);
        for (auto& sceneEntry : ctx.mapMetalScene)
        {
            scenes.push_back(sceneEntry.first);
        }
    }
    for (auto& scene : scenes)
    {
        if (scene)
        {
            metal_scene_destroy(ctx, *scene);
        }
    }
    release_obj(ctx.currentDrawable);
    release_obj(ctx.frameCommandBuffer);
    release_obj(ctx.lastCommandBuffer);
    release_obj(ctx.commandQueue);
    release_obj(ctx.device);

    if (ctx.metalView)
    {
        SDL_Metal_DestroyView((SDL_MetalView)ctx.metalView);
        ctx.metalView = nullptr;
    }
    ctx.metalLayer = nullptr;
}

} // namespace metal
