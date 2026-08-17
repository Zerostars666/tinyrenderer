#include <cstdlib>
#include <algorithm>
#include "our_gl.h"
#include "model.h"

extern mat<4,4> ModelView, Perspective; // OpenGL 风格的状态矩阵
extern std::vector<double> zbuffer;     // 深度缓冲

struct RandomShader : IShader {
    const Model &model;
    TGAColor varying_color[3]; // 三个顶点的颜色（顶点shader写入，片元shader插值）

    RandomShader(const Model &m) : model(m) {
    }

    // 根据法线 n 计算 Phong 光照颜色（环境光 + 漫反射 + 高光）
    virtual TGAColor shading(const vec3 &n, double k1, double k2, double k3)
    {
        //环境光：与方向无关的常量亮度（k1 为环境光系数）
        TGAColor ambient = ambient_term(static_cast<uint8_t>(k1 * 255));
        //漫反射：随面朝向变化（k2 为漫反射系数）
        TGAColor diffuse = diffuse_term(n);
        //高光：镜面反射（k3 为高光系数）
        TGAColor specular = specular_term(n);
        //三项相加，结果截断到 [0,255]
        int r = ambient.bgra[0] + static_cast<int>(diffuse.bgra[0] * k2) + static_cast<int>(specular.bgra[0] * k3);
        int g = ambient.bgra[1] + static_cast<int>(diffuse.bgra[1] * k2) + static_cast<int>(specular.bgra[1] * k3);
        int b = ambient.bgra[2] + static_cast<int>(diffuse.bgra[2] * k2) + static_cast<int>(specular.bgra[2] * k3);
        return { static_cast<uint8_t>(std::min(255, r)),
                 static_cast<uint8_t>(std::min(255, g)),
                 static_cast<uint8_t>(std::min(255, b)), 255 };
    }

    virtual TGAColor ambient_term(uint8_t intensity)
    {
        return {intensity,intensity,intensity,255};
    }

    //漫反射：Lambert 模型，强度 = max(0, n·l)
    virtual TGAColor diffuse_term(const vec3 &n)
    {
        vec3 l = normalized(vec3{5, 5, 5});   // 光源方向（眼空间）
        double diffuse = std::max(0.0, n * l);
        uint8_t intensity = static_cast<uint8_t>(255 * diffuse);
        return {intensity, intensity, intensity, 255};
    }

    //高光：Phong 模型，强度 = max(0, r·v)^16
    virtual TGAColor specular_term(const vec3 &n)
    {
        vec3 l = normalized(vec3{5, 5, 5});              // 光源方向（眼空间）
        vec3 r = normalized(2 * n * (n * l) - l);        // 反射向量
        vec3 v = vec3{0, 0, 1};                          // 视线方向（相机在 +z 远处）
        double spec = std::pow(std::max(0.0, r * v), 16);
        uint8_t intensity = static_cast<uint8_t>(255 * spec);
        return {intensity, intensity, intensity, 255};
    }

    //顶点shader：逐顶点变换 + 逐顶点光照（法线来自模型的 vn 数据）
    virtual vec4 vertex(const int face, const int vert) {
        vec3 v = model.vert(face, vert);                          // 模型顶点坐标
        vec4 gl_Position = ModelView * vec4{v.x, v.y, v.z, 1.};  // 世界 -> 眼空间
        // 法线：把模型空间的 vn 变换到眼空间（ModelView 是纯旋转，法线直接乘；w=0 表示只旋转不平移）
        vec3 n_model = model.normal(face, vert);
        vec3 n_eye = normalized((ModelView * vec4{n_model.x, n_model.y, n_model.z, 0.}).xyz());
        varying_color[vert] = shading(n_eye, 0.2, 0.8, 0.4);     // 逐顶点颜色（供片元shader插值）
        return Perspective * gl_Position;                         // 眼空间 -> 裁剪空间
    }

    //片元shader：用重心坐标 bar 插值三个顶点的颜色（Gouraud 平滑着色）
    virtual std::pair<bool,TGAColor> fragment(const vec3 bar) const {
        TGAColor c;
        for (int ch = 0; ch < 3; ch++) // 对 B/G/R 三个通道分别做线性插值
            c[ch] = static_cast<uint8_t>(varying_color[0][ch]*bar.x + varying_color[1][ch]*bar.y + varying_color[2][ch]*bar.z);
        c[3] = 255;
        return {false, c};
    }
};

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " obj/model.obj" << std::endl;
        return 1;
    }

    constexpr int width  = 800;      // 输出图像宽
    constexpr int height = 800;      // 输出图像高
    constexpr vec3    eye{-1, 0, 2}; // 相机位置
    constexpr vec3 center{ 0, 0, 0}; // 相机看向的目标点
    constexpr vec3     up{ 0, 1, 0}; // 相机"上"方向参考

    lookat(eye, center, up);                                   // 构建 ModelView 矩阵
    init_perspective(norm(eye-center));                        // 构建透视矩阵
    init_viewport(width/16, height/16, width*7/8, height*7/8); // 构建视口矩阵
    init_zbuffer(width, height);
    TGAImage framebuffer(width, height, TGAImage::RGB, {0, 0, 0, 255});

    for (int m=1; m<argc; m++) {                    // 遍历所有输入模型
        Model model(argv[m]);                       // 加载模型（含 vn 法线）
        RandomShader shader(model);
        for (int f=0; f<model.nfaces(); f++) {      // 遍历所有三角形
            Triangle clip = { shader.vertex(f, 0),  // 顶点shader：逐顶点变换 + 光照
                              shader.vertex(f, 1),
                              shader.vertex(f, 2) };
            rasterize(clip, shader, framebuffer);   // 光栅化（内部调用片元shader）
        }
    }

    framebuffer.write_tga_file("framebuffer.tga");
    return 0;
}
