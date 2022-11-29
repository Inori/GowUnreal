#include <Windows.h>
#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/msvc_sink.h"
#include "detours.h"
#include <string>
#include <set>
#include <d3d11.h>
#include <glm/glm.hpp>
#include <cmath>
#include <psapi.h>

struct ViewData
{
	glm::mat4    view;
	glm::mat4    invView;
	glm::mat4    proj;
	glm::mat4    invViewProjNoZReproj;
	glm::mat4    viewProj;
	glm::mat4    invViewProj;
	glm::mat4    previousViewProj;
	glm::vec4    prevLodEyePos;
	glm::vec4    lodEyePos;
	glm::vec4    viewDirDepthBias;
	glm::vec3    eyePos;
	glm::vec2    viewportSize;
	glm::vec2    invViewportSize;
	glm::vec4    mirrorPlaneWS;
	glm::vec2    mirrorViewportSize;
	glm::vec2    opaqueRefractionViewportSize;
	float        windTime;
	float        deltaTime;
	float        depthReprojectScale;
	float        depthReprojectBias;
	unsigned int shadowViewType;
	float        normalDepthBias;
	float        noNormalAA;
	float        noNormalFromAlpha;
	float        noOpaqueTransparency;
	float        translucentAmount;
	unsigned int framePhase;
	float        cameraAmbientExposure;
	float        fxExposure;
	float        fxAmbientExposure;
	unsigned int checkerboardPhase;
	float        sparkleIntensityScale;
};

typedef HANDLE(WINAPI* PFUNC_CreateFileW)(
	LPCWSTR               lpFileName,
	DWORD                 dwDesiredAccess,
	DWORD                 dwShareMode,
	LPSECURITY_ATTRIBUTES lpSecurityAttributes,
	DWORD                 dwCreationDisposition,
	DWORD                 dwFlagsAndAttributes,
	HANDLE                hTemplateFile);

PFUNC_CreateFileW g_OldCreateFileW = CreateFileW;

HANDLE g_hWadFile = NULL;

HANDLE WINAPI NewCreateFileW(
	LPCWSTR               lpFileName,
	DWORD                 dwDesiredAccess,
	DWORD                 dwShareMode,
	LPSECURITY_ATTRIBUTES lpSecurityAttributes,
	DWORD                 dwCreationDisposition,
	DWORD                 dwFlagsAndAttributes,
	HANDLE                hTemplateFile)
{
	static std::set<std::wstring> s_nameSet;

	HANDLE handle = g_OldCreateFileW(lpFileName,
					 dwDesiredAccess,
					 dwShareMode,
					 lpSecurityAttributes,
					 dwCreationDisposition,
					 dwFlagsAndAttributes,
					 hTemplateFile);

	auto logger = spdlog::get("gow-logger");

	do 
	{
		if (handle == INVALID_HANDLE_VALUE)
		{
			break;
		}

		std::wstring fileName(lpFileName);
		if (fileName[0] == '\\')
		{
			break;
		}

		if (s_nameSet.find(fileName) == s_nameSet.end())
		{
			//logger->info(L"{}", fileName);
			//logger->flush();

			s_nameSet.insert(fileName);
		}

		if (fileName.find(L"R_HeroA00.wad") != std::wstring::npos)
		{
			g_hWadFile = handle;
			logger->info("wad handle: {}", (void*)g_hWadFile);
		}


	} while (false);

	return handle;
}


typedef BOOL (WINAPI* PFUNC_ReadFile)(
	HANDLE       hFile,
	LPVOID       lpBuffer,
	DWORD        nNumberOfBytesToRead,
	LPDWORD      lpNumberOfBytesRead,
	LPOVERLAPPED lpOverlapped);

PFUNC_ReadFile g_OldReadFile = ReadFile;

