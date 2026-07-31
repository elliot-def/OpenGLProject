#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// ShaderType : identifiant typé du rôle d'un shader
// ─────────────────────────────────────────────────────────────────────────────
//
// Remplace les comparaisons de chaînes `m_shader->getName() == "..."` qui
// étaient effectuées dans les draw() à chaque frame : une comparaison
// d'enum (1 cycle CPU) remplace un std::string::operator== (parcours mémoire).
//
// L'enum est peuplé une seule fois dans le constructeur de Shader à partir
// de m_name (résolu par extractShaderType ci-dessous).
//
// Ajouter une variante ici dès qu'un nouveau nom de shader a besoin d'un
// branchement runtime dans le C++.

enum class ShaderType : uint8_t {
    Unknown = 0,           // Nom non mappé (default branch dans les draws)
    BasicTriangle,         // shape/triangle — shader triangle classique
    RoundedTriangle,       // shape/roundedTriangle — nécessite uniforms radius + resolution
    Image,                 // shape/image — texture simple
    Mask,                  // mask — utilise uniform color
    LightSource,           // lightsource / cube/lightsource
    SeveralLights,         // severallights / cube/severallights
    Outline,               // outline — silhouette pleine couleur (pas de lumiere, pas de texture)
    SkinnedModel,          // skinned — vertex shader avec bone palette
};

// Convertit le nom de fichier (ex: "shape/roundedTriangle") en ShaderType.
// Centralisé pour qu'on n'ait jamais à toucher les sites d'appel.
inline ShaderType extractShaderType(const std::string& name) {
    if (name == "shape/roundedTriangle")      return ShaderType::RoundedTriangle;
    if (name == "shape/triangle")             return ShaderType::BasicTriangle;
    if (name == "shape/image")                return ShaderType::Image;
    if (name == "mask")                       return ShaderType::Mask;
    if (name == "lightsource" ||
        name == "cube/lightsource")           return ShaderType::LightSource;
    if (name == "severallights" ||
        name == "cube/severallights")         return ShaderType::SeveralLights;
    if (name == "outline")                    return ShaderType::Outline;
    if (name == "skinned" || name == "skinnedmodel") return ShaderType::SkinnedModel;
    return ShaderType::Unknown;
}
