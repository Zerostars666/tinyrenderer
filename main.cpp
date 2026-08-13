#include <cstdlib>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include "tgaimage.h"
#include <vector>

constexpr TGAColor white   = {255, 255, 255, 255};
constexpr TGAColor red     = {  0,   0, 255, 255};

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

struct Vector3
{
    double x = 0, y = 0, z = 0;
};
int main(int argc, char** argv) {
    constexpr int width  = 4096;
    constexpr int height = 4096;
    TGAImage framebuffer(width, height, TGAImage::RGB);
    const std::string filename = "C:/Users/6/Desktop/Learn/tiny/tinyrenderer/obj/diablo3_pose/diablo3_pose.obj";
    std::vector<Vector3> verts;
    std::vector<Vector3> faces;
    std::ifstream infile(filename);
    if (!infile.is_open()) std::cerr << "Error opening file " << filename << std::endl;
    
    std::string line;
    while (std::getline(infile, line))
    {
        std::istringstream iss(line);
        std::string kind;
        iss >> kind;                // 读取行首关键字：v / vt / vn / f ...
        if (kind == "v") {          // 只匹配顶点行，不会误匹配 vt、vn
            Vector3 v;
            iss >> v.x >> v.y >> v.z;
            verts.push_back(v);
        }
        else if ( kind == "f" )
        {
            Vector3 f;
            for (int k = 0; k < 3; k++) {            // 每个面固定 3 个顶点
                std::string token;
                iss >> token;                        // 例如 "2469/2630/2469"
                // 只取 '/' 前面的顶点下标，纹理/法线下标直接跳过
                //stoi字符串转数字
                int vi = std::stoi(token.substr(0, token.find('/')));
                if (k == 0)      f.x = vi - 1;       // OBJ 从 1 开始，转成 0 起始
                else if (k == 1) f.y = vi - 1;
                else             f.z = vi - 1;
            }
            faces.push_back(f);
        }
    }
    
        /*在此处对vetrs做归一化*/

    // 将模型坐标从 [-1,1] 归一化到 [0,1]，避免负坐标越界被 set() 丢弃
    for (int i = 0; i < (int)verts.size(); i++) {
        verts[i].x = (verts[i].x + 1) / 2.0;
        verts[i].y = (verts[i].y + 1) / 2.0;
        verts[i].z = (verts[i].z + 1) / 2.0;
    }
    
    for (int i = 0; i < faces.size(); i++)
    {
        int ax = (int)(verts[(int)faces[i].x].x * width) % (width);
        int ay = (int)(verts[(int)faces[i].x].y * height) % (height);
        int bx = (int)(verts[(int)faces[i].y].x * width) % (width);
        int by = (int)(verts[(int)faces[i].y].y * height) % (height);
        int cx = (int)(verts[(int)faces[i].z].x * width) % (width);
        int cy = (int)(verts[(int)faces[i].z].y * height) % (height);
        ::line(ax, ay, bx, by, framebuffer, red);
        ::line(bx, by, cx, cy, framebuffer, red);
        ::line(cx, cy, ax, ay, framebuffer, red);
        framebuffer.set(ax, ay, white);
        framebuffer.set(bx, by, white);
        framebuffer.set(cx, cy, white);
    }
    
    /*std::cout << "顶点总数 = " << verts.size() << "\n";
    for (int i = 0;i < (int)verts.size(); i++)
        std::cout << "v[" << i << "] = (" << verts[i].x << ", "
                  << verts[i].y << ", " << verts[i].z << ")\n";*/
    
    /*std::cout << "三角面总数 = " << faces.size() << "\n";
    for (int i = 0;i < (int)faces.size(); i++)
        std::cout << "face[" << i << "] = (" << faces[i].x << ", "
                  << faces[i].y << ", " << faces[i].z << ")\n";*/
    /*
    std::cout << "三角面对应顶点的坐标 = " << faces.size() << "\n";
    for (int i = 0;i < (int)faces.size(); i++) {
        Vector3 f  = faces[i];
        std::cout << "face[" << i << "]: ("
                  << verts[(int)f.x].x << ", " << verts[(int)f.x].y << ", " << verts[(int)f.x].z << ")  ("
                  << verts[(int)f.y].x << ", " << verts[(int)f.y].y << ", " << verts[(int)f.y].z << ")  ("
                  << verts[(int)f.z].x << ", " << verts[(int)f.z].y << ", " << verts[(int)f.z].z << ")\n";
    }*/
    framebuffer.write_tga_file("framebuffer.tga");
    return 0;
}

