#include <iostream>

#include "logger/logger.h"

int main() {
  // Console Logger Preview
  g_ConsoleLogger.Info("Output to the console");
  g_ConsoleLogger.Info("{} output to the console", "Formatted");
  g_ConsoleLogger.Separator();

  g_ConsoleLogger.Warn("Output to the console");
  g_ConsoleLogger.Warn("{} output to the console", "Formatted");
  g_ConsoleLogger.Separator();

  g_ConsoleLogger.Error("Output to the console");
  g_ConsoleLogger.Error("{} output to the console", "Formatted");
  g_ConsoleLogger.Separator();

  g_ConsoleLogger.Success("Output to the console");
  g_ConsoleLogger.Success("{} output to the console", "Formatted");
  g_ConsoleLogger.Separator();

  // File Logger Preview
  g_FileLogger.Info("Output to the log.txt");
  g_FileLogger.Info("{} output to the log.txt", "Formatted");
  g_FileLogger.Separator();

  g_FileLogger.Warn("Output to the log.txt");
  g_FileLogger.Warn("{} output to the log.txt", "Formatted");
  g_FileLogger.Separator();

  g_FileLogger.Error("Output to the log.txt");
  g_FileLogger.Error("{} output to the log.txt", "Formatted");
  g_FileLogger.Separator();

  g_FileLogger.Success("Output to the log.txt");
  g_FileLogger.Success("{} output to the log.txt", "Formatted");
  g_FileLogger.Separator();

  std::cin.get();
  return 0;
}
