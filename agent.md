# AGENT.md — OpenGLProject

> Guide de bord pour toute IA / tout développeur arrivant sur ce projet.
> Si quelque chose est faux ici, c'est que le code a dérivé : ouvrir une PR pour corriger ce fichier.

---

## 1. Vue d'ensemble

Projet **C++20** de rendu graphique avec OpenGL, construit progressivement à partir de **LearnOpenGL.com**.

| Aspect | Détail |
|---|---|
| Langage | C++20 (`<LanguageStandard>stdcpp20</LanguageStandard>` dans le `.vcxproj`) |
| Build | **Visual Studio 2026**, solution `OpenGLProject.sln`, projet `OpenGLProject.vcxproj` |
| Plateforme cible | Windows x64 (Win32 aussi configuré) |
| Graphique | **GLFW 3** + **GLAD** (loader OpenGL 3.3 core profile) |
| Math | **GLM** (incluant `glm::highp_vec3` / `radians`) |
| Assets | **STB** (images, polices) + **Assimp** (modèles `.obj`) + **OpenAL** (audio 3D + EFX) |
| Config JSON | **nlohmann/json** (utilisé par `TextureManager`) |
| Réseau (partiel) | Sockets TCP custom (header `Socket.h`, `Packet.h`, `ReceiveBuffer.h`) |
| Style | Identifiants **anglais**, **commentaires français** (héritage du cours suivi). |
| Documentation utilisateur | `README.md` (succinct) — ce fichier complète. |

### Arborescence

```
OpenGLProject/
├── OpenGLProject.sln              ← ouvrir dans VS
├── OpenGLProject.vcxproj          ← projet principal (header ClCompile/ClInclude très explicites)
├── OpenGLProject.vcxproj.filters  ← organisation Solution Explorer (dossiers virtuels FR)
├── OpenGLProject.exe.lnk          ← raccourci vers l'exécutable
├── main.cpp                       ← point d'entrée (crée Game, run, delete)
├── Game.cpp / .h                  ← orchestrateur principal (singletons via unique_ptr)
├── res/
│   ├── shaders/                   ← *.vert + *.frag (1 shader = 1 sous-dossier portant son nom)
│   ├── textures/                  ← images + détails.json pour shininess/specular
│   ├── models/                    ← .obj (Assimp)
│   ├── sounds/                    ← .wav (OpenAL)
│   ├── fonts/                     ← .ttf (STB)
│   └── certificates/, credits.txt, options.json
├── dependencies/
│   ├── glm/, glad/, stb/, ...     ← vendored libs (header-only)
│   ├── lib/, bin/                 ← binaires tierce-partie (DLLs copiées via postbuild)
│   ├── openssl/, steam/           ← API headers
├── agent.md                       ← CE FICHIER
└── OpenGLProject/…                ← tout le code C++ (plats, sans sous-dossiers)
```

Toutes les sources `.cpp`/`.h` vivent à plat dans `OpenGLProject/` ; le regroupement visuel
des filtres de solution (Fichiers sources/Inputs, …) est fait via `.vcxproj.filters`.

---

## 2. Architecture & flux d'exécution

```
main()
 └─> Game (créé + run() + delete)
      ├─ Window                  (GLFW, icône, curseur custom)
      ├─ Renderer                (deltaTime, FPS capping, glClear)
      ├─ Camera                  (matrices view/proj à partir du Player)
      ├─ Player                  (Entity : position, direction, gravité, saut)
      ├─ CollisionManager        (AABB + sphère sweep + BVH statique)
      ├─ LightManager            (lumières ponctuelles + DirLight + Spotlight)
      ├─ ShaderManager           (auto-load arborescence res/shaders/)
      ├─ TextureManager          (auto-load arborescence res/textures/ + json)
      ├─ SoundManager            (OpenAL + EFX reverb + listener = caméra)
      ├─ CursorManager           (curseurs custom)
      ├─ InputManager            (touches + souris, contexte-dépendant)
      ├─ MenuManager             (états : STATE_MENU / STATE_PLAYING / STATE_OPTIONS / STATE_PAUSED)
      └─ m_textRenderers         (vector<TextRenderer> pour polices STB)
```

