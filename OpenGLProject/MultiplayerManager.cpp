#include "MultiplayerManager.h"

#include "SteamManager.h"
#include "VoiceChat.h"   // VoicePacket (classification des paquets voix)
#include "ModelEntity.h"
#include "ModelLoader.h"
#include "Animator.h"
#include "Shader.h"
#include "ShaderManager.h"
#include "Player.h"
#include "Log.h"

#include <cstring>
#include <algorithm>

namespace {
// Interpolation de cap (degrés) par le chemin le plus court : de 350° à 10°,
// on passe par 0° (delta +20°) et non par -340°.
float lerpAngleDeg(float from, float to, float t) {
    float diff = to - from;
    while (diff > 180.0f)  diff -= 360.0f;
    while (diff < -180.0f) diff += 360.0f;
    return from + diff * t;
}
} // namespace

// ---------------------------------------------------------------------------
// Format du paquet d'état (binaire, fiable, canal 0) — 32 octets :
//   [0..3]   magic   "MPGL"
//   [4..7]   version (uint32 = 2)
//   [8..19]  position (3× float32)
//   [20..23] yaw (float32, degrés)
//   [24..27] index d'animation (int32, -1 = inconnu)
//   [28..31] boucle (uint32, 1 = boucle, 0 = one-shot)
// ---------------------------------------------------------------------------
#pragma pack(push, 1)
struct PlayerStatePacket {
    char     magic[4];
    uint32_t version;
    float    posX, posY, posZ;
    float    yawDeg;
    int32_t  animIndex;
    uint32_t loop;
};
#pragma pack(pop)
static_assert(sizeof(PlayerStatePacket) == 32, "PlayerStatePacket doit faire 32 octets");

// Paquet de chat du lobby (meme canal P2P 0, magic different) :
//   [0..3]   magic       "MPGC"
//   [4..7]   version     (uint32 = 1)
//   [8..39]  senderName  (persona name, 31 chars + \0)
//   [40..295] text       (message, 255 chars + \0)
#pragma pack(push, 1)
struct ChatPacket {
    char     magic[4];
    uint32_t version;
    char     senderName[32];
    char     text[256];
};
#pragma pack(pop)
static_assert(sizeof(ChatPacket) == 296, "ChatPacket doit faire 296 octets");

static constexpr uint32_t kPacketVersion = 2;
static constexpr char     kPacketMagic[4] = { 'M', 'P', 'G', 'L' };
static constexpr uint32_t kChatVersion   = 1;
static constexpr char     kChatMagic[4]  = { 'M', 'P', 'G', 'C' };

MultiplayerManager::MultiplayerManager(SteamManager* steam, Camera* camera,
                                       LightManager* lightManager, Renderer* renderer,
                                       TextureManager* textureManager,
                                       ShaderManager* shaderManager, Player* player,
                                       ModelEntity* localHuman)
    : m_steam(steam), m_camera(camera), m_lightManager(lightManager),
      m_renderer(renderer), m_textureManager(textureManager),
      m_shaderManager(shaderManager), m_player(player), m_localHuman(localHuman) {
    m_skinnedShader = m_shaderManager->getShader("skinned");
}

MultiplayerManager::~MultiplayerManager() = default;

// ---------------------------------------------------------------------------
// Cycle de vie
// ---------------------------------------------------------------------------

void MultiplayerManager::onLobbyChanged() {
    // Repartir d'une liste vide : les pairs sont recréés à la réception de
    // leur premier paquet (création paresseuse, évite de charger un Megan par
    // membre présent mais inactif dans le lobby).
    m_remotePlayers.clear();
    LOG_INFO("[Multiplayer] Lobby rejoint : synchronisation P2P active.");
}

void MultiplayerManager::onLobbyLeft() {
    m_remotePlayers.clear();
    LOG_INFO("[Multiplayer] Lobby quitte : joueurs distants liberes.");
}

// ---------------------------------------------------------------------------
// Mise à jour
// ---------------------------------------------------------------------------

void MultiplayerManager::update(float dt) {
    if (!m_steam) return;

    // 1. Diffuser l'état local à ~20 Hz.
    m_sendTimer += dt;
    if (m_sendTimer >= SEND_INTERVAL) {
        m_sendTimer = 0.0f;
        broadcastLocalState();
    }

    // 2. Recevoir et appliquer les états distants.
    processIncomingMessages();

    // 3. Faire avancer les animations et purger les pairs silencieux.
    for (auto it = m_remotePlayers.begin(); it != m_remotePlayers.end();) {
        RemotePlayer& rp = *it->second;
        rp.timeSincePacket += dt;
        if (rp.timeSincePacket > STALE_TIMEOUT) {
            LOG_INFO("[Multiplayer] Joueur distant (SteamID %llu) muet depuis %.1fs -> retire.",
                     rp.steamID, static_cast<double>(rp.timeSincePacket));
            it = m_remotePlayers.erase(it);
            continue;
        }
        // Faire progresser l'interpolation vers le dernier checkpoint reçu.
        rp.interpAlpha = std::min(1.0f, rp.interpAlpha + dt / INTERP_DURATION);
        applyState(rp);
        if (rp.entity) rp.entity->updateAnimation(dt);
        ++it;
    }
}

