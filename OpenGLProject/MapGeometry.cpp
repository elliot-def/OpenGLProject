#include "MapGeometry.h"

#include <cmath>
#include <vector>

#include "Mesh.h"
#include "Vertex.h"

#include <glm/gtc/matrix_transform.hpp>

// ── Génération de géométrie (UV en mètres × kUvScale) ───────────────────────
// kUvScale < 1 agrandit les textures : 0.4 = une répétition tous les 2.5 m
// (au lieu de 1 m, motif de répétition trop visible).


static void pushQuad(std::vector<Vertex>& vertices, std::vector<unsigned int>& indices,
                     const glm::vec3& v0, const glm::vec3& v1,
                     const glm::vec3& v2, const glm::vec3& v3,
                     float uSpan, float vSpan) {
    // Normale de face depuis le winding (v0,v1,v2) — faces CCW vues de l'extérieur.
    const glm::vec3 normal = glm::normalize(glm::cross(v1 - v0, v2 - v0));
    const unsigned int base = static_cast<unsigned int>(vertices.size());

    vertices.push_back(Vertex(v0.x, v0.y, v0.z, normal.x, normal.y, normal.z, 0.0f, 0.0f));
    vertices.push_back(Vertex(v1.x, v1.y, v1.z, normal.x, normal.y, normal.z, uSpan * kUvScale, 0.0f));
    vertices.push_back(Vertex(v2.x, v2.y, v2.z, normal.x, normal.y, normal.z, uSpan * kUvScale, vSpan * kUvScale));
    vertices.push_back(Vertex(v3.x, v3.y, v3.z, normal.x, normal.y, normal.z, 0.0f, vSpan * kUvScale));

    indices.insert(indices.end(), { base, base + 1, base + 2,
                                    base + 2, base + 3, base });
}

static void pushTriangle(std::vector<Vertex>& vertices, std::vector<unsigned int>& indices,
                         const glm::vec3& v0, const glm::vec2& uv0,
                         const glm::vec3& v1, const glm::vec2& uv1,
                         const glm::vec3& v2, const glm::vec2& uv2) {
    const glm::vec3 normal = glm::normalize(glm::cross(v1 - v0, v2 - v0));
    const unsigned int base = static_cast<unsigned int>(vertices.size());

    vertices.push_back(Vertex(v0.x, v0.y, v0.z, normal.x, normal.y, normal.z, uv0.x * kUvScale, uv0.y * kUvScale));
    vertices.push_back(Vertex(v1.x, v1.y, v1.z, normal.x, normal.y, normal.z, uv1.x * kUvScale, uv1.y * kUvScale));
    vertices.push_back(Vertex(v2.x, v2.y, v2.z, normal.x, normal.y, normal.z, uv2.x * kUvScale, uv2.y * kUvScale));

    indices.insert(indices.end(), { base, base + 1, base + 2 });
}

// Pavé droit centré sur l'origine, taille réelle (pas de mise à l'échelle
// dans la matrice modèle). 6 faces × 4 sommets, UV en mètres.
std::unique_ptr<Mesh> buildBoxMesh(const glm::vec3& size, const glm::mat4& model) {
    const glm::vec3 h = size * 0.5f;

    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    vertices.reserve(24);
    indices.reserve(36);

    pushQuad(vertices, indices,
             {-h.x, -h.y,  h.z}, { h.x, -h.y,  h.z}, { h.x,  h.y,  h.z}, {-h.x,  h.y,  h.z},
             size.x, size.y);  // +Z
    pushQuad(vertices, indices,
             {-h.x,  h.y, -h.z}, { h.x,  h.y, -h.z}, { h.x, -h.y, -h.z}, {-h.x, -h.y, -h.z},
             size.x, size.y);  // -Z
    pushQuad(vertices, indices,
             { h.x, -h.y,  h.z}, { h.x, -h.y, -h.z}, { h.x,  h.y, -h.z}, { h.x,  h.y,  h.z},
             size.z, size.y);  // +X
    pushQuad(vertices, indices,
             {-h.x, -h.y, -h.z}, {-h.x, -h.y,  h.z}, {-h.x,  h.y,  h.z}, {-h.x,  h.y, -h.z},
             size.z, size.y);  // -X
    pushQuad(vertices, indices,
             {-h.x,  h.y,  h.z}, { h.x,  h.y,  h.z}, { h.x,  h.y, -h.z}, {-h.x,  h.y, -h.z},
             size.x, size.z);  // +Y (dessus)
    pushQuad(vertices, indices,
             {-h.x, -h.y, -h.z}, { h.x, -h.y, -h.z}, { h.x, -h.y,  h.z}, {-h.x, -h.y,  h.z},
             size.x, size.z);  // -Y (dessous)

    // UV projetés en repère monde (selon la normale dominante de la face) :
    // deux faces coplanaires de boîtes différentes deviennent texel-identiques
    // → pas de z-fighting visible aux jonctions (le culling est désactivé), et
    // la texture est continue d'une boîte à l'autre sur toute la carte.
    // ATTENTION : la normale doit être TRANSFORMÉE par la matrice modèle avant
    // de choisir les axes UV. Avec la normale locale, une boîte tournée (les
    // maisons instanciées avec "yaw" dans village.json) choisit les mauvais
    // axes : un axe UV devient constant sur la face → la texture s'écrase sur
    // une bande de 1 texel (les murs en briques "disparaissent").
    for (auto& v : vertices) {
        const glm::vec3 world = glm::vec3(model * glm::vec4(v.getPositions(), 1.0f));
        const glm::vec3 n = glm::abs(glm::mat3(model) * v.getNormals());
        glm::vec2 uv;
        if (n.x >= n.y && n.x >= n.z) uv = glm::vec2(world.z, world.y);
        else if (n.y >= n.z)          uv = glm::vec2(world.x, world.z);
        else                          uv = glm::vec2(world.x, world.y);
        v = Vertex(v.getPositions(), v.getNormals(), v.getColor(), uv * kUvScale);
    }

    return std::make_unique<Mesh>(vertices, indices,
        (unsigned int)VertexAttribute::POSITION |
        (unsigned int)VertexAttribute::NORMAL |
        (unsigned int)VertexAttribute::TEXCOORD);
}

