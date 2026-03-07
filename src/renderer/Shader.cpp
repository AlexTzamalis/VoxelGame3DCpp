#include "renderer/Shader.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

Shader::Shader(const std::string& vertexPath, const std::string& fragmentPath) {
    std::string vertSrc = readFile(vertexPath);
    std::string fragSrc = readFile(fragmentPath);
    const char* vSrc = vertSrc.c_str();
    const char* fSrc = fragSrc.c_str();

    unsigned int vert = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vert, 1, &vSrc, nullptr);
    glCompileShader(vert);
    int success;
    glGetShaderiv(vert, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(vert, sizeof(log), nullptr, log);
        std::cerr << "Vertex shader compile error: " << log << std::endl;
        glDeleteShader(vert);
        return;
    }

    unsigned int frag = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(frag, 1, &fSrc, nullptr);
    glCompileShader(frag);
    glGetShaderiv(frag, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(frag, sizeof(log), nullptr, log);
        std::cerr << "Fragment shader compile error: " << log << std::endl;
        glDeleteShader(vert);
        glDeleteShader(frag);
        return;
    }

    programId_ = glCreateProgram();
    glAttachShader(programId_, vert);
    glAttachShader(programId_, frag);
    glLinkProgram(programId_);
    glGetProgramiv(programId_, GL_LINK_STATUS, &success);
    if (!success) {
        char log[512];
        glGetProgramInfoLog(programId_, sizeof(log), nullptr, log);
        std::cerr << "Shader link error: " << log << std::endl;
    }
    glDeleteShader(vert);
    glDeleteShader(frag);
}

Shader::~Shader() {
    if (programId_)
        glDeleteProgram(programId_);
}

void Shader::use() const {
    glUseProgram(programId_);
}

void Shader::setMat4(const std::string& name, const glm::mat4& value) const {
    glUniformMatrix4fv(getUniformLocation(name), 1, GL_FALSE, &value[0][0]);
}

void Shader::setVec4(const std::string& name, const glm::vec4& value) const {
    glUniform4fv(getUniformLocation(name), 1, &value[0]);
}

void Shader::setVec3(const std::string& name, const glm::vec3& value) const {
    glUniform3fv(getUniformLocation(name), 1, &value[0]);
}

void Shader::setVec2(const std::string& name, const glm::vec2& value) const {
    glUniform2fv(getUniformLocation(name), 1, &value[0]);
}

void Shader::setInt(const std::string& name, int value) const {
    glUniform1i(getUniformLocation(name), value);
}

void Shader::setFloat(const std::string& name, float value) const {
    glUniform1f(getUniformLocation(name), value);
}

std::string Shader::readFile(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        std::cerr << "Cannot open file: " << path << std::endl;
        return {};
    }
    std::stringstream buf;
    buf << f.rdbuf();
    return buf.str();
}

int Shader::getUniformLocation(const std::string& name) const {
    auto it = uniformCache_.find(name);
    if (it != uniformCache_.end())
        return it->second;
    int loc = glGetUniformLocation(programId_, name.c_str());
    uniformCache_[name] = loc;
    return loc;
}
