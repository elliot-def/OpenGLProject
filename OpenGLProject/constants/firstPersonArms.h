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
    inline constexpr float FP_ARMS_OFFSET_Z = -0.20f;
    inline constexpr float FP_ARMS_SCALE_X = 0.65f;
    inline constexpr float FP_ARMS_SCALE_Y = 0.50f;
    inline constexpr float FP_ARMS_SCALE_Z = 0.50f;

    // Attachement des bras au corps du joueur en 3e personne (world-space) :
    // le rig est en metres (epaules a y≈1.6, root a y≈1.47). OFFSET_Y positionne
    // le rig pour aligner les epaules sur le torse du joueur (~1.4 m).
    // SCALE = 1.0 : le rig est deja a l'echelle humaine (contrairement au 1P
    // ou on reduit pour le viewmodel).
    inline constexpr float FP_ARMS_3P_OFFSET_Y = -1.0f;
    inline constexpr float FP_ARMS_3P_SCALE = 1.0f;

    // Couleur de peau pour l'éclairage des bras (ambient ≈ diffuse * 0.5)
    inline constexpr glm::vec3 FP_ARMS_SKIN_COLOR = glm::vec3(0.7f, 0.65f, 0.55f);

    // Bobbing (oscillation) des bras en première personne pendant la marche
    inline constexpr float FP_ARMS_BOB_SPEED = 5.0f;   // Fréquence d'oscillation (rad/s)
    inline constexpr float FP_ARMS_BOB_AMPLITUDE = 0.04f;  // Amplitude verticale (mètres)

    // Animation de course : oscillation plus rapide et plus ample
    inline constexpr float FP_ARMS_RUN_SPEED = 10.0f;  // Fréquence course (rad/s)
    inline constexpr float FP_ARMS_RUN_AMPLITUDE = 0.07f;  // Amplitude verticale course (m)
    inline constexpr float FP_ARMS_RUN_Z_SWING = 0.06f;  // Balancement avant/arrière (m)
    inline constexpr float FP_ARMS_RUN_PITCH = 8.0f;   // Rotation avant/arrière (degrés)

    // Offsets de pose en 1re personne (FirstPersonArms::applyViewmodelOffsets).
    // Depuis le fix "propagation" (Animator.cpp), les offsets se propagent aux
    // enfants : un offset sur upper_arm déplace coude ET main. L'idle est
    // l'animation finger_gun_idle (main DROITE déjà levée par l'anim, doigts en
    // "finger gun") ; les offsets touchent donc les DEUX bras :
    //  - bras GAUCHE : élévation (RotX +60°/+70°) + écartement (RotZ +spread) —
    //    la main gauche arrive en miroir de la droite (~(-0.16,-0.19,-0.89)
    //    espace caméra).
    //  - bras DROIT : écartement opposé (RotZ -spread) uniquement — écarte la main droite
    //    de l'axe de visée (la pose vient de l'animation). Signe OPPOSÉ au
    //    gauche : les repères locaux des deux bras sont en miroir (vérifié par
    //    la simulation spread_test.py — même signe des deux côtés rapprocherait
    //    les mains au lieu de les écarter).
    //
    // Angles calculés par FK offline (fix_search.py / spread_test.py, pose
    // finger_gun_idle) : rotation autour des axes locaux X (élévation, axe
    // haut-extérieur du bras gauche → angles POSITIFS = vers l'avant) et Z
    // (abduction). +60°/bras + +70°/avant-bras : coude à (~0.38,1.48,0.21),
    // main gauche à (~0.22,1.57,0.35) en espace rig. Réglables ici si besoin.
    inline constexpr float FP_ARMS_BONE_UPPER_ARM_DEG = 60.0f;        // élévation du bras gauche
    inline constexpr float FP_ARMS_BONE_FOREARM_DEG = 60.0f;          // pli du coude gauche
    inline constexpr float FP_ARMS_SPREAD_DEG = 18.0f;                // angle d'écartement (L: +Z, R: -Z)
    inline constexpr float FP_ARMS_SPACING = -0.20f;                   // distance supplémentaire entre les bras (unités du rig)
    }
}