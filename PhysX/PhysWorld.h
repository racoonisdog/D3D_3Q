#pragma once
#include "PhysX.h"
#include "PhysicsEventCallback.h"

//using namespace DirectX::SimpleMath;

struct PhysWorldDesc
{
	//Vec3 gravity = { 0.f, -9.81f, 0.f };
	float gravity[3] = {0.f, -9.81f, 0.f};

	uint32_t cpuThreadCount = 0;
	bool enableCCD = false;
	bool enablePVD = false;

	void* userFilterShader = nullptr;
	void* userEventCallback = nullptr;
};

class PhysWorld
{
public:
	PhysWorld() = default;
	~PhysWorld() = default;

	bool Create(const PhysWorldDesc& desc);
	bool CreateScene();

	void Destroy();

	//시뮬레이션을 한 프레임 진행시키는 함수 //simulate 계산 시작, fetchResults 계산이 끝날때까지 대기 //멀티스레드
	void Step(float dt);

	//PxRigidActor를 PxScene에 등록 // add한순간부터 물리가 적용
	void AddActor(physx::PxActor& actor);
	//Scene에서 actor를 빼는 함수  //Scene에서 빠지면 더이상 시뮬안함
	void RemoveActor(physx::PxActor& actor);

	physx::PxScene& GetScene() { return *m_scene; }

private:
	PhysWorldDesc m_desc;

	physx::PxScene* m_scene = nullptr;
	physx::PxDefaultCpuDispatcher* m_dispatcher = nullptr;
	
	PhysicsEventCallback m_callback;
};

