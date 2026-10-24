#pragma once

#include <string>

class ModelEntity;
class Camera;
class Player;
class LightManager;

// ─────────────────────────────────────────────────────────────────────────────
// HeadCameraRig : attache la camera 1P et la lampe torche a l'os de tete du modele
// anime du joueur (avec le decalage avant du regard et une limite du lean avant de
// la tete contre les murs). L'os de tete est cherche une seule fois puis mis en cache.
// ─────────────────────────────────────────────────────────────────────────────
class HeadCameraRig {
public:
    void update(ModelEntity* human, Camera* camera, Player* player, LightManager* lightManager);

private:
    const std::string& findHeadBone(ModelEntity* human);

    ModelEntity* m_cachedFor = nullptr;   // modele pour lequel m_headBone a ete resolu
    std::string  m_headBone;
};
