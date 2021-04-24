#define GLEW_STATIC
#include "Global.h"
#include "TDObjects.h"
#include "swapBuffers.h"
#include "Raycaster.h"
#include "Mods.h"
#include "noclip.h"
#include "objectSpawner.h"
#include "c4.h"
#include "maths.h"
#include "camera.h"
#include "toolgun.h"
#include "lantern.h"
#include "envOptions.h"
#include "miscPatches.h"

#pragma comment(lib, "glew32s.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
WNDPROC oWndProc;

std::once_flag swapBuffersInit;

LRESULT APIENTRY hWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_KEYDOWN:
        if (wParam == VK_F1 || wParam == VK_INSERT)
        {
            glb::displayMenu = !glb::displayMenu;
            return true;
        }

        if (wParam == mods::noclipKey && !glb::displayMenu) {
            noclip::ToggleNoclip();
        }

        if (wParam == 0x5A) { //Z
            spawner::deleteLastObject();
        }
    }

    ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);

    if (glb::displayMenu)
    {
        return true;
    }

    return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
}

bool hwCursor(int x, int y) {

    if (glb::displayMenu) {
        return false;
    }

    return glb::ocursor(x, y);
}

void onSwapBuffersInit()
{
    glewInit(); // initialize glew
    ImGui::CreateContext(); // create ImGui's context
    ImGui_ImplWin32_Init(glb::gWnd);
    const char* glsl_version = "#version 130";
    ImGui_ImplOpenGL3_Init(glsl_version);
}

void initHIDsHook() {
    HMODULE u32 = GetModuleHandle(L"user32.dll");
    glb::ocursor = (hk_SetCursorPos)GetProcAddress(u32, "SetCursorPos");

    oWndProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(glb::gWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(hWndProc)));

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourAttach(&(PVOID&)glb::ocursor, hwCursor);
    DetourTransactionCommit();
}

void terminateHIDsHook() {
    SetWindowLongPtrA(glb::gWnd, GWLP_WNDPROC, LONG_PTR(oWndProc));

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourDetach(&(PVOID&)glb::ocursor, hwCursor);
    DetourTransactionCommit();
}

std::vector<spawner::spawnerCatagory> spawnerObjects;
bool needToLoadObjects = true;

char position[64];
char velocity[64];
char cPosition[64];
char health[64];
static ImGuiTextFilter filter;
char tempSkyboxPath[128] = {};

