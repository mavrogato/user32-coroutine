
#include <iostream>
#include <coroutine>
#include <tuple>

#include <windows.h>
#include <windowsx.h>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

inline namespace aux
{
    template <size_t...> struct seq { };
    template <size_t N, size_t... I> struct gen_seq : gen_seq<N-1, N-1, I...> { };
    template <size_t... I> struct gen_seq<0, I...> : seq<I...> { };
    template <class Ch, class Tuple, size_t... I>
    void print(std::basic_ostream<Ch>& output, Tuple const& t, seq<I...>) noexcept {
        using swallow = int[];
        (void) swallow{0, (void(output << (I==0? "" : " ") << std::get<I>(t)), 0)...};
    }
    template <class Ch, class...Args>
    auto& operator<<(std::basic_ostream<Ch>& output, std::tuple<Args...> const& t) noexcept {
        output.put('(');
        print(output, t, gen_seq<sizeof... (Args)>());
        output.put(')');
        return output;
    }
    template <class T>
    struct resumable {
        struct promise_type {
            void unhandled_exception() { throw; }
            auto get_return_object() noexcept { return resumable{*this}; }
            auto initial_suspend() noexcept { return std::suspend_always{}; }
            auto final_suspend() noexcept { return std::suspend_always{}; }
            auto yield_value(T value) noexcept {
                this->result = value;
                return std::suspend_always{};
            }
            auto return_void() noexcept { }
            T result;
        };

    private:
        std::coroutine_handle<promise_type> handle;
        explicit resumable(promise_type& p) noexcept
            : handle{std::coroutine_handle<promise_type>::from_promise(p)}
        {
        }

    public:
        ~resumable() noexcept {
            if (handle) handle.destroy();
        }
        T result() const noexcept {
            this->handle.resume();
            return this->handle.promise().result;
        }
    };
} // ::aux

int main() {
    struct wndclass : WNDCLASS {
        ATOM atom;
        operator ATOM() const noexcept { return this->atom; }
        wndclass(auto... args) noexcept : WNDCLASS(args...), atom(RegisterClass(this))
        {
        }
        ~wndclass() noexcept {
            if (this->atom) {
                if (!UnregisterClass(this->lpszClassName, this->hInstance)) {
                    std::cerr << "UnregisterClass failed: " << GetLastError() << std::endl;
                }
            }
        }
    } wc(CS_HREDRAW | CS_VREDRAW,
         [](auto... args) noexcept -> LRESULT {
             thread_local std::tuple<HWND, UINT, WPARAM, LPARAM> params;
             params = std::tuple{args...};
             thread_local auto continuation = [](auto& params) -> resumable<LRESULT> {
                 auto const& [hwnd, message, wParam, lParam] = params;
                 while (message != WM_DESTROY) {
                     co_yield std::apply(DefWindowProc, params);
                 }
                 PostQuitMessage(0);
                 co_yield 0;
             }(params);
             return continuation.result();
         },
         0,
         0,
         GetModuleHandle(nullptr),
         LoadIcon(nullptr, IDI_APPLICATION),
         LoadCursor(nullptr, IDC_CROSS),
         static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)),
         nullptr,
         TEXT("b82d9816-f598-4157-8471-83a9a88ec5b1"));
    if (!wc) {
        std::cerr << "RegisterClass failed: " << GetLastError() << std::endl;
        return 1;
    }
    if (HWND hwnd = CreateWindowEx(WS_EX_CLIENTEDGE,
                                   wc.lpszClassName,
                                   wc.lpszClassName,
                                   WS_OVERLAPPEDWINDOW,
                                   CW_USEDEFAULT, CW_USEDEFAULT,
                                   CW_USEDEFAULT, CW_USEDEFAULT,
                                   nullptr, nullptr, wc.hInstance, nullptr))
    {
        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);
        for (;;) {
            MSG msg;
            if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
                if (msg.message == WM_QUIT) {
                    return static_cast<int>(msg.wParam);
                }
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
            else {
                WaitMessage();
            }
        }
    }
    else {
        std::cerr << "CreateWindowEx failed: " << GetLastError() << std::endl;
        return 1;
    }
}
