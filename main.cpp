#include <cstdlib>
#include <cmath>
#include <tuple>
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include "geometry.h"
#include "model.h"
#include "tgaimage.h"
#include <vector>

constexpr TGAColor white   = {255, 255, 255, 255}; // attention, BGRA order
constexpr TGAColor green   = {  0, 255,   0, 255};
constexpr TGAColor red     = {  0,   0, 255, 255};
constexpr TGAColor blue    = {255, 128,  64, 255};
constexpr TGAColor yellow  = {  0, 200, 255, 255};

constexpr int width  = 1024;
constexpr int height = 1024;

void line(int ax, int ay, int bx, int by, TGAImage &framebuffer, TGAColor color)
{
    bool steep = std::abs(ax - bx) < std::abs(ay - by);
    if (steep)
    {
        std::swap(ax, ay);
        std::swap(bx, by);
    }
    if ( ax > bx )
    {
        std::swap(ax, bx);
        std::swap(ay, by);
    }
    int y = ay;
    int ierror = 0;
    for (int x = ax; x <= bx; x ++)
    {
        if ( steep )
            framebuffer.set(y, x, color);
        else
            framebuffer.set(x, y, color);
        ierror += 2 * std::abs(by - ay);
        if ( ierror > bx - ax )
        {
            y += by > ay ? 1 : -1;
            ierror -= 2 * (bx - ax);
        }
    }
}

// 线段 AB 上 y == targety 处的 x；如果该 y 不在线段上，返回 -1
int line_x_at_y(int ax, int ay, int bx, int by, int targety) {
    if (targety < std::min(ay, by) || targety > std::max(ay, by))
        return -1;                                        // targety 超出线段的 y 范围
    if (ay == by)
        return ax;                                        // 水平线：整段都是同一个 y，x 不唯一
    double t = double(targety - ay) / double(by - ay);    // 参数 t ∈ [0,1]
    return std::lround(ax + (bx - ax) * t);               // x = ax + (bx-ax)*t，四舍五入
}

//使用鞋带公式计算三角形面积,带符号的
// //½[(by-ay)(bx+ax) + (cy-by)(cx+bx) + (ay-cy)(ax+cx)]
// = ½[ax·by - ay·bx + bx·cy - by·cx + cx·ay - cy·ax]
// = ½[(B-A) × (C-A)]
// 平行四边形法则下的 叉乘
double signed_triangle_area(int ax, int ay, int bx, int by, int cx, int cy) {
    return .5*((by-ay)*(bx+ax) + (cy-by)*(cx+bx) + (ay-cy)*(ax+cx));
}

void triangle(int ax, int ay, int bx, int by, int cx, int cy, TGAImage &framebuffer, TGAColor color)
{
    int bbminx = (std::min(std::min(ax, bx), cx));
    int bbminy = (std::min(std::min(ay, by), cy));
    int bbmaxx = (std::max(std::max(ax, bx), cx));
    int bbmaxy = (std::max(std::max(ay, by), cy));
    
    //利用重心坐标判断点是否在三角形内，重心坐标中三个参数的值由重心划分的小三角形 / 整个三角形面积获得
    //与用叉乘判断在向量左右侧类似
    double total_area = signed_triangle_area(ax, ay, bx, by, cx, cy);
    if (total_area < 1) return;
    
    for (int x = bbminx; x <= bbmaxx; x ++)
    {
        for (int y = bbminy; y <= bbmaxy; y ++)
        {
            double alpha = signed_triangle_area(x, y, bx, by, cx, cy) / total_area;
            double beta = signed_triangle_area(x, y, cx, cy, ax, ay) / total_area;
            double gamma = signed_triangle_area(x, y, ax, ay, bx, by) / total_area;
            if ( alpha < 0 || beta < 0 || gamma < 0 ) continue;
            framebuffer.set(x, y, color);
        }
    }
}

std::tuple<int,int> project(vec3 v) { // 正交投影：[-1,1] -> 屏幕坐标
    return { (v.x + 1.) * width/2, (v.y + 1.) * height/2 };
}

//运行程序时携带的参数
int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " obj/model.obj" << std::endl;
        return 1;
    }

    Model model(argv[1]);                       // 模型加载已封装进 Model 类（model.h/model.cpp）
    TGAImage framebuffer(width, height, TGAImage::RGB);

    for (int i = 0; i < model.nfaces(); i++) {  // 遍历所有三角形
        auto [ax, ay] = project(model.vert(i, 0));
        auto [bx, by] = project(model.vert(i, 1));
        auto [cx, cy] = project(model.vert(i, 2));
        TGAColor rnd;
        for (int c = 0; c < 3; c++) rnd[c] = std::rand()%255;
        triangle(ax, ay, bx, by, cx, cy, framebuffer, rnd);
        
        // line(ax, ay, bx, by, framebuffer, red);
        // line(bx, by, cx, cy, framebuffer, red);
        // line(cx, cy, ax, ay, framebuffer, red);
    }

    framebuffer.write_tga_file("framebuffer.tga");
    return 0;
}
