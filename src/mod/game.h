#pragma once
// The ONLY layer that knows Game.exe addresses, struct offsets and calling
// conventions. Everything here was verified on the Steam build (Game.exe
// SHA-256 95C5A7BA..., buildid 4434295); see PATCH_TARGETS.md.
#include <cstdint>

namespace sga {

struct ScriptThread;  // opaque; accessed through the helpers below

using InstProc = void(__cdecl*)(ScriptThread*);

namespace Game {

// Waits for SteamStub to decrypt .text, then resolves every signature.
// Returns false (and logs each miss) when this build is not supported.
bool Resolve();

uintptr_t Base();

// Address of a named game function from its byte signature (0 if missing).
uintptr_t Find(const char* name);

// Script VM
uint8_t*& ThreadPc(ScriptThread* t);
int ThreadScriptBuffer(ScriptThread* t);
int Eval(ScriptThread* t);  // evaluates the expression at pc, advancing pc
const uint8_t* ScriptString(int scriptBuffer, int stringId);

enum class Table { System, Graph, User1 };
// Replaces an instruction handler; returns the original.
InstProc HookInstruction(Table table, int op, InstProc replacement);

// Script state
bool Flag(int id);
int Work(int id);

// Native state (int at an RVA of Game.exe; see PATCH_TARGETS.md)
int ReadInt(uintptr_t rva);
bool SkipMode();  // the E key skip mode is on

}  // namespace Game
}  // namespace sga
