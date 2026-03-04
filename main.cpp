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

const float PLAYER_SPEED = 7.0f;
const float BULLET_SPEED = 12.0f;
const float PLAYER_BULLET_COOLDOWN = 0.3f;

const int ALIEN_ROWS = 3;
const int ALIEN_COLS = 6;
const float ALIEN_SPACING = 1.2f;
const float ALIEN_DROP_SPEED = 0.4f;
const float ALIEN_STEP_X = 0.3f;
const float WALL_LIMIT = 4.8f;

struct GameObject {
    glm::vec3 position;
    glm::vec3 size;
    glm::vec3 color;
    bool active;
};

struct Bullet {
    glm::vec3 position;
    glm::vec3 velocity;
    bool active;
};

// Состояние игры
GameObject player = { glm::vec3(0.0f, -3.0f, 0.0f), glm::vec3(0.8f, 0.2f, 0.4f), glm::vec3(0.2f, 0.6f, 1.0f), true };
std::vector<Bullet> playerBullets;
std::vector<GameObject> aliens;
float lastShotTime = 0.0f, alienMoveTimer = 0.0f, alienMoveDirection = 1.0f;
int score = 0, currentWave = 1;
bool gameRunning = true;
float currentMoveInterval = 0.5f;

// --- Шейдеры с освещением ---
const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal; // Входящие нормали

out vec3 FragPos;
out vec3 Normal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    FragPos = vec3(model * vec4(aPos, 1.0));
    // Вычисляем нормаль в мировых координатах (с учетом вращения/масштаба)
    Normal = mat3(transpose(inverse(model))) * aNormal;  
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
)";

const char* fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;

in vec3 Normal;
in vec3 FragPos;

uniform vec3 uColor;
uniform vec3 lightPos; // Позиция лампочки
uniform vec3 viewPos;  // Позиция камеры

void main() {
    // 1. Ambient (Фоновое освещение)
    float ambientStrength = 0.3;
    vec3 ambient = ambientStrength * vec3(1.0, 1.0, 1.0);
  	
    // 2. Diffuse (Диффузное освещение)
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * vec3(1.0, 1.0, 1.0);
    
    // Результат
    vec3 result = (ambient + diffuse) * uColor;
    FragColor = vec4(result, 1.0);
}
)";

// --- Вспомогательные функции отрисовки ---

GLuint createProgram(const char* v, const char* f) {
    auto c = [](GLenum t, const char* s) { GLuint h = glCreateShader(t); glShaderSource(h, 1, &s, 0); glCompileShader(h); return h; };
    GLuint vs = c(GL_VERTEX_SHADER, v), fs = c(GL_FRAGMENT_SHADER, f), p = glCreateProgram();
    glAttachShader(p, vs); glAttachShader(p, fs); glLinkProgram(p); return p;
}

