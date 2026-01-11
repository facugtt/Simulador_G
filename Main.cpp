#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <vector>
#include <deque>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// --- CONFIGURACIÓN ---
int Ancho = 1280;
int Alto = 720;

glm::vec3 CamaraPos = glm::vec3(0.0f, -180.0f, 140.0f);
bool primeroRaton = true;
float ultimoX = 640, ultimoY = 360, azimuth = 0.0f, elevacion = -35.0f, radioCamara = 250.0f;

// --- SHADERS ---
const char* vShader = R"glsl(
#version 330 core
layout(location=0) in vec3 aPos;
uniform mat4 model; uniform mat4 view; uniform mat4 projection;
out float depth;
void main() {
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    depth = aPos.z;
})glsl";

const char* fShader = R"glsl(
#version 330 core
in float depth; out vec4 FragColor;
uniform vec4 objectColor; uniform bool isGrid;
void main() {
    if (isGrid) {
        // Multiplicamos por 0.05 para que pequeñas profundidades (planetas) 
        // ya generen un brillo azul claro visible.
        float intensity = 0.05 + clamp(abs(depth) * 0.01, 0.0, 0.9);
        FragColor = vec4(objectColor.rgb * intensity, 0.7);
    } else { 
        FragColor = objectColor; 
    }
})glsl";

// --- CALLBACKS ---
void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    if (primeroRaton) { ultimoX = (float)xpos; ultimoY = (float)ypos; primeroRaton = false; }
    
    float xoffset = (float)xpos - ultimoX;
    float yoffset = ultimoY - (float)ypos;
    ultimoX = (float)xpos; ultimoY = (float)ypos;

    azimuth += xoffset * 0.1f;
    elevacion += yoffset * 0.1f;

    // Permitimos que suba hasta 89.9 grados para ver casi desde el polo norte del Sol
    if (elevacion > 89.9f) elevacion = 89.9f;
    if (elevacion < -89.9f) elevacion = -89.9f;

    float radElev = glm::radians(elevacion);
    float radAzim = glm::radians(azimuth);

    CamaraPos.x = radioCamara * cos(radElev) * sin(radAzim);
    CamaraPos.y = radioCamara * sin(radElev);
    CamaraPos.z = radioCamara * cos(radElev) * cos(radAzim);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    radioCamara -= (float)yoffset * 25.0f;    
    radioCamara = glm::clamp(radioCamara, 50.0f, 2500.0f); 
}

// --- CLASE CUERPO CELESTE ---
class Cuerpo {
public:
    GLuint VAO; glm::vec3 pos, vel, color;
    float masa, radioVis; int nInd;
    std::deque<glm::vec3> estela;
    bool tieneAnillos = false;

    Cuerpo(float m, float x, float vy, glm::vec3 col, float r, bool anillos = false) 
        : masa(m), pos(x, 0, 0), vel(0, vy, 0), color(col), radioVis(r), tieneAnillos(anillos) {
        
        std::vector<float> v; std::vector<unsigned int> ind;
        int res = 32;
        for(int i=0; i<=res; ++i) {
            float t = i * 3.14159f / res;
            for(int j=0; j<=res; ++j) {
                float p = j * 6.28318f / res;
                v.push_back(radioVis * sin(t) * cos(p));
                v.push_back(radioVis * sin(t) * sin(p));
                v.push_back(radioVis * cos(t));
            }
        }
        for(int i=0; i<res; ++i) {
            for(int j=0; j<res; ++j) {
                int f = i*(res+1)+j, s = f+res+1;
                ind.push_back(f); ind.push_back(s); ind.push_back(f+1);
                ind.push_back(s); ind.push_back(s+1); ind.push_back(f+1);
            }
        }
        nInd = (int)ind.size();
        GLuint VBO, EBO; glGenVertexArrays(1, &VAO); glGenBuffers(1, &VBO); glGenBuffers(1, &EBO);
        glBindVertexArray(VAO); glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, v.size()*4, v.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, ind.size()*4, ind.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 12, 0); glEnableVertexAttribArray(0);
    }

    void Dibujar(unsigned int sp, GLuint vaoAnillo) {
        glm::mat4 m = glm::translate(glm::mat4(1.0f), pos);
        glUniformMatrix4fv(glGetUniformLocation(sp, "model"), 1, GL_FALSE, glm::value_ptr(m));
        glUniform4f(glGetUniformLocation(sp, "objectColor"), color.r, color.g, color.b, 1.0f);
        glUniform1i(glGetUniformLocation(sp, "isGrid"), false);
        glBindVertexArray(VAO); glDrawElements(GL_TRIANGLES, nInd, GL_UNSIGNED_INT, 0);

        if (tieneAnillos) {
            glUniform4f(glGetUniformLocation(sp, "objectColor"), 0.6f, 0.5f, 0.4f, 0.6f);
    
            glm::mat4 mAnillo = glm::translate(glm::mat4(1.0f), pos);
            mAnillo = glm::rotate(mAnillo, glm::radians(25.0f), glm::vec3(1.0f, 0.0f, 0.0f));
            
            glUniformMatrix4fv(glGetUniformLocation(sp, "model"), 1, GL_FALSE, glm::value_ptr(mAnillo));
            
            glBindVertexArray(vaoAnillo);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 64);
}
    }
};

