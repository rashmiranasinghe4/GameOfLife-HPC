
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dwmapi.h>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <vector>
#include <string>
#include <algorithm>
#include <chrono>

#pragma comment(lib, "dwmapi.lib")

constexpr int MAX_GENERATIONS = 100;
constexpr UINT_PTR TIMER_ID = 1;
constexpr UINT TIMER_INTERVAL_MS = 30;

constexpr COLORREF BG = RGB(15, 19, 30);
constexpr COLORREF PANEL = RGB(25, 32, 47);
constexpr COLORREF PANEL_LIGHT = RGB(33, 42, 59);
constexpr COLORREF TEXT_COLOR = RGB(238, 243, 250);
constexpr COLORREF MUTED = RGB(151, 165, 187);
constexpr COLORREF GREEN = RGB(49, 220, 155);
constexpr COLORREF BLUE = RGB(85, 157, 255);
constexpr COLORREF RED = RGB(239, 100, 119);
constexpr COLORREF BORDER = RGB(53, 67, 89);

constexpr DWORD DIB_BACKGROUND_PIXEL =
    (24u << 16) | (27u << 8) | 34u;

constexpr DWORD DIB_ALIVE_PIXEL =
    (0u << 16) | (220u << 8) | 140u;

constexpr int ID_START = 1001;
constexpr int ID_PAUSE = 1002;
constexpr int ID_RESET = 1003;
constexpr int ID_GRID_SIZE = 1004;
constexpr int ID_APPLY_GRID = 1005;

int GRID_SIZE = 1000;
int TOTAL_CELLS = 1000000;

int client_width = 1400;
int client_height = 950;

int left_panel_width = 310;
int right_panel_x = 350;
int grid_x = 370;
int grid_y = 155;
int display_size = 600;

std::vector<unsigned char> grid;
std::vector<unsigned char> nextGrid;
DWORD* display_pixels = nullptr;

HWND hwndMain = nullptr;
HWND hwndStart = nullptr;
HWND hwndPause = nullptr;
HWND hwndReset = nullptr;
HWND hwndGridSize = nullptr;
HWND hwndApplyGrid = nullptr;

HFONT control_font = nullptr;

int generation = 0;
int alive_cells = 0;

double total_cpu_ms = 0.0;

bool simulation_running = false;

void DrawTextStyled(
    HDC hdc,
    const char* text,
    int x,
    int y,
    COLORREF color,
    int size,
    int weight = FW_NORMAL)
{
    HFONT font = CreateFontA(
        size, 0, 0, 0, weight,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        "Segoe UI");

    HFONT old = static_cast<HFONT>(
        SelectObject(hdc, font));

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);

    TextOutA(
        hdc, x, y, text,
        static_cast<int>(strlen(text)));

    SelectObject(hdc, old);
    DeleteObject(font);
}

void DrawPanel(
    HDC hdc,
    int x,
    int y,
    int width,
    int height,
    COLORREF fill,
    COLORREF border)
{
    HPEN pen = CreatePen(PS_SOLID, 1, border);
    HBRUSH brush = CreateSolidBrush(fill);

    HPEN old_pen = static_cast<HPEN>(
        SelectObject(hdc, pen));

    HBRUSH old_brush = static_cast<HBRUSH>(
        SelectObject(hdc, brush));

    RoundRect(
        hdc, x, y,
        x + width, y + height,
        18, 18);

    SelectObject(hdc, old_pen);
    SelectObject(hdc, old_brush);

    DeleteObject(pen);
    DeleteObject(brush);
}

void DrawMetricCard(
    HDC hdc,
    int x,
    int y,
    int width,
    int height,
    const char* label,
    const char* value,
    COLORREF accent)
{
    DrawPanel(
        hdc, x, y, width, height,
        PANEL_LIGHT, BORDER);

    HBRUSH brush = CreateSolidBrush(accent);

    RECT accent_rect = {
        x + 12, y + 12,
        x + 16, y + height - 12
    };

    FillRect(hdc, &accent_rect, brush);
    DeleteObject(brush);

    DrawTextStyled(
        hdc, label,
        x + 27, y + 8,
        MUTED, 11);

    DrawTextStyled(
        hdc, value,
        x + 27, y + 27,
        TEXT_COLOR, 17, FW_BOLD);
}

