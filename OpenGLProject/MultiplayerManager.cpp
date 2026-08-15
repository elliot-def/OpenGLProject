#include "MultiplayerManager.h"

#include "SteamManager.h"
#include "ModelEntity.h"
#include "ModelLoader.h"
#include "Animator.h"
#include "Shader.h"
#include "ShaderManager.h"
#include "Player.h"
#include "Log.h"

#include <cstring>

// ---------------------------------------------------------------------------
// Format du paquet d'état (binaire, fiable, canal 0) — 32 octets :
//   [0..3]   magic   "MPGL"
//   [4..7]   version (uint32 = 1)
//   [8..19]  position (3× float32)
//   [20..23] yaw (float32, degrés)
//   [24..27] index d'animation (int32, -1 = inconnu)
//   [28..31] réservé (uint32)
// ---------------------------------------------------------------------------
#pragma pack(push, 1)
struct PlayerStatePacket {
    char     magic[4];
    uint32_t version;
    float    posX, posY, posZ;
    float    yawDeg;
    int32_t  animIndex;
    uint32_t reserved;
};
#pragma pack(pop)
static_assert(sizeof(PlayerStatePacket) == 32, "PlayerStatePacket doit faire 32 octets");

static constexpr uint32_t kPacketVersion = 1;
static constexpr char     kPacketMagic[4] = { 'M', 'P', 'G', 'L' };

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

    // 1. Diffuser l'état local à ~10 Hz.
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
    // charge les mêmes clips dans le même ordre).
    Animator* anim = m_localHuman->getAnimator();
    pkt.animIndex = anim ? anim->getCurrentAnimationIndex() : -1;

    m_steam->broadcastP2P(&pkt, sizeof(pkt));
}

void MultiplayerManager::processIncomingMessages() {
    std::vector<SteamManager::P2PMessage> messages;
    m_steam->receiveP2P(messages, 32);

    for (const auto& msg : messages) {
        if (msg.data.size() != sizeof(PlayerStatePacket)) continue;

        PlayerStatePacket pkt;
        std::memcpy(&pkt, msg.data.data(), sizeof(pkt));

        if (std::memcmp(pkt.magic, kPacketMagic, 4) != 0) continue;
        if (pkt.version != kPacketVersion) continue;

        const uint64_t id = msg.sender.ConvertToUint64();
        ensureRemotePlayer(id);

        RemotePlayer& rp = *m_remotePlayers[id];
        rp.targetPos = glm::vec3(pkt.posX, pkt.posY, pkt.posZ);
        rp.targetYawDeg = pkt.yawDeg;
        rp.animIndex = static_cast<int>(pkt.animIndex);
        rp.timeSincePacket = 0.0f;
    }
}

void MultiplayerManager::applyState(RemotePlayer& rp) {
    if (!rp.entity) return;

    rp.entity->setPosition(rp.targetPos);
    rp.entity->getDirection()->setYawPitch(static_cast<double>(rp.targetYawDeg), 0.0);

    if (rp.animIndex >= 0 && rp.animIndex != rp.lastAnimIndex) {
        rp.entity->playAnimation(rp.animIndex, true);
        rp.lastAnimIndex = rp.animIndex;
    }
}
