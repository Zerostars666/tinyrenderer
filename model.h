#pragma once
#include <string>
#include <vector>
#include "geometry.h"   // vec2/vec3/vec4、mat<4,4> 等数学类型
#include "tgaimage.h"   // TGAImage、TGAColor（法线贴图就用它存）

// Model：负责解析 .obj 文件，并把"模型数据"提供给渲染器（顶点、法线、纹理坐标、法线贴图）
class Model {
    // ─── 数据数组：每一类数据各自一个数组 ───
    // .obj 里的 v 行（顶点坐标），存成 vec4，w=1（齐次坐标，方便和 4x4 矩阵相乘）
    std::vector<vec4> verts = {};
    // .obj 里的 vn 行（顶点法线），存成 vec4，w=0（方向向量，矩阵变换时不做平移）
    std::vector<vec4> norms = {};
    // .obj 里的 vt 行（纹理坐标 u,v），存成 vec2
    std::vector<vec2> tex = {};

    // ─── 索引数组：每个三角形 3 个角点，各存一个下标 ───
    // 注意：上面三个数组是"去重后的数据池"，数量各不相同；
    // 下面三个数组记录"第几个面、第几个角点"对应数据池里的第几个，所以大小都是 nfaces()*3
    std::vector<int> facet_vrt = {}; // 每个角点 → verts 的下标
    std::vector<int> facet_nrm = {}; // 每个角点 → norms 的下标
    std::vector<int> facet_tex = {}; // 每个角点 → tex  的下标

    // 法线贴图：由 xxx.obj 同目录下的 xxx_nm.tga 加载而来
    TGAImage normalmap = {};
    // 漫反射颜色贴图：由 xxx_diffuse.tga 加载而来（模型的基础颜色）
    TGAImage diffusemap = {};
    // 高光权重贴图：由 xxx_spec.tga 加载而来（控制每个像素高光的强弱）
    TGAImage specularmap = {};

public:
    // 构造：读入 .obj 文件，解析 v/vn/vt/f 四类行，并加载法线贴图
    Model(const std::string filename);

    // 顶点总数（verts.size()）
    int nverts() const;
    // 三角形总数（facet_vrt.size()/3）
    int nfaces() const;

    // 第 i 个顶点坐标（i 是数据池下标，0 <= i < nverts()）
    vec4 vert(const int i) const;
    // 第 iface 个三角形、第 nthvert 个角点的顶点坐标（nthvert = 0/1/2）
    vec4 vert(const int iface, const int nthvert) const;

    // 第 iface 个三角形、第 nthvert 个角点的顶点法线（来自 .obj 的 vn）
    vec4 normal(const int iface, const int nthvert) const;
    // 按纹理坐标 uv 从法线贴图里采样出一个法线（逐像素，用于法线贴图）
    vec4 normal(const vec2 &uv) const;

    // 第 iface 个三角形、第 nthvert 个角点的纹理坐标（uv）
    vec2 uv(const int iface, const int nthvert) const;

    // 返回漫反射贴图（供片元着色器采样）
    const TGAImage& diffuse()  const;
    // 返回高光贴图（供片元着色器采样）
    const TGAImage& specular() const;
};