// Куб с нормалями для каждой грани (чтобы грани были плоскими и четкими)
GLuint createCubeVAO() {
    float vertices[] = {
        // Позиции          // Нормали
        -0.5f,-0.5f,-0.5f,  0.0f, 0.0f,-1.0f,  0.5f,-0.5f,-0.5f,  0.0f, 0.0f,-1.0f,  0.5f, 0.5f,-0.5f,  0.0f, 0.0f,-1.0f, -0.5f, 0.5f,-0.5f,  0.0f, 0.0f,-1.0f,
        -0.5f,-0.5f, 0.5f,  0.0f, 0.0f, 1.0f,  0.5f,-0.5f, 0.5f,  0.0f, 0.0f, 1.0f,  0.5f, 0.5f, 0.5f,  0.0f, 0.0f, 1.0f, -0.5f, 0.5f, 0.5f,  0.0f, 0.0f, 1.0f,
        -0.5f, 0.5f, 0.5f, -1.0f, 0.0f, 0.0f, -0.5f, 0.5f,-0.5f, -1.0f, 0.0f, 0.0f, -0.5f,-0.5f,-0.5f, -1.0f, 0.0f, 0.0f, -0.5f,-0.5f, 0.5f, -1.0f, 0.0f, 0.0f,
         0.5f, 0.5f, 0.5f,  1.0f, 0.0f, 0.0f,  0.5f, 0.5f,-0.5f,  1.0f, 0.0f, 0.0f,  0.5f,-0.5f,-0.5f,  1.0f, 0.0f, 0.0f,  0.5f,-0.5f, 0.5f,  1.0f, 0.0f, 0.0f,
        -0.5f,-0.5f,-0.5f,  0.0f,-1.0f, 0.0f,  0.5f,-0.5f,-0.5f,  0.0f,-1.0f, 0.0f,  0.5f,-0.5f, 0.5f,  0.0f,-1.0f, 0.0f, -0.5f,-0.5f, 0.5f,  0.0f,-1.0f, 0.0f,
        -0.5f, 0.5f,-0.5f,  0.0f, 1.0f, 0.0f,  0.5f, 0.5f,-0.5f,  0.0f, 1.0f, 0.0f,  0.5f, 0.5f, 0.5f,  0.0f, 1.0f, 0.0f, -0.5f, 0.5f, 0.5f,  0.0f, 1.0f, 0.0f
    };
    unsigned int indices[] = {
        0,1,2,2,3,0, 4,5,6,6,7,4, 8,9,10,10,11,8, 12,13,14,14,15,12, 16,17,18,18,19,16, 20,21,22,22,23,20
    };
    GLuint vao, vbo, ebo;
    glGenVertexArrays(1, &vao); glGenBuffers(1, &vbo); glGenBuffers(1, &ebo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    // Атрибут позиций
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // Атрибут нормалей
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    return vao;
}

GLuint createSphereVAO() {
    std::vector<float> v; std::vector<unsigned int> ind;
    int sectors = 12, stacks = 12;
    for (int i = 0; i <= stacks; ++i) {
        float phi = i * (PI / stacks);
        for (int j = 0; j <= sectors; ++j) {
            float theta = j * (2 * PI / sectors);
            float x = cos(theta) * sin(phi);
            float y = cos(phi);
            float z = sin(theta) * sin(phi);
            v.push_back(x * 0.2f); v.push_back(y * 0.2f); v.push_back(z * 0.2f); // Pos
            v.push_back(x); v.push_back(y); v.push_back(z); // Normal (для сферы совпадает с нормализованной позицией)
        }
    }
    for (int i = 0; i < stacks; ++i) for (int j = 0; j < sectors; ++j) {
        int f = i * (sectors + 1) + j, s = f + sectors + 1;
        ind.push_back(f); ind.push_back(s); ind.push_back(f + 1); ind.push_back(f + 1); ind.push_back(s); ind.push_back(s + 1);
    }
    GLuint vao, vbo, ebo; glGenVertexArrays(1, &vao); glGenBuffers(1, &vbo); glGenBuffers(1, &ebo);
    glBindVertexArray(vao); glBindBuffer(GL_ARRAY_BUFFER, vbo); glBufferData(GL_ARRAY_BUFFER, v.size() * 4, v.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo); glBufferData(GL_ELEMENT_ARRAY_BUFFER, ind.size() * 4, ind.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, 0, 24, 0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, 0, 24, (void*)12); glEnableVertexAttribArray(1);
    return vao;
}

// --- Логика Игры ---

void initAliens() {
    aliens.clear(); playerBullets.clear();
    for (int row = 0; row < ALIEN_ROWS; ++row) {
        for (int col = 0; col < ALIEN_COLS; ++col) {
            GameObject a;
            a.position = glm::vec3((col - (ALIEN_COLS - 1) / 2.0f) * ALIEN_SPACING, 2.0f + row * ALIEN_SPACING, 0.0f);
            a.size = glm::vec3(0.7f);
            a.color = glm::vec3(0.2f + (currentWave * 0.1f), 0.8f - row * 0.2f, 0.4f);
            a.active = true;
            aliens.push_back(a);
        }
    }
    alienMoveDirection = 1.0f;
}

void updateGame(float dt, GLFWwindow* window) {
    if (!gameRunning) return;

    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) player.position.x = std::max(player.position.x - PLAYER_SPEED * dt, -WALL_LIMIT);
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) player.position.x = std::min(player.position.x + PLAYER_SPEED * dt, WALL_LIMIT);

    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS && glfwGetTime() - lastShotTime > PLAYER_BULLET_COOLDOWN) {
        playerBullets.push_back({ player.position + glm::vec3(0, 0.5f, 0), glm::vec3(0, BULLET_SPEED, 0), true });
        lastShotTime = glfwGetTime();
    }

    int activeAliens = 0;
    for (auto& b : playerBullets) {
        b.position += b.velocity * dt;
        if (b.position.y > 7.0f) b.active = false;
        for (auto& a : aliens) if (a.active && glm::distance(b.position, a.position) < 0.6f) { a.active = false; b.active = false; score += 10; }
    }
    playerBullets.erase(std::remove_if(playerBullets.begin(), playerBullets.end(), [](const Bullet& b) {return !b.active; }), playerBullets.end());

    for (auto& a : aliens) if (a.active) activeAliens++;
    if (activeAliens == 0) { currentWave++; currentMoveInterval *= 0.8f; initAliens(); return; }

    alienMoveTimer += dt;
    if (alienMoveTimer >= currentMoveInterval) {
        alienMoveTimer = 0;
        bool shouldTurn = false;
        for (auto& a : aliens) if (a.active && ((alienMoveDirection > 0 && a.position.x >= WALL_LIMIT) || (alienMoveDirection < 0 && a.position.x <= -WALL_LIMIT))) { shouldTurn = true; break; }
        if (shouldTurn) {
            alienMoveDirection *= -1.0f;
            for (auto& a : aliens) { a.position.y -= ALIEN_DROP_SPEED; if (a.active && a.position.y <= player.position.y + 0.3f) gameRunning = false; }
        }
        else for (auto& a : aliens) if (a.active) a.position.x += alienMoveDirection * ALIEN_STEP_X;
    }
}

