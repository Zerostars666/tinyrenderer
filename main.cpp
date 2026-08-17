#include <cstdlib>
#include <algorithm>
#include "our_gl.h"
#include "model.h"

extern mat<4,4> ModelView, Perspective; // "OpenGL" state matrices and
extern std::vector<double> zbuffer;     // the depth buffer

struct RandomShader : IShader {
    const Model &model;
    TGAColor color = {};
    vec3 tri[3];  // triangle in eye coordinates

    RandomShader(const Model &m) : model(m) {
    }
    
    virtual TGAColor shading(double k1, double k2, double k3)
    {
        double all = k1 + k2 + k3;
        k1 = k1 / all; k2 = k2 / all; k3 = k3 / all;
        //环境光：与方向无关的常量亮度（k1 为环境光系数）
        TGAColor ambient = ambient_term(static_cast<uint8_t>(k1 * 255));
        //漫反射：随面朝向变化（k2 为漫反射系数）
        TGAColor diffuse = diffuse_term();
        //高光：镜面反射，随视线与反射方向接近程度变化（k3 为高光系数）
        TGAColor specular = specular_term();
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
    virtual TGAColor diffuse_term()
    {
        //面法线：相机空间两条 3D 边的叉积（顺序决定朝向，若画面全黑就交换两条边）
        vec3 n = normalized(cross(tri[1]-tri[0], tri[2]-tri[0]));
        //光源方向（相机空间）：从右上前方照向模型
        vec3 l = normalized(vec3{5, 5, 5});
        //Lambert 漫反射：n·l，裁剪到 [0,1]
        double diffuse = std::max(0.0, n * l);
        uint8_t intensity = static_cast<uint8_t>(255 * diffuse);
        return {intensity, intensity, intensity, 255};
    }
    virtual TGAColor specular_term()
    {
        vec3 n = normalized(cross(tri[1]-tri[0], tri[2]-tri[0])); // 面法线（相机空间）
        vec3 l = normalized(vec3{5, 5, 5});                       // 光源方向（相机空间）
        //反射向量：l 关于 n 的镜面反射 r = 2(n·l)n - l
        vec3 r = normalized(2 * n * (n * l) - l);
        //视线方向：相机在眼空间 +z 远处，对近原点的小模型近似为 (0,0,1)
        vec3 v = vec3{0, 0, 1};
        //Phong 高光：max(0, r·v) 的 shininess 次方，指数越大高光越锐利
        double spec = std::pow(std::max(0.0, r * v), 16);
        uint8_t intensity = static_cast<uint8_t>(255 * spec);
        return {intensity, intensity, intensity, 255};
    }
    
    //顶点shader
    virtual vec4 vertex(const int face, const int vert) {
        vec3 v = model.vert(face, vert);                          // current vertex in object coordinates
        vec4 gl_Position = ModelView * vec4{v.x, v.y, v.z, 1.};
        tri[vert] = gl_Position.xyz();                            // in eye coordinates
        return Perspective * gl_Position;                         // in clip coordinates
    }
    
    //片元shader 包含环境光照 漫反射 高光
    virtual std::pair<bool,TGAColor> fragment(const vec3 bar) const {
        return {false, color};                                    // do not discard the pixel
    }
};

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " obj/model.obj" << std::endl;
        return 1;
    }

    constexpr int width  = 800;      // output image size
    constexpr int height = 800;
    constexpr vec3    eye{0, 0, 2}; // camera position
    constexpr vec3 center{ 0, 0, 0}; // camera direction
    constexpr vec3     up{ 0, 1, 0}; // camera up vector

    lookat(eye, center, up);                                   // build the ModelView   matrix
    init_perspective(norm(eye-center));                        // build the Perspective matrix
    init_viewport(width/16, height/16, width*7/8, height*7/8); // build the Viewport    matrix
    init_zbuffer(width, height);
    TGAImage framebuffer(width, height, TGAImage::RGB, {0, 0, 0, 255});

    for (int m=1; m<argc; m++) {                    // iterate through all input objects
        Model model(argv[m]);                       // load the data
        RandomShader shader(model);
        for (int f=0; f<model.nfaces(); f++) {      // iterate through all facets
            //shader.color = { static_cast<std::uint8_t>(std::rand()%255), static_cast<std::uint8_t>(std::rand()%255), static_cast<std::uint8_t>(std::rand()%255), 255 };
            Triangle clip = { shader.vertex(f, 0),  // assemble the primitive
                              shader.vertex(f, 1),
                              shader.vertex(f, 2) };
            shader.color = shader.shading(1, 1, 1); // 顶点变换完成后才能算光照（diffuse 需要 tri[] 法线）
            rasterize(clip, shader, framebuffer);   // rasterize the primitive
        }
    }

    framebuffer.write_tga_file("framebuffer.tga");
    return 0;
}