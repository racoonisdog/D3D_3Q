#include <shared.fxh>
// ���� ���̴�.
VertexOut main(VertexIn vIn)
{
    VertexOut vOut;
    vOut.posH = mul(float4(vIn.posL, 1.0f), world);   // MUL�� ��� ���ϱ��Դϴ�. ������*�� ��ü�� �䱸�մϴ�.
    vOut.posH = mul(vOut.posH, view);                 // ���� ���� ��� �����ִ� �� ���� ���, ����� ������ �����ϴ�.
    vOut.posH = mul(vOut.posH, projection);                 // cij = aij *bij
    vOut.color = vIn.color;                             // ���⼭ Alpha ä���� ���� �⺻������ 1.0�Դϴ�.

    return vOut;
}