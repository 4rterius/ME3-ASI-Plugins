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

void SetupConsoleIO()
{
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

void TeardownConsoleIO()
{
	fprintf(stdout, "BYE\n");
	FreeConsole();
}

inline bool StringStartsWith(char* a, char* b)
{
	return (strncmp(a, b, strlen(b)) == 0);
}

inline bool StringStartsWith(wchar_t* a, wchar_t* b)
{
	return (wcsncmp(a, b, wcslen(b)) == 0);
}

inline bool StringEquals(char* a, char* b)
{
	return 0 == strcmp(a, b);
}

inline bool StringEquals(wchar_t* a, wchar_t* b)
{
	return 0 == wcscmp(a, b);
}

template<typename T>
std::vector<T*> FindObjects(UClass* type, bool allowDefaults = false)
{
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

template<typename T>
T* FindADefaultObject(UClass* type)
{
	const auto objects = UObject::GObjObjects();
	for (auto i = 0; i < objects->Count; i++)
	{
		auto object = objects->Data[i];
		if (object && object->IsA(type) && StringStartsWith(object->Name.GetName(), "Default_"))
		{
			return reinterpret_cast<T*>(object);
		}
	}
	return nullptr;
}

inline USFXEngine* GetGameEngine()
{
	const auto engineInstances = FindObjects<USFXEngine>(USFXEngine::StaticClass());
	const auto engineInstanceCount = engineInstances.size();

	//printf("GetGameEngine: found %Iu instance(s)\n", engineInstanceCount);

	if (engineInstanceCount > 0)
	{
		return engineInstances[0];
	}
	else
	{
		printf("GetGameEngine: FAILED TO FIND A GAME ENGINE\n");
		return nullptr;
	}
}

#pragma endregion


// Custom logic.
// --------------------------------------------------
#pragma region Custom logic

#pragma region changesize
inline void _changePawnSize(APawn* pawn, float scale)
{
	if (pawn == nullptr)
	{
		return;
	}

	pawn->CylinderComponent->CollisionRadius *= scale;
	pawn->CylinderComponent->CollisionHeight *= scale;
	pawn->SetDrawScale(scale);
	pawn->SetLocation(pawn->location, 0);
}

inline bool ChangeSize(wchar_t* subject, float scale)
{
	if (subject == nullptr)
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
		_changePawnSize(pawn, scale);
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
		for (int i = 0; i < squadMembers.Count; i++)
		{
			_changePawnSize(squadMembers(i), scale);
		}

		return true;
	}
	else if (StringEquals(subject, L"all"))
	{
		TArray<AActor*> actors;
		worldInfo->FindActorsOfClass(APawn::StaticClass(), &actors);
		for (int i = 0; i < actors.Count; i++)
		{
			_changePawnSize(reinterpret_cast<APawn*>(actors(i)), scale);
		}

		return true;
	}

	return false;
}
#pragma endregion

#pragma region setspeed
inline void _setPawnSpeed(APawn* pawn, float scale)
{
	if (pawn == nullptr)
	{
		return;
	}

	auto defaultPawn = FindADefaultObject<APawn>(pawn->StaticClass());
	if (!defaultPawn)
	{
		printf("_setPawnSpeed: default pawn is NULL, simply multiplying...\n");
		pawn->GroundSpeed *= scale;
		pawn->WaterSpeed *= scale;
	}
	else
	{
		pawn->GroundSpeed = defaultPawn->GroundSpeed * scale;
		pawn->WaterSpeed = defaultPawn->WaterSpeed * scale;
	}
}

inline bool SetSpeed(wchar_t* subject, float scale)
{
	if (subject == nullptr)
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
		_setPawnSpeed(pawn, scale);
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
		for (int i = 0; i < squadMembers.Count; i++)
		{
			_setPawnSpeed(squadMembers(i), scale);
		}

		return true;
	}
	else if (StringEquals(subject, L"all"))
	{
		TArray<AActor*> actors;
		worldInfo->FindActorsOfClass(APawn::StaticClass(), &actors);
		for (int i = 0; i < actors.Count; i++)
		{
			_setPawnSpeed(reinterpret_cast<APawn*>(actors(i)), scale);
		}

		return true;
	}

	return false;
}
#pragma endregion

