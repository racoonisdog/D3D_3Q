#include "BlinnPhong.h"
#include "../Common/Helper.h"
#include <directxtk/simplemath.h>
#include <d3dcompiler.h>
#include <Directxtk/DDSTextureLoader.h>

#pragma comment (lib, "d3d11.lib")
#pragma comment(lib,"d3dcompiler.lib")

using namespace DirectX::SimpleMath;

/*
	  6-----7
	 /|    /|
	1-+---3-+
	| |   | |
	| 4---|-5
	|/    |/
	0-----2
*/


BlinnPhong::BlinnPhong(HINSTANCE hInstance) : GameApp(hInstance)
{

}


BlinnPhong::~BlinnPhong()
{
	UninitScene();
	UninitD3D();
}

void BlinnPhong::Reset()
{
	m_World = Matrix::Identity;
	m_CWorld = Matrix::Identity;
	m_Rotation = Vector3(0.0f, 0.0f, 0.0f);
	m_Position = Vector3(0.0f, 0.0f, -10.0f);
}

void BlinnPhong::SetMatrix()
{
	//부모는 World matrix = local matrix 이므로 W_ 변수명
	XMMATRIX S = XMMatrixScaling(P_Scale.x, P_Scale.y, P_Scale.z);
	XMMATRIX R = XMMatrixRotationRollPitchYaw(P_rotation.x, P_rotation.y, P_rotation.z);
	XMMATRIX T = XMMatrixTranslation(P_position.x, P_position.y, P_position.z);
	m_World = S * R * T;

	//view(카메라 수치)
	Vector3 eye = m_CWorld.Translation();
	Vector3 target = m_CWorld.Translation() + m_CWorld.Backward();
	Vector3 up = m_CWorld.Up();
	SanitizeCamera(eye, target, up);
	m_View = XMMatrixLookAtLH(eye, target, up);

	//projection 설정
	m_Proj = XMMatrixPerspectiveFovLH(fovy, aspect, zNear, zFar);
}


void BlinnPhong::SanitizeCamera(Vector3& eye, Vector3& target, Vector3& up)
{
	 // eye == target 방지
    if ((eye - target).LengthSquared() < 1e-12f) {
        target = eye + Vector3(0,0,1); // +Z 기준
    }

    // up 정규화 및 forward와 평행 방지
    Vector3 fwd = (target - eye); 
    if (fwd.LengthSquared() < 1e-12f) fwd = Vector3(0,0,1);
    fwd.Normalize();

    if (up.LengthSquared() < 1e-12f) up = Vector3(0,1,0);
    up.Normalize();

    // up이 forward와 너무 나란하면 살짝 비틀기
	if (fabsf(fwd.Dot(up)) > 0.999f) {
		up = fabsf(fwd.y) < 0.9f ? Vector3(0, 1, 0) : Vector3(1, 0, 0);
	}
}

bool BlinnPhong::Initialize(UINT Width, UINT Height)
{
	__super::Initialize(Width, Height);

	if (!InitD3D())
		return false;

	if (!InitImGUI())
		return false;

	if (!InitScene())
		return false;

	if (!InitSkyBox())
		return false;

	return true;
}

void BlinnPhong::Update()
{
	__super::Update();
	SetMatrix();
}

void BlinnPhong::Render()
{
	float color[4] = { 0.0f, 0.5f, 0.5f, 1.0f };


	// 컬러 버퍼(RTV) 초기화
	m_pDeviceContext->ClearRenderTargetView(m_pRenderTargetView, color);
	//깊이 버퍼초기화
	m_pDeviceContext->ClearDepthStencilView(m_pDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);

	/*Constant cb1;*/
	XMStoreFloat4x4(&constandices.world, XMMatrixTranspose(m_World));
	XMStoreFloat4x4(&constandices.view, XMMatrixTranspose(m_View));
	XMStoreFloat4x4(&constandices.projection, XMMatrixTranspose(m_Proj));
	XMMATRIX WInvT = XMMatrixTranspose(XMMatrixInverse(nullptr, m_World));
	XMStoreFloat4x4(&constandices.worldinverseT, WInvT);
	constandices.vLightDir = m_LightDirsEvaluated;
	constandices.vLightColor = m_LightColors;
	constandices.vOutputColor = XMFLOAT4(0, 0, 0, 0);

	Vector3 tempPos = m_CWorld.Translation();
	constandices.camPos = { tempPos.x, tempPos.y, tempPos.z };




	// Draw계열 함수를 호출하기전에 렌더링 파이프라인에 필수 스테이지 설정을 해야한다.	
	m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST); // 정점을 이어서 그릴 방식 설정.

	// 정점버퍼를 바인딩 해주는 단계
	m_pDeviceContext->IASetVertexBuffers(0, 1, &m_pVertexBuffer, &m_VertextBufferStride, &m_VertextBufferOffset);
	//입력 레이아웃 바인딩
	m_pDeviceContext->IASetInputLayout(m_pInputLayout);

	// 인덱스 버퍼 바인딩
	m_pDeviceContext->IASetIndexBuffer(m_pIndexBuffer, DXGI_FORMAT_R32_UINT, 0);

	
	// 버텍스 셰이더 바인딩
	m_pDeviceContext->VSSetShader(m_pVertexShader, nullptr, 0);
	// 픽셀 셰이더 바인딩
	m_pDeviceContext->PSSetShader(m_pPixelShader, nullptr, 0);

	//임시 컨테이너 생성, Map 결과를 담을 임시 구조체
	D3D11_MAPPED_SUBRESOURCE mapped{};
	//Map -> GPU의 이전 메모리 블록 주소는 버리고 새로 넘겨줄 데이터를 담을 메모리 블록 주소를 CPU에 넘겨줌 ( 이 주소를 mapped 에 담음)
	//CPU 기준으로는 이전 메모리의 주소를 잃어버려 이전 값이 폐기, GPU기준으로는 이전 데이터를 내부적으로 보유하고 있어서 그리기 가능
	//밑의 상수 버퍼에서 D3D11_USAGE_DYNAMIC 설정했다는 가정하에
	m_pDeviceContext->Map(m_pConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
	
	memcpy(mapped.pData, &constandices, sizeof(constandices));
	//D3D11_MAP_WRITE_DISCARD는 내부적으로 버퍼 리네이밍(새 메모리 조각 교체)을 유도하기때문에 여기서만 사용
	//-> 카피해준후 Unmap
	m_pDeviceContext->Unmap(m_pConstantBuffer, 0);
	//상수버퍼 바인딩

	m_pDeviceContext->Map(m_tConstantBuffer , 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
	memcpy(mapped.pData, &materialconstand, sizeof(materialconstand));
	m_pDeviceContext->Unmap(m_tConstantBuffer, 0);

	m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_pConstantBuffer);
	m_pDeviceContext->PSSetConstantBuffers(0, 1, &m_pConstantBuffer);
	m_pDeviceContext->PSSetConstantBuffers(1, 1, &m_tConstantBuffer);
	m_pDeviceContext->PSSetShaderResources(0, 1, &m_pTextureRV);
	m_pDeviceContext->PSSetSamplers(0, 1, &m_pSamplerLinear);


	//m_pDeviceContext->Draw(m_VertexCount, 0);				//인덱스 미사용시
	m_pDeviceContext->DrawIndexed(m_IndexCount, 0, 0);		//인덱스 사용시
	


	RenderSkyBox();
	RenderGUI();
	// Present the information rendered to the back buffer to the front buffer (the screen)
	m_pSwapChain->Present(0, 0);
}

