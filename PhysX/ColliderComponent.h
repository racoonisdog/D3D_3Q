#pragma once
#include <physx/PxPhysicsAPI.h>


class ColliderComponent
{
public:
	virtual ~ColliderComponent() = default;

	enum class Role
	{
		Physics,
		Trigger,
		Attack
	};

	void SetRole(Role r);

	physx::PxShape* GetPxShape() const { return m_Shape; }


protected:
	physx::PxShape* m_Shape = nullptr;
	Role role = Role::Physics;

	void ApplyRoleFlags();
};

