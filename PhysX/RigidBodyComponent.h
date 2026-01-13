#pragma once
#include <physx/PxPhysicsAPI.h>
#include "ColliderComponent.h"
#include "Transform.h"

class RigidBodyComponent
{
public:
	enum class Type
	{
		Static,
		Dynamic
	};

	struct Desc
	{
		Type type = Type::Dynamic;
		float mass = 1.0f;
		float linearDamping = 0.0f;
		float angularDamping = 0.05f;
		bool useGravity = true;
		bool isKinematic = false;
	};


public:
	explicit RigidBodyComponent(const Desc& desc) : m_Desc(desc) {};

	//Scene에 등록
	void CreateActor(physx::PxPhysics* physics, const Transform& transform);

	//Shape 붙이기
	void AttachCollider(ColliderComponent* collider);

	physx::PxRigidActor* GetActor() const { return m_Actor; }

private:
	Desc m_Desc;
	physx::PxRigidActor* m_Actor = nullptr;
	std::vector<ColliderComponent*> m_PendingColliders;
};

