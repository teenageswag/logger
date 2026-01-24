#include <print>
#include <iostream>
#include "logger/logger.h"

int main() {
	g_Logger.Info("Это обычный INFO {}");
	g_Logger.Info("А это {}", "форматированный INFO");
	g_Logger.Separator();

	g_Logger.Warn("Это обычный WARN {}");
	g_Logger.Warn("А это {}", "форматированный WARN");
	g_Logger.Separator();

	g_Logger.Error("Это обычный ERROR {}");
	g_Logger.Error("А это {}", "форматированный ERROR");
	g_Logger.Separator();

	g_Logger.Success("Это обычный SUCCESS {}");
	g_Logger.Success("А это {}", "форматированный SUCCESS");
	g_Logger.Separator();

	std::cin.get();

	return 0;
}