#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

struct GlobalUniformsData {
    glm::mat4 view;         // 64 bytes
    glm::mat4 projection;   // 64 bytes
    glm::vec4 viewPosTime;  // 16 bytes (xyz=pos, w=time)
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
        data.viewPosTime = glm::vec4(camPos, time);

        glBindBuffer(GL_UNIFORM_BUFFER, UBO);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(GlobalUniformsData), &data);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }
};
