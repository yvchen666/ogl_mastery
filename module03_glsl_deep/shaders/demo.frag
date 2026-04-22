#version 460 core

// ── UBO：std140 布局，binding=0 ──────────────────────────────────────────────
// std140 对齐规则（对本结构体的影响见注释）：
//   float uTime      : 偏移  0，大小 4，对齐 4
//   float uAspect    : 偏移  4，大小 4，对齐 4
//   vec2  uResolution: 偏移  8，大小 8，对齐 8（vec2 的 std140 对齐 = 8）
// 总大小 = 16 字节（16 字节对齐满足）
//
// 如果使用 vec3，基础对齐变成 16，会在 float 后插入 8 字节 padding，总大小 32 字节！
// 这是 std140 中 vec3 坑的根源（见 README 第5节）。
layout(std140, binding = 0) uniform FrameData {
    float uTime;         // 程序运行时间（秒），用于动画
    float uAspect;       // 宽高比 width/height，用于修正长宽比变形
    vec2  uResolution;   // 分辨率（像素），偏移8（vec2对齐到8字节，无需padding）
};

// ── 输入：来自顶点着色器的插值变量 ──────────────────────────────────────────
in vec2 vUV;          // UV 坐标 [0,1]
in vec2 vScreenPos;   // 屏幕归一化位置 [0,1]（vScreenPos = vUV，因为矩形是全屏的）

// ── 输出：片元颜色 ───────────────────────────────────────────────────────────
layout(location = 0) out vec4 FragColor;

void main() {
    // ── 效果 1：棋盘格（基于 gl_FragCoord）──────────────────────────────────
    // gl_FragCoord.xy：片元的窗口空间坐标（像素单位，原点左下角）
    // floor(... / 50.0)：以 50 像素为一格
    // mod(x+y, 2.0)：当行+列为偶数时 c=0（暗），奇数时 c=1（亮）
    vec2 checker = floor(gl_FragCoord.xy / 50.0);
    float c = mod(checker.x + checker.y, 2.0);   // 0.0 或 1.0

    // ── 效果 2：时间驱动的颜色动画（sin/cos）────────────────────────────────
    // 三个通道用不同频率的 sin/cos，产生循环变化的 RGB 颜色
    // 频率差异（×0.7, ×1.3）使三个通道不同步，颜色随时间缓慢变换
    //
    // sin 的值域 [-1, 1]，乘 0.5 加 0.5 映射到 [0, 1]（亮度范围）
    // vUV.x/y 空间调制：使颜色在屏幕上有空间变化（而不是全屏单色）
    vec3 color = vec3(
        0.5 + 0.5 * sin(uTime       + vUV.x * 3.14159),   // R：随 x 和时间变化
        0.5 + 0.5 * cos(uTime * 0.7 + vUV.y * 3.14159),   // G：随 y 和时间变化（cos 相位差 π/2）
        0.5 + 0.5 * sin(uTime * 1.3)                       // B：纯时间变化，全屏一致
    );

    // ── 效果 3：smoothstep 边缘渐变 ─────────────────────────────────────────
    // smoothstep(edge0, edge1, x)：
    //   x < edge0 → 0.0
    //   x > edge1 → 1.0
    //   edge0 < x < edge1 → 平滑 Hermite 插值（3t²-2t³，导数为0，无突变）
    //
    // 这里对 UV 的四条边缘做 2% 宽度的渐变，使矩形边缘有柔和过渡
    // 四个 smoothstep 相乘：只有四条边都"通过"时 edge 才为 1.0（远离边缘时）
    float edge =
        smoothstep(0.0,  0.02, vUV.x) *   // 左边缘：x 从 0 渐变到 0.02
        smoothstep(1.0,  0.98, vUV.x) *   // 右边缘：x 从 1 渐变到 0.98（注意方向反转）
        smoothstep(0.0,  0.02, vUV.y) *   // 下边缘
        smoothstep(1.0,  0.98, vUV.y);    // 上边缘

    // ── 合成最终颜色 ─────────────────────────────────────────────────────────
    // mix(a, b, t)：线性插值，t=0 返回 a，t=1 返回 b
    // - 棋盘格区域（edge=0 处的边缘）：显示暗色棋盘格 vec3(c*0.2)
    // - 内部区域（edge=1）：显示动画颜色
    // - 边缘（0 < edge < 1）：平滑混合
    FragColor = vec4(mix(vec3(c * 0.2), color, edge), 1.0);
}
