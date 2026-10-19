void imgui_Customize_Weather_V2(int mode)
{
	int wflags = ImGuiTreeNodeFlags_None;
	bool bCHOpen = true;
	if (mode != 3)
	{
		if (pref.bAutoClosePropertySections && iLastOpenHeader != 4)
			ImGui::SetNextItemOpen(false, ImGuiCond_Always);

		bCHOpen = ImGui::StyleCollapsingHeader("Weather", wflags);
	}

	if (bCHOpen && ImGui::windowTabVisible() ) {

		if (mode != 3)
			iLastOpenHeader = 4;

		ImGui::Indent(10);
		ImGui::PushItemWidth(-10);

		ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX(), ImGui::GetCursorPosY() + 3));

		float w = ImGui::GetWindowContentRegionWidth();

		int icon_size = (w - 20.0) / 3.0;
		icon_size -= 7; //Padding


		ImVec2 oldstyle = ImGui::GetStyle().FramePadding;
		ImGui::GetStyle().FramePadding = { 2,2 };

		if (t.visuals.iEnvironmentWeather == 0)
		{
			ImVec2 vSelectionDraw = ImGui::GetCurrentWindow()->DC.CursorPos;
			ImVec2 padding = { 2.0, 2.0 };
			const ImRect image_bb((vSelectionDraw - padding), vSelectionDraw + padding + ImVec2(icon_size, icon_size));
			ImGui::GetCurrentWindow()->DrawList->AddRect(image_bb.Min, image_bb.Max, ImGui::GetColorU32(tool_selected_col), 0.0f, 15, 2.0f);
		}

		if (ImGui::ImgBtn(ENV_SUN, ImVec2(icon_size, icon_size), ImColor(255,255,255,0), ImColor(220, 220, 220, 220), ImColor(255, 255, 255, 255), ImColor(180, 180, 160, 255) ,-1,0,0,0,false,false,false,false,false, bBoostIconColors))
		{
			t.visuals.iEnvironmentWeather = 0;
			t.gamevisuals.iEnvironmentWeather = t.visuals.iEnvironmentWeather;
			g.projectmodified = 1;
		}
		if (ImGui::windowTabVisible() && ImGui::IsItemHovered()) ImGui::SetTooltip("Set No Weather Effect");
		ImGui::SameLine();

		if (t.visuals.iEnvironmentWeather == 1 || t.visuals.iEnvironmentWeather == 2 )
		{
			ImVec2 vSelectionDraw = ImGui::GetCurrentWindow()->DC.CursorPos;
			ImVec2 padding = { 2.0, 2.0 };
			const ImRect image_bb((vSelectionDraw - padding), vSelectionDraw + padding + ImVec2(icon_size, icon_size));
			ImGui::GetCurrentWindow()->DrawList->AddRect(image_bb.Min, image_bb.Max, ImGui::GetColorU32(tool_selected_col), 0.0f, 15, 2.0f);
		}
		if (ImGui::ImgBtn(ENV_RAIN, ImVec2(icon_size, icon_size), ImColor(255, 255, 255, 0), ImColor(220, 220, 220, 220), ImColor(255, 255, 255, 255), ImColor(180, 180, 160, 255), -1, 0, 0, 0, false, false, false, false, false, bBoostIconColors))
		{
			t.visuals.iEnvironmentWeather = 1;
			t.gamevisuals.iEnvironmentWeather = t.visuals.iEnvironmentWeather;
			bEnableWeather = true;
			reset_env_particles();
			g.projectmodified = 1;
		}
		if (ImGui::windowTabVisible() && ImGui::IsItemHovered()) ImGui::SetTooltip("Set Rain Weather Effect");
		ImGui::SameLine();

		if (t.visuals.iEnvironmentWeather == 3 || t.visuals.iEnvironmentWeather == 4)
		{
			ImVec2 vSelectionDraw = ImGui::GetCurrentWindow()->DC.CursorPos;
			ImVec2 padding = { 2.0, 2.0 };
			const ImRect image_bb((vSelectionDraw - padding), vSelectionDraw + padding + ImVec2(icon_size, icon_size));
			ImGui::GetCurrentWindow()->DrawList->AddRect(image_bb.Min, image_bb.Max, ImGui::GetColorU32(tool_selected_col), 0.0f, 15, 2.0f);
		}
		if (ImGui::ImgBtn(ENV_SNOW, ImVec2(icon_size, icon_size), ImColor(255, 255, 255, 0), ImColor(220, 220, 220, 220), ImColor(255, 255, 255, 255), ImColor(180, 180, 160, 255), -1, 0, 0, 0, false, false, false, false, false, bBoostIconColors))
		{
			t.visuals.iEnvironmentWeather = 3;
			t.gamevisuals.iEnvironmentWeather = t.visuals.iEnvironmentWeather;
			bEnableWeather = true;
			reset_env_particles();
			g.projectmodified = 1;
		}
		if (ImGui::windowTabVisible() && ImGui::IsItemHovered()) ImGui::SetTooltip("Set Snow Weather Effect");

		ImGui::GetStyle().FramePadding = oldstyle;

		ImGui::Separator();

		ImGui::TextCenter("Weather Intensity");
		if (ImGui::MaxSliderInputFloat("##fWeatherIntensity:", &t.visuals.fWeatherIntensity, 0.0f, 100.0f, "Set Weather Intensity"))
		{
			t.gamevisuals.fWeatherIntensity = t.visuals.fWeatherIntensity;
			Wicked_Update_Visuals((void *)&t.visuals);
			g.projectmodified = 1;
		}
	
		//Fog

		ImGui::TextCenter("Fog Range");

		float fogDist = sqrt( t.visuals.FogDistance_f );
		float fogStart = sqrt( t.visuals.FogNearest_f );
		if ( fogDist > 1000 ) fogDist = 1000;
		if (ImGui::MaxSliderInputRangeFloat("##WickedFogNewRange", &fogStart, &fogDist, 0.0, 1000.0f, "Set Fog Range"))
		{
			fogStart *= fogStart;
			fogDist *= fogDist;
			t.visuals.FogNearest_f = fogStart;
			t.visuals.FogDistance_f = fogDist;
			t.gamevisuals.FogNearest_f = t.visuals.FogNearest_f;
			t.gamevisuals.FogDistance_f = t.visuals.FogDistance_f;
			Wicked_Update_Visuals((void *)&t.visuals);
			g.projectmodified = 1;
		}
		
		ImGui::TextCenter("Fog Opacity");
		if (ImGui::MaxSliderInputFloat("##WickedFogHeight_f", &t.visuals.FogA_f, 0.0f, 1.0f, "Set Fog Opacity"))
		{
			ggterrain_global_render_params2.flags2 |= GGTERRAIN_SHADER_FLAG2_USE_FOG; //PE: Rick had it disabled at some point, think it has to do with developer mode (key_g), so just make sure to enable it here.
			t.gamevisuals.FogA_f = t.visuals.FogA_f;
			Wicked_Update_Visuals((void *)&t.visuals);
			g.projectmodified = 1;
		}

		ImGui::TextCenter("Horizon/Fog Color");
		ImVec4 mycolor = ImVec4(t.visuals.FogR_f / 255.0, t.visuals.FogG_f / 255.0, t.visuals.FogB_f / 255.0, 1.0);
		bool open_popup = ImGui::ColorButton("##NewV2WickedfHorizonColor", mycolor, 0, ImVec2(w - 20.0, 0));
		if (open_popup)
			ImGui::OpenPopup("##pickV2WickedfHorizonColor");
		if (ImGui::BeginPopup("##pickV2WickedfHorizonColor", ImGuiWindowFlags_NoMove))
		{
			if (ImGui::ColorPicker4("##pickerV2WickedfHorizonColor", (float*)&mycolor, ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview))
			{
				t.gamevisuals.FogR_f = t.visuals.FogR_f = mycolor.x * 255.0;
				t.gamevisuals.FogG_f = t.visuals.FogG_f = mycolor.y * 255.0;
				t.gamevisuals.FogB_f = t.visuals.FogB_f = mycolor.z * 255.0;
				g.projectmodified = 1;
				Wicked_Update_Visuals((void *)&t.visuals);
				g.projectmodified = 1;
			}
			ImGui::EndPopup();
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Set Fog Color");
		ImGuiWindow* window = ImGui::GetCurrentWindow(); //PE: Add a pencil to all color gadgets.
		ID3D11ShaderResourceView* lpTexture = GetImagePointerView(TOOL_PENCIL);
		ImVec2 vDrawPos = { ImGui::GetCursorScreenPos().x + (ImGui::GetContentRegionAvail().x - 30.0f) ,ImGui::GetCursorScreenPos().y - (ImGui::GetFontSize()*1.5f) - 3.0f };
		window->DrawList->AddImage((ImTextureID)lpTexture, vDrawPos, vDrawPos + ImVec2(16, 16), ImVec2(0, 0), ImVec2(1, 1), ImGui::GetColorU32(ImVec4(1, 1, 1, 1)));

		#ifdef POSTPROCESSRAIN
		//PE: Postprocess rain.
		if (t.visuals.iEnvironmentWeather == 1)
		{
			// Hidden for now
			/*
			if (ImGui::StyleCollapsingHeader("Rain Post Process", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::PushItemWidth(-10);
				if (ImGui::Checkbox("Rain Post Process##setRainEnabled", &t.visuals.bRainEnabled)) {
					t.gamevisuals.bRainEnabled = t.visuals.bRainEnabled;
					if (master_renderer)
						master_renderer->setRainEnabled(t.visuals.bRainEnabled); //PE: test post process shader.
					g.projectmodified = 1;
				}
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("Enable Rain Post Process");
				ImGui::PopItemWidth();

				if (t.visuals.bRainEnabled)
				{
					ImGui::PushItemWidth(-10);
					ImGui::TextCenter("Rain Speed X");
					if (ImGui::SliderFloat("##Rain Speed X", &t.visuals.fRainSpeedX, -100.0, 100.0))
					{
						t.gamevisuals.fRainSpeedX = t.visuals.fRainSpeedX;
					}
					ImGui::PopItemWidth();

					ImGui::PushItemWidth(-10);
					ImGui::TextCenter("Rain Speed Y");
					if (ImGui::SliderFloat("##Rain Speed Y", &t.visuals.fRainSpeedY, -100.0, 100.0))
					{
						t.gamevisuals.fRainSpeedY = t.visuals.fRainSpeedY;
					}
					ImGui::PopItemWidth();


					ImGui::PushItemWidth(-10);
					ImGui::TextCenter("Rain Opacity");
					if (ImGui::SliderFloat("##Rain Opacity", &t.visuals.fRainOpacity, -1.0, 1.0))
					{
						master_renderer->setRainOpacity(t.visuals.fRainOpacity);
						t.gamevisuals.fRainOpacity = t.visuals.fRainOpacity;
					}
					ImGui::PopItemWidth();

					ImGui::PushItemWidth(-10);
					ImGui::TextCenter("Rain Scale X");
					if (ImGui::SliderFloat("##Rain Scal X", &t.visuals.fRainScaleX, -4.0, 4.0))
					{
						master_renderer->setRainScaleX(t.visuals.fRainScaleX);
						t.gamevisuals.fRainScaleX = t.visuals.fRainScaleX;
					}
					ImGui::PopItemWidth();

					ImGui::PushItemWidth(-10);
					ImGui::TextCenter("Rain Scale Y");
					if (ImGui::SliderFloat("##Rain Scal Y", &t.visuals.fRainScaleY, -4.0, 4.0))
					{
						master_renderer->setRainScaleY(t.visuals.fRainScaleY);
						t.gamevisuals.fRainScaleY = t.visuals.fRainScaleY;
					}
					ImGui::PopItemWidth();


					ImGui::PushItemWidth(-10);
					ImGui::TextCenter("Refreaction Scale");
					if (ImGui::SliderFloat("##RefreactionScale", &t.visuals.fRainRefreactionScale, 0.0, 0.5))
					{
						master_renderer->setRainRefreactionScale(t.visuals.fRainRefreactionScale);
						t.gamevisuals.fRainRefreactionScale = t.visuals.fRainRefreactionScale;
					}
					ImGui::PopItemWidth();
				}
			}
			*/
			//if (!bEnableWeather)
			//{
			//	if (master_renderer)
			//	{
			//		if(!t.visuals.bRainEnabled)
			//			master_renderer->setRainEnabled(false); //PE: test post process shader.
			//		else
			//			master_renderer->setRainEnabled(t.visuals.bRainEnabled); //PE: test post process shader.
			//	}
			//}
		}
		#endif


		ImGui::Separator();

		static bool bUpdatePPWeather = true;

		if (ImGui::Checkbox("Display Weather in Editor##DisplayWeather", &bEnableWeather))
		{
			reset_env_particles();
			g.projectmodified = 1;
			bUpdatePPWeather = true;
		}

		ImGui::Indent(-10);
		if (ImGui::StyleCollapsingHeader("Wind Controls", ImGuiTreeNodeFlags_DefaultOpen))
		{

			//PE: Weather wind speed.
			float iItemWidth = -10;
			float fWickedStartX = 130;
			// UI AUDIT 2026-07-28: "PP Snow / Dust" + "Disable When Indoor" HIDDEN — the DX11
			// voxel post-process weather path was removed with the WickedEngine upgrade, so the
			// checkboxes visibly did nothing. Fields/save/load kept for level-file compatibility.
			// Restore alongside a Wicked-native snow (weather rain/snow emitter) if wanted.
			//if (ImGui::Checkbox("PP Snow / Dust", &t.visuals.bPPSnow))
			//{
			//	t.gamevisuals.bPPSnow = t.visuals.bPPSnow;
			//	bUpdatePPWeather = true;
			//	if(t.visuals.bPPSnow)
			//		bEnableWeather = true;
			//}
			//if (ImGui::Checkbox("Disable When Indoor", &t.visuals.bpp_disable_indoor))
			//{
			//	t.gamevisuals.bpp_disable_indoor = t.visuals.bpp_disable_indoor;
			//	bUpdatePPWeather = true;
			//}

			ImGui::Text("Wind Speed");
			ImGui::SameLine(); ImGui::SetCursorPosX(fWickedStartX);
			ImGui::PushItemWidth((float)iItemWidth);
			if (ImGui::SliderFloat("##Wind Speed", &t.visuals.wind_speed, 0.0f, 5.0f,"%.2f"))
			{
				t.gamevisuals.wind_speed = t.visuals.wind_speed;
				bUpdatePPWeather = true;
			}
			ImGui::PopItemWidth();

			float dir_width = ((ImGui::GetContentRegionAvailWidth() - fWickedStartX) / 3.0) - 6.0f;

			ImGui::Text("Wind Direction");
			ImGui::SameLine(); ImGui::SetCursorPosX(fWickedStartX);
			ImGui::PushItemWidth((float)dir_width);
			if (ImGui::SliderFloat("##Wind Direction X", &t.visuals.wind_direction_x, -20.0f, 20.0f, "%.1f"))
			{
				t.gamevisuals.wind_direction_x = t.visuals.wind_direction_x;
				bUpdatePPWeather = true;
			}
			ImGui::PopItemWidth();

			// Wind Direction Y (vertical) removed from UI and forced to 0 at apply: vertical wind flings tiny grass blades up/down.

			ImGui::SameLine();
			ImGui::PushItemWidth((float)dir_width);
			if (ImGui::SliderFloat("##Wind Direction Z", &t.visuals.wind_direction_z, -20.0f, 20.0f, "%.1f"))
			{
				t.gamevisuals.wind_direction_z = t.visuals.wind_direction_z;
				bUpdatePPWeather = true;
			}
			ImGui::PopItemWidth();

			ImGui::Text("Wind Randomness");
			ImGui::SameLine(); ImGui::SetCursorPosX(fWickedStartX);
			ImGui::PushItemWidth((float)iItemWidth);
			if (ImGui::SliderFloat("##Windrandomness", &t.visuals.wind_randomness, 0.0f, 2.0f, "%.2f"))
			{
				t.gamevisuals.wind_randomness = t.visuals.wind_randomness;
				bUpdatePPWeather = true;
			}
			ImGui::PopItemWidth();


			// UI AUDIT 2026-07-28: "PP Alpha" HIDDEN — consumed only by the removed DX11 voxel
			// weather path (field no longer exists in the new WickedEngine WeatherComponent).
			//ImGui::Text("PP Alpha");
			//ImGui::SameLine(); ImGui::SetCursorPosX(fWickedStartX);
			//ImGui::PushItemWidth((float)iItemWidth);
			//if (ImGui::SliderFloat("##PPpp_alpha", &t.visuals.pp_alpha, 0.0f, 5.0f, "%.2f"))
			//{
			//	t.gamevisuals.pp_alpha = t.visuals.pp_alpha;
			//	bUpdatePPWeather = true;
			//}
			//ImGui::PopItemWidth();


			// UI AUDIT 2026-07-28: relabeled from the cryptic legacy "PP Size" — the value
			// actually drives weather->windWaveSize (see apply below), so name it that.
			// GGMAX 3.58 (Lee): "Wind Wave Size" -> "Wind Gust Size", and the range opened from
			// 1.1 to 5.0 so larger, slower gusts are reachable. The 1.1 ceiling was inherited from
			// the legacy PP-weather value, not a limit of the engine's windWaveSize.
			// GGMAX 3.62: and the direction is now TRUE - the value is inverted on the way to
			// windWaveSize, because the engine wants a frequency and the label promises a size.
			ImGui::Text("Wind Gust Size");
			ImGui::SameLine(); ImGui::SetCursorPosX(fWickedStartX);
			ImGui::PushItemWidth((float)iItemWidth);
			if (ImGui::SliderFloat("##PPpp_size", &t.visuals.pp_size, 0.0f, 5.0f, "%.2f"))
			{
				t.gamevisuals.pp_size = t.visuals.pp_size;
				bUpdatePPWeather = true;
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Size of the wind gusts rolling through grass, foliage and trees. Larger = broader, slower gusts. 1.0 is the default.");
			ImGui::PopItemWidth();


			// UI AUDIT 2026-07-28: "PP Voxel Steps" HIDDEN — the voxel PP weather renderer is gone.
			//ImGui::Text("PP Voxel Steps");
			//ImGui::SameLine(); ImGui::SetCursorPosX(fWickedStartX);
			//ImGui::PushItemWidth((float)iItemWidth);
			//if (ImGui::SliderFloat("##voxel_steps", &t.visuals.voxel_steps, 1.0f, 40.0f, "%.0f"))
			//{
			//	t.gamevisuals.voxel_steps = t.visuals.voxel_steps;
			//	bUpdatePPWeather = true;
			//}
			//ImGui::PopItemWidth();

			if (bUpdatePPWeather)
			{
				bUpdatePPWeather = false;
				extern wiECS::Entity g_weatherEntityID;
				wiScene::WeatherComponent* weather = wiScene::GetScene().weathers.GetComponent(g_weatherEntityID);
				if (weather)
				{
					//weather->pp_voxel_steps = t.visuals.voxel_steps; // REMOVED
					weather->windDirection = XMFLOAT3(t.visuals.wind_direction_x, 0.0f, t.visuals.wind_direction_z);
					weather->windSpeed = t.visuals.wind_speed;
					// GGMAX 3.62: see GGWind_GustSizeToWaveSize - bigger slider = bigger gusts.
					extern float GGWind_GustSizeToWaveSize(float gustSize);
					weather->windWaveSize = GGWind_GustSizeToWaveSize(t.visuals.pp_size);
					//weather->pp_alpha = t.visuals.pp_alpha; // REMOVED
					weather->windRandomness = t.visuals.wind_randomness;
					//weather->SetPPSnowEnabled(...); // removed in new WickedEngine API - no equivalent
				}
			}
		}
		ImGui::Indent(10);

		ImGui::PopItemWidth();
		ImGui::Indent(-10);
	}
}

