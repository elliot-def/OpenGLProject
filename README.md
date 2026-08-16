# OpenGLProject

Moteur de jeu 3D "maison" écrit en C++20 / OpenGL 4.5, développé sous Visual Studio (Windows). Le projet part d'un rendu OpenGL bas niveau (via GLAD) et construit par-dessus un petit moteur complet : chargement de modèles riggés (Assimp), animation squelettique avec crossfade, collisions AABB/OBB accélérées par BVH, audio 3D (OpenAL + EFX), un système de menus navigable à la souris et à la manette, une intégration Steamworks (lobbies, invitations, Steam Input), et une couche réseau (sockets bruts) encore expérimentale.

> La quasi-totalité des commentaires du code source sont en français ; ce README suit la même convention.

---

## Table des matières

1. [Aperçu](#aperçu)
2. [Fonctionnalités](#fonctionnalités)
3. [Architecture du projet](#architecture-du-projet)
4. [Arborescence des dossiers](#arborescence-des-dossiers)
5. [Dépendances](#dépendances)
6. [Compilation](#compilation)
7. [Lancement et arguments](#lancement-et-arguments)
8. [Contrôles](#contrôles)
9. [Fichiers de configuration persistants](#fichiers-de-configuration-persistants)
10. [Système de rendu](#système-de-rendu)
11. [Animation squelettique](#animation-squelettique)
12. [Collisions et physique](#collisions-et-physique)
13. [Audio](#audio)
14. [Entrées (clavier / souris / manette)](#entrées-clavier--souris--manette)
15. [Menus et UI](#menus-et-ui)
16. [Réseau](#réseau)
17. [Intégration Steam](#intégration-steam)
18. [Écran de chargement et threading](#écran-de-chargement-et-threading)
19. [Points d'attention / pièges connus](#points-dattention--pièges-connus)
20. [Pistes d'amélioration](#pistes-damélioration)

---

## Aperçu

Le projet est une petite scène de jeu en vue première/troisième personne :

- Un joueur capsule avec gravité, saut, sprint, accroupissement et un mode **no-clip** (vol libre sans collisions).
- Un monde composé de cubes texturés (rendu instancié), de sources de lumière ponctuelles, d'une skybox nocturne et de modèles 3D (sac à dos, personnage low-poly, personnage riggé animé façon Mixamo).
- Des bras en vue première personne (viewmodel) avec animations de tir, poussée et attrape.
- Un système de menus complet (principal, pause, options, rebinding clavier/souris et manette) navigable à la souris **et** à la manette.
- Une intégration Steamworks (lobbies multijoueur, invitations, overlay) avec repli automatique en mode hors-ligne.

Le jeu tourne actuellement **exclusivement sous Windows** (Win32, Winsock, XInput, Steamworks).

---

## Fonctionnalités

### Rendu
- Chargement OpenGL via **GLAD** (spécification `gl=4.5`, profil *compatibility*, contexte créé en 3.3 core par GLFW).
- Rendu **instancié** des cubes (`CubeRenderer` / `glDrawElementsInstanced`) : un seul cube unitaire partagé, matrices modèle + couleur envoyées par instance.
- Chargement de modèles 3D via **Assimp** (`.obj`, `.fbx`, `.glb`/`.gltf`) avec extraction de bones, textures embarquées (PNG/JPEG compressées ou pixels bruts BGRA), et dédoublonnage des meshes partagés entre plusieurs nœuds.
- Skinning GPU (jusqu'à `SHADER_MAX_BONES = 100` os envoyés par `glUniformMatrix4fv` groupé).
- **Skybox** cubemap (6 faces `px/nx/py/ny/pz/nz`), rendue en profondeur `GL_LEQUAL` sans écriture du depth buffer.
- Passe d'**outline** (silhouette) réutilisable pour les shapes 2D, les meshes 3D et les modèles riggés (`Outlineable` / `OutlinePass.cpp`).
- Éclairage : jusqu'à `MAX_LIGHTS_SOURCES = 10` lumières ponctuelles, une lumière directionnelle, et une **lampe torche** (spotlight) avec scintillement lissé.
- Effet de **fondu de proximité** (near-fade) en vue première personne pour éviter l'effet de découpe du corps du joueur trop près de la caméra.
- Rendu de texte batché (un seul `glDrawArrays` par frame) via un atlas de glyphes `stb_truetype`, support UTF-8 (icônes manette/clavier de la police Kenney en zone privée Unicode).
- Écran de chargement animé (shader plein écran, barre de progression par étapes, fondu de sortie).

### Animation
- `Animator` : lecture d'animations Assimp avec interpolation position/rotation, gestion des animations en boucle ou "one-shot".
- **Crossfade** entre deux animations basé sur le blend des transformées *locales* (translation lerp + rotation slerp), pas sur les matrices finales, pour éviter les artefacts (overshoot des doigts, etc.).
- Réparation automatique des clés de rotation corrompues par Assimp 5.4.x sur certains GLB (`GlbAnimationRepair.cpp`, relecture directe du binaire glTF).
- Compatible rigs **Mixamo** : gestion des préfixes `mixamorig:`/`mixamorig2:`, des suffixes FBX `_$AssimpFbx$_Rotation` (décomposition RrTt), et des squelettes multiples partageant les mêmes noms de bones dans un seul fichier.
- Machine à états d'animation du personnage 3P (`CharacterAnimationController`) : idle/marche/course, strafes, virages, saut/course-saut, chute libre, atterrissage, avec hystérésis anti-clignotement et orientation dynamique du modèle selon la trajectoire.
- Bras 1P (`FirstPersonArms`) : détection automatique des animations (`finger_gun_idle`, `finger_gun_fire`, `push`, `grab`, `relax`), offsets de pose procéduraux par bone (élévation/écartement des bras), filtrage des bones visibles (bras/coudes/mains uniquement) en 1P et 3P.

### Collisions & physique
- `CollisionManager` : AABB pour les objets statiques, **BVH** (arbre englobant) pour les requêtes sphère/AABB en O(log n).
- OBB (boîtes orientées) pour les objets dynamiques rotatifs, avec normale de collision fidèle à l'orientation.
- Résolution de mouvement joueur par **sweep de sphère** avec glissement (sliding) itératif et sub-stepping anti-tunneling.
- Détection sol/plafond indépendante du mouvement, gestion du saut, de la chute et du ralenti post-atterrissage.
- Gravité configurable, mode no-clip (vol libre) désactivant la gravité et les collisions verticales.

### Audio
- **OpenAL** + extension **EFX** (si disponible) : sons positionnels 3D, atténuation par distance, effet Doppler.
- Presets de réverbération EAX (`ROOM`, `HALLWAY`, `CAVE`, `ARENA`, `OUTDOORS`, `UNDERWATER`, `NONE`) appliqués globalement selon l'état du jeu (ex. réverb "underwater" dans les menus).
- Filtre passe-bas par son (`AL_EXT_EFX`).
- Pause/reprise automatique de tous les sons à la perte/reprise du focus de la fenêtre.
- Chargement de fichiers **WAV PCM** (8/16 bits, mono/stéréo) fait à la main (parsing RIFF/WAVE).

### Entrées
- Abstraction `Controller` avec deux implémentations interchangeables : `GlfwController` (XInput via GLFW) et `SteamInputController` (API Steam Input, **actuellement désactivée**, voir [Points d'attention](#points-dattention--pièges-connus)).
- Bascule automatique et notification à l'écran entre clavier/souris et manette selon la dernière source d'entrée active.
- Rebinding complet des touches clavier et des boutons manette, persistant dans `res/keys.json`.
- Navigation manette dans les menus (stick gauche + croix directionnelle, répétition avec délai/fréquence configurables).

### Menus / UI
- Menu principal avec logo DVD rebondissant (easter egg) déclenché après un délai d'inactivité (AFK), sons d'ambiance aléatoires.
- Menu pause, menu options (audio), sous-menus de rebinding clavier/souris et manette.
- Widgets : boutons, sliders (`RangeInput`), cases à cocher (`CheckboxInput`), listes déroulantes (`SelectInput`).
- Curseurs personnalisés animés (`CursorManager`), y compris un curseur "attente" à 8 frames de rotation.

### Réseau (expérimental)
- Client **TCP** brut (Winsock2), connexion asynchrone, boucle réseau non-bloquante sur un thread dédié.
- Protocole binaire maison : en-tête 8 octets (magic number `0xABCD`, type, longueur) + payload JSON (`nlohmann::json`).
- Types de paquets définis (`Ping`, `Login`, `PlayerMove`, `ChatMessage`, etc.) mais l'intégration gameplay (synchronisation multijoueur) n'est pas encore branchée dans la boucle `Game::update()`.

### Steam
- Initialisation Steamworks **bloquante puis asynchrone**, avec relance automatique du client Steam si absent (`steam://open/main`) et tentatives répétées.
- Gestion de lobbies (création, join, invitation via overlay), callbacks d'entrée/sortie/changement de membres.
- Mode hors-ligne forçable en ligne de commande (voir [Lancement](#lancement-et-arguments)).

---

## Architecture du projet

```
Game                       orchestration globale (fenêtre, boucle, états, menus, son, réseau)
 ├─ Window                 fenêtre GLFW + contexte OpenGL + contexte partagé (loading thread)
 ├─ Renderer                timing des frames, delta time, cap FPS optionnel
 ├─ Scene                   monde 3D : cubes, lumières, skybox, entités riggées
 │   ├─ CubeRenderer         rendu instancié des cubes
 │   ├─ LightManager         lumières ponctuelles + directionnelle + flashlight
 │   └─ ModelEntity(s)       backpack, fropy, personnage humain riggé
 ├─ Camera                  vue 1P / 3P, collision de caméra en 3P, offsets de marche en 1P
 ├─ Player : Entity         déplacement, saut, sprint, no-clip, direction (yaw/pitch)
 ├─ CollisionManager         AABB/OBB + BVH, résolution de mouvement, saut/chute
 ├─ ModelLoader              chargement asynchrone des modèles 3D (thread + contexte GL partagé)
 ├─ FirstPersonArms          viewmodel bras 1P/3P, animations, offsets de pose
 ├─ InputManager             clavier, souris, manette (XInput/Steam Input), rebinding, notifications
 ├─ MenuManager               machine à états des menus (Menu/Pause/Options/...)
 ├─ SoundManager              OpenAL, EFX, réverbération, volume, mute
 ├─ ShaderManager / TextureManager   arborescence de shaders/textures chargée depuis res/
 ├─ Socket                   client réseau TCP (Winsock)
 └─ SteamManager              Steamworks : init, lobbies, invitations, overlay
```

### Boucle de jeu (`Game::run`)

1. **Phase `LOADING`** : écran de chargement animé pendant que `ModelLoader` charge les modèles 3D sur un thread séparé (contexte GL partagé). Une fois terminé : recréation des VAO dans le contexte principal (non partageables entre contextes), destruction du contexte partagé, fondu de sortie.
2. **Phase `READY`**, selon l'état courant (`GameState`) :
   - `STATE_MENU` / `STATE_OPTIONS` / `STATE_PAUSED` : mise à jour des entrées + du menu courant, rendu 2D.
   - `STATE_PLAYING` : `Game::update()` (scène 3D, caméra, son, réseau) puis `Game::draw()` (skybox, opaques, transparences, HUD debug, notifications).

---

## Arborescence des dossiers

```
OpenGLProject/
├── main.cpp, Game.{h,cpp}, Scene.{h,cpp}, Window.{h,cpp}, Renderer.{h,cpp}
├── Camera.{h,cpp}, ModelLoader.{h,cpp}, FirstPersonArms.{h,cpp}
│
├── Entity.{h,cpp}, Player.{h,cpp}, Direction.{h,cpp}, Transformation.h
│
├── InputManager.{h,cpp}, InputNotification.{h,cpp}
├── Controller.{h,cpp}, GlfwController.{h,cpp}, SteamController.{h,cpp}
├── Key.h, ControllerKey.{h,cpp}, PlayerKey.{h,cpp}, configKeys.h
├── Mouse.{h,cpp}, LeftClick.{h,cpp}, Escape.{h,cpp}, Push.{h,cpp}, Grab.{h,cpp}
│
├── Shader.{h,cpp}, ShaderManager.h, ShaderType.h, Vertex.h
├── Mesh.{h,cpp}, Model.{h,cpp}, Texture.{h,cpp}, TextureManager.{h,cpp}, ImageLoader.{h,cpp}
├── Animator.{h,cpp}, SkinningData.h, GlbAnimationRepair.{h,cpp}
├── LoadingScreen.{h,cpp}
│
├── Shape.{h,cpp}, Rectangle.{h,cpp}, Triangle.{h,cpp}, Image.{h,cpp}, MaskImage.{h,cpp}
├── DVDShape.{h,cpp}, SharedQuad.h
├── Cube.{h,cpp}, CubeRenderer.{h,cpp}
│
├── Menu.{h,cpp}, MenuManager.{h,cpp}
├── MainMenu.{h,cpp}, PauseMenu.{h,cpp}, OptionsMenu.{h,cpp}
├── KeyBindingsMenu.{h,cpp}, ControllerBindingsMenu.{h,cpp}
├── TextRenderer.{h,cpp}, CursorManager.{h,cpp}
├── RangeInput.{h,cpp}, CheckboxInput.{h,cpp}, SelectInput.{h,cpp}
│
├── LightSource.{h,cpp}, LightManager.{h,cpp}, Spotlight.{h,cpp}
├── Sound.{h,cpp}, SoundManager.{h,cpp}
├── CollisionManager.{h,cpp}, AABB.h, BVHNode.h, Outlineable.h, OutlinePass.cpp
│
├── Socket.{h,cpp}, Packet.{h,cpp}, PacketBuilder.h, ReceiveBuffer.{h,cpp}
├── SteamManager.{h,cpp}
│
├── constants/               window, player, camera, physics, renderer, shader, texture,
│                             material, color, network, menu, file, firstPersonArms
│
├── glad.c, win_compat.h, gamestate.h, Log.h, File.h, config.h
│
├── res/
│   ├── shaders/              cube/, model/, skinned/, shape/, image/, skybox/, outline/, text/
│   ├── textures/              crate/, glass/, menu/ (curseurs, dvd_logo.png, cursor assets)
│   ├── models/                 backpack/, fropy/
│   ├── rigging/                 arm/ (bras 1P riggés), mixamo/models + mixamo/animation (clips)
│   ├── skybox/night/            6 faces du cubemap nocturne
│   ├── sounds/                   menu/, on&on.wav, ...
│   ├── fonts/                    armana/, Gnocchi.ttf, kenney/ (icônes manette + clavier/souris)
│   ├── keys.json                 bindings clavier/manette + sensibilités (généré/persisté)
│   ├── options.json               mute + volume (généré/persisté)
│   └── steam_input_manifest.vdf   manifest d'actions Steam Input
│
├── OpenGLProject.vcxproj
└── OpenGLProject.vcxproj.filters
```

---

## Dépendances

| Bibliothèque | Rôle | Notes |
|---|---|---|
| **GLFW3** | Fenêtrage, contexte OpenGL, entrées clavier/souris/manette | Contexte OpenGL 3.3 core, vsync désactivée par défaut |
| **GLAD** | Chargement des fonctions OpenGL | Généré pour `gl=4.5`, profil *compatibility* |
| **GLM** | Mathématiques (vecteurs, matrices, quaternions) | `GLM_ENABLE_EXPERIMENTAL` requis pour `glm/gtx/norm.hpp` |
| **Assimp** | Import de modèles 3D et d'animations | `.obj`, `.fbx`, `.glb`/`.gltf` ; lié via `assimp-vc143-mtd.lib` |
| **stb_image** | Décodage d'images (PNG/JPEG) | Implémentation compilée une seule fois dans `Texture.cpp` |
| **stb_truetype** | Rendu de police (atlas de glyphes) | Implémentation compilée dans `TextRenderer.cpp` |
| **OpenAL (+ EFX)** | Audio 3D, réverbération, filtres | Lié via `OpenAL32.lib` |
| **nlohmann/json** | Sérialisation JSON | Options, bindings, propriétés de matériaux, paquets réseau |
| **Steamworks SDK** | Lobbies, overlay, Steam Input | Lié via `steam_api64.lib` ; AppID de test `480` (Spacewar) |
| **Winsock2** | Sockets réseau TCP | Windows uniquement |

Toutes les DLL nécessaires (`dependencies/bin/*.dll`) sont copiées automatiquement dans le dossier de sortie après build (`xcopy` en post-build event).

---

## Compilation

Le projet est un projet **Visual Studio** (`.vcxproj`), testé avec le toolset **v143** (Visual Studio 2022).

### Prérequis
- Windows 10/11
- Visual Studio 2022 avec le workload "Développement Desktop en C++"
- Steamworks SDK placé dans `dependencies/` (headers dans `dependencies/steam/`, libs dans `dependencies/lib/`, DLL dans `dependencies/bin/`)
- Un fichier `steam_appid.txt` à côté de l'exécutable si Steam n'est pas déjà informé de l'AppID

### Étapes

1. Ouvrir `OpenGLProject.sln` (ou générer/ouvrir le `.vcxproj` directement) dans Visual Studio.
2. Choisir la configuration `Debug` ou `Release`, la plateforme `x64` (recommandé) ou `Win32`.
3. Compiler (`Ctrl+Shift+B`). Le standard **C++20** est requis (`LanguageStandard=stdcpp20`, x64 uniquement — la configuration Win32 n'active pas ce standard explicitement).
4. L'exécutable et les DLL dépendantes sont copiés dans `$(OutDir)`.

### Points de compilation à connaître
- `_CRT_SECURE_NO_WARNINGS` est défini globalement (usage de fonctions C non "sécurisées" comme `sscanf`).
- `/utf-8` est passé au compilateur pour les sources contenant des caractères accentués.
- `LIBCMT`/`LIBCPMT` sont explicitement ignorées à l'édition de liens (`IgnoreSpecificDefaultLibraries`) — probablement pour éviter un conflit de runtime avec les libs tierces (Steamworks/Assimp).
- **`win_compat.h` doit être inclus en premier** dans toute unité de compilation touchant au rendu qui inclut aussi `<windows.h>` indirectement, sinon collision `APIENTRY` entre `glad.h` et `minwindef.h` (voir le commentaire en tête du fichier).

---

## Lancement et arguments

```
OpenGLProject.exe [-offline | --offline | -nosteam | --nosteam]
```

- Sans argument : le jeu tente d'initialiser Steamworks (bloquant, avec relance automatique du client Steam si nécessaire, jusqu'à ~20 tentatives).
- Avec `-offline`/`-nosteam` : Steam est **entièrement ignoré** (ni initialisé, ni lancé) ; la manette passe par XInput/GLFW sans interférence de Steam Input.
- `+connect_lobby <SteamID>` : rejoint automatiquement un lobby Steam au démarrage (utilisé pour les invitations reçues jeu fermé).

---

## Contrôles

### Clavier & souris (par défaut, rebindables dans Options → Clavier & Souris)

| Action | Touche |
|---|---|
| Avancer / Reculer / Gauche / Droite | `Z`/`W` `S` `Q`/`A` `D` *(AZERTY/QWERTY selon `GLFW_KEY_*`)* |
| Sauter (jeu) / Voler vers le haut (no-clip) | `Espace` |
| S'accroupir (jeu) / Voler vers le bas (no-clip) | `Ctrl gauche` |
| Sprinter (maintenu) | `Maj gauche` |
| Lampe torche | `T` |
| Vue 1ère / 3ème personne | `C` |
| Pousser (bras 1P) | `R` |
| Attraper (bras 1P) | `E` |
| No-clip (vol libre, désactive la gravité) | `N` |
| Menu / Pause | `Échap` |
| HUD debug animations (3P uniquement) | `F3` |
| Tirer (animation bras) | Clic gauche |

### Manette (mapping Xbox, rebindable dans Options → Manette)

| Action | Bouton |
|---|---|
| Déplacement | Stick gauche |
| Regard | Stick droit |
| Sprint | Gâchette droite (RT), seuil configurable |
| Sauter | `A` |
| S'accroupir | `B` (maintenu) |
| Pousser | `X` |
| Vue 1P/3P | `Y` |
| Lampe torche | `LB` |
| Attraper | `RB` |
| No-clip | `Back` |
| Menu / Reprendre | `Start` |
| Retour (dans les menus) | `B` |
| Navigation menus | Stick gauche ou croix directionnelle + `A` pour valider |

> Le clavier/souris et la manette restent **tous deux actifs en permanence** ; seule la "source active" affichée par la notification à l'écran bascule selon la dernière entrée détectée.

---

## Fichiers de configuration persistants

### `res/keys.json`
```json
{
  "bindings": { "Forward": 87, "Jump": 32, "...": "..." },
  "controller_bindings": { "Jump": 0, "...": "..." },
  "mouse_sensitivity": 0.05,
  "controller_sensitivity": 3.0
}
```
Généré avec les valeurs par défaut de `configKeys.h` si absent ; réécrit à chaque rebinding ou changement de sensibilité.

### `res/options.json`
```json
{ "muted": false, "volume": 1.0 }
```
Généré avec les valeurs par défaut (volume maître par défaut effectif au démarrage : `0.2`, voir `constants/window.h`) ; réécrit à la sortie du menu Options.

---

## Système de rendu

- **Cubes** : un seul mesh unitaire (arête 1, centré à l'origine) partagé par toutes les instances ; position/rotation/échelle portées par une matrice modèle par instance, couleur par instance pour les cubes-lumière.
- **Modèles riggés** : squelette extrait via `Model::extractBoneDataFromMesh`, jusqu'à 4 influences de bones par sommet (`MAX_BONE_INFLUENCE = 4`), matrices finales envoyées groupées au shader `skinned`.
- **Layout de vertex compact** : `Mesh::setupMesh` n'envoie au GPU que les attributs réellement utilisés par le masque (`VertexAttribute::POSITION|NORMAL|COLOR|TEXCOORD|SKINNING`), pas systématiquement les 76 octets complets de la classe `Vertex`.
- **Shaders** organisés en arborescence sous `res/shaders/`, résolus par chemin relatif via `ShaderManager::getShader("cube/severallights")`, etc. Chaque shader connaît son "type" (`ShaderType`) résolu une seule fois à la création pour éviter les comparaisons de chaînes à chaque frame de rendu.
- **Uniform locations mises en cache** systématiquement (patron répété dans `LightManager`, `Spotlight`, `Shape`, `Shader::getUniformLocation`) pour éviter les hachages de chaînes dans le chemin chaud du rendu.

---

## Animation squelettique

- Format d'os : `BoneInfo { int id; glm::mat4 offsetMatrix; }`, stocké dans une map nom → info par modèle.
- Deux sources d'animations possibles par modèle : celles embarquées dans le fichier, et des animations **externes** chargées séparément (`Model::loadExternalAnimations`, utilisé pour les clips Mixamo du personnage 3P : idle, walking, running, jump, strafes, turns, falling, etc.).
- `Animator::computeBoneTransform` calcule récursivement les transformées globales, applique le crossfade sur les transformées locales, et propage les offsets manuels (`addBoneOffset`) aux enfants — essentiel pour que le pli du coude suive l'élévation de l'épaule sur les bras 1P.
- Garde-fous anti-NaN à plusieurs niveaux (quaternions dégénérés, temps non finis, longueurs de vecteur nulles) pour éviter qu'une keyframe corrompue ne rende un personnage invisible (matrices non finies envoyées au GPU).

---

## Collisions et physique

- `Constants::Physics::GRAVITY = -9.81f`, `PLAYER_JUMP_VELOCITY = 6.0f` (~2.45 s en l'air).
- Le joueur est modélisé comme une **capsule** (2 sphères, pieds + tête) pour gérer marches et pentes.
- `sweepSphere` : déplacement en sous-étapes (`radius * 0.4` par sous-étape max) avec glissement le long des normales de collision et sécurité anti-rebond infini dans les coins.
- Distinction statique/dynamique : les objets statiques passent par le BVH (construit une fois au chargement du niveau), les objets dynamiques (cube tournant, joueur poussé) sont testés en O(n) linéaire (nombre d'objets mobiles généralement faible).
- Ralenti de vitesse configurable après un saut ou une chute (`POST_LAND_SPEED_FACTOR`, `FALL_SLOWDOWN_FACTOR`), avec seuil de hauteur de chute minimum pour déclencher l'animation d'atterrissage.

---

## Audio

- Modèle de distance OpenAL : `AL_INVERSE_DISTANCE_CLAMPED` par défaut.
- `SoundManager::load` met en cache par nom : un même nom n'est chargé (et décodé) qu'une seule fois par session — **attention à charger les sons lourds pendant l'écran de chargement plutôt qu'à la volée** (voir [Points d'attention](#points-dattention--pièges-connus)).
- Réverbération appliquée globalement (`applyReverbToAll`) : `UNDERWATER` dans les menus/pause, `NONE` en jeu.

---

## Entrées (clavier / souris / manette)

- `Key` est la classe de base commune (clavier **et** manette y compris) : actions `onPress`/`onRelease`/`ifPressed` enregistrées par `InputContext` (`GAME`, `MENU`, `INVENTORY`, `PAUSED`).
- `PlayerKey` et `ControllerKey` sont des spécialisations génériques prenant des lambdas au constructeur, remplaçant d'anciennes classes dédiées par touche (`Forward`, `Jump`, etc.).
- Détection de branchement/débranchement à chaud de la manette avec **délai de grâce** (~30 frames) pour filtrer le clignotement de ré-énumération USB.
- `SteamController` traduit les actions du manifest `res/steam_input_manifest.vdf` vers les mêmes index `GLFW_GAMEPAD_BUTTON_*`/`GLFW_GAMEPAD_AXIS_*` que le mode XInput, pour que le reste du code (`ControllerKey`, `InputManager`) soit agnostique de la source réelle.

---

## Menus et UI

- `Menu` gère à la fois le rendu (texte centré/aligné, formes, sliders, checkboxes, listes) et la navigation manette (liste de cibles focalisables triées verticalement, reconstruite à la demande via un flag `m_focusDirty`).
- `MenuManager` fait la liaison entre `GameState` et l'instance de `Menu` à afficher, sauvegarde/restaure la position de sélection manette par menu (utile en particulier pour revenir dans un sous-menu de bindings sans repartir du premier élément).
- Le rendu de texte est **batché par frame** : `TextRenderer::beginFrame()` / `renderText()` (accumulation, aucun appel GL) / `flush()` (un seul `glDrawArrays` pour tout le texte de la frame).

---

## Réseau

État actuel : **fonctionnel au niveau transport, non branché au gameplay**. Le client se connecte de façon asynchrone au démarrage (`Constants::Network::SERVER_IP`/`SERVER_PORT`, `127.0.0.1:3333` par défaut), et les événements reçus sont simplement loggés dans `Game::update()`. La structure de paquets (`PacketBuilder`) couvre déjà l'authentification, le chat et le mouvement joueur, prête à être exploitée pour une synchronisation multijoueur.

---

## Intégration Steam

- AppID de test : `480` (Spacewar) — **à remplacer par un véritable AppID Steamworks avant toute distribution**.
- `SteamManager::initAsyncStart()`/`initAsyncPoll()` permettent une init non-bloquante (utile pour ne pas geler la fenêtre pendant que Steam se lance) ; `init()` reste disponible en variante bloquante.
- Un lobby créé déclenche automatiquement l'ouverture de la boîte de dialogue d'invitation Steam (overlay).

---

## Écran de chargement et threading

- `ModelLoader::load()` s'exécute sur un thread séparé avec un **contexte OpenGL partagé** (`Window::createSharedContext`), permettant de charger les modèles 3D (upload VBO/EBO/textures) pendant que l'écran de chargement continue de s'afficher sur le contexte principal.
- Les **VAO ne sont pas partagés entre contextes** OpenGL : chaque `Mesh` doit donc appeler `reloadGPUResources()` dans le contexte principal une fois le thread de chargement terminé (voir `Game::run`, phase `LOADING`).
- Repli synchrone automatique si la création du contexte partagé échoue.

---

## Points d'attention / pièges connus

- **Steam Input est désactivé** (`kUseSteamInput = false` dans `InputManager.cpp`) : avec l'AppID placeholder 480, la configuration Steam Input du jeu n'est jamais chargée par le client Steam, ce qui laissait la manette dans un état inutilisable. Le jeu utilise donc XInput (GLFW) même quand Steam est initialisé. Réactiver ce flag nécessite un véritable AppID Steamworks avec une configuration Steam Input associée.
- **`win_compat.h` doit être le tout premier `#include`** dans les fichiers touchant au rendu qui incluent indirectement `<windows.h>`, sous peine de warning/erreur `C4005` (redéfinition de `APIENTRY` entre `glad.h` et `minwindef.h`).
- **Premier lancement en jeu plus lent** : le premier passage en `STATE_PLAYING` charge et décode `res/sounds/on&on.wav` de façon synchrone (`SoundManager::load` avec cache par nom) et construit le sous-ensemble de triangles "bras/jambes" pour la vue 1P (`Model::buildCulledIndices`), ce qui peut provoquer un court freeze visible une seule fois par session.
- Le layout de `Vertex` est verrouillé par `static_assert` (76 octets = 11 floats + 4 ints + 4 floats) : toute modification de la classe doit être répercutée avec soin pour ne pas désynchroniser CPU/GPU.
- `Vertex::m_boneIDs`/`m_weights` sont publics par exception (contrainte de layout), documenté dans `Vertex.h`.
- Le dossier `dependencies/` (Steamworks SDK, DLL tierces) n'est **pas inclus** dans ce dépôt et doit être fourni séparément pour compiler.

---

## Pistes d'amélioration

- Brancher la couche réseau existante à une vraie synchronisation multijoueur (positions, animations, chat).
- Remplacer l'AppID Steamworks placeholder et réactiver Steam Input avec un manifest d'actions correctement enregistré.
- Précharger ou streamer les sons volumineux (musique de fond) pendant l'écran de chargement plutôt qu'au premier accès.
- Ajouter un vrai fichier `.sln`/scripts de build multi-plateforme si un portage Linux/macOS est envisagé (actuellement fortement couplé à Win32/XInput/Winsock).
- Compresser/streamer les fichiers audio plutôt que de les charger intégralement en RAM au format WAV non compressé.

---

## Licence

*(à compléter selon les intentions du projet — aucune licence n'est actuellement spécifiée dans le dépôt)*