BOOL WINAPI NewReadFile(
	HANDLE       hFile,
	LPVOID       lpBuffer,
	DWORD        nNumberOfBytesToRead,
	LPDWORD      lpNumberOfBytesRead,
	LPOVERLAPPED lpOverlapped)
{
	BOOL bRet = g_OldReadFile(hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, lpOverlapped);

	const char* szTag = "attRageEnter02";
	DWORD       dwTagLen = strlen(szTag);

	auto logger = spdlog::get("gow-logger");

	do 
	{
		if (hFile != g_hWadFile || !lpBuffer || !lpNumberOfBytesRead)
		{
			break;
		}

		DWORD dwBytesRead = *lpNumberOfBytesRead;

		if (dwBytesRead < dwTagLen)
		{
			break;
		}

		BYTE* pMem = (BYTE*)lpBuffer;
		for (DWORD i = 0; i < dwBytesRead - dwTagLen; ++i)
		{
			if (!memcmp(&pMem[i], szTag, dwTagLen))
			{
				//char szMsg[64] = {};
				//sprintf_s(szMsg, "anm addr: %p", &pMem[i]);
				//MessageBoxA(nullptr, szMsg, "Debug", MB_OK);
				logger->info("anim addr: {}", (void*)&pMem[i]);
			}
		}


	} while (false);


	return bRet;
}



ID3D11Buffer* g_pViewDataBuffer = nullptr;


typedef void(WINAPI* PFUNC_VSSetConstantBuffers)(
	ID3D11DeviceContext* pCtx,
	UINT                 StartSlot,
	UINT                 NumBuffers,
	ID3D11Buffer* const* ppConstantBuffers);

PFUNC_VSSetConstantBuffers g_OldVSSetConstantBuffers = nullptr;

void WINAPI NewVSSetConstantBuffers(
	ID3D11DeviceContext* pCtx,
	UINT                 StartSlot,
	UINT                 NumBuffers,
	ID3D11Buffer* const* ppConstantBuffers)
{
	if (StartSlot == 3 && NumBuffers == 1 && ppConstantBuffers)
	{
		ID3D11Buffer* pBuffer = *ppConstantBuffers;
		if (pBuffer)
		{
			D3D11_BUFFER_DESC Desc;
			pBuffer->GetDesc(&Desc);
			if (Desc.ByteWidth == 688)
			{
				static bool bFirst = true;
				if (bFirst)
				{
					//__debugbreak();
					//MessageBoxA(NULL, "Set View Data", "GOW", MB_OK);
					bFirst = false;
				}
				g_pViewDataBuffer = pBuffer;
			}
		}
	}
	g_OldVSSetConstantBuffers(pCtx, StartSlot, NumBuffers, ppConstantBuffers);
}


typedef void(WINAPI* PFUNC_UpdateSubresource)(
	ID3D11DeviceContext* pCtx,
	ID3D11Resource*      pDstResource,
	UINT                 DstSubresource,
	const D3D11_BOX*     pDstBox,
	const void*          pSrcData,
	UINT                 SrcRowPitch,
	UINT                 SrcDepthPitch);

PFUNC_UpdateSubresource g_OldUpdateSubresource = nullptr;

void WINAPI NewUpdateSubresource(
	ID3D11DeviceContext* pCtx,
	ID3D11Resource*      pDstResource,
	UINT                 DstSubresource,
	const D3D11_BOX*     pDstBox,
	const void*          pSrcData,
	UINT                 SrcRowPitch,
	UINT                 SrcDepthPitch)
{
	if (pDstResource == g_pViewDataBuffer)
	{
		ViewData* pViewData = (ViewData*)pSrcData;
		auto      logger    = spdlog::get("gow-logger");
		if (logger)
		{
			if (pViewData->eyePos[0] != 0.0 && pViewData->eyePos[1] != 0.0 && pViewData->eyePos[2] != 0.0)
			{
				static uint32_t nCount = 0;
				if (nCount == 60)
				{
					logger->info("EyePos {} {} {}", pViewData->eyePos[0], pViewData->eyePos[1], pViewData->eyePos[2]);
					logger->info("EyePosHex {:x} {:x} {:x}", *(uint32_t*)&pViewData->eyePos[0], *(uint32_t*)&pViewData->eyePos[1], *(uint32_t*)&pViewData->eyePos[2]);
					nCount = 0;
				}
				++nCount;
			}
			
		}
	}

	g_OldUpdateSubresource(pCtx,
						   pDstResource,
						   DstSubresource,
						   pDstBox,
						   pSrcData,
						   SrcRowPitch,
						   SrcDepthPitch);
}

