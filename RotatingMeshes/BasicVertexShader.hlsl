#include <shared.fxh>
// ���� ���̴�.
VertexOut main(VertexIn vIn)
{
    VertexOut vOut;
    vOut.posH = mul(float4(vIn.posL, 1.0f), g_World);   // MUL�� ��� ���ϱ��Դϴ�. ������*�� ��ü�� �䱸�մϴ�.
    vOut.posH = mul(vOut.posH, g_View);                 // ���� ���� ��� �����ִ� �� ���� ���, ����� ������ �����ϴ�.
    vOut.posH = mul(vOut.posH, g_Proj);                 // cij = aij *bij
    vOut.color = vIn.color;                             // ���⼭ Alpha ä���� ���� �⺻������ 1.0�Դϴ�.

    return vOut;
}