#pragma region teleport
struct MyCameraInfo
{
	FRotator viewRotation;
	FVector viewLocation;
	FVector hitNormal;
	FVector hitLocation;
	AActor* hitActor;
};

inline struct MyCameraInfo _tracePlayerView()
{
	MyCameraInfo cameraInfo;

	auto playerController = (GetGameEngine()->GetRealWorldInfo()->GetLocalPlayerController());
	auto playerCamera = playerController->PlayerCamera;

	FRotator viewRotation = playerController->Pawn->Rotation;
	FVector viewLocation = playerController->Pawn->location;
	viewLocation.Z += playerController->Pawn->BaseEyeHeight;

	auto traceInfo = reinterpret_cast<Asfxplayercamera*>(playerCamera)->m_aTraceInfo;

	cameraInfo.viewLocation = viewLocation;
	cameraInfo.viewRotation = viewRotation;

	cameraInfo.hitNormal = traceInfo.m_vCollVectorNormal;
	cameraInfo.hitLocation = traceInfo.m_vCollVectorLocation;
	cameraInfo.hitActor = traceInfo.m_oCollVectorActor;

	if (cameraInfo.hitActor)
	{
		const int someFactor = 4.0;
		cameraInfo.hitLocation.X += cameraInfo.hitNormal.X * someFactor;
		cameraInfo.hitLocation.Y += cameraInfo.hitNormal.Y * someFactor;
		cameraInfo.hitLocation.Z += cameraInfo.hitNormal.Z * someFactor;
	}

	return cameraInfo;
}

inline bool Teleport(wchar_t* subject)
{
	if (subject == nullptr)
	{
		return false;
	}

	printf("teleport: subject  OK\n");

	auto gameEngine = GetGameEngine();
	auto worldInfo = gameEngine->GetRealWorldInfo();
	if (worldInfo == nullptr)
	{
		return false;
	}
	auto playerController = worldInfo->GetLocalPlayerController();
	if (playerController == nullptr)
	{
		return false;
	}
	auto hitLocation = _tracePlayerView().hitLocation;


	printf("teleport: gameEngine, worldInfo, playerController, hitLocation  OK\n");
	printf("teleport: location = %2.3f, %2.3f, %2.3f\n",
		hitLocation.X, hitLocation.Y, hitLocation.Z);


	if (StringEquals(subject, L"me"))
	{
		playerController->ViewTarget->SetLocation(hitLocation, 1);
		return true;
	}
	else if (StringEquals(subject, L"squad"))
	{
		auto squad = worldInfo->m_playerSquad;
		if (squad == nullptr)
		{
			return false;
		}

		auto& squadMembers = squad->Members;
		for (int i = 0; i < squadMembers.Count; i++)
		{
			squadMembers(i)->SetLocation(hitLocation, 1);
		}

		return true;
	}
	else if (StringEquals(subject, L"all"))
	{
		TArray<AActor*> actors;
		worldInfo->FindActorsOfClass(APawn::StaticClass(), &actors);
		for (int i = 0; i < actors.Count; i++)
		{
			actors(i)->SetLocation(hitLocation, 1);
		}

		return true;
	}

	return false;
}
#pragma endregion

#pragma region god
inline void _god(AController* controller)
{
	if (controller == nullptr)
	{
		return;
	}

	controller->bGodMode = 1 - controller->bGodMode;
}

inline void _god(ABioPlayerController* controller)
{
	if (controller == nullptr)
	{
		return;
	}

	printf("BPC God\n");

	if (controller->Role != 3)
	{
		//controller->DispatchCommand(FString(L"God"));
		controller->bGodMode = 1 - controller->bGodMode;
	}
	else
	{
		controller->bGodMode = 1 - controller->bGodMode;
		//controller->CheatManager->God();
	}
}

