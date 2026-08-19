#include <algorithm>
#include "our_gl.h"
#include "model.h"

extern mat<4,4> Viewport, ModelView, Perspective; // "OpenGL" 状态矩阵
extern std::vector<double> zbuffer;               // 深度缓冲

// 第一趟（光源视角）使用的"空"着色器：只需写入深度，颜色随意
struct EmptyShader : IShader {
    const Model &model;
    EmptyShader(const Model &m) : model(m) {}
    virtual vec4 vertex(const int face, const int vert) {
        vec4 gl_Position = ModelView * model.vert(face, vert); // 顶点变换到裁剪坐标
        return Perspective * gl_Position;
    }
    virtual std::pair<bool,TGAColor> fragment(const vec3 bar) const {
        return {false, {255, 255, 255, 255}};     // 返回白色即可，深度由光栅化写入
    }
};

struct PhongShader : IShader {
    const Model &model;
    vec4 l;              // light direction in eye coordinates 在摄像机坐标系下的光线方向
    vec2  varying_uv[3]; // triangle uv coordinates, written by the vertex shader, read by the fragment shader UV坐标
    vec4 varying_nrm[3]; // 每顶点的法线（眼空间），片元着色器里用重心坐标插值成每个像素的法线
    vec4 tri[3];         // 三角形在眼空间（视图坐标系）的三个顶点位置，供片元阶段计算切空间基

    PhongShader(const vec3 light, const Model &m) : model(m) {
        l = normalized((ModelView*vec4{light.x, light.y, light.z, 0.})); // transform the light vector to view coordinates
    }
    
    //顶点着色器
    virtual vec4 vertex(const int face, const int vert) {
        varying_uv[vert]  = model.uv(face, vert); //获取每个顶点的UV坐标
        varying_nrm[vert] = ModelView.invert_transpose() * model.normal(face, vert); //每顶点的法线变换到眼空间（法线用逆转置变换）
        vec4 gl_Position = ModelView * model.vert(face, vert); //返回视角转换后的顶点位置
        tri[vert] = gl_Position; //保存眼空间顶点位置，供片元着色器计算切空间
        return Perspective * gl_Position;                         // in clip coordinates
    }
    
