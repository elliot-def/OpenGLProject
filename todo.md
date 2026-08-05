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

## Organisation / propreté

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
