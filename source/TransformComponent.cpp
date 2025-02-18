#include "TransformComponent.h"
#include "Factory.h"
#include "Utils.h"

#include <imgui.h>
#include <DirectXMath.h>

#include "RigidbodyComponent.h"

const Creator<IComponent, TransformComponent> g_TransformCreator{};

TransformComponent::TransformComponent(DirectX::XMFLOAT3 pos, DirectX::XMFLOAT3 rotation, DirectX::XMFLOAT3 scale)
	: IComponent{}
	, m_Position{ pos }
	, m_Rotation{0,0,0,0}
	, m_Scale{ scale }
{
	SetRotation(rotation);
}

void TransformComponent::Start()
{
	GetWorldMatrix();
	UpdateDirections();

	m_pRigidbodyComponent = m_pGameobject->GetComponent<RigidBodyComponent>();
}

void TransformComponent::Update()
{
	GetWorldMatrix();
	UpdateDirections();
}

void TransformComponent::RegisterMembers()
{
	ClassMeta<TransformComponent>::AddMemberPtr("Position", &TransformComponent::m_Position);
	ClassMeta<TransformComponent>::AddMemberPtr("Rotation", &TransformComponent::m_Rotation);
	ClassMeta<TransformComponent>::AddMemberPtr("Scale", &TransformComponent::m_Scale);
}

void TransformComponent::RenderGUI()
{
	static bool rawview;
	ImGui::Checkbox("Rawview", &rawview);
	if(rawview)
	{
		ClassMeta<TransformComponent>::RenderGUI<TransformComponent>(*this);
		return;
	}

	ImGui::Input("Position", m_Position);

	auto rotation = QuaternionToEuler(m_Rotation);
	rotation.x *= static_cast<float>(TO_DEGREES);
	rotation.y *= static_cast<float>(TO_DEGREES);
	rotation.z *= static_cast<float>(TO_DEGREES);
	if (ImGui::Input("Rotation", rotation))
		SetRotation(rotation);

	ImGui::Input("Scale", m_Scale);

}

void TransformComponent::Serialize(rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer)
{
	ClassMeta<TransformComponent>::Serialize<TransformComponent>(*this, writer);
}

void TransformComponent::Deserialize(const rapidjson::Value& value)
{
	m_pGameobject->RemoveComponent(m_pGameobject->GetComponent<TransformComponent>());
	ClassMeta<TransformComponent>::Deserialize(*this, value);
}

DirectX::XMFLOAT4X4 TransformComponent::GetWorldMatrix()
{
	if (!m_Dirty) return m_WorldMatrix;

	auto rotationQuaternion = DirectX::XMLoadFloat4(&m_Rotation);//DirectX::XMQuaternionRotationRollPitchYaw(m_Rotation.x * static_cast<float>(TO_RADIANS), m_Rotation.y * static_cast<float>(TO_RADIANS), m_Rotation.z * static_cast<float>(TO_RADIANS));
	auto worldMatrix = DirectX::XMMatrixScaling(m_Scale.x, m_Scale.y, m_Scale.z) * DirectX::XMMatrixRotationQuaternion(rotationQuaternion) * DirectX::XMMatrixTranslation(m_Position.x, m_Position.y, m_Position.z);

	if (m_pGameobject->GetParent() != nullptr)
	{
		const auto parentWorld = m_pGameobject->GetParent()->GetTransform()->GetWorldMatrix();
		worldMatrix *= DirectX::XMLoadFloat4x4(&parentWorld);
	}

	DirectX::XMFLOAT4X4 matrixToReturn;

	DirectX::XMStoreFloat4x4(&matrixToReturn, worldMatrix);
	m_WorldMatrix = matrixToReturn;

	return matrixToReturn;
}

DirectX::XMFLOAT3 TransformComponent::GetPosition() const
{
	if (m_pGameobject->GetParent() == nullptr) return m_Position;

	auto position = DirectX::XMLoadFloat3(&m_Position);
	auto parentpos = m_pGameobject->GetParent()->GetTransform()->GetPosition();
	auto parentPosition = DirectX::XMLoadFloat3(&parentpos);

	DirectX::XMFLOAT3 positionToReturn;
	DirectX::XMStoreFloat3(&positionToReturn, DirectX::XMVectorAdd(position, parentPosition));

	return positionToReturn;
}

void TransformComponent::SetPosition(DirectX::XMFLOAT3 position)
{
	m_Position = position;
	m_Dirty = true;

	if (m_pRigidbodyComponent == nullptr) return;

	m_pRigidbodyComponent->Translate(GetPosition());
}

DirectX::XMFLOAT4 TransformComponent::GetRotation() const
{
	if (m_pGameobject->GetParent() == nullptr) return m_Rotation;

	auto rotation = DirectX::XMLoadFloat4(&m_Rotation);
	auto parentrot = m_pGameobject->GetParent()->GetTransform()->GetRotation();
	auto parentRotation = DirectX::XMLoadFloat4(&parentrot);

	DirectX::XMFLOAT4 rotationToReturn;
	DirectX::XMStoreFloat4(&rotationToReturn, DirectX::XMQuaternionMultiply(rotation, parentRotation));

	return rotationToReturn;
}

void TransformComponent::SetRotation(float x, float y, float z)
{
	DirectX::XMStoreFloat4(&m_Rotation, DirectX::XMQuaternionRotationRollPitchYaw(DirectX::XMConvertToRadians(x), DirectX::XMConvertToRadians(y), DirectX::XMConvertToRadians(z)));
	m_Dirty = true;

	if (m_pRigidbodyComponent == nullptr) return;

	m_pRigidbodyComponent->Rotate(GetRotation());
}

void TransformComponent::SetRotation(DirectX::XMFLOAT3 rotation)
{
	SetRotation(rotation.x, rotation.y, rotation.z);
}

DirectX::XMFLOAT3 TransformComponent::GetScale() const
{
	return m_Scale;
}

void TransformComponent::SetScale(DirectX::XMFLOAT3 scale)
{
	m_Scale = scale;
}

void TransformComponent::SetParent(TransformComponent* /*pTransformComponent*/)
{
}

DirectX::XMFLOAT3& TransformComponent::GetForward()
{
	return m_Forward;
}

DirectX::XMFLOAT3& TransformComponent::GetRight()
{
	return m_Right;
}

DirectX::XMFLOAT3& TransformComponent::GetUp()
{
	return m_Up;
}

void TransformComponent::UpdateDirections()
{
	if (!m_Dirty) return;

	auto worldMatrix = DirectX::XMLoadFloat4x4(&m_WorldMatrix);

	DirectX::XMVECTOR pos, rot, scale;
	DirectX::XMMatrixDecompose(&scale, &rot, &pos, worldMatrix);

	auto rotMat = DirectX::XMMatrixRotationQuaternion(rot);
	const auto forward = DirectX::XMVector3TransformCoord(DirectX::XMVectorSet(0, 0, 1, 0), rotMat);
	const auto right = DirectX::XMVector3TransformCoord(DirectX::XMVectorSet(1, 0, 0, 0), rotMat);
	const auto up = DirectX::XMVector3Cross(forward, right);

	DirectX::XMStoreFloat3(&m_Forward, forward);
	DirectX::XMStoreFloat3(&m_Right, right);
	DirectX::XMStoreFloat3(&m_Up, up);
}