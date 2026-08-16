// Diagnostic temporaire : mesure le yaw Hips et la direction des pas des
// clips de strafe, avec et sans la compensation (setStrafeCompensation).
// Reproduit la logique de Animator::computeBoneTransform.
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <cstdio>
#include <string>
#include <vector>
#include <unordered_map>
#include <cmath>

static glm::mat4 aiMatrixToGlm(const aiMatrix4x4& m) {
    glm::mat4 r;
    r[0][0] = m.a1; r[1][0] = m.a2; r[2][0] = m.a3; r[3][0] = m.a4;
    r[0][1] = m.b1; r[1][1] = m.b2; r[2][1] = m.b3; r[3][1] = m.b4;
    r[0][2] = m.c1; r[1][2] = m.c2; r[2][2] = m.c3; r[3][2] = m.c4;
    r[0][3] = m.d1; r[1][3] = m.d2; r[2][3] = m.d3; r[3][3] = m.d4;
    return r;
}

static std::string stripMixamoPrefix(const std::string& name) {
    const std::string prefix = "mixamorig";
    if (name.size() > prefix.size() &&
        name.compare(0, prefix.size(), prefix) == 0) {
        size_t i = prefix.size();
        while (i < name.size() && name[i] >= '0' && name[i] <= '9') i++;
        if (i < name.size() && name[i] == ':')
            return name.substr(i + 1);
    }
    return name;
}

static std::string normalizeNodeName(const std::string& name) {
    std::string n = stripMixamoPrefix(name);
    const std::string rotSuffix = "_$AssimpFbx$_Rotation";
    if (n.size() > rotSuffix.size() &&
        n.compare(n.size() - rotSuffix.size(), rotSuffix.size(), rotSuffix) == 0) {
        n = n.substr(0, n.size() - rotSuffix.size());
    }
    return n;
}

static float quatYawDeg(const glm::quat& q) {
    const glm::vec3 fwd = q * glm::vec3(0.0f, 0.0f, 1.0f);
    return glm::degrees(std::atan2(fwd.x, fwd.z));
}

static unsigned int findKeyIndex(float animTime, const aiQuatKey* keys, unsigned int numKeys) {
    if (numKeys == 0) return 0;
    for (unsigned int i = 0; i < numKeys - 1; i++) {
        if (animTime < static_cast<float>(keys[i + 1].mTime)) return i;
    }
    return numKeys - 1;
}

static glm::quat interpRot(float animTime, const aiNodeAnim* ch, const glm::quat& def,
                           float duration, bool loop) {
    glm::quat result = def;
    if (ch->mNumRotationKeys == 1) {
        aiQuaternion q = ch->mRotationKeys[0].mValue;
        result = glm::quat(q.w, q.x, q.y, q.z);
    } else if (ch->mNumRotationKeys > 1) {
        unsigned int idx = findKeyIndex(animTime, ch->mRotationKeys, ch->mNumRotationKeys);
        unsigned int nextIdx = idx + 1;
        if (nextIdx >= ch->mNumRotationKeys) {
            if (!loop) {
                const aiQuaternion& q = ch->mRotationKeys[idx].mValue;
                const glm::quat res(q.w, q.x, q.y, q.z);
                const float rl = glm::length(res);
                return (std::isfinite(rl) && rl > 0.0001f) ? glm::normalize(res) : def;
            }
            nextIdx = 0;
        }
        float t0 = static_cast<float>(ch->mRotationKeys[idx].mTime);
        float t1 = static_cast<float>(ch->mRotationKeys[nextIdx].mTime);
        if (nextIdx == 0 && loop) t1 += duration;
        float factor = (t1 > t0) ? (animTime - t0) / (t1 - t0) : 0.0f;
        factor = glm::clamp(factor, 0.0f, 1.0f);
        aiQuaternion q0 = ch->mRotationKeys[idx].mValue;
        aiQuaternion q1 = ch->mRotationKeys[nextIdx].mValue;
        glm::quat a(q0.w, q0.x, q0.y, q0.z);
        glm::quat b(q1.w, q1.x, q1.y, q1.z);
        float lenA = glm::length(a), lenB = glm::length(b);
        a = (std::isfinite(lenA) && lenA > 0.0001f) ? a / lenA : glm::quat(1,0,0,0);
        b = (std::isfinite(lenB) && lenB > 0.0001f) ? b / lenB : glm::quat(1,0,0,0);
        float dot = glm::dot(a, b);
        if (dot < 0.0f) { b = -b; dot = -dot; }
        if (dot > 0.9999f) result = glm::normalize(glm::mix(a, b, factor));
        else result = glm::slerp(a, b, factor);
    }
    float len = glm::length(result);
    if (!std::isfinite(len) || len < 0.0001f) {
        float dLen = glm::length(def);
        return (std::isfinite(dLen) && dLen > 0.0001f) ? glm::normalize(def) : glm::quat(1,0,0,0);
    }
    return glm::normalize(result);
}

