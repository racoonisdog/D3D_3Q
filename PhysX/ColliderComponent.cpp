#include "ColliderComponent.h"

void ColliderComponent::SetRole(Role r)
{
	role = r;
	ApplyRoleFlags();
}

void ColliderComponent::ApplyRoleFlags()
{
	if (!m_Shape) return;

	const bool isTrigger = (role == Role::Trigger || role == Role::Attack);

	m_Shape->setFlag(physx::PxShapeFlag::eSIMULATION_SHAPE, !isTrigger);
	m_Shape->setFlag(physx::PxShapeFlag::eTRIGGER_SHAPE, isTrigger);
}
