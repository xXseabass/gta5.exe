-- tinyxml2 with static runtime, as used for launcher
return {
	include = function()
		includedirs "../vendor/tinyxml2/"
	end,

	run = function()
		language "C++"
		kind "StaticLib"

		staticruntime 'On'

		files {
			"../vendor/tinyxml2/tinyxml2.cpp"
		}
	end
}