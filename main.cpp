#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <string>

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// --- Константы ---
const unsigned int SCR_WIDTH = 1024;
const unsigned int SCR_HEIGHT = 768;
const float PI = 3.14159265359f;

// --- Шейдеры ---
const char* vShader = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
out vec3 FragPos;
out vec3 Normal;
uniform mat4 model; uniform mat4 view; uniform mat4 projection;
void main() {
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = mat3(transpose(inverse(model))) * aNormal;  
    gl_Position = projection * view * vec4(FragPos, 1.0);
})";

const char* fShader = R"(
#version 330 core
out vec4 FragColor;
in vec3 Normal; in vec3 FragPos;
uniform vec3 uColor; uniform vec3 lightPos;
void main() {
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 result = (0.3 + diff) * uColor;
    FragColor = vec4(result, 1.0);
})";

// --- Базовые классы ---

class GameObject {
public:
    glm::vec3 position, size, color;
    bool active = true;

    GameObject(glm::vec3 p, glm::vec3 s, glm::vec3 c) : position(p), size(s), color(c) {}
    virtual ~GameObject() = default;

    virtual void update(float dt) = 0;

    // Отрисовка объекта с использованием переданного VAO и количества индексов
    void draw(GLuint shader, GLuint vao, int indexCount) {
        if (!active) return;
        glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
        model = glm::scale(model, size);
        glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, glm::value_ptr(model));
        glUniform3fv(glGetUniformLocation(shader, "uColor"), 1, glm::value_ptr(color));
        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
    }
};

class Bullet : public GameObject {
public:
    glm::vec3 velocity;
    Bullet(glm::vec3 p, glm::vec3 v) : GameObject(p, glm::vec3(1.0f), glm::vec3(1.0f)), velocity(v) {}
    void update(float dt) override {
        position += velocity * dt;
        if (position.y > 8.0f) active = false;
    }
};

class Player : public GameObject {
public:
    float lastShot = 0;
    Player() : GameObject(glm::vec3(0, -3.5f, 0), glm::vec3(0.8f, 0.25f, 0.4f), glm::vec3(0.2f, 0.6f, 1.0f)) {}
    void update(float dt) override {}

    bool canShoot() {
        if (glfwGetTime() - lastShot > 0.35f) {
            lastShot = (float)glfwGetTime();
            return true;
        }
        return false;
    }
};

class Alien : public GameObject {
public:
    Alien(glm::vec3 p, glm::vec3 c) : GameObject(p, glm::vec3(0.7f), c) {}
    void update(float dt) override {}
};

// --- Менеджер ресурсов (Геометрия) ---

struct MeshData { GLuint vao; int count; };

class ResourceManager {
public:
    static MeshData createCube() {
        float vertices[] = {
            -0.5,-0.5,-0.5, 0,0,-1,  0.5,-0.5,-0.5, 0,0,-1,  0.5, 0.5,-0.5, 0,0,-1, -0.5, 0.5,-0.5, 0,0,-1,
            -0.5,-0.5, 0.5, 0,0, 1,  0.5,-0.5, 0.5, 0,0, 1,  0.5, 0.5, 0.5, 0,0, 1, -0.5, 0.5, 0.5, 0,0, 1,
            -0.5, 0.5, 0.5,-1,0, 0, -0.5, 0.5,-0.5,-1,0, 0, -0.5,-0.5,-0.5,-1,0, 0, -0.5,-0.5, 0.5,-1,0, 0,
             0.5, 0.5, 0.5, 1,0, 0,  0.5, 0.5,-0.5, 1,0, 0,  0.5,-0.5,-0.5, 1,0, 0,  0.5,-0.5, 0.5, 1,0, 0,
            -0.5,-0.5,-0.5, 0,-1,0,  0.5,-0.5,-0.5, 0,-1,0,  0.5,-0.5, 0.5, 0,-1,0, -0.5,-0.5, 0.5, 0,-1,0,
            -0.5, 0.5,-0.5, 0, 1,0,  0.5, 0.5,-0.5, 0, 1,0,  0.5, 0.5, 0.5, 0, 1,0, -0.5, 0.5, 0.5, 0, 1,0
        };
        unsigned int indices[] = { 0,1,2,2,3,0, 4,5,6,6,7,4, 8,9,10,10,11,8, 12,13,14,14,15,12, 16,17,18,18,19,16, 20,21,22,22,23,20 };
        return { setupVAO(vertices, sizeof(vertices), indices, sizeof(indices)), 36 };
    }

    static MeshData createSphere() {
        std::vector<float> v; std::vector<unsigned int> ind;
        int sectors = 15, stacks = 15; float radius = 0.15f;
        for (int i = 0; i <= stacks; ++i) {
            float phi = PI * i / stacks;
            for (int j = 0; j <= sectors; ++j) {
                float theta = 2.0f * PI * j / sectors;
                float x = cos(theta) * sin(phi), y = cos(phi), z = sin(theta) * sin(phi);
                v.push_back(x * radius); v.push_back(y * radius); v.push_back(z * radius);
                v.push_back(x); v.push_back(y); v.push_back(z);
            }
        }
        for (int i = 0; i < stacks; ++i) {
            for (int j = 0; j < sectors; ++j) {
                int first = i * (sectors + 1) + j, second = first + sectors + 1;
                ind.push_back(first); ind.push_back(second); ind.push_back(first + 1);
                ind.push_back(first + 1); ind.push_back(second); ind.push_back(second + 1);
            }
        }
        return { setupVAO(v.data(), v.size() * 4, ind.data(), ind.size() * 4), (int)ind.size() };
    }

private:
    static GLuint setupVAO(float* v, int vS, unsigned int* i, int iS) {
        GLuint vao, vbo, ebo;
        glGenVertexArrays(1, &vao); glGenBuffers(1, &vbo); glGenBuffers(1, &ebo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo); glBufferData(GL_ARRAY_BUFFER, vS, v, GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo); glBufferData(GL_ELEMENT_ARRAY_BUFFER, iS, i, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, 0, 24, 0); glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, 0, 24, (void*)12); glEnableVertexAttribArray(1);
        return vao;
    }
};

