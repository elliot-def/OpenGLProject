## Todo :
Possibilité de modder pour la commu

Implémentation futures :
	- Skin model avec stuff sur eux
	- Map Editor
	- Texture avec du relief comme le mur dans le tuto learnopenGL

Comment enlever le cmd ?

PlantUML depuis freebuff

DA : 
Sons bitcrushed
Low-poly les modèles

## Performance

**1. Les bone matrices sont envoyées une par une alors qu'un envoi groupé existe déjà**  
Dans `FirstPersonArms::draw()` et `ModelEntity::draw()`, tu boucles sur les ~128 bones et appelles `shader->setMat4(m_boneUniformNames[i].c_str(), boneMats[i])` pour chacune. Or `Shader::setMat4Array()` existe déjà et fait un seul `glUniformMatrix4fv(location, count, ...)`. Il suffit de déclarer `uBoneMatrices` comme tableau côté shader et de récupérer une seule fois la location de `uBoneMatrices[0]`, puis d'appeler `setMat4Array` avec tout le tableau contigu. Tu passes de ~128 appels GL + 128 hachages de string à 1 seul appel, par entité skinnée, par frame.

**2. Le cache d'uniforms de `Shader` reste un hachage de string à chaque appel**  
`getUniformLocation` cache bien la `GLint` dans une `unordered_map<string,int>`, mais chaque `setVec3("spotLight.position", ...)` reconstruit potentiellement une string temporaire et refait un hash + une comparaison, à chaque frame, pour chaque lumière (`LightManager::applyToShader`, `Spotlight::applyToShader`). `LightManager` précalcule déjà les noms d'uniforms en dur — l'étape logique suivante est de précalculer aussi les `GLint` (une fois par shader, au premier usage) et de les stocker directement, pour bypasser complètement le hash map côté hot path.

**3. Rendu de texte non batché (`TextRenderer::renderText`)**  
Chaque caractère fait un `glBindTexture` + `glBufferSubData` + `glDrawArrays` séparé, et `glGetUniformLocation("projection"/"textColor")` est refait à chaque appel de `renderText` (pas caché). Le HUD debug dans `Game::draw()` peut lister jusqu'à 32 lignes d'animations _chaque frame_ en mode 3e personne : ça fait potentiellement des centaines de draw calls rien que pour du texte. Deux gains possibles : cacher les deux uniform locations dans le constructeur de `TextRenderer`, et à terme batcher tous les glyphes d'une frame dans un seul VBO/draw call (atlas de police).

**4. Recalcul trigonométrique redondant dans `Direction`**  
`getDirectionVector()` refait `cos`/`sin`/`radians` à chaque appel, et `rotateRight90KeepY()` / `getDirectionVectorKeepY()` rappellent chacun `getDirectionVector()` en interne. Dans `Player::processDirectionKey`, appelé pour chaque touche de direction active, le vecteur peut être recalculé plusieurs fois dans la même frame. Un simple flag "dirty" (recalcul uniquement quand yaw/pitch change) éliminerait ces recalculs.

**5. `ModelEntity::getModelMatrix()` reconstruit la matrice à chaque appel**  
Elle est appelée dans `draw()`, `drawDebug()`, `checkCollision()`, `getWorldBoundingBox()`, et dans `Game::update()` pour `updateDynamic(...)` — soit 3-4 fois par frame minimum, avec `translate`+`rotate`+multiplication à chaque fois. Un cache "calculé une fois par frame" (invalidé quand position/direction/spin changent) évite ce travail redondant.