// Full sky-type switch (0=Simulated Sky, 1=Sky Box, 2=None) — the exact sequence the
// Customize Sky combo runs, extracted so the automation harness (SET_SKYMODE) can drive
// mode changes for A/B testing without the UI.
void gridedit_set_sky_type(int iSkyType)
{
	bool bRunUpdateVisual = false;
	bool bSimulatedSky = false;
	if (iSkyType == 0)
	{
		t.visuals.skyindex = 0;
		bSimulatedSky = true;
		t.gamevisuals.bDisableSkybox = t.visuals.bDisableSkybox = false;
	}
	if (iSkyType == 1)
	{
		t.visuals.skyindex = 1;
		bSimulatedSky = false;
		t.gamevisuals.bDisableSkybox = t.visuals.bDisableSkybox = false;
	}
	if (iSkyType == 2)
	{
		t.visuals.skyindex = 0;
		bSimulatedSky = false;
		t.gamevisuals.bDisableSkybox = t.visuals.bDisableSkybox = true;
	}

	//NONE only possible with new t.visuals.int
	extern wiECS::Entity g_weatherEntityID;
	wiScene::WeatherComponent* weather = wiScene::GetScene().weathers.GetComponent(g_weatherEntityID);
	int skyindex = 0;
	if (t.visuals.bDisableSkybox)
	{
		weather->SetRealisticSky(false);
		weather->SetVolumetricClouds(false);
	}
	else if (!bSimulatedSky)
	{
		weather->SetRealisticSky(false);
		weather->SetVolumetricClouds(false);
		skyindex = 1;
	}
	else
	{
		weather->SetRealisticSky(true);
		weather->SetVolumetricClouds(true);
		t.gamevisuals.iTimeOfday = t.visuals.iTimeOfday;
		visuals_calcsunanglefromtimeofday(t.gamevisuals.iTimeOfday, &t.gamevisuals.SunAngleX, &t.gamevisuals.SunAngleY, &t.gamevisuals.SunAngleZ);
		t.visuals.SunAngleX = t.gamevisuals.SunAngleX;
		t.visuals.SunAngleY = t.gamevisuals.SunAngleY;
		t.visuals.SunAngleZ = t.gamevisuals.SunAngleZ;
		bRunUpdateVisual = true;
	}

	t.visuals.skyindex = skyindex;
	g.projectmodified = 1;
	t.visuals.skyindex = skyindex;
	t.gamevisuals.skyindex = t.visuals.skyindex;

	g.skyindex = t.visuals.skyindex;
	t.visuals.sky_s = t.skybank_s[g.skyindex];
	t.gamevisuals.sky_s = t.skybank_s[g.skyindex];
	t.terrainskyspecinitmode = 0; sky_skyspec_init();
	t.sky.currenthour_f = 8.0;
	t.sky.daynightprogress = 0;

	visuals_justshaderupdate();

	// if change sky, regenerate env map
	t.visuals.refreshskysettingsfromlua = true;
	cubemap_generateglobalenvmap();
	t.visuals.refreshskysettingsfromlua = false;

	if (bRunUpdateVisual)
	{
		Wicked_Update_Visuals((void *)&t.visuals);
	}

	// when sky type changes, refresh env probes
	extern bool g_bLightProbeScaleChanged;
	g_bLightProbeScaleChanged = true;
	WickedCall_UpdateProbes();
}

