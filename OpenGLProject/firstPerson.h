#pragma once


namespace Constants {
    namespace FirstPersonArms {
        inline constexpr float FP_ARMS_OFFSET_X = 0.00f;
        inline constexpr float FP_ARMS_OFFSET_Y = -0.70f;
        inline constexpr float FP_ARMS_OFFSET_Z = -0.55f;
        inline constexpr float FP_ARMS_SCALE_X = 0.65f;
        inline constexpr float FP_ARMS_SCALE_Y = 0.50f;
        inline constexpr float FP_ARMS_SCALE_Z = 0.50f;

        inline constexpr float FP_ARMS_3P_OFFSET_Y = -0.2f;
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
