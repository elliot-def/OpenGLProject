#pragma once

#include <glm/vec3.hpp>

namespace Constants {
    namespace FirstPersonArms {
    // First-person arms
    // Rotation Y 180 deg dans FirstPersonArms::draw() :
    //  - +X rig (main GAUCHE du personnage) -> ecran GAUCHE (-X) : cotes
    //    gauche/droite corrects (pas d'effet miroir).
    //  - +Z rig (avant/paumes) pointe dans l'ecran : on voit le dos des mains,
    //    comme en vraie premiere personne.
    // PAS de rotation autour de X : le rig reste a l'endroit (Y vers le haut),
    // donc en pose "rest" (= bind pose, bras le long du corps, mains a x=+-0.75)
    // les bras pendent des epaules vers les COINS INFERIEURS de l'ecran.
    //
    // Echelle non-uniforme (X=0.65, Y=Z=0.50) : reduit le rig a taille viewmodel.
    // OFFSET_Y=-1.00 : epaules (rig y≈1.53*0.5=0.77) a y≈-0.23, mains a y≈-0.19
    // -> bras dans la moitie basse de l'ecran. OFFSET_Z=+0.30 : le viewmodel
    // est avance vers la camera, donc nettement plus proche et plus visible.
    // Reglable ici selon le cadrage souhaite.
    inline constexpr float FP_ARMS_OFFSET_X = -0.02f;
    inline constexpr float FP_ARMS_OFFSET_Y = -0.95f;
    inline constexpr float FP_ARMS_OFFSET_Z = -0.15f;

    inline constexpr float FP_ARMS_SCALE_X = 0.65f;
    inline constexpr float FP_ARMS_SCALE_Y = 0.50f;
    inline constexpr float FP_ARMS_SCALE_Z = 0.50f;

    // Attachement des bras au corps du joueur en 3e personne (world-space) :
    // Le rig fait ~1.8m de haut (epaules y≈1.6, root y≈1.47). OFFSET_Y aligne
    // les epaules du rig (~1.6m) avec celles du joueur (~1.4m) :
    //   rig_shoulder_world = playerPos.y + OFFSET_Y + rig_shoulder * SCALE
    //   = 0 + (-0.15) + 1.6*0.95 ≈ 1.37m  ✓
    // SCALE legerement < 1.0 pour que les bras ne dominent pas visuellement
    // le modele 3P (Megan, ~1.8m auto-scale). Reglable selon le modele.
    inline constexpr float FP_ARMS_3P_OFFSET_Y = -0.15f;
    inline constexpr float FP_ARMS_3P_SCALE = 0.95f;

    // Couleur de peau pour l'eclairage des bras (ambient ≈ diffuse * 0.5)
    inline constexpr glm::vec3 FP_ARMS_SKIN_COLOR = glm::vec3(0.7f, 0.65f, 0.55f);

    // Bobbing (oscillation) des bras en premiere personne pendant la marche
    inline constexpr float FP_ARMS_BOB_SPEED = 5.0f;   // Frequence d'oscillation (rad/s)
    inline constexpr float FP_ARMS_BOB_AMPLITUDE = 0.04f;  // Amplitude verticale (metres)

    // Animation de course : oscillation plus rapide et plus ample
    inline constexpr float FP_ARMS_RUN_SPEED = 10.0f;  // Frequence course (rad/s)
    inline constexpr float FP_ARMS_RUN_AMPLITUDE = 0.07f;  // Amplitude verticale course (m)
    inline constexpr float FP_ARMS_RUN_Z_SWING = 0.06f;  // Balancement avant/arriere (m)
    inline constexpr float FP_ARMS_RUN_PITCH = 8.0f;   // Rotation avant/arriere (degres)

    // Offsets de pose en 1re personne (FirstPersonArms::applyViewmodelOffsets).
    // Depuis le fix "propagation" (Animator.cpp), les offsets se propagent aux
    // enfants : un offset sur upper_arm deplace coude ET main. L'idle est
    // l'animation finger_gun_idle (main DROITE deja levee par l'anim, doigts en
    // "finger gun") ; les offsets touchent donc les DEUX bras :
    //  - bras GAUCHE : elevation (RotX +60°/+70°) + ecartement (RotZ +spread) —
    //    la main gauche arrive en miroir de la droite (~(-0.16,-0.19,-0.89)
    //    espace camera).
    //  - bras DROIT : ecartement oppose (RotZ -spread) uniquement — ecarte la main droite
    //    de l'axe de visee (la pose vient de l'animation). Signe OPPOSE au
    //    gauche : les reperes locaux des deux bras sont en miroir (verifie par
    //    la simulation spread_test.py — meme signe des deux cotes rapprocherait
    //    les mains au lieu de les ecarter).
    //
    // Angles calcules par FK offline (fix_search.py / spread_test.py, pose
    // finger_gun_idle) : rotation autour des axes locaux X (elevation, axe
    // haut-exterieur du bras gauche → angles POSITIFS = vers l'avant) et Z
    // (abduction). +60°/bras + +70°/avant-bras : coude a (~0.38,1.48,0.21),
    // main gauche a (~0.22,1.57,0.35) en espace rig. Reglables ici si besoin.
    inline constexpr float FP_ARMS_BONE_UPPER_ARM_DEG = 0.0f;       // elevation du bras gauche
    inline constexpr float FP_ARMS_BONE_FOREARM_DEG = 0.0f;         // pli du coude gauche
    inline constexpr float FP_ARMS_SPREAD_DEG = 18.0f;              // angle d'ecartement (L: +Z, R: -Z)
    inline constexpr float FP_ARMS_SPACING = -0.15f;                // distance supplementaire entre les bras (unites du rig)
    }
}