inline bool God(wchar_t* subject)
{
	if (subject == nullptr)
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
		_god(worldInfo->GetLocalPlayerController());
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
		for (int i = 0; i < squadMembers.Count; i++)
		{
			if (squadMembers(i)->Controller == nullptr)
			{
				continue;
			}

			if (squadMembers(i)->Controller->bIsPlayer == 1)
			{
				_god(reinterpret_cast<ABioPlayerController*>(squadMembers(i)->Controller));
			}
			else
			{
				_god(squadMembers(i)->Controller);
			}
		}

		return true;
	}
	else if (StringEquals(subject, L"all"))
	{
		TArray<AActor*> actors;
		worldInfo->FindActorsOfClass(APawn::StaticClass(), &actors);
		for (int i = 0; i < actors.Count; i++)
		{
			auto ctrl = reinterpret_cast<APawn*>(actors(i))->Controller;
			if (ctrl == nullptr)
			{
				continue;
			}

			if (ctrl->bIsPlayer == 1)
			{
				_god(reinterpret_cast<ABioPlayerController*>(ctrl));
			}
			else
			{
				_god(reinterpret_cast<APawn*>(actors(i))->Controller);
			}
		}

		return true;
	}

	return false;
}
#pragma endregion

#pragma region throwweapon
inline void _throwWeapon(APawn* pawn)
{
	if (pawn)
	{
		pawn->ThrowActiveWeapon();
	}
}
inline bool ThrowWeapon(wchar_t* subject)
{
	if (subject == nullptr)
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
		_throwWeapon(worldInfo->GetLocalPlayerController()->Pawn);
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
		for (int i = 0; i < squadMembers.Count; i++)
		{
			_throwWeapon(squadMembers(i));
		}

		return true;
	}
	else if (StringEquals(subject, L"all"))
	{
		TArray<AActor*> actors;
		worldInfo->FindActorsOfClass(APawn::StaticClass(), &actors);
		for (int i = 0; i < actors.Count; i++)
		{
			_throwWeapon(reinterpret_cast<APawn*>(actors(i)));
		}

		return true;
	}

	return false;
}
#pragma endregion

#pragma endregion


