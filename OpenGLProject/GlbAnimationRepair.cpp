#include "GlbAnimationRepair.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

namespace {
using json = nlohmann::json;

struct GlbAnimationData {
    json document;
    std::vector<std::uint8_t> binary;
};

bool readGlb(const std::string& path, GlbAnimationData& result) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;

    const std::vector<std::uint8_t> bytes(
        (std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (bytes.size() < 12) return false;

    const auto readU32 = [&bytes](std::size_t offset) -> std::uint32_t {
        return static_cast<std::uint32_t>(bytes[offset]) |
               (static_cast<std::uint32_t>(bytes[offset + 1]) << 8u) |
               (static_cast<std::uint32_t>(bytes[offset + 2]) << 16u) |
               (static_cast<std::uint32_t>(bytes[offset + 3]) << 24u);
    };

    if (readU32(0) != 0x46546C67u || readU32(4) != 2u) return false;

    bool gotJson = false;
    std::size_t offset = 12;
    while (offset + 8 <= bytes.size()) {
        const std::uint32_t chunkLength = readU32(offset);
        const std::uint32_t chunkType = readU32(offset + 4);
        offset += 8;
        if (chunkLength > bytes.size() - offset) return false;

        if (chunkType == 0x4E4F534Au) { // JSON
            const std::string text(reinterpret_cast<const char*>(bytes.data() + offset), chunkLength);
            result.document = json::parse(text, nullptr, false);
            if (result.document.is_discarded()) return false;
            gotJson = true;
        } else if (chunkType == 0x004E4942u) { // BIN\\0
            result.binary.assign(bytes.begin() + static_cast<std::ptrdiff_t>(offset),
                                 bytes.begin() + static_cast<std::ptrdiff_t>(offset + chunkLength));
        }
        offset += chunkLength;
    }

    return gotJson && !result.binary.empty();
}

bool readFloatAccessor(const json& document, const std::vector<std::uint8_t>& binary,
                       int accessorIndex, unsigned int components, std::vector<float>& values) {
    if (!document.contains("accessors") || !document["accessors"].is_array() ||
        accessorIndex < 0 || accessorIndex >= static_cast<int>(document["accessors"].size())) {
        return false;
    }

    const json& accessor = document["accessors"][accessorIndex];
    if (!accessor.contains("bufferView") || !accessor.contains("count") ||
        accessor.value("componentType", 0) != 5126) { // FLOAT
        return false;
    }

    const std::string type = accessor.value("type", "");
    if ((components == 1 && type != "SCALAR") ||
        (components == 4 && type != "VEC4")) {
        return false;
    }

    const int viewIndex = accessor.value("bufferView", -1);
    if (!document.contains("bufferViews") || !document["bufferViews"].is_array() ||
        viewIndex < 0 || viewIndex >= static_cast<int>(document["bufferViews"].size())) {
        return false;
    }

    const json& view = document["bufferViews"][viewIndex];
    const std::size_t count = accessor.value("count", 0u);
    const std::size_t accessorOffset = accessor.value("byteOffset", 0u);
    const std::size_t viewOffset = view.value("byteOffset", 0u);
    const std::size_t elementSize = static_cast<std::size_t>(components) * sizeof(float);
    const std::size_t stride = view.value("byteStride", elementSize);
    if (stride < elementSize || count == 0) return false;

    if (viewOffset > binary.size() || accessorOffset > binary.size() - viewOffset) return false;
    const std::size_t start = viewOffset + accessorOffset;
    if (start > binary.size() || elementSize > binary.size() - start) return false;
    if (count > 1 && (count - 1) > (binary.size() - start - elementSize) / stride) {
        return false;
    }
    const std::size_t end = start + (count - 1) * stride + elementSize;
    if (end > binary.size()) return false;

    if (count > std::numeric_limits<std::size_t>::max() / components) return false;
    values.resize(count * components);
    for (std::size_t i = 0; i < count; ++i) {
        for (unsigned int component = 0; component < components; ++component) {
            const std::size_t byteOffset = start + i * stride + component * sizeof(float);
            std::memcpy(&values[i * components + component], binary.data() + byteOffset, sizeof(float));
        }
    }
    return true;
}
} // namespace

bool patchGlbAnimationRotations(const aiScene* scene, const std::string& path) {
    if (!scene || scene->mNumAnimations == 0) return false;

    std::string extension = std::filesystem::path(path).extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (extension != ".glb") return false;

    GlbAnimationData glb;
    if (!readGlb(path, glb) || !glb.document.contains("animations") ||
        !glb.document["animations"].is_array()) {
        std::cerr << "[Model] Impossible de lire les animations brutes du GLB: " << path << std::endl;
        return false;
    }

    unsigned int restoredChannels = 0;
    unsigned int restoredKeys = 0;
    const json& sourceAnimations = glb.document["animations"];

    for (std::size_t sourceIndex = 0; sourceIndex < sourceAnimations.size(); ++sourceIndex) {
        const json& sourceAnimation = sourceAnimations[sourceIndex];
        const std::string sourceName = sourceAnimation.value("name", "");
        const aiAnimation* targetAnimation = nullptr;

        for (unsigned int i = 0; i < scene->mNumAnimations; ++i) {
            if (!sourceName.empty() && sourceName == scene->mAnimations[i]->mName.C_Str()) {
                targetAnimation = scene->mAnimations[i];
                break;
            }
        }
        if (!targetAnimation && sourceIndex < scene->mNumAnimations) {
            targetAnimation = scene->mAnimations[sourceIndex];
        }
        if (!targetAnimation || !sourceAnimation.contains("channels") ||
            !sourceAnimation["channels"].is_array() ||
            !sourceAnimation.contains("samplers") || !sourceAnimation["samplers"].is_array()) {
            continue;
        }

        std::unordered_map<std::string, aiNodeAnim*> channels;
        for (unsigned int i = 0; i < targetAnimation->mNumChannels; ++i) {
            channels[targetAnimation->mChannels[i]->mNodeName.C_Str()] = targetAnimation->mChannels[i];
        }

        for (const json& sourceChannel : sourceAnimation["channels"]) {
            if (!sourceChannel.contains("target") ||
                sourceChannel["target"].value("path", "") != "rotation") {
                continue;
            }
            const int nodeIndex = sourceChannel["target"].value("node", -1);
            const int samplerIndex = sourceChannel.value("sampler", -1);
            if (nodeIndex < 0 || !glb.document.contains("nodes") ||
                nodeIndex >= static_cast<int>(glb.document["nodes"].size()) ||
                samplerIndex < 0 || samplerIndex >= static_cast<int>(sourceAnimation["samplers"].size())) {
                continue;
            }

            const std::string nodeName = glb.document["nodes"][nodeIndex].value("name", "");
            auto channelIt = channels.find(nodeName);
            if (nodeName.empty() || channelIt == channels.end()) continue;

            const json& sampler = sourceAnimation["samplers"][samplerIndex];
            const int inputAccessor = sampler.value("input", -1);
            const int outputAccessor = sampler.value("output", -1);
            std::vector<float> times;
            std::vector<float> rotations;
            if (!readFloatAccessor(glb.document, glb.binary, inputAccessor, 1, times) ||
                !readFloatAccessor(glb.document, glb.binary, outputAccessor, 4, rotations) ||
                times.size() * 4 != rotations.size()) {
                continue;
            }

            float ticksPerSecond = static_cast<float>(targetAnimation->mTicksPerSecond);
            if (!std::isfinite(ticksPerSecond) || ticksPerSecond <= 0.001f) ticksPerSecond = 1.0f;

            std::vector<aiQuatKey> repairedKeys;
            repairedKeys.reserve(times.size());
            bool valid = true;
            for (std::size_t key = 0; key < times.size(); ++key) {
                const float x = rotations[key * 4 + 0];
                const float y = rotations[key * 4 + 1];
                const float z = rotations[key * 4 + 2];
                const float w = rotations[key * 4 + 3];
                const float length = std::sqrt(x * x + y * y + z * z + w * w);
                if (!std::isfinite(times[key]) || !std::isfinite(length) || length <= 0.0001f) {
                    valid = false;
                    break;
                }
                repairedKeys.emplace_back(static_cast<double>(times[key]) * ticksPerSecond,
                                          aiQuaternion(w / length, x / length, y / length, z / length));
            }
            if (!valid || repairedKeys.empty()) continue;

            aiNodeAnim* targetChannel = channelIt->second;
            if (!targetChannel) {
                std::cerr << "[Model] Canal d'animation null pour le noeud \"" << nodeName << "\" — ignore" << std::endl;
                continue;
            }
            // In-place overwrite : on ne delete[] JAMAIS la memoire allouee
            // par Assimp (CRT different -> heap corruption). On ecrase les
            // cles corrompues dans le buffer existant et on reduit le count.
            unsigned int newSize = std::min(targetChannel->mNumRotationKeys,
                                            static_cast<unsigned int>(repairedKeys.size()));
            std::copy_n(repairedKeys.begin(), newSize, targetChannel->mRotationKeys);
            targetChannel->mNumRotationKeys = newSize;
            restoredChannels++;
            restoredKeys += newSize;
        }
    }

    if (restoredChannels > 0) {
        std::cout << "[Model] GLB rotations restaurees: " << restoredChannels
                  << " channels, " << restoredKeys << " cles" << std::endl;
    }
    return restoredChannels > 0;
}
