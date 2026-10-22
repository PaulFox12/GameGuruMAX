// Ocean shore/wave foam tunables — Wicked's foam math is tuned for meters, GG is
// inch-scaled. The pure conversion (0.0254) makes the depth-based shore band so wide
// that shallow GG beaches drown in white; 0.08 was picked visually on TESTPRO1
// (bold foam line on the shore + collar around protruding rocks, no milky blanket).
// foam_amount brightens the result. Live-tunable via the SET_OCEAN harness command.
// Diagnostic: last Caustic Size the visuals path actually delivered (harness GET_PERF_DATA).
float g_dbgCausticSizeFromVisuals = -1.0f;
float g_fWaterFoamUnitScale = 0.08f;
float g_fWaterFoamAmount = 1.3f;

// UI AUDIT 2026-07-28: live visibility sweeps for the View Options grass/terrain checkboxes
// (implemented in GGTerrainWicked.cpp — same mechanisms as the G-key / O-key debug toggles).
namespace GGTerrain {
	void GGTerrainWicked_SetGrassVisible(bool visible);
	void GGTerrainWicked_SetTerrainVisible(bool visible);
}
// GGMAX engine delta 1.55: global env-probe brightness knob (wiScene.cpp).
namespace wi::scene {
	extern float gg_envprobe_brightness;
}

void gridedit_setvsync(bool bLevelVSyncEnabled)
{
	// ensure only test game and standalones obey VSYNC, editor should always run FULL SPEED
	extern bool bImGuiInTestGame;
	if (t.game.gameisexe == 0 && bImGuiInTestGame == false )
	{
		if(g.iEditorVSync == 0)
			bLevelVSyncEnabled = false;
	}
	master.bVsyncEnabled = bLevelVSyncEnabled;
	// SetVSync recreates the swap chain (a visible blip), and every Wicked_Update_Visuals push calls this, so only on a change
	if (master.swapChain.desc.vsync != master.bVsyncEnabled)
		wiEvent::SetVSync(master.bVsyncEnabled);
}

void gridedit_setreflection(bool bReflecctionFlag)
{
	master_renderer->setReflectionsEnabled(bReflecctionFlag);
}

void gridedit_setsky(int iSkyMode)
{
	extern wiECS::Entity g_weatherEntityID;
	wiScene::WeatherComponent* weather = wiScene::GetScene().weathers.GetComponent(g_weatherEntityID);
	if (iSkyMode == 0)
	{
		weather->SetRealisticSky(true);
		weather->SetVolumetricClouds(true);
	}
	if (iSkyMode == 1)
	{
		weather->SetRealisticSky(false);
		weather->SetVolumetricClouds(false);
	}
}


cStr sWindowName = "Environment Effects##VisualsToolsWindow";