typedef HRESULT (WINAPI* PFUNC_Map)(
	ID3D11DeviceContext*      pCtx,
	ID3D11Resource*           pResource,
	UINT                      Subresource,
	D3D11_MAP                 MapType,
	UINT                      MapFlags,
	D3D11_MAPPED_SUBRESOURCE* pMappedResource);

PFUNC_Map g_OldMap = nullptr;
void*     g_pBoneBuffer = nullptr;
void*     g_pBoneMemory = nullptr;

HRESULT WINAPI NewMap(
	ID3D11DeviceContext*      pCtx,
	ID3D11Resource*           pResource,
	UINT                      Subresource,
	D3D11_MAP                 MapType,
	UINT                      MapFlags,
	D3D11_MAPPED_SUBRESOURCE* pMappedResource)
{
	HRESULT hRet = g_OldMap(pCtx, pResource, Subresource, MapType, MapFlags, pMappedResource);

	do 
	{
		if (!pResource)
		{
			break;
		}

		D3D11_RESOURCE_DIMENSION dim = {};
		pResource->GetType(&dim);

		if (dim != D3D11_RESOURCE_DIMENSION_BUFFER)
		{
			break;
		}

		ID3D11Buffer* pBuffer = (ID3D11Buffer*)pResource;

		D3D11_BUFFER_DESC desc = {};
		pBuffer->GetDesc(&desc);

		// this buffer is for bone matrix array
		if (desc.ByteWidth != 52752 || desc.CPUAccessFlags != D3D11_CPU_ACCESS_WRITE)
		{
			break;
		}

		g_pBoneBuffer = pBuffer;
		g_pBoneMemory = pMappedResource->pData;
		
		if (IsDebuggerPresent())
		{
			char pMsg[64] = { 0 };
			sprintf_s(pMsg, "map addr %p", g_pBoneMemory);
			//MessageBoxA(0, pMsg, "Debug", MB_OK);
		}

	} while (false);

	return hRet;
}


typedef void (WINAPI* PFUNC_Unmap)(
	ID3D11DeviceContext*      pCtx,
	ID3D11Resource*           pResource,
	UINT                      Subresource);

PFUNC_Unmap g_OldUnmap = nullptr;

void WINAPI NewUnmap(
	ID3D11DeviceContext*      pCtx,
	ID3D11Resource*           pResource,
	UINT                      Subresource)
{
	if (pResource == g_pBoneBuffer && IsDebuggerPresent())
	{
		memset(g_pBoneMemory, 0, 52752);
	}

	g_OldUnmap(pCtx, pResource, Subresource);

	if (pResource == g_pBoneBuffer)
	{
		if (IsDebuggerPresent())
		{
			char pMsg[64] = { 0 };
			sprintf_s(pMsg, "unmap buffer %p", pResource);
			//MessageBoxA(0, pMsg, "Debug", MB_OK);
		}
	}
}


bool IsValidPosition(const glm::vec4& pos)
{
	return std::fabs(pos.x) > 1.0 && std::fabs(pos.y) > 1.0 && std::fabs(pos.z) > 1.0;
}

bool IsValidClass(uint64_t pCls)
{
	MODULEINFO modinfo = { 0 };
	HMODULE    hModule = GetModuleHandleA(NULL);
	GetModuleInformation(GetCurrentProcess(), hModule, &modinfo, sizeof(MODULEINFO));

	uint64_t nModStart = (uint64_t)modinfo.lpBaseOfDll;
	uint64_t nModEnd   = nModStart + modinfo.SizeOfImage;
	return pCls >= nModStart && pCls <= nModEnd;
}

void CodeCopy(void* dst, void* src, size_t size)
{
	DWORD dwOldProtect = 0;
	VirtualProtect(dst, size, PAGE_EXECUTE_READWRITE, &dwOldProtect);

	memcpy(dst, src, size);
}