GLuint crearVAOAnillo() {
    std::vector<float> v;
    float rInt = 4.0f, rExt = 7.0f;
    for (int i = 0; i <= 32; ++i) {
        float a = i * 6.28318f / 32.0f;
        v.push_back(cos(a) * rInt); v.push_back(sin(a) * rInt); v.push_back(0);
        v.push_back(cos(a) * rExt); v.push_back(sin(a) * rExt); v.push_back(0);
    }
    GLuint vao, vbo; glGenVertexArrays(1, &vao); glGenBuffers(1, &vbo);
    glBindVertexArray(vao); glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, v.size()*4, v.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 12, 0); glEnableVertexAttribArray(0);
    return vao;
}

void IntegrarFisica(std::vector<Cuerpo>& sistema, float dt) {
    const float G = 0.01f;
    
    // 1. Calcular aceleraciones para TODOS
    std::vector<glm::vec3> aceleraciones(sistema.size(), glm::vec3(0.0f));

    for (size_t i = 0; i < sistema.size(); ++i) {
        for (size_t j = 0; j < sistema.size(); ++j) {
            if (i == j) continue; // No se atrae a sí mismo

            glm::vec3 r_vec = sistema[j].pos - sistema[i].pos;
            float r_mag = glm::length(r_vec);
            
            if (r_mag < 5.0f) continue; // Evitar división por cero o colisión extrema

            // Fuerza: F = G * m1 * m2 / r^2 -> a = G * m2 / r^2
            aceleraciones[i] += glm::normalize(r_vec) * (G * sistema[j].masa / (r_mag * r_mag));
        }
    }

    // 2. Aplicar Symplectic Euler a todos
    for (size_t i = 0; i < sistema.size(); ++i) {
        sistema[i].vel += aceleraciones[i] * dt;
        sistema[i].pos += sistema[i].vel * dt;
    }
}