void tab_tab_visuals(int iPage, int iMode)
{
	//iMode = 0 , editor.
	//iMode = 1 , test game.
	//iPage = 0 , game
	//iPage = 1 , performance data
	//iPage = 2 , visuals panel
	//iPage = 2? , behavior editor. (developer mode)

	//Try to match visuals.ini
	bNeedImGuiInput = false;

	if (iMode == 1 && !bRenderTabTab && !bImGuiFrameState) // lee added !bImGuiFrameState to prevent double newframes
	{
		//We need a new frame.
		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
		extern bool bSpriteWinVisible;
		bSpriteWinVisible = false;
		bBlockImGuiUntilNewFrame = false;
		bImGuiRenderWithNoCustomTextures = false;
		bRenderTabTab = true;
	}

	if (iMode == 1 && iPage >= 1)
	{
		//Display performance data.
		bool bStatOpen = true;
		// DrawData() is now suppressed in MasterRenderer::Compose via DisableDrawForThisFrame(),
		// so enabling the profiler for data collection is safe.
		if (!wiProfiler::IsEnabled())
			wiProfiler::SetEnabled(true);
		DisplayPerformanceData(&bStatOpen);
	}
	else
	{
		if (!bProfilerEnable)
		{
			if (wiProfiler::IsEnabled())
				wiProfiler::SetEnabled(false);
		}
	}

	if (iMode == 1 && iPage == 1)
	{
		//Make window a floating one for testgame.
		ImVec2 viewPortPos = ImGui::GetMainViewport()->Pos;
		ImVec2 viewPortSize = ImGui::GetMainViewport()->Size;
		float window_width = 20 * ImGui::GetFontSize();
		float window_height = 50 * ImGui::GetFontSize();
		if (iPage == 1)
		{
			ImGui::SetNextWindowPos(viewPortPos + ImVec2(viewPortSize.x - window_width - 2.0, 2.0), ImGuiCond_Once); //ImGuiCond_FirstUseEver
			ImGui::SetNextWindowSize(ImVec2(window_width, viewPortSize.y - 4.0), ImGuiCond_Once); //ImGuiCond_FirstUseEver}
		}
		sWindowName = "Visuals##VisualsTabTabWindow";
		bNeedImGuiInput = true;
	}

	bool bVisualUpdated = false;
	float fTabColumnWidth = 120;
	static bool bSetSimpleSky = false;
	static int g_lutimage_item_count = 0;
	static char** g_lutimage_items = NULL;
	static int current_lutimage_selection = 0;

	// for now, switch visual window for behavior editor!
	bool bBehaviorEditor = false;
	if (bRenderTabTab && iPage == 1)
	{
		// if object selected for in-game debug view/live editing
		if (g_bBehaviorEditorActive ==true) bBehaviorEditor = true;
	}

	// visual window or behavior window
	if (bBehaviorEditor == false && (Visuals_Tools_Window && iMode == 0 || bRenderTabTab) && iPage == 1)
	{
		extern int iGenralWindowsFlags;
		int winflag = iGenralWindowsFlags;
		if (iMode == 1 && iPage == 1)
		{
			winflag |= ImGuiWindowFlags_NoMove;
			winflag |= ImGuiWindowFlags_NoResize;
		}
		ImGui::Begin(sWindowName.Get(), &Visuals_Tools_Window, winflag);

		float w = ImGui::GetWindowContentRegionWidth();

		//PE: Default to closed.
		int wflags = ImGuiTreeNodeFlags_None;

		if (bRenderTabTab)
		{
			imgui_Customize_Sky_V2(1);
			imgui_Customize_Water_V2(1);
			imgui_Customize_Weather_V2(1);
			wflags = ImGuiTreeNodeFlags_None;
		}
		else
		{
			//Condition from test game tab tab. possible.
			if (iLastOpenHeader == 2 || iLastOpenHeader == 3)
			{
				ImGui::SetNextItemOpen(true, ImGuiCond_Always);
				iLastOpenHeader = 1;
			}
			imgui_Customize_Sky_V2(0);
			imgui_Customize_Weather_V2(1);
		}

		if (bRenderTabTab)
		{
			if (pref.bAutoClosePropertySections && iLastOpenHeader != 7)
				ImGui::SetNextItemOpen(false, ImGuiCond_Always);

			if (ImGui::StyleCollapsingHeader("Camera Settings", wflags))
			{
				ImGui::Indent(10);
				iLastOpenHeader = 7;
				bool bUpdateCam = false;
				if (ImGui::SliderFloat("##WickedCameraNear", &t.visuals.CameraNEAR_f, 1.0f, 30.0f, "%.2f", 1.0f))
				{
					if ( t.visuals.CameraFAR_f < t.visuals.CameraNEAR_f + 0.1f ) t.visuals.CameraFAR_f = t.visuals.CameraNEAR_f + 0.1f;
					t.gamevisuals.CameraNEAR_f = t.visuals.CameraNEAR_f;
					g.projectmodified = t.storeprojectmodified = 1;
					bUpdateCam = true;
				}
				if (ImGui::SliderFloat("##WickedCameraFar", &t.visuals.CameraFAR_f, DEFAULT_NEAR_PLANE, DEFAULT_FAR_PLANE, "%.2f", 2.0f))
				{
					if ( t.visuals.CameraNEAR_f > t.visuals.CameraFAR_f - 0.1f ) t.visuals.CameraNEAR_f = t.visuals.CameraFAR_f - 0.1f;
					t.gamevisuals.CameraFAR_f = t.visuals.CameraFAR_f;
					g.projectmodified = t.storeprojectmodified = 1;
					bUpdateCam = true;
				}

				//PE: Now reverse again as we now follow weapon in tab tab.
				//if (ImGui::MaxSliderInputFloat("##WickedCameraFov", &t.visuals.CameraFOV_f, 20.0f, 110.0f, "Camera Field Of View (FOV)"))
				//LB: Use real degree value so users know the FOV they are choosing
				ImGui::Text("Camera FOV");
				ImGui::SameLine();
				int iCamFOV = t.visuals.CameraFOV_f;
				if ( ImGui::SliderInt("##WickedCameraFov", &iCamFOV, 1, 120) )
				{
					t.visuals.CameraFOV_f = iCamFOV;
					t.gamevisuals.CameraFOV_f = t.visuals.CameraFOV_f;
					g.projectmodified = t.storeprojectmodified = 1;
					bUpdateCam = true;
				}
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("Vertical Field Of View (FOV)");

				// ZJ: Update camera settings separately to stop the water going crazy. See https://github.com/TheGameCreators/GameGuruRepo/issues/1851
				if (bUpdateCam)
				{
					float fUsedFOV = t.visuals.CameraFOV_f;
					if (bImGuiInTestGame == false) fUsedFOV = 45;
					float fCameraFov = XM_PI / (fUsedFOV / 15.0f); //Fit GG settings.
					if (bImGuiInTestGame == true)
					{
						//PE: Visual change also need reerse fov in test game and standalone.
						// when in game, weapon FOV correction
						fCameraFov = GGToRadian(fUsedFOV); // Oops - backwards logic, lower FOV needs lower angle passed in
					}
					wiScene::GetCamera().CreatePerspective((float)master.masterrenderer.GetLogicalWidth(), (float)master.masterrenderer.GetLogicalHeight(), t.visuals.CameraNEAR_f, t.visuals.CameraFAR_f, fCameraFov);
				}
				ImGui::Indent(-10);
			}

			extern int g_iDevToolsOpen;
			if (g_iDevToolsOpen != 0 )
			{
				if (ImGui::StyleCollapsingHeader("Physics", wflags))
				{
					ImGui::Indent(10);
					bool bDrawPhysicsShapes = t.visuals.iPhysicsDebugDraw;
					bool bRenderStaticShapes = false;
					bool bRenderStaticTerrain = false;
					if (t.visuals.iPhysicsDebugDrawStatics & 1) bRenderStaticShapes = true;
					if (t.visuals.iPhysicsDebugDrawStatics & 2) bRenderStaticTerrain = true;
					bool bRenderConstraints = t.visuals.iPhysicsDebugDrawConstraints;
					if (ImGui::Checkbox("Render Physics Shapes", &bDrawPhysicsShapes))
					{
						t.visuals.iPhysicsDebugDraw = bDrawPhysicsShapes;
						physics_set_debug_draw(bDrawPhysicsShapes);
						bVisualUpdated = true;
					}
					if (bDrawPhysicsShapes)
					{
						if (ImGui::Checkbox("Render Static Shapes", &bRenderStaticShapes))
						{
							t.visuals.iPhysicsDebugDrawStatics = 0;
							if (bRenderStaticShapes) t.visuals.iPhysicsDebugDrawStatics += 1;
							if (bRenderStaticTerrain) t.visuals.iPhysicsDebugDrawStatics += 1<<1;
							physics_set_debug_draw(bDrawPhysicsShapes);
						}
						if (ImGui::Checkbox("Render Static Terrain", &bRenderStaticTerrain))
						{
							t.visuals.iPhysicsDebugDrawStatics = 0;
							if (bRenderStaticShapes) t.visuals.iPhysicsDebugDrawStatics += 1;
							if (bRenderStaticTerrain) t.visuals.iPhysicsDebugDrawStatics += 1 << 1;
							physics_set_debug_draw(bDrawPhysicsShapes);
						}
						if (ImGui::Checkbox("Render Constraints", &bRenderConstraints))
						{
							t.visuals.iPhysicsDebugDrawConstraints = bRenderConstraints;
							physics_set_debug_draw(bDrawPhysicsShapes);
						}
					}
					if (bDrawPhysicsShapes)
					{
						extern bool g_bDebugRagdoll;
						if (ImGui::Checkbox("Render Ragdoll Shapes", &g_bDebugRagdoll))
						{
							// done elsewhere
						}
					}
					ImGui::Indent(-10);
				}
			}
		}

		//PE: Ambient Music Track only in Editor, in test game its already playing. so..
		if (iMode == 0)
		{
			if (pref.bAutoClosePropertySections && iLastOpenHeader != 11)
				ImGui::SetNextItemOpen(false, ImGuiCond_Always);

			if (ImGui::StyleCollapsingHeader("Music Tracks", wflags))
			{
				ImGui::Indent(10);
				iLastOpenHeader = 11;

				static bool bTestSound = false;
				float but_gadget_size = ImGui::GetFontSize()*10.0;

				// allow for ambient and combat selections
				for (int iGlobalGameSounds = 0; iGlobalGameSounds < 2; iGlobalGameSounds++)
				{
					// 0-ambient, 1-combat
					LPSTR pTextEnable = "";
					LPSTR pTextTitle = "";
					LPSTR pTextSelect = "";
					LPSTR pTextName = "";
					LPSTR pTextVolume = "";
					LPSTR pTextPlay = "";
					LPSTR pTextStop = "";
					int iFreeSoundID = 0;
					bool bEnableMusicTrack = 0;
					if (iGlobalGameSounds == 0)
					{
						pTextEnable = "Enable Ambient Music Track";
						pTextTitle = "Ambient Music Track";
						pTextSelect = "Select Ambient Music Track";
						pTextName = "##iAmbientMusicTrackVolume";
						pTextVolume = "Select Ambient Music Track Volume";
						pTextPlay = "Play Ambient Music##TabTabEditBehaviors";
						pTextStop = "Stop Ambient Music##TabTabEditBehaviors";
						iFreeSoundID = g.temppreviewsoundoffset + 3;
						bEnableMusicTrack = t.visuals.bEndableAmbientMusicTrack;
					}
					if (iGlobalGameSounds == 1)
					{
						pTextEnable = "Enable Combat Music Track";
						pTextTitle = "Combat Music Track";
						pTextSelect = "Select Combat Music Track";
						pTextName = "##iCombatMusicTrackVolume";
						pTextVolume = "Select Combat Music Track Volume";
						pTextPlay = "Play Combat Music##TabTabEditBehaviors";
						pTextStop = "Stop Combat Music##TabTabEditBehaviors";
						iFreeSoundID = g.temppreviewsoundoffset + 5;
						bEnableMusicTrack = t.visuals.bEnableCombatMusicTrack;
					}

					// gadgets for managing music track
					if (ImGui::Checkbox(pTextEnable, &bEnableMusicTrack))
					{
						if (iGlobalGameSounds == 0) t.gamevisuals.bEndableAmbientMusicTrack = t.visuals.bEndableAmbientMusicTrack = bEnableMusicTrack;
						if (iGlobalGameSounds == 1) t.gamevisuals.bEnableCombatMusicTrack = t.visuals.bEnableCombatMusicTrack = bEnableMusicTrack;
						if (!bEnableMusicTrack)
						{
							if (SoundExist(iFreeSoundID) == 1 && SoundPlaying(iFreeSoundID) == 1)
							{
								// stop currently playing preview
								StopSound(iFreeSoundID);
							}
							bTestSound = false;
						}
					}
					if (bEnableMusicTrack || iGlobalGameSounds == 1)
					{
						cstr cMusicTrack = "";
						if (iGlobalGameSounds == 0) cMusicTrack = t.visuals.sAmbientMusicTrack;
						if (iGlobalGameSounds == 1) cMusicTrack = t.visuals.sCombatMusicTrack;
						cstr cNewMusicTrack = imgui_setpropertyfile2_v2(0, cMusicTrack.Get(), pTextTitle, pTextSelect, "audiobank\\", false);
						if (cNewMusicTrack != cMusicTrack)
						{
							if (iGlobalGameSounds == 0)
							{
								t.visuals.sAmbientMusicTrack = cNewMusicTrack;
								t.gamevisuals.sAmbientMusicTrack = t.visuals.sAmbientMusicTrack;
								extern bool entity_copytoremoteifnotthere(LPSTR);
								entity_copytoremoteifnotthere(t.visuals.sAmbientMusicTrack.Get());
							}
							if (iGlobalGameSounds == 1)
							{
								t.visuals.sCombatMusicTrack = cNewMusicTrack;
								t.gamevisuals.sCombatMusicTrack = t.visuals.sCombatMusicTrack;
								extern bool entity_copytoremoteifnotthere(LPSTR);
								entity_copytoremoteifnotthere(t.visuals.sCombatMusicTrack.Get());
							}
							if (SoundExist(iFreeSoundID) == 1 && SoundPlaying(iFreeSoundID) == 1)
							{
								// stop currently playing preview
								StopSound(iFreeSoundID);
							}
							bTestSound = false;
						}
						int iMusicTrackVolume = 0;
						if (iGlobalGameSounds == 0) iMusicTrackVolume = t.visuals.iAmbientMusicTrackVolume;
						if (iGlobalGameSounds == 1) iMusicTrackVolume = t.visuals.iCombatMusicTrackVolume;
						if (ImGui::MaxSliderInputInt(pTextName, &iMusicTrackVolume, 0, 100, pTextVolume))
						{
							if (iGlobalGameSounds == 0) t.visuals.iAmbientMusicTrackVolume = iMusicTrackVolume;
							if (iGlobalGameSounds == 1) t.visuals.iCombatMusicTrackVolume = iMusicTrackVolume;
							if (iGlobalGameSounds == 0) t.gamevisuals.iAmbientMusicTrackVolume = t.visuals.iAmbientMusicTrackVolume;
							if (iGlobalGameSounds == 1) t.gamevisuals.iCombatMusicTrackVolume = t.visuals.iCombatMusicTrackVolume;
							if (SoundExist(iFreeSoundID) == 1 && SoundPlaying(iFreeSoundID) == 1)
							{
								SetSoundVolume(iFreeSoundID, iMusicTrackVolume);
							}
						}
						if (cMusicTrack.Len() > 0)
						{
							if (!bTestSound)
							{
								ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2((w*0.5) - (but_gadget_size*0.5), 0.0f));
								if (ImGui::StyleButton(pTextPlay, ImVec2(but_gadget_size, 0)))
								{
									// play music
									if (SoundExist(iFreeSoundID) == 1) DeleteSound(iFreeSoundID);
									if (FileExist(cMusicTrack.Get()) == 1)
									{
										LoadSound(cMusicTrack.Get(), iFreeSoundID, 0, 1);
										if (SoundExist(iFreeSoundID) == 1)
										{
											SetSoundVolume(iFreeSoundID, iMusicTrackVolume);
											LoopSound(iFreeSoundID);
											bTestSound = true;
										}
									}
								}
							}
							else
							{
								ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2((w*0.5) - (but_gadget_size*0.5), 0.0f));
								if (ImGui::StyleButton(pTextStop, ImVec2(but_gadget_size, 0)))
								{
									if (SoundExist(iFreeSoundID) == 1 && SoundPlaying(iFreeSoundID) == 1)
									{
										// stop currently playing preview
										StopSound(iFreeSoundID);
									}
									bTestSound = false;
								}
							}
						}
					}
				}
				ImGui::Indent(-10);
			}
		}

		//PE: Filter Effect.
		if (pref.bAutoClosePropertySections && iLastOpenHeader != 10)
			ImGui::SetNextItemOpen(false, ImGuiCond_Always);

		if (ImGui::StyleCollapsingHeader("Filter Effect", wflags))
		{
			ImGui::Indent(10);
			ImGui::BeginChild("##filtereffectchild", ImVec2(ImGui::GetContentRegionAvailWidth(), 500), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);
			iLastOpenHeader = 10;
			static int iFilterThumbnailCount = 0;

			//LUT color grading.
			if (lutImages_s.size() <= 0)
			{
				g_lutimage_item_count = 0;
				cstr oldDir_s = GetDir();
				SetDir(g.fpscrootdir_s.Get());
				SetDir("Files\\editors\\lut");
				ChecklistForFiles();
				std::vector<cstr> fileAnnotates_s;

				for (int f = 1; f <= ChecklistQuantity(); f++)
				{
					cstr tfile_s = ChecklistString(f);
					LPSTR pFilename = tfile_s.Get();
					if (tfile_s != "." && tfile_s != "..")
					{
						if (strnicmp(pFilename + strlen(pFilename) - 4, ".png", 4) == NULL)
						{
							// ZJ: Now get titles from text file, this serves as a backup in case something goes wrong with the file.
							// create a readable title from file
							char pTitleName[256];
							strcpy(pTitleName, pFilename);
							pTitleName[strlen(pTitleName) - 4] = 0;
							for (int n = 0; n < strlen(pTitleName); n++)
							{
								if (n == 0)
								{
									if (pTitleName[n] >= 'a' && pTitleName[n] <= 'z')
										pTitleName[n] -= ('a' - 'A');
								}
								else
								{
									if (pTitleName[n] >= 'A' && pTitleName[n] <= 'Z')
										pTitleName[n] += ('a' - 'A');
								}
								if (pTitleName[n] == '_') pTitleName[n] = ' ';
							}

							if (!g_lutimage_items)
							{
								g_lutimage_items = new char*[ChecklistQuantity() + 1];
								//PE: Always add a none.
								lutImages_s.push_back(cstr("None"));
								g_lutimage_items[g_lutimage_item_count] = new char[256];
								strcpy(g_lutimage_items[g_lutimage_item_count], "None");
								g_lutimage_item_count++;
							}
							// add script and title to list
							lutImages_s.push_back(cstr("editors\\lut\\") + tfile_s);
							g_lutimage_items[g_lutimage_item_count] = new char[256];
							strcpy(g_lutimage_items[g_lutimage_item_count], pTitleName);
							g_lutimage_item_count++;
						}

						// Extract lut names from text file.
						if (strcmp(pFilename, "filternames.txt") == NULL)
						{
							if (FileExist(pFilename) == 1)
							{
								OpenToRead(1, pFilename);
								while (FileEnd(1) == 0)
								{
									// Get line by line.
									cstr line_s = ReadString(1);
									if (strlen(line_s.Get()) > 0)
										fileAnnotates_s.push_back(line_s);
								}
								CloseFile(1);
							}
						}
					}
				}

				// Assign the names to the filters.
				if (fileAnnotates_s.size() > 0)
				{
					for (int i = 0; i < fileAnnotates_s.size(); i++)
					{
						if(i < g_lutimage_item_count)
							strcpy(g_lutimage_items[i], fileAnnotates_s[i].Get());
					}
				}

				// Load the thumbnails for the filters, so its easier to see the effect it will have on the scene.
				if (_chdir("thumbnails") == 0)
				{
					//SetDir("thumbnails"); //PE: Standalone do not have this.
					ChecklistForFiles();

					image_setlegacyimageloading(true);
					for (int f = 1; f <= ChecklistQuantity(); f++)
					{
						cstr tfile_s = ChecklistString(f);
						LPSTR pFilename = tfile_s.Get();
						if (tfile_s != "." && tfile_s != "..")
						{
							// Only load .jpg files for the thumbnails.
							if (strnicmp(pFilename + strlen(pFilename) - 4, ".jpg", 4) == NULL)
							{
								LoadImage(pFilename, FILTER_THUMBS + iFilterThumbnailCount);
								if (ImageExist(FILTER_THUMBS + iFilterThumbnailCount))
								{
									iFilterThumbnailCount++;
								}
								else
								{
									// Number of loaded thumbnails must match the number of filters, otherwise they will be mismatched.
									for (int i = FILTER_THUMBS; i < FILTER_THUMBS + iFilterThumbnailCount; i++)
									{
										// Load failed, free any loaded images and display text only.
										if (ImageExist(i))
											DeleteImage(i);
									}
									iFilterThumbnailCount = 0;
								}
							}
						}
					}
					image_setlegacyimageloading(false);
				}
				SetDir(oldDir_s.Get());
			}

			if (master_renderer)
			{
				ImGui::PushItemWidth(-10);
				//PE: Set default selection. if not the correct.
				if (t.visuals.ColorGradingLUT != lutImages_s[current_lutimage_selection])
				{
					for (int l = 0; l < g_lutimage_item_count; l++)
					{
						if (t.visuals.ColorGradingLUT == lutImages_s[l])
						{
							current_lutimage_selection = l;
							break;
						}
					}
				}

				// Thumbnail didn't load, display as text instead.
				if (iFilterThumbnailCount == 0)
				{
					if (ImGui::Combo("##WickedLUTImageCombo", &current_lutimage_selection, g_lutimage_items, g_lutimage_item_count, 20))
					{
						if (current_lutimage_selection == 0)
							t.visuals.bColorGrading = false;
						else
							t.visuals.bColorGrading = true;

						t.gamevisuals.bColorGrading = t.visuals.bColorGrading;

						master_renderer->setColorGradingEnabled(t.visuals.bColorGrading);

						t.visuals.ColorGradingLUT = lutImages_s[current_lutimage_selection].Get();
						t.gamevisuals.ColorGradingLUT = t.visuals.ColorGradingLUT;
						bVisualUpdated = true;
					}
					if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Select Filter Effect");
				}
				else
				{
					float fButtonSize = ImGui::GetContentRegionAvailWidth() / 4 - 4.0f;
					int id = 54321;

					ImGui::Columns(4, "##filters", false);

					for (int i = 0; i < iFilterThumbnailCount; i++)
					{
						ImGui::PushID(id++);

						// Move the buttons closer together.
						ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(-10.0f, 0.0f));

						if (current_lutimage_selection == i)
						{
							ImVec4 tool_selected_col = ImGui::GetStyle().Colors[ImGuiCol_PlotHistogram];
							ImVec2 padding = { 3.0, 3.0 };
							ImGuiWindow* window = ImGui::GetCurrentWindow();
							const ImRect image_bb(window->DC.CursorPos + padding, window->DC.CursorPos + padding + padding + ImVec2(fButtonSize, fButtonSize));
							window->DrawList->AddRect(image_bb.Min, image_bb.Max, ImGui::GetColorU32(tool_selected_col), 0.0f, 15, 2.f);
						}

						// Display and handle selection of filter.
						if (ImGui::ImgBtn(i == 0 ? CCP_NONE : i + FILTER_THUMBS, ImVec2(fButtonSize, fButtonSize), ImVec4(0.0f, 0.0f, 0.0f, 0.0f), ImVec4(1.0f, 1.0f, 1.0f, 1.0f),
							ImVec4(0.8f, 0.8f, 0.8f, 0.8f), ImVec4(0.8f, 0.8f, 0.8f, 0.8f)))
						{
							current_lutimage_selection = i;

							if (current_lutimage_selection == 0)
								t.visuals.bColorGrading = false;
							else
								t.visuals.bColorGrading = true;

							t.gamevisuals.bColorGrading = t.visuals.bColorGrading;

							master_renderer->setColorGradingEnabled(t.visuals.bColorGrading);

							t.visuals.ColorGradingLUT = lutImages_s[current_lutimage_selection].Get();
							t.gamevisuals.ColorGradingLUT = t.visuals.ColorGradingLUT;
							bVisualUpdated = true;
						}
					
						ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(10.0f, -10.0f));

						// Enlarged thumbnail.
						if (ImGui::IsItemHovered())
						{
							ImGui::BeginTooltip();
							ImGui::ImgBtn(i + FILTER_THUMBS, ImVec2(fButtonSize * 2, fButtonSize * 2), ImVec4(0.0f, 0.0f, 0.0f, 0.0f), ImVec4(1.0f, 1.0f, 1.0f, 1.0f),
								ImVec4(0.8f, 0.8f, 0.8f, 0.8f), ImVec4(0.8f, 0.8f, 0.8f, 0.8f));
							ImGui::Text(g_lutimage_items[i]);
							ImGui::EndTooltip();
						}

						ImGui::NextColumn();

						ImGui::PopID();
					}
			
					ImGui::Text("");
					ImGui::Columns(1);
				}		
				ImGui::PopItemWidth();
			}
			ImGui::EndChild();
			ImGui::Indent(-10);
		}

		if (pref.iEnableAdvancedPostProcessing && iMode != 0) //PE: Now only in tab tab (iMode != 0))
		{
			bool Graphics_Performance_Settings(float fTabColumnWidth, bool bVisualUpdated);
			bVisualUpdated = Graphics_Performance_Settings(fTabColumnWidth, bVisualUpdated);

			bool PostProcess_Settings(float fTabColumnWidth, bool bVisualUpdated);
			bVisualUpdated = PostProcess_Settings(fTabColumnWidth, bVisualUpdated);
		}

		if (pref.iEnableAdvancedShadows && iMode != 0) //PE: Now only in tab tab (iMode != 0)
		{
			bool Shadows_Settings(float fTabColumnWidth, bool bVisualUpdated);
			bVisualUpdated = Shadows_Settings(fTabColumnWidth, bVisualUpdated);
		}

		// Control all in-game debugging options 
		if (pref.iEnableDeveloperProperties && iMode != 0) //PE: Now only in tab tab (iMode != 0))
		{
			bool AI_Management_Settings(float fTabColumnWidth, bool bVisualUpdated);
			bVisualUpdated = AI_Management_Settings(fTabColumnWidth, bVisualUpdated);
		}

		if (!bRenderTabTab && !pref.bHideTutorials)
		{
			#ifndef REMOVED_EARLYACCESS
			if (ImGui::StyleCollapsingHeader("Tutorial (this feature is incomplete)", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::Indent(10);
				cstr cShowTutorial = "01 - Getting started";
				char* tutorial_combo_items[] = { "01 - Getting started", "02 - Creating terrain", "03 - Add character and set a path" };
				SmallTutorialVideo(cShowTutorial.Get(), tutorial_combo_items, ARRAYSIZE(tutorial_combo_items), SECTION_VISUALS);
				float but_gadget_size = ImGui::GetFontSize()*12.0;
				float w = ImGui::GetWindowContentRegionWidth() - 20.0;
				ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2((w*0.5) - (but_gadget_size*0.5), 0.0f));
				#ifdef INCLUDESTEPBYSTEP
				if (ImGui::StyleButton("View Step by Step Tutorial", ImVec2(but_gadget_size, 0)))
				{
					bHelp_Window = true;
					bHelpVideo_Window = true;
					bSetTutorialSectionLeft = false;
					strcpy(cForceTutorialName, cShowTutorial.Get());
				}
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("Start Step by Step Tutorial");
				#endif

				ImGui::Indent(-10);
			}
			#endif
		}

		if (ImGui::GetCurrentWindow()->ScrollbarSizes.x > 0)
		{
			//Hitting exactly at the botton could cause flicker, so add some additional lines when scrollbar on.
			ImGui::Text("");
			ImGui::Text("");
		}
		ImGui::End();
	}

	// behavior editor
	static int iCountdownToRightShift = 0;
	if (bBehaviorEditor)
	{
		//Make window a floating one for testgame.
		ImVec2 viewPortPos = ImGui::GetMainViewport()->Pos;
		ImVec2 viewPortSize = ImGui::GetMainViewport()->Size;
		float window_width = 20 * ImGui::GetFontSize();
		float window_height = 50 * ImGui::GetFontSize();
		if (iPage == 1)
		{
			ImGui::SetNextWindowPos(viewPortPos + ImVec2(viewPortSize.x - window_width - 2.0, 2.0), ImGuiCond_Once); //ImGuiCond_FirstUseEver
			ImGui::SetNextWindowSize(ImVec2(window_width, viewPortSize.y - 4.0), ImGuiCond_Once); //ImGuiCond_FirstUseEver}
		}
		sWindowName = "Behavior Editor##BehaviorEditorTabTabWindow";
		bNeedImGuiInput = true;

		// start behavior editor window
		extern int iGenralWindowsFlags;
		int winflag = iGenralWindowsFlags;
		winflag |= ImGuiWindowFlags_NoMove;
		winflag |= ImGuiWindowFlags_AlwaysVerticalScrollbar;
		winflag |= ImGuiWindowFlags_AlwaysHorizontalScrollbar;

		//PE: This can crash if you resize window outside main viewport so always use main viewport.
		ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowViewport(viewport->ID);

		ImGui::Begin(sWindowName.Get(), &Visuals_Tools_Window, winflag);
		int wflags = ImGuiTreeNodeFlags_DefaultOpen;

		// Window layout and dimensions
		float fMargin = 15.0f;
		float w = ImGui::GetWindowContentRegionWidth();
		instruction_centerline = fMargin;

		// detect if width changes (need to trigger right shift recalc)
		static float lastw = -1.0f;
		if (w != lastw)
		{
			// update all states with latest rightmost shift
			iCountdownToRightShift = 40;
			lastw = w;
		}
		if (iCountdownToRightShift > 0)
		{
			iCountdownToRightShift--;
			if (iCountdownToRightShift == 0)
			{
				for (int iStateIndex = 0; iStateIndex < instruction_state_list.size(); iStateIndex++)
					instruction_state_list[iStateIndex].bRecalcRightMost = true;
			}
		}

		// a little code update behavior list if objects active count changes
		static int iLastActiveObjectCount = 0;
		static int iActiveObjectCount = 0;
		iActiveObjectCount = 0;
		for (int e = 1; e <= g.entityelementlist; e++)
		{
			int entid = t.entityelement[e].bankindex;
			if (entid > 0 && t.entityelement[e].active != 0)
				iActiveObjectCount++;
		}
		if (iActiveObjectCount != iLastActiveObjectCount)
		{
			// allows recently inactivated objects to be marked in real time
			iLastActiveObjectCount = iActiveObjectCount;
			instruction_recreatebehaviorlist = true;
		}

		// choose the object parent script you wish to edit
		bool bIsBehaviorToEditValid = false;
		if (ImGui::StyleCollapsingHeader("Behavior Choice##BehaviorEditor", wflags))
		{
			static int iObjectEditingID;
			static int iObjectCount = 0;
			static int* pObjectRefE = NULL;
			static char** pObjectAIScripts = NULL;
			int iTriggerNewObjectEditing = -1;
			if (instruction_recreatebehaviorlist == true)
			{
				if (iObjectCount > 0)
				{
					for (int e = 1; e <= iObjectCount; e++)
					{
						if (pObjectAIScripts[e - 1])
						{
							delete pObjectAIScripts[e - 1];
							pObjectAIScripts[e - 1] = NULL;
						}
					}
					delete[] pObjectAIScripts;
					delete[] pObjectRefE;
				}
				iObjectCount = 0;
				for (int e = 1; e <= g.entityelementlist; e++)
				{
					int entid = t.entityelement[e].bankindex;
					if (entid > 0 && (t.entityprofile[entid].ismarker == 0 || t.entityprofile[entid].ismarker == 3)) // objects and zones
					{
						// also only list those that would actually perform LUA logic in the loop
						if (t.entityelement[e].plrdist < MAXFREEZEDISTANCE || t.entityelement[e].eleprof.phyalways != 0)
						{
							// skip entities that are inside shops or chests, ect
							if (t.entityelement[e].collected >= 3 && t.entityelement[e].active == 0)
								continue;

							LPSTR pObjectAI = t.entityelement[e].eleprof.aimain_s.Get();
							if (stricmp(pObjectAI, "no_behavior_selected.lua") != NULL)
							{
								iObjectCount++;
							}
						}
					}
				}
				pObjectAIScripts = new char*[iObjectCount];
				pObjectRefE = new int[iObjectCount];
				memset(pObjectAIScripts, 0, iObjectCount * sizeof(char*));
				memset(pObjectRefE, 0, iObjectCount * sizeof(int));
				int iObjectIndex = 0;
				for (int e = 1; e <= g.entityelementlist; e++)
				{
					int entid = t.entityelement[e].bankindex;
					if (entid > 0 && (t.entityprofile[entid].ismarker == 0 || t.entityprofile[entid].ismarker == 3)) // objects and zones
					{
						// skip entities that are inside shops or chests, ect
						if (t.entityelement[e].collected >= 3 && t.entityelement[e].active == 0)
							continue;

						// also only list those that would actually perform LUA logic in the loop
						if (t.entityelement[e].plrdist < MAXFREEZEDISTANCE || t.entityelement[e].eleprof.phyalways != 0)
						{
							LPSTR pThisStatus = "";
							if (t.entityelement[e].active == 0) pThisStatus = " (inactive)";
							LPSTR pObjectAI = t.entityelement[e].eleprof.aimain_s.Get();
							if (stricmp(pObjectAI, "no_behavior_selected.lua") != NULL)
							{
								pObjectAIScripts[iObjectIndex] = new char[256];
								sprintf(pObjectAIScripts[iObjectIndex], "%d : %s%s", e, pObjectAI, pThisStatus);
								pObjectRefE[iObjectIndex] = e;
								iObjectIndex++;
							}
						}
					}
				}
				if (iObjectEditingID > iObjectCount-1) iObjectEditingID = iObjectCount-1;
				iTriggerNewObjectEditing = iObjectEditingID;
				instruction_recreatebehaviorlist = false;
			}
			ImGui::Indent(fMargin);
			char pComboObjectsDisplay[1024];
			sprintf(pComboObjectsDisplay, "##BehaviorEditorScriptCombo");
			ImGui::PushItemWidth(w - 20);
			if (ImGui::Combo(pComboObjectsDisplay, &iObjectEditingID, pObjectAIScripts, iObjectCount))
			{
				iTriggerNewObjectEditing = iObjectEditingID;
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Choose the object to edit its behavior");
			if (iTriggerNewObjectEditing != -1)
			{
				// refresh conditions and actions table
				gridedit_instruction_parseandpopulateinstructions();

				// assign new script to edit
				int iEntityIndex = pObjectRefE[iTriggerNewObjectEditing];// 1 + iTriggerNewObjectEditing;
				LPSTR pObjectAI = t.entityelement[iEntityIndex].eleprof.aimain_s.Get();
				sprintf(instruction_objectscriptbeingedited, "scriptbank\\%s", pObjectAI);
				instruction_objectscriptbeingedited[strlen(instruction_objectscriptbeingedited) - 4] = 0;
				strcat(instruction_objectscriptbeingedited, ".byc");

				// fill in animation list
				gridedit_instruction_populateanimationlist(t.entityelement[iEntityIndex].obj);

				// erase last instruction data (so new can be auto loaded)
				gridedit_deletebehavior();

				// and change which entity element we observe for the debug view
				instruction_running_e = 0;
				if (pObjectAI)
				{
					if (strlen(pObjectAI) > 0)
					{
						instruction_running_e = pObjectRefE[iTriggerNewObjectEditing]; //1 + iTriggerNewObjectEditing;
					}
				}
				instruction_running_index = 0;

				// also update layout as script may have changed!
				iCountdownToRightShift = 10;
			}
			ImGui::PopItemWidth();

			float but_gadget_size = ImGui::GetFontSize()*10.0;
			ImVec2 be_button_pos = ImGui::GetCursorPos() + ImVec2((w * 0.5) - (but_gadget_size * 0.5), 0.0f);
			ImGui::SetCursorPos(be_button_pos);
			if (ImGui::StyleButton("Close Behavior Editor##TabTabCloseBehaviorEditor", ImVec2(but_gadget_size, 0)))
			{
				extern bool g_bBehaviorEditorActive;
				g_bBehaviorEditorActive = false;
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Close the Behavior Editor to return to standard in-game controls");

			ImGui::Indent(-fMargin);
		}
		if (instruction_running_e > 0) bIsBehaviorToEditValid = true;
		if (bIsBehaviorToEditValid == true)
		{
			// which state are we in right now?
			int iCurrentlyInsideState = -1;
			if (instruction_freezewheneditingbehavior == false)
			{
				if (instruction_running_e > 0)
				{
					if (instruction_running_index > 0)
					{
						for (int iStateIndex = 0; iStateIndex < instruction_state_list.size(); iStateIndex++)
						{
							sStateNode* pState = &instruction_state_list[iStateIndex];
							if (gridedit_instruction_inthisstate_rec(pState->instruction_root) == true)
							{
								iCurrentlyInsideState = iStateIndex;
								break;
							}
						}
					}
				}
			}

			// go through all states
			instruction_hoveringover = NULL;
			for (int iStateIndex = 0; iStateIndex < instruction_state_list.size(); iStateIndex++)
			{
				// Get state details
				sStateNode* pState = &instruction_state_list[iStateIndex];

				// Format display of state name
				char pStateName[1024];
				strcpy (pStateName, pState->pName);
				if (iStateIndex == iCurrentlyInsideState)
				{
					// in this state now
					strupr(pStateName);
				}

				// Component for this state
				char pStateNameDisplay[1024];
				sprintf(pStateNameDisplay, "%s State##BehaviorEditor", pStateName);
				if (ImGui::StyleCollapsingHeader(pStateNameDisplay, wflags))
				{
					// Flow Chart
					instruction_border = 4.0f;
					instruction_vertical_gap = 10.0f;
					instruction_block_width = ImGui::GetFontSize()*6.0f;
					instruction_block_height = ImGui::GetFontSize()*8.6f;

					// start state panel
					ImGui::Indent(fMargin);

					// calculate required widths from hierarchy
					gridedit_instruction_calculatewidth_rec(pState->instruction_root);

					// can scan state layout and work out rightmost X block (so can shift right) 
					ImVec2 vCursorPos = ImGui::GetCursorPos();
					ImVec2 vTopCenterPos = vCursorPos + ImVec2(instruction_centerline, instruction_vertical_gap);
					if (pState->bRecalcRightMost == true)
					{
						// starts the process of working out right most margin to shift within
						pState->fRightMostX = -99999.0f;
					}
					else
					{
						// a little one to get better position in right panel
						vTopCenterPos.x += 20;
					}

					// go through all intructions for this state
					ImGui::SetCursorPos(vTopCenterPos);
					instruction_centerline_absolutex = ImGui::GetCurrentWindow()->DC.CursorPos.x;
					instruction_furthestcursor = vCursorPos;
					gridedit_instruction_block_rec (pState, vTopCenterPos, pStateName, pState->instruction_root, fMargin, 0);
					ImGui::SetCursorPos(instruction_furthestcursor);

					// once calculated, can keep 'fRightMostX' stored to affect new position of block placement
					if (pState->bRecalcRightMost == true)
						pState->bRecalcRightMost = false;

					// Allow Interupt
					char pAllowInteruptDisplay[1024];
					sprintf(pAllowInteruptDisplay, "Allow Interuptions##BehaviorEditor%s", pStateName);
					ImGui::SetCursorPos(ImVec2(vCursorPos.x, ImGui::GetCursorPos().y));
					if (ImGui::Checkbox(pAllowInteruptDisplay, &pState->bAllowInterupt))
					{
						instruction_freezewheneditingbehavior = true;
					}
					if (ImGui::IsItemHovered()) ImGui::SetTooltip("When this is ticked, the state can be interupted when the object takes damage.");

					// Delete button for state
					float fDeleteButtonWidth = ImGui::GetFontSize()*5.0f;
					ImGui::SameLine();
					ImGui::SetCursorPos(ImVec2(w - fMargin - fDeleteButtonWidth, ImGui::GetCursorPos().y));
					char pDeleteButtonDisplay[1024];
					sprintf(pDeleteButtonDisplay, "Delete State##BehaviorEditor%s", pStateName);
					if (ImGui::Button(pDeleteButtonDisplay, ImVec2(fDeleteButtonWidth, 0)))
					{
						// delete this state
						instruction_deletestate = iStateIndex;
						instruction_freezewheneditingbehavior = true;
					}
					if (ImGui::IsItemHovered()) ImGui::SetTooltip("Select this to permanently delete this state from the behavior");

					// end state panel
					ImGui::Indent(-fMargin);
				}
			}

			// Behavior management buttons
			if (ImGui::StyleCollapsingHeader("Behavior Management##BehaviorEditor", wflags))
			{
				ImGui::Indent(fMargin);

				float but_gadget_size = ImGui::GetFontSize() * 10.0;
				ImVec2 be_button_pos = ImGui::GetCursorPos() + ImVec2((w * 0.5) - (but_gadget_size * 0.5), 0.0f);
				ImGui::SetCursorPos(be_button_pos);
				if (ImGui::Button("Add New State##BehaviorEditor", ImVec2(but_gadget_size, 0)))
				{
					// Add new state
					instruction_createstate = true;
					strcpy (instruction_newstatename, "");
					instruction_freezewheneditingbehavior = true;
				}
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("Add a new state to this behavior (will require exiting the level to see updated behavior)");
				be_button_pos = ImGui::GetCursorPos() + ImVec2((w * 0.5) - (but_gadget_size * 0.5), 0.0f);
				ImGui::SetCursorPos(be_button_pos);
				if (ImGui::Button("Update Behavior##BehaviorEditor", ImVec2(but_gadget_size, 0)))
				{
					// Save behavior byte code file
					if (strlen(instruction_objectscriptbeingedited) > 0)
					{
						// save BYC file
						gridedit_savebehavior(instruction_objectscriptbeingedited);

						// the magic - we can trigger this script to reload its BYC data file
						// and resume from its current instruction using the 'iUniqueSignatureCode'
						if (instruction_running_e > 0)
						{
							// quick trigger to 
							LuaSetFunction("UpdateEntityDebugger", 2, 0);
							LuaPushInt(instruction_running_e);
							LuaPushInt(1);
							LuaCall();

							// release any freeze caused by a change/edit o the logic
							instruction_freezewheneditingbehavior = false;
						}
					}
				}
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("Save any changes that have been made to this behavior");
				be_button_pos = ImGui::GetCursorPos() + ImVec2((w * 0.5) - (but_gadget_size * 0.5), 0.0f);
				ImGui::SetCursorPos(be_button_pos);
				if (ImGui::Button("Restart Behavior##BehaviorEditor", ImVec2(but_gadget_size, 0)))
				{
					if (instruction_running_e > 0)
					{
						LuaSetFunction("UpdateEntityDebugger", 2, 0);
						LuaPushInt(instruction_running_e);
						LuaPushInt(2);
						LuaCall();
					}
				}
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("Restarts the behavior to the first state");
				be_button_pos = ImGui::GetCursorPos() + ImVec2((w * 0.5) - (but_gadget_size * 0.5), 0.0f);
				ImGui::SetCursorPos(be_button_pos);
				if (ImGui::Button("Stop Behavior##BehaviorEditor", ImVec2(but_gadget_size, 0)))
				{
					// freeze so can edit
					instruction_freezewheneditingbehavior = false;
					if (instruction_running_e > 0)
					{
						LuaSetFunction("UpdateEntityDebugger", 2, 0);
						LuaPushInt(instruction_running_e);
						LuaPushInt(3);
						LuaCall();
					}
				}
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("Restarts the behavior to the first state");

				ImGui::Indent(-fMargin);
			}
		}

		// solves vertical scroll flicker
		if (ImGui::GetCurrentWindow()->ScrollbarSizes.x > 0)
		{
			ImGui::Text("");
			ImGui::Text("");
		}

		// end behavior editor window
		ImGui::End();
	}
	else
	{
		// ensures we update layout each time we enter B.E
		iCountdownToRightShift = 2;
	}

	// if added a new instruction, must regenerate instruction indices
	if (instruction_regenerateinstructionindices == true)
	{
		gridedit_generateuniqueinstructionindices();
		instruction_regenerateinstructionindices = false;
	}

	// create a new state with unique name
	if (instruction_createstate == true)
	{
		// Can create new behvaior state
		ImGui::SetNextWindowSize(ImVec2(26 * ImGui::GetFontSize(), 8 * ImGui::GetFontSize()), ImGuiCond_Once);
		ImGui::SetNextWindowPosCenter(ImGuiCond_Once);
		cstr sUniqueWinName = cstr("Enter A Name for your New Behavior State##BehaviorEditorNewState");
		ImGui::Begin(sUniqueWinName.Get(), &instruction_createstate, 0);
		ImGui::Indent(10);
		cstr sUniqueInputName = cstr("##Behavior State Name") + cstr(1);
		ImGui::PushItemWidth(-10);
		ImGui::Text("");
		ImGui::Text("Type a name for your new Behavior State and press ENTER:");
		if (ImGui::IsRootWindowOrAnyChildFocused() && !ImGui::IsAnyItemActive() && !ImGui::IsMouseClicked(0)) ImGui::SetKeyboardFocusHere(0);
		if (ImGui::InputText(sUniqueInputName.Get(), instruction_newstatename, 250, ImGuiInputTextFlags_EnterReturnsTrue))
		{
			// create the new behavior state
			sStateNode state;
			memset(&state, 0, sizeof(state));
			strcpy (state.pName, instruction_newstatename);

			// create new instruction root node for this new state
			state.instruction_root = new sLeafNode();
			memset(state.instruction_root, 0, sizeof(sLeafNode));
			state.instruction_root->iUniqueSignatureCode = rand() % 999999;
			state.instruction_root->iInstructionIndex = -1;
			state.instruction_root->iCondition = 11;
			state.instruction_root->iAction = 11;
			state.bAllowInterupt = true;

			// add to state list
			instruction_state_list.push_back(state);

			// finished here
			instruction_createstate = false;
			strcpy(instruction_newstatename, "");

			// new changes means new instruction indices!
			instruction_regenerateinstructionindices = true;
		}
		ImGui::PopItemWidth();
		ImGui::Indent(-10);
		bImGuiGotFocus = true;
		ImGui::End();
	}

	// if delete state, wait until IMGUI stuff done
	if (instruction_deletestate != -1)
	{
		// delete all the instructions in this state
		sLeafNode* pThis = instruction_state_list[instruction_deletestate].instruction_root;
		gridedit_deletebehaviornodes_rec(pThis);

		// delete the state itself
		instruction_state_list.erase(instruction_state_list.begin() + instruction_deletestate);
		instruction_deletestate = -1;

		// new changes means new instruction indices!
		instruction_regenerateinstructionindices = true;
	}

	// if delete an instruction, do it outside of recursion
	if (instruction_deletethis)
	{
		// record relationship ptrs and delete
		sLeafNode* pParentInstruction = instruction_deletethis->pParent;
		sLeafNode* pNextAlso = instruction_deletethis->pAlso;
		sLeafNode* pNextElse = instruction_deletethis->pElse;

		// a little protection, you cannot delete an instruction that has BOTH ALSE and ELSE!! Or the last instruction in a state.
		if ((pNextAlso && pNextElse) || (pParentInstruction==NULL && pNextAlso==NULL && pNextElse ==NULL) )
		{
			// reason is, where does the other instruction chain go - not showing
			// end now, no delete today!
			instruction_deletethis = NULL;
		}
		else
		{
			// next instruction after one we deleted
			sLeafNode* pNextInstruction = pNextAlso;
			if (pNextInstruction == NULL) pNextInstruction = pNextElse;

			// reconstruct parent ref and sibling refs
			if (pParentInstruction)
			{
				if (pParentInstruction->pAlso == instruction_deletethis)
				{
					// parent also ref
					pParentInstruction->pAlso = pNextInstruction;
					if (pNextInstruction) pNextInstruction->pParent = pParentInstruction;
				}
				if (pParentInstruction->pElse == instruction_deletethis)
				{
					// parent else ref
					pParentInstruction->pElse = pNextInstruction;
					if (pNextInstruction) pNextInstruction->pParent = pParentInstruction;
				}
			}
			else
			{
				// new parent
				instruction_deleteinthisstate->instruction_root = pNextInstruction;
				instruction_deleteinthisstate->instruction_root->pParent = NULL;
			}

			// finally delete it
			delete instruction_deletethis;
			instruction_deletethis = NULL;
			instruction_deleteinthisstate = NULL;
		}
	}

	// when in instruction pcik mode, wait for a click
	if (instruction_pickaninstruction != NULL)
	{
		ImGuiIO& io = ImGui::GetIO();
		if (io.MouseDown[0] != 0)
		{
			if (instruction_hoveringover && instruction_pickaninstruction)
			{
				// selected an instruction
				sLeafNode* pChosenInstruction = instruction_hoveringover;
				if (instruction_pickstateorinstruction == 1)
				{
					// the root of this state
					sLeafNode* pFindRoot = pChosenInstruction;
					int iStateIndex = pFindRoot->iState;
					while (pFindRoot && pFindRoot->pParent && pFindRoot->pParent->iState == iStateIndex) pFindRoot = pFindRoot->pParent;
					instruction_pickaninstruction->pGoToInstruction = pFindRoot;

					// and add name to action param so user can see state being switched to in UI
					int iDestinationStateIndex = pChosenInstruction->iState - 1;
					strcpy (instruction_pickaninstruction->pActionParam1, instruction_state_list[iDestinationStateIndex].pName);
				}
				if (instruction_pickstateorinstruction == 2)
				{
					// a specific instruction
					instruction_pickaninstruction->pGoToInstruction = pChosenInstruction;
					strcpy (instruction_pickaninstruction->pActionParam1, "");
				}
			}
			instruction_pickaninstruction = NULL;
			instruction_pickstateorinstruction = 0;
		}
	}

	// states have at leat one instruction
	if (instruction_state_list.size() == 0)
	{
		if (strlen(instruction_objectscriptbeingedited) > 0)
		{
			// test load template behavior, will change as we PICK objects to edit
			gridedit_loadbehavior(instruction_objectscriptbeingedited);
		}
	}

	if (bVisualUpdated)
	{
		// visuals have been updated, inform wicked engine and mark level has modified
		Wicked_Update_Visuals((void *)&t.visuals);
		g.projectmodified = 1;
	}
}

void Wicked_Update_Shadows(void *voidvisual)
{
	extern int spot_lights_count;
	extern int point_lights_count;

	visualstype* visuals = (visualstype *)voidvisual;
	if (visuals == NULL) visuals = &t.visuals;

	// shadow sizes and counts a game script set (SetShadowResolution, SetShadowLights, SetTransparentShadows) replace the
	// visuals' for this push
	extern void LuaRenderSettings_ShadowValues(int* pSun, int* pSpot, int* pPoint, int* pSpotMax, int* pPointMax);
	extern bool LuaRenderSettings_TransparentShadows(bool bVisuals);
	int iSunRes = visuals->iShadowSpotCascadeResolution, iSpotRes = visuals->iShadowSpotResolution, iPointRes = visuals->iShadowPointResolution;
	int iSpotMax = visuals->iShadowSpotMax, iPointMax = visuals->iShadowPointMax;
	LuaRenderSettings_ShadowValues(&iSunRes, &iSpotRes, &iPointRes, &iSpotMax, &iPointMax);
	bool bTransparentShadows = LuaRenderSettings_TransparentShadows(visuals->bTransparentShadows);

	// Tree shadow LOD distance + cascade range live in the level visuals (saved with
	// the FPM, carried into test game like Transparent Shadows). Visuals are the master
	// copy — every (re)apply (level load, panel edit, preset change) syncs the tree
	// module. Guard: a zeroed struct from a legacy path must not silence tree shadows.
	if (visuals->fTreeShadowLODDist < 1.0f)
	{
		visuals->fTreeShadowLODDist = 2000.0f;   // GGMAX 3.108c default
		visuals->iTreeShadowRange = 5;
	}
	GGTrees::ggtrees_global_params.lod_dist_shadow = visuals->fTreeShadowLODDist;
	GGTrees::ggtrees_global_params.tree_shadow_range = visuals->iTreeShadowRange;

	static int total_active_2d_shadows = -1;

	bool bTransparentChanged = false;
	static bool bOldTransparent = false;
	//wiRenderer::SetTransparentShadowsEnabled(bTransparentShadows); // removed from wi::renderer
	if (bOldTransparent != bTransparentShadows)
	{
		bOldTransparent = bTransparentShadows;
		bTransparentChanged = true;
	}

	// GGMAX lowvram B5: cap the cascade/spot atlas resolution in the 4GB preset — applied at the
	// point of USE, never written back into the level's visuals (the Low VRAM checkbox must
	// round-trip tick/untick, and saving the level must not persist the degraded value). The
	// change detector keys on the CAPPED value, so a preset flip re-applies on the next
	// Wicked_Update_Visuals call. Worth ~60-150 MB on the heavier demos at 2048->1024.
	int ggCascadeRes = iSunRes;
	{
		extern bool gg_lowvram; // GGTerrainWicked.cpp (effective machine||level flag)
		if (gg_lowvram && ggCascadeRes > 1024) ggCascadeRes = 1024;
	}
	if (visuals->iShadowSpotCascadeResolution > 2048) visuals->iShadowSpotCascadeResolution = 2048; // legacy hard ceiling (pre-existing writeback)
	if (ggCascadeRes > 2048) ggCascadeRes = 2048;

	if (old_iShadowSpotCascadeResolution != ggCascadeRes || bTransparentChanged )
	{
		char debug[256];
		sprintf(debug, "wiRenderer::SetShadowProps2D: 5");
		timestampactivity(0, debug);
		old_iShadowSpotCascadeResolution = ggCascadeRes;
		wiRenderer::SetShadowProps2D(ggCascadeRes); //cascade only now.
	}


	int shadows = spot_lights_count;
	if (shadows <= 2) shadows = 2;
	else if (shadows <= 4) shadows = 4;
	else if (shadows <= 8) shadows = 8;
	else if (shadows <= 12) shadows = 12;
	else if (shadows <= 16) shadows = 16;
	if (shadows > iSpotMax) shadows = iSpotMax;

	// GGMAX 2.07: the spot RESOLUTION knob is live again — its old consumer
	// (SetShadowPropsSpot2D) was removed in the port, leaving spots to silently ride the
	// sun cascade resolution. The spot COUNT is applied below via the split budget.
	if (iSpotRes > 2048) iSpotRes = 2048;
	if (old_iShadowSpotResolution != iSpotRes || shadows != total_active_2d_shadows)
	{
		char debug[256];
		sprintf(debug, "wiRenderer::SetShadowPropsSpot: res %d count %d", iSpotRes, shadows);
		timestampactivity(0, debug);
		old_iShadowSpotResolution = iSpotRes;
		total_active_2d_shadows = shadows;
	}
	wiRenderer::SetShadowPropsSpot(iSpotRes);

	//PE: MEM - 1546 : END SetShadowProps2D                                     S:529MB V: (4157,0)     
	static int total_active_cube_shadows = -1;
	int shadowscube = point_lights_count;
	if (shadowscube <= 2) shadowscube = 2;
	else if (shadowscube <= 4) shadowscube = 4;
	else if (shadowscube <= 8) shadowscube = 8;
	else if (shadowscube <= 12) shadowscube = 12;
	else if (shadowscube <= 16) shadowscube = 16;

	//LB: Increased cap in Wicked to SIXTEEN as hitting issues on even small interior levels, lets see what the fall out is
	if (shadowscube > iPointMax) shadowscube = iPointMax;
	
	if (old_iShadowPointResolution != iPointRes || shadowscube > total_active_cube_shadows || (bForceRefreshLightCount && shadowscube != total_active_cube_shadows ) || bTransparentChanged )
	{
		bForceRefreshLightCount = false;
		char debug[256];
		sprintf(debug, "wiRenderer::SetShadowPropsCube: %d", shadowscube);
		timestampactivity(0, debug);
		total_active_cube_shadows = shadowscube;
		if (iPointRes > 2048) iPointRes = 2048;
		old_iShadowPointResolution = iPointRes;
		if(iPointRes == 0 || iPointMax == 0)
			wiRenderer::SetShadowPropsCube(iPointRes);
		else
			wiRenderer::SetShadowPropsCube(iPointRes);
	}

	// GG Phase 1 (GGMAX 2.07: SPLIT PER TYPE): each light type's shadow count is capped by its
	// OWN knob — spots/rects by iShadowSpotMax, points by iShadowPointMax. Before the split one
	// pool sized from the POINT knob gated both, so the spot count dropdown did nothing and the
	// point count dropdown turned spot shadows on/off (user-proven on TESTPRO2, 2026-08-06).
	// Pushed EVERY call (outside the change-gates above) so a LOWERED cap takes effect immediately.
	int iSpotShadowBudget = (iSpotRes == 0 || iSpotMax == 0) ? 0 : shadows;
	int iPointShadowBudget = (iPointRes == 0 || iPointMax == 0) ? 0 : shadowscube;
	wiRenderer::SetLocalShadowBudget(iSpotShadowBudget, iPointShadowBudget);
	// GG Phase 2: cache static local shadows so a bank of static lights (e.g. 16 torches all casting)
	// is cheap - render each once, reuse its atlas texels until it (or the granted set) changes.
	// Enabled whenever ANY local shadows are on; the engine falls back to full re-render for moving lights.
	wiRenderer::SetLocalShadowCachingEnabled(iSpotShadowBudget > 0 || iPointShadowBudget > 0);

	if(bForceRefreshLightCount) bForceRefreshLightCount = false;

	//PE: MEM - 1556 : END SetShadowPropsCube                                   S : 360MB V : (4518, 0)
}

// Map GG's fog settings onto the new engine's exponential fog. The old API's
// fogEnd was REMOVED in favour of fogDensity — Wicked's exponential fog
// (fogHF.hlsli) matches the DX11 curve with density = 4 / (far - near).
// ★ GGMAX 3.114: it only did when near was 0 - the stock engine integrated from the camera once
// past 2 x near; fogHF.hlsli now integrates past fogStart only, as DX11 did, for every near.
//
// SKY FIX 2026-07-31 (engine delta 1.65): the fog COLOUR model is now DX11-parity
// inside fogHF.hlsli — realistic sky fogs toward the per-direction horizon sky
// sample; skybox/gradient skies treat fog as an opt-in recolor toward the user
// Horizon/Fog RGB scaled by Fog Opacity (FogA, plumbed below as gg_fog_opacity;
// 0 = no fog, exactly DX11's "fogColor = own pixel colour" no-op). Previously the
// engine fogged toward weather->horizon at FULL strength in skybox/None modes,
// which painted the None-mode Horizon/Fog colour over terrain in Sky Box mode.
static void Wicked_ApplyFogModel(wiScene::WeatherComponent* weather, visualstype* visuals)
{
	weather->fogStart = visuals->FogNearest_f;
	float fogNear = visuals->FogNearest_f;
	float fogFar  = visuals->FogDistance_f;
	float fogDensity = 0.0f;
	if (fogFar > fogNear + 1.0f)
	{
		fogDensity = 4.0f / (fogFar - fogNear);
	}
	weather->fogDensity = fogDensity;
	weather->SetOverrideFogColor(false);
	// DX11 FogA semantics (legacy saves carry 0-255 values; DX11 clamps >2 to 0)
	float fogA = visuals->FogA_f;
	if (fogA > 2.0f) fogA = 0.0f;
	if (fogA < 0.0f) fogA = 0.0f;
	if (fogA > 1.0f) fogA = 1.0f;
	weather->gg_fog_opacity = fogA;
}

void Wicked_Update_Fog(void* visual)
{
	visualstype* visuals = (visualstype *)visual;
	wiScene::WeatherComponent* weather = wiScene::GetScene().weathers.GetComponent(g_weatherEntityID);
	if (weather)
	{
		Wicked_ApplyFogModel(weather, visuals);
		weather->horizon.x = visuals->FogR_f / 255.0f;
		weather->horizon.y = visuals->FogG_f / 255.0f;
		weather->horizon.z = visuals->FogB_f / 255.0f;
	}
}

void Wicked_Update_LightColors(void* visual)
{
	visualstype* visuals = (visualstype *)visual;
	wiScene::WeatherComponent* weather = wiScene::GetScene().weathers.GetComponent(g_weatherEntityID);

	if (weather)
	{
		weather->ambient.x = visuals->AmbienceRed_f / 255.0;
		weather->ambient.y = visuals->AmbienceGreen_f / 255.0;
		weather->ambient.z = visuals->AmbienceBlue_f / 255.0;
		weather->zenith.x = visuals->ZenithRed_f / 255.0f;
		weather->zenith.y = visuals->ZenithGreen_f / 255.0f;
		weather->zenith.z = visuals->ZenithBlue_f / 255.0f;
	}
	WickedCall_SetSunColors(visuals->SunRed_f / 255.0, visuals->SunGreen_f / 255.0, visuals->SunBlue_f / 255.0, visuals->SunIntensity_f, 1.0f, t.visuals.fSunShadowBias);
}

void Wicked_Update_Cloud(void* visual)
{
	visualstype* visuals = (visualstype*)visual;
	wiScene::WeatherComponent* weather = wiScene::GetScene().weathers.GetComponent(g_weatherEntityID);
	if (weather)
	{
		//weather->cloudScale = visuals->SkyCloudHeight; // REMOVED
		if (visuals->bDisableSkybox)
		{
			weather->volumetricCloudParameters.layerFirst.coverageAmount = 0.0f;
			weather->volumetricCloudParameters.layerFirst.windSpeed = 0.0f;
			weather->volumetricCloudParameters.layerFirst.coverageMinimum = 0.0f;
			weather->SetRealisticSky(false);
			weather->SetVolumetricClouds(false);
			//weather->SetSimpleSky(false);
		}
		else if (visuals->skyindex == 0)
		{
			weather->volumetricCloudParameters.layerFirst.coverageAmount = visuals->SkyCloudiness;
			weather->volumetricCloudParameters.layerFirst.windSpeed = visuals->SkyCloudSpeed;
			// SKY FIX 2026-07-31: the cloud shader works in WORLD units (1 GG unit = 1 inch), not
			// meters — feeding UnitsToMeters(height) put the cloud deck at ~39 real metres, a ground
			// fog bank that painted over every distant mountain (the "sky eats the mountains" bug).
			// Feed raw units, and scale the engine-default marching/render distances (stock values
			// assume metres) into units too, or the march stops ~39x short of the horizon.
			weather->volumetricCloudParameters.cloudStartHeight = visuals->SkyCloudHeight;
			{ // DX11-era shader used saturate(CoverageMinimum - 1.0) — the UI/level value is 1-based
				float ggCovMin = visuals->SkyCloudCoverage - 1.0f;
				if (ggCovMin < 0.0f) ggCovMin = 0.0f; if (ggCovMin > 1.0f) ggCovMin = 1.0f;
				weather->volumetricCloudParameters.layerFirst.coverageMinimum = ggCovMin; }
			weather->volumetricCloudParameters.cloudThickness = visuals->SkyCloudThickness;
			weather->volumetricCloudParameters.layerFirst.coverageWindSpeed = visuals->SkyCloudSpeed;
			weather->volumetricCloudParameters.maxMarchingDistance = GGTerrain_MetersToUnits(30000.0f);
			// Step density saturates once the marched span reaches this value — keep the stock
			// 3:1 ratio to shell thickness so a vertical ray through the deck still gets ~32
			// samples (a raw meter conversion here left it ~2 samples = invisible clouds).
			{ float ggStepDist = 3.0f * visuals->SkyCloudThickness; weather->volumetricCloudParameters.inverseDistanceStepCount = (ggStepDist > 1000.0f) ? ggStepDist : 1000.0f; }
			weather->volumetricCloudParameters.renderDistance = GGTerrain_MetersToUnits(70000.0f);
			weather->volumetricCloudParameters.LODDistance = GGTerrain_MetersToUnits(30000.0f);
			// SKY FIX 2026-07-31 (cont): the remaining meter-tuned engine defaults, rescaled to
			// world units. horizonBlendAmount is the critical one: it is a 1/distance fade that
			// stock-tuned completes at ~80km(m) — in an inch world that is 2 real km, which faded
			// the entire (correctly raised) cloud deck to nothing. Noise/weather scales are
			// 1/distance too; skew/curl modifiers and shadow step are distances; wind speeds are
			// m/s. All scaled once here so the engine's stock look is reproduced at inch scale.
			{
				const float M2U = GGTerrain_MetersToUnits(1.0f); // ~39.37 units per meter
				// extinction is per-distance (1/m): unscaled it makes the deck ~39x too
				// optically thick = a near-black absorbing layer across the whole sky
				weather->volumetricCloudParameters.layerFirst.extinctionCoefficient = XMFLOAT3(0.71f * 0.1f / M2U, 0.86f * 0.1f / M2U, 1.0f * 0.1f / M2U);
				weather->volumetricCloudParameters.layerSecond.extinctionCoefficient = XMFLOAT3(0.71f * 0.01f / M2U, 0.86f * 0.01f / M2U, 1.0f * 0.01f / M2U);
				weather->volumetricCloudParameters.horizonBlendAmount = 0.0000125f / M2U;
				weather->volumetricCloudParameters.shadowStepLength = 3000.0f * M2U;
				weather->volumetricCloudParameters.layerFirst.skewAlongWindDirection = 700.0f * M2U;
				weather->volumetricCloudParameters.layerFirst.totalNoiseScale = 0.0006f / M2U;
				weather->volumetricCloudParameters.layerFirst.curlNoiseModifier = 500.0f * M2U;
				weather->volumetricCloudParameters.layerFirst.skewAlongCoverageWindDirection = 2500.0f * M2U;
				weather->volumetricCloudParameters.layerFirst.weatherScale = 0.00002f / M2U;
				weather->volumetricCloudParameters.layerSecond.skewAlongWindDirection = 400.0f * M2U;
				weather->volumetricCloudParameters.layerSecond.totalNoiseScale = 0.0006f / M2U;
				weather->volumetricCloudParameters.layerSecond.curlNoiseModifier = 250.0f * M2U;
				weather->volumetricCloudParameters.layerSecond.weatherScale = 0.000025f / M2U;
				// cloud drift: UI value is read as meters/sec, world wants units/sec
				weather->volumetricCloudParameters.layerFirst.windSpeed = visuals->SkyCloudSpeed * M2U;
				weather->volumetricCloudParameters.layerFirst.coverageWindSpeed = visuals->SkyCloudSpeed * M2U;
			}
			weather->SetRealisticSky(true);
			weather->SetVolumetricClouds(true);
		}
		else
		{
			weather->volumetricCloudParameters.layerFirst.coverageAmount = 0.0f; //PE: This has changed in the new repo, same shader is now used and cloudiness turn it off, so must now be zero.
			weather->volumetricCloudParameters.layerFirst.windSpeed = 0.0f; //To stop moving lightshaft.
			//PE: Also disable volumetricCloud.
			weather->volumetricCloudParameters.layerFirst.coverageMinimum = 0.0f;
			weather->SetRealisticSky(false);
			weather->SetVolumetricClouds(false);
		}

	}
}

// GGMAX: re-apply the editor visuals NOW. Used by the Low VRAM checkbox and SET_LOWVRAM so the
// preset's immediately-applicable members (SSR off, shadow cascade cap) land without waiting for
// the next natural visuals refresh. Grass members still need a level reload (chunk-spawn build).
void GGApplyVisualsNow()
{
	extern void Wicked_Update_Visuals(void* voidvisual);
	Wicked_Update_Visuals((void*)&t.visuals);
}

// change only the colour grading LUT. Wicked_Update_Visuals would also push every other visual value (fog, sun,
// exposure, clouds...) back to the pushed struct and set vsync. LUTs once used stay loaded (the resource manager only
// holds weak references), so switching back to one does not load it from disk again
void Wicked_Update_LUT(void* voidvisual)
{
	visualstype* visuals = (visualstype*)voidvisual;
	if (!master_renderer) return;
	master_renderer->setColorGradingEnabled(visuals->bColorGrading);
	wiScene::WeatherComponent* weather = wiScene::GetScene().weathers.GetComponent(g_weatherEntityID);
	if (!weather || !visuals->bColorGrading) return;

	// on the heap and never freed: the textures must not be released after the graphics device at exit
	static std::map<std::string, wiResource>* pLUTCache = new std::map<std::string, wiResource>();
	std::string name = visuals->ColorGradingLUT.Get();
	wiResource& lut = (*pLUTCache)[name];
	if (!lut.IsValid())
	{
		// Wicked reads the file directly, so a LUT that is only in the writable folder needs its real path
		char pRealPath[MAX_PATH];
		strcpy_s(pRealPath, MAX_PATH, name.c_str());
		GG_GetRealPath(pRealPath, 0);
		lut = wiResourceManager::Load(pRealPath, wi::resourcemanager::Flags::IMPORT_COLORGRADINGLUT);
	}
	weather->colorGradingMapName = name;
	weather->colorGradingMap = lut;
}

// the weather wind (rain and snow drift, Wicked's wind) a script set, as Wicked_Update_Visuals applies the editor's: no
// vertical part, the gust size left as the level has it
void Wicked_Update_Wind(void* visual)
{
	visualstype* visuals = (visualstype*)visual;
	wiScene::WeatherComponent* weather = wiScene::GetScene().weathers.GetComponent(g_weatherEntityID);
	if (weather)
	{
		weather->windDirection = XMFLOAT3(visuals->wind_direction_x, 0.0f, visuals->wind_direction_z);
		weather->windSpeed = visuals->wind_speed;
		weather->windRandomness = visuals->wind_randomness;
	}
}

// the wind set from Lua is kept in t.gamevisuals, which the end of a test game overwrites, so it never reaches the editor.
// A full visuals push applies t.visuals, so a script's wind is applied again after it, until the level ends (the DX11
// branch's 5651c03c does the same for clouds, tree wind, water colour, water fog and the LUT; only the wind is carried)
bool g_bLuaSetWind = false;

void LuaGameVisuals_Apply(void* pPushedVisuals)
{
	if (pPushedVisuals == &t.gamevisuals) return;
	if (g_bLuaSetWind) Wicked_Update_Wind(&t.gamevisuals);
}

// at each level start and when a test game ends
void LuaGameVisuals_Clear(void)
{
	g_bLuaSetWind = false;
}

void Wicked_Update_Visuals(void *voidvisual)
{
	visualstype* visuals = (visualstype *) voidvisual;

	// prevent this being called if visual not initialised
	if (visuals->CameraNEAR_f == 0 && visuals->CameraFAR_f == 0)
	{
		return;
	}

	// Stage B.9: sync custom grass palette slots (22..GGGRASS_MAX_PALETTE_SLOTS-1) into the
	// GGGrass custom-slot registry. Runs on every level load (via M-MapFile) and any subsequent
	// visuals update, so custom grass renders immediately when a level opens — no need to first
	// enter vegetation editing mode. GGGrass_SetCustomSlotFilename is idempotent (compares against
	// stored value and only flags dirty on actual change), so calling this on non-load updates is
	// a cheap no-op if nothing changed.
	for (int iL = GGGRASS_CUSTOM_SLOT_BASE; iL < GGGRASS_MAX_PALETTE_SLOTS; iL++)
	{
		const char* cur = visuals->sGrassTextures[iL].Get();
		GGGrass_SetCustomSlotFilename(iL, (cur && cur[0]) ? cur : nullptr);
	}

	wiScene::WeatherComponent* weather = wiScene::GetScene().weathers.GetComponent(g_weatherEntityID);
	if (weather)
	{
		weather->ambient.x = visuals->AmbienceRed_f / 255.0;
		weather->ambient.y = visuals->AmbienceGreen_f / 255.0;
		weather->ambient.z = visuals->AmbienceBlue_f / 255.0;
		Wicked_ApplyFogModel(weather, visuals);
		weather->horizon.x = visuals->FogR_f / 255.0f;
		weather->horizon.y = visuals->FogG_f / 255.0f;
		weather->horizon.z = visuals->FogB_f / 255.0f;
		weather->zenith.x = visuals->ZenithRed_f / 255.0f;
		weather->zenith.y = visuals->ZenithGreen_f / 255.0f;
		weather->zenith.z = visuals->ZenithBlue_f / 255.0f;
		//weather->cloudScale = visuals->SkyCloudHeight; // REMOVED

		if (visuals->bDisableSkybox)
		{
			weather->volumetricCloudParameters.layerFirst.coverageAmount = 0.0f;
			weather->volumetricCloudParameters.layerFirst.windSpeed = 0.0f;
			weather->volumetricCloudParameters.layerFirst.coverageMinimum = 0.0f;
			weather->SetRealisticSky(false);
			weather->SetVolumetricClouds(false);
		}
		else if (visuals->skyindex == 0)
		{
			weather->volumetricCloudParameters.layerFirst.coverageAmount = visuals->SkyCloudiness;
			weather->volumetricCloudParameters.layerFirst.windSpeed = visuals->SkyCloudSpeed;
			// SKY FIX 2026-07-31: cloud shader distances/heights are WORLD units (inches here),
			// not meters — see Wicked_Update_Cloud for the full note.
			weather->volumetricCloudParameters.cloudStartHeight = visuals->SkyCloudHeight;
			{ // DX11-era shader used saturate(CoverageMinimum - 1.0) — the UI/level value is 1-based
				float ggCovMin = visuals->SkyCloudCoverage - 1.0f;
				if (ggCovMin < 0.0f) ggCovMin = 0.0f; if (ggCovMin > 1.0f) ggCovMin = 1.0f;
				weather->volumetricCloudParameters.layerFirst.coverageMinimum = ggCovMin; }
			weather->volumetricCloudParameters.cloudThickness = visuals->SkyCloudThickness;
			weather->volumetricCloudParameters.layerFirst.coverageWindSpeed = visuals->SkyCloudSpeed;
			weather->volumetricCloudParameters.maxMarchingDistance = GGTerrain_MetersToUnits(30000.0f);
			// Stock 3:1 ratio to shell thickness — see Wicked_Update_Cloud note.
			{ float ggStepDist = 3.0f * visuals->SkyCloudThickness; weather->volumetricCloudParameters.inverseDistanceStepCount = (ggStepDist > 1000.0f) ? ggStepDist : 1000.0f; }
			weather->volumetricCloudParameters.renderDistance = GGTerrain_MetersToUnits(70000.0f);
			weather->volumetricCloudParameters.LODDistance = GGTerrain_MetersToUnits(30000.0f);
			// SKY FIX 2026-07-31 (cont): the remaining meter-tuned engine defaults, rescaled to
			// world units. horizonBlendAmount is the critical one: it is a 1/distance fade that
			// stock-tuned completes at ~80km(m) — in an inch world that is 2 real km, which faded
			// the entire (correctly raised) cloud deck to nothing. Noise/weather scales are
			// 1/distance too; skew/curl modifiers and shadow step are distances; wind speeds are
			// m/s. All scaled once here so the engine's stock look is reproduced at inch scale.
			{
				const float M2U = GGTerrain_MetersToUnits(1.0f); // ~39.37 units per meter
				// extinction is per-distance (1/m): unscaled it makes the deck ~39x too
				// optically thick = a near-black absorbing layer across the whole sky
				weather->volumetricCloudParameters.layerFirst.extinctionCoefficient = XMFLOAT3(0.71f * 0.1f / M2U, 0.86f * 0.1f / M2U, 1.0f * 0.1f / M2U);
				weather->volumetricCloudParameters.layerSecond.extinctionCoefficient = XMFLOAT3(0.71f * 0.01f / M2U, 0.86f * 0.01f / M2U, 1.0f * 0.01f / M2U);
				weather->volumetricCloudParameters.horizonBlendAmount = 0.0000125f / M2U;
				weather->volumetricCloudParameters.shadowStepLength = 3000.0f * M2U;
				weather->volumetricCloudParameters.layerFirst.skewAlongWindDirection = 700.0f * M2U;
				weather->volumetricCloudParameters.layerFirst.totalNoiseScale = 0.0006f / M2U;
				weather->volumetricCloudParameters.layerFirst.curlNoiseModifier = 500.0f * M2U;
				weather->volumetricCloudParameters.layerFirst.skewAlongCoverageWindDirection = 2500.0f * M2U;
				weather->volumetricCloudParameters.layerFirst.weatherScale = 0.00002f / M2U;
				weather->volumetricCloudParameters.layerSecond.skewAlongWindDirection = 400.0f * M2U;
				weather->volumetricCloudParameters.layerSecond.totalNoiseScale = 0.0006f / M2U;
				weather->volumetricCloudParameters.layerSecond.curlNoiseModifier = 250.0f * M2U;
				weather->volumetricCloudParameters.layerSecond.weatherScale = 0.000025f / M2U;
				// cloud drift: UI value is read as meters/sec, world wants units/sec
				weather->volumetricCloudParameters.layerFirst.windSpeed = visuals->SkyCloudSpeed * M2U;
				weather->volumetricCloudParameters.layerFirst.coverageWindSpeed = visuals->SkyCloudSpeed * M2U;
			}
			weather->SetRealisticSky(true);
			weather->SetVolumetricClouds(true);
		}
		else
		{
			weather->volumetricCloudParameters.layerFirst.coverageAmount = 0.0f; //PE: This has changed in the new repo, same shader is now used and cloudiness turn it off, so must now be zero.
			weather->volumetricCloudParameters.layerFirst.windSpeed = 0.0f; //To stop moving lightshaft.
			//PE: Also disable volumetricCloud.
			weather->volumetricCloudParameters.layerFirst.coverageMinimum = 0.0f;
			weather->SetRealisticSky(false);
			weather->SetVolumetricClouds(false);
		}

		//weather->pp_voxel_steps = visuals->voxel_steps; // REMOVED
		weather->windDirection = XMFLOAT3(visuals->wind_direction_x, 0.0f, visuals->wind_direction_z);
		weather->windSpeed = visuals->wind_speed;
		// GGMAX 3.62: the slider is "Wind Gust Size" and the engine wants a FREQUENCY - invert.
		weather->windWaveSize = GGWind_GustSizeToWaveSize(visuals->pp_size);
		//weather->pp_alpha = visuals->pp_alpha; // REMOVED
		weather->windRandomness = visuals->wind_randomness;
		// GGMAX 3.57: tree sway - see the long note at GGTrees_SetSwayFromVisuals
		// (wickedcalls_part3.cpp) for why this no longer writes a weather tree_wind field.
	extern void GGTrees_SetSwayFromVisuals( float treeWind, wi::scene::WeatherComponent* weather );
		GGTrees_SetSwayFromVisuals( visuals->tree_wind, weather );
		//weather->tree_sss = visuals->tree_sss; // REMOVED

		//weather->SetPPSnowEnabled(...); // removed in new WickedEngine API - no equivalent

		// If in Test Level or in standalone, use visual settings, otherwise just use the temporary editor setting.
		bool bWaterEnabled;
		if(t.game.set.ismapeditormode == 1)
			bWaterEnabled = t.showeditorwater;
		else
			bWaterEnabled = visuals->bWaterEnable;

		// GGMAX 2.94: "Water Off" brutal off-switch. Overriding HERE, after the if/else, is
		// deliberate - the editor branch reads t.showeditorwater and the game branch reads
		// visuals->bWaterEnable, so this is the only single point that covers both. Do NOT
		// implement this by writing visuals->bWaterEnable = false: that field is re-derived
		// from the terrain type on every level load (GGTerrainFile.cpp:37) and by
		// wicked_set_water_level (M-TerrainNew_part4.cpp:1336), so the switch would silently
		// un-stick, AND it would overwrite the level's authored water setting.
		//
		// Turning this off is worth more than the ocean itself: with IsOceanEnabled() false,
		// wiRenderer.cpp:4774 never sets vis.planar_reflection_visible, so the entire planar
		// reflection Z-prepass + colour pass stops being recorded. That pair measured 4.37 ms
		// of a 10.06 ms GPU frame on the TESTPRO1 island (PERFORMANCE.md:686-687).
		{
			extern bool gg_no_water;
			if (gg_no_water) bWaterEnabled = false;
		}

		// GGMAX 3.120: the underwater postprocess (underwaterCS) now also runs with the ocean OFF under Water Bake
		// (wi::gg_underwater_without_ocean), so the values it reads must be current in BOTH branches - with the ocean
		// off they used to go stale, or stay at the engine defaults (density 0 -> fall back to waterColor.a = near-
		// black fog). Same values, same formulas as the ocean branch below.
		weather->oceanParameters.waterColor = XMFLOAT4(visuals->WaterRed_f / 255.0f, visuals->WaterGreen_f / 255.0f, visuals->WaterBlue_f / 255.0f, visuals->WaterAlpha_f / 255.0f);
		weather->oceanParameters.underwater_color = XMFLOAT4(visuals->fUnderwaterColorR / 255.0f, visuals->fUnderwaterColorG / 255.0f, visuals->fUnderwaterColorB / 255.0f, 1.0f);
		weather->oceanParameters.underwater_fog_density = visuals->fUnderwaterFog * 0.00015f;

		if (bWaterEnabled)
		{
			weather->SetOceanEnabled(true);
			weather->oceanParameters.waterHeight = g.gdefaultwaterheight;
			XMFLOAT4 waterColor = XMFLOAT4(visuals->WaterRed_f / 255.0f, visuals->WaterGreen_f / 255.0f, visuals->WaterBlue_f / 255.0f, visuals->WaterAlpha_f / 255.0f);
			weather->oceanParameters.waterColor = waterColor;
			weather->oceanParameters.time_scale = visuals->WaterSpeed1;
			weather->oceanParameters.patch_length = visuals->fWaterPatchLength;
			weather->oceanParameters.choppy_scale = visuals->fWaterChoppyScale;
			weather->oceanParameters.wave_amplitude = visuals->fWaterWaveAmplitude;
			weather->oceanParameters.wind_dependency = visuals->fWaterWindDependency;

			// Wicked's shore/wave foam constants are tuned for METERS; GG world units are
			// inches, so without this conversion the shore foam band is ~40x too thin and
			// the foam noise repeats every inch (see oceanSurfacePS.hlsl FOAM block).
			// Tunables (also poked live by the SET_OCEAN harness command).
			extern float g_fWaterFoamUnitScale;
			extern float g_fWaterFoamAmount;
			weather->oceanParameters.foam_unit_scale = g_fWaterFoamUnitScale;
			weather->oceanParameters.foam_amount = g_fWaterFoamAmount;
			// "Caustic Size" slider -> decouples the underwater light-ripple scale from
			// patch_length (Water Tiling Patch Size), which otherwise drives BOTH the ripple
			// size and the wave size. scale = 3/size, so the default size 3.0 reproduces the
			// stock patch-tied look exactly; larger size = larger ripples, waves unaffected.
			// Measured response curve (patch length held constant): scale 1.0 = fine wavy streaks,
			// 0.5-0.3 = the large soft blobs, 0.1 = over-stretched and goes locally flat. So keep
			// the whole slider inside 1.0..0.2 rather than wasting travel past 0.2.
			float ggCSize = visuals->fWaterCausticSize;
			if (ggCSize < 1.0f || ggCSize > 10.0f) ggCSize = 3.0f;
			weather->oceanParameters.caustic_scale = 1.0f / ggCSize;
			extern float g_dbgCausticSizeFromVisuals;
			g_dbgCausticSizeFromVisuals = ggCSize;

			// Underwater view (Wicked underwaterCS), DECOUPLED from the surface waterColor above so
			// changing the Water Base Color no longer forces the surface opaque. Underwater Color =
			// what the submerged view fogs toward; Underwater Fog (0..100) = how fast it fogs with
			// depth. density scale is tuned for GG's inch-scale world (see underwaterCS.hlsl).
			weather->oceanParameters.underwater_color = XMFLOAT4(visuals->fUnderwaterColorR / 255.0f, visuals->fUnderwaterColorG / 255.0f, visuals->fUnderwaterColorB / 255.0f, 1.0f);
			// Slider 0..100 -> density 0..0.015. Scale tuned on the TESTPRO1 deep-water vista: the
			// default (fUnderwaterFog 20 -> 0.003) shows the seabed + caustics clearly with a natural
			// blue depth fade; 0 = crystal clear; ~100 = murky/opaque at depth. Shallow water stays
			// clearer at the same value (less water to see through), which is physically correct.
			weather->oceanParameters.underwater_fog_density = visuals->fUnderwaterFog * 0.00015f;
			// Make the "Water Base Color" tint the surface with depth even on fully-transparent
			// water (WaterAlpha 0), so changing the colour is visible from above without forcing the
			// water opaque. 0.005 tuned on the inch-scale island (delta 1.24): clear turquoise
			// shallows over the seabed, deepening to the base colour — and it scales with the colour's
			// own saturation, so a subtle blue tints gently while a bold colour reads strongly.
			weather->oceanParameters.water_color_depth = 0.005f;
			//weather->oceanParameters.fogMaxDist = visuals->WaterFogMaxDist; // removed from OceanParameters
			//weather->oceanParameters.fogMinDist = visuals->WaterFogMinDist; // removed from OceanParameters
			//weather->oceanParameters.fogMinAmount = visuals->WaterFogMinAmount; // removed from OceanParameters

			wiScene::GetScene().ocean = {};
		}
		else
		{
			weather->oceanParameters.waterHeight = g.gdefaultwaterheight; //PE: Pauls shader need this set.
			weather->SetOceanEnabled(false);
		}
	}

	WickedCall_SetSunColors(visuals->SunRed_f / 255.0, visuals->SunGreen_f / 255.0, visuals->SunBlue_f / 255.0, visuals->SunIntensity_f, 1.0f, t.visuals.fSunShadowBias);
	WickedCall_SetSunDirection(visuals->SunAngleX, visuals->SunAngleY, visuals->SunAngleZ);

	if (master_renderer) 
	{

		#ifdef POSTPROCESSRAIN
		if (bImGuiInTestGame == true)
		{
			master_renderer->setRainEnabled(visuals->bRainEnabled);
		}
		else
		{
			if(bEnableWeather)
				master_renderer->setRainEnabled(visuals->bRainEnabled);
			else
				master_renderer->setRainEnabled(false);
		}
		master_renderer->setRainOpacity(visuals->fRainOpacity);
		master_renderer->setRainScaleX(visuals->fRainScaleX);
		master_renderer->setRainScaleY(visuals->fRainScaleY);
		master_renderer->setRainRefreactionScale(visuals->fRainRefreactionScale);
		#endif

		wiResource image;
		// through the LUT cache, which also finds a LUT that is only in the writable folder
		Wicked_Update_LUT(visuals);

		master.bVsyncEnabled = visuals->bLevelVSyncEnabled;
		if (g.gvsync == 0)
		{
			// VSYNC override to switch OFF the VSYNC (each level can control on/off of the VSYNC in MAX)
			master.bVsyncEnabled = false;
		}
		gridedit_setvsync(master.bVsyncEnabled);

		master_renderer->setBloomEnabled(visuals->bBloomEnabled);
		master_renderer->setBloomThreshold(visuals->fsetBloomThreshold);
		master_renderer->setBloomStrength(visuals->fsetBloomStrength);
		// GGMAX lowvram: SSR off in the 4GB preset — its scratch set (binned_tiles + normals +
		// roughness + rtSSR + ssrResources) is ~20-25 MB at editor res, and Z Island is the only
		// hub demo authored with it on. The engine frees everything the frame SSR turns off.
		{
			extern bool gg_lowvram; // GGTerrainWicked.cpp
			master_renderer->setSSREnabled(gg_lowvram ? false : visuals->bSSREnabled);
		}
		master_renderer->setReflectionsEnabled(visuals->bReflectionsEnabled);
		master_renderer->setFXAAEnabled(visuals->bFXAAEnabled);
		wiRenderer::SetOcclusionCullingEnabled(visuals->bOcclusionCulling);
		

		master_renderer->setDepthOfFieldEnabled(visuals->bDOF);
		if (visuals->bDOF)
		{
			wiScene::Scene& scene = wiScene::GetScene();
			wiScene::CameraComponent& camera = wiScene::GetCamera();
			camera.aperture_size = visuals->fDOFApertureSize;
			camera.focal_length = visuals->fDOFFocalLength;
			master_renderer->setDepthOfFieldStrength(visuals->fDOFStrength);
		}


		if (visuals->ApparentSize < 0.000001f)
			visuals->ApparentSize = 0.000001f;
		if (visuals->ApparentSize > 0.2f)
			visuals->ApparentSize = 0.2f;

		extern float maxApparentSize;
		maxApparentSize = visuals->ApparentSize;

		extern bool bEnableTerrainChunkCulling;
		bEnableTerrainChunkCulling = visuals->bEnableTerrainChunkCulling;
		extern bool bEnablePointShadowCulling;
		bEnablePointShadowCulling = visuals->bEnablePointShadowCulling;
		extern bool bEnableSpotShadowCulling;
		bEnableSpotShadowCulling = visuals->bEnableSpotShadowCulling;
		extern bool bEnableObjectCulling;
		bEnableObjectCulling = visuals->bEnableObjectCulling;
		extern bool bEnableAnimationCulling;
		bEnableAnimationCulling = visuals->bEnableAnimationCulling;
		extern float fLODMultiplier;
		fLODMultiplier = visuals->fLODMultiplier;

		extern bool bEnable30FpsAnimations;
		bEnable30FpsAnimations = visuals->bEnable30FpsAnimations;
		extern bool g_bDelayedShadows;
		g_bDelayedShadows = visuals->g_bDelayedShadows;
		extern bool g_bDelayedShadowsLaptop;
		g_bDelayedShadowsLaptop = visuals->g_bDelayedShadowsLaptop;


		//PE: For now let it follow g_bDelayedShadows.
		extern bool bEnableDelayPointShadow;
		extern float pointShadowScaler;
		if (g_bDelayedShadows && g_bDelayedShadowsLaptop)
		{
			bEnableDelayPointShadow = true;
			pointShadowScaler = 0.6f;
		}
		else if (g_bDelayedShadows)
		{
			bEnableDelayPointShadow = true;
			pointShadowScaler = 1.0f;
		}
		else
		{
			bEnableDelayPointShadow = false;
			pointShadowScaler = 1.0f;
		}

		extern bool bShadowsLowestLOD;
		extern bool bProbesLowestLOD;
		extern bool bRaycastLowestLOD;
		extern bool bPhysicsLowestLOD;
		extern bool bReflectionsLowestLOD;

		bShadowsLowestLOD = visuals->bShadowsLowestLOD;
		bProbesLowestLOD = visuals->bProbesLowestLOD;
		bRaycastLowestLOD = visuals->bRaycastLowestLOD;
		bPhysicsLowestLOD = visuals->bPhysicsLowestLOD;
		bReflectionsLowestLOD = visuals->bReflectionsLowestLOD;

		extern bool bThreadedPhysics;
		bThreadedPhysics = visuals->bThreadedPhysics;
		
		// when in editor, keep enforcing a fixed exposure value (so we dont see fade-ins all the time)
		if (t.game.set.ismapeditormode==0 || pref.iEnableAutoExposureInEditor )
			master_renderer->setEyeAdaptionEnabled(visuals->bAutoExposure);
		else
			master_renderer->setEyeAdaptionEnabled(false);

		master_renderer->setEyeAdaptionRate(visuals->fAutoExposureRate);
		master_renderer->setEyeAdaptionKey(visuals->fAutoExposureKey);
		master_renderer->setExposure(visuals->fExposure);

		//PE: Still need a way to disable light shafts :)
		master_renderer->setLightShaftsEnabled(visuals->bLightShafts);
		// PORT TRANSLATE 2026-07-29: DX11 shipped light shafts at hardcoded exposure 0.2 with a
		// 32-tap blur; new Wicked defaults lightShaftsStrength to 0.5 and marches 64 taps, so the
		// stock look is ~3x hotter. 0.18 matches the DX11 end-to-end gain (0.2*4.80/5.42).
		master_renderer->setLightShaftsStrength(0.18f);

		master_renderer->setLensFlareEnabled(visuals->bLensFlare);
		
		if (old_iMSAASampleCount != visuals->iMSAASampleCount) {
			//PE: Will also resize buffers , so only when needed.
			old_iMSAASampleCount = visuals->iMSAASampleCount;
			master_renderer->setMSAASampleCount(visuals->iMSAASampleCount);
		}
		
		//PE: FSR can't work with VR as it use the openXR resolution, for VR another way is needed.
		extern bool	m_bUsingVR;
		if (!m_bUsingVR)
		{
			if (old_iFSRMode != visuals->iFSRMode) {
				old_iFSRMode = visuals->iFSRMode;
				if (visuals->iFSRMode == 1)
				{
					// TODO: Set3DResolution removed, use resolutionScale instead
					//master.masterrenderer.Set3DResolution(master.masterrenderer.GetPhysicalWidth(), master.masterrenderer.GetPhysicalHeight(), false);
					// GGREDUCED DX12: FSR1 renders 3D at 1/factor internal res; engine EASU+RCAS upscales.
					// Kept in lock-step with the live panel (M-GridEditB_part24.cpp) so saved levels restore identically.
					master.masterrenderer.resolutionScale = 1.0f / 1.3f; // Ultra Quality
					master.masterrenderer.setFSREnabled(true);
					master.masterrenderer.ResizeBuffers(); //PE: Force resizebuffers.
					master.masterrenderer.setFSRSharpness(t.visuals.fFSRSharpness);
					t.gamevisuals.bFXAAEnabled = t.visuals.bFXAAEnabled = true; //PE: need FXAA or FSR dont work.
					if (master_renderer)
						master_renderer->setFXAAEnabled(t.visuals.bFXAAEnabled);
				}
				else if (visuals->iFSRMode == 2)
				{
					// TODO: Set3DResolution removed, use resolutionScale instead
					//master.masterrenderer.Set3DResolution(master.masterrenderer.GetPhysicalWidth(), master.masterrenderer.GetPhysicalHeight(), false);
					// GGREDUCED DX12: FSR1 internal res = 1/1.5 native (lock-step with the panel).
					master.masterrenderer.resolutionScale = 1.0f / 1.5f; // Quality
					master.masterrenderer.setFSREnabled(true);
					master.masterrenderer.ResizeBuffers(); //PE: Force resizebuffers.
					master.masterrenderer.setFSRSharpness(t.visuals.fFSRSharpness);
					t.gamevisuals.bFXAAEnabled = t.visuals.bFXAAEnabled = true; //PE: need FXAA or FSR dont work.
					if (master_renderer)
						master_renderer->setFXAAEnabled(t.visuals.bFXAAEnabled);
				}
				else if (visuals->iFSRMode == 3)
				{
					// TODO: Set3DResolution removed, use resolutionScale instead
					//master.masterrenderer.Set3DResolution(master.masterrenderer.GetPhysicalWidth(), master.masterrenderer.GetPhysicalHeight(), false);
					// GGREDUCED DX12: FSR1 internal res = 1/1.7 native (lock-step with the panel).
					master.masterrenderer.resolutionScale = 1.0f / 1.7f; // Balanced
					master.masterrenderer.setFSREnabled(true);
					master.masterrenderer.ResizeBuffers(); //PE: Force resizebuffers.
					master.masterrenderer.setFSRSharpness(t.visuals.fFSRSharpness);
					t.gamevisuals.bFXAAEnabled = t.visuals.bFXAAEnabled = true; //PE: need FXAA or FSR dont work.
					if (master_renderer)
						master_renderer->setFXAAEnabled(t.visuals.bFXAAEnabled);
				}
				else if (visuals->iFSRMode == 4)
				{
					// TODO: Set3DResolution removed, use resolutionScale instead
					//master.masterrenderer.Set3DResolution(master.masterrenderer.GetPhysicalWidth(), master.masterrenderer.GetPhysicalHeight(), false);
					// GGREDUCED DX12: FSR1 internal res = 1/2.0 native (lock-step with the panel).
					master.masterrenderer.resolutionScale = 1.0f / 2.0f; // Performance
					master.masterrenderer.setFSREnabled(true);
					master.masterrenderer.ResizeBuffers(); //PE: Force resizebuffers.
					master.masterrenderer.setFSRSharpness(t.visuals.fFSRSharpness);
					t.gamevisuals.bFXAAEnabled = t.visuals.bFXAAEnabled = true; //PE: need FXAA or FSR dont work.
					if (master_renderer)
						master_renderer->setFXAAEnabled(t.visuals.bFXAAEnabled);
				}
				else
				{
					//PE: Disable FSR
					// TODO: Set3DResolution removed, use resolutionScale instead
					//master.masterrenderer.Set3DResolution(master.masterrenderer.GetPhysicalWidth(), master.masterrenderer.GetPhysicalHeight(), false);
					// GGREDUCED DX12: restore native internal resolution and tear down FSR (lock-step with the panel).
					master.masterrenderer.resolutionScale = 1.0f;
					master.masterrenderer.setFSREnabled(false);
					master.masterrenderer.ResizeBuffers(); //PE: Force resizebuffers.
				}

			}
		}

		if (old_iMSAO != visuals->iMSAO || old_fMSAOPower != visuals->fMSAOPower)
		{
			master.fAOPower = old_fMSAOPower = visuals->fMSAOPower;
			master.iAOSetting = old_iMSAO = visuals->iMSAO;
			if (master.iAOSetting > 0) master.masterrenderer.setAO(RenderPath3D::AO_MSAO);
			else master.masterrenderer.setAO(RenderPath3D::AO_DISABLED);
			if (master.iAOSetting > 0) master.masterrenderer.setAOPower(master.fAOPower);
		}

		Wicked_Update_Shadows(visuals);
	}

	extern float fWickedMaxCenterTest;
	if (visuals->CameraFAR_f < 70000.0 && visuals->CameraFAR_f < visuals->fShadowFarPlane)
		fWickedMaxCenterTest = 1500000.0;
	else if (visuals->CameraFAR_f < 250000.0 && visuals->CameraFAR_f < visuals->fShadowFarPlane)
		fWickedMaxCenterTest = 1000000.0;
	else if (visuals->CameraFAR_f < 300000.0 && visuals->CameraFAR_f < visuals->fShadowFarPlane)
		fWickedMaxCenterTest = 500000.0;
	else
		fWickedMaxCenterTest = 0.0; //PE: 0.0=Auto adjust min max z for orthographic shadow matrix, auto dont really work when CameraFAR_f < fShadowFarPlane ?

	WickedCall_SetShadowRange(visuals->fShadowFarPlane);

	// when in editor mode, separate control of gamma
	extern float g_fGlobalGammaFadeIn;
	extern float g_fGlobalGammaFadeInDest;
	if (bImGuiInTestGame == false && t.game.gameisexe == 0)
	{
		//leave editor to call setgamma
	}
	else
	{
		g_fGlobalGammaFadeIn = visuals->fGamma;
	}
	g_fGlobalGammaFadeInDest = visuals->fGamma;

	// UI AUDIT 2026-07-28: apply-on-load for the restored post sliders.
	// - Gamma: brightness-offset APPROXIMATION, neutral at the 2.2 default (the true pow-curve
	//   engine delta 1.54 is PARKED — pushed exponent read wrong in the live path, PIX needed).
	// - De Saturate: tonemap saturation; DX11 semantics are 1 = full color (default), 0 = gray.
	// - Global Probe Brightness: sample-time multiplier via engine delta 1.55 (1 = stock).
	if (master_renderer)
	{
		master_renderer->setBrightness((visuals->fGamma - 2.2f) * 0.15f);
		master_renderer->setSaturation(visuals->fDeSaturate);
	}
	wi::scene::gg_envprobe_brightness = visuals->fEnvProbeBrightness;

	float fUsedFOV = visuals->CameraFOV_f;
	if (bImGuiInTestGame == false) fUsedFOV = 45;
	float fCameraFov = XM_PI / (fUsedFOV / 15.0f); //Fit GG settings.
	if (bImGuiInTestGame == true)
	{
		//PE: Visual change also need reerse fov in test game and standalone.
		// when in game, weapon FOV correction
		fCameraFov = GGToRadian(fUsedFOV); // Oops - backwards logic, lower FOV needs lower angle passed in
	}
	wiScene::GetCamera().CreatePerspective((float)master.masterrenderer.GetLogicalWidth(), (float)master.masterrenderer.GetLogicalHeight(), visuals->CameraNEAR_f, visuals->CameraFAR_f, fCameraFov);

	if (visuals->bWaterEnable) 
	{
		static float last_wave_amplitude = -1;
		static float last_wind_dependency = -1;
		weather->oceanParameters.wave_amplitude = visuals->fWaterWaveAmplitude;
		weather->oceanParameters.wind_dependency = visuals->fWaterWindDependency;
		if (last_wave_amplitude != weather->oceanParameters.wave_amplitude || last_wind_dependency != weather->oceanParameters.wind_dependency) 
		{
			last_wave_amplitude = weather->oceanParameters.wave_amplitude;
			last_wind_dependency = weather->oceanParameters.wind_dependency;

			// delay update because RunWeatherUpdateSystem needs to run before our new values can take effect
			iUpdateOcean = 2;
		}
	}

	// If in Test Level or in standalone, use visual settings, otherwise just use the temporary editor setting.
	bool bSetting;
	if(t.game.set.ismapeditormode == 1)
		bSetting = t.showeditortrees;
	else
		bSetting = t.visuals.bEndableTreeDrawing;

	// GGMAX 2.94: fold "Trees Off" into the single point draw_enabled is derived, so the legacy
	// CPU tree update, the virtual collision cylinders (M-Physics_part0.cpp:521) and the navmesh
	// tree obstacles all early-out too. Clearing draw_enabled is the CORRECT half of that
	// trade: leave it set and the player collides with trees that are no longer drawn.
	{
		extern bool gg_no_trees;
		if (gg_no_trees) bSetting = false;
	}
	if(ggtrees_global_params.hide_until_update == 0)
		ggtrees_global_params.draw_enabled = bSetting;

	// If in Test Level or in standalone, use visual settings, otherwise just use the temporary editor setting.
	if (t.game.set.ismapeditormode == 1)
		bSetting = t.showeditorveg;
	else
		bSetting = t.visuals.bEndableGrassDrawing;
	gggrass_global_params.draw_enabled = bSetting;
	// UI AUDIT 2026-07-28: shipping grass is Wicked hair entities — draw_enabled only gates
	// the legacy GG path, so the View Options veg checkboxes visibly did nothing. Sweep the
	// live entities too (same mechanism as the G-key toggle).
	GGTerrain::GGTerrainWicked_SetGrassVisible(bSetting);

	// can disable terrain drawing in graphics engine
	if (t.visuals.bEnableEmptyLevelMode == false)
	{
		// can call this to affect some visibles without causing water to flicker
		Wicked_Update_Visibles(voidvisual);
		GGTrees::ggtrees_draw_enabled = 1;

		// UI AUDIT 2026-07-28: same story for the View Options terrain checkboxes —
		// ggterrain_draw_enabled gates dead code; hide the live Wicked chunk objects instead
		// (same mechanism as the O-key toggle).
		bool bTerrainVisible;
		if (t.game.set.ismapeditormode == 1)
			bTerrainVisible = (t.showeditorterrain < 0) ? (t.visuals.bEndableTerrainDrawing != false) : (t.showeditorterrain != 0);
		else
			bTerrainVisible = t.visuals.bEndableTerrainDrawing;
		GGTerrain::GGTerrainWicked_SetTerrainVisible(bTerrainVisible);
	}
	else
	{
		GGTerrain::ggterrain_draw_enabled = 0;
		GGTrees::ggtrees_draw_enabled = 0;
		// GGMAX 2.68e (Lee's repro: Completely Empty Level still showed the grey grid in
		// the generator, the editor AND test game — collision correctly gone, visuals not):
		// the two flags above only gate the DEAD legacy draw path (the 07-28 audit lesson,
		// fourth occurrence) — hide the live wicked chunk objects too. The untick/normal
		// path is the `if` branch above, which re-shows them per the usual checkboxes.
		GGTerrain::GGTerrainWicked_SetTerrainVisible(false);
	}

	// Populate GG custom frame staging data for shader constant buffer (b4)
	ggCustomFrameStaging.treeWind = visuals->tree_wind;
	ggCustomFrameStaging.treeSubSurfaceScattering = visuals->tree_sss;
	ggCustomFrameStaging.sunEnergy = visuals->SunIntensity_f;
	ggCustomFrameStaging.deSaturate = visuals->fDeSaturate;
	ggCustomFrameStaging.waterFogMin = visuals->WaterFogMinDist;
	ggCustomFrameStaging.waterFogMax = visuals->WaterFogMaxDist;
	ggCustomFrameStaging.waterFogMinAmount = visuals->WaterFogMinAmount;
	ggCustomFrameStaging.fogOpacity = visuals->FogA_f;
	ggCustomFrameStaging.fogColor = XMFLOAT3(visuals->FogR_f / 255.0f, visuals->FogG_f / 255.0f, visuals->FogB_f / 255.0f);
	ggCustomFrameStaging.cloudiness = visuals->SkyCloudiness;
	ggCustomFrameStaging.cloudSpeed = visuals->SkyCloudSpeed;

	// values a game script set keep them through this push
	extern void LuaRenderSettings_Apply(void);
	extern void LuaPostEffects_Apply(void);
	LuaRenderSettings_Apply();
	LuaPostEffects_Apply();
	LuaGameVisuals_Apply(voidvisual);
}

void Wicked_Update_Visibles(void* voidvisual)
{
	// vars
	bool bSetting = false;

	if (t.visuals.bEnableEmptyLevelMode == false)
	{
		// If in Test Level or in standalone, use visual settings, otherwise just use the temporary editor setting.
		if (t.game.set.ismapeditormode == 1)
		{
			bSetting = t.showeditorterrain;
		}
		else
		{
			bSetting = t.visuals.bEndableTerrainDrawing;
			if (bSetting == true)
			{
				if (t.hardwareinfoglobals.noterrain == 1) bSetting = false;
			}
		}
		GGTerrain::ggterrain_draw_enabled = (int)bSetting;
	}
	else
	{
		//PE: Can be called from LUA. so also disable here.
		GGTerrain::ggterrain_draw_enabled = 0;
	}
}