// GoW.exe:
// 1.0.475.7534
// 19833344 bytes
// 
// Instructions that write to eye position:
// 
// GoW.exe+4E7AEB - 0F11 53 30            - movups [rbx+30],xmm2
// GoW.exe+4E7C17 - 0F11 4B 30            - movups [rbx+30],xmm1
// GoW.exe+4E7D42 - 0F11 53 30            - movups [rbx+30],xmm2
// GoW.exe+4E7E28 - 0F11 53 30            - movups [rbx+30],xmm2
// 
// 

// Function address
// .text:0000000140001000 base
// .text:00000001404E7A40 move_camera     proc near               ; CODE XREF: sub_14045BF40+209¡üp

// Function header:
// .text:00000001404E7A40 ; __unwind { // __GSHandlerCheck
// .text:00000001404E7A40                 mov     rax, rsp
// .text:00000001404E7A43                 mov     [rax+10h], rbx
// .text:00000001404E7A47                 push    rbp
// .text:00000001404E7A48                 push    rsi
// .text:00000001404E7A49                 push    rdi
// .text:00000001404E7A4A                 push    r12
// .text:00000001404E7A4C                 push    r13
// .text:00000001404E7A4E                 push    r14
// .text:00000001404E7A50                 push    r15
// .text:00000001404E7A52                 lea     rbp, [rax-48h]
// .text:00000001404E7A56                 sub     rsp, 110h
// .text:00000001404E7A5D                 movaps  xmmword ptr [rax-48h], xmm6
// .text:00000001404E7A61                 movaps  xmmword ptr [rax-58h], xmm7
// .text:00000001404E7A65                 movaps  xmmword ptr [rax-68h], xmm8
// .text:00000001404E7A6A                 movaps  xmmword ptr [rax-78h], xmm9
// .text:00000001404E7A6F                 movaps  xmmword ptr [rax-88h], xmm10
// .text:00000001404E7A77                 mov     rax, cs:__security_cookie
// .text:00000001404E7A7E                 xor     rax, rsp
// .text:00000001404E7A81                 mov     [rbp+40h+var_90], rax
// .text:00000001404E7A85                 mov     rdi, [rbp+40h+arg_20]
// .text:00000001404E7A89                 mov     rbx, r9
// .text:00000001404E7A8C                 mov     r15, [rbp+40h+arg_28]
// .text:00000001404E7A90                 mov     r9, r8
// .text:00000001404E7A93                 mov     r12, [rbp+40h+arg_30]
// .text:00000001404E7A9A                 mov     rsi, rdx
// .text:00000001404E7A9D                 mov     r13, [rbp+40h+arg_38]
// .text:00000001404E7AA4                 mov     r14, [rbp+40h+arg_40]
// .text:00000001404E7AAB                 mov     [rsp+140h+var_120], r8
// .text:00000001404E7AB0                 test    rcx, rcx
// .text:00000001404E7AB3                 jz      loc_1404E7B64
// .text:00000001404E7AB9                 add     rcx, 60h ; '`'


typedef void (*PFUNC_MoveCamera)(
	uint64_t nArg1,
	uint64_t nArg2,
	uint64_t nArg3,
	uint64_t nCameraInfo,
	uint64_t nArg5,
	uint64_t nArg6,
	uint64_t nArg7,
	uint64_t nArg8,
	uint64_t nArg9);

PFUNC_MoveCamera g_OldMoveCamera = nullptr;

bool      g_bUnlimitedCamera = false;
glm::vec4 g_vEyePos;

