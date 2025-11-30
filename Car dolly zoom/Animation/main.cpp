#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <cmath>
#include <cstdlib>

// Third-Party Libraries
#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// Model Loader
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

// Configuration
const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;
const int MAX_POINT_LIGHTS = 10;
const char* CAR_MODEL_PATH = "bin\\Debug\\Porshe911CarreraGTS.obj";
const float CAR_SCALE_FACTOR = 1.5f;

// Multi-Light Phong Shader
const char* phongVertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
out vec3 FragPos;
out vec3 Normal;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
void main() {
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = mat3(transpose(inverse(model))) * aNormal;
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
)";
const char* phongFragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;
in vec3 FragPos;
in vec3 Normal;
struct PointLight {
    vec3 position;
    vec3 color;
    float constant;
    float linear;
    float quadratic;
};
uniform vec3 objectColor;
uniform vec3 viewPos;
uniform float ambientStrength;
uniform float specularStrength;
uniform int shininess;
uniform PointLight pointLights[10];
uniform int numPointLights;
uniform vec3 dirLightDir;
uniform vec3 dirLightColor;
uniform vec3 fogColor;
uniform float fogDensity;

vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir) {
    vec3 lightDir = normalize(light.position - fragPos);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * light.color;
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), float(shininess));
    vec3 specular = specularStrength * spec * light.color;
    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));
    return (diffuse + specular) * attenuation;
}

void main() {
    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 lightDir = normalize(-dirLightDir);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * dirLightColor;
    vec3 result = (ambientStrength * dirLightColor) + diffuse;
    for (int i = 0; i < numPointLights; i++) {
        result += CalcPointLight(pointLights[i], norm, FragPos, viewDir);
    }
    result *= objectColor;
    float dist = length(viewPos - FragPos);
    float fogFactor = exp(-pow(dist * fogDensity, 2.0));
    FragColor = mix(vec4(fogColor, 1.0), vec4(result, 1.0), clamp(fogFactor, 0.0, 1.0));
}
)";

// Emission Shader
const char* emissionVertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
void main() {
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
)";
const char* emissionFragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;
uniform vec3 objectColor;
void main() {
    FragColor = vec4(objectColor, 1.0);
}
)";

// Function Prototypes
GLuint compileShader(const char* vertexSource, const char* fragmentSource);
glm::vec3 getBezierPoint(float t, const std::vector<glm::vec3>& controlPoints);
glm::vec3 getBezierTangent(float t, const std::vector<glm::vec3>& controlPoints);
void framebuffer_size_callback(GLFWwindow* window, int width, int height);

