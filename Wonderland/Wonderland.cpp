#include <stdio.h>
#include <io.h>
#include <fcntl.h>     /* for _O_TEXT and _O_BINARY */  
#include <string>
#include <fstream>
#include <iostream>
#include <ostream>
#include <streambuf>
#include <shlwapi.h>
#include <vector>

#include "..\ME3SDK\ME3TweaksHeader.h"
#include "..\ME3SDK\SdkHeaders.h"
#include "..\detours\detours.h"

#define _CRT_SECURE_NO_WARNINGS
#pragma comment(lib, "detours.lib")

// Utilities.
// --------------------------------------------------
#pragma region Utilities

void SetupConsoleIO() {
	AllocConsole();
	freopen_s((FILE**)stdin, "CONIN$", "r", stdin);
	freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
	freopen_s((FILE**)stderr, "CONOUT$", "w", stderr);

	fprintf(stdout, "stdout OK\n");
	fprintf(stderr, "stderr OK\n");

	HANDLE h_console = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO lpConsoleScreenBufferInfo;
	GetConsoleScreenBufferInfo(h_console, &lpConsoleScreenBufferInfo);

	COORD new_size = {
		lpConsoleScreenBufferInfo.dwSize.X,
		30000
	};
	SetConsoleScreenBufferSize(h_console, new_size);
}

void TeardownConsoleIO() {
	fprintf(stdout, "BYE\n");
	FreeConsole();
}

inline bool StringStartsWith(char* a, char* b) {
	return (strncmp(a, b, strlen(b)) == 0);
}

inline bool StringStartsWith(wchar_t* a, wchar_t* b) {
	return (wcsncmp(a, b, wcslen(b)) == 0);
}

inline bool StringEquals(char* a, char* b) {
	return 0 == strcmp(a, b);
}

inline bool StringEquals(wchar_t* a, wchar_t* b)
{
	return 0 == wcscmp(a, b);
}

template<typename T>
std::vector<T*> FindObjects(UClass* type, bool allowDefaults = false) {
	std::vector<T*> found;

	const auto objects = UObject::GObjObjects();
	for (auto i = 0; i < objects->Count; i++)
	{
		auto object = objects->Data[i];
		if (object && object->IsA(type) && (allowDefaults || nullptr == strstr(object->Name.GetName(), "Default_")))
		{
			found.push_back(reinterpret_cast<T*>(object));
		}
	}
	return found;
}

inline USFXEngine* GetGameEngine()
{
	const auto engineInstances = FindObjects<USFXEngine>(USFXEngine::StaticClass());
	const auto engineInstanceCount = engineInstances.size();

	//printf("GetGameEngine: found %Iu instance(s)\n", engineInstanceCount);

	if (engineInstanceCount > 0) {
		return engineInstances[0];
	}
	else {
		printf("GetGameEngine: FAILED TO FIND A GAME ENGINE\n");
		return nullptr;
	}
}

#pragma endregion


// Scaling logic.
// --------------------------------------------------
#pragma region Scaling logic

inline void ScalePawn(APawn* pawn, float scale)
{
	if (pawn == nullptr || scale < 0.f)
	{
		return;
	}

	pawn->CylinderComponent->CollisionRadius *= scale;
	pawn->CylinderComponent->CollisionHeight *= scale;
	pawn->SetDrawScale(scale);
	pawn->SetLocation(pawn->location, 0);
}

inline bool MyChangeSize(wchar_t* subject, float scale)
{
	if (subject == nullptr || scale < 0.f)
	{
		return false;
	}

	auto gameEngine = GetGameEngine();
	auto worldInfo = gameEngine->GetRealWorldInfo();
	if (worldInfo == nullptr)
	{
		return false;
	}

	if (StringEquals(subject, L"me"))
	{
		const auto& pawn = worldInfo->GetLocalPlayerController()->Pawn;
		ScalePawn(pawn, scale);
		return true;
	}
	else if (StringEquals(subject, L"squad"))
	{
		const auto& squad = worldInfo->m_playerSquad;
		if (squad == nullptr)
		{
			return false;
		}

		const auto& squadMembers = squad->Members;
		for (int i = 0; i < squadMembers.Count; i++) {
			ScalePawn(squadMembers(i), scale);
		}

		return true;
	}
	else if (StringEquals(subject, L"all"))
	{
		TArray<AActor*> actors;
		worldInfo->FindActorsOfClass(APawn::StaticClass(), &actors);
		for (int i = 0; i < actors.Count; i++)
		{
			ScalePawn(reinterpret_cast<APawn*>(actors(i)), scale);
		}

		return true;
	}

	return false;
}

#pragma endregion


// ProcessEvent.
// --------------------------------------------------
void __fastcall HookedPE(UObject* pObject, void* edx, UFunction* pFunction, void* pParms, void* pResult)
{
	if (IsA<USFXConsole>(pObject) && StringEquals(pFunction->GetFullName(), "Function Console.Typing.InputChar"))
	{
		auto sfxConsole = reinterpret_cast<USFXConsole*>(pObject);
		auto inputParams = reinterpret_cast<UConsole_execInputChar_Parms*>(pParms);
		if (inputParams->Unicode.Count >= 1 && inputParams->Unicode.Data[0] == '\r')
		{
			auto command = sfxConsole->TypedStr.Data;

			auto engine = GetGameEngine();
			if (engine) {
				auto len = wcslen(command);

				if (StringStartsWith(command, L"changesize"))
				{
					wchar_t subject[16];
					float scale = 1.f;

					wmemset(subject, 0, 16);

					int read = swscanf(command, L"changesize %15ls %f", subject, &scale);
					if (read == 2)
					{
						wprintf(L"Overriden 'changesize': scaling %s to %f\n", subject, scale);
						MyChangeSize(subject, scale);
					}
					else
					{
						wprintf(L"Overriden 'changesize': error, read %d\n", read);
					}

					return;
				}
			}
		}
	}

	ProcessEvent(pObject, pFunction, pParms, pResult);
}


void OnAttach()
{
	SetupConsoleIO();

	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourAttach(&(PVOID&)ProcessEvent, HookedPE);
	DetourTransactionCommit();
}

void OnDetach()
{
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourDetach(&(PVOID&)ProcessEvent, HookedPE);
	DetourTransactionCommit();

	TeardownConsoleIO();
}

BOOL WINAPI DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved)
{
	switch (dwReason)
	{
	case DLL_PROCESS_ATTACH:
		DisableThreadLibraryCalls(hModule);
		CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)OnAttach, NULL, 0, NULL);
		return true;

	case DLL_PROCESS_DETACH:
		OnDetach();
		return true;
	}
	return true;
};