#pragma once

#include <glad/glad.h>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <unordered_map>

class Shader {
public:
    unsigned int ID = 0;

    Shader(const char* vertexPath, const char* fragmentPath);
    Shader(std::string vertexCode, std::string fragmentCode, bool isCode = true);
    ~Shader();

    // Delete copy constructor and assignment to prevent double-free
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    void use();
    void setBool(const std::string& name, bool value) const;
    void setInt(const std::string& name, int value) const;
    void setFloat(const std::string& name, float value) const;
    void setVec2(const std::string& name, float x, float y) const;
    void setVec3(const std::string& name, float x, float y, float z) const;
    void setVec4(const std::string& name, float x, float y, float z, float w) const;
    void setMat4(const std::string& name, const float* value) const;

    // For UBOs
    void setUniformBlock(const std::string& name, unsigned int bindingPoint) const;

private:
    void checkCompileErrors(unsigned int shader, std::string type);
    void compile(const char* vShaderCode, const char* fShaderCode);
    int getUniformLocation(const std::string& name) const;
    mutable std::unordered_map<std::string, int> uniformLocationCache;
};