// Main Application
int main() {
    // Initialize GLFW and GLEW
    if (!glfwInit()) { std::cerr << "Failed to initialize GLFW" << std::endl; return -1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Neon Velocity - OpenGL", NULL, NULL);
    if (window == NULL) { std::cerr << "Failed to create GLFW window" << std::endl; glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) { std::cerr << "Failed to initialize GLEW" << std::endl; return -1; }

    // Set OpenGL state
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.05f, 0.05f, 0.1f, 1.0f);

    // Compile shaders
    GLuint phongShader = compileShader(phongVertexShaderSource, phongFragmentShaderSource);
    GLuint emissionShader = compileShader(emissionVertexShaderSource, emissionFragmentShaderSource);

    // Procedurally generate road geometry
    std::vector<glm::vec3> roadControlPoints = {
        glm::vec3(-50.0f, 0.0f, 0.0f), glm::vec3(-25.0f, 0.0f, 0.0f),
        glm::vec3(25.0f, 0.0f, 50.0f), glm::vec3(50.0f, 0.0f, 50.0f)
    };
    std::vector<float> roadVertices;
    for (int i = 0; i < 100; ++i) {
        float t1=(float)i/100, t2=(float)(i+1)/100;
        glm::vec3 p1=getBezierPoint(t1,roadControlPoints), p2=getBezierPoint(t2,roadControlPoints);
        glm::vec3 n1=glm::normalize(glm::cross(glm::normalize(getBezierTangent(t1,roadControlPoints)), glm::vec3(0,1,0)));
        glm::vec3 n2=glm::normalize(glm::cross(glm::normalize(getBezierTangent(t2,roadControlPoints)), glm::vec3(0,1,0)));
        glm::vec3 v1=p1-n1*5.0f, v2=p1+n1*5.0f, v3=p2-n2*5.0f, v4=p2+n2*5.0f;
        roadVertices.insert(roadVertices.end(),{v1.x,v1.y,v1.z,0,1,0, v2.x,v2.y,v2.z,0,1,0, v3.x,v3.y,v3.z,0,1,0});
        roadVertices.insert(roadVertices.end(),{v2.x,v2.y,v2.z,0,1,0, v4.x,v4.y,v4.z,0,1,0, v3.x,v3.y,v3.z,0,1,0});
    }
    GLuint roadVAO, roadVBO;
    glGenVertexArrays(1,&roadVAO); glGenBuffers(1,&roadVBO);
    glBindVertexArray(roadVAO); glBindBuffer(GL_ARRAY_BUFFER, roadVBO);
    glBufferData(GL_ARRAY_BUFFER, roadVertices.size()*sizeof(float), &roadVertices[0], GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,6*sizeof(float),(void*)0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,6*sizeof(float),(void*)(3*sizeof(float))); glEnableVertexAttribArray(1);

    // Define vertices for a generic cube
    float cubeVertices[]={-0.5f,-0.5f,-0.5f,0,0,-1,0.5f,-0.5f,-0.5f,0,0,-1,0.5f,0.5f,-0.5f,0,0,-1,0.5f,0.5f,-0.5f,0,0,-1,-0.5f,0.5f,-0.5f,0,0,-1,-0.5f,-0.5f,-0.5f,0,0,-1,-0.5f,-0.5f,0.5f,0,0,1,0.5f,-0.5f,0.5f,0,0,1,0.5f,0.5f,0.5f,0,0,1,0.5f,0.5f,0.5f,0,0,1,-0.5f,0.5f,0.5f,0,0,1,-0.5f,-0.5f,0.5f,0,0,1,-0.5f,0.5f,0.5f,-1,0,0,-0.5f,0.5f,-0.5f,-1,0,0,-0.5f,-0.5f,-0.5f,-1,0,0,-0.5f,-0.5f,-0.5f,-1,0,0,-0.5f,-0.5f,0.5f,-1,0,0,-0.5f,0.5f,0.5f,-1,0,0,0.5f,0.5f,0.5f,1,0,0,0.5f,0.5f,-0.5f,1,0,0,0.5f,-0.5f,-0.5f,1,0,0,0.5f,-0.5f,-0.5f,1,0,0,0.5f,-0.5f,0.5f,1,0,0,0.5f,0.5f,0.5f,1,0,0,-0.5f,-0.5f,-0.5f,0,-1,0,0.5f,-0.5f,-0.5f,0,-1,0,0.5f,-0.5f,0.5f,0,-1,0,0.5f,-0.5f,0.5f,0,-1,0,-0.5f,-0.5f,0.5f,0,-1,0,-0.5f,-0.5f,-0.5f,0,-1,0,-0.5f,0.5f,-0.5f,0,1,0,0.5f,0.5f,-0.5f,0,1,0,0.5f,0.5f,0.5f,0,1,0,0.5f,0.5f,0.5f,0,1,0,-0.5f,0.5f,0.5f,0,1,0,-0.5f,0.5f,-0.5f,0,1,0};
    GLuint cubeVAO, cubeVBO;
    glGenVertexArrays(1,&cubeVAO); glGenBuffers(1,&cubeVBO);
    glBindVertexArray(cubeVAO); glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER,sizeof(cubeVertices),cubeVertices,GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,6*sizeof(float),(void*)0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,6*sizeof(float),(void*)(3*sizeof(float))); glEnableVertexAttribArray(1);

    // Procedurally generate positions for buildings, lights, and windows
    std::vector<glm::mat4> buildingModels, darkWindowModels, litWindowModels, streetlightPostModels, streetlightLampModels, streetlightHoodModels;
    std::vector<glm::vec3> pointLightPositions;
    for(int i=0;i<20;++i){
        float t=(float)i/20;
        glm::vec3 pos=getBezierPoint(t,roadControlPoints);
        glm::vec3 tangent=glm::normalize(getBezierTangent(t,roadControlPoints));
        glm::vec3 n=glm::normalize(glm::cross(tangent,glm::vec3(0,1,0)));
        float side=(i%2==0)?1.0f:-1.0f;
        float h=10.0f+(std::rand()%10)*4.0f, w=4.0f+(std::rand()%5), offset=5.0f+w;
        glm::mat4 model=glm::translate(glm::mat4(1.0f),pos+n*side*offset+glm::vec3(0,h/2.0f,0));
        model=glm::rotate(model,(float)atan2(tangent.x,tangent.z),glm::vec3(0,1,0));
        model=glm::scale(model,glm::vec3(w,h,w));
        buildingModels.push_back(model);
        for(float y=2.0f;y<h-2.0f;y+=3.0f){
            for(float x=-w/2.0f+1.5f;x<w/2.0f-1.5f;x+=3.0f){
                glm::mat4 winModel=glm::translate(model,glm::vec3(x/w,(y-h/2.0f)/h,0.51f));
                winModel=glm::scale(winModel,glm::vec3(1.5f/w,1.5f/h,0.1f));
                if(std::rand()%3==0)litWindowModels.push_back(winModel);else darkWindowModels.push_back(winModel);
            }
        }
        if(i%3==0&&pointLightPositions.size()<MAX_POINT_LIGHTS){
            glm::vec3 pPos=pos+n*side*(5.0f+1.0f);
            glm::mat4 pModel=glm::translate(glm::mat4(1.0f),pPos+glm::vec3(0,3.0f,0));
            pModel=glm::scale(pModel,glm::vec3(0.2f,6.0f,0.2f));
            streetlightPostModels.push_back(pModel);
            glm::vec3 lPos=pPos+glm::vec3(0,6.5f,0);
            pointLightPositions.push_back(lPos);
            glm::mat4 lModel=glm::translate(glm::mat4(1.0f),lPos);
            lModel=glm::scale(lModel,glm::vec3(0.5f));
            streetlightLampModels.push_back(lModel);
            glm::mat4 hModel=glm::translate(glm::mat4(1.0f),lPos+glm::vec3(0,0.3f,0));
            hModel=glm::scale(hModel,glm::vec3(0.8f,0.1f,0.8f));
            streetlightHoodModels.push_back(hModel);
        }
    }

    // Load the car model from the OBJ file
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;
    if(!tinyobj::LoadObj(&attrib,&shapes,&materials,&warn,&err,CAR_MODEL_PATH)){throw std::runtime_error(warn+err);}
    std::vector<float> carVertices;
    for(const auto& shape:shapes){
        for(const auto& index:shape.mesh.indices){
            if(3*index.vertex_index+2<attrib.vertices.size()&&index.vertex_index>=0&&3*index.normal_index+2<attrib.normals.size()&&index.normal_index>=0){
                carVertices.push_back(attrib.vertices[3*index.vertex_index+0]);carVertices.push_back(attrib.vertices[3*index.vertex_index+1]);carVertices.push_back(attrib.vertices[3*index.vertex_index+2]);
                carVertices.push_back(attrib.normals[3*index.normal_index+0]);carVertices.push_back(attrib.normals[3*index.normal_index+1]);carVertices.push_back(attrib.normals[3*index.normal_index+2]);
            }
        }
    }
    GLuint carVAO, carVBO;
    glGenVertexArrays(1,&carVAO); glGenBuffers(1,&carVBO);
    glBindVertexArray(carVAO); glBindBuffer(GL_ARRAY_BUFFER,carVBO);
    glBufferData(GL_ARRAY_BUFFER,carVertices.size()*sizeof(float),&carVertices[0],GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,6*sizeof(float),(void*)0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,6*sizeof(float),(void*)(3*sizeof(float))); glEnableVertexAttribArray(1);

    // Main Render Loop
    while (!glfwWindowShouldClose(window)) {
        // Get animation progress
        float animProgress = fmod(glfwGetTime(), 10.0f) / 10.0f;
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Define camera and projection
        float zoomFactor = 35.0f - (35.0f - 10.0f) * animProgress;
        float fov = 60.0f - (60.0f - 45.0f) * animProgress;
        glm::mat4 projection = glm::perspective(glm::radians(fov), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 200.0f);
        glm::vec3 carPos = getBezierPoint(animProgress, roadControlPoints);
        glm::vec3 carTangent = glm::normalize(getBezierTangent(animProgress, roadControlPoints));
        glm::vec3 cameraPos = carPos - carTangent * zoomFactor + glm::vec3(0, 5.0f, 0);
        glm::mat4 view = glm::lookAt(cameraPos, carPos, glm::vec3(0, 1, 0));

        // Set uniforms for the Phong (main lighting) shader
        glUseProgram(phongShader);
        glUniformMatrix4fv(glGetUniformLocation(phongShader, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(glGetUniformLocation(phongShader, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniform3f(glGetUniformLocation(phongShader, "viewPos"), cameraPos.x, cameraPos.y, cameraPos.z);
        glUniform1f(glGetUniformLocation(phongShader, "ambientStrength"), 0.3f);
        glUniform1f(glGetUniformLocation(phongShader, "specularStrength"), 1.0f);
        glUniform3f(glGetUniformLocation(phongShader, "dirLightDir"), -20.0f, -50.0f, -20.0f);
        glUniform3f(glGetUniformLocation(phongShader, "dirLightColor"), 0.6f, 0.6f, 0.7f);
        glUniform3f(glGetUniformLocation(phongShader, "fogColor"), 0.05f, 0.05f, 0.1f);
        glUniform1f(glGetUniformLocation(phongShader, "fogDensity"), 0.02f);
        glUniform1i(glGetUniformLocation(phongShader, "numPointLights"), pointLightPositions.size());
        for(int i = 0; i < pointLightPositions.size(); ++i) {
            std::string base = "pointLights[" + std::to_string(i) + "]";
            glUniform3fv(glGetUniformLocation(phongShader, (base+".position").c_str()), 1, &pointLightPositions[i][0]);
            glUniform3f(glGetUniformLocation(phongShader, (base+".color").c_str()), 1.0f, 0.7f, 0.3f);
            glUniform1f(glGetUniformLocation(phongShader, (base+".constant").c_str()), 1.0f);
            glUniform1f(glGetUniformLocation(phongShader, (base+".linear").c_str()), 0.07f);
            glUniform1f(glGetUniformLocation(phongShader, (base+".quadratic").c_str()), 0.017f);
        }

        // Draw the road
        glUniform1i(glGetUniformLocation(phongShader, "shininess"), 256);
        glUniform3f(glGetUniformLocation(phongShader, "objectColor"), 0.15f, 0.15f, 0.15f);
        glm::mat4 model = glm::mat4(1.0f);
        glUniformMatrix4fv(glGetUniformLocation(phongShader, "model"), 1, GL_FALSE, glm::value_ptr(model));
        glBindVertexArray(roadVAO);
        glDrawArrays(GL_TRIANGLES, 0, roadVertices.size()/6);

        // Draw the buildings and streetlights
        glBindVertexArray(cubeVAO);
        glUniform1i(glGetUniformLocation(phongShader, "shininess"), 32);
        glUniform3f(glGetUniformLocation(phongShader, "objectColor"), 0.2f, 0.2f, 0.25f);
        for(const auto& m:buildingModels) { glUniformMatrix4fv(glGetUniformLocation(phongShader,"model"),1,GL_FALSE,glm::value_ptr(m)); glDrawArrays(GL_TRIANGLES,0,36); }
        glUniform3f(glGetUniformLocation(phongShader, "objectColor"), 0.05f, 0.05f, 0.05f);
        for(const auto& m:darkWindowModels) { glUniformMatrix4fv(glGetUniformLocation(phongShader,"model"),1,GL_FALSE,glm::value_ptr(m)); glDrawArrays(GL_TRIANGLES,0,36); }
        glUniform3f(glGetUniformLocation(phongShader, "objectColor"), 0.4f, 0.4f, 0.4f);
        for(const auto& m:streetlightPostModels) { glUniformMatrix4fv(glGetUniformLocation(phongShader,"model"),1,GL_FALSE,glm::value_ptr(m)); glDrawArrays(GL_TRIANGLES,0,36); }
        for(const auto& m:streetlightHoodModels) { glUniformMatrix4fv(glGetUniformLocation(phongShader,"model"),1,GL_FALSE,glm::value_ptr(m)); glDrawArrays(GL_TRIANGLES,0,36); }

        // Draw the car
        glm::mat4 carRotation = glm::inverse(glm::lookAt(glm::vec3(0.0f), carTangent, glm::vec3(0.0f, 1.0f, 0.0f)));
        model = glm::translate(glm::mat4(1.0f), carPos + glm::vec3(0, -0.2f, 0));
        model = model * carRotation;
        model = glm::rotate(model, glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::scale(model, glm::vec3(CAR_SCALE_FACTOR));
        glUniformMatrix4fv(glGetUniformLocation(phongShader, "model"), 1, GL_FALSE, glm::value_ptr(model));
        glUniform3f(glGetUniformLocation(phongShader, "objectColor"), 0.1f, 0.25f, 0.6f);
        glUniform1i(glGetUniformLocation(phongShader, "shininess"), 512);
        glBindVertexArray(carVAO);
        glDrawArrays(GL_TRIANGLES, 0, carVertices.size()/6);

        // Set uniforms for the Emission (glowing) shader
        glUseProgram(emissionShader);
        glUniformMatrix4fv(glGetUniformLocation(emissionShader, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(glGetUniformLocation(emissionShader, "view"), 1, GL_FALSE, glm::value_ptr(view));

        // Draw glowing objects
        glUniform3f(glGetUniformLocation(emissionShader, "objectColor"), 1.0f, 0.9f, 0.7f);
        for(const auto& m:litWindowModels) { glUniformMatrix4fv(glGetUniformLocation(emissionShader,"model"),1,GL_FALSE,glm::value_ptr(m)); glDrawArrays(GL_TRIANGLES,0,36); }
        glUniform3f(glGetUniformLocation(emissionShader, "objectColor"), 1.0f, 0.7f, 0.3f);
        for(const auto& m:streetlightLampModels) { glUniformMatrix4fv(glGetUniformLocation(emissionShader,"model"),1,GL_FALSE,glm::value_ptr(m)); glDrawArrays(GL_TRIANGLES,0,36); }
        glm::mat4 moonModel = glm::translate(glm::mat4(1.0f), glm::vec3(20.0f, 50.0f, 20.0f));
        moonModel = glm::scale(moonModel, glm::vec3(5.0f));
        glUniformMatrix4fv(glGetUniformLocation(emissionShader, "model"), 1, GL_FALSE, glm::value_ptr(moonModel));
        glUniform3f(glGetUniformLocation(emissionShader, "objectColor"), 0.9f, 0.9f, 1.0f);
        glDrawArrays(GL_TRIANGLES, 0, 36);

        // Swap buffers and poll events
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup resources
    glDeleteVertexArrays(1, &roadVAO); glDeleteVertexArrays(1, &cubeVAO); glDeleteVertexArrays(1, &carVAO);
    glDeleteBuffers(1, &roadVBO); glDeleteBuffers(1, &cubeVBO); glDeleteBuffers(1, &carVBO);
    glDeleteProgram(phongShader); glDeleteProgram(emissionShader);
    glfwTerminate();
    return 0;
}

// Utility Functions
GLuint compileShader(const char* vertexSource, const char* fragmentSource) {
    int success; char infoLog[512];
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexSource, NULL); glCompileShader(vertexShader);
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) { glGetShaderInfoLog(vertexShader, 512, NULL, infoLog); std::cerr << "Shader Vertex Compilation Failed\n" << infoLog << std::endl; }
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentSource, NULL); glCompileShader(fragmentShader);
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) { glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog); std::cerr << "Shader Fragment Compilation Failed\n" << infoLog << std::endl; }
    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) { glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog); std::cerr << "Shader Program Linking Failed\n" << infoLog << std::endl; }
    glDeleteShader(vertexShader); glDeleteShader(fragmentShader);
    return shaderProgram;
}
glm::vec3 getBezierPoint(float t, const std::vector<glm::vec3>& controlPoints) {
    float u = 1.0f-t; float tt = t*t; float uu = u*u; float uuu=uu*u; float ttt=tt*t;
    glm::vec3 p = uuu * controlPoints[0]; p += 3*uu*t*controlPoints[1]; p += 3*u*tt*controlPoints[2]; p += ttt*controlPoints[3];
    return p;
}
glm::vec3 getBezierTangent(float t, const std::vector<glm::vec3>& controlPoints) {
    float u=1.0f-t; float tt=t*t; float uu=u*u;
    glm::vec3 p = -3*uu*controlPoints[0]; p+=3*(uu-2*u*t)*controlPoints[1]; p+=3*(2*u*t-tt)*controlPoints[2]; p+=3*tt*controlPoints[3];
    return glm::normalize(p);
}
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

