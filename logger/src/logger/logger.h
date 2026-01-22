#pragma once
#include <windows.h>
#include <print>
#include <format>
#include <string>
#include <string_view>
#include <chrono>
#include <cstdint>
#include <concepts>

enum class LogLevel : uint8_t {
    Info,
    Warn,
    Error,
    Success
};

struct Color {
    uint8_t r, g, b;
    constexpr Color(uint8_t r = 255, uint8_t g = 255, uint8_t b = 255) noexcept : r(r), g(g), b(b) {}

    //  Convert HEX to RGB
    static constexpr Color FromHex(uint32_t hex) noexcept {
        return Color(
            static_cast<uint8_t>((hex >> 16) & 0xFF),
            static_cast<uint8_t>((hex >> 8) & 0xFF),
            static_cast<uint8_t>(hex & 0xFF)
        );
    }

    //  Get ANSI escape sequence for this color
    [[nodiscard]] auto ToAnsi() const noexcept {
        return std::format("\x1b[38;2;{};{};{}m", r, g, b);
    }
};

//  Color palette
namespace LogColors {
    inline constexpr Color Timestamp = Color::FromHex(0x8C8C8C);
    inline constexpr Color Info = Color::FromHex(0x1A8CFF);
    inline constexpr Color Warn = Color::FromHex(0xFF8C1A);
    inline constexpr Color Error = Color::FromHex(0xFF1A40);
    inline constexpr Color Success = Color::FromHex(0x1AFF1A);
    inline constexpr Color Default = Color::FromHex(0xE6E6E6);
    inline constexpr Color Separator = Color::FromHex(0x8C8C8C);
}

class ConsoleLogger {
private:
    static inline HANDLE s_hConsole = nullptr;
    static inline bool s_initialized = false;
    static constexpr std::string_view s_ResetSeq = "\x1b[0m";

    static auto GetTimestamp() noexcept {
        const auto time = std::chrono::floor<std::chrono::seconds> (
                std::chrono::current_zone()->to_local (
                    std::chrono::system_clock::now()
                )
            );
        return std::format("{:%H:%M:%S}", time);
    }
    static constexpr std::string_view GetLevelPrefix(LogLevel level) noexcept {
        switch (level) {
            case LogLevel::Info:    return "INF";
            case LogLevel::Warn:    return "WRN";
            case LogLevel::Error:   return "ERR";
            case LogLevel::Success: return "SUC";
            default:                return "UNK";
        }
    }
    static constexpr Color GetLevelColor(LogLevel level) noexcept {
        switch (level) {
            case LogLevel::Info:    return LogColors::Info;
            case LogLevel::Warn:    return LogColors::Warn;
            case LogLevel::Error:   return LogColors::Error;
            case LogLevel::Success: return LogColors::Success;
            default:                return LogColors::Default;
        }
    }
    [[nodiscard]] static int GetConsoleWidth() noexcept {
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
            return csbi.srWindow.Right - csbi.srWindow.Left + 1;
        }
        return 80;
    }
    static void LogImpl(LogLevel level, std::string_view message) {
        if (!s_initialized) [[unlikely]] Initialize();

        std::print(
            "{}[{}] {}[{}]{} {}\n",
            LogColors::Timestamp.ToAnsi(),
            GetTimestamp(),
            GetLevelColor(level).ToAnsi(),
            GetLevelPrefix(level),
            s_ResetSeq,
            message);
    }

public:
    static void Initialize() noexcept {
        if (s_initialized) [[likely]] return;

        s_hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        SetConsoleOutputCP(CP_UTF8);

        if (DWORD mode = 0; GetConsoleMode(s_hConsole, &mode)) {
            SetConsoleMode(s_hConsole, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
        s_initialized = true;
    }

    //  Basic logging
    static void Info(std::string_view msg) { LogImpl(LogLevel::Info, msg); }
    static void Warn(std::string_view msg) { LogImpl(LogLevel::Warn, msg); }
    static void Error(std::string_view msg) { LogImpl(LogLevel::Error, msg); }
    static void Success(std::string_view msg) { LogImpl(LogLevel::Success, msg); }

    //  Formatted logging
    template<typename... Args>
    static void Info(std::format_string<Args...> fmt, Args&&... args) {
        LogImpl(LogLevel::Info, std::format(fmt, std::forward<Args>(args)...));
    }

    template<typename... Args>
    static void Warn(std::format_string<Args...> fmt, Args&&... args) {
        LogImpl(LogLevel::Warn, std::format(fmt, std::forward<Args>(args)...));
    }

    template<typename... Args>
    static void Error(std::format_string<Args...> fmt, Args&&... args) {
        LogImpl(LogLevel::Error, std::format(fmt, std::forward<Args>(args)...));
    }

    template<typename... Args>
    static void Success(std::format_string<Args...> fmt, Args&&... args) {
        LogImpl(LogLevel::Success, std::format(fmt, std::forward<Args>(args)...));
    }

    template<typename... Args>
    static void Debug(std::format_string<Args...> fmt, Args&&... args) {
        LogImpl(LogLevel::Debug, std::format(fmt, std::forward<Args>(args)...));
    }

    //  Utilities
    static void Clear() noexcept {
        if (!s_hConsole) [[unlikely]] return;

        CONSOLE_SCREEN_BUFFER_INFO csbi;
        COORD topLeft = { 0, 0 };

        if (GetConsoleScreenBufferInfo(s_hConsole, &csbi)) {
            DWORD consoleSize = csbi.dwSize.X * csbi.dwSize.Y;
            DWORD written;
            FillConsoleOutputCharacterW(s_hConsole, L' ', consoleSize, topLeft, &written);
            FillConsoleOutputAttribute(s_hConsole, csbi.wAttributes, consoleSize, topLeft, &written);
            SetConsoleCursorPosition(s_hConsole, topLeft);
        }
    }
    static void Separator() {
        if (!s_initialized) [[unlikely]] Initialize();

        const int width = GetConsoleWidth();
        std::print("{}", LogColors::Separator.ToAnsi());

        for (int i = 0; i < width; ++i) {
            std::print("\xE2\x94\x80"); //  Unicode '─' (UTF-8 encoded)
        }

        std::println("{}", s_ResetSeq);
    }
    static void NewLine() noexcept {
        std::println("");
    }
};
ConsoleLogger g_Logger;