**6. Exceptions et `std::cout` dans le chemin de rendu**  
`Cube::drawLightSourceShader()` / `drawSeveralLightShader()` font `throw std::invalid_argument(...)` en plein `draw()` (avec un `return;` mort juste après, signe que la logique n'est pas finalisée). Et la branche `else` de `Cube::draw()` fait un `std::cout` à chaque frame si le type de shader n'est pas reconnu. Les exceptions/`iostream` sont coûteuses et n'ont rien à faire dans une boucle appelée 60+ fois par seconde — remplace par un `assert`/log une seule fois, ou une gestion silencieuse.

**7. Format de `Vertex` universel et surdimensionné**  
`Vertex` fait 76 octets (position + normale + couleur + UV + 4 bone IDs + 4 poids), et `Mesh` utilise toujours `sizeof(Vertex)` comme stride, même pour un `Cube` qui n'active que POSITION+COLOR ou un `Triangle` 2D. Tu uploades et bind donc 3x plus de données que nécessaire par sommet pour tout ce qui n'est pas skinné. Une piste : des structs de vertex plus légers par cas d'usage (2D, statique 3D, skinné), ou au minimum ne pas envoyer les champs bone/poids quand `SKINNING` n'est pas dans le masque (ce que `SharedQuad`, qui bypass carrément `Vertex`, fait déjà bien).

### Organisation / propreté

**1. Gestion mémoire hétérogène**  
Le projet mélange `std::unique_ptr` (dans `Game`, `ModelEntity`) et `new`/`delete` bruts partout ailleurs (`Cube`, `LightSource`, `Mesh`, `Menu::m_shapes`/`m_ranges`/`m_checkboxes`/`m_selects`, `Model::m_meshes`). Chaque conteneur de pointeurs bruts a son propre rituel de nettoyage manuel (`Menu::clear()`, destructeurs de `MenuRange`/`MenuCheckbox`/`MenuSelect`...). Standardiser sur `unique_ptr`/`vector<unique_ptr<T>>` réduirait le risque de fuite et simplifierait beaucoup ces classes.

**2. Duplication du "outline pass"**  
Le bloc de rendu d'outline (silhouette) est copié-collé quasi identique dans `Rectangle::draw()`, `Triangle::draw()`, `Image::draw()`, `MaskImage::draw()`, `Cube::draw()` et `ModelEntity::draw()`. C'est le genre de duplication qui, à chaque futur ajustement (couleur, épaisseur, ordre des passes), doit être répété 6 fois avec risque d'oubli. Un helper commun dans `Outlineable` ou une fonction libre `drawOutlinePass(shader, mesh/quad, transform, color, thickness)` centraliserait ça.

**3. Ownership ambigu de `Direction` dans `Entity`/`Spotlight`**  
`Entity::m_direction` est parfois possédé (`m_ownsDirection = true`), parfois juste un pointeur emprunté (`Spotlight::update(Player*)` fait `m_direction = player->getDirection()`). Ce flag booléen "possède ou pas" est fragile : si `Player` est détruit avant `Spotlight`, pointeur pendouillant. Un pointeur observateur explicite (ou `shared_ptr`/référence) rendrait l'intention plus claire et plus sûre.

**4. Includes lourds dans des headers très utilisés**  
`Camera.h` inclut `Renderer.h` et `Entity.h` en entier (au lieu de forward-declare), `Player.h` inclut `<stdio.h>`, `<iostream>` au niveau du header. Comme ces headers sont inclus très largement, ça gonfle le temps de compilation à chaque modification de `Renderer`/`Entity`. Vaut le coup de repasser en forward declarations partout où c'est possible et de déplacer les includes lourds dans les `.cpp`.

**5. `Game` fait trop de choses**  
`Game::update()`/`draw()` mélangent polling réseau, input, physique, son, machine à états d'animation de l'humain 3P (avec des variables `static` locales cachant de l'état — `lastHumanPos`, `punching`, `restTimer`...), et un bloc HUD de debug entier inline dans `draw()`. Extraire la logique d'animation du personnage 3P dans une méthode dédiée (voire une petite classe `CharacterAnimationController`) et le HUD debug dans son propre module rendrait `Game` beaucoup plus lisible.

**6. `Model.cpp` mélange deux responsabilités très différentes**  
Le parsing bas niveau du GLB (`readGlb`, `readFloatAccessor`, `patchGlbAnimationRotations`) et la classe `Model` elle-même cohabitent dans un seul fichier de plus de 700 lignes. Séparer le patch GLB dans son propre fichier (`GlbAnimationRepair.h/.cpp`) rendrait les deux parties plus faciles à tester et à faire évoluer indépendamment.

**7. Logging incohérent**  
`printf`, `std::cout`, `std::cerr` sont mélangés partout sans niveaux ni moyen de les couper en release (`ImageLoader`, `Model`, `SoundManager`, `Animator`...). Un petit système de log avec macros (`LOG_INFO`/`LOG_WARN`/`LOG_ERROR`, désactivables par `#ifdef _DEBUG`) éviterait le coût des `iostream` en build release et uniformiserait le style.

### Si tu ne devais garder que 3 priorités

1. Batcher les bone matrices via `setMat4Array` (gain direct et simple, la méthode existe déjà).
2. Retirer les exceptions/`cout` du chemin de rendu de `Cube`.
3. Choisir une stratégie de gestion mémoire unique (probablement `unique_ptr` partout) pour `Menu` et les entités de scène — ça élimine une bonne partie de la duplication de destructeurs et des risques de fuite.
