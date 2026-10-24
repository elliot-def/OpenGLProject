# OpenGLProject

Moteur de jeu 3D "maison" écrit en C++20 / OpenGL 4.5, développé sous Visual Studio (Windows), sur lequel est construit un **jeu d'horreur / de poursuite solo et multijoueur** : des joueurs enfermés dans une carte doivent accomplir des tâches (mini-jeux) tout en échappant à **fropy**, une créature qui les chasse, les écoute et ouvre les portes. Le projet part d'un rendu OpenGL bas niveau (via GLAD) et construit par-dessus un petit moteur complet : chargement de modèles riggés (Assimp), animation squelettique avec crossfade, collisions AABB/OBB accélérées par BVH, audio 3D (OpenAL + EFX), cartes décrites en JSON, IA avec pathfinding A*, un système de menus navigable à la souris et à la manette, une intégration Steamworks (lobbies, invitations, Steam Input) et le multijoueur temps réel (positions, chat textuel, **chat vocal**) en P2P Steam.

> La quasi-totalité des commentaires du code source sont en français ; ce README suit la même convention. Le guide technique à destination des développeurs/IA est dans [`agent.md`](agent.md).

---

## Table des matières

1. [Vision globale](#vision-globale)
2. [Le jeu en pratique](#le-jeu-en-pratique)
3. [La créature (fropy)](#la-créature-fropy)
4. [Quêtes et mini-jeux](#quêtes-et-mini-jeux)
5. [Multijoueur](#multijoueur)
6. [Fonctionnalités du moteur](#fonctionnalités-du-moteur)
7. [Architecture du projet](#architecture-du-projet)
8. [Arborescence des dossiers](#arborescence-des-dossiers)
9. [Dépendances](#dépendances)
10. [Compilation](#compilation)
11. [Lancement et arguments](#lancement-et-arguments)
12. [Contrôles](#contrôles)
13. [Fichiers de configuration persistants](#fichiers-de-configuration-persistants)
14. [Cartes JSON (MapLoader)](#cartes-json-maploader)
15. [Animation squelettique](#animation-squelettique)
16. [Collisions et physique](#collisions-et-physique)
17. [Audio](#audio)
18. [Écran de chargement et threading](#écran-de-chargement-et-threading)
19. [Outils et auto-tests](#outils-et-auto-tests)
20. [Points d'attention / pièges connus](#points-dattention--pièges-connus)
21. [Pistes d'amélioration](#pistes-damélioration)

---

## Vision globale

**Le jeu.** Un groupe de joueurs (solo ou lobby Steam) apparaît dans une carte fermée — un village de nuit, un village de jour ou un bâtiment. Leur objectif : accomplir toutes les tâches de la partie (des mini-jeux répartis sur des postes de la carte) avant que **fropy**, une créature qui rôde, ne leur retire toutes leurs vies. Fropy voit, entend (la **voix des joueurs en chat vocal émet du bruit qu'elle capte**), ouvre les portes qui barrent sa route et se repère sur toute la carte. La difficulté, la carte et le mode de jeu (chasse ou exploration paisible) sont choisis par l'hôte dans une salle de lobby avant de lancer la partie.

**Le moteur.** Tout est écrit à la main pour apprendre : fenêtrage et entrées (GLFW), rendu (OpenGL 4.5 via GLAD), mathématiques (GLM), import de modèles et d'animations (Assimp), audio 3D avec réverbération (OpenAL + EFX), physique simple (capsule, sweep de sphère, BVH), UI 2D (atlas de glyphes stb_truetype, rendu de texte batché), réseau P2P (Steam Networking Messages) et voix (capture OpenAL, compression IMA ADPCM). Le gameplay (poursuite, quêtes, lobby, multijoueur) est posé par-dessus, dans des managers séparés de l'orchestration (`Game` ne fait que de l'orchestration, cf. [`agent.md`](agent.md)).

**La direction artistique.** Low-poly assumé, ambiance horrifique "maison hantée" (sons d'ambiance étouffés, voicelines de fropy bitcrushées, brume volumétrique la nuit), et une signature visuelle : le **soleil et la lune sont une tête de fropy géante** dans le ciel qui regarde la créature et l'éclaire — avec éblouissement plein écran quand on la regarde en face. Les écrans de télévision des cartes diffusent en boucle le logo DVD rebondissant du menu principal.

**Où le projet va.** (cf. [`todo.md`](todo.md)) Vrai multijoueur abouti, éditeur de cartes, PNJ avec dialogues, personnalisation visuelle des personnages (torche, sac à dos...), normal mapping étendu, support de modding communautaire, et une DA toujours plus cohérente (modèles low-poly, sons bitcrushés).

Le jeu tourne actuellement **exclusivement sous Windows** (Win32, Winsock, XInput, Steamworks).

---

## Le jeu en pratique

### Boucle d'une partie

1. **Menu principal** → clic sur **Jouer** (ou arrivée dans un lobby Steam après une invitation).
2. **Salle du lobby** (`res/maps/lobby/lobby.json`) : une maison géante où tous les joueurs apparaissent. Contre le mur du fond, **4 fropys qui tournent** font office de boutons : l'hôte les actionne avec `E` pour choisir la **difficulté**, la **carte**, le **mode de jeu**, puis **lancer la partie**.
3. **Partie** : les joueurs apparaissent sur la carte choisie. Un panneau HUD liste les tâches à accomplir ; chaque tâche est un poste (fropy tournant) à activer avec `E`, qui ouvre un mini-jeu. Pendant ce temps, fropy chasse (en mode Chasse).
4. **Fin de partie** :
   - **Victoire** : toutes les tâches accomplies → message, puis retour à la salle du lobby.
   - **Défaite** : un joueur touché par fropy perd une vie ; plus aucune vie → fin de partie, puis retour au lobby.

Le mode **Exploration** (choisi par l'hôte) désactive fropy : on visite la carte sans menace, utile pour découvrir les maps ou jouer aux mini-jeux tranquillement.

### Options de partie (l'hôte)

Les options sont choisies dans le lobby, synchronisées vers tous les joueurs via les données du lobby Steam, et affichées dans un résumé à droite du HUD. Une graine (`seed`) tirée par l'hôte détermine les postes de tâches tirés à chaque carte — **identiques chez tous les joueurs** d'une même partie.

| Option | Valeurs | Effets |
|---|---|---|
| **Difficulté** | Facile / Normal / Difficile | Vies (5 / 3 / 1), nombre de tâches (3 / 5 / 8), vitesse de fropy (×0.8 / ×1.0 / ×1.3) et ses sens (vue+ouïe : ×0.75 / ×1.0 / ×1.35) |
| **Carte** | Village (jour) / Village (nuit) / Bâtiment | La carte chargée (`res/maps/...`) |
| **Mode de jeu** | Chasse / Exploration | Fropy actif ou non |
| **Lancer la partie** | — | Recharge la carte choisie chez tous les joueurs |

En solo, le joueur est son propre hôte : mêmes boutons, même déroulé.

### Cartes

Les cartes sont des fichiers **JSON** (géométrie, matériaux, lumières, portes, boutons, spawns, ambiance) chargés par `MapLoader` — cf. [Cartes JSON](#cartes-json-maploader).

| Carte | Fichier | Particularités |
|---|---|---|
| **Lobby** | `res/maps/lobby/lobby.json` | Maison géante (24×16 m), 4 boutons fropy, pas de créature |
| **Village (jour)** | `res/maps/village/village_day.json` | Ciel de jour, tête-fropy soleil jaune ; inclut `village_common.json` + `house.json` (maisons instanciées plusieurs fois) |
| **Village (nuit)** | `res/maps/village/village_night.json` | Brouillard + brume en particules, tête-fropy lune blanc-gris, portée de vue de fropy réduite |
| **Bâtiment** | `res/maps/batiment/batiment.json` | Export Blender (`batiment.obj`/`.mtl` + textures), collision précise par triangles, étages et escaliers |

### Déroulé concret en jeu

- **Tâches** : postes nommés `quest:<type>:<id>` dans la carte (clés `"buttons"`). À chaque partie, `count` postes sont tirés (graine commune) et les autres sont masqués. Un poste réussi disparaît et se coche dans le HUD.
- **Fropy** : poursuit, ouvre les portes, perd l'agro si on lui échappe, retourne en ronde. Un joueur touché perd une vie (courte invulnérabilité après un coup, et fropy est brièvement étourdi).
- **Portes** : `E` ouvre/ferme la porte visée (elle s'ouvre toujours du côté opposé au joueur). Un contour pulsé signale les portes interactives ; fropy pousse celles qui barrent sa trajectoire.
- **HUD** : liste des tâches, vies restantes, invite `[E] ...` sur l'élément visé, chat, joueurs qui parlent en vocal.
- **Interactions** : tout ce qui est "actionnable" (options du lobby, postes de quête, boutons) est un **fropy qui tourne**, visé au centre de l'écran et activé avec `E`.

---

## La créature (fropy)

`Creature` est un ennemi complet, réutilisable sur toutes les cartes :

- **Aspect** : modèle articulé basse-poly (`res/models/fropy/fropy_player.glb`, squelette Mixamo construit par `res/models/fropy/make_fropy_player.py`), animé en repos / marche / course selon sa vitesse réelle.
- **Perception** : **vue** (portée configurable par carte, test de ligne de vue échantillonné avec occlusion) et **ouïe** (chaque bruit émis — voix des joueurs en chat vocal, etc. — est un `NoiseEvent` avec une position et une portée ; les murs réduisent la portée d'un bruit non vu).
- **États** : *Patrol* (ronde sur toute la carte via une grille de cellules, en visant toujours la zone la plus ancienne), *Chase* (poursuite du joueur le plus proche visible), *Investigate* (se rend au dernier bruit entendu et cherche sur place).
- **Déplacement** : **A\* sur grille** (XZ + hauteur du sol : monte les escaliers, marche par marche), recalculé périodiquement, lissage des waypoints par visibilité, repli en ligne droite si dégagé. Le corps passe par `sweepSphere` : fropy **glisse le long des murs** au lieu de les traverser, et **ouvre les portes fermées** qui barrent son chemin (avec un surcoût dans l'A\*).
- **Réglages** : tous dans `OpenGLProject/constants/creature.h` (vitesses, portées, temporisations, taille de grille A\*...). La difficulté applique des multiplicateurs de vitesse et de sens.
- **Voix** : `CreatureVoice` joue des voicelines bitcrushées (`res/sounds/fropy/voicelines`, générées par `make_voicelines.py`) **spatialisées à sa tête** : atténuées par la distance, étouffées (passe-bas) derrière un mur.

Côté ciel, `SkySystem` place la **tête de fropy géante** (soleil jaune le jour, lune blanc-gris la nuit) qui oriente son regard vers la créature et l'éclaire d'un spot ; regarder le soleil sans obstacle provoque un éblouissement plein écran (halo + rayons + voile).

---

## Quêtes et mini-jeux

`QuestManager` gère les tâches de la partie ; `MiniGames` contient la **logique pure** des mini-jeux (aucun appel OpenGL — testée par `-selftest`), le rendu passant par une interface `QuestPainter`. Quatre types de postes :

| Type | Nom | Principe |
|---|---|---|
| `wires` | Rebrancher les câbles | Relier chaque couleur de gauche à sa prise de droite |
| `simon` | Pirater le digicode | Mémoriser puis rejouer une séquence de flèches (3 manches) |
| `timing` | Calibrer la chaudière | Bloquer un curseur dans une zone qui rétrécit (3 réussites) |
| `wave` | Accorder la radio | Caler fréquence et amplitude sur une onde cible |

Un mini-jeu s'ouvre en panneau plein écran (touche `E` sur le poste) qui capture les commandes — **fropy continue de chasser pendant ce temps**. Commandes : flèches/ZQSD + `Entrée`/`Espace` au clavier, croix directionnelle + `A` à la manette. Réussi : le poste disparaît, la tâche se coche ; si fropy vous attrape pendant un mini-jeu, il se ferme. Toutes les tâches faites → victoire.

Les visuels du HUD des quêtes sont générés par `res/textures/quests/make_quest_ui.py` (panneaux, icônes, coches).

---

## Multijoueur

Le multijoueur repose sur les **lobbies Steam** et la **P2P fiable `ISteamNetworkingMessages`** (la couche TCP historique `Socket`/`Packet` existe encore mais n'est pas branchée au gameplay).

### Synchronisation des joueurs (`MultiplayerManager`)

- Chaque client diffuse son état (position, cap, animation courante) **~20×/s** à tous les membres du lobby.
- Les joueurs distants sont représentés par un modèle de personnage (`Megan`, mêmes animations Mixamo que le joueur local), créé paresseusement à la réception du premier paquet.
- **Interpolation** entre deux checkpoints consécutifs pour un rendu fluide ; au-delà de ~5 s de silence P2P, le pair est considéré déconnecté.
- Les positions des joueurs distants sont transmises à la créature : **fropy peut chasser n'importe qui**, visible ou non.
- Le monde continue de tourner quand un menu est ouvert (le personnage ne se fige plus en l'air et reste visible chez les autres).

### Chat textuel (`LobbyChat`)

- `Entrée` ouvre la saisie, `Entrée` envoie, `Echap` annule. Historique des messages joueurs + logs système (lobby créé, arrivée/départ des joueurs), pseudo Steam affiché.
- Diffusé à tous les membres du lobby via le même canal P2P fiable.

### Chat vocal (`VoiceChat`)

- **Capture** OpenAL (`alcCaptureOpenDevice`), 16 kHz mono 16 bits, traitée par blocs de 20 ms sur le thread principal (aucun thread supplémentaire).
- **Compression IMA ADPCM** (4:1) avec état de codeur transporté par bloc : chaque paquet se décode indépendamment, les pertes/désordres du réseau non-fiable ne corrompent pas la suite.
- **Transmission** en paquets NON-fiables (175 octets, magic `MPGV`) sur le canal P2P ; réception et lecture par **une source OpenAL par joueur distant** (file de buffers, ~160 ms de latence max).
- **Activation** : détection vocale (VAD, seuil réglable) **ou** push-to-talk (touche `V`). Micro coupable individuellement.
- Réglages (activation, mode, sensibilité, volume des voix, **périphérique de capture**) dans Options → Audio, persistés dans `res/options.json` (section `"voice"`).
- Les pseudos des joueurs qui parlent s'affichent au-dessus du chat (icône haut-parleur) — y compris en solo pour le micro local.

**Boucle de gameplay liée au vocal** : parler émet un `NoiseEvent` autour de votre position (`Constants::Player::PLAYER_VOICE_NOISE_RADIUS`) que fropy peut entendre — **parler peut vous trahir**.

---

## Fonctionnalités du moteur

### Rendu
- Chargement OpenGL via **GLAD** (spécification `gl=4.5`, profil *compatibility*, contexte créé en 3.3 core par GLFW).
- Rendu **instancié** des cubes (`CubeRenderer` / `glDrawElementsInstanced`) : un seul cube unitaire partagé, matrices modèle + couleur envoyées par instance.
- Chargement de modèles 3D via **Assimp** (`.obj`, `.fbx`, `.glb`/`.gltf`) avec extraction de bones, textures embarquées, et dédoublonnage des meshes partagés. Chargeur **OBJ maison** (`ObjLoader`) pour les exports Blender des cartes (UV, matériaux `.mtl`, couleurs, opacité/vitres).
- Skinning GPU (jusqu'à `SHADER_MAX_BONES = 100` os), 4 influences par sommet.
- **Skybox** cubemap par carte (dossier de 6 faces déclaré dans le JSON), rendue en profondeur `GL_LEQUAL`.
- Passe d'**outline** (silhouette) réutilisable : shapes 2D, meshes 3D, modèles riggés et **contours pulsés des portes interactives**.
- Éclairage : jusqu'à `MAX_LIGHTS_SOURCES = 10` lumières ponctuelles (restreintes à une pièce via les `"rooms"` des cartes), une lumière directionnelle (le soleil de la carte), la **lampe torche** du joueur (spotlight à l'os de tête), et le **spot de la tête-fropy** céleste qui éclaire la créature.
- **Normal mapping** sur les murs de briques (shader `cube/wall`, TBN reconstruit par dérivées dans le fragment shader).
- **Brouillard** : fog exponentiel par carte (`"fog"`) + **brume volumétrique en particules** (`FogParticles` : ~300 billboards autour du joueur, texture procédurale, tri arrière→avant).
- Effet de **fondu de proximité** (near-fade) en vue première personne.
- Rendu de texte batché (un seul `glDrawArrays` par frame) via un atlas de glyphes `stb_truetype`, UTF-8 (icônes manette/clavier de la police Kenney).
- Écran de chargement animé (barre de progression par étapes, fondu de sortie).

### Animation
- `Animator` : lecture d'animations Assimp avec interpolation position/rotation, boucle ou "one-shot", **crossfade** sur les transformées *locales* (lerp + slerp).
- Réparation automatique des clés de rotation corrompues par Assimp 5.4.x sur certains GLB (`GlbAnimationRepair`).
- Compatible rigs **Mixamo** : préfixes `mixamorig:`, suffixes FBX `_$AssimpFbx$_Rotation`, squelettes multiples, `AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS=false`.
- Machine à états du personnage 3P (`CharacterAnimationController`) : idle/marche/course, strafes, virages, saut, chute, atterrissage (hystérésis anti-clignotement).
- Bras 1P (`FirstPersonArms`) + **caméra et lampe torche attachées à l'os de tête** (`HeadCameraRig`, avec limite du lean de la tête contre les murs).
- Animations synchronisées aux joueurs distants (l'animation jouée est transmise avec l'état P2P).

### Collisions & physique
- `CollisionManager` : AABB statiques, **BVH** pour les requêtes sphère/AABB en O(log n), **collisions triangles** (`addStaticTriangleSoup`, grille de cellules de 4 m) pour les modèles OBJ et les toits en pente.
- OBB pour les objets dynamiques rotatifs ; colliders de **portes** mis à jour à la rotation.
- Résolution de mouvement joueur par **sweep de sphère** avec glissement itératif et sub-stepping anti-tunneling ; la créature utilise le même sweep.
- Gravité, saut, ralenti post-atterrissage, mode **no-clip** (vol libre).

### Audio
- **OpenAL** + **EFX** : sons positionnels 3D, atténuation par distance, Doppler, réverbération par presets (`ROOM`, `CAVE`, `UNDERWATER`...), filtre passe-bas (occlusion des voicelines de fropy).
- **Musique de fond en streaming** (`MusicStream`, Ogg Vorbis via stb_vorbis) : décodage par morceaux, jamais le PCM complet en mémoire.
- Pause/reprise automatique des sons à la perte/reprise du focus fenêtre.
- Chargement WAV PCM (8/16 bits) fait à la main ; cache par nom dans `SoundManager`.
- Voix des joueurs (chat vocal) et voicelines de fropy spatialisées (cf. ci-dessus).

### Entrées
- Abstraction `Controller` : `GlfwController` (XInput via GLFW) et `SteamInputController` (**désactivé**, voir [Points d'attention](#points-dattention--pièges-connus)).
- Bascule automatique clavier/souris ↔ manette avec notification à l'écran.
- Rebinding complet (clavier + manette), persisté dans `res/keys.json` ; sensibilités séparées souris/stick.
- Navigation manette dans les menus (stick + croix, répétition configurable).

### Menus / UI
- Menu principal (logo DVD rebondissant après inactivité, sons d'ambiance aléatoires), menu pause, options (volume général, musique), **sous-menu Audio** (chat vocal : micro, VAD/PTT, sensibilité, volume des voix, périphérique de capture), rebinding clavier/souris et manette.
- Widgets : boutons, sliders, cases à cocher, listes déroulantes ; curseurs personnalisés animés (dont un curseur "attente" 8 frames).
- HUD de partie : tâches, vies, invites d'interaction, chat, indicateurs vocaux (cf. [Multijoueur](#multijoueur)).

---

## Architecture du projet

```
Game                       orchestration uniquement (fenêtre, boucle, états, menus, son, réseau, loading)
 ├─ Window                 fenêtre GLFW + contexte OpenGL + contexte partagé (loading thread)
 ├─ Renderer                timing des frames, delta time, cap FPS optionnel
 ├─ Scene                   monde 3D : carte, lumières, entités, créature, ciel
 │   ├─ MapLoader            carte JSON (boîtes, toits, portes, arbres, OBJ, boutons, lumières...)
 │   ├─ CubeRenderer         rendu instancié des cubes
 │   ├─ LightManager         lumières ponctuelles par pièce + DirLight + flashlight + spot céleste
 │   ├─ Creature + CreatureVoice   fropy : A*, perception, poursuite, voicelines spatialisées
 │   ├─ SkySystem             tête-fropy soleil/lune + éblouissement
 │   ├─ FogParticles          brume volumétrique
 │   ├─ Door / TvScreen       portes interactives, écrans DVD
 │   └─ ModelEntity(s)        personnage joueur (Megan), fropy articulé, boutons fropy
 ├─ Camera + HeadCameraRig   vue 1P/3P, caméra/lampe sur l'os de tête
 ├─ Player : Entity          déplacement, saut, sprint, no-clip
 ├─ CollisionManager         AABB/OBB/BVH + collisions triangles, sweep de sphère
 ├─ ModelLoader              chargement asynchrone des modèles (thread + contexte GL partagé)
 ├─ FirstPersonArms          viewmodel bras 1P/3P
 ├─ InputManager             clavier, souris, manette, rebinding
 ├─ MenuManager              machine à états des menus (Menu/Pause/Options/Audio/Bindings)
 ├─ QuestManager + MiniGames tâches de la partie (mini-jeux, HUD)
 ├─ LobbyManager             salle du lobby, options d'hôte, vies, victoire/défaite
 ├─ MultiplayerManager       synchro P2P des joueurs (20 Hz, interpolation), chat
 ├─ LobbyChat / VoiceChat    chat textuel et chat vocal
 ├─ SoundManager + MusicStream   OpenAL, EFX, musique streaming
 ├─ ShaderManager / TextureManager / ResourceTree   ressources auto-découvertes dans res/
 ├─ Socket                   client TCP Winsock (historique, non branché au gameplay)
 └─ SteamManager              Steamworks : init, lobbies, invitations, P2P, overlay
```

### Boucle de jeu (`Game::run`)

1. **Phase `LOADING`** : écran de chargement animé pendant que `ModelLoader` charge les modèles 3D sur un thread séparé (contexte GL partagé). Une fois terminé : recréation des VAO dans le contexte principal, destruction du contexte partagé, fondu de sortie.
2. **Phase `READY`**, selon l'état courant (`GameState`) :
   - `STATE_MENU` / `STATE_OPTIONS` / `STATE_PAUSED` : entrées + menu courant, rendu 2D.
   - `STATE_PLAYING` : `Game::update()` (scène, quêtes, lobby, multijoueur, chat, voix, son) puis `Game::draw()` (skybox, opaques, transparences, créature, HUD, chat).

---

## Arborescence des dossiers

```
OpenGLProject/
├── main.cpp, Game.{h,cpp}, Scene.{h,cpp}, Window.{h,cpp}, Renderer.{h,cpp}
├── Camera.{h,cpp}, HeadCameraRig.{h,cpp}, ModelLoader.{h,cpp}, FirstPersonArms.{h,cpp}
│
├── Entity.{h,cpp}, Player.{h,cpp}, Direction.{h,cpp}, Transformation.h
│
├── InputManager.{h,cpp}, InputNotification.{h,cpp}
├── Controller.{h,cpp}, GlfwController.{h,cpp}, SteamController.{h,cpp}
├── Key.h, ControllerKey.{h,cpp}, PlayerKey.{h,cpp}, configKeys.h
├── Mouse.{h,cpp}, LeftClick.{h,cpp}, Escape.{h,cpp}, Push.{h,cpp}, Grab.{h,cpp}
│
├── Shader.{h,cpp}, ShaderManager.h, ShaderType.h, Vertex.h
├── Mesh.{h,cpp}, Model.{h,cpp}, ObjLoader.{h,cpp}, Texture.{h,cpp}, TextureManager.{h,cpp}
├── ImageLoader.{h,cpp}, ResourceTree.h
├── Animator.{h,cpp}, SkinningData.h, GlbAnimationRepair.{h,cpp}
├── LoadingScreen.{h,cpp}
│
├── Shape.{h,cpp}, Rectangle.{h,cpp}, Triangle.{h,cpp}, Image.{h,cpp}, MaskImage.{h,cpp}
├── DVDShape.{h,cpp}, SharedQuad.h
├── Cube.{h,cpp}, CubeRenderer.{h,cpp}
│
├── MapLoader.{h,cpp}, MapGeometry.{h,cpp}, Door.{h,cpp}, TvScreen.{h,cpp}
├── Creature.{h,cpp}, CreatureVoice.{h,cpp}, SkySystem.{h,cpp}, Skybox.{h,cpp}
├── FogParticles.{h,cpp}, Spotlight.{h,cpp}
│
├── QuestManager.{h,cpp}, MiniGames.{h,cpp}
├── LobbyManager.{h,cpp}, LobbyChat.{h,cpp}
├── MultiplayerManager.{h,cpp}, VoiceChat.{h,cpp}
│
├── Menu.{h,cpp}, MenuManager.{h,cpp}
├── MainMenu.{h,cpp}, PauseMenu.{h,cpp}, OptionsMenu.{h,cpp}, AudioSettingsMenu.{h,cpp}
├── KeyBindingsMenu.{h,cpp}, ControllerBindingsMenu.{h,cpp}
├── TextRenderer.{h,cpp}, CursorManager.{h,cpp}
├── RangeInput.{h,cpp}, CheckboxInput.{h,cpp}, SelectInput.{h,cpp}
│
├── LightSource.{h,cpp}, LightManager.{h,cpp}
├── Sound.{h,cpp}, SoundManager.{h,cpp}, MusicStream.{h,cpp}
├── CollisionManager.{h,cpp}, AABB.h, BVHNode.h, Outlineable.h, OutlinePass.cpp
│
├── Socket.{h,cpp}, Packet.{h,cpp}, PacketBuilder.h, ReceiveBuffer.{h,cpp}
├── SteamManager.{h,cpp}, SelfTest.{h,cpp}
│
├── constants/               window, player, camera, physics, renderer, shader, texture,
│                             material, color, network, menu, file, firstPersonArms,
│                             creature, door, resource
│
├── glad.c, win_compat.h, gamestate.h, Log.h, File.h, config.h
│
├── res/
│   ├── shaders/              cube/ (dont cube/wall : normal mapping), model/, skinned/,
│   │                          shape/, image/, skybox/, outline/, text/, leaves/, door/,
│   │                          glare/, fogparticles/, common/, wallhack/ (X-ray, non branché)...
│   ├── textures/              brickwall (+ normal map), container, parquet, leaf, grey_roof,
│   │                          riet, trees/, menu/ (curseurs, dvd_logo.png), quests/ (HUD quêtes)
│   ├── models/                backpack/, fropy/ (fropy_player.glb, fropy_low_poly, tête),
│   │                          trees/
│   ├── rigging/               arm/ (bras 1P), mixamo/models + mixamo/animation (clips)
│   ├── maps/                  lobby/lobby.json, village/ (day, night, common, house),
│   │                          batiment/ (json + obj + mtl + textures/)
│   ├── skybox/                night/, day/ (cubemaps)
│   ├── sounds/                menu/, fropy/voicelines/, on&on.ogg, ...
│   ├── fonts/                 armana/, Gnocchi.ttf, kenney/ (icônes manette + clavier)
│   ├── keys.json              bindings + sensibilités (généré/persisté)
│   ├── options.json           volumes + réglages chat vocal (généré/persisté)
│   └── steam_input_manifest.vdf
│
├── OpenGLProject.vcxproj
└── OpenGLProject.vcxproj.filters
```

---

## Dépendances

| Bibliothèque | Rôle | Notes |
|---|---|---|
| **GLFW3** | Fenêtrage, contexte OpenGL, entrées clavier/souris/manette | Contexte OpenGL 3.3 core |
| **GLAD** | Chargement des fonctions OpenGL | Généré pour `gl=4.5`, profil *compatibility* |
| **GLM** | Mathématiques (vecteurs, matrices, quaternions) | `GLM_ENABLE_EXPERIMENTAL` requis |
| **Assimp** | Import de modèles 3D et d'animations | `.obj`, `.fbx`, `.glb`/`.gltf` ; `assimp-vc143-mtd.lib` / `-mt.lib` |
| **stb_image** | Décodage d'images (PNG/JPEG) | Compilé dans `Texture.cpp` |
| **stb_truetype** | Rendu de police (atlas de glyphes) | Compilé dans `TextRenderer.cpp` |
| **stb_vorbis** | Décodage Ogg Vorbis (musique streaming) | Compilé dans `MusicStream.cpp` |
| **OpenAL (+ EFX)** | Audio 3D, capture micro, réverbération, filtres | `OpenAL32.lib` |
| **nlohmann/json** | Sérialisation JSON | Options, bindings, cartes, matériaux |
| **Steamworks SDK** | Lobbies, P2P (Networking Messages), overlay, Steam Input | `steam_api64.lib` ; AppID de test `480` (Spacewar) |
| **Winsock2** | Sockets réseau TCP (historique) | Windows uniquement |

Toutes les DLL nécessaires (`dependencies/bin/*.dll`) sont copiées dans le dossier de sortie après build (`xcopy` en post-build event).

---

## Compilation

Projet **Visual Studio** (`.vcxproj`), toolset **v143** (Visual Studio 2022 ou ultérieur).

### Prérequis
- Windows 10/11
- Visual Studio avec le workload "Développement Desktop en C++"
- Steamworks SDK placé dans `dependencies/` (headers, libs, DLL) — **le dossier `dependencies/` n'est pas dans le dépôt**, il faut le fournir séparément
- Un fichier `steam_appid.txt` à côté de l'exécutable si Steam n'est pas déjà informé de l'AppID

### Étapes

1. Ouvrir `OpenGLProject.sln` (ou le `.vcxproj`) dans Visual Studio.
2. Choisir `Debug` ou `Release`, plateforme `x64` (recommandé).
3. Compiler (`Ctrl+Shift+B`). Standard **C++20** (`stdcpp20`, configuration x64).
4. L'exécutable et les DLL dépendantes sont copiés dans `$(OutDir)`.

### Points de compilation à connaître
- `_CRT_SECURE_NO_WARNINGS` est défini globalement ; `/utf-8` est passé au compilateur.
- `LIBCMT`/`LIBCPMT` sont ignorées à l'édition de liens (conflit de runtime avec les libs tierces).
- **`win_compat.h` doit être inclus en premier** dans toute unité touchant au rendu qui inclut `<windows.h>` (collision `APIENTRY` entre `glad.h` et `minwindef.h`).
- Toute ressource ajoutée doit être **ajoutée à git dans la même session** (`.gitignore` ignore tous les `*.obj` sauf exceptions) — sinon le jeu marche chez le dev mais pas chez les autres (cf. [`agent.md`](agent.md)).

---

## Lancement et arguments

```
OpenGLProject.exe [options]
```

| Argument | Effet |
|---|---|
| *(aucun)* | Tentative d'init Steamworks (avec relance du client Steam si absent) |
| `-offline` / `--offline` / `-nosteam` / `--nosteam` | Steam entièrement ignoré ; manette en XInput/GLFW pur |
| `+connect_lobby <SteamID>` | Rejoint un lobby Steam au démarrage (invitation reçue jeu fermé) — ajouté par Steam |
| `-day` / `-night` / `-batiment` | Charge directement la carte correspondante (debug), sans passer par le lobby |
| `-selftest` | Lance les auto-tests de logique pure (sans fenêtre), code retour 0 = OK |
| `-dumprig <entrée> <sortie.json>` | Outil de contenu : exporte la hiérarchie de squelette Assimp en JSON |

Un raccourci `OpenGLProject - Release - Offline.lnk` est fourni pour le mode hors-ligne.

---

## Contrôles

### Clavier & souris (par défaut, rebindables dans Options → Clavier & Souris)

| Action | Touche |
|---|---|
| Avancer / Reculer / Gauche / Droite | `Z`/`W` `S` `Q`/`A` `D` *(selon `GLFW_KEY_*`)* |
| Sauter (jeu) / Voler vers le haut (no-clip) | `Espace` |
| S'accroupir (jeu) / Voler vers le bas (no-clip) | `Ctrl gauche` |
| Sprinter (maintenu) | `Maj gauche` |
| Lampe torche | `T` |
| Vue 1ère / 3ème personne | `C` |
| Pousser (bras 1P) | `R` |
| **Interagir** (portes, boutons du lobby, postes de quête, attraper) | `E` |
| No-clip (vol libre) | `N` |
| Menu / Pause (et retour dans les menus) | `Échap` |
| **Chat textuel** (ouvrir / envoyer) | `Entrée` |
| **Push-to-talk** (chat vocal, si mode PTT) | `V` |
| HUD debug animations (3P uniquement) | `F3` |
| Tirer (animation bras) | Clic gauche |

### Manette (mapping Xbox, rebindable dans Options → Manette)

| Action | Bouton |
|---|---|
| Déplacement / Regard | Stick gauche / Stick droit |
| Sprint | Gâchette droite (RT), seuil configurable |
| Sauter / S'accroupir | `A` / `B` (maintenu) |
| Pousser / Interagir | `X` / `RB` |
| Vue 1P/3P / Lampe torche | `Y` / `LB` |
| No-clip | `Back` |
| Menu / Reprendre | `Start` |
| Retour (dans les menus) | `B` |
| Navigation menus | Stick gauche ou croix + `A` pour valider |

### Mini-jeux (quêtes)
Flèches / `ZQSD` + `Entrée` / `Espace` au clavier ; croix directionnelle + `A` à la manette.

> Le clavier/souris et la manette restent **tous deux actifs en permanence** ; seule la "source active" affichée par la notification à l'écran bascule selon la dernière entrée détectée.

---

## Fichiers de configuration persistants

### `res/keys.json`
```json
{
  "bindings": { "Forward": 87, "Jump": 32, "Grab": 69, "...": "..." },
  "controller_bindings": { "Jump": 0, "...": "..." },
  "mouse_sensitivity": 0.05,
  "controller_sensitivity": 3.0
}
```
Généré avec les valeurs par défaut de `configKeys.h` si absent ; réécrit à chaque rebinding ou changement de sensibilité.

### `res/options.json`
```json
{
  "muted": false,
  "volume": 0.2,
  "musicVolume": 0.5,
  "voice": {
    "enabled": true,
    "micMuted": false,
    "voiceActivation": true,
    "sensitivity": 0.15,
    "volume": 1.0,
    "device": ""
  }
}
```
Volumes (maître + musique indépendants) et réglages du chat vocal ; réécrit à la sortie des menus Options/Audio.

---

## Cartes JSON (MapLoader)

Une carte est un fichier JSON décrivant la géométrie et l'ambiance (unités en mètres, Y vers le haut). Schéma résumé :

- `"includes"` : instancie d'autres cartes (ex. N fois la même maison) avec position/yaw ; récursif, détection de cycles.
- `"boxes"` : pavés droits (sols, murs, linteaux) avec rotation Euler, matériaux (`brickwall`, `container`, `parquet`, `leaf`, `grey_roof`, `riet`), UV projetés en repère monde.
- `"roof"` : toit à deux pentes avec pignons, collision `"mesh"` (triangles) possible.
- `"models"` : modèles **OBJ** (export Blender) avec matériaux `.mtl` (textures, couleurs, opacité/vitres) ; collision précise par cellules de 4 m.
- `"trees"` / `"leaves"` : arbres (mesh partagé, collision triangles) et feuilles mortes au sol générées depuis une **seed** (reproductible).
- `"doors"` : portes interactives (charnière, yaw, dimensions) — cf. `Door`.
- `"tvs"` : écrans de télévision (easter egg DVD) — cf. `TvScreen`.
- `"buttons"` : fropys tournants interactifs (`name`, `label`, `center`, `scale`) : options du lobby et postes de quête (`quest:<type>:<id>`).
- `"lights"` / `"rooms"` : lumières ponctuelles, optionnellement restreintes à une pièce (AABB nommée).
- `"spawns"` : points d'apparition nommés (`player`, `creature`, ...).
- `"sun"` : direction et couleurs (ambiante/diffuse/speculaire) de la lumière directionnelle — **le réglage d'ambiance par carte**.
- `"fog"` : couleur et densité du brouillard (0 = aucun).
- `"skybox"` : dossier du cubemap ("" = pas de ciel).
- `"sunHead"` : tête-fropy céleste (couleur jaune le jour / blanc-gris la nuit, distance, échelle, intensité du spot).
- `"creature"` : activation (`false` pour le lobby), portée de vue, zone de rondes (`patrol`).

Les collisions statiques sont enregistrées dans le `CollisionManager` et le **BVH est construit à la fin du chargement**. `Scene::loadMap` vide les collisions et lumières de la carte précédente avant de charger la nouvelle.

---

## Animation squelettique

- Format d'os : `BoneInfo { int id; glm::mat4 offsetMatrix; }`, map nom → info par modèle.
- Animations embarquées **ou externes** (`Model::loadExternalAnimations`, clips Mixamo du personnage 3P : idle, walking, running, jump, strafes, turns, falling...).
- `Animator::computeBoneTransform` : transformées globales récursives, crossfade sur les transformées locales, propagation des offsets manuels de bones (élévation des bras 1P).
- Garde-fous anti-NaN à plusieurs niveaux (quaternions dégénérés, temps non finis...) pour éviter qu'une keyframe corrompue ne rende un personnage invisible.

---

## Collisions et physique

- `Constants::Physics::GRAVITY = -9.81f` ; saut configurable (`PLAYER_JUMP_VELOCITY`).
- Joueur modélisé en **capsule** (sphères pieds/milieu/tête) pour marches et pentes.
- `sweepSphere` : sous-étapes (`radius * 0.4` max), glissement le long des normales, anti-rebond dans les coins.
- Statiques via BVH O(log n) ; dynamiques (portes, objets mobiles) en O(m) linéaire.
- Collisions **triangles** (grille de cellules) pour les modèles OBJ et les toits : la hitbox suit la géométrie réelle.
- Ralenti post-chute/atterrissage configurable ; détection sol/plafond indépendante du mouvement.

---

## Audio

- Modèle de distance OpenAL : `AL_INVERSE_DISTANCE_CLAMPED` par défaut.
- `SoundManager::load` met en cache par nom ; **charger les sons lourds pendant l'écran de chargement** plutôt qu'à la volée.
- Réverbération globale (`applyReverbToAll`) : `UNDERWATER` dans les menus/pause, `NONE` en jeu.
- Musique de fond : `MusicStream` (Ogg Vorbis en streaming, volume indépendant `musicVolume`).
- Voix distantes : source OpenAL par joueur, `AL_SOURCE_RELATIVE` (non affectées par l'atténuation 3D) ; voicelines de fropy au contraire spatialisées avec occlusion passe-bas.

---

## Écran de chargement et threading

- `ModelLoader::load()` sur thread séparé avec **contexte OpenGL partagé** (chargement pendant l'écran de chargement).
- Les **VAO ne sont pas partagés** entre contextes : `reloadGPUResources()` est rappelé dans le contexte principal à la fin du chargement.
- Repli synchrone automatique si la création du contexte partagé échoue.
- Le chat vocal est traité sur le **thread principal** par blocs de 20 ms (aucun thread dédié).

---

## Outils et auto-tests

- **`OpenGLProject.exe -selftest`** : tests de logique pure sans fenêtre (collisions, grille de triangles, A* de fropy, options de partie `MatchSettings`, mini-jeux). Code retour 0 = tout passe. **À relancer après toute modification de `CollisionManager`, `Creature`, `MiniGames` ou `MatchSettings`.**
- **`-dumprig <entrée> <sortie.json>`** : exporte la hiérarchie de squelette lue par Assimp en JSON (transformations locales de repos), utilisé par `res/models/fropy/make_fropy_player.py`.
- **Scripts Python de contenu** : `res/models/fropy/make_fropy_player.py` (assemblage du modèle articulé de fropy), `res/sounds/fropy/make_voicelines.py` (voicelines bitcrushées), `res/textures/quests/make_quest_ui.py` (visuels du HUD des quêtes).

---

## Points d'attention / pièges connus

- **Steam Input est désactivé** (`kUseSteamInput = false` dans `InputManager.cpp`) : avec l'AppID placeholder 480, la configuration Steam Input n'est jamais chargée par le client Steam, ce qui laissait la manette inutilisable. Le jeu utilise XInput (GLFW) même quand Steam est initialisé. Réactiver ce flag nécessite un véritable AppID avec une configuration Steam Input associée.
- **`win_compat.h` en tout premier `#include`** dans les fichiers de rendu touchant à `<windows.h>`, sinon `C4005` (redéfinition de `APIENTRY`).
- Le layout de `Vertex` est verrouillé par `static_assert` (76 octets) : toute modification doit être répercutée avec soin CPU/GPU. `m_boneIDs`/`m_weights` sont publics par exception (contrainte de layout, documenté dans `Vertex.h`).
- **Animations Assimp** : ne pas modifier "à l'intuition" le wrapper `_$AssimpFbx$_PreRotation` ni `repairRotationKeys` (symptômes : jambes en l'air, bras à 90°). Vérifier avec le log `[DIAG POSE]` en Debug **et** Release (`AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS=false` doit rester).
- **Encodage** : aucun accent ni caractère hors ASCII dans les `printf`/`renderText()`/textes in-game (console Windows non-UTF-8) ; les commentaires restent en français accentué.
- Bugs connus suivis dans [`todo.md`](todo.md) : tête qui avance dans un mur en marchant contre lui, keyframes manquantes au strafe droit, premier son rejoué en boucle dans le chat vocal, saisie du chat textuel parfois bloquée, caméra 1P qui rentre dans le modèle après un no-clip, modèles des autres joueurs invisibles si l'hôte quitte puis revient, clics à travers les listes déroulantes ouvertes.
- Le dossier `dependencies/` (Steamworks SDK, DLL tierces) **n'est pas inclus** dans le dépôt.

---

## Pistes d'amélioration

*(extrait de [`todo.md`](todo.md))*

- Finaliser le multijoueur (corrections de synchro, chat par session, robustesse lobby).
- **Map Editor** pour créer des cartes sans écrire le JSON à la main.
- PNJ avec dialogues (sous-titres / bulles).
- Personnalisation visuelle des personnages (torche, sac à dos, holster & pistolet...).
- Normal mapping étendu (cf. [LearnOpenGL – Normal Mapping](https://learnopengl.com/Advanced-Lighting/Normal-Mapping)).
- Hitboxes plus fidèles aux modèles 3D (notamment en 3P).
- Skybox libre de droit ; DA : sons bitcrushés, modèles low-poly.
- Support de **modding** pour la communauté.
- Ménage dans l'organisation du `.vcxproj` ; portage multi-plateforme (actuellement fortement couplé à Win32/XInput/Winsock).

---

## Licence

Copyright (c) 2026 Elliot Defurne - Tous droits réservés.

Voir le fichier [`LICENSE`](LICENSE).
