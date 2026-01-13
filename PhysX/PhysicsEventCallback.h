#pragma once
#include <physx/PxPhysicsAPI.h>
#include <iostream>

class PhysicsEventCallback : public physx::PxSimulationEventCallback
{
public:
	void onContact(const physx::PxContactPairHeader& header, const physx::PxContactPair* pair, physx::PxU32 nbPairs) override;

	void onTrigger(physx::PxTriggerPair* pairs, physx::PxU32 count) override;

	void onConstraintBreak(physx::PxConstraintInfo*, physx::PxU32) override;
	void onWake(physx::PxActor**, physx::PxU32) override;
	void onSleep(physx::PxActor**, physx::PxU32) override;
	void onAdvance(const physx::PxRigidBody* const*, const physx::PxTransform*, const physx::PxU32) override;
};

