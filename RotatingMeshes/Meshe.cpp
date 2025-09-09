#include "Meshe.h"
#include "../Common/Helper.h"
#include <directxtk/simplemath.h>
#include <d3dcompiler.h>

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


Meshe::Meshe(HINSTANCE hInstance) : GameApp(hInstance)
{

}


Meshe::~Meshe()
{
	UninitScene();
	UninitD3D();
}

bool Meshe::Initialize(UINT Width, UINT Height)
{
	__super::Initialize(Width, Height);

	if (!InitD3D())
		return false;

	if (!InitScene())
		return false;

	return true;
}

void Meshe::Update()
{
	__super::Update();
	/*P_rotation.y += 0.01f;;

	XMMATRIX scaleMat = XMMatrixScaling(P_Scale.x, P_Scale.y, P_Scale.z);
	XMMATRIX rotMat = XMMatrixRotationRollPitchYaw(P_rotation.x, P_rotation.y, P_rotation.z);
	XMMATRIX transMat = XMMatrixTranslation(P_position.x, P_position.y, P_position.z);

	P_world = scaleMat * rotMat * transMat;

	XMStoreFloat4x4(&constandices.world, XMMatrixTranspose(P_world));*/
	/*constandices.view = XMMatrixTranspose(view);*/
	/*constandices.projection = XMMatrixTranspose(m_projection);*/
	P_rotation.y += 0.01f;
	P_rotation.x += 0.013f;
	XMMATRIX S = XMMatrixScaling(P_Scale.x, P_Scale.y, P_Scale.z);
	XMMATRIX R = XMMatrixRotationRollPitchYaw(P_rotation.x, P_rotation.y, P_rotation.z);
	XMMATRIX T = XMMatrixTranslation(P_position.x, P_position.y, P_position.z);
	XMMATRIX W = S * R * T;

	//Transpose 해서 XMFLOAT4X4에 저장
	XMStoreFloat4x4(&constandices.world, XMMatrixTranspose(W));
}

void Meshe::Render()
{
	float color[4] = { 0.0f, 0.5f, 0.5f, 1.0f };

	// 화면 칠하기.
	m_pDeviceContext->ClearRenderTargetView(m_pRenderTargetView, color);

	// Draw계열 함수를 호출하기전에 렌더링 파이프라인에 필수 스테이지 설정을 해야한다.	
	m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST); // 정점을 이어서 그릴 방식 설정.

	// 버퍼를 바인딩 해주는 단계
	m_pDeviceContext->IASetVertexBuffers(0, 1, &m_pVertexBuffer, &m_VertextBufferStride, &m_VertextBufferOffset);
	m_pDeviceContext->IASetInputLayout(m_pInputLayout);

	m_pDeviceContext->IASetIndexBuffer(m_pIndexBuffer, DXGI_FORMAT_R32_UINT, 0);

	m_pDeviceContext->VSSetShader(m_pVertexShader, nullptr, 0);
	m_pDeviceContext->PSSetShader(m_pPixelShader, nullptr, 0);

	// Render a triangle	
	//m_pDeviceContext->Draw(m_VertexCount, 0);
	D3D11_MAPPED_SUBRESOURCE mapped{};
	m_pDeviceContext->Map(m_pConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
	memcpy(mapped.pData, &constandices, sizeof(constandices));
	m_pDeviceContext->Unmap(m_pConstantBuffer, 0);
	m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_pConstantBuffer);

	m_pDeviceContext->DrawIndexed(m_IndexCount, 0, 0);		//인덱스 사용시

	// Present the information rendered to the back buffer to the front buffer (the screen)
	m_pSwapChain->Present(0, 0);
}

