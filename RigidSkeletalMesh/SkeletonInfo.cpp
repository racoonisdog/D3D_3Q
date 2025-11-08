#include "SkeletonInfo.h"

void SkeletonInfo::CalculateGlobalInverseRoot()
{
    if (Bones.empty()) {
        GlobalInverseRoot = Matrix::Identity;
        return;
    }

    // 1. 모든 뼈대의 Bind Pose Global Transform을 계산
    std::vector<Matrix> BindGlobal(Bones.size());

    for (size_t i = 0; i < Bones.size(); ++i) {
        int parentIndex = Bones[i].ParentIndex;
        // BindLocal은 ModelLoader::SetBoneHierarchy에서 저장한 값입니다.
        const Matrix& BindLocal = Bones[i].BindLocal;

        if (parentIndex == -1) { // 루트 뼈 (ParentIndex가 -1)
            BindGlobal[i] = RootModelTransform * BindLocal;
        }
        else {
            BindGlobal[i] = BindGlobal[parentIndex] * BindLocal;
        }
    }

    // 2. 루트 뼈의 인덱스를 찾습니다. (ParentIndex가 -1인 첫 번째 뼈대)
    int rootIndex = -1;
    for (size_t i = 0; i < Bones.size(); ++i) {
        if (Bones[i].ParentIndex == -1) {
            rootIndex = (int)i;
            break;
        }
    }

    // 3. 루트 뼈의 Bind Global 행렬을 역행렬하여 저장
    if (rootIndex != -1) {
        GlobalInverseRoot = BindGlobal[rootIndex].Invert();
    }
    else {
        GlobalInverseRoot = Matrix::Identity; // 루트가 없으면 항등 행렬
    }
}
