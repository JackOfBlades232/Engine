/* win32_main.cpp */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>
#include <cstddef>

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
};

static bool quit = false;
static int exit_code = 0;

static HBITMAP frame_bmp           = nullptr;
static frame_buffer_t frame_buffer = {};

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
                                        WS_EX_OVERLAPPEDWINDOW,
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
    bmi.bmiHeader.biHeight      = 720;
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
    frame_buffer.byte_size = frame_buffer.w * frame_buffer.h * sizeof(u32);

    // @TEST s
    //arr_set<u32>(frame_buffer.mem, 0xFF00FF, frame_buffer.w * frame_buffer.h);
    draw_gradient(&frame_buffer);

    ShowWindow(window_handle, cmdshow);
    UpdateWindow(window_handle);

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
    }

    return exit_code;
}
