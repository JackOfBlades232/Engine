/* win32_main.cpp */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>
#include <cstddef>

// @NOTE: candidates to be replaced with our functionality
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <cmath>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;
typedef int64_t  s64;
typedef float    f32;
typedef double   f64;

static constexpr float c_f_pi = 3.14159265359f;

struct frame_buffer_t {
    u32 *mem;
    int w, h;
    int byte_size;
    int pixel_size;
};

static bool quit = false;
static int exit_code = 0;

static HBITMAP frame_bmp           = nullptr;
static frame_buffer_t frame_buffer = {};

#define ARR_SIZE(_arr) (sizeof(_arr) / sizeof((_arr)[0]))

#define MIN(_a, _b) ((_a) < (_b) ? (_a) : (_b))
#define MAX(_a, _b) ((_a) > (_b) ? (_a) : (_b))
// @HUH: is this sus for non-int literals, and would a template be better?
#define SGN(_x) ((_x) < 0 ? -1 : ((_x) > 0 ? 1 : 0))
#define ABS(_x) ((_x) < 0 ? -(_x) : (_x))

template <class T>
void arr_set(T *arr, T val, size_t arr_size)
{
    for (T *end = arr + arr_size; arr != end; *(arr++) = val) ;
}

void draw_gradient(frame_buffer_t *buf)
{
    for (int y = 0; y < buf->h; ++y)
        for (int x = 0; x < buf->w; ++x) {
            buf->mem[y*buf->w + x] = ((u32)(255.f * (float)x / buf->w) << 16) |
                                     ((u32)(255.f * (float)y / buf->h) << 8);
        }
}

void draw_bouncing_square(frame_buffer_t *buf, float dt)
{
    const float meters_per_scr_h = 5.f;
    const float cube_size        = 0.2f;
    const float init_x_vel       = 0.4f;
    const float grav             = 9.81f * 0.1f;
    const float friction         = 2.5f;
    const float absorption       = 0.5f;
    const float ground_y         = 4.f;
    const float fixed_dt         = 1.f/60.f;

    static float x = 0.f, y = 0.f;
    static float vel_x = init_x_vel, vel_y = 0.f;
    static float accum_dt = 0.f;

    accum_dt += dt;
    while (accum_dt >= fixed_dt) {
        accum_dt -= fixed_dt;

        // Simulate
        vel_y += grav * fixed_dt;

        if (y >= ground_y - cube_size) {
            y = ground_y - cube_size;
            vel_y = -vel_y * (1.f - absorption);

            float d_from_friction = MIN(friction * fixed_dt, ABS(vel_x));
            vel_x -= SGN(vel_x) * d_from_friction;
        }

        x += vel_x * fixed_dt;
        y += vel_y * fixed_dt;
    }

    arr_set<u32>(frame_buffer.mem, 0x0, frame_buffer.pixel_size);

    float pix_per_meter = (float)buf->h / meters_per_scr_h;

    // Draw ground
    u32 *pixel = &buf->mem[(int)(ground_y * pix_per_meter - 1)*buf->w];
    for (int draw_x = 0; draw_x < buf->w; ++draw_x)
        *(pixel++) = 0xFFFFFF;

    // Draw cube
    int cube_size_pix = (int)(cube_size * pix_per_meter);

    int min_x = (int)(x * pix_per_meter);
    int min_y = (int)(y * pix_per_meter);
    int max_x = min_x + cube_size_pix;
    int max_y = min_y + cube_size_pix;

    min_x = MAX(0, min_x);
    min_y = MAX(0, min_y);
    max_x = MIN(buf->w-1, max_x);
    max_y = MIN(buf->h-1, max_y);

    // @TODO: contiguous color blend
    u32 cube_col = (0xFF << 16) | ((u8)((int)(x * 255.f)) << 8) | (u8)((int)(y * 255.f));

    pixel = &buf->mem[min_y*buf->w + min_x];
    for (int draw_y = min_y; draw_y < max_y; ++draw_y) {
        for (int draw_x = min_x; draw_x < max_x; ++draw_x)
            *(pixel++) = cube_col;

        pixel += buf->w - (max_x - min_x);
    }
}

