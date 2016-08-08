#pragma once

#include "Framework/Component.h"

class PlayerComponent : public Component
{
public:
	PlayerComponent() : shooting(false), maxShotDistance(100.0f), shotCooldown(1/30.0f), shotDamage(10), shotTimer(0.0f) { }

	bool shooting;
	float shotCooldown;
	float shotTimer;
	float maxShotDistance;
	unsigned shotDamage;
};