int main() {
    glfwInit();
    GLFWwindow* win = glfwCreateWindow(Ancho, Alto, "Simulador", NULL, NULL);
    glfwMakeContextCurrent(win);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glfwSetCursorPosCallback(win, mouse_callback);
    glfwSetScrollCallback(win, scroll_callback);
    glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    unsigned int v = glCreateShader(GL_VERTEX_SHADER); glShaderSource(v, 1, &vShader, NULL); glCompileShader(v);
    unsigned int f = glCreateShader(GL_FRAGMENT_SHADER); glShaderSource(f, 1, &fShader, NULL); glCompileShader(f);
    unsigned int sp = glCreateProgram(); glAttachShader(sp, v); glAttachShader(sp, f); glLinkProgram(sp);

    GLuint vaoAnillo = crearVAOAnillo();

    int mRes = 300; float mTam = 2800.0f;
    std::vector<glm::vec3> mVerts; std::vector<unsigned int> mInds;
    for(int j=0; j<=mRes; ++j) for(int i=0; i<=mRes; ++i) mVerts.push_back({i*mTam/mRes - mTam/2, j*mTam/mRes - mTam/2, 0});
    for(int j=0; j<mRes; ++j) for(int i=0; i<mRes; ++i) {
        int a = j*(mRes+1)+i; mInds.push_back(a); mInds.push_back(a+1); mInds.push_back(a); mInds.push_back(a+mRes+1);
    }

    GLuint mVAO, mVBO, mEBO; glGenVertexArrays(1, &mVAO); glGenBuffers(1, &mVBO); glGenBuffers(1, &mEBO);
    glBindVertexArray(mVAO); glBindBuffer(GL_ARRAY_BUFFER, mVBO);
    glBufferData(GL_ARRAY_BUFFER, mVerts.size()*12, mVerts.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, mInds.size()*4, mInds.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 12, 0); glEnableVertexAttribArray(0);

    std::vector<Cuerpo> sistema;
    // CUERPO (Masa, Distancia_al_Sol, Velocidad_Orbital, Color, Radio_Visual, Anillos)

    // EL SOL (Masa: 1,989,000 unidades)
    sistema.push_back(Cuerpo(1989000.0f, 0.0f, 0.0f, {1.0f, 0.6f, 0.0f}, 15.0f));

    // MERCURIO (Masa: 0.33, Dist: 58, Vel: 18.5)
    sistema.push_back(Cuerpo(0.33f, 58.0f, 18.5f, {0.6f, 0.6f, 0.6f}, 0.8f));

    // VENUS (Masa: 4.87, Dist: 108, Vel: 13.5)
    sistema.push_back(Cuerpo(4.87f, 108.0f, 13.5f, {0.8f, 0.7f, 0.4f}, 1.2f));

    // TIERRA (Masa: 5.97, Dist: 150, Vel: 11.5)
    sistema.push_back(Cuerpo(5.97f, 150.0f, 11.5f, {0.1f, 0.4f, 0.9f}, 1.3f));

    // MARTE (Masa: 0.64, Dist: 228, Vel: 9.3)
    sistema.push_back(Cuerpo(0.64f, 228.0f, 9.3f, {0.9f, 0.3f, 0.1f}, 1.0f));

    // JÚPITER (Masa: 1898, Dist: 778, Vel: 5.0) -> Nota: Reducimos distancias exteriores para visualización
    sistema.push_back(Cuerpo(1898.0f, 450.0f, 6.6f, {0.8f, 0.6f, 0.4f}, 4.5f));

    // SATURNO (Masa: 568, Dist: 1433, Vel: 3.7) -> Escalado visual
    sistema.push_back(Cuerpo(568.0f, 650.0f, 5.5f, {0.7f, 0.7f, 0.5f}, 4.0f, true));

    // URANO (Masa: 86.8, Dist: 2872, Vel: 2.6) -> Escalado visual
    sistema.push_back(Cuerpo(86.8f, 850.0f, 4.8f, {0.6f, 0.9f, 0.9f}, 3.0f));

    // NEPTUNO (Masa: 102, Dist: 4495, Vel: 2.1) -> Escalado visual
    sistema.push_back(Cuerpo(102.0f, 1050.0f, 4.3f, {0.1f, 0.2f, 0.8f}, 2.9f));

    GLuint vaoE, vboE;
    glGenVertexArrays(1, &vaoE);
    glGenBuffers(1, &vboE);
    glBindVertexArray(vaoE);
    glBindBuffer(GL_ARRAY_BUFFER, vboE);
    glBufferData(GL_ARRAY_BUFFER, 501 * sizeof(glm::vec3), NULL, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    while (!glfwWindowShouldClose(win)) {
        for(int i=0; i<2; i++) IntegrarFisica(sistema, 0.0008f);
        
        for (auto& c : sistema) {
        if (c.estela.size() > 500) c.estela.pop_front();
        c.estela.push_back(c.pos);
        }

        //curvatura de malla
        for (auto& mv : mVerts) {
            float z = 0;
            for (size_t i = 0; i < sistema.size(); ++i) {
                float d = glm::distance(glm::vec2(mv.x, mv.y), glm::vec2(sistema[i].pos.x, sistema[i].pos.y));
                float pesoVisual = (i == 0) ? 0.0005f : 0.2f; 
                z -= (sistema[i].masa * pesoVisual) / (d * 0.1f + 1.5f);
            }
            mv.z = z;
        }

        glBindBuffer(GL_ARRAY_BUFFER, mVBO); glBufferSubData(GL_ARRAY_BUFFER, 0, mVerts.size()*12, mVerts.data());

        glClearColor(0, 0, 0.005f, 1); glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(sp);

        glm::mat4 proj = glm::perspective(glm::radians(45.0f), (float)Ancho/(float)Alto, 0.1f, 3000.0f);
        glm::mat4 view = glm::lookAt(CamaraPos, glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
        glUniformMatrix4fv(glGetUniformLocation(sp, "projection"), 1, GL_FALSE, glm::value_ptr(proj));
        glUniformMatrix4fv(glGetUniformLocation(sp, "view"), 1, GL_FALSE, glm::value_ptr(view));

        glUniform1i(glGetUniformLocation(sp, "isGrid"), true);
        glUniform4f(glGetUniformLocation(sp, "objectColor"), 0.0f, 0.4f, 0.8f, 1.0f);
        glUniformMatrix4fv(glGetUniformLocation(sp, "model"), 1, GL_FALSE, glm::value_ptr(glm::mat4(1.0f)));
        glBindVertexArray(mVAO); glDrawElements(GL_LINES, (GLsizei)mInds.size(), GL_UNSIGNED_INT, 0);

        glUniform1i(glGetUniformLocation(sp, "isGrid"), false);
        

        for (auto& c : sistema) {
            c.Dibujar(sp, vaoAnillo);

            if (c.estela.size() < 2) continue;

            glm::mat4 identidad = glm::mat4(1.0f);
            glUniformMatrix4fv(glGetUniformLocation(sp, "model"), 1, GL_FALSE, glm::value_ptr(identidad));
            
            glUniform4f(glGetUniformLocation(sp, "objectColor"), c.color.r, c.color.g, c.color.b, 0.3f);
            
            std::vector<glm::vec3> pts(c.estela.begin(), c.estela.end());
            glBindVertexArray(vaoE);
            glBindBuffer(GL_ARRAY_BUFFER, vboE);
            glBufferSubData(GL_ARRAY_BUFFER, 0, pts.size() * sizeof(glm::vec3), pts.data());
            glDrawArrays(GL_LINE_STRIP, 0, (GLsizei)pts.size());
        }
        glBindVertexArray(0);

        glfwSwapBuffers(win); glfwPollEvents();
    }
    glfwTerminate(); return 0;
}