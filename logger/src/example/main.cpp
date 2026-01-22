#include <print>
#include <iostream>
#include "logger/logger.h"

int main() {

	g_Logger.Initialize();

	g_Logger.Info("Hello from Info {}");
	g_Logger.Info("Hello from {}", "formated Info");
	g_Logger.Separator();

	g_Logger.Warn("Hello from Warn {}");
	g_Logger.Warn("Hello from {}", "formated Warn");
	g_Logger.Separator();

	g_Logger.Error("Hello from Error {}");
	g_Logger.Error("Hello from {}", "formated Error");
	g_Logger.Separator();

	g_Logger.Success("Hello from Success {}");
	g_Logger.Success("Hello from {}", "formated Success");
	g_Logger.Separator();

	std::cin.get();

	return 0;
}