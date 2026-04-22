#pragma once
#include <glad/glad.h>

// 预计算 BRDF 积分 LUT
// 输入：NdotV（x 轴）+ roughness（y 轴）
// 输出：2D RG 纹理，r = scale, g = bias
GLuint generate_brdf_lut(int size = 512);