// --- Основной класс Игры ---

class Game {
public:
    Player player;
    std::vector<Alien> aliens;
    std::vector<Bullet> bullets;
    MeshData cube, sphere;
    GLuint shader;

    float alienTimer = 0, alienDir = 1.0f, moveInterval = 0.5f;
    int score = 0, wave = 1;
    bool running = true;

    Game() {
        shader = glCreateProgram(); // В реальности тут вызов загрузки шейдеров
        auto compile = [](GLenum type, const char* src) {
            GLuint s = glCreateShader(type); glShaderSource(s, 1, &src, 0); glCompileShader(s); return s;
            };
        GLuint vs = compile(GL_VERTEX_SHADER, vShader), fs = compile(GL_FRAGMENT_SHADER, fShader);
        glAttachShader(shader, vs); glAttachShader(shader, fs); glLinkProgram(shader);

        cube = ResourceManager::createCube();
        sphere = ResourceManager::createSphere();
        initLevel();
    }

    void initLevel() {
        aliens.clear(); bullets.clear();
        for (int r = 0; r < 3; ++r) for (int c = 0; c < 6; ++c)
            aliens.emplace_back(glm::vec3((c - 2.5f) * 1.3f, 2.0f + r * 1.2f, 0.0f), glm::vec3(0.2f, 0.8f - r * 0.2f, 0.4f));
    }

    void processInput(GLFWwindow* window, float dt) {
        if (!running) return; // Управление блокируется при проигрыше

        if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) player.position.x -= 7.0f * dt;
        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) player.position.x += 7.0f * dt;
        player.position.x = glm::clamp(player.position.x, -4.8f, 4.8f);

        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS && player.canShoot())
            bullets.emplace_back(player.position + glm::vec3(0, 0.5f, 0), glm::vec3(0, 12.0f, 0));
    }

    void update(float dt) {
        if (!running) return;

        for (auto& b : bullets) {
            b.update(dt);
            for (auto& a : aliens) {
                if (a.active && b.active && glm::distance(a.position, b.position) < 0.6f) {
                    a.active = false; b.active = false; score += 10;
                }
            }
        }
        bullets.erase(std::remove_if(bullets.begin(), bullets.end(), [](const Bullet& b) {return !b.active; }), bullets.end());

        // Логика зигзага пришельцев
        alienTimer += dt;
        if (alienTimer >= moveInterval) {
            alienTimer = 0;
            bool hitWall = false;
            for (auto& a : aliens) {
                if (a.active && ((alienDir > 0 && a.position.x >= 4.5f) || (alienDir < 0 && a.position.x <= -4.5f))) {
                    hitWall = true; break;
                }
            }

            if (hitWall) {
                alienDir *= -1.0f; // Меняем направление
                for (auto& a : aliens) {
                    a.position.y -= 0.5f; // Шагаем вниз
                    if (a.active && a.position.y <= player.position.y + 0.2f) running = false;
                }
            }
            else {
                for (auto& a : aliens) if (a.active) a.position.x += alienDir * 0.3f;
            }
        }

        if (std::none_of(aliens.begin(), aliens.end(), [](const Alien& a) {return a.active; })) {
            wave++; moveInterval *= 0.8f; initLevel();
        }
    }

    void render(GLFWwindow* window) {
        glClearColor(running ? 0.02f : 0.2f, 0.02f, 0.05f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(shader);

        glm::mat4 proj = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH / SCR_HEIGHT, 0.1f, 100.0f);
        glm::mat4 view = glm::lookAt(glm::vec3(0, 1, 10), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
        glUniformMatrix4fv(glGetUniformLocation(shader, "projection"), 1, 0, glm::value_ptr(proj));
        glUniformMatrix4fv(glGetUniformLocation(shader, "view"), 1, 0, glm::value_ptr(view));
        glUniform3f(glGetUniformLocation(shader, "lightPos"), 2.0f, 5.0f, 5.0f);

        player.draw(shader, cube.vao, cube.count);
        for (auto& a : aliens) a.draw(shader, cube.vao, cube.count);
        for (auto& b : bullets) b.draw(shader, sphere.vao, sphere.count);

        glfwSetWindowTitle(window, ("Wave: " + std::to_string(wave) + " | Score: " + std::to_string(score) + (running ? "" : " - GAME OVER")).c_str());
        glfwSwapBuffers(window);
    }
};

int main() {
    if (!glfwInit()) return -1;
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Space Invaders Fix", nullptr, nullptr);
    glfwMakeContextCurrent(window);
    glewInit();
    glEnable(GL_DEPTH_TEST);

    Game game;
    float lastFrame = 0;
    while (!glfwWindowShouldClose(window)) {
        float dt = (float)glfwGetTime() - lastFrame;
        lastFrame = (float)glfwGetTime();

        glfwPollEvents();
        game.processInput(window, dt);
        game.update(dt);
        game.render(window);
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) break;
    }
    glfwTerminate();
    return 0;
}