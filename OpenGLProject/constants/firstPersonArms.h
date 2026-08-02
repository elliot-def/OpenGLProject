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
    // Echelle non-uniforme (X=0.65, Y=Z=0.50) place avec pose "rest" :
    //  - mains  : +-0.75*0.65 = +-0.49 -> COINS inferieurs (demi-largeur ~0.55)
    //  - coudes : +-0.56*0.65 = +-0.36 -> s'ecartent depuis les epaules
    //  - epaules: +-0.13*0.65 = +-0.08 -> bas-centre de l'ecran
    // OFFSET_Y=-0.70 : epaules (rig y=1.53*0.5=0.77) a y~+0.07, mains (rig
    // y=1.12*0.5=0.56) a y~-0.14 -> bras dans la moitie basse de l'ecran.
    inline constexpr float FP_ARMS_OFFSET_X = 0.00f;
    inline constexpr float FP_ARMS_OFFSET_Y = -1.00f;
    inline constexpr float FP_ARMS_OFFSET_Z = -0.55f;
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
    }
}