#include "ModelEntity.h"
#include "Model.h"
#include "Shader.h"
#include "Camera.h"
#include "LightManager.h"
#include "Animator.h"
#include "SkinningData.h"
#include <memory>

#include <glad/glad.h>  // GL_TRUE/GL_FALSE/glDepthMask — requise par l'outline pass

ModelEntity::ModelEntity(Camera* camera, LightManager* lightManager, Renderer* renderer, const std::string& modelPath, TextureManager* textureManager)
	: m_camera(camera), m_lightManager(lightManager), Entity(renderer, nullptr) {
	m_model = std::make_unique<Model>(m_camera, m_lightManager, modelPath, textureManager);

	// Animation
	m_animator = std::make_unique<Animator>();
	m_animator->setup(m_model.get());

	detectAnimations();
	if (m_hasAnimations) {
		if (m_idleAnimIndex >= 0) {
			m_animator->playAnimation(m_idleAnimIndex, true);
		} else {
			m_animator->playAnimation(0u, true);
		}
		m_animator->update(0.0f);
	}
}

ModelEntity::~ModelEntity() {
}

void ModelEntity::detectAnimations() {
	const size_t numAnims = m_model->getNumAnimations();
	if (numAnims == 0) return;

	m_hasAnimations = true;

	// Detection des animations (modele principal + animations externes).
	// Regles : 1) "walk"/"walking"/"marche" > "run", 2) suffixe -M/_m prefere
	// (GLB Mixamo), 3) premiere occurrence plutot que la derniere.
	struct AnimPick { int index = -1; bool isWalk = false; bool isMale = false; };
	AnimPick idle, walk, run, punchAny, punchJab, rest;

	for (size_t i = 0; i < numAnims; i++) {
		const aiAnimation* anim = m_model->getAnimation(i);
		if (!anim) continue;
		std::string name(anim->mName.C_Str());
		const float tps = anim->mTicksPerSecond > 0.0f ? static_cast<float>(anim->mTicksPerSecond) : 30.0f;
		printf("[ModelEntity] Animation %zu: \"%s\" (%.1fs)\n", i, name.c_str(),
		       static_cast<float>(anim->mDuration) / tps);

		std::string lower = name;
		for (auto& c : lower) c = static_cast<char>(tolower(c));

		const bool isMale   = lower.find("-m") != std::string::npos || lower.find("_m") != std::string::npos;
		const bool isIdle   = lower.find("idle") != std::string::npos;
		const bool isWalkNm = lower.find("walk") != std::string::npos
		                    || lower.find("marche") != std::string::npos;
		const bool isRunNm  = lower.find("run") != std::string::npos;
		const bool isPunch  = lower.find("punch") != std::string::npos;
		const bool isCharge = lower.find("charge") != std::string::npos;
		const bool isRest   = lower.find("rest") != std::string::npos;

		// Idle : premier match, -M prefere
		if (isIdle && (idle.index < 0 || (isMale && !idle.isMale))) {
			idle = { static_cast<int>(i), false, isMale };
		}
		// Marche : "walk"/"walking"/"marche" > "run", -M prefere
		if ((isWalkNm || isRunNm) && (walk.index < 0
			|| (isWalkNm && !walk.isWalk)
			|| (isWalkNm == walk.isWalk && isMale && !walk.isMale))) {
			walk = { static_cast<int>(i), isWalkNm, isMale };
		}
		// Course (sprint) : "run", -M prefere, premier match
		if (isRunNm && (run.index < 0 || (isMale && !run.isMale))) {
			run = { static_cast<int>(i), false, isMale };
		}
		// Punch (jab sur R) : "punch" SANS "charge" prefere (Left-Punch-M),
		// sinon n'importe quel punch (ex: Charge-Punch-M)
		if (isPunch) {
			if (isCharge) {
				if (punchAny.index < 0 || (isMale && !punchAny.isMale))
					punchAny = { static_cast<int>(i), false, isMale };
			} else {
				if (punchJab.index < 0 || (isMale && !punchJab.isMale))
					punchJab = { static_cast<int>(i), false, isMale };
				if (punchAny.index < 0 || (isMale && !punchAny.isMale))
					punchAny = { static_cast<int>(i), false, isMale };
			}
		}
		// Rest (pose d'attente avant l'idle) : premier match
		if (isRest && rest.index < 0) {
			rest = { static_cast<int>(i), false, isMale };
		}
	}

	m_idleAnimIndex = idle.index;
	m_walkAnimIndex = walk.index;
	m_runAnimIndex = run.index;
	m_punchAnimIndex = punchJab.index >= 0 ? punchJab.index : punchAny.index;
	m_restAnimIndex = rest.index;
}

void ModelEntity::updateAnimation(float deltaTime) {
	if (!m_hasAnimations || !m_animator) return;
	m_animator->update(deltaTime);
}

