#include "DesperabisOpenXrCamera.h"
#include <windows.h>
#include <Scene/Scene.h>
#include <Sample/LevelScene.h>

extern XrVector3f g_playerPos;
extern std::unique_ptr<Scene> g_desperabisScene;

DesperabisOpenXrCamera::DesperabisOpenXrCamera()
{

}

bool DesperabisOpenXrCamera::Initialize(XrInstance instance, XrSession session, XrActionSet actionSet)
{
	m_session = session;
	m_actionSet = actionSet;
	XrResult result;

	// create head space
	{
		XrReferenceSpaceCreateInfo createInfo = {};
		createInfo.type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO;
		createInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW; // Headset space
		createInfo.poseInReferenceSpace.orientation = { 0, 0, 0, 1 }; // Identity quaternion
		createInfo.poseInReferenceSpace.position = { 0, 0, 0 };       // Zero translation

		result = xrCreateReferenceSpace(session, &createInfo, &m_headSpace);
		if (XR_FAILED(result)) {
			return false; // Handle error
		}
	}

	/*
	// Create the action set for the camera
	XrActionSetCreateInfo actionSetInfo{ XR_TYPE_ACTION_SET_CREATE_INFO };
	strcpy(actionSetInfo.actionSetName, "camera_action_set");
	strcpy(actionSetInfo.localizedActionSetName, "Camera Action Set");
	actionSetInfo.priority = 0;

	result = xrCreateActionSet(instance, &actionSetInfo, &m_actionSet);
	if (XR_FAILED(result)) {
		return false; // Handle error
	}
	*/
	// Create the thumbstick action within the camera's action set
	XrActionCreateInfo actionCreateInfo{ XR_TYPE_ACTION_CREATE_INFO };
	actionCreateInfo.actionType = XR_ACTION_TYPE_VECTOR2F_INPUT;
	strcpy(actionCreateInfo.actionName, "right_thumbstick");
	strcpy(actionCreateInfo.localizedActionName, "Right Thumbstick");
	actionCreateInfo.countSubactionPaths = 0;
	actionCreateInfo.subactionPaths = nullptr;

	result = xrCreateAction(m_actionSet, &actionCreateInfo, &m_thumbstickAction);
	if (XR_FAILED(result)) {
		return false; // Handle error
	}

	{
		// Create the X button action within the camera's action set
		XrActionCreateInfo xButtonCreateInfo{ XR_TYPE_ACTION_CREATE_INFO };
		xButtonCreateInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
		strcpy(xButtonCreateInfo.actionName, "x_button");
		strcpy(xButtonCreateInfo.localizedActionName, "X Button");
		xButtonCreateInfo.countSubactionPaths = 0;
		xButtonCreateInfo.subactionPaths = nullptr;

		result = xrCreateAction(m_actionSet, &xButtonCreateInfo, &m_buttonXAction);
		if (XR_FAILED(result)) {
			return false; // Handle error
		}
	}

	{
		// Create the Y button action within the camera's action set
		XrActionCreateInfo yButtonCreateInfo{ XR_TYPE_ACTION_CREATE_INFO };
		yButtonCreateInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
		strcpy(yButtonCreateInfo.actionName, "y_button");
		strcpy(yButtonCreateInfo.localizedActionName, "Y Button");
		yButtonCreateInfo.countSubactionPaths = 0;
		yButtonCreateInfo.subactionPaths = nullptr;

		result = xrCreateAction(m_actionSet, &yButtonCreateInfo, &m_buttonYAction);
		if (XR_FAILED(result)) {
			return false; // Handle error
		}
	}

	// Suggest interaction profile bindings for the thumbstick action
	XrPath interactionProfilePath;
	xrStringToPath(instance, "/interaction_profiles/oculus/touch_controller", &interactionProfilePath);

	XrPath rightThumbstickPath;
	xrStringToPath(instance, "/user/hand/right/input/thumbstick", &rightThumbstickPath);

	// Binding for the X button on the left hand
	XrPath xButtonPath;
	xrStringToPath(instance, "/user/hand/left/input/x/click", &xButtonPath);

	// Binding for the Y button on the left hand
	XrPath yButtonPath;
	xrStringToPath(instance, "/user/hand/left/input/y/click", &yButtonPath);

	// Create the suggested bindings array
	XrActionSuggestedBinding suggestedBindings[] = {
		{ m_thumbstickAction, rightThumbstickPath },
		{ m_buttonXAction, xButtonPath },
		{ m_buttonYAction, yButtonPath }
	};

	XrInteractionProfileSuggestedBinding profileSuggestedBindings{ XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING };
	profileSuggestedBindings.interactionProfile = interactionProfilePath;
	profileSuggestedBindings.suggestedBindings = suggestedBindings;
	profileSuggestedBindings.countSuggestedBindings = sizeof(suggestedBindings) / sizeof(suggestedBindings[0]);

	result = xrSuggestInteractionProfileBindings(instance, &profileSuggestedBindings);
	if (XR_FAILED(result)) {
		return false; // Handle error
	}

	/*
	// Attach the camera's action set to the session
	XrSessionActionSetsAttachInfo attachInfo{ XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO };
	attachInfo.countActionSets = 1;
	attachInfo.actionSets = &m_actionSet;

	result = xrAttachSessionActionSets(session, &attachInfo);
	if (XR_FAILED(result)) {
		return false; // Handle error
	}
	*/
	return true;
}