// Toit à deux pentes = 2 dalles inclinées jointives au faîtage, chacune avec
// une épaisseur `thickness` (verticale) : visu « couverture » au lieu du
// prisme plein. Le pignon est porté par buildGablesMesh.
//
// Par dalle (côté s, s=-1 ou +1, hauteur h) :
//   dessus  (s·hx, 0) → (0, h)
//   dessous (s·hx, -t) → (0, h-t)  (décalage vertical)
//   tranche d'égout verticale à x=s·hx entre y=0 et y=-t
//   2 tranches d'extrémité (caps) aux bouts z=±hz : la coupe de la dalle,
//   parallélogramme (s·hx,0)-(0,h)-(0,h-t)-(s·hx,-t) → la bordure de la
//   couverture reste TEXTURÉE et garde toute son épaisseur t jusqu'au faîtage
//   (sans ces caps, l'extrémité est ouverte : la dalle dépasse du pignon d'un
//   lambeau dont l'épaisseur visible tombe à 0 au faîtage, et ce bord n'a
//   aucune face → pas de texture).
// Les 2 dessus partagent l'arête du faîtage, les 2 dessous celle en (0,h-t) :
// le volume est fermé, aucune face coplanaire entre les 2 dalles.
std::unique_ptr<Mesh> buildRoofMesh(const glm::vec2& span, float height,
                                           float thickness) {
    const float hx = span.x * 0.5f;
    const float hz = span.y * 0.5f;
    const float t  = thickness;

    const glm::vec3 A(-hx, 0.0f, -hz);   // coins de la base (y=0)
    const glm::vec3 B( hx, 0.0f, -hz);
    const glm::vec3 C( hx, 0.0f,  hz);
    const glm::vec3 D(-hx, 0.0f,  hz);
    const glm::vec3 R0(0.0f, height, -hz);   // extrémités du faîtage
    const glm::vec3 R1(0.0f, height,  hz);

    // Notation X2 = point X décalé vers le bas de l'épaisseur.
    const glm::vec3 down(0.0f, -t, 0.0f);
    const glm::vec3 A2 = A + down, B2 = B + down, C2 = C + down, D2 = D + down;
    const glm::vec3 R02 = R0 + down, R12 = R1 + down;

    const float slopeLen = std::sqrt(hx * hx + height * height);

    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    vertices.reserve(40);   // 2 dalles × (8 sommets dessus/dessous + 4 égout
                            // + 8 sommets pour les 2 caps d'extrémité)
    indices.reserve(60);

    // Dalle -X
    pushQuad(vertices, indices, A, D, R1, R0, span.y, slopeLen);       // dessus
    pushQuad(vertices, indices, A2, R02, R12, D2, span.y, slopeLen);   // dessous
    pushQuad(vertices, indices, A, A2, D2, D, t, span.y);              // égout (-X)
    pushQuad(vertices, indices, A, R0, R02, A2, slopeLen, t);          // cap z=-hz
    pushQuad(vertices, indices, D, D2, R12, R1, slopeLen, t);          // cap z=+hz

    // Dalle +X
    pushQuad(vertices, indices, R1, C, B, R0, span.y, slopeLen);       // dessus
    pushQuad(vertices, indices, R02, B2, C2, R12, span.y, slopeLen);   // dessous
    pushQuad(vertices, indices, C, C2, B2, B, t, span.y);              // égout (+X)
    pushQuad(vertices, indices, B, B2, R02, R0, slopeLen, t);          // cap z=-hz
    pushQuad(vertices, indices, C, R1, R12, C2, slopeLen, t);          // cap z=+hz

    return std::make_unique<Mesh>(vertices, indices,
        (unsigned int)VertexAttribute::POSITION |
        (unsigned int)VertexAttribute::NORMAL |
        (unsigned int)VertexAttribute::TEXCOORD);
}

