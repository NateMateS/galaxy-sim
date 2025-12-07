#include "PostProcessor.h"
#include <iostream>

PostProcessor::PostProcessor(unsigned int width, unsigned int height)
    : Width(width), Height(height) {
    std::cout << "PostProcessor: Constructor" << std::endl;

    // Load shaders
    std::cout << "PostProcessor: Loading Shaders..." << std::endl;
    postShader = std::make_unique<Shader>("assets/shaders/post.vert", "assets/shaders/post.frag");
    downsampleShader = std::make_unique<Shader>("assets/shaders/post.vert", "assets/shaders/downsample.frag");
    upsampleShader = std::make_unique<Shader>("assets/shaders/post.vert", "assets/shaders/upsample.frag");

    postShader->use();
    postShader->setInt("scene", 0);
    postShader->setInt("bloomBlur", 1);

    downsampleShader->use();
    downsampleShader->setInt("srcTexture", 0);

    upsampleShader->use();
    upsampleShader->setInt("srcTexture", 0);

    std::cout << "PostProcessor: InitFramebuffers..." << std::endl;
    InitFramebuffers();
    std::cout << "PostProcessor: InitRenderData..." << std::endl;
    InitRenderData();
    std::cout << "PostProcessor: Initialized." << std::endl;
}

PostProcessor::~PostProcessor() {
    glDeleteFramebuffers(1, &MSAAFBO);
    glDeleteTextures(1, &MSAATexture);
    glDeleteTextures(1, &MSAADepthTexture);

    glDeleteFramebuffers(1, &DepthCopyFBO);
    glDeleteTextures(1, &MSAADepthCopyTexture);
    glDeleteTextures(1, &MSAADummyColorTexture);

    glDeleteFramebuffers(1, &IntermediateFBO);
    glDeleteTextures(1, &ScreenTexture);
    glDeleteTextures(1, &DepthTexture);

    glDeleteFramebuffers(1, &MipChainFBO);
    for (auto& mip : mipChain) {
        glDeleteTextures(1, &mip.texture);
    }
    mipChain.clear();

    glDeleteVertexArrays(1, &QuadVAO);
    glDeleteBuffers(1, &QuadVBO);
}

void PostProcessor::InitFramebuffers() {
    std::cout << "PostProcessor: Generatng MSAA FBO..." << std::endl;
    // 1. Multisampled FBO
    glGenFramebuffers(1, &MSAAFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, MSAAFBO);

    glGenTextures(1, &MSAATexture);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, MSAATexture);
    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 4, GL_RGBA16F, Width, Height, GL_TRUE); // HDR & MSAA
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, MSAATexture, 0);

    glGenTextures(1, &MSAADepthTexture);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, MSAADepthTexture);
    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 4, GL_DEPTH_COMPONENT24, Width, Height, GL_TRUE);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D_MULTISAMPLE, MSAADepthTexture, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "ERROR::POSTPROCESSOR: MSAA Framebuffer not complete!" << std::endl;

    std::cout << "PostProcessor: Generatng Depth Copy FBO..." << std::endl;
    // 1b. Depth Copy FBO (MSAA) to break feedback loop
    glGenFramebuffers(1, &DepthCopyFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, DepthCopyFBO);

    glGenTextures(1, &MSAADepthCopyTexture);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, MSAADepthCopyTexture);
    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 4, GL_DEPTH_COMPONENT24, Width, Height, GL_TRUE);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D_MULTISAMPLE, MSAADepthCopyTexture, 0);

    // Dummy Color Texture to make FBO "Complete" on picky drivers
    glGenTextures(1, &MSAADummyColorTexture);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, MSAADummyColorTexture);
    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 4, GL_R8, Width, Height, GL_TRUE);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, MSAADummyColorTexture, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "ERROR::POSTPROCESSOR: Depth Copy Framebuffer not complete!" << std::endl;

    std::cout << "PostProcessor: Generatng Intermediate FBO..." << std::endl;
    // 2. Intermediate FBO (for resolving MSAA and HDR bloom extraction)
    glGenFramebuffers(1, &IntermediateFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, IntermediateFBO);

    glGenTextures(1, &ScreenTexture);
    glBindTexture(GL_TEXTURE_2D, ScreenTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, Width, Height, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ScreenTexture, 0);

    glGenTextures(1, &DepthTexture);
    glBindTexture(GL_TEXTURE_2D, DepthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, Width, Height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, DepthTexture, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "ERROR::POSTPROCESSOR: Intermediate Framebuffer not complete!" << std::endl;

    InitBloomMips();
}