bool BlinnPhong::InitD3D()
{
	// 초기화
	Reset();
	// 결과값.
	HRESULT hr = 0;

	// 스왑체인 속성 설정 구조체 생성.
	DXGI_SWAP_CHAIN_DESC swapDesc = {};
	swapDesc.BufferCount = 1;
	swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapDesc.OutputWindow = m_hWnd;	// 스왑체인 출력할 창 핸들 값.
	swapDesc.Windowed = true;		// 창 모드 여부 설정.
	swapDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	// 백버퍼(텍스처)의 가로/세로 크기 설정.
	swapDesc.BufferDesc.Width = m_ClientWidth;
	swapDesc.BufferDesc.Height = m_ClientHeight;
	// 화면 주사율 설정.
	swapDesc.BufferDesc.RefreshRate.Numerator = 60;
	swapDesc.BufferDesc.RefreshRate.Denominator = 1;
	// 샘플링 관련 설정.
	swapDesc.SampleDesc.Count = 1;
	swapDesc.SampleDesc.Quality = 0;

	UINT creationFlags = 0;
#ifdef _DEBUG
	creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
	// 1. 장치 생성.   2.스왑체인 생성. 3.장치 컨텍스트 생성.
	HR_T(D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, creationFlags, NULL, NULL,
		D3D11_SDK_VERSION, &swapDesc, &m_pSwapChain, &m_pDevice, NULL, &m_pDeviceContext));

	// 4. 렌더타겟뷰 생성.  (백버퍼를 이용하는 렌더타겟뷰)	
	ID3D11Texture2D* pBackBufferTexture = nullptr;
	HR_T(m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBufferTexture));
	HR_T(m_pDevice->CreateRenderTargetView(pBackBufferTexture, NULL, &m_pRenderTargetView));  // 텍스처는 내부 참조 증가
	SAFE_RELEASE(pBackBufferTexture);	//외부 참조 카운트를 감소시킨다.
	// 렌더 타겟을 최종 출력 파이프라인에 바인딩합니다.
	/*m_pDeviceContext->OMSetRenderTargets(1, &m_pRenderTargetView, NULL);*/

	// 뷰포트 설정.	
	D3D11_VIEWPORT viewport = {};
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.Width = (float)m_ClientWidth;
	viewport.Height = (float)m_ClientHeight;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	m_pDeviceContext->RSSetViewports(1, &viewport);
	return true;
}

void BlinnPhong::UninitD3D()
{
	SAFE_RELEASE(m_pRenderTargetView);
	SAFE_RELEASE(m_pSwapChain);
	SAFE_RELEASE(m_pDeviceContext);
	SAFE_RELEASE(m_pDevice);
	UninitImGUI();
}

void BlinnPhong::UninitSkyBox()
{
	S_VertexShader = nullptr;
	S_PixelShader = nullptr;;
	S_InputLayout = nullptr;

	S_VertexBuffer = nullptr;
	S_IndexBuffer = nullptr;
	S_ConstantBuffer = nullptr;

	S_CubeSRV = nullptr;
	S_Sampler = nullptr;

	S_DepthStencilState = nullptr;
	S_RasterizerState = nullptr;
}

