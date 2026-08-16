#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <cstdio>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    Assimp::Importer imp;
    const aiScene* s = imp.ReadFile(argv[1],
        aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenNormals);
    if (!s || !s->mRootNode) { printf("ERREUR\n"); return 1; }
    std::vector<const aiNode*> stack{ s->mRootNode };
    while (!stack.empty()) {
        const aiNode* n = stack.back();
        stack.pop_back();
        const std::string name = n->mName.C_Str();
        if (name.find("mixamorig") != std::string::npos ||
            name.find("Hip") != std::string::npos ||
            name.find("Leg") != std::string::npos ||
            name.find("Foot") != std::string::npos ||
            name.find("Spine") != std::string::npos ||
            name.find("Neck") != std::string::npos ||
            name.find("Head") != std::string::npos)
            printf("'%s'\n", name.c_str());
        for (unsigned int c = 0; c < n->mNumChildren; c++)
            stack.push_back(n->mChildren[c]);
    }
    return 0;
}
