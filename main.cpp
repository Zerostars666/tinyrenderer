#include <algorithm>
#include "our_gl.h"
#include "model.h"

extern mat<4,4> ModelView, Perspective; // "OpenGL" state matrices and
extern std::vector<double> zbuffer;     // the depth buffer

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

    constexpr int width  = 2048;      // output image size
    constexpr int height = 2048;
    constexpr vec3  light{ 1, 1, 1}; // light source
    constexpr vec3    eye{0, 0, 2}; // camera position
    constexpr vec3 center{ 0, 0, 0}; // camera direction
    constexpr vec3     up{ 0, 1, 0}; // camera up vector

    lookat(eye, center, up);                                   // build the ModelView   matrix
    init_perspective(norm(eye-center));                        // build the Perspective matrix
    init_viewport(width/16, height/16, width*7/8, height*7/8); // build the Viewport    matrix
    init_zbuffer(width, height);
    TGAImage framebuffer(width, height, TGAImage::RGB);

    for (int m=1; m<argc; m++) {                    // iterate through all input objects
        Model model(argv[m]);                       // load the data
        PhongShader shader(light, model);
        for (int f=0; f<model.nfaces(); f++) {      // iterate through all facets
            //满足现代渲染管线的阶段，先传入顶点作顶点着色器(作顶点的视角变换)，顶点组成原型Clip，进入光栅化阶段，对每个像素采样并进入fragment片元着色器，全部渲染好后进行着色
            Triangle clip = { shader.vertex(f, 0),  // assemble the primitive
                              shader.vertex(f, 1),
                              shader.vertex(f, 2) };
            rasterize(clip, shader, framebuffer);   // rasterize the primitive
        }
    }

    framebuffer.write_tga_file("framebuffer.tga");
    return 0;
}

