#pragma once
#include <d3d11.h>

#include "../Common/GameApp.h"


struct Vertex
{
	Vector3 position;		// 정점 위치
	Vector4 color;			// 정점 하나당 색깔
	Vertex(Vector3 _position, Vector4 _color) : position(_position), color(_color) {};
};

struct Constant
{
	DirectX::XMMATRIX world;
	DirectX::XMMATRIX view;				
	DirectX::XMMATRIX projection;
};


class Meshe : public GameApp
{
public:
	Meshe(HINSTANCE hInstance);
	~Meshe();

	// 렌더링 파이프라인을 구성하는 필수 객체의 인터페이스 (  뎊스 스텐실 뷰도 있지만 아직 사용하지 않는다.)
	ID3D11Device* m_pDevice = nullptr;						// 디바이스	
	ID3D11DeviceContext* m_pDeviceContext = nullptr;		// 즉시 디바이스 컨텍스트
	IDXGISwapChain* m_pSwapChain = nullptr;					// 스왑체인
	ID3D11RenderTargetView* m_pRenderTargetView = nullptr;	// 렌더링 타겟뷰


	// 렌더링 파이프라인에 적용하는  객체와 정보
	ID3D11VertexShader* m_pVertexShader = nullptr;	// 정점 셰이더.
	ID3D11PixelShader* m_pPixelShader = nullptr;	// 픽셀 셰이더.	
	ID3D11InputLayout* m_pInputLayout = nullptr;	// 입력 레이아웃.
	ID3D11Buffer* m_pVertexBuffer = nullptr;		// 버텍스 버퍼.
	ID3D11Buffer* m_pIndexBuffer = nullptr;			// 인덱스 버퍼.
	ID3D11Buffer* m_pConstantBuffer = nullptr;		// 상수 버퍼.
	UINT m_VertextBufferStride = 0;					// 버텍스 하나의 크기.
	UINT m_VertextBufferOffset = 0;					// 버텍스 버퍼의 오프셋.
	UINT m_VertexCount = 0;							// 버텍스 개수.
	UINT m_IndexCount = 0;							// 인덱스 개수


	//world 관련 변수 ( 자식 추가 해야함 )
	XMFLOAT3 P_position{ 0.0f, 0.0f, 0.0f };
	XMFLOAT3 P_rotation{ 0.0f, 0.0f, 0.0f };
	XMFLOAT3 P_Scale{ 1.0f, 1.0f, 1.0f };

	XMMATRIX P_world = XMMatrixIdentity();
	//XMMatrixIdentity()는 아래와 같이 초기화하는 함수
	//XMMatrixScaling(1.0f, 1.0f, 1.0f) *
	//XMMatrixRotationY(0.0f) *
	//XMMatrixTranslation(0.0f, 0.0f, 0.0f);;

	//카메라 임시 변수


	//Projection 변수
	float fovy = XMConvertToRadians(45.0f);    // 라디안 단위 세로 시야각
	float aspect = 1024.0f / 768.0f;  // width / height
	float zNear = 0.1f;;   // near plane
	float zFar = 1000.0f;    // far plane

	XMMATRIX m_projection = XMMatrixPerspectiveFovLH(
		fovy,
		aspect,
		zNear,
		zFar
	);

	//update를 위해 맴버변수로 둠
	Constant constandices{};
	

	virtual bool Initialize(UINT Width, UINT Height);
	virtual void Update();
	virtual void Render();

	bool InitD3D();
	void UninitD3D();

	bool InitScene();		// 쉐이더,버텍스,인덱스
	void UninitScene();

	//Vertex Buffer + Index Buffer 조합을 쓰기때문에 아래와 같이 따로 필요가 없음
	//Vertex Buffer에는 중복없는 정점 데이터만 넣고 Index Buffer에는 이것들을 어떻게 이을건지에 대한 정보를 담음
	UINT StartSlot = 0;					//연결을 시작할 첫 슬롯의 번호, 여러개의 버퍼에 정보를 나눠서 담을때 사용
	UINT NumBuffers = 0;				//이때 몇개의 버퍼를 이어서 그릴지 필요한 숫자
};