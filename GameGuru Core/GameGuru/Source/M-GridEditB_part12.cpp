entityeleproftype *lua_grideleprof;
entityeleproftype lua_readonly_grideleprof; //Temp for readonly dynamic lua parsing.

void DisplayFPEBehavior(bool readonly, int entid, entityeleproftype* edit_grideleprof, int elementID, bool bHideIcon)
{
	//FPE Properties.
	bool bIsLightProbe = false;
	bool bDisplaySmallIcon = false;
	bool bDisplayName = false;
	static int fpe_current_selected_script = 0;
	static bool fpe_current_loaded_script_has_dlua = false;
	int media_icon_size = 64;
	LPSTR pAIRoot = "scriptbank\\";

	// flag if a probe
	if (entid > 0 && t.entityprofile[entid].ismarker == 2 && edit_grideleprof->light.fLightHasProbe >= 50.0f)
	{
		bIsLightProbe = true;
	}

	if (readonly)
	{
		ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
	}

	if (readonly)
		lua_grideleprof = &lua_readonly_grideleprof;
	else {
		if (!edit_grideleprof)
			lua_grideleprof = &t.grideleprof;
		else
			lua_grideleprof = edit_grideleprof;
	}

	if (!edit_grideleprof)
	{
		edit_grideleprof = &t.grideleprof;
	}

	if (t.tflagai == 1)
	{
		if (g.quickparentalcontrolmode == 2)
		{
			if (t.entityprofile[entid].ismarker == 0)
			{
				if (t.tflagchar == 1)
					pAIRoot = "scriptbank\\people\\";
				else
					pAIRoot = "scriptbank\\objects\\";
			}
			else
			{
				pAIRoot = "scriptbank\\markers\\";
			}
		}
	}

	fPropertiesColoumWidth = ImGui::GetCursorPosX() + 90.0f;

	#ifdef USENEWMEDIASELECTWINDOWS
	char cDisplayName[MAX_PATH];
	strcpy(cDisplayName, edit_grideleprof->aimain_s.Get());
	char * find = (char *)pestrcasestr(cDisplayName, "\\");
	while (find)
	{
		find++;
		strcpy(cDisplayName, find);
		find = (char *)pestrcasestr(cDisplayName, "\\");
	}
	// special format for LUA titles
	FormatLUAFilenameToTitle(cDisplayName);
	#endif

	if (t.entityprofile[entid].ischaracter > 0)
	{
		//"Character Properties"
		if (bDisplaySmallIcon)
		{
			//Display icon.
			if (t.entityprofile[entid].iThumbnailSmall > 0) {
				float w = ImGui::GetContentRegionAvailWidth();
				ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2((w*0.5) - (media_icon_size*0.5), 0.0f));
				ImGui::ImgBtn(t.entityprofile[entid].iThumbnailSmall, ImVec2(media_icon_size, media_icon_size), drawCol_back, drawCol_normal, drawCol_normal, drawCol_normal, -1, 0, 0, 0, true);
			}
		}
		//Display name.
		if(bDisplayName)
			edit_grideleprof->name_s = imgui_setpropertystring2_v2(t.group, edit_grideleprof->name_s.Get(), "Name", t.strarr_s[204].Get(), readonly);


		int speech_entries = 0;
		bool bUpdateMainString = false;

		for (int speech_loop = 0; speech_loop < 5; speech_loop++)
			speech_ids[speech_loop] = -1;
		
		ImGui::PushItemWidth(-10);

		// scan PEOPLE folder for complete list of script
		std::vector<cstr> scriptList_s; scriptList_s.clear();
		std::vector<cstr> scriptListTitle_s; scriptListTitle_s.clear();
		cstr oldDir_s = GetDir();
		SetDir(g.fpscrootdir_s.Get());
		SetDir("Files\\scriptbank\\people");
		ChecklistForFiles();
		for (int f = 1; f <= ChecklistQuantity(); f++)
		{
			cstr tfile_s = ChecklistString(f);
			LPSTR pFilename = tfile_s.Get();
			if (tfile_s != "." && tfile_s != "..")
			{
				if (strnicmp(pFilename + strlen(pFilename) - 4, ".lua", 4) == NULL)
				{
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

					// add script and title to list
					scriptList_s.push_back(cstr("people\\") + tfile_s);
					scriptListTitle_s.push_back(cstr(pTitleName));
				}
			}
		}
		scriptList_s.push_back(cstr(""));
		scriptListTitle_s.push_back(cstr("Custom"));
		SetDir(oldDir_s.Get());

		// and create items list
		static int g_scriptpeople_item_count = 0;
		static char** g_scriptpeople_items = NULL;
		if (g_scriptpeople_item_count != scriptList_s.size())
		{
			if (g_scriptpeople_items)
			{
				for (int i = 0; i < g_scriptpeople_item_count; i++) SAFE_DELETE(g_scriptpeople_items[i]);
				SAFE_DELETE(g_scriptpeople_items);
			}
			g_scriptpeople_item_count = scriptList_s.size();
			g_scriptpeople_items = new char*[g_scriptpeople_item_count];
			for (int i = 0; i < g_scriptpeople_item_count; i++)
			{
				g_scriptpeople_items[i] = new char[256];
				strcpy(g_scriptpeople_items[i], scriptListTitle_s[i].Get());
			}
		}

		int item_current_type_selection = g_scriptpeople_item_count - 1; //Default Custom.
		for (int i = 0; i < g_scriptpeople_item_count - 1; i++)
		{
			// with workshop items updating core scripts, need to check entire path now!
			if (stricmp (edit_grideleprof->aimain_s.Get(), scriptList_s[i].Get()) == NULL)
			{
				item_current_type_selection = i;
				break;
			}
		}

		if (fpe_current_loaded_script != item_current_type_selection)
		{
			if (PreviewWPERoot != 0)
			{
				//PE: Delete effects.
				WickedCall_PerformEmitterAction(6, PreviewWPERoot);
				void DeleteEmitterEffects(uint32_t root);
				DeleteEmitterEffects(PreviewWPERoot);
				PreviewWPERoot = 0;
				bPreviewWPE = false;
			}

			//Load in lua and check for custom properties.
			cstr script_name_append = "";
			if (item_current_type_selection < g_scriptpeople_item_count - 1)
				script_name_append += (char *)scriptList_s[item_current_type_selection].Get();
			else
				script_name_append += edit_grideleprof->aimain_s;

			cstr script_name = "";
			script_name = "scriptbank\\";
			script_name += script_name_append;

			#ifdef USENEWMEDIASELECTWINDOWS
			fpe_current_loaded_script_image = PROPERTIES_CACHE_ICONS + fpe_current_loaded_script_image_count;
			if (fpe_current_loaded_script_image_count++ > 10) fpe_current_loaded_script_image_count = 0; //Make sure we dont have a old image already displayed inside imgui.
			std::string sImgName = Left(script_name.Get(), Len(script_name.Get()) - 4);
			if (pref.current_style == 25 || pref.current_style == 3)
				sImgName += ".png";
			else
				sImgName += "2.png";
			image_setlegacyimageloading(true);
			LoadImage((char *)sImgName.c_str(), fpe_current_loaded_script_image);
			image_setlegacyimageloading(false);
			if (!GetImageExistEx(fpe_current_loaded_script_image))
			{
				if (!(pref.current_style == 25 || pref.current_style == 3))
				{
					sImgName = Left(script_name.Get(), Len(script_name.Get()) - 4);
					sImgName += ".png";
					image_setlegacyimageloading(true);
					LoadImage((char *)sImgName.c_str(), fpe_current_loaded_script_image);
					image_setlegacyimageloading(false);
					if (!GetImageExistEx(fpe_current_loaded_script_image))
					{
						fpe_current_loaded_script_image = FILETYPE_SCRIPT;
					}
				}
				else
					fpe_current_loaded_script_image = FILETYPE_SCRIPT;
			}
			#endif

			//Try to parse script.
			int iObjID = t.entityelement[elementID].obj;
			if (iObjID == 0 && t.gridentityobj>0) iObjID = t.gridentityobj;
			ParseLuaScriptWithElementID(lua_grideleprof, script_name.Get(), iObjID);
			fpe_current_loaded_script = item_current_type_selection;

			if (lua_grideleprof->PropertiesVariableActive == 1)
			{
				bUpdateMainString = true;
				fpe_current_loaded_script_has_dlua = true;
			}
			else
			{
				if (fpe_current_loaded_script_has_dlua)
				{
					//Reset t.grideleprof.soundset4_s that contain the dlua calls.
					lua_grideleprof->soundset4_s = "";
					fpe_current_loaded_script_has_dlua = false;
				}
			}
		}

		#ifdef USENEWMEDIASELECTWINDOWS
		float w = ImGui::GetContentRegionAvailWidth() - 16.0f;
		float ImgW = ImageWidth(fpe_current_loaded_script_image);
		float ImgH = ImageHeight(fpe_current_loaded_script_image);
		float fRatio = w/ImgW;
		if (!bHideIcon)
		{
			ImGuiWindow* window = ImGui::GetCurrentWindow();
			if (iSelectedLibraryStingReturnID == window->GetID("ScriptSelector##+"))
			{
				//Update Script.
				if (sSelectedLibrarySting != "")
				{
					if (stricmp(edit_grideleprof->aimain_s.Get(), sSelectedLibrarySting.Get()) != NULL)
					{
						// changed behavior of object, ensure any behavior specific properties are cleared (as they cannot be set if new behavior does not expose them)
						edit_grideleprof->overrideanimset_s = "";
						edit_grideleprof->hasweapon_s = t.entityprofile[entid].hasweapon_s;
						edit_grideleprof->hasweapon = 0;
						extern bool g_bNowPopulateWithCorrectAnimSet;
						g_bNowPopulateWithCorrectAnimSet = true;
					}
					edit_grideleprof->aimain_s = sSelectedLibrarySting;
					sSelectedLibrarySting = "";
					iSelectedLibraryStingReturnID = -1; //disable.
					fpe_current_loaded_script = -1; //Reload image and DLUA.
				}
			}
			if (ImGui::ImgBtn(fpe_current_loaded_script_image, ImVec2(ImgW * fRatio, ImgH * fRatio), drawCol_black, drawCol_normal, drawCol_normal, drawCol_normal, -1, 0, 0, 0, false))
			{
				//Select script.
				sStartLibrarySearchString = "People";
				iLastDisplayLibraryType = -1;
				bExternal_Entities_Window = true;
				iDisplayLibraryType = 4;
				iLibraryStingReturnToID = window->GetID("ScriptSelector##+");
				if (edit_grideleprof->aimain_s.Len() > 0)
					sMakeDefaultSelecting = edit_grideleprof->aimain_s;

			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Select Character Behavior");
		}
		ImGui::TextCenter(cDisplayName);

		#else
		if (ImGui::Combo("##BehavioursSimpleInput", &item_current_type_selection, g_scriptpeople_items, g_scriptpeople_item_count, 20))
		{
			if (item_current_type_selection < g_scriptpeople_item_count - 1)
			{
				edit_grideleprof->aimain_s = scriptList_s[item_current_type_selection].Get();
			}
			else
			{
				edit_grideleprof->aimain_s = "";
			}
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Select Character Behavior");
		#endif

		ImGui::PopItemWidth();

		#ifndef USENEWMEDIASELECTWINDOWS
		if (item_current_type_selection == g_scriptpeople_item_count - 1)
		{
			//Custom script , display directly.
			std::string ms = t.strarr_s[417].Get();
			ms = "Script";
			cstr aim = edit_grideleprof->aimain_s;
			edit_grideleprof->aimain_s = imgui_setpropertyfile2_v2(t.group, edit_grideleprof->aimain_s.Get(), (char *)ms.c_str(), t.strarr_s[207].Get(), pAIRoot,readonly);
			if (aim != edit_grideleprof->aimain_s)
				fpe_current_loaded_script = -1;
		}
		#endif

		if (lua_grideleprof->PropertiesVariableActive == 1) 
		{
			speech_entries = DisplayLuaDescription(lua_grideleprof);
		}
		else 
		{
			if (lua_grideleprof->PropertiesVariable.VariableDescription.Len() > 0) 
			{
				if(edit_grideleprof->aimain_s != "default.lua") //No need to display.
					DisplayLuaDescriptionOnly(lua_grideleprof);
			}
		}

		bool bUseSoundVariants = lua_grideleprof->iUseSoundVariants;
		if (ImGui::Checkbox("Use Sound Variants", &bUseSoundVariants))
		{
			lua_grideleprof->iUseSoundVariants = bUseSoundVariants;
		}
		

		if (speech_entries > 0)
		{
			//@Lee all SPEECH control is moved to this function.
			//PE: need this to point to lua_grideleprof.
			SpeechControls(speech_entries, bUpdateMainString, edit_grideleprof);
		}

		// removed from MAX, we want all pertinant values through the DLUA system!
	}
	else if (t.tflaglight == 1)
	{
		bool bLightChanged = false;
		bool bLightTypeChanged = false;
		bool bNoLightRotate = false;
		bool bUpdateMainString = false;
		int speech_entries = 0;


		//PE: Add dynamic lua to the light.
		static cstr current_loaded_script = "";
		if (fpe_current_loaded_script == -1 || current_loaded_script != edit_grideleprof->aimain_s)
		{
			//Load in lua and check for custom properties.
			cstr script_name = "";
			//if (strnicmp(edit_grideleprof->aimain_s.Get(), "projectbank", 11) != NULL) 
			script_name = "scriptbank\\";
			script_name += edit_grideleprof->aimain_s;

			fpe_current_loaded_script = 9999;
			current_loaded_script = edit_grideleprof->aimain_s;

			#ifdef USENEWMEDIASELECTWINDOWS
			fpe_current_loaded_script_image = PROPERTIES_CACHE_ICONS + fpe_current_loaded_script_image_count;
			if (fpe_current_loaded_script_image_count++ > 10) fpe_current_loaded_script_image_count = 0; //Make sure we dont have a old image already displayed inside imgui.
			std::string sImgName = Left(script_name.Get(), Len(script_name.Get()) - 4);
			if (pref.current_style == 25 || pref.current_style == 3)
				sImgName += ".png";
			else
				sImgName += "2.png";
			image_setlegacyimageloading(true);
			LoadImage((char *)sImgName.c_str(), fpe_current_loaded_script_image);
			image_setlegacyimageloading(false);
			if (!GetImageExistEx(fpe_current_loaded_script_image))
			{
				if (!(pref.current_style == 25 || pref.current_style == 3))
				{
					sImgName = Left(script_name.Get(), Len(script_name.Get()) - 4);
					sImgName += ".png";
					image_setlegacyimageloading(true);
					LoadImage((char *)sImgName.c_str(), fpe_current_loaded_script_image);
					image_setlegacyimageloading(false);
					if (!GetImageExistEx(fpe_current_loaded_script_image))
					{
						fpe_current_loaded_script_image = FILETYPE_SCRIPT;
					}
				}
				else
					fpe_current_loaded_script_image = FILETYPE_SCRIPT;
			}
			#endif

			//Try to parse script.

			ParseLuaScript(lua_grideleprof, script_name.Get());

			if (lua_grideleprof->PropertiesVariableActive == 1)
			{
				bUpdateMainString = true;
				fpe_current_loaded_script_has_dlua = true;
			}
			else
			{
				if (fpe_current_loaded_script_has_dlua)
				{
					//Reset t.grideleprof.soundset4_s that contain the dlua calls.
					lua_grideleprof->soundset4_s = "";
					fpe_current_loaded_script_has_dlua = false;
				}
			}
		}
		//END DLUA.


		//Display icon.
		if (bDisplaySmallIcon)
		{
			if (t.entityprofile[entid].iThumbnailSmall > 0) 
			{
				float w = ImGui::GetContentRegionAvailWidth();
				ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2((w*0.5) - (media_icon_size*0.5), 0.0f));
				ImGui::ImgBtn(t.entityprofile[entid].iThumbnailSmall, ImVec2(media_icon_size, media_icon_size), drawCol_back, drawCol_normal, drawCol_normal, drawCol_normal, -1, 0, 0, 0, true);
			}
		}

		//#########################################
		//#### Display predefined light setup. ####
		//#########################################
		if (bIsLightProbe == true)
		{
			// GGMAX 2.90: the "Probe Range" slider is GONE. fLightHasProbe was never a range:
			// GGTerrain writes it to probe->range, but Scene::RunProbeUpdateSystem recomputes
			// probe.range from the transform scale every Scene::Update, so the value was
			// overwritten before anything read it (DX11's engine had the identical line — the
			// slider was inert there too). The probe volume comes solely from Size X/Y/Z below.
			// The value survives as what its name always said: the "has a probe" FLAG, tested
			// as >= 50 in a dozen places. See PROBE_PROPERTIES_2026-08-18.md.
			ImGui::TextCenter("Probe Size X");
			ImGui::PushItemWidth(-10);
			int iLightProbeRange = (int)edit_grideleprof->light.fLightHasProbeX;
			if (ImGui::MaxSliderInputInt("##fLightProbeScaleXSimpleInput", &iLightProbeRange, 50, 500, "Specify the X dimension of the environment probe"))
			{
				edit_grideleprof->light.fLightHasProbeX = iLightProbeRange;
				g_bLightProbeScaleChanged = true;
				bLightChanged = true;
			}
			ImGui::PopItemWidth();

			ImGui::TextCenter("Probe Size Y");
			ImGui::PushItemWidth(-10);
			iLightProbeRange = (int)edit_grideleprof->light.fLightHasProbeY;
			if (ImGui::MaxSliderInputInt("##fLightProbeScaleYSimpleInput", &iLightProbeRange, 50, 500, "Specify the Y dimension of the environment probe"))
			{
				edit_grideleprof->light.fLightHasProbeY = iLightProbeRange;
				g_bLightProbeScaleChanged = true;
				bLightChanged = true;
			}
			ImGui::PopItemWidth();

			ImGui::TextCenter("Probe Size Z");
			ImGui::PushItemWidth(-10);
			iLightProbeRange = (int)edit_grideleprof->light.fLightHasProbeZ;
			if (ImGui::MaxSliderInputInt("##fLightProbeScaleZSimpleInput", &iLightProbeRange, 50, 500, "Specify the Z dimension of the environment probe"))
			{
				edit_grideleprof->light.fLightHasProbeZ = iLightProbeRange;
				g_bLightProbeScaleChanged = true;
				bLightChanged = true;
			}
			ImGui::PopItemWidth();

			//PE: min 0.001 , 0 is used to reset to 1.0f.
			ImGui::TextCenter("Probe Brightness");
			ImGui::PushItemWidth(-10);
			if (ImGui::MaxSliderInputFloat("##fLightfProbeBrightness", &edit_grideleprof->light.fProbeBrightness, 0.01f, 10.0f, "Specify the brightness of the environment probe"))
			{
				g_bLightProbeScaleChanged = true;
				bLightChanged = true;
			}
			ImGui::PopItemWidth();
			// and done
			return;
		}

		//Palette.
		#define MAXPREDEFINEDSETUPS 32
		#define LIGHTTYPEPOINT 0
		#define LIGHTTYPESPOT 1
		static bool PredefinedLightInit = false;
		static int iPredefinedLights = 16;
		int iPredefinedSetups = 16;
		static ImVec4 vPredefined_Light_Palette[MAXPREDEFINEDSETUPS];
		static int iPredefined_Light_Type[MAXPREDEFINEDSETUPS];
		static int iPredefined_Light_Range[MAXPREDEFINEDSETUPS];
		static float fPredefined_Light_ProbeScale[MAXPREDEFINEDSETUPS];

		static int iPredefined_Light_Radius[MAXPREDEFINEDSETUPS];
		static float fPredefined_Light_AngX[MAXPREDEFINEDSETUPS];
		static float fPredefined_Light_AngY[MAXPREDEFINEDSETUPS];
		static float fPredefined_Light_AngZ[MAXPREDEFINEDSETUPS];

		static int current_light_selected = -1;
		static bool bSetUpDefaultPAletteWithDefaultColors = true;
		static bool bFirstTimeInFindBestChoice = false;
		static int iLastEntityElementIDHere = -1;
		if (elementID != iLastEntityElementIDHere)
		{
			iLastEntityElementIDHere = elementID;
			bFirstTimeInFindBestChoice = true;
		}

		float fOne = 1.0f / 255.0f;
		if (!PredefinedLightInit)
		{
			// Mix POINT , SPOT with same palette
			if (bSetUpDefaultPAletteWithDefaultColors == true)
			{
				for (int i = 0; i < 16;i++)
				{
					fPredefined_Light_ProbeScale[i] = 2.0f; //PE: Was 1
					iPredefined_Light_Radius[i] = 45;
					fPredefined_Light_AngX[i] = 90;
					fPredefined_Light_AngY[i] = 0;
					fPredefined_Light_AngZ[i] = 0;
				}
				bSetUpDefaultPAletteWithDefaultColors = false;
				vPredefined_Light_Palette[0] = ImVec4(255 * fOne, 255 * fOne, 255 * fOne, 1.0f);
				iPredefined_Light_Type[0] = LIGHTTYPEPOINT;
				iPredefined_Light_Range[0] = 300.0f / 3.0;

				vPredefined_Light_Palette[1] = ImVec4(255 * fOne, 255 * fOne, 255 * fOne, 1.0f);
				iPredefined_Light_Type[1] = LIGHTTYPESPOT;
				iPredefined_Light_Range[1] = 600.0f / 3.0;
				fPredefined_Light_AngX[1] = 45;

				vPredefined_Light_Palette[2] = ImVec4(110.0f * fOne, 110.0f * fOne, 110.0f * fOne, 1.0f);
				iPredefined_Light_Type[2] = LIGHTTYPEPOINT;
				iPredefined_Light_Range[2] = 300.0f / 3.0;

				vPredefined_Light_Palette[3] = ImVec4(110.0f * fOne, 110.0f * fOne, 110.0f * fOne, 1.0f);
				iPredefined_Light_Type[3] = LIGHTTYPESPOT;
				iPredefined_Light_Range[3] = 600.0f / 3.0;
				fPredefined_Light_AngX[3] = 45;

				vPredefined_Light_Palette[4] = ImVec4(78 * fOne, 144 * fOne, 236 * fOne, 1.0f);
				iPredefined_Light_Type[4] = LIGHTTYPEPOINT;
				iPredefined_Light_Range[4] = 300.0f / 3.0;

				vPredefined_Light_Palette[5] = ImVec4(78 * fOne, 144 * fOne, 236 * fOne, 1.0f);
				iPredefined_Light_Type[5] = LIGHTTYPESPOT;
				iPredefined_Light_Range[5] = 600.0f / 3.0;
				fPredefined_Light_AngX[5] = 45;

				vPredefined_Light_Palette[6] = ImVec4(192 * fOne, 73 * fOne, 223 * fOne, 1.0f);
				iPredefined_Light_Type[6] = LIGHTTYPEPOINT;
				iPredefined_Light_Range[6] = 300.0f / 3.0;

				vPredefined_Light_Palette[7] = ImVec4(192 * fOne, 73 * fOne, 223 * fOne, 1.0f);
				iPredefined_Light_Type[7] = LIGHTTYPESPOT;
				iPredefined_Light_Range[7] = 600.0f / 3.0;
				fPredefined_Light_AngX[7] = 45;

				vPredefined_Light_Palette[8] = ImVec4(224 * fOne, 50 * fOne, 42 * fOne, 1.0f);
				iPredefined_Light_Type[8] = LIGHTTYPEPOINT;
				iPredefined_Light_Range[8] = 300.0f / 3.0;

				vPredefined_Light_Palette[9] = ImVec4(224 * fOne, 50 * fOne, 42 * fOne, 1.0f);
				iPredefined_Light_Type[9] = LIGHTTYPESPOT;
				iPredefined_Light_Range[9] = 600.0f / 3.0;
				fPredefined_Light_AngX[9] = 45;

				vPredefined_Light_Palette[10] = ImVec4(245 * fOne, 234 * fOne, 65 * fOne, 1.0f);
				iPredefined_Light_Type[10] = LIGHTTYPEPOINT;
				iPredefined_Light_Range[10] = 300.0f / 3.0;

				vPredefined_Light_Palette[11] = ImVec4(245 * fOne, 234 * fOne, 65 * fOne, 1.0f);
				iPredefined_Light_Type[11] = LIGHTTYPESPOT;
				iPredefined_Light_Range[11] = 600.0f / 3.0;
				fPredefined_Light_AngX[11] = 45;

				vPredefined_Light_Palette[12] = ImVec4(0 * fOne, 207 * fOne, 99 * fOne, 1.0f);
				iPredefined_Light_Type[12] = LIGHTTYPEPOINT;
				iPredefined_Light_Range[12] = 300.0f / 3.0;

				vPredefined_Light_Palette[13] = ImVec4(0 * fOne, 207 * fOne, 99 * fOne, 1.0f);
				iPredefined_Light_Type[13] = LIGHTTYPESPOT;
				iPredefined_Light_Range[13] = 600.0f / 3.0;
				fPredefined_Light_AngX[13] = 45;

				vPredefined_Light_Palette[14] = ImVec4(250 * fOne, 168 * fOne, 50 * fOne, 1.0f);
				iPredefined_Light_Type[14] = LIGHTTYPEPOINT;
				iPredefined_Light_Range[14] = 300.0f / 3.0;

				vPredefined_Light_Palette[15] = ImVec4(250 * fOne, 168 * fOne, 50 * fOne, 1.0f);
				iPredefined_Light_Type[15] = LIGHTTYPESPOT;
				iPredefined_Light_Range[15] = 600.0f / 3.0;
				fPredefined_Light_AngX[15] = 45;

			}
			iPredefinedSetups = 16;
			iPredefinedLights = 16;

			//Pack all colors to the top of the list.
			int iDest = 0;
			for (int il = 0;il < 16; il++)
			{
				if (pref.iSaved_Light_Type[il] != -1)
				{
					pref.iSaved_Light_Type[iDest] = pref.iSaved_Light_Type[il];
					pref.vSaved_Light_Palette_R[iDest] = pref.vSaved_Light_Palette_R[il];
					pref.vSaved_Light_Palette_G[iDest] = pref.vSaved_Light_Palette_G[il];
					pref.vSaved_Light_Palette_B[iDest] = pref.vSaved_Light_Palette_B[il];
					pref.iSaved_Light_Range[iDest] = pref.iSaved_Light_Range[il];
					pref.fSaved_Light_ProbeScale[iDest] = pref.fSaved_Light_ProbeScale[il];

					pref.iSaved_Light_Radius[iDest] = pref.iSaved_Light_Radius[il];
					pref.fSaved_Light_AngX[iDest] = pref.fSaved_Light_AngX[il];
					pref.fSaved_Light_AngY[iDest] = pref.fSaved_Light_AngY[il];
					pref.fSaved_Light_AngZ[iDest] = pref.fSaved_Light_AngZ[il];

					if (il > iDest) pref.iSaved_Light_Type[il] = -1;
					iDest++;
				}
			}
			for (int il = 0;il < 16; il++)
			{
				if (pref.iSaved_Light_Type[il] != -1)
				{
					vPredefined_Light_Palette[iPredefinedLights] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
					vPredefined_Light_Palette[iPredefinedLights].x = pref.vSaved_Light_Palette_R[il];
					vPredefined_Light_Palette[iPredefinedLights].y = pref.vSaved_Light_Palette_G[il];
					vPredefined_Light_Palette[iPredefinedLights].z = pref.vSaved_Light_Palette_B[il];
					iPredefined_Light_Type[iPredefinedLights] = pref.iSaved_Light_Type[il];
					iPredefined_Light_Range[iPredefinedLights] = pref.iSaved_Light_Range[il];
					fPredefined_Light_ProbeScale[iPredefinedLights] = pref.fSaved_Light_ProbeScale[il];

					iPredefined_Light_Radius[iPredefinedLights] = pref.iSaved_Light_Radius[il];
					fPredefined_Light_AngX[iPredefinedLights] = pref.fSaved_Light_AngX[il];
					fPredefined_Light_AngY[iPredefinedLights] = pref.fSaved_Light_AngY[il];
					fPredefined_Light_AngZ[iPredefinedLights] = pref.fSaved_Light_AngZ[il];

					iPredefinedLights++;
				}
			}
			current_light_selected = -1;
			PredefinedLightInit = true;
		}

		// LB: this starts the picking of the best index (if any)
		if (bFirstTimeInFindBestChoice == true)
			current_light_selected = -1;

		int light_icons_columns = 4;
		float light_w = ImGui::GetContentRegionAvailWidth() - 10.0f;
		float light_image_size = light_w / (float)light_icons_columns;
		light_image_size -= ((2.0f) * light_icons_columns) - 2.0f;
		ImVec4 IconColor = { 1.0f,1.0f,1.0f,1.0f };
		int iSelections = 0;

		#ifdef INCLUDEPREDEFINEDLIGHTS

		for (int i = 0; i < iPredefinedLights; i++)
		{
			ImVec4 background = vPredefined_Light_Palette[i];

			int iTextureID = LIGHT_POINT;
			if (iPredefined_Light_Type[i] == LIGHTTYPESPOT) iTextureID = LIGHT_SPOT;

			ImRect image_bb;
			ImVec2 padding = { 1.0, 1.0 };
			ImGuiWindow* window = ImGui::GetCurrentWindow();
			ImVec4 tool_selected_col = ImGui::GetStyle().Colors[ImGuiCol_PlotHistogram];

			// LB: This picks best index
			if (bFirstTimeInFindBestChoice == true)
			{
				if (edit_grideleprof->usespotlighting == iPredefined_Light_Type[i] &&
					edit_grideleprof->light.range == iPredefined_Light_Range[i] )//&&
					//edit_grideleprof->light.fLightHasProbe == fPredefined_Light_ProbeScale[i])
				{
					DWORD color = 0xff000000 + ((unsigned int)(vPredefined_Light_Palette[i].x * 255.0f) << 16) + ((unsigned int)(vPredefined_Light_Palette[i].y * 255.0f) << 8) + +((unsigned int)(vPredefined_Light_Palette[i].z * 255.0f));
					if (color == edit_grideleprof->light.color)
					{
						current_light_selected = i;
						bFirstTimeInFindBestChoice = false;
					}
				}
			}

			if (current_light_selected == i)
			{
				image_bb= ImRect((window->DC.CursorPos - padding), window->DC.CursorPos + padding + ImVec2(light_image_size, light_image_size) );
			}

			ImGui::PushID(LIGHT_POINT + i);
			if (ImGui::ImgBtn(iTextureID, ImVec2(light_image_size, light_image_size), background, IconColor, ImVec4(0.8, 0.8, 0.8, 0.8), ImVec4(0.8, 0.8, 0.8, 0.8), 0, 0, 0, 0, false, false, false, false, true, bBoostIconColors))
			{
				//change
				current_light_selected = i;

				float fr = vPredefined_Light_Palette[i].x * 255.0f;
				float fg = vPredefined_Light_Palette[i].y * 255.0f;
				float fb = vPredefined_Light_Palette[i].z * 255.0f;
				edit_grideleprof->light.color = 0xff000000 + ((unsigned int)fr << 16) + ((unsigned int)fg << 8) + +((unsigned int)fb);
				
				//edit_grideleprof->usespotlighting now follow the same types, if we should add area,sphere.
				if (edit_grideleprof->usespotlighting != iPredefined_Light_Type[i])
				{
					bLightTypeChanged = true;
					edit_grideleprof->usespotlighting = iPredefined_Light_Type[i];
				}

				edit_grideleprof->light.range = iPredefined_Light_Range[i]; //PE: Cant do it here, custom will fail. / 3.0f; // more sensible 
				edit_grideleprof->light.fLightHasProbe = fPredefined_Light_ProbeScale[i]; //PE: Cant do it here, custom will fail. *2.0f; // more sensible 

				edit_grideleprof->light.offsetup = iPredefined_Light_Radius[i];
				if (elementID > 0)
				{
					t.entityelement[elementID].rx = fPredefined_Light_AngX[i];
					t.entityelement[elementID].ry = fPredefined_Light_AngY[i];
					t.entityelement[elementID].rz = fPredefined_Light_AngZ[i];

					if (t.entityelement[elementID].obj > 0)
					{
						RotateObject(t.entityelement[elementID].obj, t.entityelement[elementID].rx, t.entityelement[elementID].ry, t.entityelement[elementID].rz);
					}

					if (g.entityrubberbandlist.size() == 0)
					{
						int iActiveObj = t.widget.activeObject;
						if (iActiveObj > 0)
						{
							RotateObject(iActiveObj, t.entityelement[elementID].rx, t.entityelement[elementID].ry, t.entityelement[elementID].rz);
							g_bRefreshRotationValuesFromObjectOnce = true;
						}
					}
					bNoLightRotate = true;
				}

				bLightChanged = true;
				g_bLightProbeScaleChanged = true;
			}
			ImGui::PopID();
			if (current_light_selected == i)
			{
				if (iSelections++ > 0)
				{
					//Mark dublicates.
					tool_selected_col.w = 0.2;
				}

				window->DrawList->AddRect(image_bb.Min, image_bb.Max, ImGui::GetColorU32(tool_selected_col), 0.0f, 15, 2.0f);
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Select Light Preference");
			
			ImVec2 restore_cursorpos = ImGui::GetCursorPos();
			if ((i + 1) % light_icons_columns != 0 && i != iPredefinedLights - 1)
				ImGui::SameLine();
		}
		bFirstTimeInFindBestChoice = false;

		if (ImGui::StyleButton("Add Light", ImVec2((light_w*0.5f) - 4.0f, 0)))
		{
			ImGui::OpenPopup("##pickV2LightType");
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Add a New Custom Light Using The Current Settings.");

		ImGui::SetNextWindowPosCenter(ImGuiCond_Always);
		if (ImGui::BeginPopup("##pickV2LightType", ImGuiWindowFlags_NoMove))
		{
			float popupwidth = 224.0f;
			ImGui::SetCursorPosX(popupwidth);
			ImGui::SetCursorPosX(0.0f);

			int type_selection = edit_grideleprof->usespotlighting;
			ImVec4 background = { 0.0f,0.0f,0.0f,0.0f };
			ImRect image_bb;
			ImVec2 padding = { 1.0, 1.0 };
			ImGuiWindow* window = ImGui::GetCurrentWindow();
			ImVec4 tool_selected_col = ImGui::GetStyle().Colors[ImGuiCol_PlotHistogram];

			ImGui::TextCenter("Choose a Light Type");
			ImGui::Text("");
			float spacer = 64.0f;
			ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(spacer*0.5, 0.0f));
			if(type_selection == 0) image_bb = ImRect((window->DC.CursorPos - padding), window->DC.CursorPos + padding + ImVec2(spacer, spacer));

			if (ImGui::ImgBtn(LIGHT_POINT, ImVec2(spacer, spacer), background, IconColor, ImVec4(0.8, 0.8, 0.8, 0.8), ImVec4(0.8, 0.8, 0.8, 0.8), 0, 0, 0, 0, false, false, false, false, false, bBoostIconColors))
			{
				edit_grideleprof->usespotlighting = 0;
				bLightTypeChanged = true;
				bLightChanged = true;
				bNoLightRotate = true;
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Select Point Light");

			ImGui::SameLine();
			ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(spacer*0.5, 0.0f));
			if (type_selection != 0) image_bb = ImRect((window->DC.CursorPos - padding), window->DC.CursorPos + padding + ImVec2(spacer, spacer));
			if (ImGui::ImgBtn(LIGHT_SPOT, ImVec2(spacer, spacer), background, IconColor, ImVec4(0.8, 0.8, 0.8, 0.8), ImVec4(0.8, 0.8, 0.8, 0.8), 0, 0, 0, 0, false, false, false, false, false, bBoostIconColors))
			{
				edit_grideleprof->usespotlighting = 1;
				bLightTypeChanged = true;
				bLightChanged = true;
				bNoLightRotate = true;
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Select Spot Light");
			window->DrawList->AddRect(image_bb.Min, image_bb.Max, ImGui::GetColorU32(tool_selected_col), 0.0f, 15, 2.0f);

			ImGui::Text("");
			ImGui::Text("");
			if (ImGui::StyleButton("Cancel", ImVec2(96, 0)))
			{
				ImGui::CloseCurrentPopup();
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Cancel");

			ImGui::SameLine();
			ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(popupwidth-96-96, 0.0f));

			if (ImGui::StyleButton("Add Light", ImVec2(96, 0)))
			{
				int iFreeEntry = -1;
				bool bIsDuplicate = false;
				if (!bIsDuplicate)
				{
					for (int il = 0;il < 16; il++)
					{
						if (pref.iSaved_Light_Type[il] == -1)
						{
							iFreeEntry = il;
							break;
						}
					}
					if (iFreeEntry != -1)
					{
						pref.vSaved_Light_Palette_R[iFreeEntry] = ((edit_grideleprof->light.color & 0x00ff0000) >> 16) / 255.0f;
						pref.vSaved_Light_Palette_G[iFreeEntry] = ((edit_grideleprof->light.color & 0x0000ff00) >> 8) / 255.0f;
						pref.vSaved_Light_Palette_B[iFreeEntry] = (edit_grideleprof->light.color & 0x000000ff) / 255.0f;
						pref.iSaved_Light_Type[iFreeEntry] = edit_grideleprof->usespotlighting;
						pref.iSaved_Light_Range[iFreeEntry] = edit_grideleprof->light.range;
						pref.fSaved_Light_ProbeScale[iFreeEntry] = edit_grideleprof->light.fLightHasProbe;
						PredefinedLightInit = false; //Setup everything again.

						pref.iSaved_Light_Radius[iFreeEntry] = edit_grideleprof->light.offsetup;
						if (elementID > 0)
						{
							pref.fSaved_Light_AngX[iFreeEntry] = t.entityelement[elementID].rx;
							pref.fSaved_Light_AngY[iFreeEntry] = t.entityelement[elementID].ry;
							pref.fSaved_Light_AngZ[iFreeEntry] = t.entityelement[elementID].rz;
						}

					}
				}
				ImGui::CloseCurrentPopup();
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Add Light");

			ImGui::EndPopup();
		}

		ImGui::SameLine(); 
		bool bDisableButton = true;
		if (current_light_selected != -1 && current_light_selected > 15) bDisableButton = false;
		if (ImGui::StyleButtonEx("Delete Light", ImVec2((light_w*0.5f) - 4.0f, 0), bDisableButton))
		{
			if (current_light_selected >= iPredefinedSetups)
			{
				pref.iSaved_Light_Type[current_light_selected - iPredefinedSetups] = -1;
				PredefinedLightInit = false; //Setup everything again.
			}
		}
		if (ImGui::IsItemHovered())
		{
			if (bDisableButton == true)
				ImGui::SetTooltip("Cannot delete first 16 default lights");
			else
				ImGui::SetTooltip("Delete selected custom light");
		}

		if (ImGui::StyleButton("Reset to Default Light", ImVec2(light_w, 0)) )
		{
			current_light_selected = -1;
			bLightTypeChanged = true;
			bLightChanged = true;
			g_bLightProbeScaleChanged = true;
			bSetUpDefaultPAletteWithDefaultColors = true;
			PredefinedLightInit = false; //Setup everything again.
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Reset Light to The Default Settings");


		#endif

		//Name and color setup only.
		//ImGui::Text("");
		if(bDisplayName)
			edit_grideleprof->name_s = imgui_setpropertystring2_v2(t.group, edit_grideleprof->name_s.Get(), "Name", t.strarr_s[204].Get(),readonly);

		ImGui::TextCenter("Light Distance");

		ImGui::PushItemWidth(-10);
		if (ImGui::MaxSliderInputInt("##LightRangeSimpleInput", &edit_grideleprof->light.range, 0, 1000, t.strarr_s[250].Get()))
		{
			current_light_selected = -1;
			bLightChanged = true;
		}
		float fTmp = edit_grideleprof->light.range*100;
		ImGui::PopItemWidth();

		if (edit_grideleprof->usespotlighting != 0)
		{
			ImGui::TextCenter("Spotlight Radius");
			ImGui::PushItemWidth(-10);
			if (ImGui::MaxSliderInputInt("##SpotlightRangeSimpleInput", &edit_grideleprof->light.offsetup, 3, 170, "Sets the spotlight radius"))
			{
				if (edit_grideleprof->light.offsetup < 3) edit_grideleprof->light.offsetup = 3;
				if (edit_grideleprof->light.offsetup > 170) edit_grideleprof->light.offsetup = 170;
				current_light_selected = -1;
				bLightChanged = true;
			}
			ImGui::PopItemWidth();
		}

		ImGui::TextCenter("Light Color");
		float colors[5];
		colors[3] = ((edit_grideleprof->light.color & 0xff000000) >> 24) / 255.0f;
		colors[0] = ((edit_grideleprof->light.color & 0x00ff0000) >> 16) / 255.0f;
		colors[1] = ((edit_grideleprof->light.color & 0x0000ff00) >> 8) / 255.0f;
		colors[2] = (edit_grideleprof->light.color & 0x000000ff) / 255.0f;

		ImVec4 mycolor = ImVec4(colors[0], colors[1], colors[2], 1.0);
		float w = ImGui::GetContentRegionAvailWidth();
		extern bool bUseOrgHue;
		bUseOrgHue = true;
		bool open_popup = ImGui::ColorButton("##NewV2LightColor", mycolor, 0, ImVec2(w - 10.0, 0));
		if (open_popup) ImGui::OpenPopup("##pickV2LightColor");
		if (ImGui::BeginPopup("##pickV2LightColor", ImGuiWindowFlags_NoMove))
		{
			if (ImGui::ColorPicker4("##pickerV2LightColor", (float*)&mycolor, ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview))
			{
				//LB: team decided the select light is being actively reconfigured
				//current_light_selected = -1; (see bwlow)
				bLightChanged = true;
			}
			ImGui::EndPopup();
		}
		bUseOrgHue = false;
		colors[0] = mycolor.x * 255.0f;
		colors[1] = mycolor.y * 255.0f;
		colors[2] = mycolor.z * 255.0f;
		edit_grideleprof->light.color = 0xff000000 + ((unsigned int)colors[0] << 16) + ((unsigned int)colors[1] << 8) + +((unsigned int)colors[2]);

		ImGuiWindow* window = ImGui::GetCurrentWindow();
		ID3D11ShaderResourceView* lpTexture = GetImagePointerView(TOOL_PENCIL);
		ImVec2 vDrawPos = { ImGui::GetCursorScreenPos().x + (ImGui::GetContentRegionAvail().x - 30.0f) ,ImGui::GetCursorScreenPos().y - (ImGui::GetFontSize()*1.5f) - 3.0f };
		window->DrawList->AddImage((ImTextureID)lpTexture, vDrawPos, vDrawPos + ImVec2(16, 16), ImVec2(0, 0), ImVec2(1, 1), ImGui::GetColorU32(ImVec4(1, 1, 1, 1)));
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Set Light Color");

		//LB: Grr, seems all castshadows up to now (Feb2022) are zero, and we need them to be
		// casting by default, so we will assume 'castshadow==0 or -1' is CAST and 'castshadow==1' is NO CAST
		bool bCastShadow = true;
		if (edit_grideleprof->castshadow == 1 ) bCastShadow = false;
		if (ImGui::Checkbox("Cast Shadow", &bCastShadow))
		{
			if (bCastShadow == true)
				edit_grideleprof->castshadow = -1;
			else
				edit_grideleprof->castshadow = 1;
			bLightChanged = true;
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Set whether a shadow is cast");

		//LB: whatever the current palette selection, it will be changed!
		if (current_light_selected != -1)
		{
			vPredefined_Light_Palette[current_light_selected].x = mycolor.x;
			vPredefined_Light_Palette[current_light_selected].y = mycolor.y;
			vPredefined_Light_Palette[current_light_selected].z = mycolor.z;
		}

		//#### Light Behaviour ####
		ImGui::TextCenter("Light Behaviour");

		#ifdef USENEWLIGHTBEHAVIOUR
		w = ImGui::GetContentRegionAvailWidth() - 16.0f;
		float ImgW = ImageWidth(fpe_current_loaded_script_image);
		float ImgH = ImageHeight(fpe_current_loaded_script_image);
		float fRatio = w / ImgW;
		if (!bHideIcon)
		{
			if (iSelectedLibraryStingReturnID == window->GetID("ScriptSelector##+"))
			{
				//Update Script.
				if (sSelectedLibrarySting != "")
				{
					edit_grideleprof->aimain_s = sSelectedLibrarySting;
					sSelectedLibrarySting = "";
					iSelectedLibraryStingReturnID = -1; //disable.
					fpe_current_loaded_script = -1; //Reload image and DLUA.
				}
			}
			if (ImGui::ImgBtn(fpe_current_loaded_script_image, ImVec2(ImgW * fRatio, ImgH * fRatio), drawCol_black, drawCol_normal, drawCol_normal, drawCol_normal, -1, 0, 0, 0, false))
			{
				//Select script.
				sStartLibrarySearchString = "Light";
				iLastDisplayLibraryType = -1;
				bExternal_Entities_Window = true;
				iDisplayLibraryType = 4;
				iLibraryStingReturnToID = window->GetID("ScriptSelector##+");
				if (edit_grideleprof->aimain_s.Len() > 0)
					sMakeDefaultSelecting = edit_grideleprof->aimain_s;

			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Select Light Behavior");
		}
		ImGui::TextCenter(cDisplayName);
		#else

		//Combo light script selection.

		const char* light_behaviour[] = { "None" , "Flicker", "Strobe", "Rotate" };
		const char* light_scripts[] = { "markers\\light1.lua" , "markers\\lightflicker.lua", "markers\\lightstrobe.lua", "markers\\lightrotate.lua" };
		int iBehaviourItems = IM_ARRAYSIZE(light_behaviour);
		int current_light_behaviour = -1;
		for (int i = 0; i < iBehaviourItems; i++)
		{
			if (stricmp(light_scripts[i], edit_grideleprof->aimain_s.Get()) == 0)
			{
				current_light_behaviour = i;
				break;
			}
		}
		if (current_light_behaviour < 0) current_light_behaviour = 0; //default None.

		ImGui::PushItemWidth(-10);
		if (ImGui::Combo("##comboLightBehaviour", &current_light_behaviour, light_behaviour, iBehaviourItems))
		{
			edit_grideleprof->aimain_s = light_scripts[current_light_behaviour];
			fpe_current_loaded_script = -1; //Reload image and DLUA.
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Select Light Behavior");
		ImGui::PopItemWidth();
		#endif	

		if (lua_grideleprof->PropertiesVariableActive == 1) {
			speech_entries = DisplayLuaDescription(lua_grideleprof);
		}
		else
		{
			if (lua_grideleprof->PropertiesVariable.VariableDescription.Len() > 0) 
			{
				if (edit_grideleprof->aimain_s != "default.lua" )//&& edit_grideleprof->aimain_s != "markers\\light1.lua") //No need to display.
					DisplayLuaDescriptionOnly(lua_grideleprof);
			}
		}

		if (speech_entries > 0)
		{
			// all SPEECH control is moved to this function.
			SpeechControls(speech_entries, bUpdateMainString, edit_grideleprof);
		}

		// update the light live

		bool bAllowProbeUpdate = false;

		if (!readonly)
		{
			// update light if change angles
			static float fLastLightXAngle = -1.0f;
			static float fLastLightYAngle = -1.0f;
			static float fLastLightZAngle = -1.0f;
			if (elementID > 0)
			{
				if (t.entityelement[elementID].rx != fLastLightXAngle || t.entityelement[elementID].ry != fLastLightYAngle || t.entityelement[elementID].rz != fLastLightZAngle)
				{
					fLastLightXAngle = t.entityelement[elementID].rx;
					fLastLightYAngle = t.entityelement[elementID].ry;
					fLastLightZAngle = t.entityelement[elementID].rz;
					bLightChanged = true;
				}
			}

			// check if light needs updating in the engine
			if (bLightChanged)
			{
				int ilightIndex = 0;
				float lightx, lighty, lightz, lightax, lightay, lightaz;
				if (elementID > 0)
				{
					ilightIndex = t.entityelement[elementID].eleprof.light.index;
				}
				if (!bNoLightRotate && bLightTypeChanged && elementID > 0)
				{
					if (edit_grideleprof->usespotlighting == LIGHTTYPESPOT)
						t.entityelement[elementID].rx = 45.0f; //Default to 45 angle x so we can see the spot on terrain.
					else
					{
						t.entityelement[elementID].rx = 90.0f; //PE: Wicked object default to angle x 90.
					}

					if (t.entityelement[elementID].obj > 0)
					{
						RotateObject(t.entityelement[elementID].obj, t.entityelement[elementID].rx, t.entityelement[elementID].ry, t.entityelement[elementID].rz);
					}

					if (g.entityrubberbandlist.size() == 0)
					{
						int iActiveObj = t.widget.activeObject;
						if (iActiveObj > 0)
						{
							RotateObject(iActiveObj, t.entityelement[elementID].rx, t.entityelement[elementID].ry, t.entityelement[elementID].rz);
							g_bRefreshRotationValuesFromObjectOnce = true;
						}
					}
				}

				if (elementID > 0 && ilightIndex > 0)
				{
					// any light probes destroy the effect of the light sibling
					float fLightHasProbe = edit_grideleprof->light.fLightHasProbe;
					if (fLightHasProbe >= 50.0f)
					{
						colors[0] = 0;
						colors[1] = 0;
						colors[2] = 0;
						edit_grideleprof->light.color = 255<<24;
						edit_grideleprof->light.range = 0;
						edit_grideleprof->castshadow = 1;
					}

					int iWickedLightIndex = t.infinilight[ilightIndex].wickedlightindex;
					if (bLightTypeChanged)
					{
						//Recreate light.
						WickedCall_DeleteLight(iWickedLightIndex);
						int iLightType = 1;
						if (edit_grideleprof->usespotlighting) iLightType = 2;
						iWickedLightIndex = WickedCall_AddLight(iLightType);
						t.infinilight[ilightIndex].wickedlightindex = iWickedLightIndex;
						bLightTypeChanged = false;
					}
					lightx = t.entityelement[elementID].x;
					lighty = t.entityelement[elementID].y;
					lightz = t.entityelement[elementID].z;
					lightax = t.entityelement[elementID].rx;
					lightay = t.entityelement[elementID].ry;
					lightaz = t.entityelement[elementID].rz;
					float lightrange = edit_grideleprof->light.range;
					float spotlightradius = edit_grideleprof->light.offsetup;
					int colr = colors[0];
					int colg = colors[1];
					int colb = colors[2];
					bool bCastShadow = true;
					if (edit_grideleprof->castshadow == 1) bCastShadow = false;

					//PE: Also update infinilight.
					t.infinilight[ilightIndex].f_angle_x = lightax;
					t.infinilight[ilightIndex].f_angle_y = lightay;
					t.infinilight[ilightIndex].f_angle_z = lightaz;
					t.infinilight[ilightIndex].x = lightx;
					t.infinilight[ilightIndex].y = lighty;
					t.infinilight[ilightIndex].z = lightz;
					t.infinilight[ilightIndex].range = lightrange;
					t.infinilight[ilightIndex].spotlightradius = spotlightradius;
					t.infinilight[ilightIndex].colrgb.r = colr;
					t.infinilight[ilightIndex].colrgb.g = colg;
					t.infinilight[ilightIndex].colrgb.b = colb;
					t.infinilight[ilightIndex].fLightHasProbe = fLightHasProbe;
					t.infinilight[ilightIndex].bCanShadow = bCastShadow;
					WickedCall_UpdateLight(iWickedLightIndex, lightx, lighty, lightz, lightax, lightay, lightaz, lightrange, spotlightradius, colr, colg, colb, bCastShadow);
				}
				else
				{
					if (bLightTypeChanged)
					{
						//Recreate light.
						if(t.gridentitywickedlightindex > 0)
							WickedCall_DeleteLight(t.gridentitywickedlightindex);
						t.gridentitywickedlightindex = 0;
						bLightTypeChanged = false;
					}
					if (t.gridentitywickedlightindex == 0)
					{
						int iLightType = 1;
						if (edit_grideleprof->usespotlighting) iLightType = 2;
						t.gridentitywickedlightindex = WickedCall_AddLight(iLightType);
					}
					if (t.gridentitywickedlightindex > 0)
					{
						lightx = t.gridentityposx_f;
						lighty = t.gridentityposy_f;
						lightz = t.gridentityposz_f;
						lightax = t.gridentityrotatex_f;
						lightay = t.gridentityrotatey_f;
						lightaz = t.gridentityrotatez_f;
						float lightrange = edit_grideleprof->light.range;
						float spotlightradius = edit_grideleprof->light.offsetup;
						int colr = colors[0];
						int colg = colors[1];
						int colb = colors[2];
						bool bCastShadow = true;
						if (edit_grideleprof->castshadow == 1) bCastShadow = false;
						WickedCall_UpdateLight(t.gridentitywickedlightindex, lightx, lighty, lightz, lightax, lightay, lightaz, lightrange, spotlightradius, colr, colg, colb, bCastShadow);
					}
				}

				// ensure the light object itself is updated
				int obj = t.entityelement[elementID].obj;
				entity_updatelightobj(elementID,obj);
			}
		}

	}
	#ifdef USENEWPARTICLESETUP
	else if (t.entityprofile[entid].ismarker == 10)
	{
		//Particles.
		#define MAXPREDEFINEDPARTICLESETUPS 32

		static bool bPredefinedParticleInit = false;

		static int iPredefinedParticles = 9;
		int iPredefinedParticleSetups = 9;
		static cstr Predefined_Particle_Name[MAXPREDEFINEDPARTICLESETUPS];
		static int Predefined_Particle_Image[MAXPREDEFINEDPARTICLESETUPS];
		static bool Predefined_bParticle_Preview[MAXPREDEFINEDPARTICLESETUPS];
		static bool Predefined_bParticle_Show_At_Start[MAXPREDEFINEDPARTICLESETUPS];
		static bool Predefined_bParticle_Looping_Animation[MAXPREDEFINEDPARTICLESETUPS];
		static bool Predefined_bParticle_Full_Screen[MAXPREDEFINEDPARTICLESETUPS];
		static float Predefined_fParticle_Fullscreen_Duration[MAXPREDEFINEDPARTICLESETUPS];
		static float Predefined_fParticle_Fullscreen_Fadein[MAXPREDEFINEDPARTICLESETUPS];
		static float Predefined_fParticle_Fullscreen_Fadeout[MAXPREDEFINEDPARTICLESETUPS];
		static cstr Predefined_Particle_Fullscreen_Transition[MAXPREDEFINEDPARTICLESETUPS];
		static float Predefined_fParticle_Speed[MAXPREDEFINEDPARTICLESETUPS];
		static float Predefined_fParticle_Opacity[MAXPREDEFINEDPARTICLESETUPS];


		newparticletype newparticle_init[MAXPREDEFINEDPARTICLESETUPS];

		if (!bPredefinedParticleInit)
		{
			//Init.
			for (int i = 0; i < iPredefinedParticleSetups; i++)
			{
				if (ImageExist(Predefined_Particle_Image[i]))
				{
					if(Predefined_Particle_Image[i] != FILETYPE_PARTICLE)
						DeleteImage(Predefined_Particle_Image[i]);
				}
				Predefined_Particle_Image[i] = 0;
			}

			for (int i = 0; i < iPredefinedParticleSetups; i++)
			{
				//Defaults
				Predefined_bParticle_Preview[i] = true;
				Predefined_bParticle_Show_At_Start[i] = true;
				Predefined_bParticle_Looping_Animation[i] = true;
				Predefined_bParticle_Full_Screen[i] = false;
				Predefined_fParticle_Fullscreen_Duration[i] = 10.0f;
				Predefined_fParticle_Fullscreen_Fadein[i] = 1.0f;
				Predefined_fParticle_Fullscreen_Fadeout[i] = 1.0f;
				Predefined_Particle_Fullscreen_Transition[i] = "";
				Predefined_fParticle_Speed[i] = 1.0f;
				Predefined_fParticle_Opacity[i] = 1.0f;

				if (i == 0) Predefined_Particle_Name[i] = "particlesbank\\default";
				if (i == 1) Predefined_Particle_Name[i] = "particlesbank\\portal5";
				if (i == 2) Predefined_Particle_Name[i] = "particlesbank\\stylized_poisonring";
				if (i == 3) Predefined_Particle_Name[i] = "particlesbank\\smoke_billowy";
				if (i == 4) Predefined_Particle_Name[i] = "particlesbank\\fountain_directional";
				if (i == 5) Predefined_Particle_Name[i] = "particlesbank\\fire_tornado_3";
				if (i == 6) Predefined_Particle_Name[i] = "particlesbank\\fire_and_smoke";
				if (i == 7) Predefined_Particle_Name[i] = "particlesbank\\explosion";
				if (i == 8) Predefined_Particle_Name[i] = "particlesbank\\smoke_thick";

				if (i == 7) Predefined_bParticle_Looping_Animation[i] = false;

				Predefined_Particle_Image[i] = MARKETPLACE_ICONS + 50 + i;

				cstr img = Predefined_Particle_Name[i] + cstr(".arx");
				CreateBackBufferCacheName(img.Get(), 512, 288);

				SetMipmapNum(1); //PE: mipmaps not needed.
				image_setlegacyimageloading(true);
				if (!(strlen(Storyboard.gamename) > 0 && strlen(Storyboard.customprojectfolder) > 0))
				{
					GG_SetWritablesToRoot(true);
				}
				if (FileExist(BackBufferCacheName.Get()))
				{
					LoadImage((char *)BackBufferCacheName.Get(), Predefined_Particle_Image[i]);
					if (!ImageExist(Predefined_Particle_Image[i]))
					{
						Predefined_Particle_Image[i] = FILETYPE_PARTICLE;
					}
				}
				else
				{
					Predefined_Particle_Image[i] = FILETYPE_PARTICLE;
				}
				image_setlegacyimageloading(false);
				SetMipmapNum(-1);
				GG_SetWritablesToRoot(false);
			}

			iPredefinedParticles = iPredefinedParticleSetups;

			//Pack to top of list.
			int iDest = 0;
			for (int i = 0;i < 16; i++)
			{
				if (strlen(pref.Saved_Particle_Name[i]) > 0)
				{
					strcpy(pref.Saved_Particle_Name[iDest],pref.Saved_Particle_Name[i]);
					pref.Saved_bParticle_Preview[iDest] = pref.Saved_bParticle_Preview[i];
					pref.Saved_bParticle_Show_At_Start[iDest] = pref.Saved_bParticle_Show_At_Start[i];
					pref.Saved_bParticle_Looping_Animation[iDest] = pref.Saved_bParticle_Looping_Animation[i];
					pref.Saved_bParticle_Full_Screen[iDest] = pref.Saved_bParticle_Full_Screen[i];
					pref.Saved_fParticle_Fullscreen_Duration[iDest] = pref.Saved_fParticle_Fullscreen_Duration[i];
					pref.Saved_fParticle_Fullscreen_Fadein[iDest] = pref.Saved_fParticle_Fullscreen_Fadein[i];
					pref.Saved_fParticle_Fullscreen_Fadeout[iDest] = pref.Saved_fParticle_Fullscreen_Fadeout[i];
					strcpy(pref.Saved_Particle_Fullscreen_Transition[iDest],pref.Saved_Particle_Fullscreen_Transition[i]);
					pref.Saved_fParticle_Speed[iDest] = pref.Saved_fParticle_Speed[i];
					pref.Saved_fParticle_Opacity[iDest] = pref.Saved_fParticle_Opacity[i];

					if (i > iDest) strcpy(pref.Saved_Particle_Name[i],"");
					iDest++;
				}
			}

			//Add saved from prefs.
			for (int i = 0; i < 16; i++)
			{
				if (strlen(pref.Saved_Particle_Name[i]) > 0)
				{
					Predefined_Particle_Image[iPredefinedParticles] = MARKETPLACE_ICONS + 50 + iPredefinedParticles + i;
					Predefined_Particle_Name[iPredefinedParticles] = pref.Saved_Particle_Name[i];
					Predefined_bParticle_Preview[iPredefinedParticles] = pref.Saved_bParticle_Preview[i];
					Predefined_bParticle_Show_At_Start[iPredefinedParticles] = pref.Saved_bParticle_Show_At_Start[i];
					Predefined_bParticle_Looping_Animation[iPredefinedParticles] = pref.Saved_bParticle_Looping_Animation[i];
					Predefined_bParticle_Full_Screen[iPredefinedParticles] = pref.Saved_bParticle_Full_Screen[i];
					Predefined_fParticle_Fullscreen_Duration[iPredefinedParticles] = pref.Saved_fParticle_Fullscreen_Duration[i];
					Predefined_fParticle_Fullscreen_Fadein[iPredefinedParticles] = pref.Saved_fParticle_Fullscreen_Fadein[i];
					Predefined_fParticle_Fullscreen_Fadeout[iPredefinedParticles] = pref.Saved_fParticle_Fullscreen_Fadeout[i];
					Predefined_Particle_Fullscreen_Transition[iPredefinedParticles] = pref.Saved_Particle_Fullscreen_Transition[i];
					Predefined_fParticle_Speed[iPredefinedParticles] = pref.Saved_fParticle_Speed[i];
					Predefined_fParticle_Opacity[iPredefinedParticles] = pref.Saved_fParticle_Opacity[i];

					cstr img = Predefined_Particle_Name[iPredefinedParticles];
					if (strlen(pref.Saved_Particle_Name[i]) > 4)
					{
						if (strnicmp(img.Get() + strlen(img.Get()) - 4, ".arx", 4) == NULL)
						{
							img = Left(img.Get(), Len(img.Get()) - 4);
						}
					}
					// ★ GGMAX 3.38 (DX11 3e21f674, Particle System Upgrade): a WPE particle is a '.pe' file and
					// ships its own .jpg thumbnail beside it. Use that directly when present; only fall back to
					// generating a back-buffer cache thumb for the legacy .arx effects.
					// ⚠ Hand-placed: DX11 also wraps the GG_SetWritablesToRoot below in a Storyboard test this tree
					// did not have; that guard is now ported too (DX11 1ad98209).
					bool bHasAJPG = false;
					LPSTR pParticleName = Predefined_Particle_Name[iPredefinedParticles].Get();
					if (strnicmp(pParticleName + strlen(pParticleName) - 3, ".pe", 3) == NULL)
					{
						BackBufferCacheName = Left(pParticleName, strlen(pParticleName) - 3);
						BackBufferCacheName = BackBufferCacheName + ".jpg";
						if (FileExist(BackBufferCacheName.Get()))
						{
							bHasAJPG = true;
						}
					}
					else
					{
						img = img + cstr(".arx");
					}
					if (bHasAJPG == false)
					{
						// revert to traditional thumb
						CreateBackBufferCacheName(img.Get(), 512, 288);
					}
					if (!(strlen(Storyboard.gamename) > 0 && strlen(Storyboard.customprojectfolder) > 0))
					{
						GG_SetWritablesToRoot(true);
					}
					SetMipmapNum(1); //PE: mipmaps not needed.
					image_setlegacyimageloading(true);
					if (FileExist(BackBufferCacheName.Get()))
					{
						LoadImage((char *)BackBufferCacheName.Get(), Predefined_Particle_Image[iPredefinedParticles]);
						if (!ImageExist(Predefined_Particle_Image[iPredefinedParticles]))
						{
							Predefined_Particle_Image[iPredefinedParticles] = FILETYPE_PARTICLE;
						}
					}
					else
					{
						Predefined_Particle_Image[iPredefinedParticles] = FILETYPE_PARTICLE;
					}
					image_setlegacyimageloading(false);
					SetMipmapNum(-1);
					GG_SetWritablesToRoot(false);

					iPredefinedParticles++;
				}
			}

			bPredefinedParticleInit = true;
		}


		static int current_particle_selected = -1;
		bool bUpdateParticle = false;

		//static bool bSetUpDefaultPAletteWithDefaultColors = true;
		static bool bFindBestParticleChoice = false;
		static int iLastParticleEntityElementIDHere = -1;
		if (elementID != iLastParticleEntityElementIDHere)
		{
			iLastParticleEntityElementIDHere = elementID;
			current_particle_selected = -1;
			bFindBestParticleChoice = true;
		}

		if (bFindBestParticleChoice)
		{
			bFindBestParticleChoice = false;

			//Try to locate a matching particle setup.
			for (int i = 0; i < iPredefinedParticles; i++)
			{
				bool bValid = true;
				std::string sString = t.entityelement[elementID].eleprof.newparticle.emittername.Get();
				replaceAll(sString, "/", "\\");
				if ( stricmp(sString.c_str() , Predefined_Particle_Name[i].Get()) != 0 ) bValid = false;
				if (t.entityelement[elementID].eleprof.newparticle.bParticle_Preview != Predefined_bParticle_Preview[i]) bValid = false;
				if (t.entityelement[elementID].eleprof.newparticle.bParticle_Show_At_Start != Predefined_bParticle_Show_At_Start[i]) bValid = false;
				if (t.entityelement[elementID].eleprof.newparticle.bParticle_Looping_Animation != Predefined_bParticle_Looping_Animation[i]) bValid = false;
				if (t.entityelement[elementID].eleprof.newparticle.Particle_Fullscreen_Transition != Predefined_Particle_Fullscreen_Transition[i]) bValid = false;

				//Float roundings so cant just compare. just use int's.
				if ( (int) t.entityelement[elementID].eleprof.newparticle.bParticle_Full_Screen != (int) Predefined_bParticle_Full_Screen[i]) bValid = false;
				if ( (int) t.entityelement[elementID].eleprof.newparticle.fParticle_Fullscreen_Duration != (int) Predefined_fParticle_Fullscreen_Duration[i]) bValid = false;
				if ( (int) t.entityelement[elementID].eleprof.newparticle.fParticle_Fullscreen_Fadein != (int) Predefined_fParticle_Fullscreen_Fadein[i]) bValid = false;
				if ( (int) t.entityelement[elementID].eleprof.newparticle.fParticle_Fullscreen_Fadeout != (int) Predefined_fParticle_Fullscreen_Fadeout[i]) bValid = false;
				//Remove precision so we get the best match.
				if ( (int) (t.entityelement[elementID].eleprof.newparticle.fParticle_Speed*5) != (int) (Predefined_fParticle_Speed[i]*5) ) bValid = false;
				if ( (int) (t.entityelement[elementID].eleprof.newparticle.fParticle_Opacity*5) != (int) (Predefined_fParticle_Opacity[i]*5) ) bValid = false;

				if (bValid)
				{
					current_particle_selected = i;
					bUpdateParticle = true;
					break;
				}

			}
		}

		#define USEPARTICLECOLUMNS
		int particle_icons_columns = 3;
		float particle_w = ImGui::GetContentRegionAvailWidth() - 10.0f;
		float particle_image_size = particle_w / (float)particle_icons_columns;
		#ifdef USEPARTICLECOLUMNS
		particle_image_size -= 3; //Border of 1
		#endif
		float fRatio = 288.0f / 512.0f;
		particle_image_size -= ((2.0f) * particle_icons_columns) - 2.0f;
		ImVec4 IconColor = { 1.0f,1.0f,1.0f,1.0f };
		ImVec4 background = { 0.0f,0.0f,0.0f,1.0f };
		ImGuiWindow* window = ImGui::GetCurrentWindow();

		#ifdef USEPARTICLECOLUMNS
		ImVec2 iOldWindowPadding = ImGui::GetStyle().WindowPadding;
		ImGui::GetStyle().WindowPadding = { 1.0f,1.0f };
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 4));
		ImGui::Columns(3, "mycolumns3particles", false);  //false no border
		if (particle_image_size < 200)
		{
		}
		#endif
		
		bool bPossibleWickedParticleSelectionChanged = false;
		ImVec4 tool_selected_col = ImGui::GetStyle().Colors[ImGuiCol_PlotHistogram];
		bool bDrawsSelection = false;
		ImRect image_draw_bb;
		for (int i = 0; i < iPredefinedParticles; i++)
		{
			ImRect image_bb;
			ImVec2 padding = { 0.0, 1.0 };

			//Auto pick ?

			if (current_particle_selected == i)
			{
				image_bb = ImRect((window->DC.CursorPos - padding), window->DC.CursorPos + padding + ImVec2(particle_image_size, particle_image_size * fRatio));
			}

			ImGui::PushID(FILETYPE_PARTICLE + i);
			if (ImGui::ImgBtn(Predefined_Particle_Image[i], ImVec2(particle_image_size, particle_image_size * fRatio), background, IconColor, ImVec4(0.8, 0.8, 0.8, 0.8), ImVec4(0.8, 0.8, 0.8, 0.8), 0, 0, 0, 0, false, false, false, false, true, false)) //bBoostIconColors
			{
				//Change selection.
				current_particle_selected = i;
				//Setup default parameters.
				bUpdateParticle = true;
				bPossibleWickedParticleSelectionChanged = true;

				if (elementID > 0)
				{
					// clear any old particle
					int iParticleEmitter = t.entityelement[elementID].eleprof.newparticle.emitterid;
					if (iParticleEmitter != -1)
					{
						gpup_deleteEffect(iParticleEmitter);
					}

					// set the new settings
					t.entityelement[elementID].eleprof.newparticle.emitterid = -1;
					t.entityelement[elementID].eleprof.newparticle.emittername = Predefined_Particle_Name[current_particle_selected];
					t.entityelement[elementID].eleprof.newparticle.bParticle_Preview = Predefined_bParticle_Preview[current_particle_selected];
					t.entityelement[elementID].eleprof.newparticle.bParticle_Show_At_Start = Predefined_bParticle_Show_At_Start[current_particle_selected];
					t.entityelement[elementID].eleprof.newparticle.bParticle_Looping_Animation = Predefined_bParticle_Looping_Animation[current_particle_selected];
					t.entityelement[elementID].eleprof.newparticle.bParticle_Full_Screen = Predefined_bParticle_Full_Screen[current_particle_selected];
					t.entityelement[elementID].eleprof.newparticle.fParticle_Fullscreen_Duration = Predefined_fParticle_Fullscreen_Duration[current_particle_selected];
					t.entityelement[elementID].eleprof.newparticle.fParticle_Fullscreen_Fadein = Predefined_fParticle_Fullscreen_Fadein[current_particle_selected];
					t.entityelement[elementID].eleprof.newparticle.fParticle_Fullscreen_Fadeout = Predefined_fParticle_Fullscreen_Fadeout[current_particle_selected];
					t.entityelement[elementID].eleprof.newparticle.Particle_Fullscreen_Transition = Predefined_Particle_Fullscreen_Transition[current_particle_selected];
					t.entityelement[elementID].eleprof.newparticle.fParticle_Speed = Predefined_fParticle_Speed[current_particle_selected];
					t.entityelement[elementID].eleprof.newparticle.fParticle_Opacity = Predefined_fParticle_Opacity[current_particle_selected];

					// and decide on script to use
					LPSTR pParticleName = Predefined_Particle_Name[current_particle_selected].Get();
					if (strnicmp(pParticleName + strlen(pParticleName) - 3, ".pe", 3) == NULL)
					{
						t.entityelement[elementID].eleprof.aimain_s = "particles\\wpe_area.lua";
						t.entityelement[elementID].eleprof.soundset4_s = "wpe_area_properties(2";
						t.entityelement[elementID].eleprof.soundset4_s += "\"";
						t.entityelement[elementID].eleprof.soundset4_s += t.entityelement[elementID].eleprof.newparticle.emittername;
						t.entityelement[elementID].eleprof.soundset4_s += "\",3\"";
						if (t.entityelement[elementID].eleprof.newparticle.bParticle_Show_At_Start == 1)
							t.entityelement[elementID].eleprof.soundset4_s += "1";
						else
							t.entityelement[elementID].eleprof.soundset4_s += "0";
						t.entityelement[elementID].eleprof.soundset4_s += "\",0\"0\",0\"0\",0\"0\")";
					}
					else
					{
						t.entityelement[elementID].eleprof.aimain_s = "markers\\particle.lua";
					}

					// and copy to cursor object
					edit_grideleprof->newparticle = t.entityelement[elementID].eleprof.newparticle;
				}
			}
			ImGui::PopID();

			if (current_particle_selected == i)
			{
				//if (iSelections++ > 0)
				//{
				//	//Mark dublicates.
				//	tool_selected_col.w = 0.2;
				//}
				#ifndef USEPARTICLECOLUMNS
				window->DrawList->AddRect(image_bb.Min, image_bb.Max, ImGui::GetColorU32(tool_selected_col), 0.0f, 15, 2.0f);
				#else
				image_draw_bb = image_bb;
				bDrawsSelection = true;
				#endif
			}
			#ifndef USEPARTICLECOLUMNS
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Select Particle Preference");
			#endif

			#ifdef USEPARTICLECOLUMNS
			char *find = (char *)pestrcasestr(Predefined_Particle_Name[i].Get(), "particlesbank\\");
			if (find) find += 14;
			if (particle_image_size > 130)
			{
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("Select Particle Preference");
				if (find)
				{
					ImGui::TextCenter(find);
				}
			}
			else if(find)
			{
				cstr tooltip = cstr("Select Particle (") + cstr(find) + cstr(")");
				if (ImGui::IsItemHovered()) ImGui::SetTooltip(tooltip.Get());
			}
			else
			{
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("Select Particle Preference");
			}
			#endif

			#ifdef USEPARTICLECOLUMNS
			ImGui::NextColumn();
			#else	
			if ((i + 1) % particle_icons_columns != 0 && i != iPredefinedParticles - 1)
				ImGui::SameLine();
			#endif

		}
		#ifdef USEPARTICLECOLUMNS
		ImGui::Columns(1);
		if (bDrawsSelection)
		{
			window->DrawList->AddRect(image_draw_bb.Min, image_draw_bb.Max, ImGui::GetColorU32(tool_selected_col), 0.0f, 15, 2.0f);
		}

		ImGui::GetStyle().WindowPadding = iOldWindowPadding;
		ImGui::PopStyleVar();

		#endif
		ImGui::Text("");

		if (ImGui::StyleButton("New Particle##+", ImVec2((particle_w*0.5f) - 4.0f, 0)) || iSelectedLibraryStingReturnID == window->GetID("New Particle##+"))
		{
			bPossibleWickedParticleSelectionChanged = true;
			if (iSelectedLibraryStingReturnID == window->GetID("New Particle##+"))
			{
				//This goes to the saved in pref. find free.
				int iSaveTo = -1;
				for (int i = 0; i < 16; i++)
				{
					if (strlen(pref.Saved_Particle_Name[i]) <= 0)
					{
						iSaveTo = i;
						break;
					}
				}
				if (iSaveTo > -1)
				{
					if (strnicmp(sSelectedLibrarySting.Get() + strlen(sSelectedLibrarySting.Get()) - 4, ".arx", 4) == NULL) sSelectedLibrarySting = Left(sSelectedLibrarySting.Get(), Len(sSelectedLibrarySting.Get()) - 4);
					strcpy(pref.Saved_Particle_Name[iSaveTo], sSelectedLibrarySting.Get());

					current_particle_selected = iSaveTo + iPredefinedParticleSetups;

					if (current_particle_selected >= 0 && current_particle_selected < 32)
						Predefined_Particle_Name[current_particle_selected] = sSelectedLibrarySting;

				}

				sSelectedLibrarySting = "";
				iSelectedLibraryStingReturnID = -1; //disable.

				//PE: Add new settings to current particle.
				if (iSaveTo > -1 && elementID > 0)
				{
					int iParticleEmitter = t.entityelement[elementID].eleprof.newparticle.emitterid;
					if (iParticleEmitter != -1)
					{
						gpup_deleteEffect(iParticleEmitter);
					}
					t.entityelement[elementID].eleprof.newparticle.emitterid = -1;
					t.entityelement[elementID].eleprof.newparticle.emittername = Predefined_Particle_Name[current_particle_selected];
					//Setup defaults.
					Predefined_bParticle_Preview[current_particle_selected] = true;
					Predefined_bParticle_Show_At_Start[current_particle_selected] = true;
					Predefined_bParticle_Looping_Animation[current_particle_selected] = true;
					Predefined_bParticle_Full_Screen[current_particle_selected] = false;
					Predefined_fParticle_Fullscreen_Duration[current_particle_selected] = 10.0f;
					Predefined_fParticle_Fullscreen_Fadein[current_particle_selected] = 1.0f;
					Predefined_fParticle_Fullscreen_Fadeout[current_particle_selected] = 1.0f;
					Predefined_Particle_Fullscreen_Transition[current_particle_selected] = "";
					Predefined_fParticle_Speed[current_particle_selected] = 1.0f;
					Predefined_fParticle_Opacity[current_particle_selected] = 1.0f;
					//Add new settings to eleprof.
					t.entityelement[elementID].eleprof.newparticle.bParticle_Preview = Predefined_bParticle_Preview[current_particle_selected];
					t.entityelement[elementID].eleprof.newparticle.bParticle_Show_At_Start = Predefined_bParticle_Show_At_Start[current_particle_selected];
					t.entityelement[elementID].eleprof.newparticle.bParticle_Looping_Animation = Predefined_bParticle_Looping_Animation[current_particle_selected];
					t.entityelement[elementID].eleprof.newparticle.bParticle_Full_Screen = Predefined_bParticle_Full_Screen[current_particle_selected];
					t.entityelement[elementID].eleprof.newparticle.fParticle_Fullscreen_Duration = Predefined_fParticle_Fullscreen_Duration[current_particle_selected];
					t.entityelement[elementID].eleprof.newparticle.fParticle_Fullscreen_Fadein = Predefined_fParticle_Fullscreen_Fadein[current_particle_selected];
					t.entityelement[elementID].eleprof.newparticle.fParticle_Fullscreen_Fadeout = Predefined_fParticle_Fullscreen_Fadeout[current_particle_selected];
					t.entityelement[elementID].eleprof.newparticle.Particle_Fullscreen_Transition = Predefined_Particle_Fullscreen_Transition[current_particle_selected];
					t.entityelement[elementID].eleprof.newparticle.fParticle_Speed = Predefined_fParticle_Speed[current_particle_selected];
					t.entityelement[elementID].eleprof.newparticle.fParticle_Opacity = Predefined_fParticle_Opacity[current_particle_selected];

					// and decide on script to use (code reuse, see above)
					LPSTR pParticleName = Predefined_Particle_Name[current_particle_selected].Get();
					if (strnicmp(pParticleName + strlen(pParticleName) - 3, ".pe", 3) == NULL)
					{
						t.entityelement[elementID].eleprof.aimain_s = "particles\\wpe_area.lua";
						t.entityelement[elementID].eleprof.soundset4_s = "wpe_area_properties(2";
						t.entityelement[elementID].eleprof.soundset4_s += "\"";
						t.entityelement[elementID].eleprof.soundset4_s += t.entityelement[elementID].eleprof.newparticle.emittername;
						t.entityelement[elementID].eleprof.soundset4_s += "\",3\"";
						if(t.entityelement[elementID].eleprof.newparticle.bParticle_Show_At_Start==1)
							t.entityelement[elementID].eleprof.soundset4_s += "1";
						else
							t.entityelement[elementID].eleprof.soundset4_s += "0";
						t.entityelement[elementID].eleprof.soundset4_s += "\",0\"0\",0\"0\",0\"0\")";
					}
					else
					{
						t.entityelement[elementID].eleprof.aimain_s = "markers\\particle.lua";
					}

					edit_grideleprof->newparticle = t.entityelement[elementID].eleprof.newparticle;

					bUpdateParticle = true; //Start new effect.
				}

				bPredefinedParticleInit = false; //Setup everything again.

			}
			else
			{
				//Open select particle window.
				bExternal_Entities_Window = true;
				iDisplayLibraryType = 5;
				iLibraryStingReturnToID = window->GetID("New Particle##+");
			}
		}

		ImGui::SameLine();
		bool bDisableButton = true;
		if (current_particle_selected != -1 && current_particle_selected > iPredefinedParticleSetups-1) bDisableButton = false;
		if (ImGui::StyleButtonEx("Delete Particle", ImVec2((particle_w*0.5f) - 4.0f, 0), bDisableButton))
		{
			bPossibleWickedParticleSelectionChanged = true;
			if (current_particle_selected >= iPredefinedParticleSetups)
			{
				strcpy(pref.Saved_Particle_Name[current_particle_selected - iPredefinedParticleSetups], "");
				bPredefinedParticleInit = false; //Setup everything again.
			}
		}
		if (ImGui::IsItemHovered())
		{
			if (bDisableButton == true)
				ImGui::SetTooltip("Cannot delete first 9 default particles");
			else
				ImGui::SetTooltip("Delete selected custom particle");
		}

		ImGui::TextCenter("Particle States");

		bool bLegacyParticle = true;
		LPSTR pParticleName = edit_grideleprof->newparticle.emittername.Get();
		if (strnicmp(pParticleName + strlen(pParticleName) - 3, ".pe", 3) == NULL)
			bLegacyParticle = false;

		bool btmp = edit_grideleprof->newparticle.bParticle_Preview;
		if( ImGui::Checkbox("Preview Particle Effect", &btmp) )
		{
			bPossibleWickedParticleSelectionChanged = true;
			bUpdateParticle = true;
		}
		edit_grideleprof->newparticle.bParticle_Preview = btmp;
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toggle whether the particle effect shows in the editor");

		btmp = edit_grideleprof->newparticle.bParticle_Show_At_Start;
		if (ImGui::Checkbox("Show at start of level", &btmp))
		{
			bUpdateParticle = true;
			if (bLegacyParticle == false)
			{
				t.entityelement[elementID].eleprof.aimain_s = "particles\\wpe_area.lua";
				t.entityelement[elementID].eleprof.soundset4_s = "wpe_area_properties(2";
				t.entityelement[elementID].eleprof.soundset4_s += "\"";
				t.entityelement[elementID].eleprof.soundset4_s += t.entityelement[elementID].eleprof.newparticle.emittername;
				t.entityelement[elementID].eleprof.soundset4_s += "\",3\"";
				if (btmp == 1)
					t.entityelement[elementID].eleprof.soundset4_s += "1";
				else
					t.entityelement[elementID].eleprof.soundset4_s += "0";
				t.entityelement[elementID].eleprof.soundset4_s += "\",0\"0\",0\"0\",0\"0\")";
			}
		}
		edit_grideleprof->newparticle.bParticle_Show_At_Start = btmp;
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toggle whether the particle effect shows at the start of the level");

		if (bLegacyParticle == true)
		{
			// Legacy Particles
			ImGui::TextCenter("Legacy Particle Controls");

			btmp = edit_grideleprof->newparticle.bParticle_Looping_Animation;
			if (ImGui::Checkbox("Looping Animation", &btmp))
			{
				bUpdateParticle = true;
			}
			edit_grideleprof->newparticle.bParticle_Looping_Animation = btmp;
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Choose whether the particle repeats, or plays once only");

			ImGui::TextCenter("Animation Speed");
			ImGui::PushItemWidth(-10);
			int tmpint = edit_grideleprof->newparticle.fParticle_Speed * 100.0f; // 1.0 = normal.
			if (ImGui::MaxSliderInputInt("##Animation Speed", &tmpint, 0, 200, "Animation Speed"))
			{
				bUpdateParticle = true;
			}
			edit_grideleprof->newparticle.fParticle_Speed = (float)tmpint / 100.0f;
			ImGui::PopItemWidth();
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Set the global speed of the particle effect");

			ImGui::TextCenter("Opacity");
			ImGui::PushItemWidth(-10);
			tmpint = edit_grideleprof->newparticle.fParticle_Opacity * 100.0f; // 1.0 = normal.
			if (ImGui::MaxSliderInputInt("##OpacityParticle", &tmpint, 0, 200, "Opacity"))
			{
				bUpdateParticle = true;
			}
			edit_grideleprof->newparticle.fParticle_Opacity = (float)tmpint / 100.0f;
			ImGui::PopItemWidth();
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Set the global opacity of the particle effect");
		}
		else
		{
			// Wicked Particle Properties
			ImGui::TextCenter("Wicked Particle Controls");

			// wicked particle system
			bool bPreviewWPENow = edit_grideleprof->newparticle.bParticle_Preview;
			if (PreviewWPERoot == 0 && bPreviewWPENow)
			{
				PreviewWPERoot = WickedCall_LoadWPE(edit_grideleprof->newparticle.emittername.Get());
				//iAction = 1 Burst all. 2 = Pause. - 3 = Resume. - 4 = Restart - 5 - visible - 6 = not visible. - 7 = pause emit - 8 = resume emit
				void WickedCall_PerformEmitterAction(int iAction, uint32_t emitter_root);
				WickedCall_PerformEmitterAction(1, PreviewWPERoot);
				WickedCall_PerformEmitterAction(4, PreviewWPERoot);
				WickedCall_PerformEmitterAction(5, PreviewWPERoot);
			}

			// test burst mode
			if (ImGui::Button("Trigger Particle Burst##Burst", ImVec2(particle_w, 0)))
			{
				WickedCall_PerformEmitterAction(1, PreviewWPERoot);
			}
		}

		// wicked particle system cleanup
		if (bExternal_Entities_Window == false)
		{
			// but only when not viewing wicked particles in particles library browser 
			bool bPreviewWPENow = edit_grideleprof->newparticle.bParticle_Preview;
			if ((PreviewWPERoot != 0 && !bPreviewWPENow) || (PreviewWPERoot != 0 && bPossibleWickedParticleSelectionChanged == true))
			{
				WickedCall_PerformEmitterAction(6, PreviewWPERoot);
				void DeleteEmitterEffects(uint32_t root);
				DeleteEmitterEffects(PreviewWPERoot);
				PreviewWPERoot = 0;
			}
		}

		sObject* pObject = GetObjectData(t.entityelement[elementID].obj);
		if (bUpdateParticle && elementID > 0 && pObject)
		{
			t.entityelement[elementID].eleprof.newparticle = edit_grideleprof->newparticle;
			if (current_particle_selected >= 0)
			{
				//Update predefined values.
				Predefined_bParticle_Preview[current_particle_selected] = edit_grideleprof->newparticle.bParticle_Preview;
				Predefined_bParticle_Show_At_Start[current_particle_selected] = edit_grideleprof->newparticle.bParticle_Show_At_Start;
				Predefined_bParticle_Looping_Animation[current_particle_selected] = edit_grideleprof->newparticle.bParticle_Looping_Animation;
				Predefined_bParticle_Full_Screen[current_particle_selected] = edit_grideleprof->newparticle.bParticle_Full_Screen;
				Predefined_fParticle_Fullscreen_Duration[current_particle_selected] = edit_grideleprof->newparticle.fParticle_Fullscreen_Duration;
				Predefined_fParticle_Fullscreen_Fadein[current_particle_selected] = edit_grideleprof->newparticle.fParticle_Fullscreen_Fadein;
				Predefined_fParticle_Fullscreen_Fadeout[current_particle_selected] = edit_grideleprof->newparticle.fParticle_Fullscreen_Fadeout;
				Predefined_Particle_Fullscreen_Transition[current_particle_selected] = edit_grideleprof->newparticle.Particle_Fullscreen_Transition;
				Predefined_fParticle_Speed[current_particle_selected] = edit_grideleprof->newparticle.fParticle_Speed;
				Predefined_fParticle_Opacity[current_particle_selected] = edit_grideleprof->newparticle.fParticle_Opacity;
			}

			if (current_particle_selected >= iPredefinedParticleSetups)
			{
				//This is a pref setup so update new settings.
				int icust = current_particle_selected - iPredefinedParticleSetups;
				if (icust >= 0 && icust < 16)
				{
					//pref.Saved_Particle_Name[icust] never change.
					pref.Saved_bParticle_Preview[icust] = edit_grideleprof->newparticle.bParticle_Preview;
					pref.Saved_bParticle_Show_At_Start[icust] = edit_grideleprof->newparticle.bParticle_Show_At_Start;
					pref.Saved_bParticle_Looping_Animation[icust] = edit_grideleprof->newparticle.bParticle_Looping_Animation;
					pref.Saved_bParticle_Full_Screen[icust] = edit_grideleprof->newparticle.bParticle_Full_Screen;
					pref.Saved_fParticle_Fullscreen_Duration[icust] = edit_grideleprof->newparticle.fParticle_Fullscreen_Duration;
					pref.Saved_fParticle_Fullscreen_Fadein[icust] = edit_grideleprof->newparticle.fParticle_Fullscreen_Fadein;
					pref.Saved_fParticle_Fullscreen_Fadeout[icust] = edit_grideleprof->newparticle.fParticle_Fullscreen_Fadeout;
					strcpy(pref.Saved_Particle_Fullscreen_Transition[icust], edit_grideleprof->newparticle.Particle_Fullscreen_Transition.Get());
					pref.Saved_fParticle_Speed[icust] = edit_grideleprof->newparticle.fParticle_Speed;
					pref.Saved_fParticle_Opacity[icust] = edit_grideleprof->newparticle.fParticle_Opacity;
				}
			}

			int iParticleEmitter = t.entityelement[elementID].eleprof.newparticle.emitterid;
			if (iParticleEmitter == -1)
			{
				iParticleEmitter = gpup_loadEffect(t.entityelement[elementID].eleprof.newparticle.emittername.Get(), 0, 0, 0, 1.0);
				gpup_emitterActive(iParticleEmitter, 0);
				t.entityelement[elementID].eleprof.newparticle.emitterid = iParticleEmitter;
			}
			if (iParticleEmitter != -1)
			{
				gpup_setGlobalPosition(iParticleEmitter, t.entityelement[elementID].x, t.entityelement[elementID].y, t.entityelement[elementID].z);
				gpup_resetLocalPosition(iParticleEmitter);
				float fSpeedX, fSpeedY, fSpeedZ;
				gpup_getEmitterSpeedAngleAdjustment(iParticleEmitter, &fSpeedX, &fSpeedY, &fSpeedZ);
				GGVECTOR3 vecSpeedDirection = GGVECTOR3(fSpeedX - 0.5f, fSpeedY - 0.5f, fSpeedZ - 0.5f);
				GGVec3TransformCoord(&vecSpeedDirection, &vecSpeedDirection, &pObject->position.matRotation);
				gpup_setEmitterSpeedAngleAdjustment(iParticleEmitter, 0.5f + vecSpeedDirection.x, 0.5f + vecSpeedDirection.y, 0.5f + vecSpeedDirection.z);
				gpup_setGlobalRotation(iParticleEmitter, t.entityelement[elementID].rx, t.entityelement[elementID].ry, t.entityelement[elementID].rz);
				gpup_setGlobalScale(iParticleEmitter, 100.0f + t.entityelement[elementID].scalex);
				gpup_emitterActive(iParticleEmitter, t.entityelement[elementID].eleprof.newparticle.bParticle_Preview);
				gpup_setEffectAnimationSpeed(iParticleEmitter, t.entityelement[elementID].eleprof.newparticle.fParticle_Speed);
				gpup_setEffectOpacity(iParticleEmitter, t.entityelement[elementID].eleprof.newparticle.fParticle_Opacity);

				if(!t.entityelement[elementID].eleprof.newparticle.bParticle_Looping_Animation)
					gpup_emitterFire(iParticleEmitter);
			}
			edit_grideleprof->newparticle = t.entityelement[elementID].eleprof.newparticle;
		}
	}
	#endif
	else if (t.entityprofile[entid].ismarker == 1)
	{
		//Start Marker.
		//Display icon.
		if (bDisplaySmallIcon)
		{
			if (t.entityprofile[entid].iThumbnailSmall > 0) {
				float w = ImGui::GetContentRegionAvailWidth();
				ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2((w*0.5) - (media_icon_size*0.5), 0.0f));
				ImGui::ImgBtn(t.entityprofile[entid].iThumbnailSmall, ImVec2(media_icon_size, media_icon_size), drawCol_back, drawCol_normal, drawCol_normal, drawCol_normal, -1, 0, 0, 0, true);
			}
		}

		//Name only.
		if(bDisplayName)
			edit_grideleprof->name_s = imgui_setpropertystring2_v2(t.group, edit_grideleprof->name_s.Get(), "Name", t.strarr_s[204].Get(),readonly);


		// DLUA support added here.
		int speech_entries = 0;
		bool bUpdateMainString = false;
		for (int speech_loop = 0; speech_loop < 5; speech_loop++)
			speech_ids[speech_loop] = -1;
		if (fpe_current_loaded_script != fpe_current_selected_script)
		{
			if (PreviewWPERoot != 0)
			{
				//PE: Delete effects.
				WickedCall_PerformEmitterAction(6, PreviewWPERoot);
				void DeleteEmitterEffects(uint32_t root);
				DeleteEmitterEffects(PreviewWPERoot);
				PreviewWPERoot = 0;
				bPreviewWPE = false;
			}

			//Load in lua and check for custom properties.
			cstr script_name = "";
			script_name = "scriptbank\\";
			script_name += edit_grideleprof->aimain_s;

			//Try to parse script.
			ParseLuaScript(lua_grideleprof, script_name.Get());
			fpe_current_loaded_script = fpe_current_selected_script;

			if (lua_grideleprof->PropertiesVariableActive == 1) {
				bUpdateMainString = true;
				fpe_current_loaded_script_has_dlua = true;
			}
			else {
				if (fpe_current_loaded_script_has_dlua) {
					//Reset edit_grideleprof->soundset4_s that contain the dlua calls.
					edit_grideleprof->soundset4_s = "";
					fpe_current_loaded_script_has_dlua = false;
				}
			}
		}

		#ifdef USENEWMEDIASELECTWINDOWS
		if (pref.current_style == 25 || pref.current_style == 3)
		{
			fpe_current_loaded_script_image = PLAYER_START;
		}
		else
		{
			fpe_current_loaded_script_image = PLAYER_START2;
		}
		//Display only no selections.
		ImGui::PushItemWidth(-10);
		float w = ImGui::GetContentRegionAvailWidth() - 16.0f;
		float ImgW = ImageWidth(fpe_current_loaded_script_image);
		float ImgH = ImageHeight(fpe_current_loaded_script_image);
		float fRatio = w / ImgW;
		ImGui::ImgBtn(fpe_current_loaded_script_image, ImVec2(ImgW*fRatio, ImgH*fRatio), drawCol_black, drawCol_normal, drawCol_normal, drawCol_normal, -1, 0, 0, 0, false);
		//ImGui::TextCenter(cDisplayName);
		ImGui::PopItemWidth();
		#endif

		// Markers behaviours
		if (lua_grideleprof->PropertiesVariableActive == 1 || lua_grideleprof->PropertiesVariable.VariableDescription.Len() > 0)
		{
			//"Behaviors"
			if (lua_grideleprof->PropertiesVariableActive == 1) {
				speech_entries = DisplayLuaDescription(lua_grideleprof);
			}
			else {
				if (lua_grideleprof->PropertiesVariable.VariableDescription.Len() > 0) {
					if (edit_grideleprof->aimain_s != "default.lua") //No need to display.
						DisplayLuaDescriptionOnly(lua_grideleprof);
				}
			}
		}

		if (speech_entries > 0)
		{
			// all SPEECH control is moved to this function.
			SpeechControls(speech_entries, bUpdateMainString, edit_grideleprof);
		}

		// Player Has Weapon
		if (t.tflaghasweapon == 1 && t.playercontrol.thirdperson.enabled == 0 && g.quickparentalcontrolmode != 2)
		{
			// weapon selection
			LPSTR pAttachmentTitle = "Weapon";
			edit_grideleprof->hasweapon_s = imgui_setpropertylist2c_v2(t.group, t.controlindex, edit_grideleprof->hasweapon_s.Get(), pAttachmentTitle, t.strarr_s[209].Get(), 1, readonly, false, true, true, 0);
			t.findgun_s = Lower(edit_grideleprof->hasweapon_s.Get()); gun_findweaponindexbyname();
			if (t.foundgunid > 0)
			{
				// hands selection (if using new weapon system)
				if (t.gun[t.foundgunid].newweaponsystem==1)
				{
					pAttachmentTitle = "Hands";
					edit_grideleprof->texaltd_s = imgui_setpropertylist2c_v2(t.group, t.controlindex, edit_grideleprof->texaltd_s.Get(), pAttachmentTitle, "Choose the style of hands for the player", 3, readonly, false, true, true, 0);
				}
				else
				{
					ImGui::TextCenter("(Hands Fixed For Legacy Weapons)");
				}

				// any ammo used
				int iPoolIndex = g.firemodes[t.foundgunid][0].settings.poolindex;
				if (iPoolIndex > 0)
				{
					ImGui::TextCenter("Ammo Quantity");
					ImGui::MaxSliderInputInt("##AmmoQuantity", &edit_grideleprof->quantity, 0, 1000, "Amount of ammo the player starts with");
				}
			}

			// multipliers
			ImGui::TextCenter("Weapon Damage Multiplier");
			int iModifier = edit_grideleprof->weapondamagemultiplier * 100;
			if (ImGui::MaxSliderInputInt("##WeaponDamageM", &iModifier, 1, 1000, "Percent of weapon damage that the player does compared to enemies"))
			{
				edit_grideleprof->weapondamagemultiplier = iModifier * 0.01f;
			}
			ImGui::TextCenter("Melee Damage Multiplier");
			iModifier = edit_grideleprof->meleedamagemultiplier * 100;
			if (ImGui::MaxSliderInputInt("##MeleeDamageM", &iModifier, 1, 1000, "Percent of melee damage that the player does compared to enemies"))
			{
				edit_grideleprof->meleedamagemultiplier = iModifier * 0.01f;
			}

			// configure weapon slots for player (functionality exists from Classic)
			ImGui::TextCenter("Preferred Weapon Slots");
			for (int key = 1; key <= 9; key++)
			{
				char pLabel[32];
				sprintf(pLabel, "Key %d", key);
				char pLabelTokenName[32];
				sprintf(pLabelTokenName, "weapprefkey1%d", key);
				ImGui::Text(pLabel);
				ImGui::SameLine();
				cstr slot_s = "";

				for (int gunid = 1; gunid <= g.gunmax; gunid++)
				{
					if (t.weaponSlotPreferrenceSettings[key - 1] > 0 && t.weaponSlotPreferrenceSettings[key - 1] == gunid)
					{
						slot_s = t.gun[gunid].name_s;
						break;
					}
				}

				cstr lastslot = slot_s;
				slot_s = imgui_setpropertylist2c_v2(t.group, t.controlindex, slot_s.Get(), pLabelTokenName, t.strarr_s[209].Get(), 61, readonly, false, true, true, 0);
				if (stricmp(slot_s.Get(), lastslot.Get()) != NULL)
				{
					// assign a new preference
					if (stricmp(slot_s.Get(), "") == NULL) // when "No Preference" option is selected slot_s is set to ""
					{
						t.weaponSlotPreferrenceSettings[key - 1] = 0;
						t.weaponslot[key].pref = 0;
					}
					else
					{
						int iFoundGunID = -1;
						for (int gunid = 1; gunid <= g.gunmax; gunid++)
						{
							if (stricmp(slot_s.Get(), t.gun[gunid].name_s.Get()) == NULL)
							{
								iFoundGunID = gunid;
								break;
							}
						}
						if (iFoundGunID != -1)
						{
							// if not a slot blocker
							if (stricmp(t.gun[iFoundGunID].name_s.Get(), "Slot Not Used") != NULL)
							{
								// erase old preference if already assigned
								for (int n = 0; n < 9; n++)
								{
									if (n != (key - 1) && t.weaponSlotPreferrenceSettings[n] == iFoundGunID)
									{
										t.weaponSlotPreferrenceSettings[n] = 0;
										t.weaponslot[1 + n].pref = 0;
									}
								}
							}

							// add new preference
							t.weaponSlotPreferrenceSettings[key - 1] = iFoundGunID;
							t.weaponslot[key].pref = iFoundGunID;
						}
					}

					// and save out the new layout
					extern void gun_gatherslotorder_save (void);
					gun_gatherslotorder_save();
				}
			}
		}
		ImGui::Spacing();

		ImGui::TextCenter("Player Settings");
		ImGui::TextCenter("Speed");
		ImGui::PushItemWidth(-10);
		ImGui::MaxSliderInputInt("##Movement SpeedSimpleInput", &edit_grideleprof->speed, 1, 500, 0);
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Set Player Speed");
		if (t.playercontrol.thirdperson.enabled == 1) t.tanimspeed_f = t.entityelement[t.playercontrol.thirdperson.charactere].eleprof.animspeed;
		else t.tanimspeed_f = edit_grideleprof->animspeed;
		ImGui::PopItemWidth();

		ImGui::TextCenter("Swimming Speed");
		ImGui::PushItemWidth(-10);
		ImGui::MaxSliderInputInt("##SwimSpeedSimpleInput", &edit_grideleprof->iSwimSpeed, 1, 100, "Modifies how much distance is travelled with each swimming stroke");
		ImGui::PopItemWidth();
	
		//edit_grideleprof->lives = atol(imgui_setpropertystring2_v2(t.group, Str(edit_grideleprof->lives), t.strarr_s[452].Get(), "Specifies how many lives the player starts with. Enter zero for infinite lives.", readonly));
		ImGui::TextCenter("Lives");
		ImGui::PushItemWidth(-10);
		ImGui::MaxSliderInputInt("##PlayerLivesSimpleInput", &edit_grideleprof->lives, 0, 10, "Specifies how many lives the player starts with. Enter zero for infinite lives");
		ImGui::PopItemWidth();

		ImGui::TextCenter("Health");
		static int iPlayerNormalStrength = 500;
		int iPlayerInvincible = 0;
		if (edit_grideleprof->strength == 99999) iPlayerInvincible = 1;
		if (iPlayerInvincible == 0)
		{
			ImGui::PushItemWidth(-10);
			ImGui::MaxSliderInputInt("##PlayerHealthSimpleInput", &edit_grideleprof->strength, 1, 1000, 0);
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Set Player Health");
			ImGui::PopItemWidth();
		}

		int iLastPlayerInvincible = iPlayerInvincible;
		iPlayerInvincible = imgui_setpropertylist2_v2(t.group, t.controlindex, Str(iPlayerInvincible), "Invulnerable", "Controls whether the player has infinite health", 0, readonly);
		if (iLastPlayerInvincible != iPlayerInvincible)
		{
			if (iPlayerInvincible == 1)
			{
				iPlayerNormalStrength = edit_grideleprof->strength;
				edit_grideleprof->strength = 99999;
			}
			else
			{
				edit_grideleprof->strength = iPlayerNormalStrength;
			}
		}

		// health regeneration
		ImGui::TextCenter("Health Regeneration");
		ImGui::TextCenter("Amount");
		ImGui::MaxSliderInputInt("##RegenRate", &t.playercontrol.regenrate, 0, 1000, "Sets the amount of health to be regenerated at a time");
		ImGui::TextCenter("Rate");
		ImGui::MaxSliderInputInt("##RegenSpeed", &t.playercontrol.regenspeed, 0, 1000, "Sets how often health will be regenerated (in milliseconds)");
		ImGui::TextCenter("Delay");
		ImGui::MaxSliderInputInt("##RegenDelay", &t.playercontrol.regendelay, 0, 5000, "Sets the amount of time to wait  (in milliseonds) after taking damage, until health starts regenerating");
		ImGui::Spacing();
		
		ImGui::TextCenter("Effects");
		bool bHeartbeatSound = edit_grideleprof->perentityflags & 1;
		bHeartbeatSound = !bHeartbeatSound;
		bool bScreenBlood = edit_grideleprof->perentityflags & (1 << 1);
		bScreenBlood = !bScreenBlood;
		if (ImGui::Checkbox("Heartbeat Sound", &bHeartbeatSound))
		{
		}
		if(ImGui::IsItemHovered()) ImGui::SetTooltip("Controls whether a heartbeat sound is looped when the player is hurt");
		if (ImGui::Checkbox("Screen Blood", &bScreenBlood))
		{
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Controls whether blood is rendered on screen when the player is hurt");

		DWORD flags = !bHeartbeatSound;
		int iScreenBloodOff = !bScreenBlood;
		flags |= (iScreenBloodOff << 1);
		edit_grideleprof->perentityflags = flags;

		bool bDamageIndicator = t.huddamage.damageindicatoron;
		if (ImGui::Checkbox("Damage Indicator", &bDamageIndicator))
		{
			t.huddamage.damageindicatoron = bDamageIndicator;
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Sets whether a damage indicator appears when the player gets hurt");

		bool bFlashlightDisabled = edit_grideleprof->usespotlighting;
		if (ImGui::Checkbox("Flashlight Disabled", &bFlashlightDisabled))
		{
			edit_grideleprof->usespotlighting = bFlashlightDisabled;
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Sets whether the flashlight is disabled for the player");

		// additional sound control for player start marker
		edit_grideleprof->soundset_s = imgui_setpropertylist2c_v2(t.group, t.controlindex, edit_grideleprof->soundset_s.Get(), "Preferred Voice", "Choose the style of voice for the player", 32, readonly, false, false, false, 0);
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Choose the style of voice for the player");

		edit_grideleprof->soundset1_s = imgui_setpropertyfile2_v2(t.group, edit_grideleprof->soundset1_s.Get(), "Hard Impact", "", "audiobank\\", readonly);
		//if (ImGui::IsItemHovered()) ImGui::SetTooltip("Choose an optional sound when player strikes a non-character, typically a hard surface");
		edit_grideleprof->soundset2_s = imgui_setpropertyfile2_v2(t.group, edit_grideleprof->soundset2_s.Get(), "Soft Impact", "", "audiobank\\", readonly);
		//if (ImGui::IsItemHovered()) ImGui::SetTooltip("Choose an optional sound when player strieks a character, typically soft and fleshy");
	}
	else
	{
		//#################
		//#### Objects ####
		//################# 

		t.tokay = 1;
		if (ObjectExist(g.entitybankoffset + entid) == 1)
		{
			if (GetNumberOfFrames(g.entitybankoffset + entid) > 0)
			{
				t.tokay = 0;
			}
		}

		int speech_entries = 0;
		bool bUpdateMainString = false;

		for (int speech_loop = 0; speech_loop < 5; speech_loop++)
			speech_ids[speech_loop] = -1;

		//health.lua
		cstr aimain = edit_grideleprof->aimain_s.Lower();
		//new: trigger anyting not a marker.
		if (t.entityprofile[entid].ismarker == 0 || t.entityprofile[entid].ismarker == 12) // || aimain == "key.lua" || aimain == "objects\\key.lua" || aimain == "door.lua" || aimain == "default.lua" || aimain == "health.lua" || aimain == "pickuppable.lua" ) ) 
		{
			//"Name"
			//Display icon.
			if (bDisplaySmallIcon)
			{
				if (t.entityprofile[entid].iThumbnailSmall > 0) {
					float w = ImGui::GetContentRegionAvailWidth();
					ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2((w*0.5) - (media_icon_size*0.5), 0.0f));
					ImGui::ImgBtn(t.entityprofile[entid].iThumbnailSmall, ImVec2(media_icon_size, media_icon_size), drawCol_back, drawCol_normal, drawCol_normal, drawCol_normal, -1, 0, 0, 0, true);
				}
			}
			if(bDisplayName)
				edit_grideleprof->name_s = imgui_setpropertystring2_v2(t.group, edit_grideleprof->name_s.Get(), "Name", t.strarr_s[204].Get(),readonly);

			// Object behaviours "Behaviors"
			// scan OBJECTS folder for complete list of script
			std::vector<cstr> scriptList_s; scriptList_s.clear();
			std::vector<cstr> scriptListTitle_s; scriptListTitle_s.clear();
			cstr oldDir_s = GetDir();
			SetDir(g.fpscrootdir_s.Get());
			if(t.entityprofile[entid].ismarker == 12)
				SetDir("Files\\scriptbank\\global");
			else
				SetDir("Files\\scriptbank\\objects");
			ChecklistForFiles();
			for (int f = 1; f <= ChecklistQuantity(); f++)
			{
				cstr tfile_s = ChecklistString(f);
				LPSTR pFilename = tfile_s.Get();
				if (tfile_s != "." && tfile_s != "..")
				{
					if (strnicmp(pFilename + strlen(pFilename) - 4, ".lua", 4) == NULL)
					{
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

						// add script and title to list
						if (t.entityprofile[entid].ismarker == 12)
							scriptList_s.push_back(cstr("global\\") + tfile_s);
						else
							scriptList_s.push_back(cstr("objects\\") + tfile_s);
						scriptListTitle_s.push_back(cstr(pTitleName));
					}
				}
			}
			scriptList_s.push_back(cstr(""));
			scriptListTitle_s.push_back(cstr("Custom"));
			SetDir(oldDir_s.Get());

			// and create items list
			static int g_scriptobjects_item_count = 0;
			static char** g_scriptobjects_items = NULL;
			if (g_scriptobjects_item_count != scriptList_s.size())
			{
				if (g_scriptobjects_items)
				{
					for (int i = 0; i < g_scriptobjects_item_count; i++) SAFE_DELETE(g_scriptobjects_items[i]);
					SAFE_DELETE(g_scriptobjects_items);
				}
				g_scriptobjects_item_count = scriptList_s.size();
				g_scriptobjects_items = new char*[g_scriptobjects_item_count];
				for (int i = 0; i < g_scriptobjects_item_count; i++)
				{
					g_scriptobjects_items[i] = new char[256];
					strcpy(g_scriptobjects_items[i], scriptListTitle_s[i].Get());
				}
			}

			// find selection
			int item_current_type_selection = g_scriptobjects_item_count - 1; //Default Custom.
			for (int i = 0; i < g_scriptobjects_item_count - 1; i++)
			{
				// with workshop items updating core scripts, need to check entire path now!
				//if (pestrcasestr(edit_grideleprof->aimain_s.Get(), scriptList_s[i].Get()))
				if(stricmp (edit_grideleprof->aimain_s.Get(), scriptList_s[i].Get()) == NULL )
				{
					item_current_type_selection = i;
					break;
				}
			}

			if (fpe_current_loaded_script != item_current_type_selection)
			{
				if (PreviewWPERoot != 0)
				{
					//PE: Delete effects.
					WickedCall_PerformEmitterAction(6, PreviewWPERoot);
					void DeleteEmitterEffects(uint32_t root);
					DeleteEmitterEffects(PreviewWPERoot);
					PreviewWPERoot = 0;
					bPreviewWPE = false;
				}

				//Load in lua and check for custom properties.
				cstr script_name_appendage = "";
				if (item_current_type_selection < g_scriptobjects_item_count - 1) //PE: Need to check for custom
					script_name_appendage += scriptList_s[item_current_type_selection];
				else
					script_name_appendage += edit_grideleprof->aimain_s;
				cstr script_name = "";
				script_name = "scriptbank\\";
				script_name += script_name_appendage;

				#ifdef USENEWMEDIASELECTWINDOWS
				fpe_current_loaded_script_image = PROPERTIES_CACHE_ICONS + fpe_current_loaded_script_image_count;
				if (fpe_current_loaded_script_image_count++ > 10) fpe_current_loaded_script_image_count = 0;
				std::string sImgName = Left(script_name.Get(), Len(script_name.Get()) - 4);
				if (pref.current_style == 25 || pref.current_style == 3)
					sImgName += ".png";
				else
					sImgName += "2.png";
				image_setlegacyimageloading(true);
				LoadImage((char *)sImgName.c_str(), fpe_current_loaded_script_image);
				image_setlegacyimageloading(false);
				if (!GetImageExistEx(fpe_current_loaded_script_image))
				{
					if (!(pref.current_style == 25 || pref.current_style == 3))
					{
						sImgName = Left(script_name.Get(), Len(script_name.Get()) - 4);
						sImgName += ".png";
						image_setlegacyimageloading(true);
						LoadImage((char *)sImgName.c_str(), fpe_current_loaded_script_image);
						image_setlegacyimageloading(false);
						if (!GetImageExistEx(fpe_current_loaded_script_image))
						{
							fpe_current_loaded_script_image = FILETYPE_SCRIPT;
						}
					}
					else
						fpe_current_loaded_script_image = FILETYPE_SCRIPT;
				}
				#endif

				//Try to parse script.
				ParseLuaScriptWithElementID(lua_grideleprof, script_name.Get(),t.entityelement[elementID].obj);
				fpe_current_loaded_script = item_current_type_selection;

				if (lua_grideleprof->PropertiesVariableActive == 1)
				{
					bUpdateMainString = true;
					fpe_current_loaded_script_has_dlua = true;
				}
				else
				{
					if (fpe_current_loaded_script_has_dlua)
					{
						//Reset edit_grideleprof->soundset4_s that contain the dlua calls.
						edit_grideleprof->soundset4_s = "";
						fpe_current_loaded_script_has_dlua = false;
					}
				}
			}

			if (t.entityprofile[entid].bIsDecal)
			{
				if (elementID > 0)
				{
					int obj = t.entityelement[elementID].obj;
					ImGui::TextCenter("Decal Speed");
					ImGui::PushItemWidth(-10);
					int tmpint = t.entityelement[elementID].fDecalSpeed * 100.0; // 1.0 = normal.
					if (ImGui::MaxSliderInputInt("##Decal Speed", &tmpint, 1, 200, "Decal Speed"))
					{
						t.entityelement[elementID].fDecalSpeed = (float)tmpint / 100.0;
						if(obj > 0)
							SetupDecalObject(obj, elementID);
					}
					t.entityelement[elementID].fDecalSpeed = (float)tmpint / 100.0;
					ImGui::PopItemWidth();
					if (ImGui::IsItemHovered()) ImGui::SetTooltip("Set the speed of the decal effect");

					ImGui::TextCenter("Decal Opacity");
					ImGui::PushItemWidth(-10);
					tmpint = t.entityelement[elementID].fDecalOpacity * 100.0; // 1.0 = normal.
					if (ImGui::MaxSliderInputInt("##DecalOpacity", &tmpint, 0, 100, "Decal Opacity"))
					{
						if (tmpint > 100) tmpint = 100;
						if (tmpint < 0) tmpint = 0;
						t.entityelement[elementID].fDecalOpacity = (float)tmpint / 100.0;
						if (obj > 0)
							SetupDecalObject(obj, elementID);
					}
					t.entityelement[elementID].fDecalOpacity = (float)tmpint / 100.0;
					ImGui::PopItemWidth();
					if (ImGui::IsItemHovered()) ImGui::SetTooltip("Set the opacity of the decal effect");
				}
			}

			#ifdef USENEWMEDIASELECTWINDOWS
			ImGui::PushItemWidth(-10);
			float w = ImGui::GetContentRegionAvailWidth() - 16.0f;
			float ImgW = ImageWidth(fpe_current_loaded_script_image);
			float ImgH = ImageHeight(fpe_current_loaded_script_image);
			float fRatio = w / ImgW;

			if (!bHideIcon)
			{
				ImGuiWindow* window = ImGui::GetCurrentWindow();
				if (iSelectedLibraryStingReturnID == window->GetID("ScriptSelector##+"))
				{
					//Update Script.
					if (sSelectedLibrarySting != "")
					{
						edit_grideleprof->aimain_s = sSelectedLibrarySting;
						sSelectedLibrarySting = "";
						iSelectedLibraryStingReturnID = -1; //disable.
						fpe_current_loaded_script = -1; //Reload image and DLUA.
						if (!pestrcasestr(edit_grideleprof->aimain_s.Get(), "default.lua"))
						{
							//PE: When selecting a script, disable static so script will run.
							if (elementID > 0) t.entityelement[elementID].staticflag = 0;
						}
					}
				}
				//if (ImGui::ImgBtn(fpe_current_loaded_script_image, ImVec2(ImgW*fRatio, ImgH*fRatio), drawCol_back, drawCol_normal, drawCol_normal, drawCol_normal, -1, 0, 0, 0, true))
				if (ImGui::ImgBtn(fpe_current_loaded_script_image, ImVec2(ImgW * fRatio, ImgH * fRatio), drawCol_black, drawCol_normal, drawCol_normal, drawCol_normal, -1, 0, 0, 0, false))
				{
					//Select script.
					if (t.entityprofile[entid].bIsDecal)
						sStartLibrarySearchString = "Decal";
					else
					{
						if (t.entityprofile[entid].ismarker == 12)
							sStartLibrarySearchString = "global";
						else
							sStartLibrarySearchString = "Objects";
					}
					iLastDisplayLibraryType = -1;
					bExternal_Entities_Window = true;
					iDisplayLibraryType = 4;
					iLibraryStingReturnToID = window->GetID("ScriptSelector##+");
					if (edit_grideleprof->aimain_s.Len() > 0)
						sMakeDefaultSelecting = edit_grideleprof->aimain_s;
				}
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Select Object Behavior");
			}
			ImGui::TextCenter(cDisplayName);
			ImGui::PopItemWidth();
			#else
			ImGui::PushItemWidth(-10);
			if (ImGui::Combo("##Behaviours2SimpleInput", &item_current_type_selection, g_scriptobjects_items, g_scriptobjects_item_count, 20))
			{
				if (item_current_type_selection >= 0) {
					edit_grideleprof->aimain_s = scriptList_s[item_current_type_selection];
					//PE: When selecting a script, disable static so script will run.
					if (elementID > 0) t.entityelement[elementID].staticflag = 0;
				}
				else
					edit_grideleprof->aimain_s = "default.lua";
				aimain = edit_grideleprof->aimain_s.Lower();
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Select Object Behavior");
			ImGui::PopItemWidth();
			#endif

			#ifndef USENEWMEDIASELECTWINDOWS
			if (item_current_type_selection == g_scriptobjects_item_count - 1)
			{
				//Custom Behaviours , display directly.
				std::string ms = t.strarr_s[417].Get();
				ms = "Script";
				cstr aim = edit_grideleprof->aimain_s;
				edit_grideleprof->aimain_s = imgui_setpropertyfile2_v2(t.group, edit_grideleprof->aimain_s.Get(), (char *)ms.c_str(), t.strarr_s[207].Get(), pAIRoot,readonly);
				aimain = edit_grideleprof->aimain_s.Lower();
				if (aim != edit_grideleprof->aimain_s)
				{
					fpe_current_loaded_script = -1;
					//PE: When selecting a script, disable static so script will run.
					if (elementID > 0) t.entityelement[elementID].staticflag = 0;
				}
			}
			#endif

			if (lua_grideleprof->PropertiesVariableActive == 1)
			{
				speech_entries = DisplayLuaDescription(lua_grideleprof);
			}
			else
			{
				if (lua_grideleprof->PropertiesVariable.VariableDescription.Len() > 0)
				{
					if (edit_grideleprof->aimain_s != "default.lua") //No need to display.
						DisplayLuaDescriptionOnly(lua_grideleprof);
				}
			}


			if (speech_entries > 0)
			{
				// all SPEECH control is moved to this function.
				SpeechControls(speech_entries, bUpdateMainString, edit_grideleprof);
			}
		}
		else
		{
			//PE: Support markers > 1 here.
			if (t.entityprofile[entid].ismarker > 1) 
			{
				//ismarker = 1 has its own function.
				//DLUA support added here.
				bool bUpdateMainString = false;
				for (int speech_loop = 0; speech_loop < 5; speech_loop++)
					speech_ids[speech_loop] = -1;
				if (fpe_current_loaded_script != fpe_current_selected_script) 
				{
					if (PreviewWPERoot != 0)
					{
						//PE: Delete effects.
						WickedCall_PerformEmitterAction(6, PreviewWPERoot);
						void DeleteEmitterEffects(uint32_t root);
						DeleteEmitterEffects(PreviewWPERoot);
						PreviewWPERoot = 0;
						bPreviewWPE = false;
					}

					//Load in lua and check for custom properties.
					cstr script_name = "";
					//if (strnicmp(edit_grideleprof->aimain_s.Get(), "projectbank", 11) != NULL) 
					script_name = "scriptbank\\";
					script_name += edit_grideleprof->aimain_s;

					#ifdef USENEWMEDIASELECTWINDOWS
					fpe_current_loaded_script_image = PROPERTIES_CACHE_ICONS + fpe_current_loaded_script_image_count;
					if (fpe_current_loaded_script_image_count++ > 10) fpe_current_loaded_script_image_count = 0;
					std::string sImgName = Left(script_name.Get(), Len(script_name.Get()) - 4);
					if (pref.current_style == 25 || pref.current_style == 3)
						sImgName += ".png";
					else
						sImgName += "2.png";
					image_setlegacyimageloading(true);
					LoadImage((char *)sImgName.c_str(), fpe_current_loaded_script_image);
					image_setlegacyimageloading(false);
					if (!GetImageExistEx(fpe_current_loaded_script_image))
					{
						if (!(pref.current_style == 25 || pref.current_style == 3))
						{
							sImgName = Left(script_name.Get(), Len(script_name.Get()) - 4);
							sImgName += ".png";
							image_setlegacyimageloading(true);
							LoadImage((char *)sImgName.c_str(), fpe_current_loaded_script_image);
							image_setlegacyimageloading(false);
							if (!GetImageExistEx(fpe_current_loaded_script_image))
							{
								fpe_current_loaded_script_image = FILETYPE_SCRIPT;
							}
						}
						else
							fpe_current_loaded_script_image = FILETYPE_SCRIPT;
					}
					#endif

					//Try to parse script.
					ParseLuaScript(edit_grideleprof, script_name.Get());
					fpe_current_loaded_script = fpe_current_selected_script;

					if (edit_grideleprof->PropertiesVariableActive == 1) {
						bUpdateMainString = true;
						fpe_current_loaded_script_has_dlua = true;
					}
					else 
					{
						if (fpe_current_loaded_script_has_dlua) 
						{
							//Reset edit_grideleprof->soundset4_s that contain the dlua calls.
							edit_grideleprof->soundset4_s = "";
							fpe_current_loaded_script_has_dlua = false;
						}
					}
				}

				#ifdef USENEWMEDIASELECTWINDOWS
				//Display only no selections.
				ImGui::PushItemWidth(-10);
				float w = ImGui::GetContentRegionAvailWidth() - 16.0f;
				float ImgW = ImageWidth(fpe_current_loaded_script_image);
				float ImgH = ImageHeight(fpe_current_loaded_script_image);
				float fRatio = w / ImgW;

				if (!bHideIcon)
				{
					//LB: Do allow TRIGGER ZONE script to change
					ImGuiWindow* window = ImGui::GetCurrentWindow();
					if (t.entityprofile[entid].ismarker == 3 && t.entityprofile[entid].trigger.stylecolor == 2)
					{
						if (iSelectedLibraryStingReturnID == window->GetID("ScriptSelector##+"))
						{
							//Update Script.
							if (sSelectedLibrarySting != "")
							{
								edit_grideleprof->aimain_s = sSelectedLibrarySting;
								sSelectedLibrarySting = "";
								iSelectedLibraryStingReturnID = -1;
								fpe_current_loaded_script = -1;
							}
						}
					}
					if (ImGui::ImgBtn(fpe_current_loaded_script_image, ImVec2(ImgW * fRatio, ImgH * fRatio), drawCol_black, drawCol_normal, drawCol_normal, drawCol_normal, -1, 0, 0, 0, false))
					{
						//LB: But do allow TRIGGER ZONE script to change
						if (t.entityprofile[entid].ismarker == 3 && t.entityprofile[entid].trigger.stylecolor == 2)
						{
							//Select script.
							sStartLibrarySearchString = "Markers";
							iLastDisplayLibraryType = -1;
							bExternal_Entities_Window = true;
							iDisplayLibraryType = 4;
							iLibraryStingReturnToID = window->GetID("ScriptSelector##+");
							if (edit_grideleprof->aimain_s.Len() > 0)
							{
								sMakeDefaultSelecting = edit_grideleprof->aimain_s;
							}
						}
					}
				}
				ImGui::TextCenter(cDisplayName);
				ImGui::PopItemWidth();
				#endif


				if (edit_grideleprof->PropertiesVariableActive == 1 || edit_grideleprof->PropertiesVariable.VariableDescription.Len() > 0)
				{
					//ImGui::Indent(10);

					if (edit_grideleprof->PropertiesVariableActive == 1) {
						speech_entries = DisplayLuaDescription(edit_grideleprof);
					}
					else {
						if (edit_grideleprof->PropertiesVariable.VariableDescription.Len() > 0) {
							DisplayLuaDescriptionOnly(edit_grideleprof);
						}
					}

					//ImGui::Indent(-10);
				}

				if (speech_entries > 0)
				{
					// all SPEECH control is moved to this function.
					SpeechControls(speech_entries, bUpdateMainString, edit_grideleprof);
				}

			}

		}
	}

	// All objects need 'certain fields' as pretty commmon  to have it for the script
	//PE: Markers also have these fields. So anything with dlua active.
	if (edit_grideleprof->PropertiesVariable.VariableDescription.Len() > 0) //t.entityprofile[entid].ismarker == 0) // so as not to interfere with markers
	{
		bool bSound0Mentioned = false;
		bool bSound1Mentioned = false;
		bool bSound2Mentioned = false;
		bool bSound3Mentioned = false;
		bool bSound4Mentioned = false;
		bool bSound5Mentioned = false;
		bool bSound6Mentioned = false;
		bool bVideoSlotMentioned = false;
		bool bIfUsedMentioned = false;
		bool bUseKeyMentioned = false;
		bool bShootingWeaponMentioned = false;
		bool bMeleeWeaponMentioned = false;
		bool bUnarmedMentioned = false;
		int iAnimationSetMentioned = 0;
		char pCaptureAnyScriptDesc[10240 + (80 * 300) + (80 * 300)];

		strcpy(pCaptureAnyScriptDesc, edit_grideleprof->PropertiesVariable.VariableDescription.Get());
		for (int i = 0; i < edit_grideleprof->PropertiesVariable.iVariables; i++)
		{
			strcat(pCaptureAnyScriptDesc, edit_grideleprof->PropertiesVariable.VariableSectionDescription[i].Get());
		}
		for (int i = 0; i < edit_grideleprof->PropertiesVariable.iVariables; i++)
		{
			strcat(pCaptureAnyScriptDesc, edit_grideleprof->PropertiesVariable.VariableSectionEndDescription[i].Get());
		}
		if (strstr(pCaptureAnyScriptDesc, "<Sound0>") != 0) bSound0Mentioned = true;
		if (strstr(pCaptureAnyScriptDesc, "<Sound1>") != 0) bSound1Mentioned = true;
		if (strstr(pCaptureAnyScriptDesc, "<Sound2>") != 0) bSound2Mentioned = true;
		if (strstr(pCaptureAnyScriptDesc, "<Sound3>") != 0) bSound3Mentioned = true;
		if (strstr(pCaptureAnyScriptDesc, "<Sound4>") != 0) bSound4Mentioned = true;
		if (strstr(pCaptureAnyScriptDesc, "<Sound5>") != 0) bSound5Mentioned = true;
		if (strstr(pCaptureAnyScriptDesc, "<Sound6>") != 0) bSound6Mentioned = true;
		if (strstr(pCaptureAnyScriptDesc, "<Video Slot>") != 0) bVideoSlotMentioned = true;
		if (strstr(pCaptureAnyScriptDesc, "<If Used>") != 0) bIfUsedMentioned = true;
		if (strstr(pCaptureAnyScriptDesc, "<Use Key>") != 0) bUseKeyMentioned = true;
		if (strstr(pCaptureAnyScriptDesc, "<Shooting Weapon>") != 0) bShootingWeaponMentioned = true;
		if (strstr(pCaptureAnyScriptDesc, "<Melee Weapon>") != 0) bMeleeWeaponMentioned = true;
		if (strstr(pCaptureAnyScriptDesc, "<Any Weapon>") != 0) { bShootingWeaponMentioned = true; bMeleeWeaponMentioned = true; }
		if (strstr(pCaptureAnyScriptDesc, "<Unarmed>") != 0) bUnarmedMentioned = true;
		if (strstr(pCaptureAnyScriptDesc, "<Soldier Animations>") != 0) iAnimationSetMentioned = 1;
		if (strstr(pCaptureAnyScriptDesc, "<Melee Animations>") != 0) iAnimationSetMentioned = 2;
		if (strstr(pCaptureAnyScriptDesc, "<Zombie Animations>") != 0) iAnimationSetMentioned = 3;
		if (strstr(pCaptureAnyScriptDesc, "<Default Animations>") != 0) iAnimationSetMentioned = 4;		

		if (bSound0Mentioned || bSound1Mentioned || bSound2Mentioned || bSound3Mentioned || bSound4Mentioned || bSound5Mentioned || bVideoSlotMentioned || bIfUsedMentioned || bUseKeyMentioned || bShootingWeaponMentioned || bMeleeWeaponMentioned || iAnimationSetMentioned>0)
		{
			if (bVideoSlotMentioned == true)
			{
				#define VIDEOFILEID (PROPERTIES_CACHE_ICONS+997)
				static cstr videofile = "";
				static int videofile_preview_id = 0;
				if (edit_grideleprof->soundset1_s != videofile)
				{
					//Load new image preview.
					videofile_preview_id = 0;
					if (edit_grideleprof->soundset1_s != "")
					{
						videofile_preview_id = VIDEOFILEID;

						std::string stmp = edit_grideleprof->soundset1_s.Get();
						replaceAll(stmp, "videobank", ""); //Video thumbs stored without videobank.
						replaceAll(stmp, "\\\\", "\\"); //Remove double backslash.

						bool CreateBackBufferCacheName(char *file, int width, int height);
						extern cstr BackBufferCacheName;
						CreateBackBufferCacheName((char *)stmp.c_str(), 512, 288);
						if (!(strlen(Storyboard.gamename) > 0 && strlen(Storyboard.customprojectfolder) > 0))
						{
							GG_SetWritablesToRoot(true);
						}
						SetMipmapNum(1); //PE: mipmaps not needed.
						image_setlegacyimageloading(true);
						if (FileExist(BackBufferCacheName.Get()))
						{
							LoadImage((char *)BackBufferCacheName.Get(), videofile_preview_id);
						}
						image_setlegacyimageloading(false);
						SetMipmapNum(-1);
						GG_SetWritablesToRoot(false);
						if (!GetImageExistEx(VIDEOFILEID))
						{
							videofile_preview_id = 0;
						}
					}
					videofile = edit_grideleprof->soundset1_s;
				}
				edit_grideleprof->soundset1_s = imgui_setpropertyfile2_v2(t.group, edit_grideleprof->soundset1_s.Get(), "Video Slot", "Choose a movie file (mp4 format) to play when the player enters this zone", "videobank\\", readonly);
				if (videofile_preview_id > 0 && GetImageExistEx(videofile_preview_id))
				{
					extern ImVec4 drawCol_back;
					extern ImVec4 drawCol_normal;
					extern ImVec4 drawCol_hover;
					extern ImVec4 drawCol_Down;

					float w = ImGui::GetContentRegionAvailWidth();
					float iwidth = w;
					float ImgW = ImageWidth(videofile_preview_id);
					float ImgH = ImageHeight(videofile_preview_id);
					float fHighRatio = ImgH / ImgW;
					ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2((w*0.5) - (iwidth*0.5), 0.0f));
					ImGui::ImgBtn(videofile_preview_id, ImVec2(iwidth - 18.0f, (iwidth - 18.0f) * fHighRatio), drawCol_back, drawCol_normal, drawCol_normal, drawCol_normal, -1, 0, 0, 0, true);
				}
			}
			if (bSound0Mentioned == true && bVideoSlotMentioned == false) edit_grideleprof->soundset_s = imgui_setpropertyfile2_v2(t.group, edit_grideleprof->soundset_s.Get(), "Sound0", t.strarr_s[253].Get(), "audiobank\\",readonly);
			if (bSound1Mentioned == true) edit_grideleprof->soundset1_s = imgui_setpropertyfile2_v2(t.group, edit_grideleprof->soundset1_s.Get(), "Sound1", t.strarr_s[254].Get(), "audiobank\\", readonly);
			if (bSound2Mentioned == true) edit_grideleprof->soundset2_s = imgui_setpropertyfile2_v2(t.group, edit_grideleprof->soundset2_s.Get(), "Sound2", t.strarr_s[254].Get(), "audiobank\\", readonly);
			if (bSound3Mentioned == true) edit_grideleprof->soundset3_s = imgui_setpropertyfile2_v2(t.group, edit_grideleprof->soundset3_s.Get(), "Sound3", t.strarr_s[254].Get(), "audiobank\\", readonly);
			if (bSound4Mentioned == true) edit_grideleprof->soundset4a_s = imgui_setpropertyfile2_v2(t.group, edit_grideleprof->soundset4a_s.Get(), "Sound4", t.strarr_s[254].Get(), "audiobank\\", readonly);
			if (bSound5Mentioned == true) edit_grideleprof->soundset5_s = imgui_setpropertyfile2_v2(t.group, edit_grideleprof->soundset5_s.Get(), "Sound5", t.strarr_s[254].Get(), "audiobank\\", readonly);
			if (bSound6Mentioned == true) edit_grideleprof->soundset6_s = imgui_setpropertyfile2_v2(t.group, edit_grideleprof->soundset6_s.Get(), "Sound6", t.strarr_s[254].Get(), "audiobank\\", readonly);
			if (bIfUsedMentioned == true)
			{
				if (t.entityprofile[entid].ischaracter != 1)
				{
					// but not characters which now use IFUSED to contain LOOT information
					edit_grideleprof->ifused_s = imgui_setpropertystring2_v2(t.group, edit_grideleprof->ifused_s.Get(), t.strarr_s[437].Get(), t.strarr_s[226].Get(), readonly);
				}
			}
			if (bUseKeyMentioned == true) edit_grideleprof->usekey_s = imgui_setpropertystring2_v2(t.group, edit_grideleprof->usekey_s.Get(), t.strarr_s[436].Get(), t.strarr_s[225].Get(), readonly);
			bool readonly = false;
			bool bFromCharacterCreator = false;
			bool bMustUpdateAnimations = false;
			extern bool g_bNowPopulateWithCorrectAnimSet;
			if (bShootingWeaponMentioned == true || bMeleeWeaponMentioned == true)
			{
				bool btmp = g_bNowPopulateWithCorrectAnimSet;
				g_bNowPopulateWithCorrectAnimSet = false;
				extern void animsystem_weaponproperty (int, bool, entityeleproftype*, bool, bool);
				animsystem_weaponproperty(t.entityprofile[entid].characterbasetype, bFromCharacterCreator, edit_grideleprof, bShootingWeaponMentioned, bMeleeWeaponMentioned);
				//PE: We changed weapon so must update animations.
				if (g_bNowPopulateWithCorrectAnimSet)
				{
					//PE: Must refresh DLUA
					bMustUpdateAnimations = true;
					fpe_current_loaded_script = -1;
				}
				g_bNowPopulateWithCorrectAnimSet = btmp;
			}
			else if (bUnarmedMentioned)
			{
				if (edit_grideleprof->hasweapon_s.Len() > 0)
				{
					edit_grideleprof->hasweapon_s = "";
					edit_grideleprof->overrideanimset_s = "";
					extern bool g_bNowPopulateWithCorrectAnimSet;
					g_bNowPopulateWithCorrectAnimSet = true;
				}
			}
			if (iAnimationSetMentioned > 0 || bMustUpdateAnimations )
			{
				extern void animsystem_animationsetproperty(int, bool, entityeleproftype*, int, int);
				bool btmp = g_bNowPopulateWithCorrectAnimSet;
				if (bMustUpdateAnimations)
				{
					g_bNowPopulateWithCorrectAnimSet = true;
					iAnimationSetMentioned = 1; //soldier
					animsystem_animationsetproperty(t.entityprofile[entid].characterbasetype, readonly, edit_grideleprof, iAnimationSetMentioned, elementID);
				}
				else
				{
					animsystem_animationsetproperty(t.entityprofile[entid].characterbasetype, readonly, edit_grideleprof, iAnimationSetMentioned, elementID);
				}
				g_bNowPopulateWithCorrectAnimSet = btmp;
			}
		}
	}
	if (t.entityprofile[entid].ischaracter == 1)
	{
		extern void animsystem_dropcollectablesetproperty(bool, entityeleproftype*);
		animsystem_dropcollectablesetproperty(readonly, edit_grideleprof);
	}
	if (readonly)
	{
		ImGui::PopItemFlag();
		ImGui::PopStyleVar();
	}
}

// Allow Bullet Holes tick for a placement, shown ticked when entity_allowsbulletholes allows holes; a change is stored
// as 1 (yes) or 2 (no), over the placement default (0: allowbulletholes in the .fpe, else static entities only)
void DisplayAllowBulletHolesCheckbox(int elementID)
{
	if (elementID <= 0 || elementID >= t.entityelement.size()) return;
	bool bAllowBulletHole = entity_allowsbulletholes(elementID);
	if (ImGui::Checkbox("Allow Bullet Holes ?", &bAllowBulletHole))
	{
		t.entityelement[elementID].iAllowBuletHole = bAllowBulletHole ? 1 : 2;
	}
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("If set shots leave bullet holes on this object. Set by allowbulletholes in its .fpe, else on for static objects only");
}

// Clear Grass and Trees tick for a placement auto flatten does not already clear under, shown ticked when
// entity_clearsvegetation clears; a change is stored as 1 (yes) or 2 (no), over the placement default (0: clearvegetation
// in the .fpe, else large static objects only)
void DisplayClearVegetationCheckbox(int elementID)
{
	if (elementID <= 0 || elementID >= t.entityelement.size()) return;
	if (g.entityrubberbandlist.size() > 0) return;
	int entid = t.entityelement[elementID].bankindex;
	if (entid <= 0 || t.entityprofile[entid].ismarker != 0) return;
	if (t.entityprofile[entid].autoflatten != 0 && g_bEnableAutoFlattenSystem && t.entityelement[elementID].eleprof.bAutoFlatten) return;
	bool bClearVegetation = entity_clearsvegetation(elementID);
	if (ImGui::Checkbox("Clear Grass and Trees ?", &bClearVegetation))
	{
		t.entityelement[elementID].iClearVegetation = bClearVegetation ? 1 : 2;
		entity_autoFlattenWhenAdded(elementID);
		g.projectmodified = 1;
	}
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("If set grass and trees under this object are hidden, without flattening the terrain. Set by clearvegetation in its .fpe, else on for static objects at least 3m across and 2.5m tall");
}

