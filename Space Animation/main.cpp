#include <GL/glut.h>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <ctime>

// ===================== CONSTANTS =====================
#define PI 3.1415926535

// Rocket animation state
float rocket_x = 100.0f;
float rocket_y = 370.0f;   // top of left planet
float rocket_angle_deg = 70.0f;
float t_param = 0.0f;
const float animation_speed = 0.001f;

// Camera (clipping) state
float cam_left = 50, cam_right = 150;
float cam_bottom = 250, cam_top = 450;

// ===================== COLORS =====================
const float COLOR_BLACK[]   = {0.0f, 0.0f, 0.0f};
const float COLOR_WHITE[]   = {1.0f, 1.0f, 1.0f};
const float COLOR_RED[]     = {1.0f, 0.0f, 0.0f};
const float COLOR_SILVER[]  = {0.8f, 0.8f, 0.9f};
const float COLOR_DARKGREY[]= {0.3f, 0.3f, 0.3f};
const float COLOR_BLUE[]    = {0.2f, 0.4f, 1.0f};
const float COLOR_ORANGE[]  = {0.9f, 0.3f, 0.1f};

// ===================== HELPERS =====================
struct Point { int x, y; };

void drawFilledCircle(float cx, float cy, float r, const float color[3]) {
    glColor3fv(color);
    glBegin(GL_TRIANGLE_FAN);
        glVertex2f(cx, cy);
        for (int i = 0; i <= 360; i++) {
            float theta = i * PI / 180.0f;
            glVertex2f(cx + r * cos(theta), cy + r * sin(theta));
        }
    glEnd();
}

void drawStars() {
    glColor3fv(COLOR_WHITE);
    glBegin(GL_POINTS);
    for (int i = 0; i < 200; i++) {
        glVertex2i(rand() % 800, rand() % 600);
    }
    glEnd();
}

void drawFlames(Point baseL, Point baseR, float angle_deg, float scale) {
    float flame_angle_rad = (angle_deg - 90.0f) * PI / 180.0f;
    float dir_x = cos(flame_angle_rad);
    float dir_y = sin(flame_angle_rad);

    for (int i = 0; i < 15; i++) {
        float t = (float)rand() / RAND_MAX;
        Point base = {
            (int)(baseL.x + t * (baseR.x - baseL.x)),
            (int)(baseL.y + t * (baseR.y - baseL.y))
        };

        int flame_length = (10 + rand() % 20) * scale;
        Point tip = {
            (int)(base.x + flame_length * dir_x),
            (int)(base.y + flame_length * dir_y)
        };

        glColor3f(1.0f, 0.5f + (rand() % 50) / 100.0f, 0.0f);
        glBegin(GL_TRIANGLES);
            glVertex2i(base.x - 2, base.y);
            glVertex2i(base.x + 2, base.y);
            glVertex2i(tip.x, tip.y);
        glEnd();
    }
}

void drawRocket(Point translate, float angle_deg, float scale) {
    float angle_rad = angle_deg * PI / 180.0f;
    float cosA = cos(angle_rad), sinA = sin(angle_rad);

    auto transform = [&](const std::vector<Point>& v_in) {
        std::vector<Point> v_out;
        for (const auto& v : v_in) {
            v_out.push_back({
                (int)(scale * (v.x * cosA - v.y * sinA)) + translate.x,
                (int)(scale * (v.x * sinA + v.y * cosA)) + translate.y
            });
        }
        return v_out;
    };

    // Rocket parts
    auto nose      = transform({{0,40},{-15,10},{15,10}});
    auto body      = transform({{-15,10},{15,10},{15,-30},{-15,-30}});
    auto leftFin   = transform({{-15,0},{-15,-25},{-25,-35}});
    auto rightFin  = transform({{15,0},{15,-25},{25,-35}});
    auto exhaust   = transform({{-10,-30},{10,-30},{15,-40},{-15,-40}});
    auto exhaustBase = transform({{-15,-40},{15,-40}});

    drawFlames(exhaustBase[0], exhaustBase[1], angle_deg, scale);

    // Draw rocket parts filled
    glColor3fv(COLOR_RED);
    glBegin(GL_POLYGON); for (auto&p: nose) glVertex2i(p.x,p.y); glEnd();

    glColor3fv(COLOR_SILVER);
    glBegin(GL_POLYGON); for (auto&p: body) glVertex2i(p.x,p.y); glEnd();

    glColor3fv(COLOR_RED);
    glBegin(GL_POLYGON); for (auto&p: leftFin) glVertex2i(p.x,p.y); glEnd();
    glBegin(GL_POLYGON); for (auto&p: rightFin) glVertex2i(p.x,p.y); glEnd();

    glColor3fv(COLOR_DARKGREY);
    glBegin(GL_POLYGON); for (auto&p: exhaust) glVertex2i(p.x,p.y); glEnd();
}

// ===================== SCENE =====================
void drawPlanets() {
    drawFilledCircle(100, 300, 50, COLOR_BLUE);
    drawFilledCircle(700, 300, 80, COLOR_ORANGE);
}

void display() {
    glClear(GL_COLOR_BUFFER_BIT);

    // set dynamic camera
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(cam_left, cam_right, cam_bottom, cam_top);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    drawStars();
    drawPlanets();

    // Scale factor based on t_param (0.5 → 1 → 0.5)
    float scale_factor = 1.0f - fabs(0.5f - t_param);

    drawRocket({(int)rocket_x, (int)rocket_y}, rocket_angle_deg, scale_factor);

    glutSwapBuffers();
}

// ===================== ANIMATION =====================
void update(int value) {
    Point start = {100, 370};
    Point end   = {700, 400};
    Point peak  = {400, 550};

    if (t_param < 1.0f) {
        t_param += animation_speed;
        float t = t_param, u = 1.0f - t;

        // Rocket follows the parabolic path
        rocket_x = u*u*start.x + 2*u*t*peak.x + t*t*end.x;
        rocket_y = u*u*start.y + 2*u*t*peak.y + t*t*end.y;

        // --- PARABOLIC MANUAL ANGLE CONTROL ---
        // 90° at start, 0° at mid-flight, 90° at landing
        rocket_angle_deg = -360.0f * t_param * (1.0f - t_param);

        // Camera zoom out during flight
        cam_left   = 50 - 50*t;
        cam_right  = 150 + 650*t;
        cam_bottom = 250 - 250*t;
        cam_top    = 450 + 150*t;
    } else {
        // After landing, zoom into planet 2
        static float zoom_t = 0.0f;
        if (zoom_t < 1.0f) zoom_t += 0.005f;

        cam_left   = 600 - 50*(1-zoom_t);
        cam_right  = 800 + 50*(1-zoom_t);
        cam_bottom = 200 - 50*(1-zoom_t);
        cam_top    = 400 + 50*(1-zoom_t);
    }

    glutPostRedisplay();
    glutTimerFunc(16, update, 0); // ~60 FPS
}


// ===================== SETUP =====================
void myInit() {
    glClearColor(0,0,0,1);
}

int main(int argc, char** argv) {
    srand(time(0));
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(800, 600);
    glutInitWindowPosition(100,100);
    glutCreateWindow("Space Scene - Animated");
    myInit();
    glutDisplayFunc(display);
    glutTimerFunc(25, update, 0);
    glutMainLoop();
    return 0;
}