void UpdateLayout()
{
    RECT rect;
    GetClientRect(hwndMain, &rect);

    client_width = rect.right - rect.left;
    client_height = rect.bottom - rect.top;

    left_panel_width = 310;
    right_panel_x = 350;

    int right_width = client_width - right_panel_x - 20;

    int available_width = right_width - 40;
    int available_height = client_height - 235;

    display_size = std::min(
        available_width,
        available_height);

    if (display_size > 1000)
        display_size = 1000;

    if (display_size < 200)
        display_size = 200;

    grid_x = right_panel_x +
        (right_width - display_size) / 2;

    grid_y = 155;

    if (hwndGridSize)
        MoveWindow(
            hwndGridSize,
            45, 165,
            left_panel_width - 90, 180,
            TRUE);

    if (hwndApplyGrid)
        MoveWindow(
            hwndApplyGrid,
            45, 210,
            left_panel_width - 90, 38,
            TRUE);

    int button_width = (left_panel_width - 70) / 3;

    if (hwndStart)
        MoveWindow(
            hwndStart, 35, 263,
            button_width, 38, TRUE);

    if (hwndPause)
        MoveWindow(
            hwndPause,
            40 + button_width, 263,
            button_width, 38, TRUE);

    if (hwndReset)
        MoveWindow(
            hwndReset,
            45 + button_width * 2, 263,
            button_width, 38, TRUE);

    DWORD* resized_pixels = new (std::nothrow) DWORD[
        static_cast<size_t>(display_size) * display_size];

    if (resized_pixels)
    {
        delete[] display_pixels;
        display_pixels = resized_pixels;
    }
}

int CountAliveCells()
{
    int count = 0;

    for (int i = 0; i < TOTAL_CELLS; i++)
        count += grid[i] == 1;

    return count;
}

void GenerateRandomGrid()
{
    for (int i = 0; i < TOTAL_CELLS; i++)
        grid[i] = (rand() % 100 < 22) ? 1 : 0;

    alive_cells = CountAliveCells();
}

bool AllocateGridMemory()
{
    try
    {
        grid.assign(TOTAL_CELLS, 0);
        nextGrid.assign(TOTAL_CELLS, 0);
    }
    catch (...)
    {
        MessageBoxA(
            hwndMain,
            "Unable to allocate CPU grid memory.",
            "Memory Error",
            MB_OK | MB_ICONERROR);

        grid.clear();
        nextGrid.clear();
        return false;
    }

    return true;
}

void UpdateDisplayPixels()
{
    if (!display_pixels || grid.empty())
        return;

    for (int y = 0; y < display_size; y++)
    {
        int source_y = y * GRID_SIZE / display_size;

        for (int x = 0; x < display_size; x++)
        {
            int source_x = x * GRID_SIZE / display_size;

            int source_index =
                source_y * GRID_SIZE + source_x;

            int display_index =
                y * display_size + x;

            display_pixels[display_index] =
                grid[source_index]
                ? DIB_ALIVE_PIXEL
                : DIB_BACKGROUND_PIXEL;
        }
    }
}

