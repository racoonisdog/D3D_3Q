#include "PhysWorld.h"
#include "PhysXHelper.h"
#include "PhysicsFilter.h"

bool PhysWorld::Create(const PhysWorldDesc& desc)
{
	m_desc = desc;

	//Dispatcher(thread ¼³Á¤)
	uint32_t threads = (m_desc.cpuThreadCount == 0) ? 2u : m_desc.cpuThreadCount;
	m_dispatcher = physx::PxDefaultCpuDispatcherCreate(threads);
	if (!m_dispatcher) return false;
	
	if (!CreateScene())
	{
		SAFE_RELEASE(m_dispatcher);
		return false;
	}

	if (m_desc.enablePVD){
		//SetupPvdFlags();
	}

	return true;
}

bool PhysWorld::CreateScene()
{
	auto& physics = PhysicX::Get().GetPhysics();

	physx::PxSceneDesc pxDesc(physics.getTolerancesScale());
	pxDesc.gravity = ToPxVec(m_desc.gravity);
	pxDesc.cpuDispatcher = m_dispatcher;
	pxDesc.filterShader = CustomFilterShader;
	pxDesc.simulationEventCallback = &m_callback;

	m_scene = PhysicX::Get().GetPhysics().createScene(pxDesc);
	if (m_scene == nullptr) return false;

	return true;
}

void PhysWorld::Destroy()
{
	if (!m_scene) return;

}

void PhysWorld::Step(float dt)
{
	m_scene->simulate(dt);
	m_scene->fetchResults(true);
}

void PhysWorld::AddActor(physx::PxActor& actor)
{
	m_scene->addActor(actor);
}

void PhysWorld::RemoveActor(physx::PxActor& actor)
{
	m_scene->removeActor(actor);
}
