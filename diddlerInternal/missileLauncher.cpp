#include <vector>
#include "Global.h"
#include "Raycaster.h"
#include "objectSpawner.h"
#include "TDObjects.h"
#include "Mods.h"
#include "maths.h"
#include "camera.h"
#include "toolgun.h"
#include "drawCube.h"

namespace missile {
    struct missileObject {
        spawner::KMSpawnedObject obj;
        int age = 0;
        bool destroyed = false;
    };

	std::vector<missileObject> launchedMissiles = {};
    missileObject mostRecentMissile = {};

    bool runOnce = false;
    td::Color white{ 1.f, 1.f, 1.f, 1.f };

	void launchMissile() {
        spawner::objectSpawnerParams osp;
        td::Vec3 cameraDirection = glb::player->cameraEuler();
        osp.spawnType = spawner::objectSpawnType::front;
        osp.rotateFacePlayer = true;
        osp.unbreakable = true;
        osp.nocull = true;
        osp.pushSpawnList = false;
        osp.customRotation = glb::player->cameraQuat;
        osp.backTranslate = { 0.85f, 0.15f, 0.15f };
        osp.startVelocity = { cameraDirection.x * 100, cameraDirection.y * 100, cameraDirection.z * 100 };

        const char* currentPath = "vox\\Default\\missile\\object.vox";
        mostRecentMissile = { spawner::spawnObjectProxy(currentPath, osp), 0 };
        launchedMissiles.push_back(mostRecentMissile);
        //mostRecentMissile = {};
	}

    int lastResolution = 64;
    bool flip = true;
    byte* frameBuffer;
    bool genFilter = true;
    float* pixels = nullptr;
    float minDist = 1000.f;
    float maxDist = 0.f;
    int deadCameraframes = 0;
    byte* pixelsColor = nullptr;
    RaycastFilter rcf = {};

    void updateCamera() {
        int res = toolgun::cameraResolution;
        float fov = 2.f;
        int pixelOffset = 0;
        flip = !flip;

        if (!mostRecentMissile.obj.body || !mostRecentMissile.obj.shape) {
            if (deadCameraframes > 0) {
                free(pixelsColor);
                pixelsColor = new byte[(res * res) * 4];

                for (int y = res; y > 0; y--) {
                    for (int x = 0; x < res; x++) {
                        int shade = rand() % 255;
                        pixelsColor[pixelOffset] = (byte)(shade);
                        pixelsColor[pixelOffset + 1] = (byte)(shade);
                        pixelsColor[pixelOffset + 2] = (byte)(shade);
                        pixelsColor[pixelOffset + 3] = (byte)(255);
                        pixelOffset += 4;
                    }
                }
                camera::constructFrameManual(pixelsColor, res, false);
                camera::drawCameraWindow(0.f);
                deadCameraframes--;
            }
            else {
                if (toolgun::cameraResolution != lastResolution || !frameBuffer) {
                    free(frameBuffer);
                    frameBuffer = new byte[(toolgun::cameraResolution * toolgun::cameraResolution) * 4];
                }
                camera::interlacedImage(frameBuffer, toolgun::cameraResolution, flip, fov, 1.f, (glm::quat*)&glb::player->cameraQuat, glb::player->cameraPosition, { 0, 0, -1 }, { 0, 1, 0 }, &rcf);
                camera::constructFrameManual(frameBuffer, toolgun::cameraResolution, false);
                camera::drawCameraWindow(0.f);
            }
            return; 
        }


        td::Color red{ 1.f, 0.f, 0.f, 1.f };
        glm::vec3 bodyPos = glm::vec3(mostRecentMissile.obj.body->Position.x, mostRecentMissile.obj.body->Position.y, mostRecentMissile.obj.body->Position.z);
        glm::quat bodyQuat = *(glm::quat*)&mostRecentMissile.obj.body->Rotation;
        glm::vec3 vx = bodyQuat * glm::vec3(1, 0, 0);
        glm::vec3 vy = bodyQuat * glm::vec3(0, 1, 0);
        glm::vec3 vz = bodyQuat * glm::vec3(0, 0, 1); //(UP)
        glm::vec3 centerpoint = bodyPos + ((vz * 0.4f) + (vy * 0.4f) + (vx * 0.2f));

        deadCameraframes = 30;

        camera::drawCameraWindow(0.f);

        if (toolgun::cameraResolution != lastResolution || !frameBuffer) {
            free(frameBuffer);
            frameBuffer = new byte[(toolgun::cameraResolution * toolgun::cameraResolution) * 4];
        }
        lastResolution = toolgun::cameraResolution;
        if (camera::transparency) {
            rcf.m_RejectTransparent = true;
        }
        else {
            rcf.m_RejectTransparent = false;
        }

        rcf.m_IgnoredShapes.push_back(mostRecentMissile.obj.shape);

        camera::interlacedImage(frameBuffer, toolgun::cameraResolution, flip, fov, 1.f, (glm::quat*)&mostRecentMissile.obj.body->Rotation, { centerpoint.x, centerpoint.y, centerpoint.z }, { 0, 0, -1 }, { 0, 1, 0 }, &rcf);
        camera::constructFrameManual(frameBuffer, toolgun::cameraResolution, false);

    }

