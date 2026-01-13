#include "PhysicsEventCallback.h"

void PhysicsEventCallback::onContact(const physx::PxContactPairHeader& header, const physx::PxContactPair* pair, physx::PxU32 nbPairs)
{
	std::cout << "충돌 발생" << std::endl;
}

void PhysicsEventCallback::onTrigger(physx::PxTriggerPair* pairs, physx::PxU32 count)
{

}

void PhysicsEventCallback::onConstraintBreak(physx::PxConstraintInfo*, physx::PxU32)
{

}

void PhysicsEventCallback::onWake(physx::PxActor**, physx::PxU32)
{

}

void PhysicsEventCallback::onSleep(physx::PxActor**, physx::PxU32)
{

}

void PhysicsEventCallback::onAdvance(const physx::PxRigidBody* const*, const physx::PxTransform*, const physx::PxU32)
{

}
