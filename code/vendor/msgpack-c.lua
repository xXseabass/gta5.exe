return {
	include = function()
		includedirs { "../vendor/msgpack-c/src", "../vendor/msgpack-c/include/", "deplibs/include/msgpack-c/" }
	end,

	run = function()
		targetname "msgpack-c"
		language "C++"
		kind "StaticLib"
		defines { "MSGPACK_DLLEXPORT=" }

		files
		{
			"../vendor/msgpack-c/include/**.hpp",
			"../vendor/msgpack-c/src/*.c" 
		}
	end
}