// Reproduit repairRotationKeys (mode FBX phantom : garde i%4==0)
static void repairKeys(const aiAnimation* anim) {
    const float duration = static_cast<float>(anim->mDuration);
    for (unsigned int c = 0; c < anim->mNumChannels; c++) {
        aiNodeAnim* ch = anim->mChannels[c];
        if (!ch) continue;
        const unsigned int n = ch->mNumRotationKeys;
        if (n == 0) continue;
        bool fbxPhantomPattern = false;
        if (n >= 4) {
            const aiQuatKey& k1 = ch->mRotationKeys[1];
            const glm::quat q1(k1.mValue.w, k1.mValue.x, k1.mValue.y, k1.mValue.z);
            const float len1 = glm::length(q1);
            fbxPhantomPattern = std::isfinite(len1) &&
                                std::abs(k1.mValue.w) < 1e-6f &&
                                len1 > 1.5f && len1 < 4.0f;
        }
        if (fbxPhantomPattern) {
            unsigned int kept = 0;
            for (unsigned int i = 0; i < n; i++) {
                if (i % 4 != 0) continue;
                aiQuatKey& k = ch->mRotationKeys[i];
                const glm::quat q(k.mValue.w, k.mValue.x, k.mValue.y, k.mValue.z);
                const float len = glm::length(q);
                const bool quatOk = std::isfinite(len) && len > 0.0001f && len < 10.0f;
                if (!quatOk) continue;
                if (kept != i) ch->mRotationKeys[kept] = k;
                aiQuatKey& keptKey = ch->mRotationKeys[kept];
                keptKey.mValue.w /= len;
                keptKey.mValue.x /= len;
                keptKey.mValue.y /= len;
                keptKey.mValue.z /= len;
                kept++;
            }
            ch->mNumRotationKeys = kept;
            continue;
        }
        // filtre temporel
        unsigned int kept = 0;
        float lastTime = -FLT_MAX;
        for (unsigned int i = 0; i < n; i++) {
            aiQuatKey& k = ch->mRotationKeys[i];
            const float t = static_cast<float>(k.mTime);
            const glm::quat q(k.mValue.w, k.mValue.x, k.mValue.y, k.mValue.z);
            const float len = glm::length(q);
            const bool timeOk = std::isfinite(t) && t >= 0.0f && t <= duration + 1.0f &&
                                t >= lastTime && (kept == 0 || t > 0.1f);
            if (!timeOk) continue;
            const glm::quat qq(k.mValue.w, k.mValue.x, k.mValue.y, k.mValue.z);
            const float ql = glm::length(qq);
            if (!std::isfinite(ql) || ql < 1e-6f) continue;
            if (kept != i) ch->mRotationKeys[kept] = k;
            aiQuatKey& kk = ch->mRotationKeys[kept];
            kk.mValue.w /= ql; kk.mValue.x /= ql; kk.mValue.y /= ql; kk.mValue.z /= ql;
            lastTime = t;
            kept++;
        }
        ch->mNumRotationKeys = kept;
    }
}

struct Sample {
    glm::vec3 hips;
    glm::quat hipsRot;
    glm::vec3 lFoot, rFoot;
};

