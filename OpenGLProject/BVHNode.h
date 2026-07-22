#pragma once

#include <vector>
#include <algorithm>
#include <glm/glm.hpp>

#include "AABB.h" // Ta structure AABB existante

struct StaticBox {
    std::string name;
    AABB aabb;
};

struct BVHNode {
    AABB box;
    int leftChild = -1;  // Indice dans le tableau de nœuds (-1 si feuille)
    int rightChild = -1; // Indice dans le tableau de nœuds
    int firstPrimOffset = -1; // Indice dans le tableau réorganisé de StaticBox
    int primCount = 0;       // Si > 0, c'est une feuille

    bool isLeaf() const { return primCount > 0; }
};

class BVH {
public:
    void build(std::vector<StaticBox> boxes) {
        m_boxes = std::move(boxes);
        m_nodes.clear();
        if (m_boxes.empty()) return;

        m_nodes.reserve(m_boxes.size() * 2);
        buildRecursive(0, static_cast<int>(m_boxes.size()));
    }

    // Traverse le BVH pour trouver toutes les AABB intersectées par la sphère
    template<typename Func>
    void querySphere(const glm::vec3& center, float radius, Func&& callback) const {
        if (m_nodes.empty()) return;

        int stack[64];
        int stackPtr = 0;
        stack[stackPtr++] = 0; // Commencer par la racine (index 0)

        while (stackPtr > 0) {
            int nodeIdx = stack[--stackPtr];
            const BVHNode& node = m_nodes[nodeIdx];

            // Test rapide sphère / AABB englobante du nœud
            if (!node.box.intersectsSphere(center, radius)) {
                continue;
            }

            if (node.isLeaf()) {
                // Tester toutes les primitives de cette feuille
                for (int i = 0; i < node.primCount; ++i) {
                    callback(m_boxes[node.firstPrimOffset + i]);
                }
            }
            else {
                // Pousser les enfants sur la pile
                if (node.leftChild != -1)  stack[stackPtr++] = node.leftChild;
                if (node.rightChild != -1) stack[stackPtr++] = node.rightChild;
            }
        }
    }

    void clear() {
        m_boxes.clear();
        m_nodes.clear();
    }

private:
    std::vector<StaticBox> m_boxes;
    std::vector<BVHNode> m_nodes;

    int buildRecursive(int start, int end) {
        int nodeIdx = static_cast<int>(m_nodes.size());
        m_nodes.push_back({});

        // 1. Calculer l'AABB englobant toutes les primitives de ce nœud
        AABB bounds;
        for (int i = start; i < end; ++i) {
            bounds.expand(m_boxes[i].aabb);
        }

        int count = end - start;
        // Condition d'arrêt (feuille avec 2 objets max par feuille par exemple)
        if (count <= 2) {
            m_nodes[nodeIdx].box = bounds;
            m_nodes[nodeIdx].firstPrimOffset = start;
            m_nodes[nodeIdx].primCount = count;
            return nodeIdx;
        }

        // 2. Découpage sur l'axe le plus large (SAH simplifié / Midpoint)
        glm::vec3 size = bounds.max - bounds.min;
        int axis = 0;
        if (size.y > size.x) axis = 1;
        if (size.z > size[axis]) axis = 2;

        float mid = (bounds.min[axis] + bounds.max[axis]) * 0.5f;

        // Partitionner les objets selon leur centre sur l'axe choisi
        auto midIter = std::partition(m_boxes.begin() + start, m_boxes.begin() + end,
            [axis, mid](const StaticBox& sb) {
                return sb.aabb.center()[axis] < mid;
            });

        int midIdx = static_cast<int>(std::distance(m_boxes.begin(), midIter));

        // Empêcher la récursion infinie si tous les objets finissent d'un seul côté
        if (midIdx == start || midIdx == end) {
            midIdx = start + count / 2;
        }

        // 3. Construction des enfants
        int left = buildRecursive(start, midIdx);
        int right = buildRecursive(midIdx, end);

        m_nodes[nodeIdx].box = bounds;
        m_nodes[nodeIdx].leftChild = left;
        m_nodes[nodeIdx].rightChild = right;
        m_nodes[nodeIdx].primCount = 0; // Pas une feuille

        return nodeIdx;
    }
}; #pragma once
