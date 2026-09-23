#pragma once

#include "../../common/soft3d/runtime/scratch.h"

namespace lets_and_go {
template<class T> using RenderScratch=soft3d::Scratch<T>;
template<class T> using RenderScratchBuffer=soft3d::ScratchBuffer<T>;
} // namespace lets_and_go
