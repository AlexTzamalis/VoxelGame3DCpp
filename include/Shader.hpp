#pragma once

#include <string>
#include <unordered_map>
#include <glm/glm.hpp>

class Shader {
public:
    Shader() = default;
    Shader(const std::string& vertexPath, const std::string& fragmentPath);
    ~Shader();

    void use() const;
    void setMat4(const std::string& name, const glm::mat4& value) const;
    void setVec3(const std::string& name, const glm::vec3& value) const;
    void setVec2(const std::string& name, const glm::vec2& value) const;
    void setInt(const std::string& name, int value) const;
    void setFloat(const std::string& name, float value) const;

    unsigned int id() const { return programId_; }

private:
    static std::string readFile(const std::string& path);
    int getUniformLocation(const std::string& name) const;

    unsigned int programId_ = 0;
    mutable std::unordered_map<std::string, int> uniformCache_;
};
