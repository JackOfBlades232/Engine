/* win32_main.cpp */
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

typedef int bool;
#define true  1
#define false 0

static bool quit = false;
static int exit_code = 0;

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
        FillRect(ps.hdc, &ps.rcPaint, (HBRUSH)COLOR_WINDOW);
        EndPaint(h_wnd, &ps);
    } return 0;

    case WM_KEYDOWN:
        if (w_param == VK_ESCAPE) {
            quit = true;
            exit_code = 0;
        }
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

    WNDCLASS wc = {0};
    wc.style         = 0;
    wc.lpfnWndProc   = &win32_window_proc;
    wc.hInstance     = h_inst;
    wc.lpszClassName = app_name;
    RegisterClass(&wc);

    HWND window_handle =
        CreateWindowEx(0,
                       app_name,
                       app_name,
                       WS_EX_OVERLAPPEDWINDOW | WS_EX_APPWINDOW,
                       CW_USEDEFAULT,
                       CW_USEDEFAULT,
                       CW_USEDEFAULT,
                       CW_USEDEFAULT,
                       NULL,
                       NULL,
                       h_inst,
                       NULL);

    if (!window_handle) {
        // @TODO: log
        return 1;
    }

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