// ProcessEvent.
// --------------------------------------------------
void __fastcall HookedPE(UObject* pObject, void* edx, UFunction* pFunction, void* pParms, void* pResult)
{
	static bool drawTeleport = false;  // enabled by 'drawtrace' in console

	if (drawTeleport && StringEquals(pFunction->GetFullName(), "Function SFXGame.BioHUD.PostRender"))
	{
		const auto& trace = _tracePlayerView();

		auto bioHud = reinterpret_cast<ABioHUD*>(pObject);
		auto hudCanvas = bioHud->Canvas;

		hudCanvas->SetPos(50.f, 50.f);
		hudCanvas->SetDrawColor(125, 125, 255, 255);

		wchar_t locLine[256];
		swprintf(locLine, 255, L"My location: %2.3f, %2.3f, %2.3f\nMy rotation: %d, %d, %d",
			trace.viewLocation.X, trace.viewLocation.Y, trace.viewLocation.Z,
			trace.viewRotation.Pitch, trace.viewRotation.Yaw, trace.viewRotation.Roll);
		hudCanvas->DrawTextW(FString(locLine), 1, 1.0, 1.0, nullptr);

		hudCanvas->SetDrawColor(125, 255, 255, 255);

		wchar_t posLine[256];
		swprintf(posLine, 255, L"Trace: %2.3f, %2.3f, %2.3f",
			trace.hitLocation.X, trace.hitLocation.Y, trace.hitLocation.Z);
		hudCanvas->DrawTextW(FString(posLine), 1, 1.0, 1.0, nullptr);

		bioHud->DrawDebugSphere(trace.hitLocation, 20.f, 10, 255, 0, 0, 0);

		auto hitActor = trace.hitActor;
		if (hitActor)
		{
			wchar_t hitLine[256];
			swprintf(hitLine, 255, L"Hit actor: %S_%d", hitActor->GetFullName(), hitActor->Name.GetIndex());
			hudCanvas->DrawTextW(FString(hitLine), 1, 1.0, 1.0, nullptr);

			hitActor->DrawDebugSphere(hitActor->location, 100.f, 10, 0, 0, 255, 0);

			auto collisionComponent = hitActor->CollisionComponent;
			if (collisionComponent)
			{
				hitActor->DrawDebugBox(collisionComponent->Bounds.Origin, collisionComponent->Bounds.BoxExtent, 225, 125, 255, 0);
			}
		}
	}

	if (IsA<USFXConsole>(pObject) && StringEquals(pFunction->GetFullName(), "Function Console.Typing.InputChar"))
	{
		auto sfxConsole = reinterpret_cast<USFXConsole*>(pObject);
		auto inputParams = reinterpret_cast<UConsole_execInputChar_Parms*>(pParms);
		if (inputParams->Unicode.Count >= 1 && inputParams->Unicode.Data[0] == '\r')
		{
			auto command = sfxConsole->TypedStr.Data;

			auto engine = GetGameEngine();
			if (engine)
			{
				auto len = wcslen(command);

				if (StringStartsWith(command, L"mychangesize "))
				{
					wchar_t subject[16];
					float scale = 1.f;

					wmemset(subject, 0, 16);

					int read = swscanf(command, L"mychangesize %15ls %f", subject, &scale);
					if (read == 2)
					{
						wprintf(L"Overriden 'changesize': scaling %s to %f\n", subject, scale);
						ChangeSize(subject, scale);
					}
					else
					{
						wprintf(L"Overriden 'changesize': error, read %d\n", read);
					}

					return;
				}

				if (StringStartsWith(command, L"mysetspeed "))
				{
					wchar_t subject[16];
					float scale = 1.f;

					wmemset(subject, 0, 16);

					int read = swscanf(command, L"mysetspeed %15ls %f", subject, &scale);
					if (read == 2)
					{
						wprintf(L"Overriden 'setspeed': %s -> %f\n", subject, scale);
						SetSpeed(subject, scale);
					}
					else
					{
						wprintf(L"Overriden 'setspeed': error, read %d\n", read);
					}

					return;
				}

				if (StringStartsWith(command, L"myteleport "))
				{
					wchar_t subject[16];
					wmemset(subject, 0, 16);
					int read = swscanf(command, L"myteleport %15ls", subject);
					if (read == 1)
					{
						wprintf(L"Overriden 'teleport': %s\n", subject);
						Teleport(subject);
						return;
					}
					else if (read > 0)
					{
						wprintf(L"Overriden 'teleport': error, read %d\n", read);
						return;
					}
					else
					{
						// let the original command through
					}
				}

				if (StringStartsWith(command, L"mygod "))
				{
					wchar_t subject[16];
					wmemset(subject, 0, 16);

					int read = swscanf(command, L"mygod %15ls", subject);
					if (read == 1)
					{
						wprintf(L"Overriden 'god': %s\n", subject);
						God(subject);
						return;
					}
					else
					{
						wprintf(L"Overriden 'god': error, read %d\n", read);
						return;
					}
				}

				if (StringStartsWith(command, L"mythrowweapon "))
				{
					wchar_t subject[16];
					wmemset(subject, 0, 16);

					int read = swscanf(command, L"mythrowweapon %15ls", subject);
					if (read == 1)
					{
						wprintf(L"Overriden 'throwweapon': %s\n", subject);
						ThrowWeapon(subject);
						return;
					}
					else
					{
						wprintf(L"Overriden 'throwweapon': error, read %d\n", read);
						return;
					}
				}

				if (StringEquals(command, L"drawtrace"))
				{
					drawTeleport = !drawTeleport;
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

	printf("\n");
	printf("Some custom commands for @Strife The Historian:\n");
	printf("  mychangesize <subject> <scale>\n");
	printf("    Sets <subject>'s drawing scale to the <scale> float.\n");
	printf("  mysetspeed <subject> <speed>\n");
	printf("    Sets <subject>'s ground and water speed to the <speed> float.\n");
	printf("  myteleport <subject>\n");
	printf("    Teleports <subject> to the currently traced location.\n");
	printf("  mygod <subject>\n");
	printf("    Enables godmode for the <subject>.\n");
	printf("  mythrowweapon <subject>\n");
	printf("    Makes <subject> throw their weapons.\n");
	printf("  drawtrace\n");
	printf("    Toggles hittracing utility (was used while working on 'myteleport').\n");
	printf("\n");
	printf("  <subject> is in {\"me\", \"squad\", \"all\"}\n");
	printf("    Some commands may not work properly for some of these.\n");
	printf("\n");

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