// ---------------------------------------------------------------------------
// Rendu
// ---------------------------------------------------------------------------

void MultiplayerManager::draw() {
    if (!m_skinnedShader) return;
    for (auto& [id, rp] : m_remotePlayers) {
        if (rp && rp->entity) rp->entity->draw(m_skinnedShader);
    }
}

// ---------------------------------------------------------------------------
// Interne
// ---------------------------------------------------------------------------

void MultiplayerManager::ensureRemotePlayer(uint64_t steamID) {
    if (m_remotePlayers.count(steamID)) return;

    LOG_INFO("[Multiplayer] Creation du modele du joueur distant (SteamID %llu)...", steamID);

    auto entity = std::make_unique<ModelEntity>(m_camera, m_lightManager, m_renderer,
                                                "./res/rigging/mixamo/models/Megan.fbx",
                                                m_textureManager);
    ModelLoader::configureHumanCharacter(entity.get());

    // ── Diagnostic (à retirer une fois le bug de skinning identifié) ──
    {
        const Model* model = entity->getModel();
        const Animator* anim = entity->getAnimator();
        const aiAnimation* cur = anim ? anim->getCurrentAnimation() : nullptr;
        LOG_INFO("[Multiplayer] Joueur distant pret : %zu bones, %zu animations, "
                 "hasAnimations=%d, scale=%.4f, anim='%s' (index=%d, loop=%d)",
                 model ? model->getBoneInfoMap().size() : 0,
                 model ? model->getNumAnimations() : 0,
                 entity->hasAnimations() ? 1 : 0,
                 entity->getScale(),
                 cur ? cur->mName.C_Str() : "(null)",
                 anim ? anim->getCurrentAnimationIndex() : -1,
                 (anim && anim->isLooping()) ? 1 : 0);
        const auto& bm = anim ? anim->getFinalBoneMatrices() : std::vector<glm::mat4>();
        if (!bm.empty()) {
            // bone[0] est normalement le Hips (root) : translation ~ (0, ~0.9, 0),
            // diagonale ~1. Identité pure = translation 0 ; zéro = diag 0.
            LOG_INFO("[Multiplayer]   bone[0] transl=(%.3f, %.3f, %.3f) diag=(%.3f, %.3f, %.3f, %.3f)",
                     bm[0][3].x, bm[0][3].y, bm[0][3].z,
                     bm[0][0][0], bm[0][1][1], bm[0][2][2], bm[0][3][3]);
        }
    }

    auto rp = std::make_unique<RemotePlayer>();
    rp->steamID = steamID;
    rp->entity  = std::move(entity);
    m_remotePlayers[steamID] = std::move(rp);
}

void MultiplayerManager::broadcastLocalState() {
    if (!m_steam->isInLobby()) return;
    if (!m_player || !m_localHuman) return;

    PlayerStatePacket pkt;
    std::memset(&pkt, 0, sizeof(pkt));
    std::memcpy(pkt.magic, kPacketMagic, 4);
    pkt.version = kPacketVersion;

    const glm::vec3 pos = m_player->getPosition();
    pkt.posX = pos.x;
    pkt.posY = pos.y;
    pkt.posZ = pos.z;

    // Cap du regard (en degrés, même convention que Direction).
    pkt.yawDeg = static_cast<float>(m_player->getDirection()->getYaw());

    // Animation en cours du modèle local (index stable car tout le monde
    // charge les mêmes clips dans le même ordre). On transmet aussi le flag
    // de boucle : les one-shots (jump/turn/landing/punch) ne bouclent pas et
    // doivent être rejoués tels quels chez les pairs, sinon ils tournent en
    // boucle sur le modèle distant.
    Animator* anim = m_localHuman->getAnimator();
    pkt.animIndex = anim ? anim->getCurrentAnimationIndex() : -1;
    pkt.loop = (anim && anim->isLooping()) ? 1u : 0u;

    m_steam->broadcastP2P(&pkt, sizeof(pkt));
}

