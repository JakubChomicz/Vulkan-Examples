#include "Camera.hpp"
#include <Input.hpp>
#include <API/VulkanInit.hpp>
#include <App.hpp>

namespace Example
{
	void Camera::Destroy()
	{
		Core::Context::_device->destroyBuffer(m_CameraUniformBuffer.buffer);
		Core::Context::_device->freeMemory(m_CameraUniformBuffer.mem);
	}
	void Camera::CreateBuffer()
	{
		vk::BufferCreateInfo bufferinfo{};
		bufferinfo.flags = vk::BufferCreateFlags();
		bufferinfo.usage = vk::BufferUsageFlagBits::eUniformBuffer;
		bufferinfo.size = sizeof(CameraData);
		bufferinfo.sharingMode = vk::SharingMode::eExclusive;
		m_CameraUniformBuffer.buffer = Core::Context::_device->createBuffer(bufferinfo);

		Core::Context::allocateBufferMemory(m_CameraUniformBuffer.buffer, m_CameraUniformBuffer.mem);

		void* vmemlocation = Core::Context::_device->mapMemory(m_CameraUniformBuffer.mem, 0, sizeof(CameraData));
		auto temp = static_cast<CameraData*>(vmemlocation);
		temp->View				= m_data.View;
		temp->Projection		= m_data.Projection;
		Core::Context::_device->unmapMemory(m_CameraUniformBuffer.mem);
	}
	void Camera::OnUpdate()
	{
		UpdateProjection();
		if (Core::Input::IsMouseButtonPressed(1))	//Mous button right
		{
			auto [Wposx, Wposy] = Core::Input::GetWindowPos();
			const HML::Vector2<>& mouse{ Core::Input::GetMouseX(), Core::Input::GetMouseY() };
			HML::Vector2<> delta = (mouse - m_InitialMousePosition) * 0.003f;
			m_InitialMousePosition = mouse;
			Core::Input::HideMouseCursor();
			MouseRotate();
			if (Core::Input::IsKeyPressed(65))	//65 = KeyCode A
				m_Position += -GetRightDirection() * CameraMovemnetSpeed;
			if (Core::Input::IsKeyPressed(68))	//68 = KeyCode D
				m_Position += GetRightDirection() * CameraMovemnetSpeed;
			if (Core::Input::IsKeyPressed(87))	//87 = KeyCode W
				m_Position += GetForwardDirection() * CameraMovemnetSpeed;
			if (Core::Input::IsKeyPressed(83))	//83 = KeyCode S
				m_Position += -GetForwardDirection() * CameraMovemnetSpeed;
		}
		else
			Core::Input::ShowMouseCursor();
		UpdateView();
		UpdateBuffer();
	}
	HML::Vector3<> Camera::GetUpDirection() const
	{
		return HML::Transform::Rotate(GetOrientation(), HML::Vector3<>(0.0f, 1.0f, 0.0f));
	}
	HML::Vector3<> Camera::GetRightDirection() const
	{
		return HML::Transform::Rotate(GetOrientation(), HML::Vector3<>(1.0f, 0.0f, 0.0f));
	}
	HML::Vector3<> Camera::GetForwardDirection() const
	{
		return HML::Transform::Rotate(GetOrientation(), HML::Vector3<>(0.0f, 0.0f, 1.0f));
	}
	HML::Quat<> Camera::GetOrientation() const
	{
		return HML::Quat<>(HML::Vector3<>(m_Pitch, m_Yaw, 0.0f));
	}
	void Camera::UpdateProjection()
	{
		m_AspectRatio = float(Core::Context::_swapchain.extent.width)
			/ float(Core::Context::_swapchain.extent.height);
		m_data.Projection = HML::ClipSpace::Perspective_RH_ZO(m_FOV, m_AspectRatio, m_NearClip, m_FarClip);
	}
	void Camera::UpdateView()
	{
		m_data.View = HML::Transform::View_RH(m_Position, m_Position + GetForwardDirection(), -GetUpDirection());
	}
	void Camera::UpdateBuffer()
	{
		void* vmemlocation = Core::Context::_device->mapMemory(m_CameraUniformBuffer.mem, 0, sizeof(CameraData));
		auto temp = static_cast<CameraData*>(vmemlocation);
		temp->View = m_data.View;
		temp->Projection = m_data.Projection;
		Core::Context::_device->unmapMemory(m_CameraUniformBuffer.mem);
	}
	void Camera::MouseRotate()
	{
		//Rotationg
		auto center = Core::App::Get().GetWindow().GetWindowCenter();
		//Core::Input::SetMousePos(center.first, center.second);
		const HML::Vector2<>& mouse{ Core::Input::GetMouseX(), Core::Input::GetMouseY() };
		Core::Input::SetMousePos(center.first, center.second);
		HML::Vector2<> delta;
		delta.y = 1.6f * (float)(mouse.y - center.second) / Core::Context::_swapchain.swapchainHeight;
		delta.x = 1.6f * (float)(mouse.x - center.first) / Core::Context::_swapchain.swapchainWidth;
		float yawSign = GetUpDirection().y < 0 ? -1.0f : 1.0f;
		float RotationSpeed = 0.4f;
		m_Yaw += yawSign * delta.x * RotationSpeed;
		m_Pitch += delta.y * RotationSpeed;
		if (m_Pitch > 1.5707963f)
			m_Pitch = 1.5707963f;
		else if (m_Pitch < -1.5707963f)
			m_Pitch = -1.5707963f;
	}
}