### Boucle principale (`Game::run()`)

```cpp
while (!m_window->getShouldClose()) {
    m_renderer->handleFrameTiming();          // calcule deltaTime + cap FPS
    switch (m_menuManager->getCurrentState()) {
        case STATE_MENU/OPTIONS/PAUSED:
            m_inputManager->update();         // clavier + souris
            m_menuManager->update();
            m_renderer->clear();
            m_menuManager->draw();
            break;
        case STATE_PLAYING:
            update();                         // logique + déplacements + collisions
            m_renderer->clear();
            draw();                           // opaques (depth ON) -> transparents (depth OFF + blend)
            break;
    }
    m_window->update();                       // swapBuffers + pollEvents
}
```

`Game::update()` est responsable :
- de la MAJ de la caméra (`m_camera->update(m_player.get())`)
- des cubes / modèles / entités
- de l'`InputManager`
- du `LightManager` (mise à jour des sources, dont la lampe torche du joueur)
- du `SoundManager` (listener = caméra)
- de la mise à jour des colliders dynamiques

`Game::draw()` trace opaques, puis `glDisable(GL_DEPTH_TEST)` ne se produit pas — en réalité
les transparents sont triés par distance caméra puis dessinés avec `glDepthMask(GL_FALSE)`
+ `glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)`.

---

## 3. Système de coordonnées & unités

- Monde 3D : mètres (échelle arbitraire, mais cohérente avec `DEFAULT_PLAYER_RADIUS=0.2f`, `DEFAULT_PLAYER_HEIGHT=1.6f`).
- Caméra perspective 60° FOV, near 0.1 / far 100.
- Sprite 2D : projection ortho en pixels (`Constants::WINDOW_WIDTH=1920`, `HEIGHT=1080`).
- Constantes tunables : **toutes** dans `OpenGLProject/constants.h` (namespace `Constants`).
  Ne pas hardcoder ailleurs — ajouter une constante et la référencer.

---

## 4. Modules détaillés

### 4.1 Game (`Game.h/.cpp`)
- `initialize()` crée les managers dans le bon ordre (le `Player` est créé après `CollisionManager`).
- Appelle `m_collisionManager->buildBVH()` après avoir ajouté tous les statiques (sinon la requête BVH est désactivée — `m_useBVH=false`).
- **Règle de propreté** : `Game.cpp` ne doit PAS contenir de logique de chargement d'assets, de setup d'animation, ou de code spécifique à un sous-système. Chaque nouveau sous-système (ex: bras riggés, armes, inventaire…) doit être extrait dans une classe Manager dédiée (ex: `ArmsManager`). `Game::initialize()` ne fait qu'instancier et appeler `manager->initialize(...)`. `Game::update()` et `Game::draw()` délèguent aux managers. Le code de `Game.cpp` doit tenir en ~50 lignes utiles hors includes et boilerplate. Si un bloc de code dépasse 10 lignes dans `Game.cpp`, c'est qu'il mérite sa propre classe.

### 4.2 Window (`Window.h/.cpp`)
- Wrapper GLFW3. Forward-declare `GLFWwindow` dans le header (n'inclut jamais GLFW dans l'API publique).
- `setCursorCaptured(bool)` met `GLFW_CURSOR_DISABLED` (true) ou `GLFW_CURSOR_CAPTURED` (false). Le naming actuel est inversé de l'intuition — c'est un quirk connu, ne pas refactorer sans re-tester le focus souris.

### 4.3 Renderer (`Renderer.h/.cpp`)
- **Double rôle** : calcul `deltaTime` + cap FPS + `glClear`.
- Cap FPS par défaut **désactivé** (`Constants::DEFAULT_IS_FPS_CAPPING=false`), cible 240.
- Méthode : `std::this_thread::sleep_for(1ms)` puis yield actif (le burn CPU est volontaire quand le cap est ON).

