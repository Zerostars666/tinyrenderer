#include <fstream>
#include <sstream>
#include "model.h"

Model::Model(const std::string filename) {
    std::ifstream in;
    in.open(filename, std::ifstream::in);
    if (in.fail()) return;
    std::string line;
    while (!in.eof()) {
        std::getline(in, line);
        std::istringstream iss(line.c_str());
        char trash;
        if (!line.compare(0, 2, "v ")) {
            iss >> trash;
            vec4 v = {0,0,0,1};
            for (int i : {0,1,2}) iss >> v[i];
            verts.push_back(v);
        } else if (!line.compare(0, 3, "vn ")) {
            iss >> trash >> trash;
            vec4 n;
            for (int i : {0,1,2}) iss >> n[i];
            norms.push_back(normalized(n));
        } else if (!line.compare(0, 3, "vt ")) {
            iss >> trash >> trash;
            vec2 uv;
            for (int i : {0,1}) iss >> uv[i];
            tex.push_back({uv.x, 1-uv.y});
        } else if (!line.compare(0, 2, "f ")) {
            int f,t,n, cnt = 0;
            iss >> trash;
            while (iss >> f >> trash >> t >> trash >> n) {
                facet_vrt.push_back(--f);
                facet_tex.push_back(--t);
                facet_nrm.push_back(--n);
                cnt++;
            }
            if (3!=cnt) {
                std::cerr << "Error: the obj file is supposed to be triangulated" << std::endl;
                return;
            }
        }
    }
    std::cerr << "# v# " << nverts() << " f# "  << nfaces() << std::endl;
    auto load_texture = [&filename](const std::string suffix, TGAImage &img) {
        size_t dot = filename.find_last_of(".");
        if (dot==std::string::npos) return;
        std::string texfile = filename.substr(0,dot) + suffix;
        std::cerr << "texture file " << texfile << " loading " << (img.read_tga_file(texfile.c_str()) ? "ok" : "failed") << std::endl;
    };
    load_texture("_diffuse.tga", diffusemap);  // 加载漫反射颜色贴图
    load_texture("_nm.tga",      normalmap);   // 加载法线贴图
    load_texture("_spec.tga",    specularmap); // 加载高光权重贴图
}

int Model::nverts() const { return verts.size(); }
int Model::nfaces() const { return facet_vrt.size()/3; }

vec4 Model::vert(const int i) const {
    return verts[i];
}

vec4 Model::vert(const int iface, const int nthvert) const {
    return verts[facet_vrt[iface*3+nthvert]];
}

vec4 Model::normal(const int iface, const int nthvert) const {
    return norms[facet_nrm[iface*3+nthvert]];
}

vec4 Model::normal(const vec2 &uv) const {
    //由于前面进行了load_texture,此处normalmap可以进行采样了，采集到对应像素点的法线
    TGAColor c = normalmap.get(uv[0]*normalmap.width(), uv[1]*normalmap.height());
    //obj/diablo3_pose/diablo3_pose_nm.tga是一张图片，图片的每个像素点对应着RGB，区间是[0,255]
    //这里存储的信息不是图片颜色什么的，每一个像素点实际上是一个法线，我们需要将[0,255]化成法线的格式[-1,1]
    //光照公式 漫反射 = n·l、高光 = r·v 都依赖法线 n。法线方向稍微偏一点，亮暗就会变化。
    //轮廓、剪影不会变（几何没动），只有表面明暗变了。这是视错觉。
    //注意这里索引顺序是从2 - 0，而不是 0 - 2， 因为法线贴图给的就是反的
    return vec4{(double)c[2],(double)c[1],(double)c[0],0}*2./255. - vec4{1,1,1,0};
}

vec2 Model::uv(const int iface, const int nthvert) const {
    return tex[facet_tex[iface*3+nthvert]];
}

const TGAImage& Model::diffuse()  const { return diffusemap;  }
const TGAImage& Model::specular() const { return specularmap; }

