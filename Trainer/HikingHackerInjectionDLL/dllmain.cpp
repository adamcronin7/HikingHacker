#include <iostream>
#include <windows.h>


template<typename T>
T readMemory(LPVOID adr) {
    return *((T*)adr);
}
template<typename T>
void writeMemory(LPVOID adr, T val) {
    *((T*)adr) = val;
}
template<typename T>
DWORD protectMemory(uint64_t address, DWORD prot)
{
    DWORD oldProt;
    VirtualProtect((LPVOID)address, sizeof(T), prot, &oldProt);
    return oldProt;
}


namespace ObjectOffsets
{
    constexpr std::size_t ID = 0x00;

    namespace Player
    {
        constexpr std::size_t Y = 0x10; // y-position offset for a player
    }

    namespace Enemy
    {
        constexpr std::size_t Health = 0x08; // health offset for an enemy
    }

    namespace Chest
    {
        constexpr std::size_t isOpen = 0x08; // isOpen offset for a chest
    }
}
template<typename T>
T readOffset(const void* object, std::size_t offset)
{
    T value{};
    std::memcpy(&value,
        static_cast<const std::byte*>(object) + offset,
        sizeof(T));

    return value;
}
template<typename T>
void writeOffset(void* object, std::size_t offset, const T& value)
{
    std::memcpy(
        static_cast<std::byte*>(object) + offset,
        &value,
        sizeof(T)
    );
}

typedef uint64_t(__cdecl _origFunc)(void* arg1,
    void* arg2,
    void* arg3,
    void* arg4,
    float arg5);
_origFunc* originalFunction;


uint64_t __cdecl updateObjects(
    void* arg1,
    void* arg2,
    void* arg3,
    void* arg4,
    float arg5)
{
    if (arg4)
    {
        int id = readOffset<int>(
            arg4,
            ObjectOffsets::ID
        );

        switch (id)
        {
        case 0:
        {
            // Player detected
            // If J key is pressed jump player up by 500 units
            static bool jWasDown = false;

            bool jDown = (GetAsyncKeyState('J') & 0x8000) != 0;

            // Only execute once per key press.
            if (jDown && !jWasDown)
            {
                float y = readOffset<float>(
                    arg4,
                    ObjectOffsets::Player::Y);

                writeOffset<float>(
                    arg4,
                    ObjectOffsets::Player::Y,
                    y - 500.0f);
            }

            jWasDown = jDown;

            break;
        }

        case 2:
        {
            // Enemy detected
            // If health is more than 0, set health to 0
            int health = readOffset<int>(
                arg4,
                ObjectOffsets::Enemy::Health
            );

            if (health > 0)
            {
                writeOffset<int>(
                    arg4,
                    ObjectOffsets::Enemy::Health,
                    0);
            }
            break;
        }
        case 5:
        {
            // Chest detected
            // If isOpen is false, set to true
            bool isOpen = readOffset<int>(
                arg4,
                ObjectOffsets::Chest::isOpen);

            if (!isOpen)
            {
                writeOffset<bool>(
                    arg4,
                    ObjectOffsets::Chest::isOpen,
                    true);
            }
            break;
        }

        default:
            break;
        }
    }

    return originalFunction(arg1, arg2, arg3, arg4, arg5);
}

BYTE trampoline[12] = {
    0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // mov rax, address
    0xFF, 0xE0, // jmp rax
};

uint64_t callHook(DWORD hookOffset, uint64_t newFunction)
{
    uint64_t memBase = (uint64_t)GetModuleHandle(NULL);
    uint64_t hookAddress = memBase + hookOffset;
    uint64_t newOffset = newFunction - hookAddress - 5;

    // Put the correct memory address into the trampoline so that it jumps to our updateObject function
    memcpy(&trampoline[2], &newFunction, 8);

    // Write the trampoline in an empty space of memory
    auto oldTrampolineProtection = protectMemory<DWORD[3]>((memBase + 0x00013FAA), PAGE_EXECUTE_READWRITE);
    memcpy((LPVOID)(memBase + 0x00013FAA), trampoline, sizeof(trampoline));
    protectMemory<DWORD>((memBase + 0x00013FAA), oldTrampolineProtection);

    // Hook the update call function to call our trampoline
    auto oldProtection = protectMemory<DWORD>(hookAddress, PAGE_EXECUTE_READWRITE);
    int32_t originalOffset = readMemory<int32_t>((LPVOID)(hookAddress + 1));
    writeMemory<DWORD>((LPVOID)(hookAddress + 1), (memBase + 0x00013FAA) - hookAddress - 5);
    protectMemory<DWORD>(hookAddress, oldProtection);
    return originalOffset + hookAddress + 5;
}



DWORD WINAPI runCode(LPVOID lpParam) {
    originalFunction = (_origFunc*)callHook(0x94A9, (uint64_t)&updateObjects);
    return 1;
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        MessageBoxA(NULL, "DLL Attached!\n", "Hiking Hacker", MB_OK | MB_TOPMOST);
        auto thread = CreateThread(NULL, 0, &runCode, NULL, 0, NULL);
        CloseHandle(thread);
        break;
    }
    return TRUE;
}