void NewMoveCamera(uint64_t nArg1,
				   uint64_t nArg2,
				   uint64_t nArg3,
				   uint64_t nCameraInfo,
				   uint64_t nArg5,
				   uint64_t nArg6,
				   uint64_t nArg7,
				   uint64_t nArg8,
				   uint64_t nArg9)
{
	glm::vec4* pEyePos = (glm::vec4*)(nCameraInfo + 0x30);

	g_OldMoveCamera(nArg1,
					nArg2,
					nArg3,
					nCameraInfo,
					nArg5,
					nArg6,
					nArg7,
					nArg8,
					nArg9);

	static bool bHooked = false;
	HMODULE     hExe    = GetModuleHandleA(NULL);

	// Note:
	// It only works in camera mode.
	// [nCameraInfo + 0x30] have several possible values,
	// only one of them is EyePos,
	// I don't know how to filter there addresses in non-camera mode.

	if (g_bUnlimitedCamera)
	{
		uint8_t pNops[4] = { 0x90, 0x90, 0x90, 0x90 };
		if (!bHooked)
		{
			CodeCopy((BYTE*)hExe + 0x4E7AEB, pNops, 4);
			CodeCopy((BYTE*)hExe + 0x4E7C17, pNops, 4);
			CodeCopy((BYTE*)hExe + 0x4E7D42, pNops, 4);
			CodeCopy((BYTE*)hExe + 0x4E7E28, pNops, 4);
			bHooked = true;
		}

		if (pEyePos)
		{
			if (IsValidPosition(*pEyePos) && IsValidClass(nArg1))
			{
				*pEyePos = g_vEyePos;
			}
		}
	}
	else
	{
		if (bHooked)
		{
			uint8_t pCode1[4] = { 0x0F, 0x11, 0x53, 0x30 };
			uint8_t pCode2[4] = { 0x0F, 0x11, 0x4B, 0x30 };
			CodeCopy((BYTE*)hExe + 0x4E7AEB, pCode1, 4);
			CodeCopy((BYTE*)hExe + 0x4E7C17, pCode2, 4);
			CodeCopy((BYTE*)hExe + 0x4E7D42, pCode1, 4);
			CodeCopy((BYTE*)hExe + 0x4E7E28, pCode1, 4);
			bHooked = false;
		}

		if (pEyePos)
		{
			if (IsValidPosition(*pEyePos) && IsValidClass(nArg1))
			{
				g_vEyePos = *pEyePos;

				auto            logger = spdlog::get("gow-logger");
				static uint32_t nCount = 0;
				if (nCount == 60)
				{
					logger->info("EyePos {} {} {}", pEyePos->x, pEyePos->y, pEyePos->z);
					nCount = 0;
				}
				++nCount;
			}
		
		}
	}
}

unsigned __stdcall MoveCameraFunc(void* pArguments)
{
	while (true)
	{
		float step = 0.5;

		if ((GetAsyncKeyState(VK_NUMPAD0) & 0x8000) != 0)
		{
			g_bUnlimitedCamera = !g_bUnlimitedCamera;
		}

		if ((GetAsyncKeyState(VK_NUMPAD4) & 0x8000) != 0)
		{
			g_vEyePos.z += step;
		}

		if ((GetAsyncKeyState(VK_NUMPAD6) & 0x8000) != 0)
		{
			g_vEyePos.z -= step;
		}

		if ((GetAsyncKeyState(VK_NUMPAD8) & 0x8000) != 0)
		{
			g_vEyePos.x -= step;
		}

		if ((GetAsyncKeyState(VK_NUMPAD5) & 0x8000) != 0)
		{
			g_vEyePos.x += step;
		}

		if ((GetAsyncKeyState(VK_NUMPAD7) & 0x8000) != 0)
		{
			g_vEyePos.y -= step;
		}

		if ((GetAsyncKeyState(VK_NUMPAD9) & 0x8000) != 0)
		{
			g_vEyePos.y += step;
		}

		Sleep(10);
	}
}

struct ListEntry
{
	void* pMemory;
	uint32_t nSize;
};

unsigned __stdcall ParseGlobalList(void* pArguments)
{
	auto logger = spdlog::get("gow-logger");

	//while (!IsDebuggerPresent())
	//{
	//	Sleep(1);
	//}

	HMODULE     hMod        = GetModuleHandleA(NULL);
	ListEntry** pList       = (ListEntry**)((uint8_t*)hMod + 0x12DB8D0);
	uint32_t    nEntryCount = *(uint32_t*)((uint8_t*)hMod + 0x12DB8B8);

	while (!nEntryCount)
	{
		nEntryCount = *(uint32_t*)((uint8_t*)hMod + 0x12DB8B8);
		Sleep(1);
	}

	while (true)
	{
		ListEntry** pList       = (ListEntry**)((uint8_t*)hMod + 0x12DB8D0);
		uint32_t    nEntryCount = *(uint32_t*)((uint8_t*)hMod + 0x12DB8B8);

		logger->info("pList: {}", (void*)pList);
		logger->info("nEntryCount: {}", nEntryCount);

		for (uint32_t i = 0; i != nEntryCount; ++i)
		{
			ListEntry* pEntry = pList[i];

			if (pEntry->nSize != 0xCE10)
			{
				continue;
			}

			logger->info("memory: {}", (void*)pEntry->pMemory);
			logger->info("size: {}", (void*)pEntry->nSize);
		}

		logger->info("=================");

		Sleep(2000);
	}

	return 0;
}

