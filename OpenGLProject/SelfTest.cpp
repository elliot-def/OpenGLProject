#include "SelfTest.h"

#include <cstdio>
#include <vector>

#include "CollisionManager.h"
#include "Creature.h"
#include "LobbyManager.h"
#include "MiniGames.h"
#include "Door.h"
#include "ObjLoader.h"
#include "constants/player.h"

#include <glm/glm.hpp>
#include <assimp/Importer.hpp>
#include <assimp/config.h>
#include <assimp/scene.h>
#include <glm/gtc/matrix_transform.hpp>

namespace {
    int g_failures = 0;
    void check(bool ok, const char* what) {
        std::printf("[SelfTest] %s : %s\n", ok ? "OK  " : "ECHEC", what);
        if (!ok) ++g_failures;
    }

    // Mur vertical (2 triangles) x in [x0,x1], y in [0,h], plan z = z0.
    std::vector<glm::vec3> wall(float x0, float x1, float z0, float h) {
        const glm::vec3 a(x0, 0, z0), b(x1, 0, z0), c(x1, h, z0), d(x0, h, z0);
        return { a, b, c, a, c, d };
    }
}

int runSelfTests() {
    g_failures = 0;

    // ── Collisions : rayon + sphere (grille de triangles) ───────────────
    CollisionManager cm;
    cm.addStaticTriangleSoup(wall(-3.0f, 3.0f, 0.0f, 3.0f), "mur");
    cm.addStaticTriangleSoup({ { -20, 0, -20 }, { 20, 0, 20 }, { 20, 0, -20 }, { -20, 0, -20 }, { -20, 0, 20 }, { 20, 0, 20 } }, "sol");
    cm.buildBVH();
    check(cm.segmentBlocked({ 0, 1.5f, 5 }, { 0, 1.5f, -5 }), "rayon a travers le mur : bloque");
    check(!cm.segmentBlocked({ 5, 1.5f, 5 }, { 5, 1.5f, -5 }), "rayon a cote du mur : libre");
    check(cm.testSphereStatic({ 0, 1.0f, 0.2f }, 0.35f).hit, "sphere contre le mur : touche");
    check(!cm.testSphereStatic({ 0, 1.0f, 2.0f }, 0.35f).hit, "sphere loin du mur : libre");
    check(!cm.testSphereAll({ 50, 1.0f, 50 }, 0.35f).hit, "sphere hors de toute cellule : libre");

    // ── A* : la creature contourne le mur pour rejoindre la cible ───────
    Creature creature(nullptr, glm::vec3(0, 0, 5));
    const glm::vec3 target(0, 0, -5);
    for (int i = 0; i < 60 * 25; ++i) creature.update(1.0f / 60.0f, target, &cm);
    const glm::vec3 p = creature.getPosition();
    check(glm::length(glm::vec2(p.x - target.x, p.z - target.z)) < 2.0f, "A* : contourne le mur et atteint la cible");

    // ── Escalier en colimacon du batiment : montee automatique ──────────
    {
        CollisionManager stairs;
        const glm::mat4 m = glm::scale(glm::mat4(1.0f), glm::vec3(3.0f, 1.40625f, 3.0f));   // cf. batiment.json
        for (auto& part : loadObjParts("./res/maps/batiment/batiment.obj", m))
            stairs.addStaticTriangleSoup(part.soup, part.name);
        const float groundY = -0.9f;   // dessus de la boite "ground" de batiment.json
        stairs.addStaticTriangleSoup({ { -200, groundY, -300 }, { 300, groundY, 200 }, { 300, groundY, -300 },
                                       { -200, groundY, -300 }, { -200, groundY, 200 }, { 300, groundY, 200 } }, "ground");
        stairs.buildBVH();
        const glm::vec2 c(19.726f * 3.0f, -32.537f * 3.0f);   // axe de l'escalier (Hall_esc)
        const float a0 = 0.0f;                                  // angle de la 1re marche (cote est, prise depuis le couloir)
        const glm::vec2 start = c + 3.0f * glm::vec2(std::cos(a0), std::sin(a0)) - 1.2f * glm::vec2(std::sin(a0), -std::cos(a0));
        const float topY = 6.4f * 1.40625f;                     // plancher du 2e etage
        // A plusieurs FPS : a haut FPS le pas par frame est petit et la montee calait.
        for (int fps : { 60, 144, 240 }) {
            const float dt = 1.0f / fps;
            glm::vec3 pos(start.x, 0.05f, start.y);             // au pied de la 1re marche
            stairs.setVerticalVelocity(0.0f);
            float t = 0.0f, topTime = -1.0f;
            for (; t < 30.0f && (topTime < 0.0f || t < topTime + 4.0f); t += dt) {
                const glm::vec2 d = glm::vec2(pos.x, pos.z) - c;
                glm::vec2 dir(0.0f, 1.0f);                      // en haut : tout droit vers le couloir (nord)
                if (topTime < 0.0f) {
                    const glm::vec2 fwd = glm::normalize(glm::vec2(d.y, -d.x));   // sens de montee
                    dir = fwd + (3.0f - glm::length(d)) * glm::normalize(d);      // reste a r = 3 m
                    if (pos.y > topY - 0.05f) topTime = t;
                }
                const glm::vec3 move = glm::vec3(dir.x, 0.0f, dir.y) * (Constants::Player::PLAYER_WALKING_SPEED * dt);
                pos = stairs.resolvePlayerMovement(pos, t < 0.5f ? glm::vec3(0.0f) : move, dt, true);
                pos = stairs.pushPlayerAway(pos);
            }
            std::printf("[SelfTest]   escalier %d fps : en haut a t=%.1fs, fin pos=(%.2f %.2f %.2f)\n", fps, topTime, pos.x, pos.y, pos.z);
            check(topTime > 0.0f && topTime < 20.0f, "escalier en colimacon : montee automatique sans calage");
            check(std::abs(pos.y - topY) < 0.1f && pos.z > -30.4f * 3.0f, "escalier en colimacon : derniere marche face au couloir");
        }

        // Fropy prend l'escalier : du rez-de-chaussee jusqu'au couloir du 2e etage.
        Creature climber(nullptr, glm::vec3(c.x, 0.05f, -29.5f * 3.0f));   // dans le grand couloir du RDC
        const glm::vec3 corridor(56.25f, topY, -88.4f);
        for (int i = 0; i < 60 * 40; ++i) climber.update(1.0f / 60.0f, corridor, &stairs);
        const glm::vec3 cp = climber.getPosition();
        std::printf("[SelfTest]   fropy fin : pos=(%.2f %.2f %.2f)\n", cp.x, cp.y, cp.z);
        check(std::abs(cp.y - topY) < 0.3f && glm::length(glm::vec2(cp.x - corridor.x, cp.z - corridor.z)) < 2.0f,
              "fropy prend l'escalier jusqu'au 2e etage");

        // Escalier du batiment secondaire (P001 -> P102 -> LT207) puis porte vers le toit-terrasse.
        // Waypoints en coordonnees OBJ (x3 en jeu) : deux volees par etage, palier a l'est.
        const glm::vec2 wps[] = { { 15.3f, -0.95f }, { 15.3f, -2.5f }, { 11.0f, -2.5f },    // RDC -> 1er
                                  { 11.0f, -0.95f }, { 15.3f, -0.95f }, { 15.3f, -2.5f }, { 11.0f, -2.5f },   // 1er -> 2e
                                  { 11.0f, -7.1f }, { 8.0f, -7.1f } };                    // LT207 -> toit
        const float roofY = 6.42f * 1.40625f;
        glm::vec3 pos(11.0f * 3.0f, 0.05f, -0.95f * 3.0f);   // palier P001 (RDC)
        stairs.setVerticalVelocity(0.0f);
        size_t wi = 0;
        for (int i = 0; i < 60 * 60 && wi < std::size(wps); ++i) {
            const glm::vec2 to = wps[wi] * 3.0f - glm::vec2(pos.x, pos.z);
            if (glm::length(to) < 0.4f) { ++wi; continue; }
            const glm::vec2 dir = glm::normalize(to) * (Constants::Player::PLAYER_WALKING_SPEED / 60.0f);
            pos = stairs.resolvePlayerMovement(pos, i < 30 ? glm::vec3(0.0f) : glm::vec3(dir.x, 0.0f, dir.y), 1.0f / 60.0f, true);
            pos = stairs.pushPlayerAway(pos);
        }
        std::printf("[SelfTest]   escalier secondaire : waypoint %zu/%zu, pos=(%.2f %.2f %.2f)\n", wi, std::size(wps), pos.x, pos.y, pos.z);
        check(wi == std::size(wps) && std::abs(pos.y - roofY) < 0.1f, "escalier secondaire : du RDC jusqu'au toit-terrasse");

        Creature roofer(nullptr, glm::vec3(11.0f * 3.0f, 0.05f, -0.95f * 3.0f));
        const glm::vec3 roof(8.0f * 3.0f, roofY, -7.1f * 3.0f);
        for (int i = 0; i < 60 * 60; ++i) roofer.update(1.0f / 60.0f, roof, &stairs);
        const glm::vec3 rp = roofer.getPosition();
        std::printf("[SelfTest]   fropy toit : pos=(%.2f %.2f %.2f)\n", rp.x, rp.y, rp.z);
        check(std::abs(rp.y - roofY) < 0.3f && glm::length(glm::vec2(rp.x - roof.x, rp.z - roof.z)) < 2.0f,
              "fropy prend l'escalier secondaire jusqu'au toit");

        // Escalier exterieur : du sol a la passerelle, puis couloir C103 du 1er (batiment secondaire).
        const glm::vec2 extWps[] = { { 6.5f, -27.6f }, { 6.5f, -25.0f }, { 9.1f, -25.0f }, { 9.1f, -22.0f } };   // passerelle en L
        const float level1Y = 3.22f * 1.40625f;
        pos = glm::vec3(-2.5f * 3.0f, groundY + 0.05f, -27.6f * 3.0f);   // dehors, au pied de l'escalier
        stairs.setVerticalVelocity(0.0f);
        wi = 0;
        for (int i = 0; i < 60 * 40 && wi < std::size(extWps); ++i) {
            const glm::vec2 to = extWps[wi] * 3.0f - glm::vec2(pos.x, pos.z);
            if (glm::length(to) < 0.4f) { ++wi; continue; }
            const glm::vec2 dir = glm::normalize(to) * (Constants::Player::PLAYER_WALKING_SPEED / 60.0f);
            pos = stairs.resolvePlayerMovement(pos, i < 30 ? glm::vec3(0.0f) : glm::vec3(dir.x, 0.0f, dir.y), 1.0f / 60.0f, true);
            pos = stairs.pushPlayerAway(pos);
        }
        std::printf("[SelfTest]   escalier exterieur : waypoint %zu/%zu, pos=(%.2f %.2f %.2f)\n", wi, std::size(extWps), pos.x, pos.y, pos.z);
        check(wi == std::size(extWps) && std::abs(pos.y - level1Y) < 0.1f, "escalier exterieur : du sol a la passerelle et au 1er");

        Creature walker(nullptr, glm::vec3(-3.0f * 3.0f, groundY, -27.6f * 3.0f));
        const glm::vec3 c103(9.1f * 3.0f, level1Y, -22.0f * 3.0f);
        for (int i = 0; i < 60 * 40; ++i) walker.update(1.0f / 60.0f, c103, &stairs);
        const glm::vec3 wp = walker.getPosition();
        std::printf("[SelfTest]   fropy exterieur : pos=(%.2f %.2f %.2f)\n", wp.x, wp.y, wp.z);
        check(std::abs(wp.y - level1Y) < 0.3f && glm::length(glm::vec2(wp.x - c103.x, wp.z - c103.z)) < 2.0f,
              "fropy prend l'escalier exterieur jusqu'a la passerelle");
    }

    // ── Portes : fropy n'ouvre que celles qui barrent sa trajectoire ────
    {
        // Battant ferme de (0,0,0) a (2,0,0) (yaw 0), haut de 2.2 m.
        check(Door::panelBlocksSegment({ 0, 0, 0 }, 0.0f, 2.0f, 2.2f, { 1, 0.05f, -1 }, { 1, 0.05f, 1 }), "porte : trajectoire qui la traverse");
        check(!Door::panelBlocksSegment({ 0, 0, 0 }, 0.0f, 2.0f, 2.2f, { 3, 0.05f, -1 }, { 3, 0.05f, 1 }), "porte : trajectoire a cote");
        check(!Door::panelBlocksSegment({ 0, 0, 0 }, 0.0f, 2.0f, 2.2f, { -1, 0.05f, 0.5f }, { 3, 0.05f, 0.5f }), "porte : couloir le long du battant");
        check(!Door::panelBlocksSegment({ 0, 0, 0 }, 0.0f, 2.0f, 2.2f, { 1, 4.5f, -1 }, { 1, 4.5f, 1 }), "porte : autre etage");
        // yaw 90 : battant vers -z (de (0,0,0) a (0,0,-2))
        check(Door::panelBlocksSegment({ 0, 0, 0 }, 90.0f, 2.0f, 2.2f, { -1, 0.05f, -1 }, { 1, 0.05f, -1 }), "porte : battant tourne de 90 deg");
    }

    // ── Mini-jeux des taches : un bot joue avec des entrees simulees ────
    {
        const float dt = 1.0f / 60.0f;
        auto run = [&](MiniGame& g, auto bot, float maxTime) {
            float t = 0.0f;
            for (; t < maxTime && !g.solved(); t += dt) g.update(dt, bot());
            return t;
        };
        QuestType qt;
        check(parseQuestType("simon", qt) && qt == QuestType::Simon && !parseQuestType("x", qt), "taches : types de poste");

        WiresGame wires(7);
        // Erreur volontaire : cable 0 sur une prise d'une autre couleur.
        int wrong = 0; while (wires.socketColor(wrong) == 0) ++wrong;
        QuestInput in; in.confirm = true; wires.update(dt, in);                 // prend le cable 0
        for (int k = 0; k < wrong; ++k) { QuestInput d; d.down = true; wires.update(dt, d); }
        wires.update(dt, in);
        check(wires.error() && wires.connectedCount() == 0, "cables : mauvaise prise refusee");
        bool flip = false;
        const float tw = run(wires, [&]() {
            QuestInput q; flip = !flip; if (!flip) return q;                  // une action toutes les 2 frames
            if (wires.side() == 0) q.confirm = true;
            else if (wires.socketColor(wires.cursor()) == wires.selectedCable() && !wires.socketUsed(wires.cursor())) q.confirm = true;
            else q.down = true;
            return q;
        }, 10.0f);
        check(wires.solved() && wires.connectedCount() == WiresGame::N && tw < 2.0f, "cables : resolu");

        SimonGame simon(7);
        const float ts = run(simon, [&]() {
            QuestInput q; flip = !flip;
            if (flip && simon.waitingInput()) { const int e = simon.expected(); q.up = e == 0; q.right = e == 1; q.down = e == 2; q.left = e == 3; }
            return q;
        }, 40.0f);
        check(simon.solved() && ts < 20.0f, "digicode : resolu");

        TimingGame timing(7);
        QuestInput miss; miss.confirm = !timing.cursorInZone(); timing.update(dt, miss);
        const float tt = run(timing, [&]() { QuestInput q; q.confirm = timing.cursorInZone(); return q; }, 30.0f);
        check(timing.solved() && tt < 10.0f, "chaudiere : resolu");

        WaveGame wave(7);
        const float tv = run(wave, [&]() {
            QuestInput q;
            q.leftHeld = wave.freqError() > 0.03f;  q.rightHeld = wave.freqError() < -0.03f;
            q.downHeld = wave.ampError() > 0.015f;  q.upHeld = wave.ampError() < -0.015f;
            return q;
        }, 30.0f);
        check(wave.solved() && tv < 6.0f, "radio : resolu");
        std::printf("[SelfTest]   mini-jeux resolus en %.1f / %.1f / %.1f / %.1f s\n", tw, ts, tt, tv);
    }

    // ── Options de partie ────────────────────────────────────────────────
    MatchSettings s;
    s.difficulty = 0; check(s.lives() == 5 && s.tasks() == 3 && s.creatureSpeedMul() < 1.0f, "difficulte Facile");
    s.difficulty = 1; check(s.lives() == 3 && s.tasks() == 5, "difficulte Normal");
    s.difficulty = 2; check(s.lives() == 1 && s.tasks() == 8 && s.creatureSpeedMul() > 1.0f, "difficulte Difficile");
    for (int m = 0; m < 3; ++m) { s.map = m; check(s.mapPath() != nullptr, "chemin de carte"); }
    s.mode = 1; check(!s.creatureEnabled(), "mode Exploration : sans fropy");

    std::printf("[SelfTest] %d echec(s)\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}

int dumpRig(const char* inPath, const char* outPath) {
    Assimp::Importer importer;
    importer.SetPropertyBool(AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, false);   // comme Model::loadExternalAnimations
    const aiScene* scene = importer.ReadFile(inPath, 0);
    if (!scene || !scene->mRootNode) { std::printf("[dumprig] echec : %s\n", importer.GetErrorString()); return 1; }
    FILE* f = std::fopen(outPath, "w");
    if (!f) return 1;
    // Noeuds en ordre parent avant enfant : { name, parent, local (16 flottants, ligne par ligne) }
    std::fprintf(f, "{\n \"nodes\": [\n");
    int count = 0;
    std::vector<std::pair<const aiNode*, int>> stack{ { scene->mRootNode, -1 } };
    while (!stack.empty()) {
        const auto [n, parent] = stack.back();
        stack.pop_back();
        const aiMatrix4x4& m = n->mTransformation;
        std::fprintf(f, "%s  {\"name\": \"%s\", \"parent\": %d, \"local\": [", count ? ",\n" : "", n->mName.C_Str(), parent);
        for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c) std::fprintf(f, "%s%.9g", r || c ? ", " : "", m[r][c]);
        std::fprintf(f, "]}");
        const int self = count++;
        for (int i = static_cast<int>(n->mNumChildren) - 1; i >= 0; --i) stack.push_back({ n->mChildren[i], self });
    }
    // Bones des meshes (s'il y en a) : nom + offset (inverse du bind)
    std::fprintf(f, "\n ],\n \"bones\": [\n");
    int nb = 0;
    for (unsigned mi = 0; mi < scene->mNumMeshes; ++mi) {
        for (unsigned bi = 0; bi < scene->mMeshes[mi]->mNumBones; ++bi) {
            const aiBone* b = scene->mMeshes[mi]->mBones[bi];
            std::fprintf(f, "%s  {\"name\": \"%s\", \"offset\": [", nb++ ? ",\n" : "", b->mName.C_Str());
            for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c) std::fprintf(f, "%s%.9g", r || c ? ", " : "", b->mOffsetMatrix[r][c]);
            std::fprintf(f, "]}");
        }
    }
    // Premiere animation : cles de rotation par canal { node, ticksPerSecond, keys: [t, w, x, y, z]... }
    std::fprintf(f, "\n ],\n \"animations\": %u,\n \"channels\": [\n", scene->mNumAnimations);
    if (scene->mNumAnimations > 0) {
        const aiAnimation* a = scene->mAnimations[0];
        for (unsigned ci = 0; ci < a->mNumChannels; ++ci) {
            const aiNodeAnim* ch = a->mChannels[ci];
            std::fprintf(f, "%s  {\"node\": \"%s\", \"tps\": %g, \"rot\": [", ci ? ",\n" : "", ch->mNodeName.C_Str(),
                         a->mTicksPerSecond > 0 ? a->mTicksPerSecond : 25.0);
            for (unsigned k = 0; k < ch->mNumRotationKeys; ++k) {
                const aiQuatKey& q = ch->mRotationKeys[k];
                std::fprintf(f, "%s[%g, %g, %g, %g, %g]", k ? ", " : "", q.mTime, q.mValue.w, q.mValue.x, q.mValue.y, q.mValue.z);
            }
            std::fprintf(f, "]}");
        }
    }
    std::fprintf(f, "\n ]\n}\n");
    std::fclose(f);
    std::printf("[dumprig] %d noeuds, %d bones, %u animations -> %s\n", count, nb, scene->mNumAnimations, outPath);
    return 0;
}
