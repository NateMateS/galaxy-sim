#pragma once
#include <glad/glad.h>
#include <memory>
#include <vector>
#include <glm/glm.hpp>
#include "Shader.h"

struct BloomMip {
    unsigned int texture;
    glm::ivec2 size;
};

class PostProcessor {
public:
    static constexpr float LOW_RES_SCALE = 0.25f;

    unsigned int Width, Height;

    // Framebuffers
    unsigned int MSAAFBO; // Multisampled FBO
    unsigned int MSAATexture; // Multisampled Color Buffer
    unsigned int MSAADepthTexture; // Multisampled Depth Buffer

    // Depth Copy for Soft Particles (Breaks Feedback Loop)
    unsigned int DepthCopyFBO;
    unsigned int MSAADepthCopyTexture;
    unsigned int MSAADummyColorTexture; // Required to make FBO Complete on some drivers

    // Low-Resolution Gas Rendering (Quarter Resolution)
    // Separated FBOs to avoid feedback loops
    unsigned int LowResGasFBO;   // For drawing gas (Color Att 0: LowResGasTexture)
    unsigned int LowResDepthFBO; // For downsampling depth (Color Att 0: LowResDepthTexture)

    unsigned int LowResGasTexture; // RGB16F
    unsigned int LowResDepthTexture; // R32F (Linear Depth)

    unsigned int IntermediateFBO; // Intermediate FBO for resolving MSAA
    unsigned int ScreenTexture; // Texture attachment for Intermediate FBO
    unsigned int DepthTexture; // Resolved Depth Texture

    // Bloom Mip Chain
    unsigned int MipChainFBO;
    std::vector<BloomMip> mipChain;

    std::unique_ptr<Shader> postShader;
    std::unique_ptr<Shader> downsampleShader;
    std::unique_ptr<Shader> upsampleShader;
    std::unique_ptr<Shader> gasCompositeShader;
    std::unique_ptr<Shader> depthDownsampleShader;

    unsigned int QuadVAO = 0;
    unsigned int QuadVBO;

    PostProcessor(unsigned int width, unsigned int height);
    ~PostProcessor();

    // Prevent copying to avoid double-free of OpenGL resources
    PostProcessor(const PostProcessor&) = delete;
    PostProcessor& operator=(const PostProcessor&) = delete;

    void BeginRender();
    void EndRender();

    // Quarter-Resolution Gas Pass
    void PrepareGasPass(); // Downsamples depth
    void BeginGasPass();
    void EndGasPass();

    void Resize(unsigned int width, unsigned int height);

    // Copies the multisampled depth buffer to a second texture to allow reading while writing
    void CopyDepth();
    unsigned int GetDepthTexture() const { return DepthTexture; }

private:
    void InitRenderData();
    void InitFramebuffers();
    void InitBloomMips();
};
