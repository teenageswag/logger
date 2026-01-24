#include <print>
#include <iostream>
#include "logger/logger.h"

int main() {
	g_ConsoleLogger.Info("Это обычный INFO {}");
	g_ConsoleLogger.Info("А это {}", "форматированный INFO");
	g_ConsoleLogger.Separator();

	g_ConsoleLogger.Warn("Это обычный WARN {}");
	g_ConsoleLogger.Warn("А это {}", "форматированный WARN");
	g_ConsoleLogger.Separator();

	g_ConsoleLogger.Error("Это обычный ERROR {}");
	g_ConsoleLogger.Error("А это {}", "форматированный ERROR");
	g_ConsoleLogger.Separator();

	g_ConsoleLogger.Success("Это обычный SUCCESS {}");
	g_ConsoleLogger.Success("А это {}", "форматированный SUCCESS");
	g_ConsoleLogger.Separator();

	std::cin.get();

	return 0;
}