static void computePose(const aiScene* modelScene, const aiAnimation* anim,
                        bool usesRrTt, bool strafeComp, float time, Sample& out) {
    // channel map
    std::unordered_map<std::string, const aiNodeAnim*> chanMap;
    for (unsigned int i = 0; i < anim->mNumChannels; i++) {
        const aiNodeAnim* ch = anim->mChannels[i];
        if (!ch) continue;
        std::string name(ch->mNodeName.C_Str());
        chanMap[name] = ch;
        std::string norm = normalizeNodeName(name);
        if (norm != name) chanMap[norm] = ch;
    }

    glm::quat strafeLegComp(1.0f, 0.0f, 0.0f, 0.0f);
    const float duration = static_cast<float>(anim->mDuration);

    struct Frame {
        glm::mat4 global;
        glm::quat gRot;
    };

    // DFS iteratif
    std::vector<std::pair<const aiNode*, glm::mat4>> stack;
    stack.push_back({modelScene->mRootNode, glm::mat4(1.0f)});
    while (!stack.empty()) {
        auto [node, parent] = stack.back();
        stack.pop_back();
        const std::string nodeName = node->mName.C_Str();
        const std::string stripped = stripMixamoPrefix(nodeName);
        const glm::mat4 bindPose = aiMatrixToGlm(node->mTransformation);
        glm::mat4 nodeTransform = bindPose;

        auto chanIt = chanMap.find(nodeName);
        if (chanIt == chanMap.end()) {
            std::string normalized = normalizeNodeName(nodeName);
            if (normalized != nodeName) chanIt = chanMap.find(normalized);
        }
        if (chanIt != chanMap.end()) {
            const aiNodeAnim* ch = chanIt->second;
            const glm::quat bindRot = glm::quat_cast(glm::mat3(bindPose));
            glm::quat rot = interpRot(time, ch, bindRot, duration, true);
            glm::vec3 trans;
            if (usesRrTt) {
                trans = glm::vec3(bindPose[3]);
            } else {
                trans = glm::vec3(bindPose[3]); // translation du canal ignoree (root lock)
            }
            nodeTransform = glm::translate(glm::mat4(1.0f), trans) * glm::mat4_cast(rot);

            if (stripped == "Hips") {
                nodeTransform[3] = glm::vec4(glm::vec3(bindPose[3]), 1.0f);
                if (strafeComp) {
                    const glm::quat qOrig = glm::quat_cast(glm::mat3(nodeTransform));
                    glm::quat qZeroed = qOrig;
                    qZeroed.y = 0.0f;
                    const float lz = glm::length(qZeroed);
                    qZeroed = (lz > 0.0001f) ? glm::normalize(qZeroed)
                                             : glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
                    strafeLegComp = glm::inverse(qZeroed) * qOrig;
                }
                glm::quat q = glm::quat_cast(glm::mat3(nodeTransform));
                q.y = 0.0f;
                float len = glm::length(q);
                q = (len > 0.0001f) ? glm::normalize(q) : glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
                glm::vec3 trans2(nodeTransform[3]);
                nodeTransform = glm::mat4_cast(q);
                nodeTransform[3] = glm::vec4(trans2, 1.0f);
            }
        }

        if (strafeComp) {
            bool isLegRoot = false;
            if (usesRrTt) {
                isLegRoot = (stripped == "LeftUpLeg_$AssimpFbx$_Translation" ||
                             stripped == "RightUpLeg_$AssimpFbx$_Translation");
            } else {
                isLegRoot = (stripped == "LeftUpLeg" || stripped == "RightUpLeg");
            }
            if (isLegRoot) {
                nodeTransform = glm::mat4_cast(strafeLegComp) * nodeTransform;
            } else if (stripped == "Spine" || stripped == "Spine1" || stripped == "Spine2" ||
                       stripped == "Neck" || stripped == "Head") {
                glm::quat q = glm::quat_cast(glm::mat3(nodeTransform));
                q.y = 0.0f;
                const float len = glm::length(q);
                q = (len > 0.0001f) ? glm::normalize(q) : glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
                glm::vec3 tr(nodeTransform[3]);
                nodeTransform = glm::mat4_cast(q);
                nodeTransform[3] = glm::vec4(tr, 1.0f);
            }
        }

        if (nodeName.find("_$AssimpFbx$_PreRotation") != std::string::npos) {
            const glm::vec3 preTrans(nodeTransform[3]);
            nodeTransform = glm::translate(glm::mat4(1.0f), preTrans);
        }

        glm::mat4 globalTransform = parent * nodeTransform;
        glm::quat gRot = glm::quat_cast(glm::mat3(globalTransform));

        if (stripped == "Hips") {
            out.hips = glm::vec3(globalTransform[3]);
            out.hipsRot = gRot;
        } else if (stripped == "LeftFoot") {
            out.lFoot = glm::vec3(globalTransform[3]);
        } else if (stripped == "RightFoot") {
            out.rFoot = glm::vec3(globalTransform[3]);
        }

        for (unsigned int i = 0; i < node->mNumChildren; i++) {
            stack.push_back({node->mChildren[i], globalTransform});
        }
    }
}

