#pragma once
#include <API/VulkanInit.hpp>
#include <HML/HeaderMath.hpp>
namespace Example
{
	struct UniformBuffer
	{
		vk::Buffer buffer;
		vk::DeviceMemory mem;
	};
	struct CameraData
	{
		HML::Mat4x4<> View;
		HML::Mat4x4<> Projection;
	};
	class Camera
	{
	public:
		Camera::Camera(float fov, float aspectRatio, float nearClip, float farClip)
			: m_FOV(fov), m_AspectRatio(aspectRatio), m_NearClip(nearClip), m_FarClip(farClip),
			m_data({ HML::Mat4x4<>(1.0f), HML::ClipSpace::Perspective_RH_ZO(HML::Radians(fov), aspectRatio, nearClip, farClip) })
		{
			UpdateView();
			CreateBuffer();
		}
		void Destroy();
		void CreateBuffer();
		void OnUpdate();
		HML::Vector3<> GetUpDirection() const;
		HML::Vector3<> GetRightDirection() const;
		HML::Vector3<> GetForwardDirection() const;
		HML::Quat<> GetOrientation() const;
		void UpdateProjection();
		void UpdateView();
		void UpdateBuffer();
		UniformBuffer m_CameraUniformBuffer;
	private:
		void MouseRotate();
		float m_FOV = 45.0f, m_AspectRatio = 1.778f, m_NearClip = 0.1f, m_FarClip = 1000.0f;

		float CameraMovemnetSpeed = 0.05f;
		HML::Vector3<> m_Position = { 0.0f, 0.0f, 10.0f };
		HML::Vector3<> m_Rotation = { 0.0f, 0.0f, 0.0f };
		bool RotationFirstClick = false;

		HML::Vector2<> m_InitialMousePosition = { 0.0f, 0.0f };

		float m_Distance = 10.0f;
		float m_Pitch = 0.0f, m_Yaw = 0.0f;
		CameraData m_data;
	};
}