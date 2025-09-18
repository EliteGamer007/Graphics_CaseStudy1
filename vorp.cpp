#include <GL/glut.h>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <algorithm>


#define PI 3.1415926535

// Initial Rocket position
float rocket_x = 100.0f;
float rocket_y = 370.0f;
float rocket_angle_deg = 0.0f;
float t_param = 0.0f;
const float animation_speed = 0.001f;

// Clipping window initial state
float cam_left = 50, cam_right = 150;
float cam_bottom = 250, cam_top = 450;

// Colours
const float COLOR_BLACK[]   = {0.0f, 0.0f, 0.0f};
const float COLOR_WHITE[]   = {1.0f, 1.0f, 1.0f};
const float COLOR_RED[]     = {1.0f, 0.0f, 0.0f};
const float COLOR_SILVER[]  = {0.8f, 0.8f, 0.9f};
const float COLOR_DARKGREY[]= {0.3f, 0.3f, 0.3f};
const float COLOR_BLUE[]    = {0.2f, 0.4f, 1.0f};
const float COLOR_ORANGE[]  = {0.9f, 0.3f, 0.1f};


struct Point { int x, y; };

// Liang Barsky Line clipping algorithm
bool liangBarskyClip(Point &p0, Point &p1, float xmin, float xmax, float ymin, float ymax) {
    float dx = p1.x - p0.x;
    float dy = p1.y - p0.y;

    float t0 = 0.0f, t1 = 1.0f;

    float p[4] = {-dx, dx, -dy, dy};
    float q[4] = {p0.x - xmin, xmax - p0.x, p0.y - ymin, ymax - p0.y};

    for (int i = 0; i < 4; i++) {
        if (p[i] == 0) {
            if (q[i] < 0) return false; // Line parallel & outside
        } else {
            float r = q[i] / p[i];
            if (p[i] < 0) t0 = std::max(t0, r);
            else t1 = std::min(t1, r);
        }
    }

    if (t0 > t1) return false; // No visible part

    Point new_p0 = {int(p0.x + t0 * dx), int(p0.y + t0 * dy)};
    Point new_p1 = {int(p0.x + t1 * dx), int(p0.y + t1 * dy)};
    p0 = new_p0; p1 = new_p1;
    return true;
}

// Bresenham's line drawing algorithm
void drawLine(Point p0, Point p1, const float color[3]) {
    if (!liangBarskyClip(p0, p1, cam_left, cam_right, cam_bottom, cam_top))
        return;

    glColor3fv(color);
    glBegin(GL_POINTS);

    int dx = abs(p1.x - p0.x), sx = (p0.x < p1.x ? 1 : -1);
    int dy = -abs(p1.y - p0.y), sy = (p0.y < p1.y ? 1 : -1);
    int err = dx + dy;

    while (true) {
        glVertex2i(p0.x, p0.y);
        if (p0.x == p1.x && p0.y == p1.y) break;
        int e2 = 2*err;
        if (e2 >= dy) { err += dy; p0.x += sx; }
        if (e2 <= dx) { err += dx; p0.y += sy; }
    }
    glEnd();
}

//center point circle drawing
void drawCircleOutline(int cx, int cy, int r, const float color[3]) {
    glColor3fv(color);
    int x = 0, y = r;
    int d = 1 - r;
    while(y >= x) {
        glBegin(GL_POINTS);
        glVertex2i(cx + x, cy + y); glVertex2i(cx - x, cy + y);
        glVertex2i(cx + x, cy - y); glVertex2i(cx - x, cy - y);
        glVertex2i(cx + y, cy + x); glVertex2i(cx - y, cy + x);
        glVertex2i(cx + y, cy - x); glVertex2i(cx - y, cy - x);
        glEnd();
        x++;
        if(d < 0) d += 2*x + 1;
        else { y--; d += 2*(x - y) + 1; }
    }
}


// Scan line Fill
void scanlineFillPolygon(const std::vector<Point>& polygon, const float color[3]) {
    int minY = polygon[0].y, maxY = polygon[0].y;
    for(auto&p: polygon){ minY = std::min(minY, p.y); maxY = std::max(maxY, p.y); }

    for(int y = minY; y <= maxY; y++){
        std::vector<int> intersections;
        int n = polygon.size();
        for(int i=0;i<n;i++){
            Point a = polygon[i], b = polygon[(i+1)%n];
            if(a.y == b.y) continue;
            if(y < std::min(a.y,b.y) || y > std::max(a.y,b.y)) continue;
            int x = a.x + (y - a.y)*(b.x - a.x)/(b.y - a.y);
            intersections.push_back(x);
        }
        std::sort(intersections.begin(), intersections.end());
        for(int i=0;i+1<intersections.size();i+=2){
            drawLine({intersections[i], y},{intersections[i+1],y}, color);
        }
    }
}

