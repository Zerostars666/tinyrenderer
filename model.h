#pragma once
#include <string>
#include <vector>
#include "geometry.h"

class Model {
    std::vector<vec3> verts = {};    // 顶点数组（v）
    std::vector<vec3> vns = {};      // 顶点法线数组（vn）
    std::vector<int> facet_vrt = {}; // 每个三角面的顶点下标
    std::vector<int> facet_vn = {};  // 每个三角面的法线下标
public:
    Model(const std::string filename);
    int nverts() const;    // 顶点数量
    int nfaces() const;    // 三角形数量
    int nnormals() const;  // 法线数量
    vec3 vert(const int i) const;                          // 0 <= i < nverts()
    vec3 vert(const int iface, const int nthvert) const;   // 0 <= iface < nfaces(), 0 <= nthvert < 3
    vec3 normal(const int i) const;                        // 0 <= i < nnormals()
    vec3 normal(const int iface, const int nthvert) const; // 面 iface 的第 nthvert 个顶点的法线
};
