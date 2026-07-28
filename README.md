# Projet OpenGL en C++

Ce dépot contient un projet de rendu graphique via OpenGL.<br>
La progression est basé sur https://learnopengl.com/.


##  Structure du dépot

- `dependencies/` : Toutes les librairies;
- `res/` : Ressources (Shaders, textures, map, sons etc...);
  + `shaders/` : Les différents shaders (vertex et fragment);
    * `cube/`     : Éclairage (severallights, lightsource).
    * `model/`    : Modèles statiques de décor (backpack).
    * `outline/`  : Post-process silhouette.
    * `skinned/`  : Pipeline GPU-skinning (avant-bras, personnages).
    * `text/`     : Rendu de texte bitmap.
    * `shape/`    : 2D shapes (Triangle, Rectangle, Image).
  + `textures/`   : Les différentes textures à afficher (.png);
  + `models/`     : Modèles statiques de décor (backpack, etc.);
  + `rigging/`    : **Modèles skinned** (avant-bras, personnages riggués) avec squelette + clips d'animation Assimp;
- `fichiers .h & .cpp` : Les fichiers headers et sources. 

##  Sous-systèmes notables

- **Pipeline skinned** (`SkinningData.h` + `AnimationClip.h` + `Animator.h/.cpp` + `Model.cpp` + `res/shaders/skinned/skinned.{vert,frag}`):
  extraction bones/animations via Assimp -> stockage dans `Vertex.m_boneIDs[4]` + `Vertex.m_weights[4]` (auto-padding si > 4 influences) -> upload de `uBoneMatrices[100]` au GPU.
  Limite `MAX_BONE_INFLUENCE=4` ancrée par `static_assert` dans `Vertex.h` (toute réordonnancement de champs casse la compilation au lieu de dériver en bug GPU silencieux).
  Asset path: `res/rigging/<nom>/<nom>.glb` (fallback `.fbx`). Chargé dans `Game::initialize()` puis joué par `Animator::update(dt)`.

- **Avant-bras first-person** (`ArmsRenderer.h/.cpp`):
  helper isolé qui rend les avant-bras riggués en vue subjective. Encapsule toute la math:
  ` skinned shader use + camera-relative model matrix + bone palette upload via setMat4Array + world lights + clear depth`
  en une seule fonction statique `ArmsRenderer::drawFP(camera, shaderManager, lightManager, model, animator)`.
  Game loop l'appelle en **dernier** (après le décor opaque + transparents) avec `glClear(GL_DEPTH_BUFFER_BIT)` pour que les mains soient toujours visibles.
  Position/rotation/scale des bras par rapport à la caméra: `Constants::FP_ARMS_DOWN/FORWARD/YAW_OFFSET/SCALE` (voir `constants.h`).

- **Collision sphere-vs-AABB** (`CollisionManager.h/.cpp` + `BVHNode.h` + `AABB.h`): sweeping itératif + sliding. BVH statique construit via `buildBVH()` au boot pour O(log n) lookup.

##  Prérequis

Avant de faire tourner ce code sur votre machine, il vous faut (normalement tout est dans les dépendencies):

- C++23;
- GLFW & GLAD pour OpenGL;
- STB pour le chargement des images et les polices d'écritures;
- GLM pour les mathématiques;
- Assimp pour les modèles 3D.

##  Installation

1. Installer Visual Studio 2026+
2. Lancer le projet "OpenGLProject.sln"
3. Compiler (Debug ou Release)
4. (Optionnel) Déposer un asset skinned dans `OpenGLProject/res/rigging/<nom>/<nom>.glb`
   (ou `.fbx` en fallback). Il sera auto-chargé par `Game::initialize()` et animé
   par l'`Animator` via le shader `skinned`.
