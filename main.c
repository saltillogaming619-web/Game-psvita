// Open City Prototype - mini mundo abierto para PS Vita (VitaSDK + vitaGL)
// Controles: Cruz / R = acelerar | Cuadrado / L = frenar y marcha atras
//            Stick izquierdo o cruceta = girar | START = salir

#include <vitaGL.h>
#include <psp2/ctrl.h>
#include <psp2/kernel/processmgr.h>
#include <math.h>
#include <stdint.h>

#define SCREEN_W   960
#define SCREEN_H   544
#define GRID       5          // bloques de -GRID a +GRID en X y Z
#define SPACING    40.0f      // distancia entre centros de bloque
#define BLOCK_HALF 14.0f      // medio lado de cada manzana
#define CAR_RADIUS 1.3f
#define WORLD_LIM  (GRID * SPACING + 20.0f)
#define DEG        57.2957795f

typedef struct {
    float x, z;      // posicion
    float heading;   // radianes, 0 = hacia -Z
    float speed;     // unidades/segundo
} Car;

// ---------- utilidades ----------

static float rnd(int i, int j) {
    uint32_t h = (uint32_t)(i * 374761393) + (uint32_t)(j * 668265263);
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= h >> 16;
    return (h & 0xFFFF) / 65535.0f;
}

static float building_height(int i, int j) {
    return 6.0f + rnd(i, j) * 30.0f;
}

static void quad(float x1, float y1, float z1, float x2, float y2, float z2,
                 float x3, float y3, float z3, float x4, float y4, float z4) {
    glVertex3f(x1, y1, z1); glVertex3f(x2, y2, z2); glVertex3f(x3, y3, z3);
    glVertex3f(x1, y1, z1); glVertex3f(x3, y3, z3); glVertex3f(x4, y4, z4);
}

// Caja centrada en (cx,cy,cz) con medias dimensiones (hx,hy,hz)
static void draw_box(float cx, float cy, float cz,
                     float hx, float hy, float hz,
                     float r, float g, float b) {
    float x0 = cx - hx, x1 = cx + hx;
    float y0 = cy - hy, y1 = cy + hy;
    float z0 = cz - hz, z1 = cz + hz;

    glBegin(GL_TRIANGLES);
    glColor3f(r, g, b);                // arriba
    quad(x0, y1, z0,  x0, y1, z1,  x1, y1, z1,  x1, y1, z0);
    glColor3f(r * .8f, g * .8f, b * .8f);   // frente / atras
    quad(x0, y0, z1,  x1, y0, z1,  x1, y1, z1,  x0, y1, z1);
    quad(x1, y0, z0,  x0, y0, z0,  x0, y1, z0,  x1, y1, z0);
    glColor3f(r * .62f, g * .62f, b * .62f); // lados
    quad(x1, y0, z1,  x1, y0, z0,  x1, y1, z0,  x1, y1, z1);
    quad(x0, y0, z0,  x0, y0, z1,  x0, y1, z1,  x0, y1, z0);
    glEnd();
}

// ---------- mundo ----------

static void draw_world(void) {
    // asfalto
    glBegin(GL_TRIANGLES);
    glColor3f(0.18f, 0.18f, 0.20f);
    quad(-300, 0, -300,  -300, 0, 300,  300, 0, 300,  300, 0, -300);
    glEnd();

    for (int i = -GRID; i <= GRID; i++) {
        for (int j = -GRID; j <= GRID; j++) {
            float bx = i * SPACING, bz = j * SPACING;
            // acera
            draw_box(bx, 0.15f, bz, BLOCK_HALF, 0.15f, BLOCK_HALF, 0.55f, 0.55f, 0.55f);
            // edificio
            float h = building_height(i, j);
            float c = 0.35f + rnd(j, i) * 0.45f;
            float tint = rnd(i + 7, j + 3);
            draw_box(bx, 0.3f + h * 0.5f, bz, BLOCK_HALF - 3.0f, h * 0.5f, BLOCK_HALF - 3.0f,
                     c, c * (0.7f + tint * 0.3f), c * (0.6f + (1.0f - tint) * 0.4f));
        }
    }
}

static void draw_car(const Car *car) {
    glPushMatrix();
    glTranslatef(car->x, 0.0f, car->z);
    glRotatef(-car->heading * DEG, 0, 1, 0);
    draw_box(0, 0.65f, 0,     0.9f, 0.35f, 2.0f,  0.85f, 0.10f, 0.10f); // carroceria
    draw_box(0, 1.15f, 0.3f,  0.75f, 0.30f, 0.9f, 0.60f, 0.75f, 0.85f); // cabina
    // ruedas
    draw_box(-0.95f, 0.35f,  1.3f, 0.15f, 0.35f, 0.4f, 0.05f, 0.05f, 0.05f);
    draw_box( 0.95f, 0.35f,  1.3f, 0.15f, 0.35f, 0.4f, 0.05f, 0.05f, 0.05f);
    draw_box(-0.95f, 0.35f, -1.3f, 0.15f, 0.35f, 0.4f, 0.05f, 0.05f, 0.05f);
    draw_box( 0.95f, 0.35f, -1.3f, 0.15f, 0.35f, 0.4f, 0.05f, 0.05f, 0.05f);
    glPopMatrix();
}

