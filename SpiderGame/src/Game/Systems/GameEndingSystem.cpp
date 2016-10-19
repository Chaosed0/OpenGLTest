
#include "GameEndingSystem.h"

#include "Game/Components/PlayerComponent.h"
#include "Game/Components/TransformComponent.h"
#include "Game/Components/CameraComponent.h"
#include "Game/Events/EndGameEvent.h"

#include "Game/Components/SpiderComponent.h"
#include "Game/Components/SpawnerComponent.h"

#include "Util.h"

#include <algorithm>

const float GameEndingSystem::screenShakeTime = 3.0f;
const float GameEndingSystem::gemDefenseTime = 30.0f;
const float GameEndingSystem::blackoutTime = 3.0f;
const float GameEndingSystem::fadeInTime = 2.0f;
const float GameEndingSystem::endRestTime = 2.0f;

GameEndingSystem::GameEndingSystem(World& world, EventManager& eventManager, SoundManager& soundManager)
	: System(world), eventManager(eventManager), state(GameEndState_NotEnded), soundManager(soundManager)
{
	require<TransformComponent>();
	require<PlayerComponent>();

	eventManager.registerForEvent<GemCountChangedEvent>(std::bind(&GameEndingSystem::onGemCountChanged, this, std::placeholders::_1));
}

void GameEndingSystem::updateEntity(float dt, eid_t entity)
{
	PlayerComponent* playerComponent = world.getComponent<PlayerComponent>(entity);

	timer += dt;
	if (state == GameEndState_DefendingGems) {
		if (timer >= gemDefenseTime) {
			state = GameEndState_Blackout;
			timer -= gemDefenseTime;

			playerComponent->data.blackoutQuad->isVisible = true;
			soundManager.stopAllClips();

			std::vector<eid_t> spiders = world.getEntitiesWithComponent<SpiderComponent>();
			for (unsigned i = 0; i < spiders.size(); i++) {
				world.removeEntity(spiders[i]);
			}

			std::vector<eid_t> spawners = world.getEntitiesWithComponent<SpawnerComponent>();
			for (unsigned i = 0; i < spawners.size(); i++) {
				world.removeEntity(spawners[i]);
			}

			eventManager.sendEvent(VictorySequenceStartedEvent());
		}
	} else if (state == GameEndState_Blackout) {
		if (timer >= blackoutTime) {
			state = GameEndState_Fadein;
			timer -= blackoutTime;

			// Setup the camera
			std::vector<eid_t> cameras = world.getEntitiesWithComponent<CameraComponent>();
			assert (cameras.size() > 0);
			CameraComponent* oldCameraComponent = world.getComponent<CameraComponent>(cameras[0]);

			eid_t endGameCamera = world.getNewEntity();
			TransformComponent* transformComponent = world.addComponent<TransformComponent>(endGameCamera);
			CameraComponent* cameraComponent = world.addComponent<CameraComponent>(endGameCamera);

			// Copy all the properties of the old camera
			cameraComponent->data = oldCameraComponent->data;

			// Point us toward the sky
			transformComponent->data->setPosition(glm::vec3(0.0f, 100.0f, 0.0f));
			transformComponent->data->setRotation(glm::angleAxis(glm::radians(75.0f), Util::right));

			// Disable the old camera
			oldCameraComponent->isActive = false;
		}
	} else if (state == GameEndState_Fadein) {
		float alpha = (std::max)(1.0f - timer / fadeInTime, 0.0f);
		Material& material = playerComponent->data.blackoutQuad->material;
		material.setProperty("color", MaterialProperty(glm::vec4(0.0f, 0.0f, 0.0f, alpha)));
		if (timer >= fadeInTime) {
			state = GameEndState_Rest;
			timer -= fadeInTime;
		}
	} else if (state == GameEndState_Rest) {
		if (timer >= endRestTime) {
			eventManager.sendEvent(VictorySequenceEndedEvent());
			state = GameEndState_Finished;
			timer = 0.0f;
		}
	}
}

void GameEndingSystem::onGemCountChanged(const GemCountChangedEvent& gemCountChangedEvent)
{
	PlayerComponent* playerComponent = world.getComponent<PlayerComponent>(gemCountChangedEvent.source);
	assert (playerComponent != nullptr);

	allGemsPlaced = true;
	for (unsigned i = 0; i < playerComponent->gemStates.size(); i++) {
		if (playerComponent->gemStates[i] != PlayerGemState_Placed) {
			allGemsPlaced = false;
			break;
		}
	}

	if (allGemsPlaced) {
		AllGemsCollectedEvent event;
		event.source = gemCountChangedEvent.source;
		eventManager.sendEvent(event);

		this->state = GameEndState_DefendingGems;
		this->timer = 0.0f;
	}
}