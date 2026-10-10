// Console test for DffLoader - prints mesh stats.
#include <cstdio>
#include "DffLoader.h"

int main(int argc, char** argv) {
    const char* path = argc > 1 ? argv[1]
        : "C:\\Users\\fufid\\Documents\\Decomps\\gta-sa\\models\\generic\\arrow.DFF";
    DffModel m = DffLoader::Load(path);
    printf("valid=%d meshes=%zu\n", (int)m.valid, m.meshes.size());
    for (size_t i = 0; i < m.meshes.size(); i++) {
        const auto& mesh = m.meshes[i];
        printf("  mesh %zu: verts=%zu tris=%zu normals=%d uvs=%d\n",
            i, mesh.vertices.size(), mesh.indices.size() / 3,
            (int)mesh.hasNormals, (int)mesh.hasUVs);
        if (!mesh.vertices.empty()) {
            const auto& v = mesh.vertices[0];
            printf("    v0=(%.2f,%.2f,%.2f)\n", v.x, v.y, v.z);
        }
    }
    return m.valid ? 0 : 1;
}