bool hwglSwapBuffers(_In_ HDC hDc)
{
    std::call_once(swapBuffersInit, onSwapBuffersInit);

    //if (((((GetAsyncKeyState(VK_F1) >> 15) & 0x0001) == 0x0001))) {
    //    glb::displayMenu = true;
    //}
    //else {
    //    glb::displayMenu = false;
    //}

    if (needToLoadObjects) {
        needToLoadObjects = false;
        spawnerObjects = spawner::enumerateSpawnableObjects();
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    ImGuiIO* IO = &ImGui::GetIO();
    IO->MouseDrawCursor = glb::displayMenu;

    ImGuiWindowFlags infoBoxFlags = 0;
    infoBoxFlags |= ImGuiWindowFlags_NoMove;
    infoBoxFlags |= ImGuiWindowFlags_NoResize;
    infoBoxFlags |= ImGuiWindowFlags_NoCollapse;
    infoBoxFlags |= ImGuiWindowFlags_NoTitleBar;
    infoBoxFlags |= ImGuiWindowFlags_AlwaysAutoResize;
    ImGui::SetNextWindowPos(ImVec2(5, 5));
    ImGui::Begin("FennTD", (bool*)false, infoBoxFlags);

    char healthTx[256] = {};
    char positionTx[256] = {};
    sprintf_s(positionTx, 256, "X: %.2f, Y: %.2f, Z: %.2f", (glb::player->position.x), (glb::player->position.y), (glb::player->position.z));
    sprintf_s(healthTx, 256, "Health : %i", (int)(glb::player->health * 100));
    
    ImGui::Text(positionTx);
    ImGui::Text(healthTx);
    switch (glb::game->State)
    {
    case gameState::ingame:
        ImGui::Text("Game state: In game");
        break;
    case gameState::menu:
        ImGui::Text("Game state: In menu");
        break;
    case gameState::quitting:
        ImGui::Text("Game state: Quitting");
        break;
    case gameState::splash:
        ImGui::Text("Game state: Splash");
        break;
    case gameState::start_ui:
        ImGui::Text("Game state: StartUI");
        break;
    case gameState::editor:
        ImGui::Text("Game state: Editor");
        break;
    }

    ImGui::End();

    if (glb::displayMenu) {
        ImGui::Begin("KnedMod");
        {
            int counter = 0;

            ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar;
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.0f);
            ImGui::BeginChild("ChildL", ImVec2(ImGui::GetWindowWidth() * 0.75, 0), true, window_flags);

            if (ImGui::BeginMenuBar())
            {
                if (ImGui::BeginMenu("Spawn menu"))
                {
                    ImGui::MenuItem("Refresh", NULL, &needToLoadObjects);
                    ImGui::EndMenu();
                }
                ImGui::EndMenuBar();
            }

            ImGui::Text("Search: ");
            ImGui::SameLine();
            filter.Draw("##search", 210.f);

            for (spawner::spawnerCatagory catig : spawnerObjects) {
                ImGuiTreeNodeFlags flags = 0;
                if (filter.IsActive()) {
                    flags = 32;
                }

                if (ImGui::CollapsingHeader(catig.name.c_str(), flags)) {
                    while (counter % 5 != 0) { counter++; }

                    for (spawner::LoadedSpawnableObject lso : catig.objects) {
                        if (filter.PassFilter(lso.basePath.c_str())) {
                            if (counter % 5 != 0) {
                                ImGui::SameLine();
                            }

                            ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar;
                            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.0f);
                            ImGui::BeginChild(std::to_string(counter).c_str(), ImVec2(ImGui::GetWindowWidth() / 6, ImGui::GetWindowHeight() / 3), true, window_flags);

                            if (ImGui::BeginMenuBar())
                            {
                                std::string objectName = lso.basePath.substr(catig.name.length() + 5, lso.basePath.length() - catig.name.length() - 5).c_str();
                                if (ImGui::BeginMenu(objectName.c_str()))
                                {
                                    if (lso.attributes.size() > 0) {
                                        for (spawner::objectAttribute att : lso.attributes) {
                                            std::string attribString = (std::string(att.attribute.c_str()) + " (" + std::string(att.level.c_str()) + ")");
                                            ImGui::Text((attribString).c_str());
                                        }
                                    }
                                    else {
                                        ImGui::Text("none");
                                    }

                                    ImGui::EndMenu();
                                }
                                ImGui::EndMenuBar();
                            }

                            ImGui::Image((void*)(intptr_t)lso.imageTexture, ImVec2(ImGui::GetWindowWidth() - 17, ImGui::GetWindowWidth() - 17));

                            if (ImGui::Button("Spawn")) {
                                spawner::objectSpawnerParams osp;
                                osp.attributes = lso.attributes;
                                spawner::spawnObjectProxy(lso.voxPath, osp);
                            }

                            if (ImGui::Button("Spawn w/ spawngun")) {
                                toolgun::currentSpawngunObject = lso;
                            }

                            ImGui::EndChild();
                            ImGui::PopStyleVar();

                            counter++;
                        }
                    }
                }     
            }

            ImGui::EndChild();
            ImGui::PopStyleVar();
        }

        ImGui::SameLine();

        {
            ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar;
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.0f);
            ImGui::BeginChild("ChildR", ImVec2(0, 0), true, window_flags);

            if (ImGui::CollapsingHeader("toolgun")) {
                const char* items[] = { "Spawner", "Minigun", "Explosions", "Flamethrower", "Remover", "Set attribute", "Testing" };
                const char* bulletTypes[] = { "Bullet", "Shotgun", "Missile", "???" };
                static int item_current = 0;
                static int bullet_current = 0;
                ImGui::Combo("Current tool", &item_current, items, IM_ARRAYSIZE(items));
                toolgun::currentsetting = (toolgun::tgSettings)item_current;

                if (toolgun::currentsetting == toolgun::tgSettings::minigun) {
                    ImGui::Combo("Bullet type", &bullet_current, bulletTypes, IM_ARRAYSIZE(bulletTypes));
                    toolgun::minigunBulletType = bullet_current;
                    ImGui::SliderFloat("Minigun spread", &toolgun::spreadVal, 0, 0.5, "%.2f");
                    ImGui::SliderInt("Minigun bullet count", &toolgun::bulletsPerFrame, 0, 25);
                }
                if (toolgun::currentsetting == toolgun::tgSettings::explosion) {
                    ImGui::SliderFloat("Explosion spread", &toolgun::EXspreadVal, 0, 0.5, "%.2f");
                    ImGui::SliderInt("Explosion count", &toolgun::EXbulletsPerFrame, 0, 25);
                    ImGui::SliderFloat("Power", &toolgun::power, 0.25f, 25.f, "%.2f");
                }
                if (toolgun::currentsetting == toolgun::tgSettings::flamethrower) {
                    ImGui::SliderFloat("Radius", &toolgun::flRadius, 0.1f, 3.f, "%.2f");
                    ImGui::SliderInt("Chance %", &toolgun::chance, 1, 100);
                }
                if (toolgun::currentsetting == toolgun::tgSettings::testing) {
                    ImGui::SliderFloat("Max Dist", &toolgun::maxRange, 0.1f, 100.f, "%.2f");
                    ImGui::SliderFloat("Hole size", &toolgun::holeSize, 0.1f, 100.f, "%.2f");
                }
                if (toolgun::currentsetting == toolgun::tgSettings::setAtttibute) {
                    ImGui::InputText("Attribute 1", toolgun::setAttributeFirst, 128);
                    ImGui::InputText("Attribute 2", toolgun::setAttributeSecond, 128);
                }
            }

            if (ImGui::CollapsingHeader("misc")) {
                ImGui::Checkbox("Godmode", &mods::godmode);
                ImGui::Checkbox("Jetpack", &mods::jetpack);
                ImGui::Checkbox("Flamethrower", &mods::flamethrower);
                ImGui::Checkbox("Remove map boundary", &mods::removeWalls);
                if (ImGui::Checkbox("Remove plank length limit", &miscPatches::plankPatch)) {
                    miscPatches::updatePlankPatch();
                }
                if (ImGui::CollapsingHeader("objects")) {
                    if (ImGui::Button("Spawn lantern")) {
                        lantern::spawnLantern();
                    }

                    ImGui::SliderFloat("pointSize", &lantern::a1, 0.1f, 255.f, "%.2f");
                    ImGui::SliderFloat("shadows", &lantern::a2, 0.1f, 255.f, "%.2f");
                    ImGui::SliderFloat("brightness", &lantern::a3, 0.1f, 255.f, "%.2f");
                    ImGui::SliderFloat("fog", &lantern::a4, 0.1f, 255.f, "%.2f");
                    ImGui::Spacing();
                    ImGui::SliderFloat("R", &lantern::lightR, 0.1f, 255.f, "%.2f");
                    ImGui::SliderFloat("G", &lantern::lightG, 0.1f, 255.f, "%.2f");
                    ImGui::SliderFloat("B", &lantern::lightB, 0.1f, 255.f, "%.2f");
                }
            }

            if (ImGui::CollapsingHeader("c4")) {
                ImGui::Checkbox("Global C4 detonation", &mods::c4_global_detonation);
                ImGui::SliderInt("C4 size", &c4::selectedBombSizeInt, 1, 4);
            }

            if (ImGui::CollapsingHeader("environment")) {
            //    if (ImGui::Button("Set night")) {
            //        envOptions::setNight();
            //    }

                if (ImGui::Button("All lights off")) {
                    for (TDLight** tdl : *(glb::scene->pLightList)) {
                        ((TDLight*)tdl)->m_Enabled = false;
                    }
                }

                if (ImGui::Button("All lights on")) {
                    for (TDLight** tdl : *(glb::scene->pLightList)) {
                        ((TDLight*)tdl)->m_Enabled = true;
                    }
                }

            //    ImGui::InputText("Skybox", tempSkyboxPath, 128);
            //    ImGui::SliderFloat("Sun brightness", &glb::scene->pEnvironment->sunBrightness, 0.f, 255.f, "%.2f");
            //    ImGui::SliderFloat("Sun spread", &glb::scene->pEnvironment->sunSpread, 0.f, 255.f, "%.2f");
            //    ImGui::SliderFloat("Sun length", &glb::scene->pEnvironment->sunLength, 0.f, 255.f, "%.2f");
            //    ImGui::SliderFloat("Sun fog", &glb::scene->pEnvironment->sunFogScale, 0.f, 255.f, "%.2f");
            //    ImGui::SliderFloat("Sun glare", &glb::scene->pEnvironment->sunGlare, 0.f, 255.f, "%.2f");
            //    ImGui::Spacing();
            //    ImGui::SliderFloat("sun R", &glb::scene->pEnvironment->sunColor.x, 0.f, 255.f, "%.2f");
            //    ImGui::SliderFloat("sun G", &glb::scene->pEnvironment->sunColor.y, 0.f, 255.f, "%.2f");
            //    ImGui::SliderFloat("sun B", &glb::scene->pEnvironment->sunColor.z, 0.f, 255.f, "%.2f");
            //    ImGui::Spacing();
            //    ImGui::SliderFloat("sun TR", &glb::scene->pEnvironment->sunColorTint.x, 0.f, 255.f, "%.2f");
            //    ImGui::SliderFloat("sun TG", &glb::scene->pEnvironment->sunColorTint.y, 0.f, 255.f, "%.2f");
            //    ImGui::SliderFloat("sun TB", &glb::scene->pEnvironment->sunColorTint.z, 0.f, 255.f, "%.2f");
            //    ImGui::Spacing();
            //    ImGui::SliderFloat("Exposure max", &glb::scene->pEnvironment->exposure.x, 0.f, 255.f, "%.2f");
            //    ImGui::SliderFloat("Exposure min", &glb::scene->pEnvironment->exposure.y, 0.f, 255.f, "%.2f");
            //    ImGui::Spacing();
            //    ImGui::SliderFloat("Ambience", &glb::scene->pEnvironment->ambient, 0.f, 255.f, "%.2f");
            //    ImGui::SliderFloat("Brightness", &glb::scene->pEnvironment->brightness, 0.f, 255.f, "%.2f");
            //    ImGui::SliderFloat("rain", &glb::scene->pEnvironment->rain, 0.f, 1.f, "%.2f");
            //    ImGui::SliderFloat("wetness", &glb::scene->pEnvironment->wetness, 0.f, 1.f, "%.2f");
            //    ImGui::SliderFloat("puddle ammount", &glb::scene->pEnvironment->puddleamount, 0.f, 1.f, "%.2f");
            //    ImGui::SliderFloat("puddle size", &glb::scene->pEnvironment->puddlesize, 0.f, 1.f, "%.2f");
            //    ImGui::SliderFloat("slippery", &glb::scene->pEnvironment->slippery, 0.f, 1.f, "%.2f");
            //    ImGui::SliderFloat("sky rotation", &glb::scene->pEnvironment->skyboxrot, 0.f, 255.f, "%.2f");
            //    ImGui::Checkbox("Nightlight", &glb::scene->pEnvironment->nightlight);
            //    if (ImGui::Button("Update pEnvironment") && glb::game->State == gameState::ingame) {
            //        glb::oEnvUpdate((uintptr_t)glb::scene->pEnvironment);
            //    }

            }

            if (ImGui::CollapsingHeader("Smoker")) {
                if (ImGui::Button("Spawn smoker")) {
                    smoker::spawnSmoker();
                }

                ImGui::SliderFloat("Velo X", &smoker::velocity.x, 0.f, 5.f, "%.2f");
                ImGui::SliderFloat("Velo Y", &smoker::velocity.y, 0.f, 10.f, "%.2f");
                ImGui::SliderFloat("Velo Z", &smoker::velocity.z, 0.f, 10.f, "%.2f");
                ImGui::SliderFloat("Lifetime", &smoker::lifetime, 0.f, 10.f, "%.2f");

                ImGui::SliderFloat("U01", &smoker::pInfo.U01, 0.f, 5.f, "%.2f");
                ImGui::SliderFloat("U02", &smoker::pInfo.U02, 0.f, 5.f, "%.2f");
                ImGui::SliderFloat("U03", &smoker::pInfo.U03, 0.f, 5.f, "%.2f");
                ImGui::SliderFloat("U04", &smoker::pInfo.U04, 0.f, 5.f, "%.2f");
                ImGui::SliderFloat("U05", &smoker::pInfo.U05, 0.f, 5.f, "%.2f");
                ImGui::SliderFloat("U06", &smoker::pInfo.U06, 0.f, 5.f, "%.2f");
                ImGui::SliderFloat("U07", &smoker::pInfo.U07, 0.f, 5.f, "%.2f");
                ImGui::SliderFloat("U08", &smoker::pInfo.U08, 0.f, 5.f, "%.2f");
                ImGui::SliderFloat("U09", &smoker::pInfo.U09, 0.f, 5.f, "%.2f");
                ImGui::SliderFloat("U10", &smoker::pInfo.U10, 0.f, 5.f, "%.2f");
                ImGui::SliderFloat("U11", &smoker::pInfo.U11, 0.f, 5.f, "%.2f");
                ImGui::SliderFloat("U12", &smoker::pInfo.U12, 0.f, 5.f, "%.2f");
                ImGui::SliderFloat("U13", &smoker::pInfo.U13, 0.f, 5.f, "%.2f");
                ImGui::SliderFloat("U14", &smoker::pInfo.U14, 0.f, 5.f, "%.2f");
                ImGui::SliderFloat("U15", &smoker::pInfo.U15, 0.f, 5.f, "%.2f");
                ImGui::SliderFloat("U16", &smoker::pInfo.U16, 0.f, 5.f, "%.2f");
                ImGui::SliderFloat("U17", &smoker::pInfo.U17, 0.f, 5.f, "%.2f");
                ImGui::SliderFloat("U18", &smoker::pInfo.U18, 0.f, 5.f, "%.2f");
                ImGui::SliderFloat("U19", &smoker::pInfo.U19, 0.f, 5.f, "%.2f");
                ImGui::SliderFloat("U20", &smoker::pInfo.U20, 0.f, 5.f, "%.2f");
                ImGui::SliderFloat("U21", &smoker::pInfo.U21, 0.f, 5.f, "%.2f");
                ImGui::SliderFloat("U22", &smoker::pInfo.U22, 0.f, 5.f, "%.2f");
                ImGui::SliderFloat("U23", &smoker::pInfo.U23, 0.f, 5.f, "%.2f");
                ImGui::SliderFloat("U24", &smoker::pInfo.U24, 0.f, 5.f, "%.2f");
                ImGui::SliderFloat("U25", &smoker::pInfo.U25, 0.f, 5.f, "%.2f");
                ImGui::SliderFloat("U26", &smoker::pInfo.U26, 0.f, 5.f, "%.2f");
                ImGui::SliderFloat("U27", &smoker::pInfo.U27, 0.f, 5.f, "%.2f");
                ImGui::SliderFloat("U28", &smoker::pInfo.U28, 0.f, 5.f, "%.2f");
                ImGui::SliderFloat("U29", &smoker::pInfo.U29, 0.f, 5.f, "%.2f");
                ImGui::SliderFloat("U30", &smoker::pInfo.U30, 0.f, 5.f, "%.2f");
                ImGui::SliderFloat("U31", &smoker::pInfo.U31, 0.f, 5.f, "%.2f");
                ImGui::SliderFloat("U32", &smoker::pInfo.U32, 0.f, 5.f, "%.2f");
                ImGui::SliderFloat("U33", &smoker::pInfo.U33, 0.f, 5.f, "%.2f");
                ImGui::SliderFloat("U34", &smoker::pInfo.U34, 0.f, 5.f, "%.2f");
                ImGui::SliderFloat("U35", &smoker::pInfo.U35, 0.f, 5.f, "%.2f");
                ImGui::SliderFloat("U36", &smoker::pInfo.U36, 0.f, 5.f, "%.2f");
                ImGui::SliderFloat("U37", &smoker::pInfo.U37, 0.f, 5.f, "%.2f");
                ImGui::SliderFloat("U38", &smoker::pInfo.U38, 0.f, 5.f, "%.2f");
                ImGui::SliderFloat("U39", &smoker::pInfo.U39, 0.f, 5.f, "%.2f");
                ImGui::SliderFloat("U40", &smoker::pInfo.U40, 0.f, 5.f, "%.2f");
                ImGui::SliderFloat("U41", &smoker::pInfo.U41, 0.f, 5.f, "%.2f");
                ImGui::SliderFloat("U42", &smoker::pInfo.U42, 0.f, 5.f, "%.2f");
                ImGui::SliderFloat("U43", &smoker::pInfo.U43, 0.f, 5.f, "%.2f");
                ImGui::SliderFloat("U44", &smoker::pInfo.U44, 0.f, 5.f, "%.2f");
                ImGui::SliderFloat("U45", &smoker::pInfo.U45, 0.f, 5.f, "%.2f");
                ImGui::SliderFloat("U46", &smoker::pInfo.U46, 0.f, 5.f, "%.2f");
                ImGui::SliderFloat("U47", &smoker::pInfo.U47, 0.f, 5.f, "%.2f");
                ImGui::SliderFloat("U48", &smoker::pInfo.U48, 0.f, 5.f, "%.2f");
                ImGui::SliderFloat("U49", &smoker::pInfo.U49, 0.f, 5.f, "%.2f");
                ImGui::SliderFloat("U50", &smoker::pInfo.U50, 0.f, 5.f, "%.2f");
                ImGui::SliderFloat("U51", &smoker::pInfo.U51, 0.f, 5.f, "%.2f");
                ImGui::SliderFloat("U52", &smoker::pInfo.U52, 0.f, 5.f, "%.2f");
                ImGui::SliderFloat("U53", &smoker::pInfo.U53, 0.f, 5.f, "%.2f");
                ImGui::SliderFloat("U54", &smoker::pInfo.U54, 0.f, 5.f, "%.2f");
                ImGui::SliderFloat("U55", &smoker::pInfo.U55, 0.f, 5.f, "%.2f");
                ImGui::SliderFloat("U56", &smoker::pInfo.U56, 0.f, 5.f, "%.2f");
                ImGui::SliderFloat("U57", &smoker::pInfo.U57, 0.f, 5.f, "%.2f");
                ImGui::SliderFloat("U58", &smoker::pInfo.U58, 0.f, 5.f, "%.2f");
                ImGui::SliderFloat("U59", &smoker::pInfo.U59, 0.f, 5.f, "%.2f");
                ImGui::SliderFloat("U60", &smoker::pInfo.U60, 0.f, 5.f, "%.2f");
                ImGui::SliderFloat("U61", &smoker::pInfo.U61, 0.f, 5.f, "%.2f");
                ImGui::SliderFloat("U62", &smoker::pInfo.U62, 0.f, 5.f, "%.2f");
                ImGui::SliderFloat("U63", &smoker::pInfo.U63, 0.f, 5.f, "%.2f");
                ImGui::SliderFloat("U64", &smoker::pInfo.U64, 0.f, 5.f, "%.2f");
            }

            ImGui::EndChild();
            ImGui::PopStyleVar();
        }


        //ImGui::Text("UI test");
        //sprintf_s(health, 64, "HP: %.4f", glb::player->health);
        //ImGui::Text(health);
        //sprintf_s(position, 64, "X: %.4f Y: %.4f Z: %.4f", glb::player->position.x, glb::player->position.y, glb::player->position.z);
        //ImGui::Text(position);
        //ImGui::Text(glb::player->heldItemName);

        ImGui::End();
    }
 
    mods::execMods();

    //std::cout << "====================" << std::endl;
    //std::cout << "Real QRT: X: " << glb::player->cameraQuat.x << " Y: " << glb::player->cameraQuat.y << " Z: " << glb::player->cameraQuat.z << " W: " << glb::player->cameraQuat.w << std::endl;
    //td::Vec3 cameraEuler = glb::player->cameraEuler();
    //td::Vec4 cameraSimulatedQuat = euler2quat(cameraEuler.x, cameraEuler.y, cameraEuler.z);
    //std::cout << "Simu QRT: X: " << cameraSimulatedQuat.x << " Y: " << cameraSimulatedQuat.y << " Z: " << cameraSimulatedQuat.z << " W: " << cameraSimulatedQuat.w << std::endl;
    //std::cout << "Real EUL: X: " << cameraEuler.x << " Y: " << cameraEuler.y << " Z: " << cameraEuler.z << std::endl;
    //td::Vec3 cameraSimulatedEuler = quat2euler(cameraSimulatedQuat);
    //std::cout << "Simu EUL: X: " << cameraSimulatedEuler.x << " Y: " << cameraSimulatedEuler.y << " Z: " << cameraSimulatedEuler.z << std::endl;
    //std::cout << glb::player->heldItemName << std::endl;

    //td::Vec4 ray = castRayPlayer();
    //std::cout << "X: " << std::to_string(ray.x) << " Y: " << std::to_string(ray.y) << " Z: " << std::to_string(ray.z) << " D: " << std::to_string(ray.w) << std::endl;

    //td::Vec3 boomPos = { ray.x, ray.y, ray.z };
    //glb::TDcreateExplosionWrapped(0, &boomPos, 5);
    //glb::createExplosion((uintptr_t)glb::scene, &boomPos, 5);

    ImGui::EndFrame();
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    return glb::owglSwapBuffers(hDc);
}

void terminateSwapBuffersHook() {
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourDetach(&(PVOID&)glb::owglSwapBuffers, hwglSwapBuffers);
    DetourTransactionCommit();
}

void initSwapBuffersHook() {
    HMODULE OpenGL = GetModuleHandle(L"opengl32.dll");
    glb::owglSwapBuffers = (twglSwapBuffers)GetProcAddress(OpenGL, "wglSwapBuffers");

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourAttach(&(PVOID&)glb::owglSwapBuffers, hwglSwapBuffers);
    DetourTransactionCommit();
}

