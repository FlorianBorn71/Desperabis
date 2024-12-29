#pragma once
#include <openxr/openxr.h>


class DesperabisOpenXrCamera
{
public:
	DesperabisOpenXrCamera();

	bool Initialize(XrInstance instance, XrSession session, XrActionSet actionSet);
	void Update(XrSpace appSpace, XrTime currentTime);
	//void SetControllerState(const XrActionStateVector2f& thumbstickState, const glm::quat& viewRotation);

	void LoadLevel(int index);
	void IncLevelIndex(int delta) { LoadLevel(m_currentLevelIndex + delta); }
protected:
	XrActionSet m_actionSet = XR_NULL_HANDLE;
	XrSession m_session = XR_NULL_HANDLE;
	XrSpace m_headSpace = XR_NULL_HANDLE;

	XrAction m_thumbstickAction = XR_NULL_HANDLE;
	XrAction m_buttonXAction = XR_NULL_HANDLE;
	XrAction m_buttonYAction = XR_NULL_HANDLE;

	XrVector3f m_velocity = { 0,0,0 };
	int m_currentLevelIndex = 0;
};

