#include "PhysicsFilter.h"

physx::PxFilterFlags CustomFilterShader(physx::PxFilterObjectAttributes attributes0, physx::PxFilterData filterData0, physx::PxFilterObjectAttributes attributes1, physx::PxFilterData filterData1, physx::PxPairFlags& pairFlags, const void* constantBlock, physx::PxU32 constantBlockSize)
{
    //트리거처리
    if (physx::PxFilterObjectIsTrigger(attributes0) || physx::PxFilterObjectIsTrigger(attributes1))
    {
        pairFlags = physx::PxPairFlag::eTRIGGER_DEFAULT;
        pairFlags |= physx::PxPairFlag::eNOTIFY_TOUCH_FOUND;
        pairFlags |= physx::PxPairFlag::eNOTIFY_TOUCH_LOST;

        return physx::PxFilterFlag::eDEFAULT;
    }
    
    pairFlags = physx::PxPairFlag::eCONTACT_DEFAULT;

    pairFlags |= physx::PxPairFlag::eNOTIFY_TOUCH_FOUND;
    pairFlags |= physx::PxPairFlag::eNOTIFY_TOUCH_LOST;

    return physx::PxFilterFlag::eDEFAULT;
}