bool BlinnPhong::InitScene()
{
	HRESULT hr = 0; // 결과값.
	ID3D10Blob* errorMessage = nullptr;	 // 컴파일 에러 메시지가 저장될 버퍼.	

	//정점하나에 법선 3개를 담을 수 없기 때문에 중복된 정점 버퍼도 정의해줘야함
	VertexL vertices[] =
	{
		//Noarmal Y+
		VertexL(Vector3(-1.0f, 1.0f, -1.0f), Vector3(0.0f, 1.0f, 0.0f), Vector2(1.0f, 0.0f)),
		VertexL(Vector3(1.0f, 1.0f, -1.0f), Vector3(0.0f, 1.0f, 0.0f), Vector2(0.0f, 0.0f)),
		VertexL(Vector3(1.0f, 1.0f, 1.0f), Vector3(0.0f, 1.0f, 0.0f), Vector2(0.0f, 1.0f)),
		VertexL(Vector3(-1.0f, 1.0f, 1.0f), Vector3(0.0f, 1.0f, 0.0f), Vector2(1.0f, 1.0f)),
		//Normal Y-
		VertexL(Vector3(-1.0f, -1.0f, -1.0f), Vector3(0.0f, -1.0f, 0.0f), Vector2(0.0f, 0.0f)),
		VertexL(Vector3(1.0f, -1.0f, -1.0f), Vector3(0.0f, -1.0f, 0.0f), Vector2(1.0f, 0.0f)),
		VertexL(Vector3(1.0f, -1.0f, 1.0f), Vector3(0.0f, -1.0f, 0.0f), Vector2(1.0f, 1.0f)),
		VertexL(Vector3(-1.0f, -1.0f, 1.0f), Vector3(0.0f, -1.0f, 0.0f), Vector2(0.0f, 1.0f)),
		//Normal X-
		VertexL(Vector3(-1.0f, -1.0f, 1.0f), Vector3(-1.0f, 0.0f, 0.0f), Vector2(0.0f, 1.0f)),
		VertexL(Vector3(-1.0f, -1.0f, -1.0f), Vector3(-1.0f, 0.0f, 0.0f), Vector2(1.0f, 1.0f)),
		VertexL(Vector3(-1.0f, 1.0f, -1.0f), Vector3(-1.0f, 0.0f, 0.0f), Vector2(1.0f, 0.0f)),
		VertexL(Vector3(-1.0f, 1.0f, 1.0f), Vector3(-1.0f, 0.0f, 0.0f), Vector2(0.0f, 0.0f)),
		//Normal X+
		VertexL(Vector3(1.0f, -1.0f, 1.0f), Vector3(1.0f, 0.0f, 0.0f), Vector2(1.0f, 1.0f)),
		VertexL(Vector3(1.0f, -1.0f, -1.0f), Vector3(1.0f, 0.0f, 0.0f), Vector2(0.0f, 1.0f)),
		VertexL(Vector3(1.0f, 1.0f, -1.0f), Vector3(1.0f, 0.0f, 0.0f), Vector2(0.0f, 0.0f)),
		VertexL(Vector3(1.0f, 1.0f, 1.0f), Vector3(1.0f, 0.0f, 0.0f), Vector2(1.0f, 0.0f)),
		//Normal Z-
		VertexL(Vector3(-1.0f, -1.0f, -1.0f), Vector3(0.0f, 0.0f, -1.0f), Vector2(0.0f, 1.0f)),
		VertexL(Vector3(1.0f, -1.0f, -1.0f), Vector3(0.0f, 0.0f, -1.0f), Vector2(1.0f, 1.0f)),
		VertexL(Vector3(1.0f, 1.0f, -1.0f), Vector3(0.0f, 0.0f, -1.0f), Vector2(1.0f, 0.0f)),
		VertexL(Vector3(-1.0f, 1.0f, -1.0f), Vector3(0.0f, 0.0f, -1.0f), Vector2(0.0f, 0.0f)),
		//Normal Z+
		VertexL(Vector3(-1.0f, -1.0f, 1.0f), Vector3(0.0f, 0.0f, 1.0f), Vector2(1.0f, 1.0f)),
		VertexL(Vector3(1.0f, -1.0f, 1.0f), Vector3(0.0f, 0.0f, 1.0f), Vector2(0.0f, 1.0f)),
		VertexL(Vector3(1.0f, 1.0f, 1.0f), Vector3(0.0f, 0.0f, 1.0f), Vector2(0.0f, 0.0f)),
		VertexL(Vector3(-1.0f, 1.0f, 1.0f), Vector3(0.0f, 0.0f, 1.0f), Vector2(1.0f, 0.0f)),
	};

	D3D11_BUFFER_DESC vbDesc = {};
	m_VertexCount = ARRAYSIZE(vertices);	// 정점의 수
	vbDesc.ByteWidth = sizeof(VertexL) * m_VertexCount; // 버텍스 버퍼의 크기(Byte).
	vbDesc.CPUAccessFlags = 0;
	vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER; // 정점 버퍼로 사용.
	vbDesc.MiscFlags = 0;
	vbDesc.Usage = D3D11_USAGE_DEFAULT;	// CPU는 접근불가 ,  GPU에서 읽기/쓰기 가능한 버퍼로 생성.

	// 정점 버퍼 생성.
	D3D11_SUBRESOURCE_DATA vbData = {};
	vbData.pSysMem = vertices;	// 버퍼를 생성할때 복사할 데이터의 주소 설정 
	HR_T(hr = m_pDevice->CreateBuffer(&vbDesc, &vbData, &m_pVertexBuffer));


	// 인덱스 리스트
	// index를 그릴때 순열이 중요함 ( 그리는 방향이 같아도 순열이 다르면 회전시 다른 방향이 될 수 있다 )
	UINT indices[] = {
		//Y+
		3,0,1, 2,3,1,
		//Y-
		6,5,4, 7,6,4,
		//X-
		11,8,9, 10,11,9,
		//X+
		14,13,12, 15,14,12,
		//Z-
		19,16,17, 18,19,17,
		//Z+
		22,21,20, 23,22,20
	};

	m_IndexCount = ARRAYSIZE(indices);	// 인덱스 수
	D3D11_BUFFER_DESC idDesc = {};
	idDesc.ByteWidth = sizeof(UINT) * m_IndexCount; // 인덱스 버퍼의 크기(Byte).
	idDesc.CPUAccessFlags = 0;
	idDesc.BindFlags = D3D11_BIND_INDEX_BUFFER; // 인덱스 버퍼로 사용.
	idDesc.MiscFlags = 0;
	idDesc.Usage = D3D11_USAGE_DEFAULT;	// CPU는 접근불가 ,  GPU에서 읽기/쓰기 가능한 버퍼로 생성.


	// 인덱스 버퍼 생성.
	D3D11_SUBRESOURCE_DATA idData = {};
	idData.pSysMem = indices;	// 버퍼를 생성할때 복사할 데이터의 주소 설정 
	HR_T(hr = m_pDevice->CreateBuffer(&idDesc, &idData, &m_pIndexBuffer));


	// 버텍스 버퍼 정보 
	m_VertextBufferStride = sizeof(VertexL); // 버텍스 하나의 크기
	m_VertextBufferOffset = 0;	// 버텍스 시작 주소에서 더할 오프셋 주소

	// 2. Render에서 파이프라인에 바인딩할  버텍스 셰이더 생성
	ID3DBlob* vertexShaderBuffer = nullptr; // 버텍스 세이더 HLSL의 컴파일된 결과(바이트코드)를 담을수 있는 버퍼 객체
	HR_T(CompileShaderFromFile(L"BasicVertexShader.hlsl", "main", "vs_4_0", &vertexShaderBuffer));
	HR_T(m_pDevice->CreateVertexShader(vertexShaderBuffer->GetBufferPointer(), // 필요한 데이터를 복사하며 객체 생성 
		vertexShaderBuffer->GetBufferSize(), NULL, &m_pVertexShader));

	// 3. Render() 에서 파이프라인에 바인딩할 InputLayout 생성 	
	D3D11_INPUT_ELEMENT_DESC layout[] =  // 인풋 레이아웃은 버텍스 쉐이더가 입력받을 데이터의 형식을 지정한다.
	{// SemanticName , SemanticIndex , Format , InputSlot , AlignedByteOffset , InputSlotClass , InstanceDataStepRate		
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{ "TEXCOORD",    0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0}
	};
	// 버텍스 셰이더의 Input에 지정된 내용과 같은지 검증하면서 InputLayout을 생성한다.
	HR_T(hr = m_pDevice->CreateInputLayout(layout, ARRAYSIZE(layout),
		vertexShaderBuffer->GetBufferPointer(), vertexShaderBuffer->GetBufferSize(), &m_pInputLayout));

	SAFE_RELEASE(vertexShaderBuffer); // 복사했으니 버퍼는 해제 가능
	HR_T(CreateDDSTextureFromFile(m_pDevice, L"../resource/single/test.dds", nullptr, &m_pTextureRV));

	//객체생성
	D3D11_SAMPLER_DESC sampDesc = {};
	//텍스처를 어떻게 보간해서 읽을지 지정 ( MIN: 멀리서볼때 , MAG: 가까이서 볼때, MIP: 밉맵 사이 전환할때 )
	// 조합에 따라 다르게 씀
	sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	//텍스처 좌표가 0~1 범위를 벗어났을때 어떻게 처리할지 결정
	//U, V, W 는 각각 텍스처의 3차원 축 ( 2D 텍스처라면 U/V만 사용)
	//WRAP: 1.2->0.2로 바꿔서 텍스처를 타일처럼 반복,  CLAMP:1.2 ->1.0(가장자리 색 유지) , MIRROR:1.2->0.8(대칭 반복)
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	//샘플러가 비교모드로 동작할때 사용, 보통 섀도우 맵(그림자 계산용)에서 SampleCmp함수로 깊이 비교할때 사용, Never은 비교기능 사용 X -> 일반 텍스처 샘플링 용도로 사용
	sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	//샘플링할 수 있는 최소 LOD 지정, 0 -> 밉맵 레벨 0(가장 고해상도)부터 사용가능
	sampDesc.MinLOD = 0;
	// 샘플링 할 수 있는 최대 LOD 지정  Max-> 제한없음 (가장 낮은 해상도 밉맵까지 다 사용 가능)
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

	HR_T(m_pDevice->CreateSamplerState(&sampDesc, &m_pSamplerLinear));


	// 4. Render에서 파이프라인에 바인딩할 픽셀 셰이더 생성
	ID3DBlob* pixelShaderBuffer = nullptr; // 픽셀 세이더 HLSL의 컴파일된 결과(바이트코드)를 담을수 있는 버퍼 객체
	HR_T(CompileShaderFromFile(L"BasicPixelShader.hlsl", "main", "ps_4_0", &pixelShaderBuffer));
	HR_T(m_pDevice->CreatePixelShader(	  // 필요한 데이터를 복사하며 객체 생성 
		pixelShaderBuffer->GetBufferPointer(),
		pixelShaderBuffer->GetBufferSize(), NULL, &m_pPixelShader));
	//SAFE_RELEASE(pixelShaderBuffer); // 복사했으니 버퍼는 해제 가능

	// 단일색상 픽셸 셰이더 생성
	HR_T(CompileShaderFromFile(L"BasicPixelShader.hlsl", "PSSolid", "ps_4_0", &pixelShaderBuffer));
	HR_T(m_pDevice->CreatePixelShader(pixelShaderBuffer->GetBufferPointer(),
		pixelShaderBuffer->GetBufferSize(), NULL, &m_pPixelShaderSolid));
	SAFE_RELEASE(pixelShaderBuffer);

	constandices.lightambient = { 0.04f, 0.04f, 0.04f, 1.0f }; // 환경광
	constandices.lightdiffuse = { 1.00f, 1.00f, 1.00f, 1.0f }; // 난반사 색
	constandices.lightspecular = { 1.00f, 1.00f, 1.00f, 1.0f }; // 정반사 색
	materialconstand.matambient = { 1.0f, 1.0f, 1.0f, 1.0f };
	materialconstand.matdiffuse = { 1.0f, 1.0f, 1.0f, 1.0f };
	materialconstand.matspecular = { 0.04f, 0.04f, 0.04f, 1.0f }; // ks(비금속 기본치)
	constandices.shininess = 2000.0f;

	D3D11_BUFFER_DESC cbDesc{};
	//상수버퍼는 보통 구조체 1개 크기로 만들기 때문에 아래와 같이 설정
	cbDesc.ByteWidth = (sizeof(Constant) + 15) & ~15;;
	//자주 갱신하는 상수 버퍼 옵션
	cbDesc.Usage = D3D11_USAGE_DYNAMIC;
	//상수버퍼로 쓰겠다는 선언
	cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	//D3D11_MAPPED_SUBRESOURCE 을 사용할때는 아래
	cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	//Map 없이 하는 방식 UpdateSubresource를 사용할때는 아래옵션
	/*cbDesc.CPUAccessFlags = 0;*/
	cbDesc.MiscFlags = 0;
	cbDesc.StructureByteStride = 0;

	D3D11_SUBRESOURCE_DATA idDataP{};
	idData.pSysMem = indices;	// 버퍼를 생성할때 복사할 데이터의 주소 설정 
	HR_T(hr = m_pDevice->CreateBuffer(&cbDesc, nullptr, &m_pConstantBuffer));
	cbDesc.ByteWidth = (sizeof(Material) + 15) & ~15;;
	HR_T(hr = m_pDevice->CreateBuffer(&cbDesc, nullptr, &m_tConstantBuffer));

	//6. 뎊스&스텐실 뷰 생성
	D3D11_TEXTURE2D_DESC descDepth = {};
	descDepth.Width = m_ClientWidth;
	descDepth.Height = m_ClientHeight;
	descDepth.MipLevels = 1;
	descDepth.ArraySize = 1;
	descDepth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	descDepth.SampleDesc.Count = 1;
	descDepth.SampleDesc.Quality = 0;
	descDepth.Usage = D3D11_USAGE_DEFAULT;
	descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	descDepth.CPUAccessFlags = 0;
	descDepth.MiscFlags = 0;

	ID3D11Texture2D* textureDepthStencil = nullptr;
	HR_T(m_pDevice->CreateTexture2D(&descDepth, nullptr, &textureDepthStencil));

	// Create the depth stencil view
	D3D11_DEPTH_STENCIL_VIEW_DESC descDSV = {};
	descDSV.Format = descDepth.Format;
	descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	descDSV.Texture2D.MipSlice = 0;
	HR_T(m_pDevice->CreateDepthStencilView(textureDepthStencil, &descDSV, &m_pDepthStencilView));
	m_pDeviceContext->OMSetRenderTargets(1, &m_pRenderTargetView, m_pDepthStencilView);
	SAFE_RELEASE(textureDepthStencil);

	return true;
}

