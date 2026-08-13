#include <cmath>
#include <cstdlib>
#include <ctime>
#include "tgaimage.h"

constexpr TGAColor white   = {255, 255, 255, 255}; // attention, BGRA order
constexpr TGAColor green   = {  0, 255,   0, 255};
constexpr TGAColor red     = {  0,   0, 255, 255};
constexpr TGAColor blue    = {255, 128,  64, 255};
constexpr TGAColor yellow  = {  0, 200, 255, 255};

void line(int ax, int ay, int bx, int by, TGAImage &framebuffer, TGAColor color)
{
    //判断x和y上哪个需要更高的采样率
    bool steep = std::abs(ax - bx) < std::abs(ay - by);
    if (steep)
    {
        std::swap(ax, ay);
        std::swap(bx, by);
    }
    //交换两个点再绘制
    if ( ax > bx )
    {
        std::swap(ax, bx);
        std::swap(ay, by);
    }
    //自适配采样率，同样可能需要应用于y
    int y = ay;
    int ierror = 0;
    /*int y = ay;
    float error = 0;*/
    /*float y = ay;*/
    for (int x = ax; x < bx; x ++)
    {
        //static_cast<float>用于做整型转化为浮点型,存在精度缺失
        if ( steep )
            framebuffer.set(y, x, color);
        else
            framebuffer.set(x, y, color);
        /*y += (by-ay) / static_cast<float>(bx-ax);*/
        /*error += std::abs(by-ay)/static_cast<float>(bx-ax);
        if (error>.5) 
        {
            y += by > ay ? 1 : -1;
            error -= 1.;
        }*/
        ierror += 2 * std::abs(by - ay);
        if ( ierror > bx - ax )
        {
            y += by > ay ? 1 : -1;
            ierror -= 2 * (bx - ax);
        }
    }
        
        
    
    //对于t的递增，涉及到采样率的问题，如果采样率过低，线可能出现空白区域
    //对于cx到ax的线，采样率需要满足 1 / (62 - 7) = 0.018
    /*for (float t = 0.; t <= 1.; t += .018)
    {
        //round是一个四舍五入
        int x = std::round(ax + (bx - ax) * t);
        int y = std::round(ay + (by - ay) * t);
        //在图像上画某个点
        framebuffer.set(x, y, color);
    }*/
}

int main(int argc, char** argv) {
    constexpr int width  = 64;
    constexpr int height = 64;
    TGAImage framebuffer(width, height, TGAImage::RGB);

    int ax =  7, ay =  3;
    int bx = 12, by = 37;
    int cx = 62, cy = 53;
    
    line(ax, ay, bx, by, framebuffer, blue);
    line(cx, cy, bx, by, framebuffer, green);
    line(cx, cy, ax, ay, framebuffer, yellow);
    line(ax, ay, cx, cy, framebuffer, red);
    
    /*std::srand(std::time({}));
    for (int i=0; i<(1<<24); i++) {
        int ax = rand()%width, ay = rand()%height;
        int bx = rand()%width, by = rand()%height;
        line(ax, ay, bx, by, framebuffer, { std::uint8_t(rand()%255), std::uint8_t(rand()%255),
                                    std::uint8_t(rand()%255), std::uint8_t(rand()%255) });
    }*/

    
    framebuffer.set(ax, ay, white);
    framebuffer.set(bx, by, white);
    framebuffer.set(cx, cy, white);

    framebuffer.write_tga_file("framebuffer.tga");
    return 0;
}

