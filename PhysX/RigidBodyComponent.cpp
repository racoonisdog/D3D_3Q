#include "RigidBodyComponent.h"
#include "PhysXHelper.h"

void RigidBodyComponent::CreateActor(physx::PxPhysics* physics, const Transform& transform)
{
	if (m_Actor) return;

	physx::PxTransform px_Trans = ToPxTrans(transform.localPosition, transform.localRotation);

	if (m_Desc.type == Type::Static)
	{
		m_Actor = physics->createRigidDynamic(px_Trans);
	}
	else
	{
		auto* t_dynamic = physics->createRigidDynamic(px_Trans);
		m_Actor = t_dynamic;

		t_dynamic->setLinearDamping(m_Desc.linearDamping);
		t_dynamic->setAngularDamping(m_Desc.angularDamping);
		t_dynamic->setActorFlag(physx::PxActorFlag::eDISABLE_GRAVITY, !m_Desc.useGravity);

		if (m_Desc.isKinematic) {
			t_dynamic->setRigidBodyFlag(physx::PxRigidBodyFlag::eKINEMATIC, true);
		}
	}

	for (auto* t_collider : m_PendingColliders) {
		AttachCollider(t_collider);
	}
	m_PendingColliders.clear();

	if (auto* t_dynamic = m_Actor->is<physx::PxRigidDynamic>()){
		physx::PxRigidBodyExt::updateMassAndInertia(*t_dynamic, m_Desc.mass);
	}
}

void RigidBodyComponent::AttachCollider(ColliderComponent* collider)
{
	if (!collider) return;

	if (!m_Actor)
	{
		//actor 생성 전이면 보관
		m_PendingColliders.push_back(collider);
		return;
	}

	physx::PxShape* shape = collider->GetPxShape();
	if (!shape) return;

	m_Actor->attachShape(*shape);

	//dynamic이면 질량/관성 재계산
	if (auto t_dynamic = m_Actor->is<physx::PxRigidDynamic>()) {
		physx::PxRigidBodyExt::updateMassAndInertia(*t_dynamic, m_Desc.mass);
	}
}