static int hits_building(float x, float z) {
    int i = (int)roundf(x / SPACING);
    int j = (int)roundf(z / SPACING);
    if (i < -GRID || i > GRID || j < -GRID || j > GRID) return 0;
    float lim = BLOCK_HALF + CAR_RADIUS;
    return fabsf(x - i * SPACING) < lim && fabsf(z - j * SPACING) < lim;
}

// ---------- fisica del coche ----------

static void update_car(Car *car, const SceCtrlData *pad, float dt) {
    int gas   = (pad->buttons & (SCE_CTRL_CROSS | SCE_CTRL_RTRIGGER)) != 0;
    int brake = (pad->buttons & (SCE_CTRL_SQUARE | SCE_CTRL_LTRIGGER)) != 0;

    float steer = ((float)pad->lx - 128.0f) / 128.0f;
    if (fabsf(steer) < 0.15f) steer = 0.0f;
    if (pad->buttons & SCE_CTRL_LEFT)  steer = -1.0f;
    if (pad->buttons & SCE_CTRL_RIGHT) steer =  1.0f;

    if (gas)   car->speed += 18.0f * dt;
    if (brake) car->speed -= (car->speed > 0.0f ? 32.0f : 14.0f) * dt;
    car->speed -= car->speed * 0.6f * dt;                 // rozamiento
    if (car->speed >  40.0f) car->speed =  40.0f;
    if (car->speed < -10.0f) car->speed = -10.0f;

    // el giro depende de la velocidad (no gira parado)
    car->heading += steer * 1.8f * dt * car->speed / (fabsf(car->speed) + 8.0f);

    float nx = car->x + sinf(car->heading) * car->speed * dt;
    float nz = car->z - cosf(car->heading) * car->speed * dt;

    if (fabsf(nx) > WORLD_LIM || fabsf(nz) > WORLD_LIM || hits_building(nx, nz)) {
        car->speed *= -0.3f;       // choque: rebote
    } else {
        car->x = nx;
        car->z = nz;
    }
}

// ---------- camara y HUD ----------

static void setup_camera(const Car *car) {
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    float near = 0.5f, far = 400.0f;
    float top = near * 0.5774f;                      // fov vertical 60
    float right = top * (float)SCREEN_W / SCREEN_H;
    glFrustum(-right, right, -top, top, near, far);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    float fx = sinf(car->heading), fz = -cosf(car->heading);
    float cx = car->x - fx * 9.0f;
    float cz = car->z - fz * 9.0f;
    glRotatef(18.0f, 1, 0, 0);
    glRotatef(car->heading * DEG, 0, 1, 0);
    glTranslatef(-cx, -4.5f, -cz);
}

static void draw_hud(const Car *car) {
    glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, SCREEN_W, SCREEN_H, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    float frac = fabsf(car->speed) / 40.0f;
    glBegin(GL_TRIANGLES);
    glColor3f(0.0f, 0.0f, 0.0f);
    quad(30, 480, 0,  30, 510, 0,  270, 510, 0,  270, 480, 0);
    glColor3f(1.0f - frac * 0.5f, 0.4f + frac * 0.5f, 0.1f);
    quad(34, 484, 0,  34, 506, 0,  34 + 232 * frac, 506, 0,  34 + 232 * frac, 484, 0);
    glEnd();

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glEnable(GL_DEPTH_TEST);
}

// ---------- main ----------

int main(void) {
    vglInit(0x800000);
    glViewport(0, 0, SCREEN_W, SCREEN_H);
    glClearColor(0.45f, 0.65f, 0.90f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);

    Car car = { SPACING * 0.5f, SPACING * 0.5f, 0.0f, 0.0f };  // en un cruce
    uint64_t last = sceKernelGetProcessTimeWide();
    SceCtrlData pad;

    for (;;) {
        sceCtrlPeekBufferPositive(0, &pad, 1);
        if (pad.buttons & SCE_CTRL_START) break;

        uint64_t now = sceKernelGetProcessTimeWide();
        float dt = (float)(now - last) / 1000000.0f;
        last = now;
        if (dt > 0.05f) dt = 0.05f;

        update_car(&car, &pad, dt);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        setup_camera(&car);
        draw_world();
        draw_car(&car);
        draw_hud(&car);
        vglSwapBuffers(GL_FALSE);
    }

    vglEnd();
    sceKernelExitProcess(0);
    return 0;
}
