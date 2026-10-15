#include <filesystem>
#include <fstream>
#include <iostream>
#include <semaphore>
#include <string>
#include <thread>
#include <windows.h>

using Run = int(__cdecl *)(const wchar_t *, int);
using Stop = int(__cdecl *)();
struct Module {
  HMODULE handle = nullptr;
  Stop stop = nullptr;
  bool asynchronous = false;
  ~Module() {
    if (handle) {
      if (asynchronous && stop)
        stop();
      FreeLibrary(handle);
    }
  }
};
int wmain(int argc, wchar_t **argv) {
  if (argc < 2 || argc > 3)
    return 2;
  const bool owned_console = argc == 3;
  if (owned_console)
    FreeConsole(); // Detach only this hidden test process.
  const auto output = GetStdHandle(STD_OUTPUT_HANDLE);
  const auto error = GetStdHandle(STD_ERROR_HANDLE);
  const auto codepage = GetConsoleOutputCP();
  for (int mode = owned_console ? 2 : 0; mode < (owned_console ? 3 : 2); ++mode) {
    for (int iteration = 0; iteration < (owned_console ? 3 : 25); ++iteration) {
      const auto path = std::filesystem::current_path() /
                        (L"dll-fixture-" + std::to_wstring(GetCurrentProcessId()) + L".log");
      std::filesystem::remove(path);
      {
        Module module;
        module.asynchronous = mode == 1;
        Run run = nullptr;
        int result = 0;
        std::binary_semaphore go{0}, done{0};
        // This host thread already exists before LoadLibrary attaches the DLL.
        std::jthread caller([&] {
          go.acquire();
          if (run)
            result = run(path.c_str(), mode);
          done.release();
        });
        module.handle = LoadLibraryW(argv[1]);
        if (module.handle) {
          run = reinterpret_cast<Run>(GetProcAddress(module.handle, "run_logger"));
          module.stop = reinterpret_cast<Stop>(GetProcAddress(module.handle, "stop_logger"));
        }
        go.release();
        done.acquire();
        caller.join();
        if (!run || !module.stop || result != 1)
          return 3;
        if (owned_console && GetConsoleOutputCP() == 0)
          return 6;
        // Synchronous teardown intentionally relies on normal DLL CRT destruction.
      }
      if (GetStdHandle(STD_OUTPUT_HANDLE) != output || GetStdHandle(STD_ERROR_HANDLE) != error ||
          GetConsoleOutputCP() != codepage)
        return 4;
      std::ifstream input(path, std::ios::binary);
      const std::string text((std::istreambuf_iterator<char>(input)), {});
      input.close();
      std::filesystem::remove(path);
      if (text.find("DLL record: ") == std::string::npos ||
          text.find("late destructor") != std::string::npos)
        return 5;
    }
  }
  std::cout << "PASS 50 DLL load/unload cycles, existing host threads and late destruction\n";
}