void draw_fire(frame_buffer_t *buf, float dt)
{
    const float pix_per_sec = 100.f;
    const float step_dt     = 1.f/60.f;

    static u32 palette[256];
    static bool palette_inited = false;
    static float accum_dt = 0.f;

    if (!palette_inited) {
        for (int i = 0; i < ARR_SIZE(palette); ++i) {
            int hue = i / 3;
            int saturation = 255;
            int lightness = MIN(255, i * 2);

            u8 r = (u8)lightness;
            u8 g = (u8)((float)lightness * hue / 85.f);
            u8 b = (u8)0;

            palette[i] = (r << 16) | (g << 8) | b;
        }

        palette_inited = true;
    }

    accum_dt += dt;

    while (accum_dt >= step_dt) {
        accum_dt -= step_dt;

        for (int x = 0; x < buf->w; ++x)
            buf->mem[(buf->h - 1)*buf->w + x] = palette[(int)(((float)rand()/RAND_MAX) * 255.f)];

        u32 *pixel = buf->mem;
        for (int y = 0; y < buf->h-1; ++y)
            for (int x = 0; x < buf->w; ++x) {
                int xm1 = MAX(x - 1, 0);
                int xp1 = MIN(x + 1, buf->w-1);
                int yp1 = MIN(y + 1, buf->h-1);
                int yp2 = MIN(y + 2, buf->h-1);

                u32 p1m1 = buf->mem[yp1*buf->w + xm1];
                u32 p1   = buf->mem[yp1*buf->w + x];
                u32 p1p1 = buf->mem[yp1*buf->w + xp1];
                u32 p2   = buf->mem[yp2*buf->w + x];

                u32 r_sum = (p1m1 >> 16) + (p1 >> 16) + (p1p1 >> 16) + (p2 >> 16);
                u32 g_sum = ((p1m1 >> 8) & 0xFF) + ((p1 >> 8) & 0xFF) + ((p1p1 >> 8) & 0xFF) + ((p2 >> 8) & 0xFF);
                u32 b_sum = (p1m1 & 0xFF) + (p1 & 0xFF) + (p1p1 & 0xFF) + (p2 & 0xFF);

                *(pixel++) = 
                    ((u8)(r_sum * 64 / 257) << 16) |
                    ((u8)(g_sum * 64 / 257) << 8)  |
                    (u8)(b_sum * 64 / 257);
            }
    }
}

