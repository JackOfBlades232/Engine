/* win32_main.cpp */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>
#include <cstddef>

#include <cstdio>
#include <cstdlib>
#include <ctime>

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

        for (int y = 0; y < buf->h; ++y)
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

                buf->mem[y*buf->w + x] = 
                    ((u8)(r_sum * 64 / 257) << 16) |
                    ((u8)(g_sum * 64 / 257) << 8)  |
                    (u8)(b_sum * 64 / 257);
            }
    }

}

// @TODO: basic drawing & msg loop, quit on esc

LRESULT win32_window_proc(HWND h_wnd,
                          UINT u_msg,
                          WPARAM w_param,
                          LPARAM l_param)
{
    switch (u_msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(h_wnd, &ps);
        HDC bmp_hdc = CreateCompatibleDC(ps.hdc);
        HGDIOBJ old_obj = SelectObject(bmp_hdc, frame_bmp);
        BitBlt(ps.hdc,
               ps.rcPaint.left,
               ps.rcPaint.top,
               ps.rcPaint.right - ps.rcPaint.left,
               ps.rcPaint.bottom - ps.rcPaint.top,
               bmp_hdc,
               ps.rcPaint.left,
               ps.rcPaint.top,
               SRCCOPY);
        DeleteDC(bmp_hdc);
        EndPaint(h_wnd, &ps);
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

    HDC wnd_dc = GetDC(window_handle);
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = 720;
    bmi.bmiHeader.biHeight      = -720;
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    frame_bmp = CreateDIBSection(
        wnd_dc, &bmi, DIB_RGB_COLORS, (void **)&frame_buffer.mem, nullptr, 0);
    if (!frame_bmp) {
        // @TODO: log
        return 1;
    }

    frame_buffer.w = 720;
    frame_buffer.h = 720;
    frame_buffer.pixel_size = frame_buffer.w * frame_buffer.h;
    frame_buffer.byte_size = frame_buffer.pixel_size * sizeof(u32);

    arr_set<u32>(frame_buffer.mem, 0x0, frame_buffer.pixel_size);

    // @TEST: static img
    //draw_gradient(&frame_buffer);

    ShowWindow(window_handle, cmdshow);
    UpdateWindow(window_handle);

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

        // @TEST: animated img
        //draw_bouncing_square(&frame_buffer, dt);
        draw_fire(&frame_buffer, dt);

        RECT wnd_rect;
        GetWindowRect(window_handle, &wnd_rect);
        InvalidateRect(window_handle, &wnd_rect, false);
        UpdateWindow(window_handle);
    }

    return exit_code;
}
