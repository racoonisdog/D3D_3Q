#include "AnimationController.h"
#include "AnimSampler.h"

void AnimationController::Update(double dtSec)
{
    if (!m_skel || m_skel->Bones.empty()) return;

    m_timeSec = dtSec;

    const size_t n = m_skel->Bones.size();
    m_LBT.resize(n); m_GBT.resize(n); m_Final.resize(n);

    for (size_t i = 0; i < n; ++i)
    {
        const auto& bone = m_skel->Bones[i];

        // 1) 로컬 변환 LBT: 채널이 있으면 샘플 보간, 없으면 바인드 포즈 유지
        Matrix L;
        if (m_clip) {
            const BoneAnimChannel* ch = m_clip->FindChannel(bone.Name);
            if (ch) {
                Vector3 T, S; Quaternion R;
                AnimSampler::SampleTRS(*m_clip, ch, m_timeSec, T, R, S);
                L = Matrix::CreateScale(S) * Matrix::CreateFromQuaternion(R) * Matrix::CreateTranslation(T);
            }
            else {
                L = bone.BindLocal; // 채널 없으면 바인드 로컬
            }
        }
        else {
            L = bone.BindLocal;     // 클립 없으면 바인드 로컬
        }
        m_LBT[i] = L;

        // 2) 전역 변환 GBT 누적 (부모→자식)
        if (bone.ParentIndex == -1)
            m_GBT[i] = L;
        else
            m_GBT[i] = m_GBT[bone.ParentIndex] * L;

        // 3) 최종 스킨 행렬 Final = GlobalInverseRoot * GBT * Offset
        //m_Final[i] = m_skel->m_GlobalInverseRoot * m_GBT[i] * bone.OffsetTransform;
        m_Final[i] = m_GBT[i] * bone.OffsetTransform;

        // 상수버퍼가 row_major가 아니면 CPU에서 전치
        m_Final[i] = m_Final[i].Transpose();
    }
}

void AnimationController::GetFinalMatrices(Matrix out[128]) const
{
    const size_t n = m_Final.size();
    const size_t lim = (n < 128) ? n : 128;
    for (size_t i = 0; i < lim; ++i) out[i] = m_Final[i];
    for (size_t i = lim; i < 128; ++i) out[i] = Matrix::Identity;
}