#pragma once
#include <iostream>
namespace Core
{
	class Input
	{
	public:
		static bool IsKeyPressed(int keycode);
		static bool IsMouseButtonPressed(int button);
		static std::pair<float, float> GetMousePos();
		static void SetMousePos(float x, float y);
		static void HideMouseCursor();
		static void ShowMouseCursor();
		static float GetMouseX();
		static float GetMouseY();
		static std::pair<int, int> GetWindowPos();
	};
}