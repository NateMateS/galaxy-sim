#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

struct GlobalUniformsData {
    glm::mat4 view;         // 64 bytes (Offset 0)
    glm::mat4 projection;   // 64 bytes (Offset 64)
    glm::vec4 viewPos;      // 16 bytes (Offset 128) - w unused
    float time;             // 4 bytes  (Offset 144)
    float _pad[3];          // 12 bytes (Offset 148) -> Total 160 bytes
};

class GlobalUniformBuffer {
public:
    unsigned int UBO;
    const unsigned int BINDING_POINT = 0;

    GlobalUniformBuffer() {
        glGenBuffers(1, &UBO);
        glBindBuffer(GL_UNIFORM_BUFFER, UBO);
        glBufferData(GL_UNIFORM_BUFFER, sizeof(GlobalUniformsData), NULL, GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_UNIFORM_BUFFER, BINDING_POINT, UBO);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }

    ~GlobalUniformBuffer() {
        if (UBO != 0) {
            glDeleteBuffers(1, &UBO);
            UBO = 0;
        }
    }

    // Prevent copying
    GlobalUniformBuffer(const GlobalUniformBuffer&) = delete;
    GlobalUniformBuffer& operator=(const GlobalUniformBuffer&) = delete;

    void update(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& camPos, float time) {
        GlobalUniformsData data;
        data.view = view;
        data.projection = projection;
        data.viewPos = glm::vec4(camPos, 1.0f);
        data.time = time;

        glBindBuffer(GL_UNIFORM_BUFFER, UBO);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(GlobalUniformsData), &data);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }
};
