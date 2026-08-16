#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cstdio>
#include <cmath>

static float yawDeg(const glm::quat& q) {
    const glm::vec3 fwd = q * glm::vec3(0.0f, 0.0f, 1.0f);
    return glm::degrees(std::atan2(fwd.x, fwd.z));
}

int main(int argc, char** argv) {
    Assimp::Importer imp;
    const aiScene* s = imp.ReadFile(argv[1],
        aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenNormals);
    if (!s || !s->mRootNode) { printf("ERREUR\n"); return 1; }
    // Parcours : root -> ... -> Hips, en imprimant chaque transform de bind
    const aiNode* n = s->mRootNode;
    int depth = 0;
    while (n && depth < 8) {
        glm::mat4 m;
        m[0][0]=n->mTransformation.a1; m[1][0]=n->mTransformation.a2; m[2][0]=n->mTransformation.a3; m[3][0]=n->mTransformation.a4;
        m[0][1]=n->mTransformation.b1; m[1][1]=n->mTransformation.b2; m[2][1]=n->mTransformation.b3; m[3][1]=n->mTransformation.b4;
        m[0][2]=n->mTransformation.c1; m[1][2]=n->mTransformation.c2; m[2][2]=n->mTransformation.c3; m[3][2]=n->mTransformation.c4;
        m[0][3]=n->mTransformation.d1; m[1][3]=n->mTransformation.d2; m[2][3]=n->mTransformation.d3; m[3][3]=n->mTransformation.d4;
        const glm::quat q = glm::quat_cast(glm::mat3(m));
        const glm::vec3 t(m[3]);
        printf("depth=%d '%s' transl=(%.3f, %.3f, %.3f) yaw=%.1f pitch=%.1f roll=%.1f\n",
               depth, n->mName.C_Str(), t.x, t.y, t.z, yawDeg(q),
               glm::degrees(std::asin(glm::clamp(2.0f*(q.w*q.x - q.y*q.z), -1.0f, 1.0f))),
               glm::degrees(std::atan2(2.0f*(q.w*q.z + q.x*q.y), 1.0f - 2.0f*(q.y*q.y + q.z*q.z))));
        // descendre vers Hips
        const aiNode* next = nullptr;
        for (unsigned int c = 0; c < n->mNumChildren; c++) {
            const std::string name = n->mChildren[c]->mName.C_Str();
            if (name.find("Hips") != std::string::npos ||
                name.find("mixamorig") != std::string::npos) { next = n->mChildren[c]; break; }
        }
        n = next;
        depth++;
    }
    return 0;
}