void DrawGrid(HDC hdc)
{
    if (!display_pixels || grid.empty())
        return;

    UpdateDisplayPixels();

    BITMAPINFO bitmap_info = {};

    bitmap_info.bmiHeader.biSize =
        sizeof(BITMAPINFOHEADER);

    bitmap_info.bmiHeader.biWidth = display_size;
    bitmap_info.bmiHeader.biHeight = -display_size;
    bitmap_info.bmiHeader.biPlanes = 1;
    bitmap_info.bmiHeader.biBitCount = 32;
    bitmap_info.bmiHeader.biCompression = BI_RGB;

    StretchDIBits(
        hdc,
        grid_x, grid_y,
        display_size, display_size,
        0, 0,
        display_size, display_size,
        display_pixels,
        &bitmap_info,
        DIB_RGB_COLORS,
        SRCCOPY);

    HPEN pen = CreatePen(PS_SOLID, 1, BORDER);
    HPEN old_pen = static_cast<HPEN>(
        SelectObject(hdc, pen));

    HBRUSH old_brush = static_cast<HBRUSH>(
        SelectObject(hdc, GetStockObject(HOLLOW_BRUSH)));

    Rectangle(
        hdc,
        grid_x - 1, grid_y - 1,
        grid_x + display_size + 1,
        grid_y + display_size + 1);

    SelectObject(hdc, old_brush);
    SelectObject(hdc, old_pen);

    DeleteObject(pen);
}

void DrawInterface(HDC hdc)
{
    RECT rect;
    GetClientRect(hwndMain, &rect);

    HBRUSH background = CreateSolidBrush(BG);
    FillRect(hdc, &rect, background);
    DeleteObject(background);

    DrawTextStyled(
        hdc,
        "CPU GAME OF LIFE",
        24, 18, TEXT_COLOR, 26, FW_BOLD);

    DrawTextStyled(
        hdc,
        "HIGH PERFORMANCE COMPUTING  |  GROUP 13",
        27, 55, MUTED, 12);

    DrawPanel(
        hdc, 20, 90,
        left_panel_width, client_height - 110,
        PANEL, BORDER);

    DrawPanel(
        hdc,
        right_panel_x, 90,
        client_width - right_panel_x - 20,
        client_height - 110,
        PANEL, BORDER);

    DrawTextStyled(
        hdc, "SIMULATION CONTROLS",
        40, 110, TEXT_COLOR, 16, FW_BOLD);

    DrawTextStyled(
        hdc, "Choose grid size",
        45, 143, MUTED, 12);

    DrawTextStyled(
        hdc, "PERFORMANCE",
        40, 325, TEXT_COLOR, 16, FW_BOLD);

    char value[128];

    sprintf_s(
        value, sizeof(value),
        "%d x %d", GRID_SIZE, GRID_SIZE);

    DrawMetricCard(
        hdc, 35, 355, left_panel_width - 30, 55,
        "GRID DIMENSIONS", value, BLUE);

    sprintf_s(
        value, sizeof(value),
        "%d / %d", generation, MAX_GENERATIONS);

    DrawMetricCard(
        hdc, 35, 420, left_panel_width - 30, 55,
        "GENERATIONS COMPLETED", value, GREEN);

    sprintf_s(
        value, sizeof(value),
        "%d", alive_cells);

    DrawMetricCard(
        hdc, 35, 485, left_panel_width - 30, 55,
        "LIVING CELLS", value, GREEN);

    sprintf_s(
        value, sizeof(value),
        "%.3f ms", total_cpu_ms);

    DrawMetricCard(
        hdc, 35, 550, left_panel_width - 30, 55,
        "ACCUMULATED CPU TIME", value, BLUE);

    sprintf_s(
        value, sizeof(value),
        "%d", TOTAL_CELLS);

    DrawMetricCard(
        hdc, 35, 615, 115, 55,
        "TOTAL CELLS", value, BLUE);

    DrawMetricCard(
        hdc, 160, 615, left_panel_width - 155, 55,
        "CPU WORKERS", "1", GREEN);

    DrawTextStyled(
        hdc, "LIVE SIMULATION",
        right_panel_x + 25, 108,
        TEXT_COLOR, 17, FW_BOLD);

    DrawTextStyled(
        hdc,
        "Visualization is scaled to fit the window",
        right_panel_x + 25, 130,
        MUTED, 11);

    DrawGrid(hdc);

    const char* status = "READY";

    if (simulation_running)
        status = "RUNNING";
    else if (generation >= MAX_GENERATIONS)
        status = "COMPLETED";
    else if (generation > 0)
        status = "PAUSED";

    DrawTextStyled(
        hdc, "STATUS",
        right_panel_x + 25,
        grid_y + display_size + 12,
        MUTED, 11);

    DrawTextStyled(
        hdc, status,
        right_panel_x + 90,
        grid_y + display_size + 12,
        simulation_running ? GREEN : MUTED,
        12, FW_BOLD);
}

