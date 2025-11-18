#pragma once

#include <string>
#include <vector>
#include <openvr.h>

struct VRDevice
{
	int id = -1;
	vr::TrackedDeviceClass deviceClass = vr::TrackedDeviceClass::TrackedDeviceClass_Invalid;
	std::string model = "";
	std::string serial = "";
	std::string trackingSystem = "";
	vr::ETrackedControllerRole controllerRole = vr::ETrackedControllerRole::TrackedControllerRole_Invalid;
};

struct VRState
{
	std::vector<std::string> trackingSystems;
	std::vector<VRDevice> devices;

	[[nodiscard]] int FindDevice(const std::string& trackingSystem, const std::string& model, const std::string& serial) const;

	static VRState Load();
};
struct BodyTrackerIDs {
    int hmd = 0;
    int chest = -1;
    int waist = -1;
    int leftKnee = -1;
    int rightKnee = -1;
    int leftFoot = -1;
    int rightFoot = -1;
};
bool findBodyTrackers(BodyTrackerIDs& result);