    td::Color green{ 0.f, 1.f, 0.f, 1.f };
    void runMissile() {
        if (glb::game->State == gameState::menu) {
            launchedMissiles.clear();
        }
          
        const char* launcherName = "launcher";
        if (memcmp(glb::player->heldItemName, launcherName, 8) == 0) {
            updateCamera();

            if (glb::player->isAttacking == true) {
                if (runOnce) {
                    launchMissile();
                    runOnce = false;
                }
            }
            else {
                runOnce = true;
            }
        }

        bool hasActiveMissile = false;

        for (missileObject& missile : launchedMissiles)
        {
            if (!missile.destroyed) {  
                hasActiveMissile = true;

                uintptr_t special = *(uintptr_t*)((uintptr_t)glb::scene + 0x68);
                missile.age++;

                td::Vec3 rotationEul = quat2euler(missile.obj.body->Rotation, 0, true);
                td::Vec3 rotationEulForward = { rotationEul.x * 1.5f, rotationEul.y * 1.5f, rotationEul.z * 1.5f };

                float totalSpeed = missile.obj.body->Velocity.x + missile.obj.body->Velocity.y + missile.obj.body->Velocity.z;

                if (totalSpeed < 200.f && totalSpeed > -200.f) {
                    missile.obj.body->Velocity.x += (rotationEul.x * 30);
                    missile.obj.body->Velocity.y += (rotationEul.y * 30);
                    missile.obj.body->Velocity.z += (rotationEul.z * 30);
                }

                RaycastFilter rcf = {};
                rcf.m_IgnoredBodies.push_back(missile.obj.body);
                rcf.m_IgnoredShapes.push_back(missile.obj.shape);

                td::Vec3 objectMin = missile.obj.shape->posMin;
                td::Vec3 objectMax = missile.obj.shape->posMax;
                td::Vec3 centerpoint = { objectMax.x - ((objectMax.x - objectMin.x) / 2) + rotationEulForward.x, objectMax.y - ((objectMax.y - objectMin.y) / 2) + rotationEulForward.y, objectMax.z - ((objectMax.z - objectMin.z) / 2) + rotationEulForward.z };

                raycaster::rayData rayDat = raycaster::castRayManual(centerpoint, rotationEul, &rcf);

                td::Vec3 vel = { 0.f, 0.f, 0.f };
                td::particleInfo tdpi = { 0.f, 0.f, 0.f, 0.7f, 0.7f, 0.7f, 1.f, 0.7f, 0.7f, 0.7f, 1.f, 0.f, 0.f, 0.f, 0.2f, 0.f, 0.f, 0.f, 0.f, 0.15f, 0.25f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 1.f, 1.f, 0.f, 0.f, 0.f, 1.f, 1.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f };
                glb::TDspawnParticle((DWORD*)special, &tdpi, centerpoint, vel, 0.5f);

                //glb::oFDL(glb::renderer, centerpoint, rayDat.worldPos, green, green, false);
                //drawCube(rayDat.worldPos, 0.2f, green);

                //isAwake seems to be set if the object is colliding with another object. 
                if ((missile.obj.body->isAwake && missile.age > 10) || (missile.age > 300)) {
                    glb::TDcreateExplosion((uintptr_t)glb::scene, &centerpoint, 2.f);

                    if (missile.obj.shape == mostRecentMissile.obj.shape) {
                        mostRecentMissile.obj.shape->Destroy(mostRecentMissile.obj.shape, true);
                        mostRecentMissile.obj.body->Destroy(mostRecentMissile.obj.body, true);
                        mostRecentMissile = {};
                    }

                    missile.obj.shape->Destroy(missile.obj.shape, true);
                    missile.obj.body->Destroy(missile.obj.body, true);
                    missile.destroyed = true;
                }
            }
        }
        if (!hasActiveMissile && launchedMissiles.size() > 0) {
            std::cout << "Clearing missiles" << std::endl;
            launchedMissiles.clear();
        }
    }
}