void ModelEntity::playAnimation(int animIndex, bool loop) {
	if (!m_hasAnimations || !m_animator || animIndex < 0) return;
	m_animator->playAnimation(animIndex, loop);
}

void ModelEntity::playIdle() {
	playAnimation(m_idleAnimIndex, true);
}

void ModelEntity::playWalk() {
	playAnimation(m_walkAnimIndex, true);
}

void ModelEntity::draw(Shader* shader) {
    // Matrice modele (cachee : recompute seulement si pos/direction/spin changent)
    glm::mat4 model = getModelMatrix();

    // ── OUTLINE PASS ─────────────────────────────────────────────────────────────
    if (m_outlineEnabled && m_outlineShader) {
        Outline::draw3DModel(m_outlineShader, shader,
                             m_outlineColor, m_outlineThickness,
                             model, *m_model);
    }

    shader->use();
    shader->setModel(model);      // il faut setter le model avant
    shader->setupMatrices();      // envoie model + view + projection

    // Bone matrices pour le skinning (shader "skinned" uniquement)
    if (m_hasAnimations && m_animator && shader->getType() == ShaderType::SkinnedModel) {
        const auto& boneMats = m_animator->getFinalBoneMatrices();
        // Envoi groupé : un seul glUniformMatrix4fv pour tout le tableau (au
        // lieu de ~128 appels + 128 hachages de string). count est borné par
        // SHADER_MAX_BONES, la taille du tableau uniform côté shader.
        size_t count = boneMats.size() < SHADER_MAX_BONES
                       ? boneMats.size() : SHADER_MAX_BONES;
        shader->setMat4Array("uBoneMatrices[0]", boneMats.data(), static_cast<int>(count));
    }

    shader->setVec3("viewPos", m_camera->getPosition());
    m_lightManager->applyToShader(shader);

    m_model->draw(*shader);
}

void ModelEntity::drawDebug(Shader* shader) {
    shader->use();

    glm::mat4 model = getModelMatrix();

    shader->setMat4("model", model);

    // Dessiner la bounding box
    m_model->drawBoundingBox(*shader);
}

bool ModelEntity::checkCollision(const ModelEntity& other) const {
    glm::mat4 thisMatrix = getModelMatrix();
    glm::mat4 otherMatrix = other.getModelMatrix();

    return m_model->checkCollision(*other.m_model, thisMatrix, otherMatrix);
}

bool ModelEntity::raycast(const glm::vec3& origin, const glm::vec3& direction,
    float& distance) const {
    glm::mat4 modelMatrix = getModelMatrix();
    return m_model->raycast(origin, direction, modelMatrix, distance);
}

BoundingBox ModelEntity::getWorldBoundingBox() const {
    return m_model->getTransformedBoundingBox(getModelMatrix());
}

glm::mat4 ModelEntity::getModelMatrix() const {
    // Cache : retourne la matrice deja calculee si rien n'a change depuis le
    // dernier appel (position / pointeur de direction / version de direction /
    // axe + angle de spin). Evite le translate+rotate+multiplie a chaque
    // appel (3-4 appels par frame sans ce cache).
    const Direction* dir = m_direction;
    const unsigned int dirVersion = dir->getVersion();
    if (m_modelMatrixValid
        && m_position == m_cachedPos
        && dir == m_cachedDirPtr
        && dirVersion == m_cachedDirVersion
        && m_spinAxis == m_cachedSpinAxis
        && m_spinAngle == m_cachedSpinAngle
        && m_scale == m_cachedScale
        && m_yawOffsetDeg == m_cachedYawOffsetDeg) {
        return m_cachedModelMatrix;
    }

    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, m_position);

    glm::vec3 dirVec = dir->getDirectionVector();
    // yaw = direction de la camera + offset de cap (ex: orienter le modele 3P
    // vers le sens de marche pour aligner l'animation avec la trajectoire).
    float yaw = atan2(dirVec.x, dirVec.z) + glm::radians(m_yawOffsetDeg);
    model = glm::rotate(model, yaw, glm::vec3(0, 1, 0));

    // Spin sur soi-même (hérité d'Entity)
    model = model * getSpinRotation();

    // Echelle uniforme
    model = glm::scale(model, glm::vec3(m_scale));

    m_cachedModelMatrix = model;
    m_cachedPos = m_position;
    m_cachedDirPtr = dir;
    m_cachedDirVersion = dirVersion;
    m_cachedSpinAxis = m_spinAxis;
    m_cachedSpinAngle = m_spinAngle;
    m_cachedScale = m_scale;
    m_cachedYawOffsetDeg = m_yawOffsetDeg;
    m_modelMatrixValid = true;
    return m_cachedModelMatrix;
}