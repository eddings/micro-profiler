//	Copyright (c) 2011-2019 by Artem A. Gevorkyan (gevorkyan.org)
//
//	Permission is hereby granted, free of charge, to any person obtaining a copy
//	of this software and associated documentation files (the "Software"), to deal
//	in the Software without restriction, including without limitation the rights
//	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//	copies of the Software, and to permit persons to whom the Software is
//	furnished to do so, subject to the following conditions:
//
//	The above copyright notice and this permission notice shall be included in
//	all copies or substantial portions of the Software.
//
//	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//	THE SOFTWARE.

#include <common/module.h>

#include <common/string.h>

#include <windows.h>
#include <tlhelp32.h>

using namespace std;

namespace micro_profiler
{
	string get_current_executable()
	{	return get_module_info(0).path;	}

	mapped_module get_module_info(const void *address)
	{
		HMODULE load_address = 0;
		char path[MAX_PATH + 1] = { };

		::GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, static_cast<LPCWSTR>(address), &load_address);
		::GetModuleFileNameA(load_address, path, sizeof(path));
		::FreeLibrary(load_address);
		mapped_module info = { path, static_cast<byte *>(static_cast<void *>(load_address)), };
		return info;
	}

	void enumerate_process_modules(const module_callback_t &callback)
	{
		mapped_module module;
		shared_ptr<void> snapshot(::CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, 0), &::CloseHandle);
		MODULEENTRY32 entry = { sizeof(MODULEENTRY32), };

		for (auto lister = &::Module32First; lister(snapshot.get(), &entry); lister = &::Module32Next, module.addresses.clear())
		{
			module.path = unicode(entry.szExePath);
			module.base = entry.modBaseAddr;
			module.addresses.push_back(byte_range(entry.modBaseAddr, entry.modBaseSize));
			callback(module);
		}
	}
}
