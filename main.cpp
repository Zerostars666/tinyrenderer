#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include "tgaimage.h"
#include <vector>

constexpr TGAColor white   = {255, 255, 255, 255}; // attention, BGRA order
constexpr TGAColor green   = {  0, 255,   0, 255};
constexpr TGAColor red     = {  0,   0, 255, 255};
constexpr TGAColor blue    = {255, 128,  64, 255};
constexpr TGAColor yellow  = {  0, 200, 255, 255};

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

void triangle(int ax, int ay, int bx, int by, int cx, int cy, TGAImage &framebuffer, TGAColor color)
{
    line(ax, ay, bx, by, framebuffer, color);
    line(bx, by, cx, cy, framebuffer, color);
    line(cx, cy, ax, ay, framebuffer, color);
    line(ax, ay, bx, by, framebuffer, white);
    line(bx, by, cx, cy, framebuffer, white);
    line(cx, cy, ax, ay, framebuffer, white);
    
    auto f = [](int &ax, int &ay, int &bx, int &by) { std::swap(ax, bx); std::swap(ay, by); };
    
    if ( ay > by ) f(ax, ay, bx, by);
    if ( by > cy ) f(bx, by, cx, cy);
    if ( ay > by ) f(ax, ay, bx, by);
    
    //std::cout << "color:" << "ay:" << ay << "by:" << by << "cy:" << cy << std::endl;
    
    for (int y = ay; y <= by; y ++)
    {
        int x1 = line_x_at_y(ax, ay, cx, cy, y);
        int x2 = line_x_at_y(ax, ay, bx, by, y);
        if ( x1 == -1 || x2 == -1 ) continue;
        line(x1, y, x2, y, framebuffer, color);
    }
    for (int y = by; y <= cy; y ++)
    {
        int x1 = line_x_at_y(ax, ay, cx, cy, y);
        int x2 = line_x_at_y(bx, by, cx, cy, y);
        if ( x1 == -1 || x2 == -1 ) continue;
        line(x1, y, x2, y, framebuffer, color);
    }
    
    line(ax, ay, bx, by, framebuffer, white);
    line(bx, by, cx, cy, framebuffer, white);
    line(cx, cy, ax, ay, framebuffer, white);
}
int main(int argc, char** argv) {
    constexpr int width  = 128;
    constexpr int height = 128;
    TGAImage framebuffer(width, height, TGAImage::RGB);
    
    
    triangle(  7, 45, 35, 100, 45,  60, framebuffer, red);
    triangle(120, 35, 90,   5, 45, 110, framebuffer, blue);
    triangle(115, 83, 80,  90, 85, 120, framebuffer, green);
    
    
    framebuffer.write_tga_file("framebuffer.tga");
    return 0;
}