bool BlinnPhong::InitSkyBox()
{
	HRESULT hr = 0; // 결과값.
	ID3D10Blob* errorMessage = nullptr;	 // 컴파일 에러 메시지가 저장될 버퍼.	

	//정점하나에 법선 3개를 담을 수 없기 때문에 중복된 정점 버퍼도 정의해줘야함
	VertexSky vertices[] =
	{
		//Noarmal Y+
		VertexSky(Vector3(-1.0f, 1.0f, -1.0f)),
		VertexSky(Vector3(1.0f, 1.0f, -1.0f)),
		VertexSky(Vector3(1.0f, 1.0f, 1.0f)),
		VertexSky(Vector3(-1.0f, 1.0f, 1.0f)),
		//Normal Y-
		VertexSky(Vector3(-1.0f, -1.0f, -1.0f)),
		VertexSky(Vector3(1.0f, -1.0f, -1.0f)),
		VertexSky(Vector3(1.0f, -1.0f, 1.0f)),
		VertexSky(Vector3(-1.0f, -1.0f, 1.0f)),
		//Normal X-
		VertexSky(Vector3(-1.0f, -1.0f, 1.0f)),
		VertexSky(Vector3(-1.0f, -1.0f, -1.0f)),
		VertexSky(Vector3(-1.0f, 1.0f, -1.0f)),
		VertexSky(Vector3(-1.0f, 1.0f, 1.0f)),
		//Normal X+
		VertexSky(Vector3(1.0f, -1.0f, 1.0f)),
		VertexSky(Vector3(1.0f, -1.0f, -1.0f)),
		VertexSky(Vector3(1.0f, 1.0f, -1.0f)),
		VertexSky(Vector3(1.0f, 1.0f, 1.0f)),
		//Normal Z-
		VertexSky(Vector3(-1.0f, -1.0f, -1.0f)),
		VertexSky(Vector3(1.0f, -1.0f, -1.0f)),
		VertexSky(Vector3(1.0f, 1.0f, -1.0f)),
		VertexSky(Vector3(-1.0f, 1.0f, -1.0f)),
		//Normal Z+
		VertexSky(Vector3(-1.0f, -1.0f, 1.0f)),
		VertexSky(Vector3(1.0f, -1.0f, 1.0f)),
		VertexSky(Vector3(1.0f, 1.0f, 1.0f)),
		VertexSky(Vector3(-1.0f, 1.0f, 1.0f))
	};

	D3D11_BUFFER_DESC vbDesc = {};
	S_VertexCount = ARRAYSIZE(vertices);	// 정점의 수
	vbDesc.ByteWidth = sizeof(VertexSky) * S_VertexCount; // 버텍스 버퍼의 크기(Byte).
	vbDesc.CPUAccessFlags = 0;
	vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER; // 정점 버퍼로 사용.
	vbDesc.MiscFlags = 0;
	vbDesc.Usage = D3D11_USAGE_DEFAULT;	// CPU는 접근불가 ,  GPU에서 읽기/쓰기 가능한 버퍼로 생성.

	// 정점 버퍼 생성.
	D3D11_SUBRESOURCE_DATA vbData = {};
	vbData.pSysMem = vertices;	// 버퍼를 생성할때 복사할 데이터의 주소 설정 
	HR_T(hr = m_pDevice->CreateBuffer(&vbDesc, &vbData, S_VertexBuffer.GetAddressOf()));

	// 인덱스 리스트 ( 반시계 방향 )
	UINT indices[] = {
		// Y+
		1,0,3, 1,3,2,
		// Y-
		4,5,6, 4,6,7,
		// X-
		9,8,11, 9,11,10,
		// X+
		12,13,14, 12,14,15,
		// Z-
		17,16,19, 17,19,18,
		// Z+
		20,21,22, 20,22,23
	};



	S_IndexCount = ARRAYSIZE(indices);	// 인덱스 수
	D3D11_BUFFER_DESC idDesc = {};
	idDesc.ByteWidth = sizeof(UINT) * S_IndexCount; // 인덱스 버퍼의 크기(Byte).
	idDesc.CPUAccessFlags = 0;
	idDesc.BindFlags = D3D11_BIND_INDEX_BUFFER; // 인덱스 버퍼로 사용.
	idDesc.MiscFlags = 0;
	idDesc.Usage = D3D11_USAGE_DEFAULT;	// CPU는 접근불가 ,  GPU에서 읽기/쓰기 가능한 버퍼로 생성.


	// 인덱스 버퍼 생성.
	D3D11_SUBRESOURCE_DATA idData = {};
	idData.pSysMem = indices;	// 버퍼를 생성할때 복사할 데이터의 주소 설정 
	HR_T(hr = m_pDevice->CreateBuffer(&idDesc, &idData, S_IndexBuffer.GetAddressOf()));


	// 버텍스 버퍼 정보 
	S_VertexBufferStride = sizeof(VertexSky); // 버텍스 하나의 크기
	S_VertexBufferOffset = 0;	// 버텍스 시작 주소에서 더할 오프셋 주소

	
	ComPtr<ID3DBlob> vertexShaderBuffer = nullptr;
	HR_T(CompileShaderFromFile(L"SkyVertexShader.hlsl", "main", "vs_4_0", &vertexShaderBuffer));
	HR_T(m_pDevice->CreateVertexShader(vertexShaderBuffer->GetBufferPointer(), // 필요한 데이터를 복사하며 객체 생성 
		vertexShaderBuffer->GetBufferSize(), NULL, S_VertexShader.GetAddressOf()));

	// 3. Render() 에서 파이프라인에 바인딩할 InputLayout 생성 	
	D3D11_INPUT_ELEMENT_DESC layout[] =  // 인풋 레이아웃은 버텍스 쉐이더가 입력받을 데이터의 형식을 지정한다.
	{// SemanticName , SemanticIndex , Format , InputSlot , AlignedByteOffset , InputSlotClass , InstanceDataStepRate		
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	// 버텍스 셰이더의 Input에 지정된 내용과 같은지 검증하면서 InputLayout을 생성한다.
	HR_T(hr = m_pDevice->CreateInputLayout(layout, ARRAYSIZE(layout),
		vertexShaderBuffer->GetBufferPointer(), vertexShaderBuffer->GetBufferSize(), S_InputLayout.GetAddressOf()));

	HR_T(CreateDDSTextureFromFile(m_pDevice, L"../resource/box/skybox.dds", nullptr, S_CubeSRV.GetAddressOf()));

	//객체생성
	D3D11_SAMPLER_DESC sampDesc = {};
	//텍스처를 어떻게 보간해서 읽을지 지정 ( MIN: 멀리서볼때 , MAG: 가까이서 볼때, MIP: 밉맵 사이 전환할때 )
	// 조합에 따라 다르게 씀
	sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	//텍스처 좌표가 0~1 범위를 벗어났을때 어떻게 처리할지 결정
	//U, V, W 는 각각 텍스처의 3차원 축 ( 2D 텍스처라면 U/V만 사용)
	//WRAP: 1.2->0.2로 바꿔서 텍스처를 타일처럼 반복,  CLAMP:1.2 ->1.0(가장자리 색 유지) , MIRROR:1.2->0.8(대칭 반복)
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	//샘플러가 비교모드로 동작할때 사용, 보통 섀도우 맵(그림자 계산용)에서 SampleCmp함수로 깊이 비교할때 사용, Never은 비교기능 사용 X -> 일반 텍스처 샘플링 용도로 사용
	sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	//샘플링할 수 있는 최소 LOD 지정, 0 -> 밉맵 레벨 0(가장 고해상도)부터 사용가능
	sampDesc.MinLOD = 0;
	// 샘플링 할 수 있는 최대 LOD 지정  Max-> 제한없음 (가장 낮은 해상도 밉맵까지 다 사용 가능)
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

	HR_T(m_pDevice->CreateSamplerState(&sampDesc, S_Sampler.GetAddressOf()));

	// 4. Render에서 파이프라인에 바인딩할 픽셀 셰이더 생성
	ComPtr<ID3DBlob> pixelShaderBuffer = nullptr; // 픽셀 세이더 HLSL의 컴파일된 결과(바이트코드)를 담을수 있는 버퍼 객체
	HR_T(CompileShaderFromFile(L"SkyPixelShader.hlsl", "main", "ps_4_0", &pixelShaderBuffer));
	HR_T(m_pDevice->CreatePixelShader(	  // 필요한 데이터를 복사하며 객체 생성 
		pixelShaderBuffer->GetBufferPointer(),
		pixelShaderBuffer->GetBufferSize(), NULL, S_PixelShader.GetAddressOf()));


	D3D11_BUFFER_DESC cbDesc{};
	//상수버퍼는 보통 구조체 1개 크기로 만들기 때문에 아래와 같이 설정
	cbDesc.ByteWidth = (sizeof(SkyConstant) + 15) & ~15;;
	//자주 갱신하는 상수 버퍼 옵션
	cbDesc.Usage = D3D11_USAGE_DYNAMIC;
	//상수버퍼로 쓰겠다는 선언
	cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	//D3D11_MAPPED_SUBRESOURCE 을 사용할때는 아래
	cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	//Map 없이 하는 방식 UpdateSubresource를 사용할때는 아래옵션
	/*cbDesc.CPUAccessFlags = 0;*/
	cbDesc.MiscFlags = 0;
	cbDesc.StructureByteStride = 0;

	D3D11_SUBRESOURCE_DATA idDataP{};
	idData.pSysMem = indices;	// 버퍼를 생성할때 복사할 데이터의 주소 설정 
	HR_T(hr = m_pDevice->CreateBuffer(&cbDesc, nullptr, S_ConstantBuffer.GetAddressOf()));


	//6. 뎊스&스텐실 뷰 생성
	D3D11_DEPTH_STENCIL_DESC descDepth = {};
	descDepth.DepthEnable = TRUE;
	descDepth.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	descDepth.DepthFunc = D3D11_COMPARISON_LESS_EQUAL; // 또는 LESS
	descDepth.StencilEnable = FALSE;
	HR_T(m_pDevice->CreateDepthStencilState(&descDepth, S_DepthStencilState.GetAddressOf()));


	D3D11_RASTERIZER_DESC rd = {};
	rd.FillMode = D3D11_FILL_SOLID;
	rd.CullMode = D3D11_CULL_FRONT;
	rd.DepthClipEnable = TRUE;
	HR_T(m_pDevice->CreateRasterizerState(&rd, S_RasterizerState.GetAddressOf()));

	return true;
}

void BlinnPhong::RenderSkyBox()
{
	// 상수버퍼 업데이트 (world/view/projection)
	SkyConstant scb{};
	XMStoreFloat4x4(&scb.world, XMMatrixTranspose(XMMatrixIdentity()));
	XMMATRIX viewNoTrans = m_View;
	viewNoTrans.r[3] = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
	XMStoreFloat4x4(&scb.view, XMMatrixTranspose(viewNoTrans));
	XMStoreFloat4x4(&scb.projection, XMMatrixTranspose(m_Proj));
	D3D11_MAPPED_SUBRESOURCE mapped{};
	HR_T(m_pDeviceContext->Map(S_ConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
	memcpy(mapped.pData, &scb, sizeof(scb));
	m_pDeviceContext->Unmap(S_ConstantBuffer.Get(), 0);


	// IA
	UINT stride = sizeof(VertexSky), offset = 0;
	m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_pDeviceContext->IASetInputLayout(S_InputLayout.Get());
	m_pDeviceContext->IASetVertexBuffers(0, 1, S_VertexBuffer.GetAddressOf(), &stride, &offset);
	m_pDeviceContext->IASetIndexBuffer(S_IndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);

	// 셰이더/상수/자원
	m_pDeviceContext->VSSetShader(S_VertexShader.Get(), nullptr, 0);
	m_pDeviceContext->PSSetShader(S_PixelShader.Get(), nullptr, 0);
	m_pDeviceContext->VSSetConstantBuffers(0, 1, S_ConstantBuffer.GetAddressOf());
	m_pDeviceContext->PSSetConstantBuffers(0, 1, S_ConstantBuffer.GetAddressOf());
	m_pDeviceContext->PSSetShaderResources(0, 1, S_CubeSRV.GetAddressOf());
	m_pDeviceContext->PSSetSamplers(0, 1, S_Sampler.GetAddressOf());

	// 상태
	m_pDeviceContext->OMSetDepthStencilState(S_DepthStencilState.Get(), 0);
	m_pDeviceContext->RSSetState(S_RasterizerState.Get());

	// 드로우
	m_pDeviceContext->DrawIndexed(S_IndexCount, 0, 0);

	// 초기화
	m_pDeviceContext->OMSetDepthStencilState(nullptr, 0);
}

void BlinnPhong::UninitScene()
{
	SAFE_RELEASE(m_pVertexBuffer);
	SAFE_RELEASE(m_pInputLayout);
	SAFE_RELEASE(m_pVertexShader);
	SAFE_RELEASE(m_pPixelShader);
	SAFE_RELEASE(m_pIndexBuffer);
	SAFE_RELEASE(m_pConstantBuffer);
	SAFE_RELEASE(m_pDepthStencilView);
}

bool BlinnPhong::InitImGUI()
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	// 스타일
	ImGui::StyleColorsDark();
	// ImGui::StyleColorsLight();

	// 플랫폼/렌더러 백엔드 연결
	ImGui_ImplWin32_Init(m_hWnd);
	ImGui_ImplDX11_Init(this->m_pDevice, this->m_pDeviceContext);
	return true;
}

void BlinnPhong::UninitImGUI()
{
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

void BlinnPhong::RenderGUI()
{
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;





	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	{
		ImGui::Begin("Controller");

		// Camera
		ImGui::PushID(0);
		ImGui::Text("Camera");
		ImGui::DragFloat("CamSpeed", &speed, 0.1f, 2.0f, 5000.0f);
		ImGui::Checkbox("LocalTrue", &m_RotateAboutLocal);
		ImGui::DragFloat3("Position", CamPosition, 0.05f, -1000.0f, 1000.0f);
		ImGui::DragFloat3("Rotation", CamRotation, 0.05f, -1000.0f, 1000.0f);

		const float eps_local = 0.001f; // 안전 간격
		ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.35f);
		ImGui::DragFloat("zNear", &zNear, 0.1f, eps_local, zFar - eps_local);
		ImGui::SameLine();
		ImGui::DragFloat("zFar", &zFar, 0.1f, zNear + eps_local, 1000.0f);
		ImGui::PopItemWidth();

		ImGui::DragFloat("Fov", &CamFovy, 0.05f, 10.0f, 170.0f);
		

		float dt = GameTimer::m_Instance->DeltaTime();
		//고정 수치가 아닌 카메라 월드행렬 기준 정면으로
		Vector3 Camfwd = m_CWorld.Backward(); Camfwd.Normalize();
		Vector3 Camright = m_CWorld.Right(); Camright.Normalize();
		Vector3 Camup = m_CWorld.Up(); Camup.Normalize();

		Vector3 dpos = Vector3::Zero;
		if (ImGui::IsKeyDown(ImGuiKey_W)) dpos += Camfwd;
		if (ImGui::IsKeyDown(ImGuiKey_S)) dpos -= Camfwd;
		if (ImGui::IsKeyDown(ImGuiKey_D)) dpos += Camright;
		if (ImGui::IsKeyDown(ImGuiKey_A)) dpos -= Camright;
		if (ImGui::IsKeyDown(ImGuiKey_E)) dpos += Camup;
		if (ImGui::IsKeyDown(ImGuiKey_Q)) dpos -= Camup;

		if (dpos.LengthSquared() > 0) dpos.Normalize();
		m_Position += dpos * speed * dt;

		// UI 표시용 배열 동기화(선택)
		CamPosition[0] = m_Position.x;
		CamPosition[1] = m_Position.y;
		CamPosition[2] = m_Position.z;

		Vector3 uiRot = { CamRotation[0], CamRotation[1], CamRotation[2] }; // 라디안
		Vector3 d = uiRot - m_UIRotPrev;
		//이전값 갱신
		m_UIRotPrev = uiRot;
		Vector3 dRad = {
		XMConvertToRadians(d.x),
		XMConvertToRadians(d.y),
		XMConvertToRadians(d.z)
		};

		Quaternion qDelta = Quaternion::CreateFromYawPitchRoll(dRad.y, dRad.x, dRad.z);

		if (m_RotateAboutLocal) {
			// 로컬축 회전: 기존 방향 뒤에 곱함
			m_COri = m_COri * qDelta;
		}
		else {
			// 월드축 회전: 앞에 곱
			m_COri = qDelta * m_COri;
		}


		//초기화
		if (ImGui::Button("Reset##Cam")) {
			CamPosition[0] = 0.0f; CamPosition[1] = 0.0f; CamPosition[2] = -10.0f; // 헤더 기본값
			CamRotation[0] = 0.0f; CamRotation[1] = 0.0f; CamRotation[2] = 0.0f;
			m_UIRotPrev = Vector3(0.0f, 0.0f, 0.0f);
			m_COri = Quaternion::Identity;
			zNear = 0.1f; zFar = 1000.0f;
			CamFovy = 45.0f;
		}



		// 적용
		m_Position = Vector3{ CamPosition[0], CamPosition[1],CamPosition[2] };
		fovy = XMConvertToRadians(CamFovy);
		m_CWorld = Matrix::CreateFromQuaternion(m_COri) * Matrix::CreateTranslation(m_Position);


		ImGui::PopID();
		ImGui::NewLine();

		// Parent
		ImGui::PushID(1);
		ImGui::Text("Parent");
		ImGui::DragFloat3("Position", &P_position.x, 0.05f, -1000.0f, 1000.0f);
		ImGui::DragFloat3("Rotation", &P_rotation.x, 0.05f, -1000.0f, 1000.0f);
		ImGui::DragFloat3("Scale", &P_Scale.x, 0.05f, -1000.0f, 1000.0f);
		if (ImGui::Button("Reset##Parent")) {
			P_position = XMFLOAT3(0.0f, 0.0f, 5.0f);
			P_rotation = XMFLOAT3(0.0f, 0.0f, 0.0f);
			P_Scale = XMFLOAT3(1.0f, 1.0f, 1.0f);
		}
		ImGui::PopID();
		ImGui::NewLine();
		// 색깔
		const float eps_color = 0.0001f; // 안전 간격
		//빛벡터
		ImGui::PushID(2);
		ImGui::Text("Direction");
		ImGui::DragFloat3("Box1", LightDir1, 0.01f, -1.0f + eps_local, 1.0f - eps_local);
		if (ImGui::Button("Reset##Direction")) {
			LightDir1[0] = 1.0f; LightDir1[1] = 0.0f; LightDir1[2] = 0.0f;
		}
		m_LightDirsEvaluated.x = LightDir1[0]; m_LightDirsEvaluated.y = LightDir1[1]; m_LightDirsEvaluated.z = LightDir1[2];


		ImGui::PopID();
		ImGui::NewLine();

		//빛변수
		ImGui::PushID(3);
		ImGui::Text("Light");
		ImGui::ColorEdit3("lightambient", L_ambient);
		ImGui::ColorEdit3("lightdiffuse", L_diffuse);
		ImGui::ColorEdit3("lightspecular", L_specular);

		ImGui::PopID();
		ImGui::NewLine();

		//재질변수
		ImGui::PushID(4);
		ImGui::Text("Material");
		ImGui::ColorEdit3("matambient", M_ambient);
		ImGui::ColorEdit3("matdiffuse", M_diffuse);
		ImGui::ColorEdit3("matspecular", M_specular);
		ImGui::DragFloat("shininess", &constandices.shininess, 1.0f, 1000.0f, 20000.0f);
		if (ImGui::Button("Reset##shininess")) {
			constandices.shininess = 2000.0f;
		}


		constandices.lightambient = { (L_ambient[0]), (L_ambient[1]), (L_ambient[2]) , 1.0f };
		constandices.lightdiffuse = { (L_diffuse[0]), (L_diffuse[1]) , (L_diffuse[2]) , 1.0f };
		constandices.lightspecular = { (L_specular[0]),(L_specular[1]),(L_specular[2]) , 1.0f };

		materialconstand.matambient = { (M_ambient[0]),(M_ambient[1]) , (M_ambient[2]) , 1.0f };
		materialconstand.matdiffuse = { (M_diffuse[0]),(M_diffuse[1]) ,(M_diffuse[2]) , 1.0f };
		materialconstand.matspecular = { (M_specular[0]),(M_specular[1]) ,(M_specular[2]) , 1.0f };

		ImGui::PopID();

		ImGui::End();
	}



	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT BlinnPhong::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
		return 1; // LRESULT 반환

	return __super::WndProc(hWnd, message, wParam, lParam);
}

