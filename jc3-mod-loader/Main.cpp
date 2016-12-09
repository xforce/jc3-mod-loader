#include <Windows.h>

#include <filesystem>
#include <thread>
#include <chrono>
#include <mutex>
#include <cassert>

using DirectInput8Create_t = HRESULT(WINAPI *)(HINSTANCE, DWORD, REFIID, LPVOID *, LPUNKNOWN);

struct MeowLoadedDll {
    HMODULE hModule;
    std::string file;
    FILETIME lastWriteTime;
};

std::vector<MeowLoadedDll> LoadedDllEntries;

HRESULT WINAPI DirectInput8Create(HINSTANCE hinst, DWORD dwVersion, REFIID riidltf, LPVOID * ppvOut, LPUNKNOWN punkOuter)
{
    static std::once_flag flag;
    std::call_once(flag, []() {
        std::thread([]() {
            for (;;) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                for (auto &loadedDll : LoadedDllEntries) {
                    FILETIME LastWriteTime = {};
                    WIN32_FILE_ATTRIBUTE_DATA Data;
                    if (GetFileAttributesExA(loadedDll.file.c_str(), GetFileExInfoStandard, &Data))
                    {
                        LastWriteTime = Data.ftLastWriteTime;
                    }

                    if (CompareFileTime(&LastWriteTime, &loadedDll.lastWriteTime)) {
                        FreeLibrary(loadedDll.hModule);
                        CopyFile(loadedDll.file.c_str(), (loadedDll.file + "_tmp").c_str(), FALSE);
                        loadedDll.hModule = LoadLibraryA((loadedDll.file + "_tmp").c_str());
                        if (loadedDll.hModule) {
                            loadedDll.lastWriteTime = LastWriteTime;
                        }
                    }
                }


            }
        }).detach();
    });


	char buffer[MAX_PATH] = { 0 };
	GetSystemDirectoryA(buffer, MAX_PATH);
	strcat_s(buffer, MAX_PATH, "\\dinput8.dll");

	auto module = LoadLibraryA(buffer);
	if (module)
	{
		static auto original = (DirectInput8Create_t)GetProcAddress(module, "DirectInput8Create");
		if (original)
			return original(hinst, dwVersion, riidltf, ppvOut, punkOuter);
	}

	return E_FAIL;
}

namespace fs = std::experimental::filesystem;



BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    if (fdwReason == DLL_PROCESS_ATTACH) {
        // Allocate a section that mods can use to to do hooking things, because of relative jump/call offset
        VirtualAlloc((LPVOID)0x0000000160000000, 0x6000000, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
        for (auto& p : fs::directory_iterator("mods"))
        {
            if (p.path().extension() == ".dll") {
                auto path = p.path().relative_path().string();
                CopyFile(path.c_str(), (path + "_tmp").c_str(), FALSE);

                MeowLoadedDll meow;
                meow.hModule = LoadLibraryA((path + "_tmp").c_str());
                meow.file = path;

                FILETIME LastWriteTime = {};
                WIN32_FILE_ATTRIBUTE_DATA Data;
                if (GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &Data))
                {
                    LastWriteTime = Data.ftLastWriteTime;
                }

                meow.lastWriteTime = LastWriteTime;
                LoadedDllEntries.push_back(meow);
            }
        }
    }
	return TRUE;
}