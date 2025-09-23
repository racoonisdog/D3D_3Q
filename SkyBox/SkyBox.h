#pragma once
#include <d3d11.h>
#include <vector>
#include "../Common/GameApp.h"
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>

//윈도우 스마트 포인터용
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;

struct VertexL
{
	Vector3 position;		// 정점 위치
	Vector3 normalV;		// 정점 노말벡터
	Vector2 Tex;			// 텍스쳐 uv 좌표
	VertexL(Vector3 _position, Vector3 _normal, Vector2 _tex) : position(_position), normalV(_normal), Tex(_tex) {};
};

struct VertexSky
{
	Vector3 position;		// 정점 위치
	VertexSky(Vector3 _position) : position(_position){};
};

struct Constant
{
	XMFLOAT4X4 world;
	XMFLOAT4X4 view;
	XMFLOAT4X4 projection;

	Vector4 vLightDir[2];
	Vector4 vLightColor[2];
	Vector4 vOutputColor;
};

struct SkyConstant
{
	XMFLOAT4X4 world;
	XMFLOAT4X4 view;
	XMFLOAT4X4 projection;
};

class SkyBox : public GameApp
{
public:
	SkyBox(HINSTANCE hInstance);
	~SkyBox();

	// 렌더링 파이프라인을 구성하는 필수 객체의 인터페이스 (  뎊스 스텐실 뷰도 있지만 아직 사용하지 않는다.)
	ID3D11Device* m_pDevice = nullptr;						// 디바이스	
	ID3D11DeviceContext* m_pDeviceContext = nullptr;		// 즉시 디바이스 컨텍스트
	IDXGISwapChain* m_pSwapChain = nullptr;					// 스왑체인
	ID3D11RenderTargetView* m_pRenderTargetView = nullptr;	// 렌더링 타겟뷰
	ID3D11DepthStencilView* m_pDepthStencilView = nullptr;	// 깊이 타겟뷰


	// 렌더링 파이프라인에 적용하는  객체와 정보
	ID3D11VertexShader* m_pVertexShader = nullptr;		// 정점 셰이더.
	ID3D11PixelShader* m_pPixelShader = nullptr;		// 픽셀 셰이더.	
	ID3D11PixelShader* m_pPixelShaderSolid = nullptr;	//단일 색상 픽셀 셰이더
	ID3D11InputLayout* m_pInputLayout = nullptr;		// 입력 레이아웃.
	ID3D11Buffer* m_pVertexBuffer = nullptr;			// 버텍스 버퍼.
	ID3D11Buffer* m_pIndexBuffer = nullptr;				// 인덱스 버퍼.
	ID3D11Buffer* m_pConstantBuffer = nullptr;			// 상수 버퍼.
	ID3D11ShaderResourceView* m_pTextureRV = nullptr;	// 텍스처 리소스 뷰.
	ID3D11SamplerState* m_pSamplerLinear = nullptr;		// 샘플러 상태.
	
	UINT m_VertextBufferStride = 0;					// 버텍스 하나의 크기.
	UINT m_VertextBufferOffset = 0;					// 버텍스 버퍼의 오프셋.
	UINT m_VertexCount = 0;							// 버텍스 개수.
	UINT m_IndexCount = 0;							// 인덱스 개수

	void Reset();
	void SetMatrix();

	//Update에서 갱신할 Matrix들
	XMMATRIX m_World{};
	XMMATRIX m_View{};
	XMMATRIX m_Proj{};
	
	//world 관련 변수 //
	XMFLOAT3 P_position{ 0.0f, 0.0f, 5.0f };
	XMFLOAT3 P_rotation{ 0.0f, 0.0f, 0.0f };
	XMFLOAT3 P_Scale{ 1.0f, 1.0f, 1.0f };


	//카메라 변수
	Matrix m_CWorld;
	Vector3 m_Position;
	Vector3 m_Rotation;


	//카메라 보정함수
	void SanitizeCamera(Vector3& eye, Vector3& target, Vector3& up);

	//Projection 변수
	float fovy = XMConvertToRadians(45.0f);    // 라디안 단위 세로 시야각
	float aspect = 1024.0f / 768.0f;  // width / height
	float zNear = 0.1f;;   // near plane
	float zFar = 1000.0f;    // far plane


	//update를 위해 맴버변수로 둠
	/*std::vector<Constant> ConstantList;*/
	Constant constandices{};
	

	virtual bool Initialize(UINT Width, UINT Height);
	virtual void Update();
	virtual void Render();

	bool InitD3D();
	void UninitD3D();

	bool InitScene();		// 쉐이더,버텍스,인덱스
	bool InitSkyBox();		// 스카이 박스 init용
	void RenderSkyBox();	// 스카이 박스 render용
	void UninitScene();

	//Vertex Buffer + Index Buffer 조합을 쓰기때문에 아래와 같이 따로 필요가 없음
	//Vertex Buffer에는 중복없는 정점 데이터만 넣고 Index Buffer에는 이것들을 어떻게 이을건지에 대한 정보를 담음
	UINT StartSlot = 0;					//연결을 시작할 첫 슬롯의 번호, 여러개의 버퍼에 정보를 나눠서 담을때 사용
	UINT NumBuffers = 0;				//이때 몇개의 버퍼를 이어서 그릴지 필요한 숫자


	// 멤버 변수 (ImGui UI 상태)
	bool  m_show_another_window = false;
	bool  m_show_demo_window = true;
	float m_f = 0.0f;
	int   m_counter = 0;

	// ImGui 초기화/해제 선언
	bool InitImGUI();
	void UninitImGUI();
	void RenderGUI();
	
	float CamPosition[3] = { 0.0f , 0.0f, -10.0f };
	float CamRotation[3] = { 0.0f , 0.0f, 0.0f };
	const float eps = 0.01f;
	float CamFovy = 45.0f;

	float Pspeed = 2.0f;
	float Ch1speed = 4.0f;

	virtual LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

	//빛 관련 변수들
	
	//라이트 색
	XMFLOAT4 m_LightColors[2] =
	{
		XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
		XMFLOAT4(0.0f, 1.0f, 0.0f, 0.0f)
	};
	//라이트 변수
	XMFLOAT4 m_LightDirsEvaluated[2] =
	{
		XMFLOAT4(1.0f , 0.0f, 0.0f , 1.0f),
		XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f)
	};

	float LightColor1[3] = { 1.0f, 1.0f, 1.0f };
	float LightColor2[3] = { 0.0f, 1.0f, 0.0f };
	
	float LightDir1[3] = { 1.0f, 0.0f, 0.0f };
	float LightDir2[3] = { 0.0f, 1.0f, 0.0f };

	ComPtr<ID3D11VertexShader> S_VertexShader;
	ComPtr<ID3D11PixelShader> S_PixelShader;
	ComPtr<ID3D11InputLayout> S_InputLayout;

	ComPtr<ID3D11Buffer> S_VertexBuffer;
	ComPtr<ID3D11Buffer> S_IndexBuffer;
	ComPtr<ID3D11Buffer> S_ConstantBuffer;   // Constant buffer (view/proj 등, 16바이트 배수 정렬)

	ComPtr<ID3D11ShaderResourceView> S_CubeSRV;   // TextureCube
	ComPtr<ID3D11SamplerState> S_Sampler;   // 보통 공용 가능
	
	ComPtr<ID3D11DepthStencilState> S_DepthStencilState;
	ComPtr<ID3D11RasterizerState>   S_RasterizerState;

	UINT S_VertexCount = 0;
	UINT S_IndexCount = 0;
	UINT S_VertexBufferStride = 0;
	UINT S_VertexBufferOffset = 0;
};