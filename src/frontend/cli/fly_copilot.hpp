#pragma once

#include <filesystem>
#include <string_view>
#include <vector>

// `glideslope_cli fly-copilot`: an aircraft flown by the AI over the DEM with a
// language model as its copilot, asked as the flight goes (fly_copilot.cpp).
int fly_copilot(const std::filesystem::path& data, const std::vector<std::string_view>& args);
