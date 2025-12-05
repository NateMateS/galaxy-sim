#pragma once

#include <glad/glad.h>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

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
    void setVec3(const std::string& name, float x, float y, float z) const;
    void setMat4(const std::string& name, const float* value) const;

private:
    void checkCompileErrors(unsigned int shader, std::string type);
    void compile(const char* vShaderCode, const char* fShaderCode);
};
