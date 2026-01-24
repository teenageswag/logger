#include <print>
#include <iostream>
#include "logger/logger.h"

int main() {
	// Console Logger Preview
	g_ConsoleLogger.Info("Вывод в консоль");
	g_ConsoleLogger.Info("{} вывод в консоль", "Форматированный");
	g_ConsoleLogger.Separator();
	g_ConsoleLogger.Warn("Вывод в консоль");
	g_ConsoleLogger.Warn("{} вывод в консоль", "Форматированный");
	g_ConsoleLogger.Separator();
	g_ConsoleLogger.Error("Вывод в консоль");
	g_ConsoleLogger.Error("{} вывод в консоль", "Форматированный");
	g_ConsoleLogger.Separator();
	g_ConsoleLogger.Success("Вывод в консоль");
	g_ConsoleLogger.Success("{} вывод в консоль", "Форматированный");
	g_ConsoleLogger.Separator();

	// File Logger Preview
	g_FileLogger.Info("Вывод в файл");
	g_FileLogger.Info("{} вывод в файл", "Форматированный");
	g_FileLogger.Separator();
	g_FileLogger.Warn("Вывод в файл");
	g_FileLogger.Warn("{} вывод в файл", "Форматированный");
	g_FileLogger.Separator();
	g_FileLogger.Error("Вывод в файл");
	g_FileLogger.Error("{} вывод в файл", "Форматированный");
	g_FileLogger.Separator();
	g_FileLogger.Success("Вывод в файл");
	g_FileLogger.Success("{} вывод в файл", "Форматированный");
	g_FileLogger.Separator();

	std::cin.get();
	return 0;
}