#include "PostProcessor.h"
#include <iostream>

PostProcessor::PostProcessor(unsigned int width, unsigned int height)
    : Width(width), Height(height) {
    std::cout << "PostProcessor: Constructor" << std::endl;

    // Load shaders
    std::cout << "PostProcessor: Loading Shaders..." << std::endl;
    postShader = std::make_unique<Shader>("assets/shaders/post.vert", "assets/shaders/post.frag");
    blurShader = std::make_unique<Shader>("assets/shaders/post.vert", "assets/shaders/blur.frag");
    extractShader = std::make_unique<Shader>("assets/shaders/post.vert", "assets/shaders/extract.frag");

    postShader->use();
    postShader->setInt("scene", 0);
    postShader->setInt("bloomBlur", 1);

    blurShader->use();
    blurShader->setInt("image", 0);

    extractShader->use();
    extractShader->setInt("scene", 0);

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

    glDeleteFramebuffers(2, PingPongFBO);
    glDeleteTextures(2, PingPongColorbuffers);

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

    // 3. Ping-Pong Framebuffers for Blur (Downsampled to Half Resolution)
    unsigned int bloomWidth = Width / 2;
    unsigned int bloomHeight = Height / 2;

    glGenFramebuffers(2, PingPongFBO);
    glGenTextures(2, PingPongColorbuffers);
    for (unsigned int i = 0; i < 2; i++)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, PingPongFBO[i]);
        glBindTexture(GL_TEXTURE_2D, PingPongColorbuffers[i]);
        // Note: Using bloomWidth/bloomHeight here
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, bloomWidth, bloomHeight, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, PingPongColorbuffers[i], 0);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            std::cout << "ERROR::POSTPROCESSOR: PingPong Framebuffer not complete!" << std::endl;
    }
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

    // 2. Extraction & Downsampling (Full Res -> Half Res)
    unsigned int bloomWidth = Width / 2;
    unsigned int bloomHeight = Height / 2;

    // Bind the first ping-pong buffer (which is half-res)
    glBindFramebuffer(GL_FRAMEBUFFER, PingPongFBO[0]);
    glViewport(0, 0, bloomWidth, bloomHeight); // IMPORTANT: Set viewport for downsampling
    glClear(GL_COLOR_BUFFER_BIT);

    extractShader->use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, ScreenTexture); // Read from Full Res Resolved Scene
    glBindVertexArray(QuadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // 3. Gaussian Blur (Half Res)
    bool horizontal = true;
    unsigned int amount = 10; // More iterations for smoother glow

    blurShader->use();
    for (unsigned int i = 0; i < amount; i++)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, PingPongFBO[horizontal]);

        blurShader->setInt("horizontal", horizontal);
        glActiveTexture(GL_TEXTURE0);

        // Bind texture of other framebuffer (or extracted brightness for first iteration)
        glBindTexture(GL_TEXTURE_2D, PingPongColorbuffers[!horizontal]);

        glDrawArrays(GL_TRIANGLES, 0, 6);
        horizontal = !horizontal;
    }

    // 4. Render to Screen (Combine Full Res Scene + Half Res Blur)
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, Width, Height); // Restore Full Viewport

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    postShader->use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, ScreenTexture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, PingPongColorbuffers[!horizontal]); // Result of last blur
    postShader->setInt("bloom", true);
    postShader->setFloat("exposure", 0.5f); // Exposure level

    glBindVertexArray(QuadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void PostProcessor::Resize(unsigned int width, unsigned int height) {
    Width = width;
    Height = height;

    // Re-create framebuffers with new size
    glDeleteFramebuffers(1, &MSAAFBO);
    glDeleteTextures(1, &MSAATexture);
    glDeleteTextures(1, &MSAADepthTexture);

    glDeleteFramebuffers(1, &DepthCopyFBO);
    glDeleteTextures(1, &MSAADepthCopyTexture);
    glDeleteTextures(1, &MSAADummyColorTexture);

    glDeleteFramebuffers(1, &IntermediateFBO);
    glDeleteTextures(1, &ScreenTexture);
    glDeleteTextures(1, &DepthTexture);
    glDeleteFramebuffers(2, PingPongFBO);
    glDeleteTextures(2, PingPongColorbuffers);

    InitFramebuffers();
}

void PostProcessor::CopyDepth() {
    // Copy MSAA Depth -> MSAA Depth Copy
    // This is safe and supported (same format/samples)
    glBindFramebuffer(GL_READ_FRAMEBUFFER, MSAAFBO);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, DepthCopyFBO);
    glBlitFramebuffer(0, 0, Width, Height, 0, 0, Width, Height, GL_DEPTH_BUFFER_BIT, GL_NEAREST);

    // Restore render target
    glBindFramebuffer(GL_FRAMEBUFFER, MSAAFBO);
}
