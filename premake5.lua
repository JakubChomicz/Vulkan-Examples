include "dependencies.lua"
workspace "Vulkan Examples"
	architecture "x64"
	startproject "Cube"

	configurations
	{
		"Debug",
		"Release"
	}

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

group "Dependencies"
	include "Core/dependencies/GLFW"
	include "Core/dependencies/assimp"

group "Core"
	include "Core"

group "Examples"
	include "Cube"
	include "Monke"
	include "PlayerController"