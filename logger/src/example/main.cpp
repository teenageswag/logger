#include <print>
#include <iostream>
#include "logger/logger.h"

int main() {
	g_Logger.cl_Info("Hello from Info {}");
	g_Logger.cl_Info("Hello from {}", "formated Info");
	g_Logger.Separator();

	g_Logger.cl_Warn("Hello from Warn {}");
	g_Logger.cl_Warn("Hello from {}", "formated Warn");
	g_Logger.Separator();

	g_Logger.cl_Error("Hello from Error {}");
	g_Logger.cl_Error("Hello from {}", "formated Error");
	g_Logger.Separator();

	g_Logger.cl_Success("Hello from Success {}");
	g_Logger.cl_Success("Hello from {}", "formated Success");
	g_Logger.Separator();

	std::cin.get();

	return 0;
}