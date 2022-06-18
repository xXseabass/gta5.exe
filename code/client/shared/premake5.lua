	-- we have a different shared lib depending on whether we're shared-runtime (launcher)
	-- or dll-runtime (rest of game)

	local function do_shared(libc)
		project(libc and "SharedLibc" or "Shared")
			targetname(libc and "shared_libc" or "shared")
			language "C++"
			kind "StaticLib"
			
			if os.istarget('windows') then
				dependson { 'CfxPrebuild' }
			end

			if libc then
				staticruntime "On"
				add_dependencies { 'vendor:fmtlib-crt' }
			else
				add_dependencies { 'vendor:fmtlib' }
			end
			
			add_dependencies { 'vendor:utfcpp' }

			if os.istarget('windows') then
				links { libc and 'fmtlib-crt' or 'fmtlib' }
			end

			defines "COMPILING_SHARED"

			if libc then
				defines "COMPILING_SHARED_LIBC"
			end

			includedirs { "../citicore" }

			files
			{
				"../../shared/**.cpp", "../../shared/**.h", "**.cpp", "**.h"
			}

			if _OPTIONS["game"] ~= "ny" then
				files("**.asm")
			end

			filter { "system:not windows" }
				excludes { "Hooking.*", "*.asm" }
	end

	do_shared(false)
	do_shared(true)