bool RunOneGeneration()
{
    if (generation >= MAX_GENERATIONS ||
        grid.empty() || nextGrid.empty())
        return false;

    const auto start_time =
        std::chrono::steady_clock::now();

    for (int y = 0; y < GRID_SIZE; y++)
    {
        int above = (y == 0) ? GRID_SIZE - 1 : y - 1;
        int below = (y == GRID_SIZE - 1) ? 0 : y + 1;

        for (int x = 0; x < GRID_SIZE; x++)
        {
            int left = (x == 0) ? GRID_SIZE - 1 : x - 1;
            int right = (x == GRID_SIZE - 1) ? 0 : x + 1;

            int neighbors = 0;

            neighbors += grid[above * GRID_SIZE + left];
            neighbors += grid[above * GRID_SIZE + x];
            neighbors += grid[above * GRID_SIZE + right];

            neighbors += grid[y * GRID_SIZE + left];
            neighbors += grid[y * GRID_SIZE + right];

            neighbors += grid[below * GRID_SIZE + left];
            neighbors += grid[below * GRID_SIZE + x];
            neighbors += grid[below * GRID_SIZE + right];

            int index = y * GRID_SIZE + x;
            unsigned char cell = grid[index];

            if (cell)
            {
                nextGrid[index] =
                    (neighbors == 2 || neighbors == 3) ? 1 : 0;
            }
            else
            {
                nextGrid[index] = (neighbors == 3) ? 1 : 0;
            }
        }
    }

    const auto end_time =
        std::chrono::steady_clock::now();

    total_cpu_ms +=
        std::chrono::duration<double, std::milli>(
            end_time - start_time).count();

    grid.swap(nextGrid);

    alive_cells = CountAliveCells();
    generation++;

    return true;
}

bool ApplyGridSize(int selected_size)
{
    simulation_running = false;
    KillTimer(hwndMain, TIMER_ID);

    GRID_SIZE = selected_size;
    TOTAL_CELLS = GRID_SIZE * GRID_SIZE;

    generation = 0;
    total_cpu_ms = 0.0;
    alive_cells = 0;

    if (!AllocateGridMemory())
    {
        InvalidateRect(hwndMain, nullptr, TRUE);
        return false;
    }

    GenerateRandomGrid();

    SetWindowTextA(hwndStart, "Start");
    EnableWindow(hwndStart, TRUE);

    InvalidateRect(hwndMain, nullptr, TRUE);
    return true;
}

void ResetSimulation()
{
    simulation_running = false;
    KillTimer(hwndMain, TIMER_ID);

    if (grid.empty())
        return;

    GenerateRandomGrid();

    generation = 0;
    total_cpu_ms = 0.0;

    SetWindowTextA(hwndStart, "Start");
    EnableWindow(hwndStart, TRUE);

    InvalidateRect(hwndMain, nullptr, TRUE);
}

void ReleaseResources()
{
    delete[] display_pixels;
    display_pixels = nullptr;

    grid.clear();
    nextGrid.clear();

    if (control_font)
    {
        DeleteObject(control_font);
        control_font = nullptr;
    }
}