// Pignons : 2 triangles verticaux aux extrémités du faîtage (la façade
// continue les murs jusqu'au toit). Le triangle remplit exactement la zone
// SOUS le dessous des 2 dalles de couverture : base en y=-thickness, sommet
// en (0, height-thickness) — les arêtes suivent le dessous des dalles
// ((-hx,-t) → (0,h-t) → (hx,-t)). Les caps d'extrémité des dalles (cf.
// buildRoofMesh) couvrent au-dessus : chaque face est dans son plan, aucune
// coplanaire, et la coupe de la couverture reste visible et texturée.
// UV = coordonnées locales en mètres (u = x + hx, v = y + thickness).
std::unique_ptr<Mesh> buildGablesMesh(const glm::vec2& span, float height,
                                             float thickness, float inset, float baseLocalY,
                                             float wallThickness, const glm::mat4& model) {
    const float hx = span.x * 0.5f;
    // Le pignon est rentré de `inset` par rapport au bout du toit (le plan des murs
    // avant/arrière est en retrait du débord de toiture) et sa base est relevée en
    // baseLocalY (haut des murs) : sa demi-largeur est alors celle du dessous des
    // dalles à cette hauteur, pas celle de l'égout. Par défaut (inset 0, base
    // -thickness) : comportement d'origine.
    const float hz = span.y * 0.5f - inset;
    const float baseY = baseLocalY;
    const float halfW = hx * (1.0f - (baseY + thickness) / height);

    const glm::vec3 A(-halfW, baseY, -hz);   // base au niveau du haut des murs
    const glm::vec3 B( halfW, baseY, -hz);
    const glm::vec3 C( halfW, baseY,  hz);
    const glm::vec3 D(-halfW, baseY,  hz);
    // Sommet sous le faîtage (dessous des dalles), pas au faîtage lui-même :
    // au-dessus, ce sont les caps des dalles (matériau couverture) qui
    // ferment, avec l'épaisseur t visible jusqu'en haut.
    const glm::vec3 R0(0.0f, height - thickness, -hz);
    const glm::vec3 R1(0.0f, height - thickness,  hz);

    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    vertices.reserve(6);
    indices.reserve(6);

    pushTriangle(vertices, indices,
                 D, {0.0f, 0.0f}, C, {span.x, 0.0f},
                 R1, {hx, height});   // +Z
    pushTriangle(vertices, indices,
                 B, {span.x, 0.0f}, A, {0.0f, 0.0f},
                 R0, {hx, height});   // -Z

    // Epaisseur du mur : face INTERIEURE en retrait de wallThickness (meme plan que la
    // face interieure des murs), orientee vers la piece.
    if (wallThickness > 0.0f) {
        const float hzi = hz - wallThickness;
        const glm::vec3 Ai(-halfW, baseY, -hzi), Bi(halfW, baseY, -hzi);
        const glm::vec3 Ci(halfW, baseY, hzi), Di(-halfW, baseY, hzi);
        const glm::vec3 R0i(0.0f, height - thickness, -hzi), R1i(0.0f, height - thickness, hzi);
        pushTriangle(vertices, indices, Ci, {span.x, 0.0f}, Di, {0.0f, 0.0f}, R1i, {hx, height});   // +Z interieur
        pushTriangle(vertices, indices, Ai, {0.0f, 0.0f}, Bi, {span.x, 0.0f}, R0i, {hx, height});    // -Z interieur
    }

    // UV projetés en repère MONDE selon la normale dominante (comme buildBoxMesh) :
    // les briques du pignon prolongent exactement celles des murs (mêmes rangées,
    // même décalage), au lieu d'une texture locale décalée.
    for (auto& v : vertices) {
        const glm::vec3 world = glm::vec3(model * glm::vec4(v.getPositions(), 1.0f));
        const glm::vec3 n = glm::abs(glm::mat3(model) * v.getNormals());
        glm::vec2 uv;
        if (n.x >= n.y && n.x >= n.z) uv = glm::vec2(world.z, world.y);
        else if (n.y >= n.z)          uv = glm::vec2(world.x, world.z);
        else                          uv = glm::vec2(world.x, world.y);
        v = Vertex(v.getPositions(), v.getNormals(), v.getColor(), uv * kUvScale);
    }

    return std::make_unique<Mesh>(vertices, indices,
        (unsigned int)VertexAttribute::POSITION |
        (unsigned int)VertexAttribute::NORMAL |
        (unsigned int)VertexAttribute::TEXCOORD);
}

