## Todo :

Features : 
- Skin model avec stuff sur eux (torche, sac à dos, holter & pistolet ...)
- Texture avec du relief comme le mur de briques dans le tuto learnopenGL (https://learnopengl.com/Advanced-Lighting/Normal-Mapping)
- Map Editor
- NPC avec dialogue
- Sous-titres en bas de l'écran / Ajouter des bulles de dialogues ?
- PlantUML depuis freebuff

Changements et Debug : 
- Easter Egg DVD derrière le texte (devrait être devant)
- Rendre les hitbox plus fidèles au modèles 3D (en troisième personne pour les joueurs) 
- Nettoyer les caractères non-ascii dans les printf.
- Trouver une skybox libre de droit
- DA : 
	- Sons bitcrushed
	- Low-poly les modèles
 - 
Fix :
- Position des yeux en première personne
- Temps de chargement après lancement du jeu pour la première fois (quand steam se lance)
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
























