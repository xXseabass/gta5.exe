return {
	include = function()
		includedirs { "vendor/folly/", "../vendor/folly/", "../vendor/chromium/base/third_party/double_conversion/", "../vendor/fmtlib/include" }
	end,

	run = function()
		language "C++"
		kind "StaticLib"
		
		defines { 'FOLLY_NO_CONFIG' }
		
		if os.istarget('linux') then
			defines { 'FOLLY_HAVE_RECVMMSG' }
		end

		files_project '../vendor/folly/folly/'
		{
			'IPAddress.cpp',
			'IPAddressV4.cpp',
			'IPAddressV6.cpp',
			'detail/IPAddress.cpp',
			'hash/*.cpp',
			'hash/detail/*.cpp',
			'lang/Assume.cpp',
			'Conv.cpp',
			'String.cpp',
			'Format.cpp',
			'memory/detail/MallocImpl.cpp',
			'MacAddress.cpp',
			'ScopeGuard.cpp',
			'portability/Sockets.cpp',
			'net/NetOps.cpp',
			'net/detail/SocketFileDescriptorMap.cpp',
			-- SharedMutex
			'synchronization/SanitizeThread.cpp',
			'synchronization/ParkingLot.cpp',
			'concurrency/*.cpp', 
			'portability/Builtins.h',
			'portability/Builtins.cpp',
			'portability/Unistd.cpp',
			'portability/SysResource.cpp',
			'detail/Futex.cpp',
			'SharedMutex.cpp',
			'lang/SafeAssert.cpp',
			'FileUtil.cpp',
			'logging/LogLevel.h',
			'logging/LogLevel.cpp',
		}
		
		files_project '../vendor/chromium/base/' {
			"third_party/double_conversion/double-conversion/bignum-dtoa.cc",
			"third_party/double_conversion/double-conversion/bignum-dtoa.h",
			"third_party/double_conversion/double-conversion/bignum.cc",
			"third_party/double_conversion/double-conversion/bignum.h",
			"third_party/double_conversion/double-conversion/cached-powers.cc",
			"third_party/double_conversion/double-conversion/cached-powers.h",
			"third_party/double_conversion/double-conversion/diy-fp.h",
			"third_party/double_conversion/double-conversion/double-conversion.h",
			"third_party/double_conversion/double-conversion/double-to-string.cc",
			"third_party/double_conversion/double-conversion/double-to-string.h",
			"third_party/double_conversion/double-conversion/fast-dtoa.cc",
			"third_party/double_conversion/double-conversion/fast-dtoa.h",
			"third_party/double_conversion/double-conversion/fixed-dtoa.cc",
			"third_party/double_conversion/double-conversion/fixed-dtoa.h",
			"third_party/double_conversion/double-conversion/ieee.h",
			"third_party/double_conversion/double-conversion/string-to-double.cc",
			"third_party/double_conversion/double-conversion/string-to-double.h",
			"third_party/double_conversion/double-conversion/strtod.cc",
			"third_party/double_conversion/double-conversion/strtod.h",
			"third_party/double_conversion/double-conversion/utils.h",
		}
	end
}