void DrawOwnerButton(const DRAWITEMSTRUCT* item)
{
    HDC hdc = item->hDC;
    RECT rect = item->rcItem;

    int id = static_cast<int>(item->CtlID);

    COLORREF fill = PANEL_LIGHT;

    if (id == ID_START || id == ID_APPLY_GRID)
        fill = RGB(24, 145, 103);
    else if (id == ID_PAUSE)
        fill = RGB(49, 87, 150);
    else if (id == ID_RESET)
        fill = RGB(133, 56, 69);

    if (item->itemState & ODS_DISABLED)
        fill = RGB(60, 65, 78);

    if (item->itemState & ODS_SELECTED)
        fill = RGB(65, 78, 100);

    HPEN pen = CreatePen(PS_SOLID, 1, fill);
    HBRUSH brush = CreateSolidBrush(fill);

    HPEN old_pen = static_cast<HPEN>(
        SelectObject(hdc, pen));

    HBRUSH old_brush = static_cast<HBRUSH>(
        SelectObject(hdc, brush));

    RoundRect(
        hdc,
        rect.left + 1, rect.top + 1,
        rect.right - 1, rect.bottom - 1,
        10, 10);

    SelectObject(hdc, old_pen);
    SelectObject(hdc, old_brush);

    DeleteObject(pen);
    DeleteObject(brush);

    char label[128] = {};

    GetWindowTextA(
        item->hwndItem,
        label,
        static_cast<int>(sizeof(label)));

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, TEXT_COLOR);

    HFONT font = CreateFontA(
        13, 0, 0, 0, FW_BOLD,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        "Segoe UI");

    HFONT old_font = static_cast<HFONT>(
        SelectObject(hdc, font));

    DrawTextA(
        hdc, label, -1, &rect,
        DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdc, old_font);
    DeleteObject(font);
}

LRESULT CALLBACK WindowProcedure(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        hwndMain = hwnd;

        hwndGridSize = CreateWindowA(
            "COMBOBOX", "",
            WS_VISIBLE | WS_CHILD |
            CBS_DROPDOWNLIST | WS_VSCROLL,
            45, 165, 220, 180,
            hwnd,
            reinterpret_cast<HMENU>(ID_GRID_SIZE),
            nullptr, nullptr);

        SendMessageA(
            hwndGridSize, CB_ADDSTRING, 0,
            reinterpret_cast<LPARAM>("1000 x 1000"));

        SendMessageA(
            hwndGridSize, CB_ADDSTRING, 0,
            reinterpret_cast<LPARAM>("500 x 500"));

        SendMessageA(
            hwndGridSize, CB_ADDSTRING, 0,
            reinterpret_cast<LPARAM>("250 x 250"));

        SendMessageA(
            hwndGridSize, CB_SETCURSEL, 0, 0);

        hwndApplyGrid = CreateWindowA(
            "BUTTON", "Apply Grid",
            WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
            45, 210, 220, 38,
            hwnd,
            reinterpret_cast<HMENU>(ID_APPLY_GRID),
            nullptr, nullptr);

        hwndStart = CreateWindowA(
            "BUTTON", "Start",
            WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
            35, 263, 80, 38,
            hwnd,
            reinterpret_cast<HMENU>(ID_START),
            nullptr, nullptr);

        hwndPause = CreateWindowA(
            "BUTTON", "Pause",
            WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
            125, 263, 80, 38,
            hwnd,
            reinterpret_cast<HMENU>(ID_PAUSE),
            nullptr, nullptr);

        hwndReset = CreateWindowA(
            "BUTTON", "Reset",
            WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
            215, 263, 80, 38,
            hwnd,
            reinterpret_cast<HMENU>(ID_RESET),
            nullptr, nullptr);

        control_font = CreateFontA(
            14, 0, 0, 0, FW_NORMAL,
            FALSE, FALSE, FALSE,
            DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE,
            "Segoe UI");

        SendMessageA(
            hwndGridSize, WM_SETFONT,
            reinterpret_cast<WPARAM>(control_font), TRUE);

        if (!AllocateGridMemory())
            return -1;

        srand(static_cast<unsigned int>(time(nullptr)));
        GenerateRandomGrid();

        return 0;
    }

    case WM_SIZE:
        UpdateLayout();
        InvalidateRect(hwnd, nullptr, TRUE);
        return 0;

    case WM_DRAWITEM:
        DrawOwnerButton(
            reinterpret_cast<const DRAWITEMSTRUCT*>(lParam));
        return TRUE;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case ID_START:
            if (!simulation_running &&
                generation < MAX_GENERATIONS)
            {
                simulation_running = true;

                SetTimer(
                    hwndMain,
                    TIMER_ID,
                    TIMER_INTERVAL_MS,
                    nullptr);

                InvalidateRect(hwndMain, nullptr, TRUE);
            }
            break;

        case ID_PAUSE:
            simulation_running = false;
            KillTimer(hwndMain, TIMER_ID);
            InvalidateRect(hwndMain, nullptr, TRUE);
            break;

        case ID_RESET:
            ResetSimulation();
            break;

        case ID_APPLY_GRID:
        {
            int selection = static_cast<int>(
                SendMessageA(
                    hwndGridSize,
                    CB_GETCURSEL,
                    0, 0));

            int selected_size = 1000;

            if (selection == 1)
                selected_size = 500;
            else if (selection == 2)
                selected_size = 250;

            ApplyGridSize(selected_size);
            break;
        }
        }

        return 0;

    case WM_TIMER:
        if (wParam == TIMER_ID && simulation_running)
        {
            if (!RunOneGeneration() ||
                generation >= MAX_GENERATIONS)
            {
                simulation_running = false;
                KillTimer(hwndMain, TIMER_ID);
            }

            InvalidateRect(hwndMain, nullptr, TRUE);
        }

        return 0;

    case WM_PAINT:
    {
        PAINTSTRUCT paint;
        HDC hdc = BeginPaint(hwnd, &paint);

        DrawInterface(hdc);

        EndPaint(hwnd, &paint);
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_DESTROY:
        simulation_running = false;
        KillTimer(hwndMain, TIMER_ID);
        ReleaseResources();
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcA(hwnd, message, wParam, lParam);
}

