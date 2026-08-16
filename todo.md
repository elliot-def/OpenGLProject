## Todo :

Features : 
- Skin model avec stuff sur eux (torche, sac à dos, holter & pistolet ...)
- Texture avec du relief comme le mur de briques dans le tuto learnopenGL (https://learnopengl.com/Advanced-Lighting/Normal-Mapping)
- Map Editor
- NPC avec dialogue
	- Sous-titres en bas de l'écran / Ajouter des bulles de dialogues ?
- PlantUML depuis freebuff
- Multijoueur

Changements et Debug : 
- Rendre les hitbox plus fidèles au modèles 3D (en troisième personne pour les joueurs) 
- Trouver une skybox libre de droit
- Faire le ménage dans l'organisation du projet .vsxproj
- DA : 
	- Sons bitcrushed
	- Low-poly les modèles
 
Fix :
- Si le chef du lobby quitte le lobby puis revient, il ne voit plus les autres players modèles
- Le premier son est rejouer à l'infini dans le chat vocal
- On ne peut pas taper dans le chat textuel
- Le bug de la tete qui avance quand  on avance droit dans un mur (malgrès la hitbox)
- Select (attention au clic sur les boutons derrière les selects) + ajouter un fond derrière les options du select avec un couleur différente pour le hover
- Chat propre à la session (reset quand on en recréer une ou passe en solo)
- Fix la caméra en première personne qui rentre dans le player model quand il est en no clip (ne reviens pas à la normal quand on s'enleve du no clip)- Position des yeux en première personne
- Le rectangle blanc derrière le chat n'est visible qu'en première personne
- Changer le nom des micro dans le select (enlevé openal machin truc)
- Il manque certaines keyframes quand je strafe à droite

Opti :
- Regarder les logs de chargement de texture :
	+ Fichier de propriÚtÚs manquant pour container: ./res/textures/container\container.json
    + Loading texture from: ./res/textures/container\container.png
	+ Texture chargÚe: container (shininess: 32, specular: oui), shininess file: non trouvÚ
	+ Fichier de propriÚtÚs manquant pour glass: ./res/textures/glass\glass.json
	+ Texture principale manquante: ./res/textures/glass\glass.png
	+ Fichier de propriÚtÚs manquant pour menu: ./res/textures/menu\menu.json
	+ Texture principale manquante: ./res/textures/menu\menu.png

Recherches à faire : 
- Possibilité de modder pour la commu
- Comment enlever le cmd lors de l'execution de l'exe
- Regarder cette vidéo et potentiellement changer les bras en première personne pour mettre ceux des modèles mixamo.
