//Scan line fill
void drawFilledCircleScanline(int cx, int cy, int r, const float color[3]){
    for(int y=-r;y<=r;y++){
        int dx = (int)sqrt(r*r - y*y);
        drawLine({cx-dx, cy+y},{cx+dx, cy+y}, color);
    }
}

// Random pixel glow stars
void drawStars(){
    glColor3fv(COLOR_WHITE);
    glBegin(GL_POINTS);
    for(int i=0;i<200;i++) glVertex2i(rand()%800, rand()%600);
    glEnd();
}

//Draw Flames
void drawFlames(Point baseL, Point baseR, float angle_deg, float scale){
    float rad = (angle_deg-90)*PI/180;
    float dir_x = cos(rad), dir_y = sin(rad);
    for(int i=0;i<15;i++){
        float t = (float)rand()/RAND_MAX;
        Point base = {(int)(baseL.x + t*(baseR.x-baseL.x)), (int)(baseL.y + t*(baseR.y-baseL.y))};
        int len = (10+rand()%20)*scale;
        Point tip = {(int)(base.x+len*dir_x),(int)(base.y+len*dir_y)};
        glColor3f(1.0f,0.5f+(rand()%50)/100.0f,0.0f);
        drawLine({base.x-2,base.y},{tip.x,tip.y}, COLOR_RED);
        drawLine({base.x+2,base.y},{tip.x,tip.y}, COLOR_RED);
    }
}

//Draw Rocket
void drawRocket(Point translate, float angle_deg, float scale){
    float rad = angle_deg*PI/180;
    float cosA = cos(rad), sinA = sin(rad);
    auto transform = [&](const std::vector<Point>& v_in){
        std::vector<Point> v_out;
        for(auto &v:v_in){
            v_out.push_back({(int)(scale*(v.x*cosA - v.y*sinA))+translate.x,
                             (int)(scale*(v.x*sinA + v.y*cosA))+translate.y});
        }
        return v_out;
    };
    auto nose = transform({{0,40},{-15,10},{15,10}});
    auto body = transform({{-15,10},{15,10},{15,-30},{-15,-30}});
    auto leftFin = transform({{-15,0},{-15,-25},{-25,-35}});
    auto rightFin= transform({{15,0},{15,-25},{25,-35}});
    auto exhaust = transform({{-10,-30},{10,-30},{15,-40},{-15,-40}});
    auto exhaustBase = transform({{-15,-40},{15,-40}});

    scanlineFillPolygon(body,COLOR_WHITE);
    scanlineFillPolygon(nose,COLOR_RED);
    scanlineFillPolygon(leftFin,COLOR_RED);
    scanlineFillPolygon(rightFin,COLOR_RED);
    scanlineFillPolygon(exhaust, COLOR_DARKGREY);

    drawFlames(exhaustBase[0], exhaustBase[1], angle_deg, scale);
}

// PLANETS
void drawPlanets(){
    drawFilledCircleScanline(100,300,50,COLOR_BLUE);
    drawFilledCircleScanline(700,300,80,COLOR_RED);
}

// DISPLAY
void display(){
    glClear(GL_COLOR_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(cam_left, cam_right, cam_bottom, cam_top);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    drawStars();
    drawPlanets();
    float scale_factor = 1.0f - fabs(0.5f - t_param);
    drawRocket({(int)rocket_x, (int)rocket_y}, rocket_angle_deg, scale_factor);
    glutSwapBuffers();
}

//Rocket position update
void update(int value){
    Point start = {100,370}, peak={400,550}, end={700,400};
    if(t_param<1.0f){
        t_param += animation_speed;
        float t = t_param, u = 1-t;
        rocket_x = u*u*start.x + 2*u*t*peak.x + t*t*end.x;
        rocket_y = u*u*start.y + 2*u*t*peak.y + t*t*end.y;
        rocket_angle_deg = -360.0f*t*(1-t);
        cam_left = 50-50*t; cam_right = 150+650*t;
        cam_bottom = 250-250*t; cam_top = 450+150*t;
    } else {
        static float zoom_t = 0.0f;
        if(zoom_t<1.0f) zoom_t+=0.005f;
        cam_left = 600-50*(1-zoom_t);
        cam_right = 800+50*(1-zoom_t);
        cam_bottom = 200-50*(1-zoom_t);
        cam_top = 400+50*(1-zoom_t);
    }
    glutPostRedisplay();
    glutTimerFunc(16, update, 0);
}

void myInit(){ glClearColor(0,0,0,1); }

int main(int argc, char** argv){
    srand(time(0));
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(800,600);
    glutInitWindowPosition(100,100);
    glutCreateWindow("Interstellar 2.0");
    myInit();
    glutDisplayFunc(display);
    glutTimerFunc(25, update,0);
    glutMainLoop();
    return 0;
}
