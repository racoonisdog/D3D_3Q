// 01_imgui.cpp : 애플리케이션에 대한 진입점을 정의합니다.
//
#include "DeferredRendering.h"


int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
#ifdef _DEBUG
	AllocConsole();

	FILE* fpOut;
	FILE* fpIn;

	freopen_s(&fpOut, "CONOUT$", "w", stdout);
	freopen_s(&fpIn, "CONIN$", "r", stdin);
#endif

	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	DeferredRendering App(hInstance);  // 생성자에서 아이콘,윈도우 이름만 바꾼다
	if (!App.Initialize(1600, 1200))
		return -1;

	return App.Run();
}