    //片元着色器,每一个点会传入一个重心坐标进来
    virtual std::pair<bool,TGAColor> fragment(const vec3 bar) const {
        mat<2,4> E = { tri[1]-tri[0], tri[2]-tri[0] }; // 三角形在眼空间的两条边（3D）
        mat<2,2> U = { varying_uv[1]-varying_uv[0], varying_uv[2]-varying_uv[0] }; // 对应的两条UV边（2D）
        mat<2,4> T = U.invert() * E; // (t,b) = E·U^(-1)：解出切线t和副切线b
        mat<4,4> D = {normalized(T[0]),  // 切线 t（对应UV的u方向）
                      normalized(T[1]),  // 副切线 b（对应UV的v方向）
                      normalized(varying_nrm[0]*bar[0] + varying_nrm[1]*bar[1] + varying_nrm[2]*bar[2]), // 用重心坐标插值出法线n
                      {0,0,0,1}}; // 组装成Darboux局部坐标系
        vec2 uv = varying_uv[0] * bar[0] + varying_uv[1] * bar[1] + varying_uv[2] * bar[2];
        vec4 n = normalized(D.transpose() * model.normal(uv)); // 法线：从切空间法线贴图采样，用TBN转回眼空间
        vec4 r = normalized(n * (n * l)*2 - l);                   // 反射光方向      
        double ambient  = .4;                                     // 环境光强度
        double diffuse  = 1.*std::max(0., n * l);                 // 漫反射强度
        double specular = (3.*sample2D(model.specular(), uv)[0]/255.) * std::pow(std::max(r.z, 0.), 35); // 高光强度 × 高光贴图权重
        TGAColor gl_FragColor = sample2D(model.diffuse(), uv);    // 从漫反射贴图采样基础颜色
        for (int channel : {0,1,2})                               // 逐通道：颜色 × (环境光 + 漫反射 + 高光)
            gl_FragColor[channel] = std::min<int>(255, gl_FragColor[channel]*(ambient + diffuse + specular));
        return {false, gl_FragColor};                             // do not discard the pixel
    }
};

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " obj/model.obj" << std::endl;
        return 1;
    }

    constexpr int width  = 2048;      // 最终输出图像尺寸
    constexpr int height = 2048;
    constexpr int shadoww = 8000;     // 阴影贴图分辨率（越高阴影边缘越锐利，也越费内存）
    constexpr int shadowh = 8000;
    constexpr vec3  light{ 1, 1, 1}; // light source 光源位置
    constexpr vec3    eye{0, 0, 2}; // camera position 相机位置
    constexpr vec3 center{ 0, 0, 0}; // camera direction 相机看向的目标
    constexpr vec3     up{ 0, 1, 0}; // camera up vector 相机上方向

    std::vector<double> zshadow;    // 阴影贴图：从光源视角看到的最近深度
    mat<4,4> ShadowMatrix;          // 光源视角的完整变换链 Viewport*Perspective*ModelView

    { // ===== 第一趟：从光源视角渲染，只记录深度（阴影贴图） =====
        lookat(light, center, up);                                   // 把"光源"当作相机
        init_perspective(norm(eye-center));                          // 透视投影
        init_viewport(shadoww/16, shadowh/16, shadoww*7/8, shadowh*7/8); // 视口
        init_zbuffer(shadoww, shadowh);                              // 光源视角的深度缓冲
        TGAImage shadow_fb(shadoww, shadowh, TGAImage::RGB, {177, 195, 209, 255});

        for (int m=1; m<argc; m++) {                    // 遍历所有输入模型
            Model model(argv[m]);
            EmptyShader shader{model};                  // 只写深度，颜色无所谓
            for (int f=0; f<model.nfaces(); f++) {
                Triangle clip = { shader.vertex(f, 0),  // 顶点着色 → 组装三角形
                                  shader.vertex(f, 1),
                                  shader.vertex(f, 2) };
                rasterize(clip, shader, shadow_fb);     // 光栅化（写入深度）
            }
        }
        zshadow = zbuffer;                                  // 保存阴影贴图（深度值）
        ShadowMatrix = Viewport * Perspective * ModelView;  // 保存光源变换链
        shadow_fb.write_tga_file("shadowmap.tga");          // 可视化阴影贴图
    }

    // ===== 第二趟：从相机视角正常渲染（切空间 Phong 着色） =====
    lookat(eye, center, up);                                   // 恢复相机视角
    init_perspective(norm(eye-center));
    init_viewport(width/16, height/16, width*7/8, height*7/8);
    init_zbuffer(width, height);
    TGAImage framebuffer(width, height, TGAImage::RGB);

    for (int m=1; m<argc; m++) {                    // 遍历所有输入模型
        Model model(argv[m]);
        PhongShader shader(light, model);
        for (int f=0; f<model.nfaces(); f++) {
            Triangle clip = { shader.vertex(f, 0),  // 顶点着色 → 组装三角形
                              shader.vertex(f, 1),
                              shader.vertex(f, 2) };
            rasterize(clip, shader, framebuffer);   // 光栅化 + 片元着色
        }
    }
    framebuffer.write_tga_file("framebuffer.tga");  // 先输出无阴影版本

    // ===== 后处理：逐像素判定是否在阴影中，阴影区变暗 =====
#pragma omp parallel for
    for (int x=0; x<width; x++) {
        for (int y=0; y<height; y++) {
            // 把相机屏幕空间的片元 (x, y, 深度) 用相机变换的逆 M⁻¹ 送回物体坐标
            vec4 fragment = (Viewport * Perspective * ModelView).invert() * vec4{static_cast<double>(x), static_cast<double>(y), zbuffer[x+y*width], 1.};
            // 再用光源变换 N 把它投影到光源的屏幕空间
            vec4 q = ShadowMatrix * fragment;
            vec3 p = q.xyz()/q.w;                   // 透视除法：光源视角下的 (x', y', z')
            if (p.x<0 || p.x>=shadoww || p.y<0 || p.y>=shadowh) continue; // 不在光源视野内，跳过
            // 若片元深度比阴影贴图记录的"最近深度"更远 → 被别的表面挡住 → 阴影
            if (p.z < zshadow[int(p.x) + int(p.y)*shadoww] - .03) { // 减 0.03 是深度偏移，避免 z-fighting
                TGAColor c = framebuffer.get(x, y);
                framebuffer.set(x, y, { static_cast<unsigned char>(c[0]/2), static_cast<unsigned char>(c[1]/2), static_cast<unsigned char>(c[2]/2), c[3] }); // 变暗一半
            }
        }
    }

    framebuffer.write_tga_file("shadow.tga");       // 输出带阴影的最终图
    return 0;
}
