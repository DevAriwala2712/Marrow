#pragma once

#include <vector>

namespace marrow {

bool load_png_rgba(const char* path, std::vector<unsigned char>& out_rgba, int& out_width, int& out_height);

}  // namespace marrow