int main(int argc, char** argv) {
    if (argc < 3) {
        printf("Usage: analyze_strafe <model> <anim...>\n");
        return 1;
    }
    Assimp::Importer modelImp;
    const aiScene* modelScene = modelImp.ReadFile(argv[1],
        aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenNormals);
    if (!modelScene || !modelScene->mRootNode) {
        printf("ERREUR chargement modele\n");
        return 1;
    }

    bool usesRrTt = false;
    {
        std::vector<const aiNode*> stack{ modelScene->mRootNode };
        while (!stack.empty()) {
            const aiNode* n = stack.back();
            stack.pop_back();
            if (std::string(n->mName.C_Str()).find("_$AssimpFbx$_Translation") != std::string::npos) {
                usesRrTt = true;
                break;
            }
            for (unsigned int c = 0; c < n->mNumChildren; c++)
                stack.push_back(n->mChildren[c]);
        }
        printf("Modele: %s  RrTt=%s\n", argv[1], usesRrTt ? "oui" : "non");
    }

    for (int a = 2; a < argc; a++) {
        Assimp::Importer imp;
        const aiScene* scene = imp.ReadFile(argv[a], 0);
        if (!scene || scene->mNumAnimations == 0) {
            printf("ERREUR chargement anim %s\n", argv[a]);
            continue;
        }
        const aiAnimation* anim = scene->mAnimations[0];
        const float duration = static_cast<float>(anim->mDuration);
        const int keyCount = anim->mNumChannels > 0 ? anim->mChannels[0]->mNumRotationKeys : 0;

        printf("\n=== %s  (dur=%.2f s, %u channels, cles ch0=%d) ===\n",
               argv[a], duration / 30.0, anim->mNumChannels, keyCount);

        // canaux
        printf("Channels:\n");
        for (unsigned int i = 0; i < anim->mNumChannels && i < 40; i++) {
            const aiNodeAnim* ch = anim->mChannels[i];
            printf("  [%u] '%s' rot=%u pos=%u\n", i, ch->mNodeName.C_Str(),
                   ch->mNumRotationKeys, ch->mNumPositionKeys);
        }

        // reparer les cles comme le jeu
        repairKeys(anim);

        // echantillonnage
        const int N = 16;
        Sample rawSamples[2][N]; // [comp off/on][t]
        for (int comp = 0; comp < 2; comp++) {
            for (int i = 0; i < N; i++) {
                float t = duration * static_cast<float>(i) / static_cast<float>(N);
                Sample s;
                computePose(modelScene, anim, usesRrTt, comp == 1, t, s);
                rawSamples[comp][i] = s;
            }
        }

        auto report = [&](int comp) {
            printf("--- compensation %s ---\n", comp ? "ON" : "OFF");
            printf("  t       hipsYaw  lFoot(rel hips, frame hips)     rFoot(rel hips, frame hips)\n");
            for (int i = 0; i < N; i++) {
                const Sample& s = rawSamples[comp][i];
                const float yaw = quatYawDeg(s.hipsRot);
                glm::mat3 invH = glm::mat3_cast(glm::inverse(s.hipsRot));
                glm::vec3 lr = invH * (s.lFoot - s.hips);
                glm::vec3 rr = invH * (s.rFoot - s.hips);
                printf("  %5.2f  %+7.1f   (%+6.2f, %+6.2f)          (%+6.2f, %+6.2f)\n",
                       duration * i / N, yaw, lr.x, lr.z, rr.x, rr.z);
            }
        };
        report(0);
        report(1);
    }
    return 0;
}
