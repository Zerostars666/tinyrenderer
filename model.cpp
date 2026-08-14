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
        if (kind == "v") {
            vec3 v;
            iss >> v.x >> v.y >> v.z;
            verts.push_back(v);
        } else if (kind == "f") {
            for (int k = 0; k < 3; k++) {   // each face is a triangle
                std::string token;
                iss >> token;               // token looks like "2469/2630/2469"
                facet_vrt.push_back(std::stoi(token.substr(0, token.find('/'))) - 1);
            }
        }
    }
    std::cerr << "# v# " << nverts() << " f# " << nfaces() << std::endl;
}

int Model::nverts() const { return verts.size(); }
int Model::nfaces() const { return facet_vrt.size() / 3; }
vec3 Model::vert(const int i) const { return verts[i]; }
vec3 Model::vert(const int iface, const int nthvert) const { return verts[facet_vrt[iface*3 + nthvert]]; }
