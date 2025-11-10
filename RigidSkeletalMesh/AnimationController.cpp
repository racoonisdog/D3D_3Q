#include "AnimationController.h"
#include "AnimSampler.h"

void AnimationController::Update(double dtSec)
{

    if (!m_skel || m_skel->Bones.empty()) return;


    m_timeSec = dtSec; 

    //if (m_clip)
    //{
    //    const double dur = m_clip->GetDurationTicks();
    //    if (dur > 0.0) m_timeSec = fmod(m_timeSec, dur);
    //}

    const size_t n = m_skel->Bones.size();
    if (m_LBT.size() != n) { m_LBT.assign(n, Matrix::Identity); }
    if (m_GBT.size() != n) { m_GBT.assign(n, Matrix::Identity); }
    if (m_Final.size() != n) { m_Final.assign(n, Matrix::Identity); }

    for (size_t i = 0; i < n; ++i)
    {
        const auto& bone = m_skel->Bones[i];
        //로컬 변환 LBT: 채널이 있으면 샘플 보간, 없으면 바인드 포즈 유지
        Matrix L;
        const BoneAnimChannel* ch = nullptr;
        if (m_clip) {
            ch = m_clip->FindChannel(bone.Name);
            if (ch) {
                Vector3 T, S; Quaternion R;
                AnimSampler::SampleTRS(*m_clip, ch, m_timeSec, T, R, S);

                auto e_T = Matrix::CreateTranslation(T);
                auto e_r = Matrix::CreateFromQuaternion(R);
                auto e_s = Matrix::CreateScale(S);

                L = e_s * e_r * e_T;
                int a = 0;
            }
            else {
                L = bone.BindLocal; // 채널 없으면 바인드 로컬 유지
            }
        }
        else {
           L = bone.BindLocal;     // 클립 없으면 바인드 로컬
        }

        m_LBT[i] = L;

        //전역 변환 GBT 누적 (부모→자식)
        if (bone.ParentIndex == -1) { m_GBT[i] = L; }
        else if (bone.ParentIndex >= 0 && bone.ParentIndex < m_GBT.size())
        {
            //m_GBT[i] = m_GBT[bone.ParentIndex] * L;
            m_GBT[i] = L * m_GBT[bone.ParentIndex];
        }
        else
        {
            // 인덱스가 유효하지 않으면 부모의 GBT를 사용할 수 없기때문에 if구문으로 나누기
            m_GBT[i] = L;
        }

        //최종 스킨 행렬 Final = GlobalInverseRoot * GBT * Offset
        //m_Final[i] = m_GBT[i] * bone.OffsetTransform;
        m_Final[i] = bone.OffsetTransform * m_GBT[i];

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