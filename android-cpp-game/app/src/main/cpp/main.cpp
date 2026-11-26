#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <cmath>
#include <vector>
#include <cstring>

struct Vec3 { float x, y, z; };

struct Building {
    float x, z;
    float w, h, d;
    float r, g, b;
    bool visited;
};

struct Engine {
    struct android_app* app;
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLSurface surface = EGL_NO_SURFACE;
    EGLContext context = EGL_NO_CONTEXT;
    int32_t width = 0, height = 0;
    GLuint program = 0;

    float playerX = 0.f, playerY = 1.8f, playerZ = 15.f;
    float yaw = 0.f, pitch = 0.f;
    float moveForward = 0.f, moveRight = 0.f;
    float lastTouchX = 0.f, lastTouchY = 0.f;
    bool touchingRight = false;
    int visitedCount = 0;

    std::vector<Building> buildings;
};

static const char* VERTEX_SHADER = R"(#version 300 es
layout(location = 0) in vec3 aPos;
uniform mat4 uMVP;
void main() {
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)";

static const char* FRAGMENT_SHADER = R"(#version 300 es
precision mediump float;
uniform vec3 uColor;
out vec4 FragColor;
void main() {
    FragColor = vec4(uColor, 1.0);
}
)";

static void mat4Identity(float* m) {
    std::memset(m, 0, 16 * sizeof(float));
    m[0] = m[5] = m[10] = m[15] = 1.f;
}

static void mat4Mul(float* out, const float* a, const float* b) {
    float r[16];
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            r[j * 4 + i] = a[0 * 4 + i] * b[j * 4 + 0] +
                           a[1 * 4 + i] * b[j * 4 + 1] +
                           a[2 * 4 + i] * b[j * 4 + 2] +
                           a[3 * 4 + i] * b[j * 4 + 3];
    std::memcpy(out, r, sizeof(r));
}

static void mat4Perspective(float* m, float fovRad, float aspect, float zn, float zf) {
    std::memset(m, 0, 16 * sizeof(float));
    float f = 1.f / std::tan(fovRad * 0.5f);
    m[0] = f / aspect;
    m[5] = f;
    m[10] = (zf + zn) / (zn - zf);
    m[11] = -1.f;
    m[14] = (2.f * zf * zn) / (zn - zf);
}

static void mat4Translate(float* m, float x, float y, float z) {
    mat4Identity(m);
    m[12] = x; m[13] = y; m[14] = z;
}

static void mat4Scale(float* m, float x, float y, float z) {
    mat4Identity(m);
    m[0] = x; m[5] = y; m[10] = z;
}

static void mat4RotateY(float* m, float a) {
    mat4Identity(m);
    float c = std::cos(a), s = std::sin(a);
    m[0] = c; m[2] = -s; m[8] = s; m[10] = c;
}

static void mat4RotateX(float* m, float a) {
    mat4Identity(m);
    float c = std::cos(a), s = std::sin(a);
    m[5] = c; m[6] = s; m[9] = -s; m[10] = c;
}

static GLuint compileShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    return s;
}

static void initBuildings(Engine* e) {
    e->buildings = {
        { -12.f, -10.f, 6.f, 8.f, 6.f, 0.2f, 0.6f, 0.9f, false }, // Shop
        {  14.f, -12.f, 7.f, 10.f, 7.f, 0.9f, 0.3f, 0.3f, false }, // Hospital
        {   0.f, -25.f, 8.f, 14.f, 8.f, 0.9f, 0.8f, 0.2f, false }, // Quest Hub
        { -20.f,  10.f, 6.f, 5.f, 8.f, 0.4f, 0.8f, 0.4f, false }, // Garage
        {  18.f,  12.f, 5.f, 6.f, 5.f, 0.7f, 0.4f, 0.9f, false }  // Safe House
    };
}

static void initDisplay(Engine* e) {
    const EGLint attribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_BLUE_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_RED_SIZE, 8,
        EGL_DEPTH_SIZE, 24,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_NONE
    };
    e->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(e->display, nullptr, nullptr);

    EGLConfig config;
    EGLint numConfigs;
    eglChooseConfig(e->display, attribs, &config, 1, &numConfigs);

    e->surface = eglCreateWindowSurface(e->display, config, e->app->window, nullptr);
    const EGLint ctxAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
    e->context = eglCreateContext(e->display, config, nullptr, ctxAttribs);
    eglMakeCurrent(e->display, e->surface, e->surface, e->context);

    eglQuerySurface(e->display, e->surface, EGL_WIDTH, &e->width);
    eglQuerySurface(e->display, e->surface, EGL_HEIGHT, &e->height);

    GLuint vs = compileShader(GL_VERTEX_SHADER, VERTEX_SHADER);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, FRAGMENT_SHADER);
    e->program = glCreateProgram();
    glAttachShader(e->program, vs);
    glAttachShader(e->program, fs);
    glLinkProgram(e->program);
    glEnable(GL_DEPTH_TEST);
}

