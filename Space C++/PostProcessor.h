#pragma once
#include <glad/glad.h>
#include <memory>
#include "Shader.h"

class PostProcessor {
public:
    unsigned int Width, Height;

    // Framebuffers
    unsigned int MSAAFBO; // Multisampled FBO
    unsigned int MSAATexture; // Multisampled Color Buffer
    unsigned int MSAADepthTexture; // Multisampled Depth Buffer

    // Depth Copy for Soft Particles (Breaks Feedback Loop)
    unsigned int DepthCopyFBO;
    unsigned int MSAADepthCopyTexture;
    unsigned int MSAADummyColorTexture; // Required to make FBO Complete on some drivers

    unsigned int IntermediateFBO; // Intermediate FBO for resolving MSAA
    unsigned int ScreenTexture; // Texture attachment for Intermediate FBO
    unsigned int DepthTexture; // Resolved Depth Texture

    unsigned int PingPongFBO[2]; // For Gaussian Blur
    unsigned int PingPongColorbuffers[2];

    std::unique_ptr<Shader> postShader;
    std::unique_ptr<Shader> blurShader;
    std::unique_ptr<Shader> extractShader;

    unsigned int QuadVAO = 0;
    unsigned int QuadVBO;

    PostProcessor(unsigned int width, unsigned int height);
    ~PostProcessor();

    // Prevent copying to avoid double-free of OpenGL resources
    PostProcessor(const PostProcessor&) = delete;
    PostProcessor& operator=(const PostProcessor&) = delete;

    void BeginRender();
    void EndRender();
    void Resize(unsigned int width, unsigned int height);

    // Copies the multisampled depth buffer to a second texture to allow reading while writing
    void CopyDepth();
    unsigned int GetDepthTexture() const { return DepthTexture; }

private:
    void InitRenderData();
    void InitFramebuffers();
};