void DesperabisOpenXrCamera::Update(XrSpace appSpace, XrTime currentTime)
{
	// Retrieve the current state of the thumbstick
	XrActionStateGetInfo getInfo{ XR_TYPE_ACTION_STATE_GET_INFO };
	getInfo.action = m_thumbstickAction;

	XrActionStateVector2f thumbstickState{ XR_TYPE_ACTION_STATE_VECTOR2F };
	xrGetActionStateVector2f(m_session, &getInfo, &thumbstickState);

	if (thumbstickState.isActive) 
	{
		//glm::quat viewRotation = ...; // Retrieve the current view rotation from the HMD's pose
		//SetControllerState(thumbstickState, viewRotation);
	}

	// Get the head pose (view pose) using xrLocateSpace
	XrSpaceLocation headLocation{ XR_TYPE_SPACE_LOCATION };
	XrResult result = xrLocateSpace(m_headSpace, appSpace, currentTime, &headLocation);
/*
	// HACK:
	thumbstickState.isActive = true;
	thumbstickState.currentState.x = 1.f;
	thumbstickState.currentState.y = 0.f;
*/

	if (XR_SUCCEEDED(result) && (headLocation.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) && thumbstickState.isActive)
	{

		// Extract orientation from the head location
		XrQuaternionf headOrientation = headLocation.pose.orientation;

		// Convert quaternion to forward/right vector:
		XrVector3f forward, right;
		forward.x = 2.0f * (headOrientation.x * headOrientation.z + headOrientation.w * headOrientation.y);
		forward.y = 2.0f * (headOrientation.y * headOrientation.z - headOrientation.w * headOrientation.x);
		forward.z = 1.0f - 2.0f * (headOrientation.x * headOrientation.x + headOrientation.y * headOrientation.y);

		right.x = 1.0f - 2.0f * (headOrientation.y * headOrientation.y + headOrientation.z * headOrientation.z);
		right.y = 2.0f * (headOrientation.x * headOrientation.y + headOrientation.w * headOrientation.z);
		right.z = 2.0f * (headOrientation.x * headOrientation.z - headOrientation.w * headOrientation.y);

		// 3. Get the forward direction from the head's orientation
		XrVector3f speed;
		speed.x = right.x * thumbstickState.currentState.x - forward.x * thumbstickState.currentState.y;
		speed.y = right.y * thumbstickState.currentState.x - forward.y * thumbstickState.currentState.y;
		speed.z = right.z * thumbstickState.currentState.x - forward.z * thumbstickState.currentState.y;

		float acc = 0.007f;
		m_velocity.x += speed.x * acc;
		m_velocity.y += speed.y * acc;
		m_velocity.z += speed.z * acc;
	}

	float dampen = 0.9f;
	m_velocity.x *= dampen;
	m_velocity.y *= dampen;
	m_velocity.z *= dampen;
	g_playerPos.x += m_velocity.x;
	g_playerPos.y += m_velocity.y;
	g_playerPos.z += m_velocity.z;

	// Retrieve the current state of the X button
	{
		XrActionStateGetInfo xButtonGetInfo{ XR_TYPE_ACTION_STATE_GET_INFO };
		xButtonGetInfo.action = m_buttonXAction;

		XrActionStateBoolean xButtonState{ XR_TYPE_ACTION_STATE_BOOLEAN };
		xrGetActionStateBoolean(m_session, &xButtonGetInfo, &xButtonState);

		if (xButtonState.isActive && xButtonState.currentState && xButtonState.changedSinceLastSync)
		{
			IncLevelIndex(1);
		}
	}
	// Retrieve the current state of the Y button
	{
		XrActionStateGetInfo yButtonGetInfo{ XR_TYPE_ACTION_STATE_GET_INFO };
		yButtonGetInfo.action = m_buttonYAction;

		XrActionStateBoolean yButtonState{ XR_TYPE_ACTION_STATE_BOOLEAN };
		xrGetActionStateBoolean(m_session, &yButtonGetInfo, &yButtonState);

		if (yButtonState.isActive && yButtonState.currentState && yButtonState.changedSinceLastSync)
		{
			IncLevelIndex(-1);
		}
	}
}

const int SupportedLevels[] = {
	10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26, 29,30,31,32,33,36,34,35, 50,51, 60, 90,91,92,95,96,97,98,99
};

void DesperabisOpenXrCamera::LoadLevel(int index)
{
	const int levelCount = sizeof(SupportedLevels) / sizeof(SupportedLevels[0]);
	while (index < 0) index += levelCount;
	index %= levelCount;
	m_currentLevelIndex = index;
	g_desperabisScene = std::make_unique<LevelScene>(SupportedLevels[m_currentLevelIndex]);
	CO_AWAIT g_desperabisScene->CreateScene();
	g_playerPos = { 0,0,0 };
}
