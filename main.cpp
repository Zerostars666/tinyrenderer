#include <limits>
#include <algorithm>
#include "geometry.h"
#include "model.h"
#include "tgaimage.h"

// 三个全局变换矩阵，构成完整的渲染管线：
//   ModelView   : 世界坐标 -> 相机坐标（把相机放到 eye，看向 center）
//   Perspective : 相机坐标 -> 裁剪坐标（实现近大远小的透视效果）
//   Viewport    : NDC 坐标 -> 屏幕像素坐标
mat<4,4> ModelView, Viewport, Perspective;

// 构建 ModelView 矩阵：相机位于 eye，看向 center，up 是"上方向"参考向量
void lookat(const vec3 eye, const vec3 center, const vec3 up) {
    vec3 n = normalized(eye-center);          // 视线反方向，作为相机坐标系的 -z 轴
    vec3 l = normalized(cross(up,n));         // 叉积得到相机"右"方向（x 轴）
    vec3 m = normalized(cross(n, l));         // 再叉积得到相机"上"方向（y 轴）
    // ModelView = 旋转矩阵(把世界坐标旋转到相机朝向) * 平移矩阵(把 center 移到原点)
    ModelView = mat<4,4>{{{l.x,l.y,l.z,0}, {m.x,m.y,m.z,0}, {n.x,n.y,n.z,0}, {0,0,0,1}}} *
                mat<4,4>{{{1,0,0,-center.x}, {0,1,0,-center.y}, {0,0,1,-center.z}, {0,0,0,1}}};
}

// 构建透视投影矩阵：f 为相机到观察目标的距离
// 该矩阵把 (x,y,z,1) 变为 (x,y,z,1-z/f)，即 w = 1 - z/f
// 之后做透视除法（除以 w）：越远 z 越大、w 越小，投影后越小 → 近大远小
void perspective(const double f) {
    Perspective = {{{1,0,0,0}, {0,1,0,0}, {0,0,1,0}, {0,0, -1/f,1}}};
}

// 构建视口矩阵：把 NDC（[-1,1]^3）映射到屏幕上的矩形区域
// (x,y) 为区域左上角，(w,h) 为宽高
void viewport(const int x, const int y, const int w, const int h) {
    Viewport = {{{w/2., 0, 0, x+w/2.}, {0, h/2., 0, y+h/2.}, {0,0,1,0}, {0,0,0,1}}};
}

// 光栅化一个三角形：clip[3] 是裁剪坐标（齐次坐标）下的三个顶点
void rasterize(const vec4 clip[3], std::vector<double> &zbuffer, TGAImage &framebuffer, const TGAColor color) {
    vec4 ndc[3]    = { clip[0]/clip[0].w, clip[1]/clip[1].w, clip[2]/clip[2].w };                // 透视除法：裁剪坐标 ÷ w 得到 NDC（归一化设备坐标）
    vec2 screen[3] = { (Viewport*ndc[0]).xy(), (Viewport*ndc[1]).xy(), (Viewport*ndc[2]).xy() }; // NDC -> 屏幕像素坐标

    // ABC 的三行分别是三个屏幕顶点的 (x, y, 1)
    // 重心坐标满足 [x0 x1 x2; y0 y1 y2; 1 1 1]·(α,β,γ) = (x,y,1)，而 ABC 按行存顶点，
    // 所以用 ABC 的"逆转置"乘像素坐标，即可一次算出该像素的重心坐标
    mat<3,3> ABC = {{ {screen[0].x, screen[0].y, 1.}, {screen[1].x, screen[1].y, 1.}, {screen[2].x, screen[2].y, 1.} }};
    if (ABC.det()<1) return; // 行列式 < 1：背面剔除 + 丢弃面积不足一个像素的三角形

    auto [bbminx,bbmaxx] = std::minmax({screen[0].x, screen[1].x, screen[2].x}); // 三角形包围盒的 x 范围
    auto [bbminy,bbmaxy] = std::minmax({screen[0].y, screen[1].y, screen[2].y}); // 三角形包围盒的 y 范围
#pragma omp parallel for  // OpenMP 多线程并行加速（未启用时被忽略）
    for (int x=std::max<int>(bbminx, 0); x<=std::min<int>(bbmaxx, framebuffer.width()-1); x++) { // 包围盒再裁剪到屏幕范围内
        for (int y=std::max<int>(bbminy, 0); y<=std::min<int>(bbmaxy, framebuffer.height()-1); y++) {
            vec3 bc = ABC.invert_transpose() * vec3{static_cast<double>(x), static_cast<double>(y), 1.}; // 求像素 (x,y) 的重心坐标
            if (bc.x<0 || bc.y<0 || bc.z<0) continue;                                                    // 重心坐标出现负值 => 像素在三角形外
            double z = bc * vec3{ ndc[0].z, ndc[1].z, ndc[2].z };                                        // 用重心坐标插值该像素的深度 z
            if (z <= zbuffer[x+y*framebuffer.width()]) continue;                                         // 深度测试：没有比已有深度更近就跳过
            zbuffer[x+y*framebuffer.width()] = z;                                                        // 更新深度缓冲（记录当前最近深度）
            framebuffer.set(x, y, color);                                                                // 写入颜色像素
        }
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " obj/model.obj" << std::endl;
        return 1;
    }

    constexpr int width  = 800;    // 输出图像宽
    constexpr int height = 800;    // 输出图像高
    constexpr vec3    eye{-1,0,2}; // 相机位置
    constexpr vec3 center{0,0,0};  // 相机看向的目标点
    constexpr vec3     up{0,1,0};  // 相机的"上"方向参考

    lookat(eye, center, up);                              // 构建 ModelView 矩阵（世界 -> 相机）
    perspective(norm(eye-center));                        // 构建透视矩阵，f = 相机到目标点的距离
    viewport(width/16, height/16, width*7/8, height*7/8); // 构建视口矩阵（屏幕中央 7/8 的区域）

    TGAImage framebuffer(width, height, TGAImage::RGB);
    std::vector<double> zbuffer(width*height, -std::numeric_limits<double>::max()); // 深度缓冲初始化为"负无穷"（最远）

    for (int m=1; m<argc; m++) { // 遍历命令行传入的每一个模型文件
        Model model(argv[m]);
        for (int i=0; i<model.nfaces(); i++) { // 遍历该模型的每个三角形
            vec4 clip[3];
            for (int d : {0,1,2}) {            // 把三角形的三个顶点逐个变换到裁剪空间
                vec3 v = model.vert(i, d);
                clip[d] = Perspective * ModelView * vec4{v.x, v.y, v.z, 1.}; // 世界->相机(ModelView)，再乘透视矩阵(Perspective)
            }
            TGAColor rnd;
            for (int c=0; c<3; c++) rnd[c] = std::rand()%255; // 生成随机颜色（仅用于演示）
            rasterize(clip, zbuffer, framebuffer, rnd);        // 光栅化（填充）这个三角形
        }
    }

    framebuffer.write_tga_file("framebuffer.tga");
    return 0;
}
