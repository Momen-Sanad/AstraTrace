#pragma once

#include <filesystem>
#include <string>
#include "core/color.hpp"
#include "core/image.hpp"

bool savePng(const Image<Color8>& image, const std::filesystem::path& path, std::string& error);
