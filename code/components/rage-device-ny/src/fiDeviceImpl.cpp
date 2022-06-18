/*
 * This file is part of the CitizenFX project - http://citizen.re/
 *
 * See LICENSE and MENTIONS in the root of the source tree for information
 * regarding licensing.
 */

#include "StdInc.h"
#include "fiDevice.h"

#define PURECALL() __asm { jmp _purecall }

namespace rage
{
fiDeviceImplemented::fiDeviceImplemented()
{
}

fiDeviceImplemented::~fiDeviceImplemented()
{

}

uint32_t WRAPPER fiDeviceImplemented::Open(const char* fileName, bool) { PURECALL(); }

uint32_t WRAPPER fiDeviceImplemented::OpenBulk(const char* fileName, uint64_t* ptr)
{
	PURECALL();
}

uint32_t WRAPPER fiDeviceImplemented::CreateLocal(const char* fileName)
{
	PURECALL();
}

uint32_t WRAPPER fiDeviceImplemented::Create(const char*)
{
	PURECALL();
}

uint32_t WRAPPER fiDeviceImplemented::Read(uint32_t handle, void* buffer, uint32_t toRead)
{
	PURECALL();
}

uint32_t WRAPPER fiDeviceImplemented::ReadBulk(uint32_t handle, uint64_t ptr, void* buffer, uint32_t toRead)
{
	PURECALL();
}

uint32_t WRAPPER fiDeviceImplemented::WriteBulk(int, int, int, int, int)
{
	PURECALL();
}

uint32_t WRAPPER fiDeviceImplemented::Write(size_t, void*, int)
{
	PURECALL();
}

uint32_t WRAPPER fiDeviceImplemented::Seek(uint32_t handle, int32_t distance, uint32_t method)
{
	PURECALL();
}

uint64_t WRAPPER fiDeviceImplemented::SeekLong(uint32_t handle, int64_t distance, uint32_t method)
{
	PURECALL();
}

int32_t WRAPPER fiDeviceImplemented::Close(uint32_t handle)
{
	PURECALL();
}

int32_t WRAPPER fiDeviceImplemented::CloseBulk(uint32_t handle)
{
	PURECALL();
}

int WRAPPER fiDeviceImplemented::GetFileLength(uint32_t handle)
{
	PURECALL();
}

int WRAPPER fiDeviceImplemented::m_34()
{
	PURECALL();
}

bool WRAPPER fiDeviceImplemented::RemoveFile(const char* file)
{
	PURECALL();
}

int WRAPPER fiDeviceImplemented::RenameFile(const char* from, const char* to)
{
	PURECALL();
}

int WRAPPER fiDeviceImplemented::CreateDirectory(const char* dir)
{
	PURECALL();
}

int WRAPPER fiDeviceImplemented::RemoveDirectory(const char * dir)
{
	PURECALL();
}

uint64_t WRAPPER fiDeviceImplemented::GetFileLengthLong(const char* file)
{
	PURECALL();
}

uint64_t WRAPPER fiDeviceImplemented::GetFileTime(const char* file)
{
	PURECALL();
}

bool WRAPPER fiDeviceImplemented::SetFileTime(const char* file, FILETIME fileTime)
{
	PURECALL();
}

size_t WRAPPER fiDeviceImplemented::FindFirst(const char* path, fiFindData* findData)
{
	PURECALL();
}

bool WRAPPER fiDeviceImplemented::FindNext(size_t handle, fiFindData* findData)
{
	PURECALL();
}

int WRAPPER fiDeviceImplemented::FindClose(size_t handle)
{
	PURECALL();
}

bool WRAPPER fiDeviceImplemented::Truncate(uint32_t handle)
{
	PURECALL();
}

uint32_t WRAPPER fiDeviceImplemented::GetFileAttributes(const char* path)
{
	PURECALL();
}

bool WRAPPER fiDeviceImplemented::SetFileAttributes(const char* file, uint32_t FileAttributes)
{
	PURECALL();
}

WRAPPER char* fiDeviceImplemented::FixRelativeName(char* out, size_t s, const char* in)
{
	PURECALL();
}

WRAPPER int32_t fiDeviceImplemented::GetResourceVersion(const char* fileName, ResourceFlags* version)
{
	PURECALL();
}

uint32_t WRAPPER fiDeviceImplemented::GetParentHandle()
{
	PURECALL();
}

WRAPPER int64_t fiDeviceImplemented::m_7C(int a)
{
	PURECALL();
}

WRAPPER void fiDeviceImplemented::m_80()
{
	PURECALL();
}

WRAPPER int32_t fiDeviceImplemented::m_84(int a)
{
	PURECALL();
}
}