void PostProcessor::InitBloomMips() {
    glGenFramebuffers(1, &MipChainFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, MipChainFBO);

    glm::vec2 mipSize((float)Width, (float)Height); // Start from Full Res
    glm::ivec2 mipIntSize = (glm::ivec2)mipSize;

    // Generate 6 mips (1/2, 1/4, 1/8, 1/16, 1/32, 1/64)
    for (int i = 0; i < 6; i++) {
        BloomMip mip;
        mipSize *= 0.5f;
        mipIntSize /= 2;

        if (mipIntSize.x < 2 || mipIntSize.y < 2) break;

        mip.size = mipIntSize;

        glGenTextures(1, &mip.texture);
        glBindTexture(GL_TEXTURE_2D, mip.texture);

        // Use R11G11B10F for better performance (half memory bandwidth of RGBA16F)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R11F_G11F_B10F, mipIntSize.x, mipIntSize.y, 0, GL_RGB, GL_FLOAT, NULL);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        mipChain.push_back(mip);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void PostProcessor::InitRenderData() {
    float quadVertices[] = {
        // positions   // texCoords
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,

        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f
    };
    glGenVertexArrays(1, &QuadVAO);
    glGenBuffers(1, &QuadVBO);
    glBindVertexArray(QuadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, QuadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
}

void PostProcessor::BeginRender() {
    glBindFramebuffer(GL_FRAMEBUFFER, MSAAFBO);
    glViewport(0, 0, Width, Height);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
}

void PostProcessor::EndRender() {
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST); // Ensure we draw to the full FBO

    // 1. Resolve MSAA to Intermediate FBO (Full Res)
    glBindFramebuffer(GL_READ_FRAMEBUFFER, MSAAFBO);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, IntermediateFBO);
    glBlitFramebuffer(0, 0, Width, Height, 0, 0, Width, Height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    // 2. Dual-Filter Bloom Pass
    glBindFramebuffer(GL_FRAMEBUFFER, MipChainFBO);

    // DOWNSAMPLE PHASE
    downsampleShader->use();
    downsampleShader->setInt("srcTexture", 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, ScreenTexture); // Source for Mip 0

    for (size_t i = 0; i < mipChain.size(); i++) {
        const BloomMip& mip = mipChain[i];

        // Set viewport to target mip size
        glViewport(0, 0, mip.size.x, mip.size.y);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mip.texture, 0);

        // Resolution of source texture
        if (i == 0) {
            downsampleShader->setVec2("srcResolution", (float)Width, (float)Height);
        } else {
            downsampleShader->setVec2("srcResolution", (float)mipChain[i-1].size.x, (float)mipChain[i-1].size.y);
        }

        glBindVertexArray(QuadVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // Prepare for next iteration: Current mip becomes source
        glBindTexture(GL_TEXTURE_2D, mip.texture);
    }

    // UPSAMPLE PHASE
    upsampleShader->use();
    upsampleShader->setInt("srcTexture", 0);
    upsampleShader->setFloat("filterRadius", 0.005f);

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE); // Additive Blending to accumulate bloom

    for (int i = (int)mipChain.size() - 1; i > 0; i--) {
        const BloomMip& mip = mipChain[i];
        const BloomMip& nextMip = mipChain[i-1];

        // Source: Current small mip
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, mip.texture);

        // Target: Next larger mip
        glViewport(0, 0, nextMip.size.x, nextMip.size.y);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, nextMip.texture, 0);

        glBindVertexArray(QuadVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    glDisable(GL_BLEND);

    // 3. Render to Screen (Composite)
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, Width, Height); // Restore Full Viewport

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    postShader->use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, ScreenTexture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, mipChain[0].texture); // Result of bloom is in Mip 0
    postShader->setInt("bloom", true);
    postShader->setFloat("exposure", 0.5f); // Exposure level

    glBindVertexArray(QuadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void PostProcessor::Resize(unsigned int width, unsigned int height) {
    Width = width;
    Height = height;

    // Re-create framebuffers
    glDeleteFramebuffers(1, &MSAAFBO);
    glDeleteTextures(1, &MSAATexture);
    glDeleteTextures(1, &MSAADepthTexture);

    glDeleteFramebuffers(1, &DepthCopyFBO);
    glDeleteTextures(1, &MSAADepthCopyTexture);
    glDeleteTextures(1, &MSAADummyColorTexture);

    glDeleteFramebuffers(1, &IntermediateFBO);
    glDeleteTextures(1, &ScreenTexture);
    glDeleteTextures(1, &DepthTexture);

    glDeleteFramebuffers(1, &MipChainFBO);
    for (auto& mip : mipChain) {
        glDeleteTextures(1, &mip.texture);
    }
    mipChain.clear();

    InitFramebuffers();
}

void PostProcessor::CopyDepth() {
    // Copy MSAA Depth -> MSAA Depth Copy
    glBindFramebuffer(GL_READ_FRAMEBUFFER, MSAAFBO);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, DepthCopyFBO);
    glBlitFramebuffer(0, 0, Width, Height, 0, 0, Width, Height, GL_DEPTH_BUFFER_BIT, GL_NEAREST);

    // Restore render target
    glBindFramebuffer(GL_FRAMEBUFFER, MSAAFBO);
}