int main() {
    glfwInit();
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Space Invaders 3D - Lit", nullptr, nullptr);
    glfwMakeContextCurrent(window);
    glewInit();
    glEnable(GL_DEPTH_TEST);

    GLuint shader = createProgram(vertexShaderSource, fragmentShaderSource);
    GLuint cubeVAO = createCubeVAO();
    GLuint sphereVAO = createSphereVAO();

    initAliens();
    float lastFrame = 0.0f;

    while (!glfwWindowShouldClose(window)) {
        float currentFrame = (float)glfwGetTime();
        float deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        updateGame(deltaTime, window);

        glClearColor(0.02f, 0.02f, 0.05f, 1.0f);
        if (!gameRunning) glClearColor(0.2f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shader);

        // Матрицы камеры
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH / SCR_HEIGHT, 0.1f, 100.0f);
        glm::vec3 cameraPos = glm::vec3(0, 1, 10);
        glm::mat4 view = glm::lookAt(cameraPos, glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));

        glUniformMatrix4fv(glGetUniformLocation(shader, "projection"), 1, 0, glm::value_ptr(projection));
        glUniformMatrix4fv(glGetUniformLocation(shader, "view"), 1, 0, glm::value_ptr(view));

        // Параметры освещения
        glUniform3f(glGetUniformLocation(shader, "lightPos"), 2.0f, 5.0f, 5.0f); // Свет сверху и сбоку
        glUniform3fv(glGetUniformLocation(shader, "viewPos"), 1, glm::value_ptr(cameraPos));

        // Рендер Игрока
        glm::mat4 model = glm::scale(glm::translate(glm::mat4(1.0f), player.position), player.size);
        glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, 0, glm::value_ptr(model));
        glUniform3fv(glGetUniformLocation(shader, "uColor"), 1, glm::value_ptr(player.color));
        glBindVertexArray(cubeVAO);
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);

        // Рендер Пришельцев
        for (auto& a : aliens) {
            if (!a.active) continue;
            model = glm::scale(glm::translate(glm::mat4(1.0f), a.position), a.size);
            glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, 0, glm::value_ptr(model));
            glUniform3fv(glGetUniformLocation(shader, "uColor"), 1, glm::value_ptr(a.color));
            glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
        }

        // Рендер Пуль
        glBindVertexArray(sphereVAO);
        for (auto& b : playerBullets) {
            model = glm::translate(glm::mat4(1.0f), b.position);
            glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, 0, glm::value_ptr(model));
            glUniform3f(glGetUniformLocation(shader, "uColor"), 1, 1, 1);
            glDrawElements(GL_TRIANGLES, 12 * 12 * 6, GL_UNSIGNED_INT, 0);
        }

        glfwSetWindowTitle(window, ("Wave: " + std::to_string(currentWave) + " | Score: " + std::to_string(score)).c_str());
        glfwSwapBuffers(window);
        glfwPollEvents();
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) break;
    }

    glfwTerminate();
    return 0;
}