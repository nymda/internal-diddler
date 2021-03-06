#include "Global.h"
#include "mem.h"
#include "maths.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace noclip {
	bool enabled = false;
	bool inNoclip = false;
	byte xBuffer[5]       = { 0xF2, 0x0F, 0x10, 0x43, 0x7C };
	byte yBuffer[8]       = { 0xF3, 0x0F, 0x11, 0x93, 0x80, 0x00, 0x00, 0x00 };
	byte yBuffer2[5]      = { 0xF2, 0x0F, 0x11, 0x43, 0x7C }; //why the fuck is this nessicary? why do we need two instructions writing to the SAME FUCKING VARIABLE in the SAME FUCKING LOOP? WHAT ARE YOU DOING DENNIS?
	byte zBuffer[8]       = { 0xF3, 0x0F, 0x11, 0x83, 0x84, 0x00, 0x00, 0x00 };
	 
	td::Vec3 movementVector = { 0, 0, 0 };
	td::Vec3 lastMmovementVector = { 0, 0, 0 };
	td::Vec3 cameraForwardVector = { 0, 0, 0 };
	td::Vec3 cameraSideVector = { 0, 0, 0 };
	td::Vec4 cameraQuat = { 0, 0, 0, 0 };


	void setCameraEnabled(bool enable) {
		uintptr_t baseAddy = (uintptr_t)glb::oCamPos;
		uintptr_t xInstruction   = baseAddy + 0x658; // ok
		uintptr_t yInstruction   = baseAddy + 0x650; // ok 
		uintptr_t yInstruction2  = baseAddy + 0x661; // ok 
		uintptr_t zInstruction   = baseAddy + 0x614; // ok

		if (enable) {
			mem::Patch((byte*)xInstruction,  xBuffer, 5);
			mem::Patch((byte*)yInstruction,  yBuffer, 8);
			mem::Patch((byte*)yInstruction2, yBuffer2, 5);
			mem::Patch((byte*)zInstruction,  zBuffer, 8);

		}
		else {
			mem::Nop((byte*)xInstruction,  5);
			mem::Nop((byte*)yInstruction,  8);
			mem::Nop((byte*)yInstruction2, 5);
			mem::Nop((byte*)zInstruction,  8);
		}
	}


	void ToggleNoclip() {


		inNoclip = !inNoclip;
		if (inNoclip) {
			setCameraEnabled(false);
		}
		else {

			setCameraEnabled(true);
			glb::player->position = { glb::player->cameraPosition.x, glb::player->cameraPosition.y - 1.7f, glb::player->cameraPosition.z };
			glb::player->velocity = { lastMmovementVector.x * 50, lastMmovementVector.y * 50, lastMmovementVector.z * 50 };
		}
	}

	float speed = 0.5;
	void update() {
		if (!inNoclip) {
			return;
		}

		glm::quat camQuatGLM(glm::vec3(glb::player->camPitch, glb::player->camYaw, 0));
		cameraQuat = { camQuatGLM.x, camQuatGLM.y, camQuatGLM.z, camQuatGLM.w };
		td::Vec3* camPos = &glb::player->cameraPosition;
		float* sm = &glb::player->SMove;
		float* fm = &glb::player->FMove;

		movementVector = { 0, 0, 0 };

		cameraForwardVector.x =		2 * (cameraQuat.w * cameraQuat.y + cameraQuat.z * cameraQuat.x);
		cameraForwardVector.y =		2 * (cameraQuat.x * cameraQuat.y - cameraQuat.z * cameraQuat.w);
		cameraForwardVector.z = 1 - 2 * (cameraQuat.w * cameraQuat.w + cameraQuat.x * cameraQuat.x);

		cameraSideVector.x =	1 - 2 * (cameraQuat.x * cameraQuat.x + cameraQuat.y * cameraQuat.y);
		cameraSideVector.y =		2 * (cameraQuat.w * cameraQuat.x + cameraQuat.z * cameraQuat.y);
		cameraSideVector.z =		2 * (cameraQuat.w * cameraQuat.y - cameraQuat.z * cameraQuat.x);

		if(*fm == 1.f) {
			movementVector.x += -(cameraForwardVector.x);
			movementVector.y += -(cameraForwardVector.y);
			movementVector.z += -(cameraForwardVector.z);
		}
		if (*fm == -1.f) {
			movementVector.x -= -(cameraForwardVector.x);
			movementVector.y -= -(cameraForwardVector.y);
			movementVector.z -= -(cameraForwardVector.z);
		}
		if (*sm == 1.f) {
			movementVector.x += (cameraSideVector.x);
			movementVector.y += (cameraSideVector.y);
			movementVector.z += (cameraSideVector.z);
		}
		if (*sm == -1.f) {
			movementVector.x -= (cameraSideVector.x);
			movementVector.y -= (cameraSideVector.y);
			movementVector.z -= (cameraSideVector.z);
		}

		if (((GetAsyncKeyState(VK_SHIFT) >> 15) & 0x0001) == 0x0001) {
			speed = 0.5f; //fast
		}
		else if (((GetAsyncKeyState(VK_CONTROL) >> 15) & 0x0001) == 0x0001) {
			speed = 10.0f; //slow
		}
		else {
			speed = 3.0f; //normal
		}

		movementVector.x = movementVector.x / speed;
		movementVector.y = movementVector.y / speed;
		movementVector.z = movementVector.z / speed;

		if (*fm != 0 || *sm != 0) {
			camPos->x = camPos->x + movementVector.x;
			camPos->y = camPos->y + movementVector.y;
			camPos->z = camPos->z + movementVector.z;
		}

		if (((GetAsyncKeyState(VK_SPACE) >> 15) & 0x0001) == 0x0001) {
			if (((GetAsyncKeyState(VK_SHIFT) >> 15) & 0x0001) == 0x0001) {
				camPos->y = camPos->y + 2.5;
			}
			else {
				camPos->y = camPos->y + 0.5;
			}
		}

		glb::player->position = { 0, 500, 0 };
		glb::player->velocity = { 0, 0, 0 };

		lastMmovementVector = movementVector;

	}
}