void draw_flower_of_life(frame_buffer_t *buf, float dt)
{
    const float grid_scale        = 5.f;
    const float ang_vel           = c_f_pi / 9.f;
    const float two_over_sqrt3    = 1.15470053838f;
    const float sqrt3_over_two    = 1.f / two_over_sqrt3;
    const float minus1_over_sqrt3 = -two_over_sqrt3 * 0.5f;

    static float angle = 0.f;
    angle += dt * ang_vel;
    float sine   = sinf(angle);
    float cosine = cosf(angle);

    float res_coeff = grid_scale / buf->h;

    arr_set<u32>(frame_buffer.mem, 0x0, frame_buffer.pixel_size);

    // @SPEED: this is slow, and unoptimized

    u32 *pixel = buf->mem;
    for (int pix_y = 0; pix_y < buf->h; ++pix_y)
        for (int pix_x = 0; pix_x < buf->w; ++pix_x) {
            float base_x = 2.f * ((float)pix_x * res_coeff - 0.5f * buf->w * res_coeff);
            float base_y = 2.f * ((float)pix_y * res_coeff - 0.5f * grid_scale);

            float r = sqrtf(base_x*base_x + base_y*base_y);
            float r_int;
            modf(r, &r_int);

            const u32 colors[] = { 0xFFFF00, 0x00FF00, 0xFF00FF, 0xFF0000, 0x00FFFF, 0x0000FF };
            u32 col = colors[(int)r_int % ARR_SIZE(colors)];
            bool reverse_rotation = (int)r_int % 2 == 1;
            
            // Rotate
            float x = cosine * base_x - sine * base_y * (reverse_rotation ? -1.f : 1.f);
            float y = sine * base_x * (reverse_rotation ? -1.f : 1.f) + cosine * base_y;

            // Transform coords to <(sqrt(3)/2, 1/2), (0, 1)> basis
            float transf_x = two_over_sqrt3 * x;
            float transf_y = minus1_over_sqrt3 * x + y;

            float grid_x, grid_y;
            float frac_x = modf(transf_x, &grid_x);
            float frac_y = modf(transf_y, &grid_y);
            if (transf_x < 0.f) {
                grid_x -= 1.f;
                frac_x += 1.f;
            }
            if (transf_y < 0.f) {
                grid_y -= 1.f;
                frac_y += 1.f;
            }

            bool is_bottom_tri = frac_x + frac_y <= 1.f;
            float grid_x_dec = sqrt3_over_two * grid_x;
            float grid_y_dec = 0.5f*grid_x + grid_y;

            float d1_x, d1_y;
            if (is_bottom_tri) {
                d1_x = grid_x_dec + sqrt3_over_two - x;
                d1_y = grid_y_dec + 1.5f - y;
            } else {
                d1_x = grid_x_dec - x;
                d1_y = grid_y_dec - y;
            }
            float d1 = d1_x*d1_x + d1_y*d1_y;
            if (d1 < 0.92f) {
                *(pixel++) = col; 
                continue;
            } else if (d1 < 1.f) {
                *(pixel++) = 0xFFFFFF; 
                continue;
            }

            float d2_x, d2_y;
            if (is_bottom_tri) {
                d2_x = grid_x_dec + sqrt3_over_two - x;
                d2_y = grid_y_dec - 0.5f - y;
            } else {
                d2_x = grid_x_dec + 2.f*sqrt3_over_two - x;
                d2_y = grid_y_dec + 1.f - y;
            }
            float d2 = d2_x*d2_x + d2_y*d2_y;
            if (d2 < 0.92f) {
                *(pixel++) = col; 
                continue;
            } else if (d2 < 1.f) {
                *(pixel++) = 0xFFFFFF; 
                continue;
            }

            float d3_x, d3_y;
            if (is_bottom_tri) {
                d3_x = grid_x_dec - sqrt3_over_two - x;
                d3_y = grid_y_dec + 0.5f - y;
            } else {
                d3_x = grid_x_dec - x;
                d3_y = grid_y_dec + 2.f - y;
            }
            float d3 = d3_x*d3_x + d3_y*d3_y;
            if (d3 < 0.92f) {
                *(pixel++) = col; 
                continue;
            } else if (d3 < 1.f) {
                *(pixel++) = 0xFFFFFF; 
                continue;
            }

            ++pixel;
        }
}

// @TODO: interactive test, main keys read

void win32_init_frame_buffer(frame_buffer_t *buf, HBITMAP *bmp,
                             int w, int h,
                             HWND h_wnd)
{
    HDC wnd_dc = GetDC(h_wnd);
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = w;
    bmi.bmiHeader.biHeight      = -h;
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    *bmp = CreateDIBSection(
        wnd_dc, &bmi, DIB_RGB_COLORS, (void **)&buf->mem, nullptr, 0);
    ReleaseDC(h_wnd, wnd_dc);
    if (!*bmp) {
        // @TODO: log & abort
        return;
    }

    buf->w = w;
    buf->h = h;
    buf->pixel_size = buf->w * buf->h;
    buf->byte_size = buf->pixel_size * sizeof(u32);

    arr_set<u32>(buf->mem, 0x0, buf->pixel_size);
}

