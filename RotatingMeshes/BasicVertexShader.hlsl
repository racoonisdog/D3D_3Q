#include <shared.fxh>
// ���� ���̴�.
VShaderOut main(VShaderIn vIn)
{
    VShaderOut VerTOut;
	//mul �Լ��� hlsl���� �����ϴ� ���/���� ���� �Լ�
	//����� ���ǰ� ����ִ� shared.fxh�� world , view , projection ��Ʈ������ ������ �������� mul �Լ��� ���Ͽ� Ŭ�� �������� ��ȯ����
    VerTOut.pos = mul(float4(vIn.pos, 1.0f), world);		// ������Ʈ ���� -> ���� �������� , ������ǥ�� Vector3���� �ޱ� ������ ������ǥ��� ��ȯ(w�� 1��)
    VerTOut.pos = mul(VerTOut.pos, view);                 // ���� ���� -> ī�޶� ����
    VerTOut.pos = mul(VerTOut.pos, projection);           // ī�޶� ���� -> Ŭ�� ����
    VerTOut.color = vIn.color;

    return VerTOut;
}