// in asm file
extern "C" void* ParseAnimeWrapper(void* pThis, uint64_t dwUnknown, void* pAnimeFile, const char* szAnimeName);

typedef void* (*PFUNC_ParseAnime)(void* pThis, uint64_t dwUnknown, void* pAnimeFile);
PFUNC_ParseAnime g_OldParseAnime = nullptr;

extern "C" void* NewParseAnime(void* pThis, uint64_t dwUnknown, void* pAnimeFile, const char* szAnimeName)
{
	void* pAnime = g_OldParseAnime(pThis, dwUnknown, pAnimeFile);


	return pAnime;
}


void InitLogger()
{
	auto msvc_sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
	auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("gow-log.txt", true);
	spdlog::sinks_init_list sink_list = { file_sink, msvc_sink };
	// spdlog::sinks_init_list sink_list = { msvc_sink };

	auto logger = std::make_shared<spdlog::logger>("gow-logger", sink_list.begin(), sink_list.end());
	spdlog::set_default_logger(logger);
	spdlog::flush_on(spdlog::level::info);
	spdlog::set_pattern("[Asuka] [%t] %v");
}

void SetupHook()
{
	HMODULE hD3D11 = LoadLibraryA("d3d11.dll");

	g_OldVSSetConstantBuffers = (PFUNC_VSSetConstantBuffers)((BYTE*)hD3D11 + 0x105160);
	g_OldUpdateSubresource    = (PFUNC_UpdateSubresource)((BYTE*)hD3D11 + 0x105970);

	g_OldMap   = (PFUNC_Map)((BYTE*)hD3D11 + 0x105640);
	g_OldUnmap = (PFUNC_Unmap)((BYTE*)hD3D11 + 0x105870);

	BYTE* pModBase  = (BYTE*)GetModuleHandleA(NULL);
	g_OldMoveCamera = (PFUNC_MoveCamera)(pModBase + 0x4E7A40);
	g_OldParseAnime = (PFUNC_ParseAnime)(pModBase + 0x4ABD20);
	

	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());


	DetourAttach((void**)&g_OldCreateFileW, NewCreateFileW);
	DetourAttach((void**)&g_OldReadFile, NewReadFile);
	//DetourAttach((void**)&g_OldVSSetConstantBuffers, NewVSSetConstantBuffers);
	//DetourAttach((void**)&g_OldUpdateSubresource, NewUpdateSubresource);
	//DetourAttach((void**)&g_OldMoveCamera, NewMoveCamera);
	//DetourAttach((void**)&g_OldMap, NewMap);
	//DetourAttach((void**)&g_OldUnmap, NewUnmap);
	//DetourAttach((void**)&g_OldParseAnime, ParseAnimeWrapper);

	DetourTransactionCommit();

	//unsigned threadID;
	//_beginthreadex(NULL, 0, &MoveCameraFunc, NULL, 0, &threadID);

	unsigned threadID;
	//_beginthreadex(NULL, 0, &ParseGlobalList, NULL, 0, &threadID);
}

void InitProc()
{
	InitLogger();
	SetupHook();
}

void UnProc()
{

}


__declspec(dllexport)void WINAPI Dummy()
{
}

BOOL APIENTRY DllMain(HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved
)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        InitProc();
        break;
    case DLL_THREAD_ATTACH:
        break;
    case DLL_THREAD_DETACH:
        break;
    case DLL_PROCESS_DETACH:
        UnProc();
        break;
    }
    return TRUE;
}
