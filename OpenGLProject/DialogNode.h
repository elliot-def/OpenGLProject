#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

// ─────────────────────────────────────────────────────────────────────────────
// DialogChoice : un choix dans un nœud de dialog
// ─────────────────────────────────────────────────────────────────────────────
struct DialogChoice {
    std::string label;       // texte affiché (ex: "Oui", "Non merci")
    std::string nextNodeId;  // ID du nœud cible si ce choix est sélectionné
};

// ─────────────────────────────────────────────────────────────────────────────
// DialogNode : un nœud de l'arbre de dialog
//
// Chaque nœud contient un texte (ce que dit le PNJ) et optionnellement
// des choix qui mènent à d'autres nœuds.
// Si choices est vide, le nœud est une feuille → fin du dialog.
// ─────────────────────────────────────────────────────────────────────────────
struct DialogNode {
    std::string id;                    // identifiant unique du nœud
    std::string speakerName;           // nom du PNJ qui parle
    std::string text;                  // texte affiché (support multiligne via \n)
    std::vector<DialogChoice> choices; // choix disponibles (vide = fin)
    bool isEnd = false;                // true = nœud terminal (pas de choix)
};

// ─────────────────────────────────────────────────────────────────────────────
// DialogTree : conteneur de l'arbre de dialog complet
//
// Usage :
//   DialogTree tree;
//   tree.addNode({"root", "Garde", "Bonjour voyageur !", {
//       {"Qui es-tu ?", "who"},
//       {"Au revoir",  "bye"}
//   }});
//   tree.addNode({"who", "Garde", "Je suis le garde du village.", {}, true});
//   tree.addNode({"bye", "Garde", "Bonne route !", {}, true});
//   tree.setRoot("root");
// ─────────────────────────────────────────────────────────────────────────────
class DialogTree {
public:
    DialogTree() = default;

    void addNode(const DialogNode& node) {
        m_nodes[node.id] = node;
    }

    void setRoot(const std::string& id) {
        m_rootId = id;
    }

    const DialogNode* getNode(const std::string& id) const {
        auto it = m_nodes.find(id);
        return (it != m_nodes.end()) ? &it->second : nullptr;
    }

    const DialogNode* getRoot() const {
        return getNode(m_rootId);
    }

    const std::string& getRootId() const { return m_rootId; }

    bool isEmpty() const { return m_nodes.empty(); }

    // Crée un dialog d'exemple (PNJ de test)
    static DialogTree createExample() {
        DialogTree tree;
        tree.addNode({"root", "Megan",
            "Hey ! Bienvenue dans ce monde !\nTu viens d'arriver, pas vrai ?",
            {{"Oui, je suis nouveau !", "newbie"},
             {"Je connais deja les lieux.", "veteran"},
             {"... (partir)", "bye"}}});
        tree.addNode({"newbie", "Megan",
            "Je m'en doutais ! Laisse-moi te montrer\nles alentours. Tu vas adorer !",
            {{"Montre-moi !", "tour"},
             {"Plus tard peut-etre.", "bye"}}});
        tree.addNode({"veteran", "Megan",
            "Ah, un habitué ! Alors tu sais que\ncet endroit regorge de secrets...",
            {{"Quels secrets ?", "secrets"},
             {"Je dois y aller.", "bye"}}});
        tree.addNode({"tour", "Megan",
            "Regarde ce cube qui tourne, et cette\nbotte qui flotte... Magique, non ?",
            {{"Incroyable !", "end_good"},
             {"Bof, c'est un peu vide.", "end_meh"}}});
        tree.addNode({"secrets", "Megan",
            "Il parait qu'un backpack magique se\ncache quelque part... Ouvre l'oeil !",
            {{"Je vais chercher !", "end_good"},
             {"OK merci.", "bye"}}});
        tree.addNode({"end_good", "Megan",
            "Ravi de t'avoir rencontre !\nReviens me voir quand tu veux.", {}, true});
        tree.addNode({"end_meh", "Megan",
            "Eh bien... Reviens quand tu seras\nplus curieux !", {}, true});
        tree.addNode({"bye", "Megan",
            "A plus tard, l'ami !", {}, true});
        tree.setRoot("root");
        return tree;
    }

private:
    std::unordered_map<std::string, DialogNode> m_nodes;
    std::string m_rootId;
};