int WINAPI WinMain(
    HINSTANCE hInstance,
    HINSTANCE,
    LPSTR,
    int)
{
    const char CLASS_NAME[] = "CpuGameOfLifeWindow";

    WNDCLASSA window_class = {};

    window_class.lpfnWndProc = WindowProcedure;
    window_class.hInstance = hInstance;
    window_class.lpszClassName = CLASS_NAME;
    window_class.hCursor = LoadCursor(nullptr, IDC_ARROW);
    window_class.hbrBackground = CreateSolidBrush(BG);

    if (!RegisterClassA(&window_class))
    {
        DeleteObject(window_class.hbrBackground);

        MessageBoxA(
            nullptr,
            "Failed to register application window.",
            "Error",
            MB_OK | MB_ICONERROR);

        return 1;
    }

    HWND hwnd = CreateWindowExA(
        0,
        CLASS_NAME,
        "CPU Game of Life | HPC Group 13",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1400,
        950,
        nullptr,
        nullptr,
        hInstance,
        nullptr);

    if (!hwnd)
    {
        DeleteObject(window_class.hbrBackground);

        MessageBoxA(
            nullptr,
            "Failed to create application window.",
            "Error",
            MB_OK | MB_ICONERROR);

        return 1;
    }

    BOOL dark_mode = TRUE;

    DwmSetWindowAttribute(
        hwnd,
        static_cast<DWMWINDOWATTRIBUTE>(20),
        &dark_mode,
        sizeof(dark_mode));

    int backdrop_type = 3;

    DwmSetWindowAttribute(
        hwnd,
        static_cast<DWMWINDOWATTRIBUTE>(38),
        &backdrop_type,
        sizeof(backdrop_type));

    ShowWindow(hwnd, SW_MAXIMIZE);
    UpdateWindow(hwnd);

    MSG message = {};

    while (GetMessageA(&message, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageA(&message);
    }

    DeleteObject(window_class.hbrBackground);

    return static_cast<int>(message.wParam);
}