### 4.4 Camera (`Camera.h/.cpp`) — non lue exhaustivement ici, suit l'impl standard LearnOpenGL.

### 4.5 Player & Entity
- `Entity` = classe de base pour tout objet positionné.
- `Player : Entity` ajoute : `m_isSprinting`, `m_isFlashlightEnabled`, `m_wantsToMove`.
- `setUseGravity(true)` par défaut dans le constructeur.
- `m_position` = **bas de la capsule** ; `Constants::PLAYER_EYE_HEIGHT = glm::vec3(0,1,0)` n'a que la composante Y d'utile — c'est l'offset vertical entre `m_position` et les yeux. `Player::getEyePosition() = m_position + PLAYER_EYE_HEIGHT`.
- Le mouvement est résolu chaque frame :
  ```cpp
  void updatePositionFromEnvironment(float dt) {
      m_position = m_collisionManager->resolvePlayerMovement(
          m_position, m_frameMovement, dt, m_useGravity);
      m_frameMovement = {};
      m_position = m_collisionManager->pushPlayerAway(m_position);
  }
  ```

### 4.6 CollisionManager
- **Header portable** (ne dépend plus de `<Windows.h>` ; c'était un bug récent).
- AABB statiques calculées une fois via `addStaticMesh` / `addDynamicMesh` (string key).
- Capsule joueur = `radius * 2 = DEFAULT_PLAYER_RADIUS` + `height = DEFAULT_PLAYER_HEIGHT` ; testée comme deux sphères (pieds, milieu, tête).
- **Saut** : `CollisionManager::tryJump(float)` ne s'active que si `m_isPlayerGrounded` ; applique `m_verticalVelocity = jumpVelocity`.
- **Gravité** : `Constants::GRAVITY = -9.81f` + `PLAYER_JUMP_VELOCITY = 12.0f` → ~2.45 s d'air time.
- `testSphereAll()` n'utilise **pas encore** le BVH même si `m_useBVH=true` — c'est un TODO latent.

### 4.7 SoundManager
- OpenAL Soft + EFX (reverb). `isEFXAvailable()` est exposé pour les fallbacks gracieux.
- Le **listener** (position+orientation) est calqué chaque frame sur la caméra (`Game::update`).
- Les sons sont identifiés par **clé string** dans le `load(name, ...)`. Recharger la même clé = réutilisation cache (pas de double-allocation).
- `pauseAll()` / `resumeAll()` sont déclenchées par `window_focus_callback` : focus perdu → pause, focus rendu → reprise.

### 4.8 InputManager
- Pattern **Key** (classe de base) avec actions par contexte :
  ```cpp
  setOnPressAction(InputContext::GAME, [this]{ m_player->processJump(); });
  setIfPressedAction(InputContext::GAME, [this]{ m_player->processDirectionKey(FORWARD); });
  ```
- Chaque touche concrète (Forward.h, Jump.h, Flashlight.h…) hérite de `Key` et configure ses actions dans son constructeur.
- `update()` lit `glfwGetKey` + dispatch `ifPressed` / `onPress` / `onRelease`.

### 4.9 MenuManager / State Machine
- Énum : `enum GameState { STATE_MENU, STATE_PLAYING, STATE_OPTIONS, STATE_PAUSED }` dans `gamestate.h`.
- `changeState(newState)` recalcule `InputContext` + capture du curseur.
- Chaque menu (MainMenu, PauseMenu, OptionsMenu) hérite de `Menu` et peuple ses items / sons.
- **AFK** : après `Constants::MAINMENU_AFK_THRESHOLD = 6.0f` secondes sans input, le `MainMenu` anime son `DVDShape` (logo qui rebondit). Piloté par `MenuManager::update(isAFK)`.
- **Sons étranges aléatoires** : `Constants::WEIRD_SOUND_INTERVAL = 12.0f` détermine la cadence à laquelle le `MainMenu` rejoue un des 6 bruitages "horror" pour taper sur l'ambiance.

### 4.9.bis Shapes 2D & `SharedQuad` (essentiels pour tout `Menu`/`Image`)
- Hiérarchie : `Shape` (classe abstraite) → `Triangle`, `Rectangle`, `Image`, `MaskImage`, `DVDShape`.
- API publique commune : `setPosition(x,y)`, `setSize(w,h)`, `setColor(r,g,b)`, `setRotation(angle)`, `getIsVisible()`, `setIsVisible(bool)`, hitbox `isPointInside(px,py)`.
- `Menu::addItem(label, x, y, w, h, onClick)` ajoute un bouton cliquable ; `addShape(id, shapePtr)` enregistre une forme décorative.
- `Image` charge un fichier via `ImageLoader` (lib `ImageLoader.h` + `res/textures/…`) ou un ID OpenGL brut ; utilise **`SharedQuad`** (header `SharedQuad.h`) pour partager un seul quad VAO/VBO entre toutes les images (économie mémoire importante).
- `MaskImage : Image` ajoute une couleur de modulation uniforme via le shader `mask`/`image/masque`.

### 4.10 Shaders & types de shader
- **Règle d'auto-découverte** : chaque dossier *feuille* qui contient `{foldername}.vert` + `{foldername}.frag` devient un shader indexé par son chemin relatif. Pas forcément un sous-dossier : `res/shaders/cube/cube.vert` existe en plus de `res/shaders/cube/lightsource/lightsource.vert`. Résultat : il y a deux shaders dans l'arbre des cubes, `cube` et `cube/lightsource`, à ne pas confondre.
- Dossiers actuels : `res/shaders/{cube, image, model, shape, text}/…`.
- `ShaderManager` les auto-découvre via `std::filesystem::recursive_directory_iterator` et les range dans une `std::unordered_map<std::string, ShaderNode*>` indexée par le chemin relatif.
- `Shader` stocke à la fois `m_name` (debug) **et** `m_type` (enum typé pour le dispatch runtime) :
  - `m_type` est résolu **une seule fois** dans le constructeur via `extractShaderType(name)` (cf. `ShaderType.h`).
  - Les `draw()` comparent `getType()` (`enum class`, 1 cycle) au lieu de chaînes (per-frame).
- **Pour ajouter un nouveau type** : étendre `ShaderType` + ajouter la correspondance dans `extractShaderType`, puis mettre à jour les `if/else` dans les `draw()` qui en ont besoin.
- ⚠️ **Incohérence connue** : `MainMenu.cpp` charge `"image/mask"` mais le dossier physique est `image/masque/`. Si vous renommez le dossier, ajustez aussi l'appel.

### 4.11 TextureManager
- Idem `ShaderManager` mais avec un arbre `TextureNode*`. Charge aussi `details.json` (shininess/specular) s'il est présent à côté de chaque texture.

### 4.12 LightManager
- `MAX_LIGHTS_SOURCES = 10` constant ; `addPointLight(LightSource*)` accumule jusqu'à la limite.
- Les uniforms (`position`, `ambient`, …) sont stockés dans `LightUniformStrings` pour chaque slot.
- La lampe torche (Spotlight) est un slot à part entière, attachée au joueur.

---

## 5. Conventions de code

### Style C++
- **Membres** préfixés par `m_`.
- **Getters inline courts** dans le header ; calcul plus long → déclaration dans le `.h`, définition dans le `.cpp`.
- **Forward declarations** dans les headers pour limiter la surcharge ; `class Foo;` est préféré à `#include "Foo.h"`.
- **Pas d'exceptions partout** — préférer `throw`/`out_of_range`/`runtime_error` quand pertinent (cohérent avec `ShaderManager::getShader`).
- **Emojis/symboles décoratifs** interdits dans le code ; commentaires box-drawing `─` OK.

### GPU / OpenGL
- `glad.c` est l'unique `.c` du projet (loader auto-généré).
- Chaque `Shader*` doit être relâché par son propriétaire. Utiliser `std::unique_ptr<Shader>` au-dessus de `ShaderManager`.
- Ne jamais faire `glUseProgram(0)` puis continuer à envoyer des uniformes — invalide.
- Toujours re-vérifier l'extension avant d'utiliser EFX (`isEFXAvailable()`).

### Audio
- `Sound::play()` n'est **pas** thread-safe. Toujours appeler depuis le thread principal.
- `pitch != 1.0` = effet Doppler-like ; ne pas confondre avec `gain`.

### Physique / collisions
- `radius` est en mètres, pas en pixels.
- Pour modifier la gravité, garder `GRAVITY < 0`. Sinon la capsule lévite.
- `PLAYER_JUMP_VELOCITY` doit rester positif. Une garde existe côté `tryJump` mais c'est une ceinture-bretelles, pas une API publique.

---

## 6. Tâches courantes — recettes

### Ajouter un nouveau shader
1. Créer `res/shaders/<cat>/<nom>/<nom>.vert` et `<nom>.frag`.
2. Lancer : `ShaderManager` le détecte au boot.
3. Récupérer : `m_shaderManager->getShader("<cat>/<nom>")`.
4. Si le shader a un comportement C++ spécifique (uniforms conditionnels), ajouter une entrée dans `ShaderType` et `extractShaderType`, puis utiliser `m_shader->getType()` dans le `draw()`.

### Ajouter une touche
1. Créer `MyAction.h/.cpp` qui hérite de `Key`.
2. Dans le constructeur : `setOnPressAction(InputContext::GAME, [this]{ /* … */ });`.
3. Ajouter une constante `KEY_MY_ACTION` dans `configKeys.h`.
4. Enregistrer dans `InputManager::loadKeys()`.

### Ajouter un son 3D
```cpp
Sound* footstep = m_soundManager->load("footstep", "res/sounds/step.wav");
footstep->setPosition(player.getPosition());
footstep->play();
```
Ne pas oublier `m_soundManager->setListenerTransform(...)` chaque frame (déjà fait dans `Game::update()`).

### Ajouter un collider
- Statique : `m_collisionManager->addStaticMesh(mesh, modelMatrix, "debug_name");`
- Dynamique : `m_collisionManager->addDynamicMesh("key", { mesh }, modelMatrix);`
- Puis **appeler `m_collisionManager->buildBVH()`** une seule fois après tous les add statiques (déjà fait dans `Game::initialize()`).

### Modifier le rendu d'un cube
- `Cube::draw()` switch sur `m_shader->getType()` (LightSource / SeveralLights / unknown).
- Pour ajouter un nouveau type de shader cube : idem, ajouter dans `ShaderType` + case dans `Cube::draw()`.

### Lire / sauver des préférences utilisateur
- `OptionsMenu` est lié à `Constants::JSON_OPTION_PATH = "./res/options.json"`.
- Utiliser `nlohmann::json` (déjà inclus) pour sérialiser.

---

## 7. Points d'attention / dette technique

- **BVH câblé** : `CollisionManager::testSphereAll()` passe désormais par `m_bvh.querySphere(...)` pour les statiques (`O(log n)`). Les **dynamiques restent en `O(m)` linéaire** parce qu'elles bougent à chaque frame — reconstruire un BVH toutes les frames annulerait le gain. Si le nombre de dynamiques explose (>50 typiquement), envisager soit un rebuild BVH périodique (toutes les N frames), soit un Spatial Hashing pour les dynamiques.
- **`Constants.h` est inclus via `Constants.h` (majuscule) dans `CollisionManager.h`** alors que le fichier est `constants.h` (minuscule). Windows tolère (case-insensitive), mais ça **cassera au port Linux**. À harmoniser.
- **Incohérence path shader** : `MainMenu.cpp` demande `"image/mask"` alors que le dossier physique est `image/masque/`. Soit on renomme le dossier, soit on renomme l'appel.
- **`Image::drawGradient()` n'existe pas** dans la version actuelle — `Image::draw()` ne fait pas de comparaison string. Si quelqu'un l'ajoute, brancher immédiatement sur `ShaderType`.
- **Shadowing de variables** déjà corrigé dans `MainMenu.cpp` (chargement de `menu_music`).
- **`Direction` stocke en `double`** puis cast en `float` à la toute fin (`getDirectionVector`). Précision utile pour l'accumulation yaw sur le long terme, mais gaspillée si la session est courte. À rediscuter si perf critique (sinon, tout en `float`).
- **`Window::setCursorCaptured(bool)` naming inversé** : `true` ⇒ `GLFW_CURSOR_DISABLED` (FPS), `false` ⇒ `GLFW_CURSOR_CAPTURED` (et non `NORMAL`). Pourquoi : les menus veulent un curseur capté à la fenêtre. Ne pas refactorer sans re-tester la capture du curseur en jeu vs menu.
- **TODOs `main.cpp`** : marque "Collision (Hitbox)" et "Multi" mais la collision est largement couverte par `CollisionManager`. Le TODO collisionnel historique est obsolète.
- **`SoundManager::window_focus_callback` est publique** mais est appelée uniquement par le callback GLFW enregistré dans le constructeur (`glfwSetWindowFocusCallback`). Ne pas l'appeler à la main.
- **Pas de tests unitaires** — toute modif physique / sonore doit être testée à la main en jeu.

---

## 8. Commandes utiles (bash / Windows)

```bash
# PowerShell/cmd : ouvrir le projet
start OpenGLProject.sln

# Chercher un symbole partout
grep -rn "GRAVITY" OpenGLProject/

# Build en ligne de commande (si MSBuild dispo)
msbuild OpenGLProject.sln /p:Configuration=Debug /p:Platform=x64
```

### 8.bis DLLs tierces requises au runtime

Le `PostBuildEvent` du `.vcxproj` copie `dependencies\bin\*.dll` vers `$(OutDir)`. Au runtime il faut au minimum :

- `glfw3.dll` → utilisé par `Window`.
- `OpenAL32.dll` (+ possiblement `soft_oal.dll` selon la version) → utilisé par `SoundManager`. Si le son plante, vérifier ces DLLs.
- DLLs **runtime Visual C++** (vcredist) — à installer une fois sur le poste.

Si vous déplacez l'exécutable sur une machine vierge sans ces DLLs, le programme crash au démarrage.

### 8.ter Référence rapide API ↔ implémentation

Pour ne pas se perdre dans les fichiers `.h` / `.cpp` :

| Classe | Header | Implémentation | Notes |
|---|---|---|---|
| `Window` | `Window.h` (forward `GLFWwindow`) | `Window.cpp` | Pas de GLFW dans le header |
| `Renderer` | `Renderer.h` | `Renderer.cpp` | deltaTime + clear |
| `Camera` | `Camera.h` | `Camera.cpp` | Suit `Player` |
| `SoundManager` | `SoundManager.h` | `SoundManager.cpp` (DLL via `#pragma comment(lib)`) | OpenAL + EFX |
| `ShaderManager` | `ShaderManager.h` | tout est inline dans le header | Pas de `.cpp` dédié |

---

## 9. Glossaire interne

| Terme | Signification |
|---|---|
| Capsule | Modèle du joueur = 3 sphères empilées (pieds, milieu, tête). |
| Sweep | Mouvement d'une sphère le long d'un vecteur avec résolution itérative des contacts. |
| BTK | (interne) abréviation inutilisée — historique. |
| AFK | "Away From Keyboard" — utilisé dans `MainMenu::update(bool isAFK)` pour activer le logo DVD. |
| BVH | Bounding Volume Hierarchy — structure d'accélération collision statique. |
| EFX | OpenAL Extension FX — réverbération & filtres avancés. |

---

## 10. Contact & licence

Projet étudiant suivant LearnOpenGL.com. Pas de licence open-source explicite
au-delà de celle des libs vendored (`dependencies/`).