static void drawBox(GLuint prog, const float* vp, float x, float y, float z, float sx, float sy, float sz, float r, float g, float b) {
    static const float cubeVerts[] = {
        -0.5f,-0.5f, 0.5f,  0.5f,-0.5f, 0.5f,  0.5f, 0.5f, 0.5f, -0.5f,-0.5f, 0.5f,  0.5f, 0.5f, 0.5f, -0.5f, 0.5f, 0.5f,
        -0.5f,-0.5f,-0.5f, -0.5f, 0.5f,-0.5f,  0.5f, 0.5f,-0.5f, -0.5f,-0.5f,-0.5f,  0.5f, 0.5f,-0.5f,  0.5f,-0.5f,-0.5f,
        -0.5f, 0.5f,-0.5f, -0.5f, 0.5f, 0.5f,  0.5f, 0.5f, 0.5f, -0.5f, 0.5f,-0.5f,  0.5f, 0.5f, 0.5f,  0.5f, 0.5f,-0.5f,
        -0.5f,-0.5f,-0.5f,  0.5f,-0.5f,-0.5f,  0.5f,-0.5f, 0.5f, -0.5f,-0.5f,-0.5f,  0.5f,-0.5f, 0.5f, -0.5f,-0.5f, 0.5f,
         0.5f,-0.5f,-0.5f,  0.5f, 0.5f,-0.5f,  0.5f, 0.5f, 0.5f,  0.5f,-0.5f,-0.5f,  0.5f, 0.5f, 0.5f,  0.5f,-0.5f, 0.5f,
        -0.5f,-0.5f,-0.5f, -0.5f,-0.5f, 0.5f, -0.5f, 0.5f, 0.5f, -0.5f,-0.5f,-0.5f, -0.5f, 0.5f, 0.5f, -0.5f, 0.5f,-0.5f
    };

    float t[16], s[16], model[16], mvp[16];
    mat4Translate(t, x, y, z);
    mat4Scale(s, sx, sy, sz);
    mat4Mul(model, t, s);
    mat4Mul(mvp, vp, model);

    glUniformMatrix4fv(glGetUniformLocation(prog, "uMVP"), 1, GL_FALSE, mvp);
    glUniform3f(glGetUniformLocation(prog, "uColor"), r, g, b);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, cubeVerts);
    glDrawArrays(GL_TRIANGLES, 0, 36);
}

static void drawFrame(Engine* e) {
    if (e->display == EGL_NO_DISPLAY) return;

    float speed = 0.15f;
    e->playerX += (std::sin(e->yaw) * e->moveForward + std::cos(e->yaw) * e->moveRight) * speed;
    e->playerZ += (-std::cos(e->yaw) * e->moveForward + std::sin(e->yaw) * e->moveRight) * speed;

    for (auto& b : e->buildings) {
        if (!b.visited) {
            float dx = e->playerX - b.x;
            float dz = e->playerZ - b.z;
            if (dx * dx + dz * dz < 64.f) {
                b.visited = true;
                e->visitedCount++;
            }
        }
    }

    float skyBoost = e->visitedCount * 0.08f;
    glClearColor(0.15f + skyBoost, 0.35f + skyBoost, 0.65f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(e->program);

    float proj[16], rx[16], ry[16], rot[16], tr[16], view[16], vp[16];
    float aspect = e->height > 0 ? (float)e->width / (float)e->height : 1.77f;
    mat4Perspective(proj, 1.047f, aspect, 0.1f, 300.f);
    mat4RotateX(rx, -e->pitch);
    mat4RotateY(ry, -e->yaw);
    mat4Mul(rot, rx, ry);
    mat4Translate(tr, -e->playerX, -e->playerY, -e->playerZ);
    mat4Mul(view, rot, tr);
    mat4Mul(vp, proj, view);

    // Ground
    drawBox(e->program, vp, 0.f, -0.5f, 0.f, 200.f, 1.f, 200.f, 0.22f, 0.45f, 0.25f);

    // Interactive Buildings
    for (const auto& b : e->buildings) {
        float mult = b.visited ? 1.3f : 1.0f;
        drawBox(e->program, vp, b.x, b.h * 0.5f, b.z, b.w, b.h, b.d, b.r * mult, b.g * mult, b.b * mult);
    }

    eglSwapBuffers(e->display, e->surface);
}

static int32_t handleInput(struct android_app* app, AInputEvent* event) {
    Engine* e = (Engine*)app->userData;
    if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION) {
        int32_t action = AMotionEvent_getAction(event) & AMOTION_EVENT_ACTION_MASK;
        float x = AMotionEvent_getX(event, 0);
        float y = AMotionEvent_getY(event, 0);

        if (action == AMOTION_EVENT_ACTION_DOWN || action == AMOTION_EVENT_ACTION_MOVE) {
            if (x < e->width * 0.5f) {
                e->moveForward = (e->height * 0.5f - y) / (e->height * 0.3f);
                e->moveRight = (x - e->width * 0.25f) / (e->width * 0.25f);
            } else {
                if (e->touchingRight) {
                    e->yaw += (x - e->lastTouchX) * 0.005f;
                    e->pitch += (y - e->lastTouchY) * 0.005f;
                }
                e->lastTouchX = x;
                e->lastTouchY = y;
                e->touchingRight = true;
            }
        } else if (action == AMOTION_EVENT_ACTION_UP) {
            e->moveForward = 0.f;
            e->moveRight = 0.f;
            e->touchingRight = false;
        }
        return 1;
    }
    return 0;
}

static void handleCmd(struct android_app* app, int32_t cmd) {
    Engine* e = (Engine*)app->userData;
    if (cmd == APP_CMD_INIT_WINDOW && e->app->window) {
        initDisplay(e);
        drawFrame(e);
    }
}

void android_main(struct android_app* state) {
    Engine engine{};
    engine.app = state;
    initBuildings(&engine);
    state->userData = &engine;
    state->onAppCmd = handleCmd;
    state->onInputEvent = handleInput;

    while (true) {
        int events;
        struct android_poll_source* source;
        while (ALooper_pollAll(0, nullptr, &events, (void**)&source) >= 0) {
            if (source) source->process(state, source);
            if (state->destroyRequested) return;
        }
        drawFrame(&engine);
    }
}
