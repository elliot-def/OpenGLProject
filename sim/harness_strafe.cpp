// Harness : joue les clips de strafe avec le VRAI Animator (compile depuis
// OpenGLProject/Animator.cpp) et mesure la direction des pas des pieds.
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cstdio>
#include <memory>
#include <string>
#include <cmath>
#include "Model.h"
#include "Animator.h"

static float quatYawDeg(const glm::quat& q) {
    const glm::vec3 fwd = q * glm::vec3(0.0f, 0.0f, 1.0f);
    return glm::degrees(std::atan2(fwd.x, fwd.z));
}

// Retire le prefixe "mixamorigN:" et les suffixes RrTt pour comparer les noms.
static std::string strip(const std::string& n) {
    std::string s = n;
    const size_t p = s.find(':');
    if (p != std::string::npos && s.rfind("mixamorig", 0) == 0) s = s.substr(p + 1);
    const size_t t = s.find("_$AssimpFbx$_Translation");
    if (t != std::string::npos) s = s.substr(0, t);
    const size_t r = s.find("_$AssimpFbx$_PreRotation");
    if (r != std::string::npos) s = s.substr(0, r);
    const size_t ro = s.find("_$AssimpFbx$_Rotation");
    if (ro != std::string::npos) s = s.substr(0, ro);
    return s;
}

// Trouve le nom reel d'un noeud par nom depouille (ex: "Hips").
static std::string findNode(const aiNode* root, const std::string& want) {
    std::vector<const aiNode*> stack{ root };
    while (!stack.empty()) {
        const aiNode* n = stack.back();
        stack.pop_back();
        if (strip(n->mName.C_Str()) == want) return n->mName.C_Str();
        for (unsigned int c = 0; c < n->mNumChildren; c++)
            stack.push_back(n->mChildren[c]);
    }
    return "";
}

int main(int argc, char** argv) {
    if (argc < 3) {
        printf("Usage: harness_strafe <model> <anim...>\n");
        return 1;
    }
    Assimp::Importer modelImp;
    const aiScene* modelScene = modelImp.ReadFile(argv[1],
        aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenNormals);
    if (!modelScene || !modelScene->mRootNode) {
        printf("ERREUR modele\n");
        return 1;
    }
    Model model;
    model.setScene(modelScene);

    std::vector<std::unique_ptr<Assimp::Importer>> importers;
    for (int a = 2; a < argc; a++) {
        auto imp = std::make_unique<Assimp::Importer>();
        const aiScene* s = imp->ReadFile(argv[a], 0);
        if (!s) { printf("ERREUR anim %s\n", argv[a]); return 1; }
        for (unsigned int i = 0; i < s->mNumAnimations; i++)
            model.addExternalAnimation(s->mAnimations[i]);
        importers.push_back(std::move(imp));
    }

    const size_t base = modelScene->mNumAnimations;
    const std::string hipsName = findNode(modelScene->mRootNode, "Hips");
    const std::string lFootName = findNode(modelScene->mRootNode, "LeftFoot");
    const std::string rFootName = findNode(modelScene->mRootNode, "RightFoot");
    if (hipsName.empty() || lFootName.empty() || rFootName.empty()) {
        printf("Noeuds introuvables: Hips='%s' LFoot='%s' RFoot='%s'\n",
               hipsName.c_str(), lFootName.c_str(), rFootName.c_str());
        return 1;
    }
    printf("Noeuds: Hips='%s' LFoot='%s' RFoot='%s'\n",
           hipsName.c_str(), lFootName.c_str(), rFootName.c_str());
    Animator anim;
    anim.setup(&model);

    for (size_t k = 0; k < model.numExternal(); k++) {
        const size_t idx = base + k;
        const aiAnimation* a = model.getAnimation(idx);
        const float duration = static_cast<float>(a->mDuration);
        const float fps = (a->mTicksPerSecond > 0.001) ? static_cast<float>(a->mTicksPerSecond) : 30.0f;
        printf("\n=== %s (idx=%zu, dur=%.2fs, fps=%.1f) ===\n",
               argv[2 + k], idx, duration / fps, fps);

        for (int comp = 0; comp < 2; comp++) {
            anim.setStrafeCompensation(comp == 1);
            anim.playAnimation(static_cast<unsigned int>(idx), true);
            printf("--- compensation %s ---\n", comp ? "ON" : "OFF");
            printf("  t      hipsYaw   lFoot(rel hips X,Z)   rFoot(rel hips X,Z)\n");
            const int N = 16;
            float lastT = 0.0f;
            for (int i = 0; i <= N; i++) {
                const float target = duration * static_cast<float>(i) / static_cast<float>(N);
                anim.update(target - lastT);
                lastT = target;
                const glm::mat4 hM = anim.getGlobalNodeTransform(hipsName);
                const glm::mat4 lM = anim.getGlobalNodeTransform(lFootName);
                const glm::mat4 rM = anim.getGlobalNodeTransform(rFootName);
                const glm::vec3 h(hM[3]), l(lM[3]), r(rM[3]);
                const glm::quat hq = glm::quat_cast(glm::mat3(hM));
                const glm::mat3 invH = glm::mat3_cast(glm::inverse(hq));
                const glm::vec3 lr = invH * (l - h);
                const glm::vec3 rr = invH * (r - h);
                printf("  %5.2f  %+7.1f  (%+6.2f, %+6.2f)   (%+6.2f, %+6.2f)\n",
                       target, quatYawDeg(hq), lr.x, lr.z, rr.x, rr.z);
            }
        }
    }
    return 0;
}