bool Meshe::InitD3D()
{
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
	m_pDeviceContext->OMSetRenderTargets(1, &m_pRenderTargetView, NULL);

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

void Meshe::UninitD3D()
{
	SAFE_RELEASE(m_pRenderTargetView);
	SAFE_RELEASE(m_pSwapChain);
	SAFE_RELEASE(m_pDeviceContext);
	SAFE_RELEASE(m_pDevice);
}

bool Meshe::InitScene()
{
	HRESULT hr = 0; // 결과값.
	ID3D10Blob* errorMessage = nullptr;	 // 컴파일 에러 메시지가 저장될 버퍼.	

	Vertex vertices[] =
	{
		Vertex(Vector3(-0.5f,  -0.5f, 0.5f), Vector4(1.0f, 0.0f, 0.0f, 1.0f)),
		Vertex(Vector3(-0.5f, 0.5f, 0.5f), Vector4(0.0f, 1.0f, 0.0f, 1.0f)),
		Vertex(Vector3(0.5f, -0.5f, 0.5f), Vector4(0.0f, 0.0f, 1.0f, 1.0f)),
		Vertex(Vector3(0.5f, 0.5f, 0.5f), Vector4(0.0f, 0.0f, 1.0f, 1.0f)),
		Vertex(Vector3(-0.5f,  -0.5f, -0.5f), Vector4(1.0f, 0.0f, 0.0f, 1.0f)),
		Vertex(Vector3(0.5f, -0.5f, -0.5f), Vector4(0.0f, 0.0f, 1.0f, 1.0f)),
		Vertex(Vector3(-0.5f, 0.5f, -0.5f), Vector4(0.0f, 1.0f, 0.0f, 1.0f)),
		Vertex(Vector3(0.5f, 0.5f, -0.5f), Vector4(0.0f, 0.0f, 1.0f, 1.0f))
	};

	D3D11_BUFFER_DESC vbDesc = {};
	m_VertexCount = ARRAYSIZE(vertices);	// 정점의 수
	vbDesc.ByteWidth = sizeof(Vertex) * m_VertexCount; // 버텍스 버퍼의 크기(Byte).
	vbDesc.CPUAccessFlags = 0;
	vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER; // 정점 버퍼로 사용.
	vbDesc.MiscFlags = 0;
	vbDesc.Usage = D3D11_USAGE_DEFAULT;	// CPU는 접근불가 ,  GPU에서 읽기/쓰기 가능한 버퍼로 생성.

	// 정점 버퍼 생성.
	D3D11_SUBRESOURCE_DATA vbData = {};
	vbData.pSysMem = vertices;	// 버퍼를 생성할때 복사할 데이터의 주소 설정 
	HR_T(hr = m_pDevice->CreateBuffer(&vbDesc, &vbData, &m_pVertexBuffer));


	// 인덱스 리스트
	UINT indices[] = {
	0, 1, 2,
	2, 1, 3,
	2, 3, 5,
	5, 3, 7,
	1, 6, 3,
	6, 7, 3,		// 보이는면
	0, 4, 1,
	1, 4, 6,
	0, 2, 4,
	2, 5, 4,
	4, 5, 6,
	5, 7, 6			// 안보이는면
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
	m_VertextBufferStride = sizeof(Vertex); // 버텍스 하나의 크기
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
		{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(Vertex, color), D3D11_INPUT_PER_VERTEX_DATA, 0}
	};
	// 버텍스 셰이더의 Input에 지정된 내용과 같은지 검증하면서 InputLayout을 생성한다.
	HR_T(hr = m_pDevice->CreateInputLayout(layout, ARRAYSIZE(layout),
		vertexShaderBuffer->GetBufferPointer(), vertexShaderBuffer->GetBufferSize(), &m_pInputLayout));

	SAFE_RELEASE(vertexShaderBuffer); // 복사했으니 버퍼는 해제 가능


	// 4. Render에서 파이프라인에 바인딩할 픽셀 셰이더 생성
	ID3DBlob* pixelShaderBuffer = nullptr; // 픽셀 세이더 HLSL의 컴파일된 결과(바이트코드)를 담을수 있는 버퍼 객체
	HR_T(CompileShaderFromFile(L"BasicPixelShader.hlsl", "main", "ps_4_0", &pixelShaderBuffer));
	HR_T(m_pDevice->CreatePixelShader(	  // 필요한 데이터를 복사하며 객체 생성 
		pixelShaderBuffer->GetBufferPointer(),
		pixelShaderBuffer->GetBufferSize(), NULL, &m_pPixelShader));
	SAFE_RELEASE(pixelShaderBuffer); // 복사했으니 버퍼는 해제 가능

	/*Constant constandices{};*/
	float aspect = float(1024.0f) / float(768.0f);
	XMMATRIX proj = XMMatrixPerspectiveFovLH(XMConvertToRadians(60.0f), aspect, 0.1f, 1000.0f);
	XMStoreFloat4x4(&constandices.projection, XMMatrixTranspose(proj));

	// 뷰 (한 번만)
	XMVECTOR eye = XMVectorSet(0, 0, -3, 0);
	XMVECTOR target = XMVectorSet(0, 0, 0, 0);
	XMVECTOR up = XMVectorSet(0, 1, 0, 0);
	XMMATRIX view = XMMatrixLookAtLH(eye, target, up);
	XMStoreFloat4x4(&constandices.view, XMMatrixTranspose(view));

	// 월드 (매 프레임)
	XMMATRIX S = XMMatrixScaling(P_Scale.x, P_Scale.y, P_Scale.z);
	XMMATRIX R = XMMatrixRotationRollPitchYaw(P_rotation.x, P_rotation.y, P_rotation.z);
	XMMATRIX T = XMMatrixTranslation(P_position.x, P_position.y, P_position.z);
	XMMATRIX W = S * R * T;
	XMStoreFloat4x4(&constandices.world, XMMatrixTranspose(W));
	

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


	return true;
}

void Meshe::UninitScene()
{
	SAFE_RELEASE(m_pVertexBuffer);
	SAFE_RELEASE(m_pInputLayout);
	SAFE_RELEASE(m_pVertexShader);
	SAFE_RELEASE(m_pPixelShader);
	SAFE_RELEASE(m_pIndexBuffer);
	SAFE_RELEASE(m_pConstantBuffer);
}

