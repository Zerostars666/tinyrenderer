#include <fstream>
#include <sstream>
#include <iostream>
#include "model.h"

Model::Model(const std::string filename) {
    std::ifstream in(filename);
    if (in.fail()) return;
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream iss(line);
        std::string kind;
        iss >> kind;
        if (kind == "v") {              // 顶点坐标
            vec3 v;
            iss >> v.x >> v.y >> v.z;
            verts.push_back(v);
        } else if (kind == "vn") {      // 顶点法线
            vec3 n;
            iss >> n.x >> n.y >> n.z;
            vns.push_back(n);
        } else if (kind == "f") {       // 三角面（每行 3 个 v/vt/vn 三元组）
            for (int k = 0; k < 3; k++) {
                std::string token;
                iss >> token;           // token 形如 "24/1/24"：顶点/纹理/法线 下标
                // 顶点下标：取第一个 '/' 之前
                facet_vrt.push_back(std::stoi(token.substr(0, token.find('/'))) - 1);
                // 法线下标：取最后一个 '/' 之后
                facet_vn.push_back(std::stoi(token.substr(token.find_last_of('/') + 1)) - 1);
            }
        }
    }
    std::cerr << "# v# " << nverts() << " vn# " << nnormals() << " f# " << nfaces() << std::endl;
}

int Model::nverts() const { return verts.size(); }
int Model::nfaces() const { return facet_vrt.size() / 3; }
int Model::nnormals() const { return vns.size(); }
vec3 Model::vert(const int i) const { return verts[i]; }
vec3 Model::vert(const int iface, const int nthvert) const { return verts[facet_vrt[iface*3 + nthvert]]; }
vec3 Model::normal(const int i) const { return vns[i]; }
vec3 Model::normal(const int iface, const int nthvert) const { return vns[facet_vn[iface*3 + nthvert]]; }
