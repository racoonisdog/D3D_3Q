#include "SkeletonInfo.h"


#include <cstdio>
#include <cstdarg>

static void LOGF(const char* fmt, ...)
{
    FILE* fp = nullptr;
    if (fopen_s(&fp, "bindcheck.log", "a") != 0 || !fp) return;
    va_list args;
    va_start(args, fmt);
    vfprintf(fp, fmt, args);
    va_end(args);
    fclose(fp);
}


static float MaxAbsDiff(const Matrix& A, const Matrix& B, int& worstIdx)
{
    const float* a = &A._11;
    const float* b = &B._11;
    float md = 0.0f; worstIdx = -1;
    for (int k = 0; k < 16; ++k) {
        float d = fabsf(a[k] - b[k]);
        if (d > md) { md = d; worstIdx = k; }
    }
    return md;
}

static void DumpMatrixRowMajor(const char* tag, const Matrix& M)
{
    LOGF("%s =\n", tag);
    LOGF("[ %.6f %.6f %.6f %.6f ]\n", M._11, M._12, M._13, M._14);
    LOGF("[ %.6f %.6f %.6f %.6f ]\n", M._21, M._22, M._23, M._24);
    LOGF("[ %.6f %.6f %.6f %.6f ]\n", M._31, M._32, M._33, M._34);
    LOGF("[ %.6f %.6f %.6f %.6f ]\n", M._41, M._42, M._43, M._44);
}


void SkeletonInfo::CalculateGlobalInverseRoot()
{

    // 1. 모든 뼈대의 Bind Pose Global Transform을 계산
    std::vector<Matrix> BindGlobal(Bones.size());

    for (size_t i = 0; i < Bones.size(); ++i) {
        int parentIndex = Bones[i].ParentIndex;
        // BindLocal은 ModelLoader::SetBoneHierarchy에서 저장한 값입니다.
        const Matrix& BindLocal = Bones[i].BindLocal;

        if (parentIndex == -1) { // 루트 뼈 (ParentIndex가 -1)
            BindGlobal[i] = BindLocal;
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
}

bool SkeletonInfo::DebugValidateBindPoseOnce() const
{
    if (Bones.empty()) return true;

    //indGlobal 계산(부모→자식 누적). 인덱스가 위상정렬이 아닐 수도 있어서 재귀+캐시.
    std::vector<Matrix> bindGlobal(Bones.size(), Matrix::Identity);
    std::vector<char>   done(Bones.size(), 0);

    std::function<const Matrix& (int)> calc = [&](int i) -> const Matrix&
        {
            if (done[i]) return bindGlobal[i];
            int p = Bones[i].ParentIndex;
            const Matrix& L = Bones[i].BindLocal;
            bindGlobal[i] = (p == -1) ? L : (calc(p) * L);
            done[i] = 1;
            return bindGlobal[i];
        };
    for (int i = 0; i < (int)Bones.size(); ++i) calc(i);

    // 2) BindGlobal * Offset  Identity 검증
    bool allOk = true;
    const Matrix I = Matrix::Identity;

    for (int i = 0; i < (int)Bones.size(); ++i) {
        Matrix M = bindGlobal[i] * Bones[i].OffsetTransform;

        int worstIdx = -1;
        float md = MaxAbsDiff(M, I, worstIdx);
        if (md > 1e-3f) {
            allOk = false;
            LOGF("[BindCheck] FAIL bone='%s' idx=%d maxDiff=%.6g at elem=%d\n",
                Bones[i].Name.c_str(), i, md, worstIdx);

            // 번역 누수 요약
            LOGF("  BindGlobal.T = (%.6f, %.6f, %.6f)\n",
                bindGlobal[i]._41, bindGlobal[i]._42, bindGlobal[i]._43);
            LOGF("  Offset.T     = (%.6f, %.6f, %.6f)\n",
                Bones[i].OffsetTransform._41, Bones[i].OffsetTransform._42, Bones[i].OffsetTransform._43);

            //자세한 값이 필요하면 주석 해제
            DumpMatrixRowMajor("BindGlobal", bindGlobal[i]);
            DumpMatrixRowMajor("Offset",     Bones[i].OffsetTransform);
            DumpMatrixRowMajor("M=BindGlobal*Offset", M);
        }
    }

    if (allOk) {
        LOGF("[BindCheck] OK: BindGlobal * Offset = Identity\n");
    }
    return allOk;

}
