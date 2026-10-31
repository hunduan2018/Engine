// FrameConfig.h
// 帧流水线配置。NumFramesInFlight = CPU 最多领先 GPU 的帧数，决定每帧资源（Command Allocator、
// 常量上传环、实例分区等）的份数；与交换链后备缓冲数无关，后备缓冲索引只用于选 RTV。

#pragma once

#include <cstdint>

inline constexpr uint32_t NumFramesInFlight = 3;