void imgui_Customize_Sky_V2(int mode)
{
	int wflags = ImGuiTreeNodeFlags_DefaultOpen;

	if (pref.bAutoClosePropertySections && iLastOpenHeader != 1)// && iLastOpenHeader < 14)
		ImGui::SetNextItemOpen(false, ImGuiCond_Always);

	if (ImGui::StyleCollapsingHeader("Customize Sky", wflags))
	{
		iLastOpenHeader = 1;
		ImGui::Indent(10);
		ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX(), ImGui::GetCursorPosY() + 3));
		ImGui::PushItemWidth(-10);

		int oldSkyIndex = t.visuals.skyindex;

		char * current_sky = t.skybank_s[t.visuals.skyindex].Get();
		if (t.skybank_s[t.visuals.skyindex] == "None") current_sky = "Dynamic Clouds";
		if (!current_sky) current_sky = "NA";

		//PE: Simulated Sky
		bool bSimulatedSky = false;
		if (t.visuals.skyindex == 0)
			bSimulatedSky = true;

		static int iSkyType = 0;

		if (t.visuals.bDisableSkybox) iSkyType = 2;
		else if (!bSimulatedSky) iSkyType = 1;
		else iSkyType = 0;

		const char* sky_combo[] = { "Simulated Sky", "Sky Box" , "None" };
		if (ImGui::Combo("##Combosky_combo", &iSkyType, sky_combo, IM_ARRAYSIZE(sky_combo)))
		{
			gridedit_set_sky_type(iSkyType);
			bSimulatedSky = (t.visuals.skyindex == 0 && !t.visuals.bDisableSkybox);
			current_sky = t.skybank_s[t.visuals.skyindex].Get();//t.terrainstylebank_s[skyindex].Get();
		}

		bool somethingChanged = false;
		if ( oldSkyIndex != t.visuals.skyindex ) somethingChanged = true;

		ImGui::PopItemWidth();

		float w = ImGui::GetWindowContentRegionWidth();

		if (!bSimulatedSky)
		{
			ImGui::Indent(-10);
			//ImGui::Indent(4);
			ImGui::Indent(-2); //PE: Windows default to indent 4.

			int iMaxVisibleSkyBoxes = 8;
			static float fContentHeight = 0;
			static int iActiveSkybBoxes = 0;
			static ImVec2 vLastRunHeight = { 0,0 };
			if (fContentHeight <= 85) {
				fContentHeight = 85; //One line default. and prevent flicker.
			}
			vLastRunHeight = { 0 ,fContentHeight };

			ImVec2 oldstylemain = ImGui::GetStyle().FramePadding;
			ImVec2 oldwinstylemain = ImGui::GetStyle().WindowPadding;
			ImGui::GetStyle().WindowPadding = { 0,0 };
			ImGui::GetStyle().FramePadding = { 0,0 };

			ImVec2 vWindowPos = ImGui::GetWindowPos();
			ImVec2 vWindowSize = ImGui::GetWindowSize();

			if(iActiveSkybBoxes > iMaxVisibleSkyBoxes)
				ImGui::BeginChild("##skybox4x4forscrollbar", vLastRunHeight, false, ImGuiWindowFlags_AlwaysVerticalScrollbar);
			else
				ImGui::BeginChild("##skybox4x4forscrollbar", vLastRunHeight, false, ImGuiWindowFlags_NoScrollbar);

			ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(0, 3));

			ImGui::Columns(4, "##skybox4x4columns", false);  //false no border

			iActiveSkybBoxes = 0;
			float curposy = ImGui::GetCursorPosY();
			for (int skyindex = 1; skyindex <= g.skymax; skyindex++)
			{

				if (t.skybank_s[skyindex].Len() > 0)
				{
					if (iActiveSkybBoxes++ <= iMaxVisibleSkyBoxes)
						fContentHeight = ImGui::GetCursorPosY() - curposy;

					cstr materialname = cstr(" ") + t.skybank_s[skyindex];

					int iLargerPreviewIconSize = 28;//PE: lowest possible icon
					float control_width = (iLargerPreviewIconSize + 3.0) * 4.0f + 6.0;

					if ( (w-ImGui::GetCurrentWindow()->ScrollbarSizes.x) > control_width) {
						//PE: fit perfectly with window width.
						iLargerPreviewIconSize = ( (w-ImGui::GetCurrentWindow()->ScrollbarSizes.x) - 20.0) / 4.0;
						iLargerPreviewIconSize -= 7.0; //Padding.
						if (iLargerPreviewIconSize > 70) iLargerPreviewIconSize = 70;
					}
					iLargerPreviewIconSize += 2;

					ImVec2 vSelectionDraw = ImGui::GetCurrentWindow()->DC.CursorPos;

					int iSkyIcon = TOOL_VISUALS;
					if (ImageExist(SKYBOX_ICONS + skyindex))
						iSkyIcon = SKYBOX_ICONS + skyindex;

					cStr sLabelChild = cStr("##skybox4x4") + cStr(skyindex);
					ImVec2 content_avail = { iLargerPreviewIconSize + 1.0f ,iLargerPreviewIconSize + 1.0f };

					ImGui::BeginChild(sLabelChild.Get(), content_avail, false, ImGuiWindowFlags_NoScrollbar);

					if (ImGui::ImgBtn(iSkyIcon, ImVec2(iLargerPreviewIconSize, iLargerPreviewIconSize), ImColor(0, 0, 0, 255)))
					{
						g.projectmodified = 1;
						current_sky = t.skybank_s[skyindex].Get();//t.terrainstylebank_s[skyindex].Get();
						t.visuals.skyindex = skyindex;
						t.gamevisuals.skyindex = t.visuals.skyindex;

						g.skyindex = t.visuals.skyindex;
						t.visuals.sky_s = t.skybank_s[g.skyindex];
						t.gamevisuals.sky_s = t.skybank_s[g.skyindex];
						t.terrainskyspecinitmode = 0;
						sky_skyspec_init();
						t.sky.currenthour_f = 8.0;
						t.sky.daynightprogress = 0;

						// must confirm new sky settings in t.gamevisuals
						t.gamevisuals.SunAngleX = t.visuals.SunAngleX;
						t.gamevisuals.SunAngleY = t.visuals.SunAngleY;
						t.gamevisuals.SunAngleZ = t.visuals.SunAngleZ;
						t.gamevisuals.SunRed_f = t.visuals.SunRed_f;
						t.gamevisuals.SunGreen_f = t.visuals.SunGreen_f;
						t.gamevisuals.SunBlue_f = t.visuals.SunBlue_f;
						t.gamevisuals.SunIntensity_f = t.visuals.SunIntensity_f;
						t.gamevisuals.fExposure = t.visuals.fExposure;
						t.gamevisuals.AmbienceRed_f = t.visuals.AmbienceRed_f;
						t.gamevisuals.AmbienceGreen_f = t.visuals.AmbienceGreen_f;
						t.gamevisuals.AmbienceBlue_f = t.visuals.AmbienceBlue_f;

						visuals_justshaderupdate();
						// if change sky, regenerate env map
						t.visuals.refreshskysettingsfromlua = true;
						cubemap_generateglobalenvmap();
						t.visuals.refreshskysettingsfromlua = false;

						WickedCall_UpdateProbes();
					}

					if (ImGui::IsItemHovered())
					{
						int tooltip_height = 200;
						int tooltip_width = 200;
						ImVec2 tooltip_position = ImVec2(ImGui::GetWindowPos());
						tooltip_position.x = vWindowPos.x - (tooltip_width + 10.0);
						if ((tooltip_position.y + tooltip_height) > (vWindowPos.y + vWindowSize.y))
							tooltip_position.y = (vWindowPos.y + vWindowSize.y) - tooltip_height - 10.0;
						ImGui::SetNextWindowPos(tooltip_position);
						ImGui::BeginTooltip();
						ImGui::ImgBtn(iSkyIcon, ImVec2(200, 200), ImColor(0, 0, 0, 255), ImColor(220, 220, 220, 220), ImColor(255, 255, 255, 255), ImColor(180, 180, 160, 255), -1, 0, 0, 0, false, false, true);
						ImGui::TextCenter(materialname.Get());
						ImGui::Separator();
						ImGui::EndTooltip();
					}

					ImGui::EndChild();

					ImGui::TextCenter(materialname.Get());

					if (current_sky == t.skybank_s[skyindex].Get())
					{
						ImVec2 padding = { 2.0, 2.0 };
						const ImRect image_bb((vSelectionDraw - padding), vSelectionDraw + padding + ImVec2(iLargerPreviewIconSize, iLargerPreviewIconSize));
						ImGui::GetCurrentWindow()->DrawList->AddRect(image_bb.Min, image_bb.Max, ImGui::GetColorU32(tool_selected_col), 0.0f, 15, 2.0f);
					}

					ImGui::NextColumn();
				}
			}
			ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(0, 2));

			if (iActiveSkybBoxes <= iMaxVisibleSkyBoxes)
				fContentHeight = 2.0+(ImGui::GetCursorPosY() - curposy);

			ImGui::GetStyle().WindowPadding = oldwinstylemain;
			ImGui::GetStyle().FramePadding = oldstylemain;

			ImGui::EndChild();
			ImGui::Indent(2);
			ImGui::Columns(1);
			ImGui::Indent(10);
		}

		ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX(), ImGui::GetCursorPosY() + 3));

		//Must have, Sun color.
		float fAmbience[4], fSunColor[4], fHorizonColor[4], fZenith[4];
		extern wiECS::Entity g_weatherEntityID;
		wiScene::WeatherComponent* weather = wiScene::GetScene().weathers.GetComponent(g_weatherEntityID);

		ImGui::PushItemWidth(-10);

		ImGui::TextCenter("Lighting Color");
		ImVec4 mycolor = ImVec4(t.visuals.SunRed_f / 255.0, t.visuals.SunGreen_f / 255.0, t.visuals.SunBlue_f / 255.0,1.0);
		bool open_popup = ImGui::ColorButton("##NewV2WickedSunColor", mycolor, 0, ImVec2(w-20.0, 0));
		if (open_popup)
			ImGui::OpenPopup("##pickWickedSunColor");
		if ( ImGui::BeginPopup("##pickWickedSunColor", ImGuiWindowFlags_NoMove) )
		{
			if (ImGui::ColorPicker4("##pickerWickedSunColor", (float*)&mycolor, ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview))
			{
				t.gamevisuals.SunRed_f = t.visuals.SunRed_f = mycolor.x * 255.0;
				t.gamevisuals.SunGreen_f = t.visuals.SunGreen_f = mycolor.y * 255.0;
				t.gamevisuals.SunBlue_f = t.visuals.SunBlue_f = mycolor.z * 255.0;
				g.projectmodified = 1;
				Wicked_Update_Visuals((void *)&t.visuals);
				// when sky type changes, refresh env probes
				extern bool g_bLightProbeScaleChanged;
				g_bLightProbeScaleChanged = true;
				WickedCall_UpdateProbes();
			}
			ImGui::EndPopup();
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Sets the light that is being emitted from the sun into the scene");
		ImGuiWindow* window = ImGui::GetCurrentWindow(); //PE: Add a pencil to all color gadgets.
		ID3D11ShaderResourceView* lpTexture = GetImagePointerView(TOOL_PENCIL);
		ImVec2 vDrawPos = { ImGui::GetCursorScreenPos().x + (ImGui::GetContentRegionAvail().x - 30.0f) ,ImGui::GetCursorScreenPos().y - (ImGui::GetFontSize()*1.5f) - 3.0f };
		window->DrawList->AddImage((ImTextureID)lpTexture, vDrawPos, vDrawPos + ImVec2(16, 16), ImVec2(0, 0), ImVec2(1, 1), ImGui::GetColorU32(ImVec4(1, 1, 1, 1)));

		//PE: Add additional color options.
		ImGui::TextCenter("Ambient Color");
		mycolor = ImVec4(t.visuals.AmbienceRed_f / 255.0, t.visuals.AmbienceGreen_f / 255.0, t.visuals.AmbienceBlue_f / 255.0, 1.0);
		open_popup = ImGui::ColorButton("##NewV2WickedAmbientColor", mycolor, 0, ImVec2(w - 20.0, 0));
		if (open_popup)
			ImGui::OpenPopup("##pickWickedAmbientColor");
		if (ImGui::BeginPopup("##pickWickedAmbientColor", ImGuiWindowFlags_NoMove))
		{
			if (ImGui::ColorPicker4("##pickerWickedAmbientColor", (float*)&mycolor, ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview))
			{
				t.gamevisuals.AmbienceRed_f = t.visuals.AmbienceRed_f = mycolor.x * 255.0;
				t.gamevisuals.AmbienceGreen_f = t.visuals.AmbienceGreen_f = mycolor.y * 255.0;
				t.gamevisuals.AmbienceBlue_f = t.visuals.AmbienceBlue_f = mycolor.z * 255.0;
				g.projectmodified = 1;
				Wicked_Update_Visuals((void *)&t.visuals);
				// when sky type changes, refresh env probes
				extern bool g_bLightProbeScaleChanged;
				g_bLightProbeScaleChanged = true;
				WickedCall_UpdateProbes();
			}
			ImGui::EndPopup();
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("The general ambient light around the whole scene uses this color");
		ImGuiWindow* window2 = ImGui::GetCurrentWindow(); //PE: Add a pencil to all color gadgets.
		ID3D11ShaderResourceView* lpTexture2 = GetImagePointerView(TOOL_PENCIL);
		ImVec2 vDrawPos2 = { ImGui::GetCursorScreenPos().x + (ImGui::GetContentRegionAvail().x - 30.0f) ,ImGui::GetCursorScreenPos().y - (ImGui::GetFontSize()*1.5f) - 3.0f };
		window2->DrawList->AddImage((ImTextureID)lpTexture2, vDrawPos2, vDrawPos2 + ImVec2(16, 16), ImVec2(0, 0), ImVec2(1, 1), ImGui::GetColorU32(ImVec4(1, 1, 1, 1)));


		if (t.visuals.bDisableSkybox)
		{
			ImGui::TextCenter("Horizon/Fog Color");
			mycolor = ImVec4(t.visuals.FogR_f / 255.0, t.visuals.FogG_f / 255.0, t.visuals.FogB_f / 255.0, 1.0);
			open_popup = ImGui::ColorButton("##NewV2WickedHorizonColor", mycolor, 0, ImVec2(w - 20.0, 0));
			if (open_popup)
				ImGui::OpenPopup("##pickWickedHorizonColor");
			if (ImGui::BeginPopup("##pickWickedHorizonColor", ImGuiWindowFlags_NoMove))
			{
				if (ImGui::ColorPicker4("##pickerWickedHorizonColor", (float*)&mycolor, ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview))
				{
					t.gamevisuals.FogR_f = t.visuals.FogR_f = mycolor.x * 255.0;
					t.gamevisuals.FogG_f = t.visuals.FogG_f = mycolor.y * 255.0;
					t.gamevisuals.FogB_f = t.visuals.FogB_f = mycolor.z * 255.0;
					g.projectmodified = 1;
					Wicked_Update_Visuals((void *)&t.visuals);
					// when sky type changes, refresh env probes
					extern bool g_bLightProbeScaleChanged;
					g_bLightProbeScaleChanged = true;
					WickedCall_UpdateProbes();
				}
				ImGui::EndPopup();
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Set Horizon/Fog Color");
			window = ImGui::GetCurrentWindow(); //PE: Add a pencil to all color gadgets.
			lpTexture = GetImagePointerView(TOOL_PENCIL);
			vDrawPos = { ImGui::GetCursorScreenPos().x + (ImGui::GetContentRegionAvail().x - 30.0f) ,ImGui::GetCursorScreenPos().y - (ImGui::GetFontSize()*1.5f) - 3.0f };
			window->DrawList->AddImage((ImTextureID)lpTexture, vDrawPos, vDrawPos + ImVec2(16, 16), ImVec2(0, 0), ImVec2(1, 1), ImGui::GetColorU32(ImVec4(1, 1, 1, 1)));


			ImGui::TextCenter("Atmospheric Color");
			mycolor = ImVec4(t.visuals.ZenithRed_f / 255.0, t.visuals.ZenithGreen_f / 255.0, t.visuals.ZenithBlue_f / 255.0, 1.0);
			open_popup = ImGui::ColorButton("##NewV2WickedZenithColor", mycolor, 0, ImVec2(w - 20.0, 0));
			if (open_popup)
				ImGui::OpenPopup("##pickWickedZenithColor");
			if (ImGui::BeginPopup("##pickWickedZenithColor", ImGuiWindowFlags_NoMove))
			{
				if (ImGui::ColorPicker4("##pickerWickedZenithColor", (float*)&mycolor, ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview))
				{
					t.gamevisuals.ZenithRed_f = t.visuals.ZenithRed_f = mycolor.x * 255.0;
					t.gamevisuals.ZenithGreen_f = t.visuals.ZenithGreen_f = mycolor.y * 255.0;
					t.gamevisuals.ZenithBlue_f = t.visuals.ZenithBlue_f = mycolor.z * 255.0;
					g.projectmodified = 1;
					Wicked_Update_Visuals((void *)&t.visuals);
					// when sky type changes, refresh env probes
					extern bool g_bLightProbeScaleChanged;
					g_bLightProbeScaleChanged = true;
					WickedCall_UpdateProbes();
				}
				ImGui::EndPopup();
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Set Atmospheric Color");
			vDrawPos = { ImGui::GetCursorScreenPos().x + (ImGui::GetContentRegionAvail().x - 30.0f) ,ImGui::GetCursorScreenPos().y - (ImGui::GetFontSize()*1.5f) - 3.0f };
			window->DrawList->AddImage((ImTextureID)lpTexture, vDrawPos, vDrawPos + ImVec2(16, 16), ImVec2(0, 0), ImVec2(1, 1), ImGui::GetColorU32(ImVec4(1, 1, 1, 1)));

		}

		ImGui::TextCenter("Sun Intensity");
		float fTmp = t.visuals.SunIntensity_f * 10.0f;
		if (ImGui::SliderFloat("##WickedSunIntensity_f", &fTmp , 0.0, 500.0, "%.2f", 2.0f))
		{
			t.visuals.SunIntensity_f = fTmp * 0.1f;
			t.gamevisuals.SunIntensity_f = t.visuals.SunIntensity_f;
			Wicked_Update_Visuals((void *)&t.visuals);
			g.projectmodified = 1;
			// when sky type changes, refresh env probes
			extern bool g_bLightProbeScaleChanged;
			g_bLightProbeScaleChanged = true;
			WickedCall_UpdateProbes();
		}

		ImGui::TextCenter("Exposure");
		fTmp = t.visuals.fExposure * 100.0f;
		if (ImGui::SliderFloat("##fFixedExposure:", &fTmp, 0.0, 400.0, "%.2f", 2.0f))
		{
			t.visuals.fExposure = fTmp * 0.01f;
			t.gamevisuals.fExposure = t.visuals.fExposure;
			Wicked_Update_Visuals((void *)&t.visuals);
			g.projectmodified = 1;
			// when sky type changes, refresh env probes
			extern bool g_bLightProbeScaleChanged;
			g_bLightProbeScaleChanged = true;
			WickedCall_UpdateProbes();
		}

		// UI AUDIT 2026-07-28 (v2): "Global Probe Brightness" RESTORED via engine delta 1.55 —
		// the probe cubemap sample is now scaled at lighting time (no probe re-render needed,
		// so dragging is smooth and live). Applied through Wicked_Update_Visuals.
		ImGui::TextCenter("Global Probe Brightness");
		fTmp = t.visuals.fEnvProbeBrightness;
		if (ImGui::MaxSliderInputFloat("##HDRI Brightness", &fTmp, 0.01f, 10.0f, "Specify the brightness of the global environment probe"))
		{
			t.visuals.fEnvProbeBrightness = fTmp;
			t.gamevisuals.fEnvProbeBrightness = t.visuals.fEnvProbeBrightness;
			Wicked_Update_Visuals((void*)&t.visuals);
			g.projectmodified = 1;
		}

		if ( !bSimulatedSky || t.visuals.bDisableSkybox )
		{
		}
		else
		{
			
			ImGui::TextCenter("Cloud Density");
			fTmp = t.visuals.SkyCloudiness * 100.0f;
			if (ImGui::MaxSliderInputFloat("##V2WickedSkyCloudiness", &fTmp, 0.0f, 400.0f, "Sets how dense the clouds are", 0.0f, 400.0f))
			{
				t.visuals.SkyCloudiness = fTmp * 0.01f;
				t.gamevisuals.SkyCloudiness = t.visuals.SkyCloudiness;
				weather->volumetricCloudParameters.layerFirst.coverageAmount = t.visuals.SkyCloudiness;
				g.projectmodified = 1;
				// when sky type changes, refresh env probes
				extern bool g_bLightProbeScaleChanged;
				g_bLightProbeScaleChanged = true;
				WickedCall_UpdateProbes();
			}
			ImGui::TextCenter("Cloud Coverage");
			fTmp = t.visuals.SkyCloudCoverage * 100.0f;
			if (ImGui::MaxSliderInputFloat("##V2WickedSkyCloudCoverage", &fTmp, 0.0f, 200.0f, "How much of the sky is covered with clouds", 0.0f, 200.0f ))
			{
				t.visuals.SkyCloudCoverage = fTmp * 0.01f;
				t.gamevisuals.SkyCloudCoverage = t.visuals.SkyCloudCoverage;
				{ // DX11-era shader used saturate(CoverageMinimum - 1.0) — the UI value is 1-based
				float ggCovMin = t.visuals.SkyCloudCoverage - 1.0f;
				if (ggCovMin < 0.0f) ggCovMin = 0.0f; if (ggCovMin > 1.0f) ggCovMin = 1.0f;
				weather->volumetricCloudParameters.layerFirst.coverageMinimum = ggCovMin; }
				g.projectmodified = 1;
				// when sky type changes, refresh env probes
				extern bool g_bLightProbeScaleChanged;
				g_bLightProbeScaleChanged = true;
				WickedCall_UpdateProbes();
			}
			ImGui::TextCenter("Cloud Height (meters)");
			float cloudHeight = GGTerrain_UnitsToMeters( t.visuals.SkyCloudHeight );
			if (ImGui::SliderFloat("##V2WickedSkyCloudScale", &cloudHeight, -100, 3500, "%.0f", 2.0f))
			{
				t.visuals.SkyCloudHeight = GGTerrain_MetersToUnits( cloudHeight );
				t.gamevisuals.SkyCloudHeight = t.visuals.SkyCloudHeight;
				//weather->cloudScale = t.visuals.SkyCloudHeight; // REMOVED
				// SKY FIX 2026-07-31: engine cloud heights are WORLD units (inches), not meters
				weather->volumetricCloudParameters.cloudStartHeight = t.visuals.SkyCloudHeight;
				g.projectmodified = 1;
				// when sky type changes, refresh env probes
				extern bool g_bLightProbeScaleChanged;
				g_bLightProbeScaleChanged = true;
				WickedCall_UpdateProbes();
			}
			ImGui::TextCenter("Cloud Thickness (meters)");
			cloudHeight = GGTerrain_UnitsToMeters( t.visuals.SkyCloudThickness );
			fTmp = cloudHeight * 0.1f;
			if (ImGui::MaxSliderInputFloat("##V2WickedSkyCloudThickness", &fTmp, 0, 400, "This is how thick the clouds are from top to bottom", 0, 400))
			{
				cloudHeight = fTmp * 10.0f;
				t.visuals.SkyCloudThickness = GGTerrain_MetersToUnits( cloudHeight );
				t.gamevisuals.SkyCloudThickness = t.visuals.SkyCloudThickness;
				// SKY FIX 2026-07-31: engine cloud heights are WORLD units (inches), not meters
				weather->volumetricCloudParameters.cloudThickness = t.visuals.SkyCloudThickness;
				g.projectmodified = 1;
				// when sky type changes, refresh env probes
				extern bool g_bLightProbeScaleChanged;
				g_bLightProbeScaleChanged = true;
				WickedCall_UpdateProbes();
			}
			ImGui::TextCenter("Cloud Speed");
			if (ImGui::MaxSliderInputFloat("##V2WickedSkyCloudSpeed", &t.visuals.SkyCloudSpeed, 0.0f, 50.0f, "The cloud movement speed across the sky", 0.0f, 50.0f))
			{
				t.gamevisuals.SkyCloudSpeed = t.visuals.SkyCloudSpeed;
				weather->volumetricCloudParameters.layerFirst.windSpeed = t.visuals.SkyCloudSpeed;
				weather->volumetricCloudParameters.layerFirst.coverageWindSpeed = t.visuals.SkyCloudSpeed;
				g.projectmodified = 1;
			}
		}
		if (!t.visuals.bSimulate24Hours && (bSimulatedSky || t.visuals.bDisableSkybox) )
		{
			ImGui::TextCenter("Time of Day");
			const char* time_combo[] = { "Dawn", "Morning", "Midday","Afternoon", "Evening", "Dusk","Night" };
			if (ImGui::Combo("##ComboTimeOfDay", &t.visuals.iTimeOfday, time_combo, IM_ARRAYSIZE(time_combo)))
			{
				//PE: Now only used to set SunAngle one time, SunAngle is used everywhere from now on.
				// > 12 = 0-180 , < 12 = 180-360
				t.gamevisuals.iTimeOfday = t.visuals.iTimeOfday;
				visuals_calcsunanglefromtimeofday(t.gamevisuals.iTimeOfday, &t.gamevisuals.SunAngleX, &t.gamevisuals.SunAngleY, &t.gamevisuals.SunAngleZ);
				t.visuals.SunAngleX = t.gamevisuals.SunAngleX;
				t.visuals.SunAngleY = t.gamevisuals.SunAngleY;
				t.visuals.SunAngleZ = t.gamevisuals.SunAngleZ;
				Wicked_Update_Visuals((void *)&t.visuals);
				g.projectmodified = 1;
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("This sets the sun at the correct position for the time of day");
 		}
		if (bSimulatedSky || t.visuals.bDisableSkybox )
		{
			extern void ControlAdvancedSetting(int&, const char*, bool* = nullptr);
			ControlAdvancedSetting(pref.iEnableAdvancedSky, "advanced sky settings");

			if (pref.iEnableAdvancedSky)
			{
				ImGui::TextCenter("Sun Direction X");
				if (ImGui::MaxSliderInputFloat("##sunAngleX:", &t.visuals.SunAngleX, 0.0, 360.0, "Sets the X direction of the sun"))
				{
					t.gamevisuals.SunAngleX = t.visuals.SunAngleX;
					Wicked_Update_Visuals((void *)&t.visuals);
					g.projectmodified = 1;
					// when sky type changes, refresh env probes
					extern bool g_bLightProbeScaleChanged;
					g_bLightProbeScaleChanged = true;
					WickedCall_UpdateProbes();
				}
				ImGui::TextCenter("Sun Direction Y");
				if (ImGui::MaxSliderInputFloat("##sunAngleY:", &t.visuals.SunAngleY, 0.0, 360.0, "Sets the Y direction of the sun"))
				{
					t.gamevisuals.SunAngleY = t.visuals.SunAngleY;
					Wicked_Update_Visuals((void *)&t.visuals);
					g.projectmodified = 1;
					// when sky type changes, refresh env probes
					extern bool g_bLightProbeScaleChanged;
					g_bLightProbeScaleChanged = true;
					WickedCall_UpdateProbes();
				}
				ImGui::TextCenter("Sun Direction Z");
				if (ImGui::MaxSliderInputFloat("##sunAngleZ:", &t.visuals.SunAngleZ, 0.0, 360.0, "Sets the Z direction of the sun"))
				{
					t.gamevisuals.SunAngleZ = t.visuals.SunAngleZ;
					Wicked_Update_Visuals((void *)&t.visuals);
					g.projectmodified = 1;
					// when sky type changes, refresh env probes
					extern bool g_bLightProbeScaleChanged;
					g_bLightProbeScaleChanged = true;
					WickedCall_UpdateProbes();
				}
			}
		}
		
		if ( somethingChanged )
		{
			Wicked_Update_Visuals((void *)&t.visuals);
			g.projectmodified = 1;
		}

		ImGui::Indent(-10);
		ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX(), ImGui::GetCursorPosY() + 3));
	}
}

void terrain_resetfornewlevel (void)
{
	extern int g_iDisableTerrainSystem;
	if (g_iDisableTerrainSystem == 0)
	{
		// so loading new level, making new level clears sculpt data
		GGTerrain_ResetSculpting();
		reset_terrain_paint_date();
		undosys_terrain_preparefornewlevel();
	}
}

// Placeholders until new terrain system established
void terrain_initstyles(void) {}
void terrain_initstyles_reset ( void ) {}
void terrain_setupedit ( void ) {}
int terrain_loadcustomtexture(LPSTR pDestPathAndFile, int iTextureSlot) { return 0; }
cstr terrain_getterrainfolder ( void )  { return ""; }
void terrain_paintselector_hide ( void ) {}
void terrain_deletesupertexturepalette ( void ) {}
int terrain_createnewterraintexture ( LPSTR pDestTerrainTextureFile, int iWhichTextureOver, LPSTR pTexFileToLoad, int iSeamlessMode, int iCompressIt ) { return 0; }
void terrain_loadlatesttexture ( void ) {}
void terrain_changestyle ( void ) {}
void terrain_getpaintmode ( void ) {}
void terrain_terraintexturesystempainterentry ( void ) {}
void terrain_detectendofterraintexturesystempainter ( void ) {}

void terrain_UpdateInputAndEntities ( void )
{
	// copied from M-Terrain.cpp
	if ( t.conkit.entityeditmode == 0 )
	{
		// Control painter objects
		// From Task:"Some number keys seem to affect the right hand tool section ... This should not happen."
		if (csForceKey2 == "1")  t.terrain.terrainpaintermode = 1;
		if (csForceKey2 == "2")  t.terrain.terrainpaintermode = 2;
		if (csForceKey2 == "3")  t.terrain.terrainpaintermode = 3;
		if (csForceKey2 == "4")  t.terrain.terrainpaintermode = 4;
		if (csForceKey2 == "5")  t.terrain.terrainpaintermode = 5;
		if (csForceKey2 == "6")  t.terrain.terrainpaintermode = 6;
		if (csForceKey2 == "7")  t.terrain.terrainpaintermode = 7;
		if (csForceKey2 == "8")  t.terrain.terrainpaintermode = 8;
		if (csForceKey2 == "9")  t.terrain.terrainpaintermode = 9;
		if (csForceKey2 == "0")  t.terrain.terrainpaintermode = 10;
		if (csForceKey2 == "11")  t.terrain.terrainpaintermode = 11;
		if (csForceKey2 == "12")  t.terrain.terrainpaintermode = 12;

		t.tmin = 50; // 220216 - new standard size for both modes
		if ( t.inputsys.k_s == "-" && t.terrain.RADIUS_f > t.tmin )  t.terrain.RADIUS_f = t.terrain.RADIUS_f - ( 25 * t.terrain.ts_f );
		if ( t.inputsys.k_s == "=" && t.terrain.RADIUS_f < g.fTerrainBrushSizeMax )  t.terrain.RADIUS_f = t.terrain.RADIUS_f + ( 25 * t.terrain.ts_f );

		//PE: We will need to add t.entityelement [ t.e ].floorposy again to raise/lower objects when changing terrain.
		if ( t.terrain.terrainpaintermode >= 1 && t.terrain.terrainpaintermode <= 5 )
		{
			// this can also undo all InstanceStamp ( constructs )
			t.trevealallinstancestampentities = 0;

			// raise any entities subject to this terrain radius
			for ( t.e = 1; t.e <= g.entityelementlist; t.e++ )
			{
				t.entid = t.entityelement [ t.e ].bankindex;
				if (t.entityelement[t.e].floorposy > -89998 && t.entityelement[t.e].editorlock == false)
				{
					t.obj = t.entityelement [ t.e ].obj;
					if ( g.gridlayershowsingle == 1 )
					{
						//  do not select if TAB slice mode active and entity too big (buildings, walls, etc)
						if ( t.obj > 0 )
						{
							if ( ObjectSizeX ( t.obj ) > 95 && ObjectSizeY ( t.obj ) > 95 && ObjectSizeZ ( t.obj ) > 95 )
							{
								t.obj = 0;
							}
						}
					}
					if ( t.obj > 0 )
					{
						if ( ObjectExist ( t.obj ) == 1 )
						{
							//LB: remembers original height from original floor, so can maintain things like items on tables, etc
							t.tadjy_f = BT_GetGroundHeight (t.terrain.TerrainID, t.entityelement[t.e].x, t.entityelement[t.e].z) - t.entityelement[t.e].floorposy;
							if ( t.tadjy_f != 0 )
							{
								t.entityelement [ t.e ].y = t.entityelement [ t.e ].y + t.tadjy_f;
								if ( t.conkit.editmodeactive == 1 )
								{
									//  when in FPS 3D Edit Mode - when physics is ACTIVE - must reset entity in new position
									t.tphyobj = t.entityelement [ t.e ].obj;
									physics_disableobject ( );
									PositionObject ( t.tphyobj, t.entityelement [ t.e ].x, t.entityelement [ t.e ].y, t.entityelement [ t.e ].z );
									RotateObject ( t.tphyobj, t.entityelement [ t.e ].rx, t.entityelement [ t.e ].ry, t.entityelement [ t.e ].rz );
									physics_prepareentityforphysics ( );
									//  also record this change PERMINANTLY for return to editor
									if ( ArrayCount ( t.storedentityelement ) > 0 )
									{
										t.storedentityelement [ t.e ].y = t.storedentityelement [ t.e ].y + t.tadjy_f;
									}
								}
								else
								{
									PositionObject ( t.obj, t.entityelement [ t.e ].x, t.entityelement [ t.e ].y, t.entityelement [ t.e ].z );
								}
							}
						}
					}
					t.entityelement [ t.e ].floorposy = -90000.0;
				}
			}
			if ( t.trevealallinstancestampentities == 1 )
			{
				// we have edited the entities in the game itself, so remove
				for ( t.e = 1; t.e <= g.entityelementlist; t.e++ )
				{
					t.entid = t.entityelement [ t.e ].bankindex;
					t.obj = t.entityelement [ t.e ].obj;
					if ( t.obj > 0 )
					{
						if ( ObjectExist ( t.obj ) == 1 )
						{
							if ( t.entityprofile [ t.entid ].ismarker == 0 )
							{
								ShowObject ( t.obj );
							}
						}
					}
				}
			}
		}
	}
}

void terrain_editcontrol ( void )
{
	//PE: Update status from input.
	terrain_UpdateInputAndEntities();
}

void terrain_recordbuffer ( void ) {}
void terrain_undo ( void ) {}
void terrain_redo ( void ) {}
void terrain_editcontrol_auxiliary ( void ) {}
void terrain_paintterrain ( void ) {}
void terrain_cursor ( void ) {}
void terrain_cursor_nograsscolor ( void ) {}
void terrain_cursor_off ( void ) {}
void terrain_renderonly ( void ) {}

// GGMAX 2.68g: the wicked bridge polls this every frame so Completely Empty Level mode is
// SELF-ENFORCING — the old wiring relied on Wicked_Update_Visuals being called after the
// level's visuals were parsed, which the fpm load path does not guarantee (Lee's ssss9
// repro: flag loaded true — collision gone — but the terrain grid still rendered).
bool GGGame_IsEmptyLevelMode ( void )
{
	return t.visuals.bEnableEmptyLevelMode;
}

// the level's own texture for a terrain slot (visuals.TerrainTextures, a palette folder's Color.dds), as a full path in
// the writable folder, or an empty string when the slot has none or the file is not there; the Wicked terrain's materials
// take it in place of the matN folder (GGTerrainWicked.cpp), as GameGuru's own terrain always has
const char* GGGame_TerrainSlotTexture ( int iSlot )
{
	static char szPath[MAX_PATH];
	szPath[0] = 0;
	if ( iSlot < 0 || iSlot >= 32 ) return szPath;
	const char* pSlot = t.visuals.sTerrainTextures[iSlot].Get();
	if ( !pSlot || !pSlot[0] ) return szPath;
	if ( strstr ( pSlot, "terraintextures\\mat" ) == pSlot || strstr ( pSlot, "terraintextures/mat" ) == pSlot ) return szPath;
	strcpy_s ( szPath, MAX_PATH, pSlot );
	GG_GetRealPath ( szPath, 0 );
	if ( !FileExist ( szPath ) ) szPath[0] = 0;
	return szPath;
}

float BT_GetGroundHeight ( unsigned long value, float x, float z )
{
	extern int g_iDisableTerrainSystem;
	if (g_iDisableTerrainSystem == 0 && t.visuals.bEnableEmptyLevelMode==false)
	{
		float height;
		if (GGTerrain_GetHeight(x, z, &height)) return height;
		else return GGORIGIN_Y;
	}
	else
	{
		if (t.visuals.bEnableEmptyLevelMode)
		{
			extern float fEmptyLevelFloorY;
			return fEmptyLevelFloorY;
		}
		return GGORIGIN_Y;
	}
}

float BT_GetGroundHeight ( unsigned long value, float x, float z, bool dsadsadsa )
{
	extern int g_iDisableTerrainSystem;
	if (g_iDisableTerrainSystem == 0 && t.visuals.bEnableEmptyLevelMode == false)
	{

		float height;
		if (GGTerrain_GetHeight(x, z, &height)) return height;
		else return GGORIGIN_Y;
	}
	else
	{
		if (t.visuals.bEnableEmptyLevelMode)
		{
			extern float fEmptyLevelFloorY;
			return fEmptyLevelFloorY;
		}
		return GGORIGIN_Y;
	}
}

void terrain_clearterraindirtyregion ( void ) {}
void terrain_cleargrassdirtyregion ( void ) {}
void terrain_cleardirtyregion ( void ) {}
void terrain_waterineditor ( void ) {}
void terrain_assignnewshader ( void ) {}
void terrain_applyshader ( void ) {}
void terrain_make ( void ) {}
void terrain_make_image_only(void) {}

void terrain_load ( char* pLevelBankLocation )
{
	int k = 0;
}

void terrain_save ( char* pLevelBankLocation )
{
}

void terrain_savetextures ( void ) {}
void terrain_generatevegandmask_grab ( void ) {}
void terrain_generatevegandmaskfromterrain ( void ) {}
void terrain_generateblanktextures ( void ) {}
void terrain_loaddata ( void ) {}
void terrain_delete ( void )
{
	int k = 0;
}
void terrain_updaterealheights ( void ) {}
void terrain_randomiseorflattenterrain ( void ) {}
void terrain_flattenterrain ( void ) {}
void terrain_randomiseterrain ( void ) {}
void terrain_refreshterrainmatrix ( void ) 
{
	int k = 0;
}
void terrain_skipifnowaterexposed ( void ) {}
void terrain_updatewatermask ( void ) {}
void terrain_updatewatermask_new ( void ) {}
void terrain_whitewashwatermask ( void ) {}
void terrain_createheightmapfromheightdata ( void ) {}
void terrain_quickupdateheightmapfromheightdata ( void ) 
{
	int k = 0;
}
void terrain_generatetextureselect ( void ) {}
void terrain_generateshadows ( void ) {}
void terrain_start_play ( void ) 
{
}

void terrain_stop_play ( void ) 
{
}

void terrain_setfog ( void ) {}
void terrain_water_init ( void ) {}
void terrain_water_free ( void ) {}
void terrain_updatewatermechanism ( void ) {}
void terrain_updatewaterphysics ( void ) {}
void terrain_water_setfog ( void ) {}


//Variables to use else where.
float fTerrainHeightStart = 0.0f; //Meters
bool bShowEditArea = true;
bool bShow3DBoundary = true;
bool bShowMiniMap = true;
float fWaterDepthMeters = -200.0;

int iTriggerInvalidateAfterFrames = 0;
void check_new_terrain_parameters(void)
{
	iTriggerInvalidateAfterFrames = 22; //PE: Also Need to be delayed so new terrain data have been updated.
	// immediate call = legacy only (new heights not read back yet — a Wicked regen here
	// would bake the OLD heights); the delayed trigger above does the real Wicked rebuild
	GGTerrain::GGTerrain_InvalidateRegion(-1000000.0, -1000000.0, 1000000.0, 1000000.0, GGTERRAIN_INVALIDATE_ALL | GGTERRAIN_INVALIDATE_NO_WICKED);

}

void procedural_set_heightmap_level(void)
{
	ggterrain_global_params.seed = Random2();
	ggterrain_global_params.offset_y = 0.0; GGTerrain_MetersToUnits(5.8f); //228.3
	ggterrain_global_params.offset_x = 0.0;
	ggterrain_global_params.height = 5000.0; //GGTerrain_MetersToUnits(131.5f); //5,177.155
	ggterrain_global_params.minHeight = 5000.0; // GGTerrain_MetersToUnits(131.5f); //5,177.155
	ggterrain_global_params.noise_power = 1.6f;
	ggterrain_global_params.noise_fallof_power = 0.27f;
	ggterrain_global_params.fractal_levels = 6;
	ggterrain_global_params.fractal_initial_freq = 0.3f;
	ggterrain_global_params.fractal_freq_increase = 2.5f;
	ggterrain_global_params.fractal_freq_weight = 0.4;
	ggterrain_global_render_params.baseLayerMaterial = 0x100 | 25;
	ggterrain_global_render_params.layerMatIndex[0] = 0x100 | 24;
	ggterrain_global_render_params.layerMatIndex[1] = 0x100 | 28;
	ggterrain_global_render_params.layerMatIndex[2] = 0x100 | 29;
	ggterrain_global_render_params.layerStartHeight[0] = GGTerrain_MetersToUnits(0.0f);
	ggterrain_global_render_params.layerStartHeight[1] = GGTerrain_MetersToUnits(4.572f);
	ggterrain_global_render_params.layerStartHeight[2] = GGTerrain_MetersToUnits(145.7f);
	ggterrain_global_render_params.layerStartHeight[3] = GGTerrain_MetersToUnits(1500.0f);
	ggterrain_global_render_params.layerStartHeight[4] = GGTerrain_MetersToUnits(1500.0f);
	ggterrain_global_render_params.layerEndHeight[0] = GGTerrain_MetersToUnits(1.524f);
	ggterrain_global_render_params.layerEndHeight[1] = GGTerrain_MetersToUnits(9.144f);
	ggterrain_global_render_params.layerEndHeight[2] = GGTerrain_MetersToUnits(205.7f);
	ggterrain_global_render_params.layerEndHeight[3] = GGTerrain_MetersToUnits(1500.0f);
	ggterrain_global_render_params.layerEndHeight[4] = GGTerrain_MetersToUnits(1500.0f);
	ggterrain_global_render_params.slopeMatIndex[0] = 0x100 | 26;
	ggterrain_global_render_params.slopeMatIndex[1] = 0x100 | 4;
	ggterrain_global_render_params.slopeStart[0] = 0.1f;
	ggterrain_global_render_params.slopeStart[1] = 1.0f;
	ggterrain_global_render_params.slopeEnd[0] = 0.3f;
	ggterrain_global_render_params.slopeEnd[1] = 1.0f;

	ggterrain_global_params.fractal_flags = (ggterrain_global_params.fractal_flags & ~GGTERRAIN_FRACTAL_RIDGES0) | GGTERRAIN_FRACTAL_VALLEYS0;
	ggterrain_global_params.fractal_flags = ggterrain_global_params.fractal_flags & ~(GGTERRAIN_FRACTAL_VALLEYS1 | GGTERRAIN_FRACTAL_RIDGES1);
	ggterrain_global_params.fractal_flags = ggterrain_global_params.fractal_flags & ~(GGTERRAIN_FRACTAL_VALLEYS2 | GGTERRAIN_FRACTAL_RIDGES2);
	ggterrain_global_params.fractal_flags = ggterrain_global_params.fractal_flags & ~(GGTERRAIN_FRACTAL_VALLEYS3 | GGTERRAIN_FRACTAL_RIDGES3);

	t.visuals.bWaterEnable = true;
	t.gamevisuals.bWaterEnable = t.visuals.bWaterEnable;

	Wicked_Update_Visuals((void *)&t.visuals);

	//Create Level
	ggterrain_extra_params.iProceduralTerrainType = 1;
}


void procedural_set_empty_level(bool bWaterReset)
{
	GGTerrain_RemoveHeightMap();
	ggterrain_extra_params.iProceduralTerrainType = 0;
	ggterrain_global_params.offset_y = GGTerrain_MetersToUnits(0.0f);
	ggterrain_global_params.height = GGTerrain_MetersToUnits(500); // now made flat with fractal_initial_amplitude
	ggterrain_global_params.minHeight = GGTerrain_MetersToUnits(500);
	ggterrain_global_params.noise_power = 1.6f;
	ggterrain_global_params.noise_fallof_power = 0.27f;
	ggterrain_global_params.fractal_levels = 1;
	ggterrain_global_params.fractal_initial_freq = 0.3f;
	ggterrain_global_params.fractal_initial_amplitude = 0.0f;
	ggterrain_global_params.fractal_freq_increase = 2.7f;
	ggterrain_global_params.fractal_freq_weight = 0.4;
	ggterrain_global_render_params.baseLayerMaterial = 0x100 | (32 - 1);
	ggterrain_global_render_params.layerMatIndex[0] = 0x100 | (32 - 1);
	ggterrain_global_render_params.layerMatIndex[1] = 0x100 | (32 - 1);
	ggterrain_global_render_params.layerMatIndex[2] = 0x100 | (32 - 1);
	ggterrain_global_render_params.layerStartHeight[0] = GGTerrain_MetersToUnits(0.0f);
	ggterrain_global_render_params.layerStartHeight[1] = GGTerrain_MetersToUnits(1500.0f);
	ggterrain_global_render_params.layerStartHeight[2] = GGTerrain_MetersToUnits(1500.0f);
	ggterrain_global_render_params.layerStartHeight[3] = GGTerrain_MetersToUnits(1500.0f);
	ggterrain_global_render_params.layerStartHeight[4] = GGTerrain_MetersToUnits(1500.0f);
	ggterrain_global_render_params.layerEndHeight[0] = GGTerrain_MetersToUnits(1.0f);
	ggterrain_global_render_params.layerEndHeight[1] = GGTerrain_MetersToUnits(1500.0f);
	ggterrain_global_render_params.layerEndHeight[2] = GGTerrain_MetersToUnits(1500.0f);
	ggterrain_global_render_params.layerEndHeight[3] = GGTerrain_MetersToUnits(1500.0f);
	ggterrain_global_render_params.layerEndHeight[4] = GGTerrain_MetersToUnits(1500.0f);
	ggterrain_global_render_params.slopeMatIndex[0] = 0x100 | (32 - 1);
	ggterrain_global_render_params.slopeMatIndex[1] = 0x100 | (32 - 1);
	ggterrain_global_render_params.slopeStart[0] = 0.2f;
	ggterrain_global_render_params.slopeStart[1] = 1.0f;
	ggterrain_global_render_params.slopeEnd[0] = 0.4f;
	ggterrain_global_render_params.slopeEnd[1] = 1.0f;

	if (bWaterReset)
	{
		// remove water from grey grid view
		t.visuals.bWaterEnable = false;
		t.terrain.waterliney_f = g.gdefaultwaterheight = -500.0f;
		t.gamevisuals.bWaterEnable = t.visuals.bWaterEnable;
		Wicked_Update_Visuals((void *)&t.visuals);
		ggterrain_extra_params.iUpdateTrees = 10;
	}
}

void wicked_set_water_level(int water_height)
{
	g.gdefaultwaterheight = water_height;
	t.terrain.waterliney_f = g.gdefaultwaterheight;

	if (ggterrain_extra_params.iProceduralTerrainType == 0)
	{
		t.visuals.bWaterEnable = false;
		t.gamevisuals.bWaterEnable = t.visuals.bWaterEnable;
	}
	else
	{
		t.visuals.bWaterEnable = true;
		t.gamevisuals.bWaterEnable = t.visuals.bWaterEnable;
	}
	Wicked_Update_Visuals((void *)&t.visuals);
	ggterrain_extra_params.iUpdateTrees = 1;
}

