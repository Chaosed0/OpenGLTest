
#include "RigidbodyMotorSystem.h"

#include "Game/Components/RigidbodyMotorComponent.h"
#include "Game/Components/CollisionComponent.h"
#include "Game/Components/HealthComponent.h"

#include "Util.h"

#include <btBulletDynamicsCommon.h>

#include <glm/gtx/vector_angle.hpp>

RigidbodyMotorSystem::RigidbodyMotorSystem(World& world)
	: System(world)
{
	require<RigidbodyMotorComponent>();
	require<CollisionComponent>();
}

void RigidbodyMotorSystem::updateEntity(float dt, eid_t entity)
{
	RigidbodyMotorComponent* rigidbodyMotorComponent = world.getComponent<RigidbodyMotorComponent>(entity);
	CollisionComponent* collisionComponent = world.getComponent<CollisionComponent>(entity);

	if (!rigidbodyMotorComponent->canMove ||
		collisionComponent->collisionObject->getInternalType() != btCollisionObject::CO_RIGID_BODY) {
		return;
	}

	btRigidBody* body = (btRigidBody*)collisionComponent->collisionObject;
	btVector3 velocity = body->getLinearVelocity();

	glm::vec2 movement;
	if (glm::length(rigidbodyMotorComponent->movement) > glm::epsilon<float>()) {
		movement = glm::normalize(rigidbodyMotorComponent->movement) * rigidbodyMotorComponent->moveSpeed;
	}

	glm::vec3 facingVec = rigidbodyMotorComponent->facing * Util::forward;
	facingVec.y = 0.0f;
	float hFacing = glm::orientedAngle(Util::forward, glm::normalize(facingVec), Util::up);
	body->getWorldTransform().setRotation(btQuaternion(Util::glmToBt(Util::up), hFacing));

	if (rigidbodyMotorComponent->noclip) {
		body->setCollisionFlags(btCollisionObject::CF_NO_CONTACT_RESPONSE);
		if (glm::length(movement) > glm::epsilon<float>()) {
			// TODO: There's something wrong with the vertical component of facing
			velocity = Util::glmToBt(rigidbodyMotorComponent->facing * glm::vec3(movement.y, 0.0f, -movement.x) * 10.0f);
		}
		else {
			velocity = btVector3(0.0f, 0.0f, 0.0f);
		}
	} else {
		body->setCollisionFlags(0);
		if (glm::length(movement) > glm::epsilon<float>()) {
			// Negate Z because forward movement is in the negative direction
			velocity.setX(movement.y);
			velocity.setZ(-movement.x);
			
			btQuaternion playerRotation = body->getWorldTransform().getRotation();
			velocity = velocity.rotate(playerRotation.getAxis(), playerRotation.getAngle());
		} else {
			velocity.setZ(0.0f);
			velocity.setX(0.0f);
		}

		if (rigidbodyMotorComponent->jump && rigidbodyMotorComponent->canJump) {
			velocity.setY(rigidbodyMotorComponent->jumpSpeed);
			rigidbodyMotorComponent->canJump = false;
		}
	}

	if (velocity.length() > glm::epsilon<float>()) {
		body->activate();
	}
	body->setLinearVelocity(velocity);
}