void MultiplayerManager::processIncomingMessages() {
    std::vector<SteamManager::P2PMessage> messages;
    m_steam->receiveP2P(messages, 32);

    for (const auto& msg : messages) {
        // ── Chat vocal : paquet voix temps reel (non-fiable) ──
        if (msg.data.size() == sizeof(VoicePacket)) {
            VoicePacket vp;
            std::memcpy(&vp, msg.data.data(), sizeof(vp));
            if (std::memcmp(vp.magic, "MPGV", 4) == 0 && vp.version == 1) {
                if (m_onVoicePacket) {
                    m_onVoicePacket(msg.sender.ConvertToUint64(),
                                    msg.data.data(),
                                    static_cast<uint32_t>(msg.data.size()));
                }
                continue;
            }
        }

        // ── Chat du lobby : message utilisateur ──
        if (msg.data.size() == sizeof(ChatPacket)) {
            ChatPacket pkt;
            std::memcpy(&pkt, msg.data.data(), sizeof(pkt));
            if (std::memcmp(pkt.magic, kChatMagic, 4) == 0 && pkt.version == kChatVersion) {
                // Sécurité : champs toujours terminés par \0 (memset à l'envoi).
                pkt.senderName[sizeof(pkt.senderName) - 1] = '\0';
                pkt.text[sizeof(pkt.text) - 1] = '\0';
                if (m_onChatMessage) {
                    m_onChatMessage(msg.sender.ConvertToUint64(), pkt.senderName, pkt.text);
                }
                continue;
            }
        }

        // ── État de joueur (synchronisation P2P classique) ──
        if (msg.data.size() != sizeof(PlayerStatePacket)) continue;

        PlayerStatePacket pkt;
        std::memcpy(&pkt, msg.data.data(), sizeof(pkt));

        if (std::memcmp(pkt.magic, kPacketMagic, 4) != 0) continue;
        if (pkt.version != kPacketVersion) continue;

        const uint64_t id = msg.sender.ConvertToUint64();
        ensureRemotePlayer(id);

        RemotePlayer& rp = *m_remotePlayers[id];
        const glm::vec3 newPos(pkt.posX, pkt.posY, pkt.posZ);
        const float newYaw = pkt.yawDeg;

        // Checkpoint précédent <- dernier checkpoint : on interpole entre les
        // deux sur la durée d'un intervalle d'envoi (~50 ms de décalage).
        if (rp.hasPrev) {
            rp.prevPos = rp.targetPos;
            rp.prevYawDeg = rp.targetYawDeg;
        } else {
            // Premier paquet : pas d'interpolation (évite un glissement depuis
            // l'origine du monde), on place directement le modèle au checkpoint.
            rp.prevPos = newPos;
            rp.prevYawDeg = newYaw;
            rp.hasPrev = true;
        }
        rp.targetPos = newPos;
        rp.targetYawDeg = newYaw;
        rp.interpAlpha = 0.0f;

        rp.animIndex = static_cast<int>(pkt.animIndex);
        rp.loopAnim = (pkt.loop != 0);
        rp.timeSincePacket = 0.0f;
    }
}

void MultiplayerManager::sendChatMessage(const std::string& text) {
    if (!m_steam || !m_steam->isInLobby()) return;

    ChatPacket pkt;
    std::memset(&pkt, 0, sizeof(pkt));
    std::memcpy(pkt.magic, kChatMagic, 4);
    pkt.version = kChatVersion;

    const char* localName = m_steam->getLocalPersonaName();
    snprintf(pkt.senderName, sizeof(pkt.senderName), "%s",
             localName ? localName : "");
    snprintf(pkt.text, sizeof(pkt.text), "%s", text.c_str());

    m_steam->broadcastP2P(&pkt, sizeof(pkt));
}

void MultiplayerManager::applyState(RemotePlayer& rp) {
    if (!rp.entity) return;

    // Position/cap interpolés entre le checkpoint précédent et le dernier reçu.
    const float t = glm::clamp(rp.interpAlpha, 0.0f, 1.0f);
    const glm::vec3 pos = glm::mix(rp.prevPos, rp.targetPos, t);
    const float yaw = lerpAngleDeg(rp.prevYawDeg, rp.targetYawDeg, t);

    rp.entity->setPosition(pos);
    rp.entity->getDirection()->setYawPitch(static_cast<double>(yaw), 0.0);

    if (rp.animIndex >= 0 && rp.animIndex != rp.lastAnimIndex) {
        // ── Diagnostic (à retirer) ──
        const aiAnimation* a = rp.entity->getModel()
            ? rp.entity->getModel()->getAnimation(rp.animIndex) : nullptr;
        LOG_INFO("[Multiplayer] Joueur %llu -> anim [%d] '%s' (loop=%d)",
                 rp.steamID, rp.animIndex,
                 a ? a->mName.C_Str() : "(INVALIDE)", rp.loopAnim ? 1 : 0);

        // Respecter le flag de boucle reçu : jouer un one-shot (jump/turn/
        // landing/punch) avec loop=false, sinon il boucle indéfiniment sur le
        // modèle distant — et pour "turn", la rotation racine de 90° restait
        // appliquée (root-motion lock `if (!m_loop)`), faisant pivoter le
        // modèle distant sur lui-même.
        rp.entity->playAnimation(rp.animIndex, rp.loopAnim);
        rp.lastAnimIndex = rp.animIndex;
    }
}