LRESULT win32_window_proc(HWND h_wnd,
                          UINT u_msg,
                          WPARAM w_param,
                          LPARAM l_param)
{
    switch (u_msg) {
    case WM_SIZE: {
        UINT w = LOWORD(l_param);
        UINT h = HIWORD(l_param);
        if (w != frame_buffer.w || h != frame_buffer.h) {
            DeleteObject(frame_bmp);
            win32_init_frame_buffer(&frame_buffer, &frame_bmp, w, h, h_wnd);
        }
    } return 0;

    case WM_KEYDOWN:
        if (w_param == VK_ESCAPE) {
            quit = true;
            exit_code = 0;
        }
        return 0;

    case WM_CLOSE:
        quit = true;
        exit_code = 0;
        return 0;
    
    default:
        return DefWindowProc(h_wnd, u_msg, w_param, l_param);
    }
}

int APIENTRY WinMain(HINSTANCE h_inst, 
                     HINSTANCE h_inst_prev, 
                     PSTR cmdline, 
                     int cmdshow)
{
    // @TODO: functions for random
    srand(time(nullptr));

    const char app_name[] = "My engine";

    WNDCLASS wc = {};
    wc.style         = CS_OWNDC;
    wc.lpfnWndProc   = &win32_window_proc;
    wc.hInstance     = h_inst;
    wc.lpszClassName = app_name;
    RegisterClass(&wc);

    HWND window_handle = CreateWindowEx(0,
                                        app_name,
                                        app_name,
                                        WS_OVERLAPPEDWINDOW,
                                        0,
                                        0,
                                        720,
                                        720,
                                        nullptr,
                                        nullptr,
                                        h_inst,
                                        nullptr);
    if (!window_handle) {
        // @TODO: log
        return 1;
    }

    ShowWindow(window_handle, cmdshow);

    LARGE_INTEGER qpc_freq;
    QueryPerformanceFrequency(&qpc_freq);

    LARGE_INTEGER cur_qpc_counts;
    LARGE_INTEGER prev_qpc_counts;
    QueryPerformanceCounter(&cur_qpc_counts);
    prev_qpc_counts = cur_qpc_counts;

    for (;;) {
        MSG msg;
        int msg_res;
        while ((msg_res = PeekMessage(&msg, window_handle, 0, 0, PM_REMOVE)) != 0) {
            if (msg_res == -1) {
                // @TODO: log/deal with
                return 2;
            } else {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }

        if (quit)
            break;

        QueryPerformanceCounter(&cur_qpc_counts);
        u64 d_qpc_counts = cur_qpc_counts.QuadPart - prev_qpc_counts.QuadPart;
        prev_qpc_counts = cur_qpc_counts;
        float dt = (float)d_qpc_counts / qpc_freq.QuadPart;

        // @TODO: debug logging funcs
        char buf[64];
        snprintf(buf, sizeof(buf), "%.2fms per frame, %.2f fps\n", dt * 1e3, 1.f / dt);
        OutputDebugStringA(buf);

        // @TEST: static img
        //draw_gradient(&frame_buffer);

        // @TEST: animated img
        //draw_bouncing_square(&frame_buffer, dt);
        //draw_fire(&frame_buffer, dt);
        draw_flower_of_life(&frame_buffer, dt);

        RECT wnd_rect;
        GetClientRect(window_handle, &wnd_rect);
        HDC wnd_dc = GetDC(window_handle);
        HDC bmp_dc = CreateCompatibleDC(wnd_dc);
        SelectObject(bmp_dc, frame_bmp);
        BitBlt(wnd_dc,
               wnd_rect.left,
               wnd_rect.top,
               wnd_rect.right - wnd_rect.left,
               wnd_rect.bottom - wnd_rect.top,
               bmp_dc,
               wnd_rect.left,
               wnd_rect.top,
               SRCCOPY);
        DeleteDC(bmp_dc);
        ReleaseDC(window_handle, wnd_dc);
    }

    return exit_code;
}
