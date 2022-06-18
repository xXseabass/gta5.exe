/*
 * This file is part of the CitizenFX project - http://citizen.re/
 *
 * See LICENSE and MENTIONS in the root of the source tree for information
 * regarding licensing.
 */

// method names based on research by listener, though they seem to match actual
// names somewhat (i.e. 'bulk' isn't some made-up shit)

#pragma once

#include "sysAllocator.h"

#ifdef COMPILING_RAGE_DEVICE_NY
#define DEVICE_EXPORT COMPONENT_EXPORT
#define DEVICE_IMPORT
#else
#define DEVICE_IMPORT __declspec(dllimport)
#define DEVICE_EXPORT
#endif

namespace rage
{
struct fiFindData
{
	char fileName[512];
	uint64_t fileSize;
	FILETIME lastWriteTime;
	DWORD fileAttributes;
};

using ResourceFlags = uint32_t;

class DEVICE_EXPORT __declspec(novtable) fiDevice : public sysUseAllocator
{
public:
	static fiDevice* GetDevice(const char* path, bool allowRoot);

	static void Unmount(const char* rootPath);

	static void MountGlobal(const char* path, fiDevice* device, bool allowRoot);

	static DEVICE_IMPORT fwEvent<> OnInitialMount;

public:
	virtual ~fiDevice() = 0;

	virtual uint32_t Open(const char* fileName, bool readOnly) = 0;

	virtual uint32_t OpenBulk(const char* fileName, uint64_t* ptr) = 0;

	virtual uint32_t CreateLocal(const char* fileName) = 0;

	virtual uint32_t Create(const char* fileName);

	virtual uint32_t Read(uint32_t handle, void* buffer, uint32_t toRead) = 0;

	virtual uint32_t ReadBulk(uint32_t handle, uint64_t ptr, void* buffer, uint32_t toRead) = 0;

	virtual uint32_t WriteBulk(int, int, int, int, int) = 0;

	virtual uint32_t Write(uint32_t, void*, int) = 0;

	virtual uint32_t Seek(uint32_t handle, int32_t distance, uint32_t method) = 0;

	virtual uint64_t SeekLong(uint32_t handle, int64_t distance, uint32_t method) = 0;

	virtual int32_t Close(uint32_t handle) = 0;

	virtual int32_t CloseBulk(uint32_t handle) = 0;

	virtual int GetFileLength(uint32_t handle) = 0;

	virtual int m_34() = 0;
	virtual bool RemoveFile(const char* file) = 0;
	virtual int RenameFile(const char* from, const char* to) = 0;
	virtual int CreateDirectory(const char* dir) = 0;

	virtual int RemoveDirectory(const char * dir) = 0;
	virtual uint64_t GetFileLengthLong(const char* file) = 0;
	virtual uint64_t GetFileTime(const char* file) = 0;
	virtual bool SetFileTime(const char* file, FILETIME fileTime) = 0;

	virtual size_t FindFirst(const char* path, fiFindData* findData) = 0;
	virtual bool FindNext(size_t handle, fiFindData* findData) = 0;
	virtual int FindClose(size_t handle) = 0;
	virtual char* FixRelativeName(char* out, size_t s, const char* in) = 0;
	virtual bool Truncate(uint32_t handle) = 0;

	virtual uint32_t GetFileAttributes(const char* path) = 0;
	virtual bool SetFileAttributes(const char* file, uint32_t FileAttributes) = 0;
	virtual int32_t GetResourceVersion(const char* fileName, ResourceFlags* version) = 0;
	virtual uint32_t GetParentHandle() = 0;
	virtual int64_t m_7C(int) = 0;
	virtual void m_80() = 0;
	virtual int32_t m_84(int) = 0;
};

class DEVICE_EXPORT __declspec(novtable) fiDeviceImplemented : public fiDevice
{
protected:
	fiDeviceImplemented();

public:
	virtual ~fiDeviceImplemented();

	virtual uint32_t Open(const char* fileName, bool);

	virtual uint32_t OpenBulk(const char* fileName, uint64_t* ptr);

	virtual uint32_t CreateLocal(const char* fileName);

	virtual uint32_t Create(const char*);

	virtual uint32_t Read(uint32_t handle, void* buffer, uint32_t toRead);

	virtual uint32_t ReadBulk(uint32_t handle, uint64_t ptr, void* buffer, uint32_t toRead);

	virtual uint32_t WriteBulk(int, int, int, int, int);

	virtual uint32_t Write(size_t, void*, int);

	virtual uint32_t Seek(uint32_t handle, int32_t distance, uint32_t method);

	virtual uint64_t SeekLong(uint32_t handle, int64_t distance, uint32_t method);

	virtual int32_t Close(uint32_t handle);

	virtual int32_t CloseBulk(uint32_t handle);

	virtual int GetFileLength(uint32_t handle);

	virtual int m_34();
	virtual bool RemoveFile(const char* file);
	virtual int RenameFile(const char* from, const char* to);
	virtual int CreateDirectory(const char* dir);

	virtual int RemoveDirectory(const char * dir);
	virtual uint64_t GetFileLengthLong(const char* file);
	virtual uint64_t GetFileTime(const char* file);
	virtual bool SetFileTime(const char* file, FILETIME fileTime);

	virtual size_t FindFirst(const char* path, fiFindData* findData);
	virtual bool FindNext(size_t handle, fiFindData* findData);
	virtual int FindClose(size_t handle);
	virtual char* FixRelativeName(char* out, size_t s, const char* in) override;
	virtual bool Truncate(uint32_t handle);

	virtual uint32_t GetFileAttributes(const char* path);
	virtual bool SetFileAttributes(const char* file, uint32_t FileAttributes);
	virtual int32_t GetResourceVersion(const char* fileName, ResourceFlags* version);
	virtual uint32_t GetParentHandle();
	virtual int64_t m_7C(int) override;
	virtual void m_80() override;
	virtual int32_t m_84(int) override;
};

class DEVICE_EXPORT __declspec(novtable) fiDeviceRelative : public fiDeviceImplemented
{
private:
	char m_pad[524];
public:
	fiDeviceRelative();

	// any RAGE path can be used; including root-relative paths
	void SetPath(const char* relativeTo, bool allowRoot);

	// mounts the device in the device stack
	void Mount(const char* mountPoint);
};

class DEVICE_EXPORT __declspec(novtable) fiPackfile : public fiDeviceImplemented
{
private:
	char m_pad[1144];
public:
	fiPackfile();

	// any RAGE path can be used; including root-relative paths
	void OpenPackfile(const char* archive, bool bTrue, bool bFalse, int type);

	// mounts the device in the device stack
	void Mount(const char* mountPoint);

	// closes the package file
	void ClosePackfile();
};
}
