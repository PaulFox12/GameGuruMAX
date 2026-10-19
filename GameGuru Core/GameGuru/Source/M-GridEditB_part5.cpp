//#####################################
//#### Process Preferences window. ####
//#####################################

void ProcessPreferences(void) 
{
	if (!bPreferences_Window)
		return;

	float fs = ImGui::CalcTextSize("#").x;

	if (refresh_gui_docking == 1)
	{
		ImGui::SetNextWindowSize(ImVec2(54 * ImGui::GetFontSize(), 38 * ImGui::GetFontSize()), ImGuiCond_Always);
		ImGui::SetNextWindowPosCenter(ImGuiCond_Always);
	}
	else
	{
		ImGui::SetNextWindowSize(ImVec2(54 * ImGui::GetFontSize(), 38 * ImGui::GetFontSize()), ImGuiCond_Once);
		ImGui::SetNextWindowPosCenter(ImGuiCond_Once);
	}

	static float fLastContentWidth = 0;
	static ImVec2 vLastWindowSize = ImVec2(0, 0);
	if (refresh_gui_docking >= 3 )
	{
		if (fLastContentWidth > 0 && fLastContentWidth < 700.0f && vLastWindowSize.y > 0)
		{
			ImGui::SetNextWindowSize(ImVec2(700.0f, vLastWindowSize.y), ImGuiCond_Always);
		}
	}

	ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoSavedSettings;
	if (bDigAHoleToHWND) window_flags |= ImGuiWindowFlags_ForceRender;

	ImGui::Begin("Settings", &bPreferences_Window, window_flags);

	ImGuiWindow* bwindow = ImGui::GetCurrentWindow(); // ImGui::FindWindowByName("Save New Level##Storyboard");
	if (bDigAHoleToHWND && bwindow)
		bwindow->DrawList->AddCallback((ImDrawCallback)10, NULL); //force render.

	int inputcolx = fs * 18;
	bool change_colors = false;

	static int iCurrentTab = 0;

	//Tabs
	if (ImGui::BeginTabBar("preferencestabbar"))
	{
		int flags = 0;
		if(iSetSettingsFocusTab == 1)
			flags = ImGuiTabItemFlags_SetSelected;
		if (ImGui::BeginTabItem(" General ",NULL, flags))
		{
			iCurrentTab = 0;

			if (iSetSettingsFocusTab == 1) iSetSettingsFocusTab = 0;
			ImGui::Columns(2, "preferencescolumns2", false);  //false no border
			ImGui::SetColumnWidth(0, ImGui::GetWindowSize().x*0.45);
			ImGui::PushItemWidth(-10);
			ImGui::Text("Editor Options");
			//ImGui::Text("");
			ImGui::Indent(10);


			bool bIntroStartup = pref.iDisplayIntroScreen;
			if (ImGui::Checkbox("Show Intro Video on Start Up", &bIntroStartup)) {
				pref.iDisplayIntroScreen = bIntroStartup;
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Show or hide the Intro Video when GameGuru MAX first loads");

			//pref.iDisplayWelcomeScreen
			bool bWelcomeStartup = pref.iDisplayWelcomeScreen;
			if (ImGui::Checkbox("Show Hub on Startup", &bWelcomeStartup)) 
			{
				pref.iDisplayWelcomeScreen = bWelcomeStartup;
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Show or hide the Welcome Screen when GameGuru MAX first loads");

			ImGui::Checkbox("Auto Close Property Sections", &pref.bAutoClosePropertySections);
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "After opening a new property section on the right, auto close any open ones");

			ImGui::Checkbox("Hide Tutorials Components From All Panels", &pref.bHideTutorials);
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Hides all tutorial sections from all panels");

			bool bHideShortcuts = pref.iHideKeyboardShortcuts;
			if(ImGui::Checkbox("Hide Keyboard Shortcuts From All Panels", &bHideShortcuts))
			{
				pref.iHideKeyboardShortcuts = bHideShortcuts;
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Hides all keyboard shortcuts from all panels");

			bool bTmp = pref.iTurnOffUITransparent;
			if (ImGui::Checkbox("Turn Off UI Transparency", &bTmp)) {
				pref.iTurnOffUITransparent = bTmp;
				change_colors = true;
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Stops the backgrounds in the UI from using transparency and uses an opaque color");

			bTmp = pref.iTurnOffEditboxTooltip;
			if (ImGui::Checkbox("Turn Off Help Pop-up Tips on Edit fields", &bTmp)) {
				pref.iTurnOffEditboxTooltip = bTmp;
				change_colors = true;
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Hides the pop-up help tips when you hover over edit fields");

			//PE: TODO - This system dont really work , it need to be per object.
			bTmp = pref.iEnableAutoFlattenSystem; //g_bEnableAutoFlattenSystem;
			if (ImGui::Checkbox("Auto flatten terrain under large objects", &bTmp)) 
			{
				g_bEnableAutoFlattenSystem = bTmp;
				pref.iEnableAutoFlattenSystem = bTmp;
				bool bChangeState = false;
				if (bTmp)
				{
					bChangeState = true;
				}
				//PE: Loop into all.
				GGGrass_DeferInstanceUpdates(1); // the grass is placed again once, not for each flat area
				for (int i = 1; i <= g.entityelementlist; i++) {

					int entid = t.entityelement[i].bankindex;
					if (entid > 0)
					{
						int iAutoFlattenMode = t.entityprofile[entid].autoflatten;
						if (iAutoFlattenMode != 0)
							t.entityelement[i].eleprof.bAutoFlatten = bChangeState;
						else
							t.entityelement[i].eleprof.bAutoFlatten = false;
						if (!bTmp && t.entityelement[i].eleprof.iFlattenID != -1)
						{
							//PE: Disabled remove any flatten.
							GGTerrain_RemoveFlatArea(t.entityelement[i].eleprof.iFlattenID);
							t.entityelement[i].eleprof.iFlattenID = -1;
						}
						if (!bTmp)
						{
							// Clear Grass and Trees may now apply, and the removal above took those areas too (entity_clearsvegetation)
							entity_autoFlattenWhenAdded(i);
						}
						if (iAutoFlattenMode != 0 && bTmp)
						{
							if (t.entityelement[i].eleprof.iFlattenID == -1)
								entity_autoFlattenWhenAdded(i);
							else
								entity_updateautoflatten(i);
							g.projectmodified = 1;
						}
					}
				}
				GGGrass_DeferInstanceUpdates(0);
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Flattens the terrain and clears any trees and grass when placing large objects");

			bool iCheckFileModifications = pref.iCheckFilesModifiedOnFocus;
			if (ImGui::Checkbox("Auto replace objects when files are modified", &iCheckFileModifications))
			{
				pref.iCheckFilesModifiedOnFocus = iCheckFileModifications;
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("When the software regains focus, any objects that use outdated files will be reloaded");

			ImGui::Indent(-10);
			ImGui::Text("Graphic Settings");
			ImGui::Indent(10);

			const char* quaity_items[] = { "Test Level - Low Settings", "Test Level - Medium Settings", "Test Level - High Settings" };
			if (ImGui::Combo("##GraphicsQualityTest", &pref.iTestGameGraphicsQuality, quaity_items, IM_ARRAYSIZE(quaity_items))) {
				//PE: This will overwrite "level designer" settings , in editor and test game, this is only for test game, so set later.
			}

			ImGui::PopItemWidth();
			ImGui::Indent(-10);

			ImGui::NextColumn();

			ImGui::PushItemWidth(-10);
			ImGui::Text("Interface style");
			ImGui::Indent(10);

			const char* style_combo[] = { 
						"Blue Style", //0->1
						"Dark Style",//1->12
						"Modern Dark",//2->13 
						"Evening Blue",//3->9
						"Green Tea",//4->2
						"Light Style",//5->14 
						"Moody Red",//6->8
						"Purple Haze",//7->4
						"Racing Green",//8->10
						"Red Lines",//9->7
						"Retro Green",//10->11
						"Sea Blue",//11->0
						"Smart Purple",//12->5
						"Striking Yellow",//13->6
						"Sunset Red",//14->3
						"Tango",//15->3
						"Darker Style",//16->9
			};

			int style_current_type_selection;
			if (pref.current_style == 0) style_current_type_selection = 1;
			if (pref.current_style == 1) style_current_type_selection = 2;
			if (pref.current_style == 3) style_current_type_selection = 5;
			if (pref.current_style >= 10) style_current_type_selection = pref.current_style - 9;
			if (pref.current_style == 25) style_current_type_selection = 0; // ZJ: Moved blue to top of list, so pref.current_style - 10 no longer works.
			if (pref.current_style == 9) style_current_type_selection = 16;

			if (ImGui::Combo("##BehavioursSimpleInput", &style_current_type_selection, style_combo, IM_ARRAYSIZE(style_combo)))
			{
				myDefaultStyles();
				if (style_current_type_selection == 0) {
					//Blue
					pref.tint_style = ImVec4(1.0, 1.0, 1.0, 1.0);
					pref.shade_style = ImVec4(0.0, 0.0, 0.0, 0.0);
					pref.title_style = ImVec4(0.0, 0.0, 0.0, 0.0);
					pref.current_style = 25;
					myStyleBlue(NULL);
					SetIconSet();
				}
				if (style_current_type_selection == 1) {
					// dark
					pref.tint_style = ImVec4(1.0, 1.0, 1.0, 1.0);
					pref.shade_style = ImVec4(0.0, 0.0, 0.0, 0.0);
					pref.title_style = ImVec4(0.0, 0.0, 0.0, 0.0);
					pref.current_style = 0;
					myStyle2(NULL);
					SetIconSet();
				}
				if (style_current_type_selection == 2) {
					// darker
					pref.tint_style = ImVec4(1.0, 1.0, 1.0, 1.0);
					pref.shade_style = ImVec4(0.0, 0.0, 0.0, 0.0);
					pref.title_style = ImVec4(0.0, 0.0, 0.0, 0.0);
					pref.current_style = 1;
					void DarkColorsNoTransparent(void);
					myStyle2(NULL);
					DarkColorsNoTransparent();
					SetIconSet();
				}
				if (style_current_type_selection == 3) {
					//Evening blue
					pref.tint_style = ImVec4(255.0 / 255.0, 255.0 / 255.0, 255.0 / 255.0, 0.0);
					pref.shade_style = ImVec4(0, 0, 0, 1.0);
					pref.title_style = ImVec4(1 / 255.0, 36 / 255.0, 73 / 255.0, 0.0);
					pref.current_style = 12;
					myStyle2(NULL);
					SetIconSet();
				}
				if (style_current_type_selection == 4) {
					//Green tea
					pref.tint_style = ImVec4(0, 0, 0, 1.0);
					pref.shade_style = ImVec4(4 / 255.0, 124 / 255.0, 10 / 255.0, 0.0);
					pref.title_style = ImVec4(89 / 255.0, 160 / 255.0, 93 / 255.0, 0.0);
					pref.current_style = 13;
					myStyle2(NULL);
					SetIconSet();
				}
				if (style_current_type_selection == 5) {
					// Light style
					pref.tint_style = ImVec4(0.0, 0.0, 0.0, 0.0);
					pref.shade_style = ImVec4(1.0, 1.0, 1.0, 1.0);
					pref.title_style = ImVec4(1.0, 1.0, 1.0, 1.0);
					pref.current_style = 3;
					myLightStyle(NULL);
					SetIconSet();
				}
				if (style_current_type_selection == 6) {
					//Moody red
					pref.tint_style = ImVec4(255.0 / 255.0, 255.0 / 255.0, 255.0 / 255.0, 0.0);
					pref.shade_style = ImVec4(14 / 255.0, 12 / 255.0, 29 / 255.0, 0.0);
					pref.title_style = ImVec4(131 / 255.0, 16 / 255.0, 6 / 255.0, 0.0);
					pref.current_style = 15;
					myStyle2(NULL);
					SetIconSet();
				}
				if (style_current_type_selection == 7) {
					//Purple haze
					pref.tint_style = ImVec4(0, 0, 0, 1.0);
					pref.shade_style = ImVec4(163 / 255.0, 43 / 255.0, 179 / 255.0, 0.0);
					pref.title_style = ImVec4(251 / 255.0, 251 / 255.0, 251 / 255.0, 0.0);
					pref.current_style = 16;
					myStyle2(NULL);
					SetIconSet();
				}
				if (style_current_type_selection == 8) {
					//Racing green
					pref.tint_style = ImVec4(255.0 / 255.0, 255.0 / 255.0, 255.0 / 255.0, 0.0);
					pref.shade_style = ImVec4(0, 0, 0, 1.0);
					pref.title_style = ImVec4(18 / 255.0, 62 / 255.0, 0 / 255.0, 0.0);
					pref.current_style = 17;
					myStyle2(NULL);
					SetIconSet();
				}
				if (style_current_type_selection == 9) {
					//Red Lines
					pref.tint_style = ImVec4(0, 0, 0, 1.0);
					pref.shade_style = ImVec4(255.0 / 255.0, 255.0 / 255.0, 255.0 / 255.0, 0.0);
					pref.title_style = ImVec4(230 / 255.0, 56 / 255.0, 56 / 255.0, 0.0);
					pref.current_style = 18;
					myStyle2(NULL);
					SetIconSet();
				}
				if (style_current_type_selection == 10) {
					//Retro green
					pref.tint_style = ImVec4(11 / 255.0, 248 / 255.0, 25 / 255.0, 0.0);
					pref.shade_style = ImVec4(0, 0, 0, 1.0);
					pref.title_style = ImVec4(0, 0, 0, 1.0);
					pref.current_style = 19;
					myStyle2(NULL);
					SetIconSet();
				}
				if (style_current_type_selection == 11) {
					//Sea blue
					pref.tint_style = ImVec4(7 / 255.0, 7 / 255.0, 7 / 255.0, 1.0);
					pref.shade_style = ImVec4(12 / 255.0, 100 / 255.0, 168 / 255.0, 0.0);
					pref.title_style = ImVec4(28 / 255.0, 77 / 255.0, 244 / 255.0, 0.0);
					pref.current_style = 20;
					myStyle2(NULL);
					SetIconSet();
				}
				if (style_current_type_selection == 12) {
					//Smart purple
					pref.tint_style = ImVec4(0, 0, 0, 1.0);
					pref.shade_style = ImVec4(255.0 / 255.0, 255.0 / 255.0, 255.0 / 255.0, 0.0);
					pref.title_style = ImVec4(172 / 255.0, 96 / 255.0, 182 / 255.0, 0.0);
					pref.current_style = 21;
					myStyle2(NULL);
					SetIconSet();
				}
				if (style_current_type_selection == 13) {
					//Striking yellow
					pref.tint_style = ImVec4(0, 0, 0, 1.0);
					pref.shade_style = ImVec4(255.0 / 255.0, 255.0 / 255.0, 255.0 / 255.0, 0.0);
					pref.title_style = ImVec4(200 / 255.0, 191 / 255.0, 34 / 255.0, 0.0);
					pref.current_style = 22;
					myStyle2(NULL);
					SetIconSet();
				}
				if (style_current_type_selection == 14) {
					//Sunset red
					pref.tint_style = ImVec4(0, 0, 0, 1.0);
					pref.shade_style = ImVec4(164 / 255.0, 70 / 255.0, 70 / 255.0, 0.0);
					pref.title_style = ImVec4(204 / 255.0, 63 / 255.0, 50 / 255.0, 0.0);
					pref.current_style = 23;
					myStyle2(NULL);
					SetIconSet();
				}
				if (style_current_type_selection == 15) {
					//Tango
					pref.tint_style = ImVec4(0, 0, 0, 1.0);
					pref.shade_style = ImVec4(244 / 255.0, 251 / 255.0, 0, 0.0);
					pref.title_style = ImVec4(237 / 255.0, 86 / 255.0, 7 / 255.0, 0.0);
					pref.current_style = 24;
					myStyle2(NULL);
					SetIconSet();
				}
				if (style_current_type_selection == 16) {
					pref.tint_style = ImVec4(1.0, 1.0, 1.0, 1.0);
					pref.shade_style = ImVec4(0.0, 0.0, 0.0, 0.0);
					pref.title_style = ImVec4(0.0, 0.0, 0.0, 0.0);
					pref.current_style = 9;
					myDarkStyle(NULL);
					SetIconSet();
				}

			}

			if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Select your preferred user interface style");
			
			ImGui::PopItemWidth();		

			if (pref.current_style == 1)
			{
				for (int i = 0; i < 2; i++)
				{

					float ChangeColor[4];
					if (i == 0)
					{
						//VS2022 colors.
						ChangeColor[0] = pref.status_bar_color.x;
						ChangeColor[1] = pref.status_bar_color.y;
						ChangeColor[2] = pref.status_bar_color.z;
						ChangeColor[3] = 1.0f;
					}
					else
					{
						ChangeColor[0] = pref.highlight_color.x;
						ChangeColor[1] = pref.highlight_color.y;
						ChangeColor[2] = pref.highlight_color.z;
						ChangeColor[3] = 1.0f;
					}

					std::string label = "";
					if (i == 0) label = "Statusbar color: ";
					else label = "Highlight color: ";

					float fixed_pos_x = ImGui::GetCursorPosX();

					ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2);
					ImGui::Text(label.c_str());
					ImGui::SameLine();

					ImGui::PushItemWidth(32);
					ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 1);
					ImGui::SetCursorPosX(fixed_pos_x+120.0f);

					if (i == 0) label = "##statusbarHighlightpopup";
					else label = "##Highlightpopup";

					ImVec4* colorvaluechange;
					if (i == 0)
						colorvaluechange = &pref.status_bar_color;
					else
						colorvaluechange = &pref.highlight_color;

					std::string tooltip = "";
					if (i == 0) tooltip = "Change the status bar colors";
					else tooltip = "Change the highlight colors";

					bool open_popup = ImGui::ColorButton(label.c_str(), *colorvaluechange, 0, ImVec2(32, 18));
					if (ImGui::IsItemHovered()) ImGui::SetTooltip(tooltip.c_str());
					ImGui::PopItemWidth();
					if (open_popup) ImGui::OpenPopup(label.c_str());
					if (ImGui::BeginPopup(label.c_str(), ImGuiWindowFlags_NoMove))
					{
						if (ImGui::ColorPicker4(label.c_str(), &ChangeColor[0], ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview))
						{
							ImVec4 colorselect = ImVec4(ChangeColor[0], ChangeColor[1], ChangeColor[2], 1);
							*colorvaluechange = colorselect;
							change_colors = true;
						}
						ImGui::EndPopup();
					}

					ImGui::SameLine();

					ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 8);

					if (i == 0) label = "##statusbarHighlightdefault";
					else  label = "##Highlightdefault";

					ImVec4 colorselect = ImVec4((1.0f / 255.0f) * 14, (1.0f / 255.0f) * 99, (1.0f / 255.0f) * 156, 1.0);
					if (ImGui::ColorButton(std::string(label + "1").c_str(), colorselect, 0, ImVec2(18, 18)))
					{
						*colorvaluechange = colorselect;
						change_colors = true;
					}

					ImGui::SameLine();
					colorselect = ImVec4((1.0f / 255.0f) * 202, (1.0f / 255.0f) * 81, 0, 1.0);
					if (ImGui::ColorButton(std::string(label + "2").c_str(), colorselect, 0, ImVec2(18, 18)))
					{
						*colorvaluechange = colorselect;
						change_colors = true;
					}

					ImGui::SameLine();
					colorselect = ImVec4((1.0f / 255.0f) * 18, (1.0f / 255.0f) * 117, (1.0f / 255.0f) * 58, 1.0);
					if (ImGui::ColorButton(std::string(label + "3").c_str(), colorselect, 0, ImVec2(18, 18)))
					{
						*colorvaluechange = colorselect;
						change_colors = true;
					}

					ImGui::SameLine();
					colorselect = ImVec4((1.0f / 255.0f) * 92, (1.0f / 255.0f) * 53, (1.0f / 255.0f) * 174, 1.0);
					if (ImGui::ColorButton(std::string(label + "4").c_str(), colorselect, 0, ImVec2(18, 18)))
					{
						*colorvaluechange = colorselect;
						change_colors = true;
					}

					ImGui::SameLine();
					colorselect = ImVec4((1.0f / 255.0f) * 166, (1.0f / 255.0f) * 45, (1.0f / 255.0f) * 25, 1.0);
					if (ImGui::ColorButton(std::string(label + "5").c_str(), colorselect, 0, ImVec2(18, 18)))
					{
						*colorvaluechange = colorselect;
						change_colors = true;
					}

					ImGui::SameLine();
					colorselect = ImVec4((1.0f / 255.0f) * 79, (1.0f / 255.0f) * 79, (1.0f / 255.0f) * 79, 1.0);
					if (ImGui::ColorButton(std::string(label + "6").c_str(), colorselect, 0, ImVec2(18, 18)))
					{
						*colorvaluechange = colorselect;
						change_colors = true;
					}

					ImGui::SameLine();
					colorselect = ImVec4(0, 0, 0, 1.0);
					if (ImGui::ColorButton(std::string(label + "7").c_str(), colorselect, 0, ImVec2(18, 18)))
					{
						*colorvaluechange = colorselect;
						change_colors = true;
					}
				}
				ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2);
			}


			bTmp = pref.iEnableCustomColors;
			if (ImGui::Checkbox("Enable Custom Colors", &bTmp)) 
			{
				pref.iEnableCustomColors = bTmp;
				change_colors = true;
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Use this to design your own custom interface style");

			if (pref.iEnableCustomColors)
			{
				//Tint
				float ChangeColor[4];
				ChangeColor[0] = pref.tint_style.x;
				ChangeColor[1] = pref.tint_style.y;
				ChangeColor[2] = pref.tint_style.z;
				ChangeColor[3] = 1.0f;

				float hw = (ImGui::GetContentRegionAvailWidth() - 20.0f) * 0.33;
				ImGui::Text("");
				ImVec2 cpos = ImGui::GetCursorPos();
				ImGui::Text("Text ");
				ImGui::SetCursorPos(cpos + ImVec2(hw + 8.0, 0));
				ImGui::Text("Background ");
				ImGui::SetCursorPos(cpos + ImVec2((hw*2) + 16.0, 0));
				ImGui::Text("Highlight "); 

				ImGui::PushItemWidth(hw);
				if (ImGui::ColorPicker4("##ChangeTintColor", &ChangeColor[0], ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_DisplayRGB))
				{
					pref.tint_style.x = ChangeColor[0];
					pref.tint_style.y = ChangeColor[1];
					pref.tint_style.z = ChangeColor[2];
					change_colors = true;
				}
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Change the color of the text to make your own custom style");

				ChangeColor[0] = pref.shade_style.x;
				ChangeColor[1] = pref.shade_style.y;
				ChangeColor[2] = pref.shade_style.z;
				ChangeColor[3] = 1.0f;
				ImGui::SameLine();
				if (ImGui::ColorPicker4("##ChangeshadeColor", &ChangeColor[0], ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_DisplayRGB))
				{
					pref.shade_style.x = ChangeColor[0];
					pref.shade_style.y = ChangeColor[1];
					pref.shade_style.z = ChangeColor[2];
					change_colors = true;
				}
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Change the color of the background to make your own custom style");

				ChangeColor[0] = pref.title_style.x;
				ChangeColor[1] = pref.title_style.y;
				ChangeColor[2] = pref.title_style.z;
				ChangeColor[3] = 1.0f;
				ImGui::SameLine();
				if (ImGui::ColorPicker4("##ChangehighlightColor", &ChangeColor[0], ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_DisplayRGB))
				{
					pref.title_style.x = ChangeColor[0];
					pref.title_style.y = ChangeColor[1];
					pref.title_style.z = ChangeColor[2];
					change_colors = true;
				}
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Change the color of the highlight to make your own custom style");
				ImGui::PopItemWidth();


			}

			if (change_colors) {
				if (pref.current_style == 0 || pref.current_style >= 10 && pref.current_style != 25) {
					myStyle2(NULL);
				}
				if (pref.current_style == 1) {
					void DarkColorsNoTransparent(void);
					myStyle2(NULL);
					DarkColorsNoTransparent();
				}
				if(pref.current_style == 9)
				{
					myDarkStyle(NULL);
				}

				if (pref.current_style == 3) {
					myLightStyle(NULL);
				}
				if (pref.current_style == 25) {
					myStyleBlue(NULL);
				}
				SetIconSet();
			}


			ImGui::PushItemWidth(-10);
			ImGui::Text("");
			ImGui::Text("Grid and Alignment Gadget");

			if (pref.iSmallToolbar < 0 || pref.iSmallToolbar > 3)//4)
				pref.iSmallToolbar = 0;
			const char* smalltoolbar_combo[] = {
											"None",
											"Titlebar",
											"Floating Large",
											"Floating",
											//"Toolbar Large",
			};
			int smalltoolbar_selection;
			if (ImGui::Combo("##SmallToolbarSetup", &pref.iSmallToolbar, smalltoolbar_combo, IM_ARRAYSIZE(smalltoolbar_combo)))
			{
				if (pref.iSmallToolbar > 0)
				{
					//PE: Set new grid system defaults.
					pref.fEditorGridOffsetX = 0;
					pref.fEditorGridOffsetY = 0;
					pref.fEditorGridOffsetZ = 0;
					pref.fEditorGridSizeX = 10.0f;
					pref.fEditorGridSizeY = pref.fEditorGridSizeX;
					pref.fEditorGridSizeZ = pref.fEditorGridSizeX;
				}
				else
				{
					//PE: Old defaults.
					pref.fEditorGridOffsetY = 0;
					pref.fEditorGridSizeY = 0;
					pref.fEditorGridOffsetX = 50.0f;
					pref.fEditorGridOffsetZ = 50.0f;
					pref.fEditorGridSizeX = 100.0f;
					pref.fEditorGridSizeZ = 100.0f;
				}
			}
			ImGui::PopItemWidth();


			ImGui::PushItemWidth(-10);
			ImGui::Indent(-10);
			ImGui::Text("");
			ImGui::Text("Windows Layout");
			ImGui::Text("");
			ImGui::Indent(10);
			float w = ImGui::GetContentRegionAvailWidth() - 10.0f;
			if (ImGui::StyleButton("Reset Interface",ImVec2(w,0)) ) {
				refresh_gui_docking = 0;
				MaximiseWindow(); 
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "This will reset the interface to fit the current desktop size");

			ImGui::Indent(-10);

			ImGui::PopItemWidth();

			ImGui::Columns(1);
			ImGui::EndTabItem();
		}

		flags = 0;
		if (iSetSettingsFocusTab == 2)
			flags = ImGuiTabItemFlags_SetSelected;

		if (ImGui::BeginTabItem(" Advanced ", NULL , flags) )
		{
			static bool bCheckedInitialState = false;
			iCurrentTab = 1;
			if (iSetSettingsFocusTab == 2) iSetSettingsFocusTab = 0;
			static int iAdvCountDown = 10;
			if (iAdvCountDown > 0) 
			{
				iAdvCountDown--;
				if (iAdvCountDown == 0)
				{
					if (pref.iObjectEnableAdvanced == 2)
					{
						// skip message warnings when advanced user is in compact mode
					}
				}
			}

			ImGui::Columns(2, "preferencesAdvancedcolumns2", false);  //false no border

			ImGui::PushItemWidth(-10);
			ImGui::Text("Panel and Drop-Down Menu Advanced Settings");
			ImGui::Indent(10);

			bool bTmp = pref.bAutoOpenMenuItems;
			if (ImGui::Checkbox("Auto Open Drop-down Menus", &bTmp)) {
				pref.bAutoOpenMenuItems = bTmp;
				bCheckedInitialState = false;
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Automatically open drop down menus (without clicking)");

			bTmp = pref.iEnableSingleRightPanelAdvanced;
			if (ImGui::Checkbox("Show Multiple Panels in Tabs", &bTmp)) {
				pref.iEnableSingleRightPanelAdvanced = bTmp;
				bCheckedInitialState = false;
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Allow tabbed sections in the right panel");

			bTmp = pref.iEnableAdvancedEntityList;
			if (ImGui::Checkbox("Allow Selection of the Collection List", &bTmp)) {
				current_sort_order = 0;
				pref.iEnableAdvancedEntityList = bTmp;
				bCheckedInitialState = false;
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Enable the collection list filter in the level objects window");

			void ToggleDPIAwareness(bool);
			// DPI Awareness Flag
			bool bDPIAware = true;
			// read DPI Aware from registry
			char pDPINotAware[256];
			strcpy(pDPINotAware, "0");
			HKEY hKeyNames = 0;
			DWORD Status = RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\GameGuruMAX", 0L, KEY_READ, &hKeyNames);
			if (Status == ERROR_SUCCESS)
			{
				DWORD Type = REG_SZ;
				DWORD Size = 256;
				Status = RegQueryValueExA(hKeyNames, "DPINotAware", NULL, &Type, NULL, &Size);
				if (Size < 255)
				{
					RegQueryValueExA(hKeyNames, "DPINotAware", NULL, &Type, (LPBYTE)pDPINotAware, &Size);
				}
				RegCloseKey(hKeyNames);
			}
			if (pDPINotAware[0] == '1') bDPIAware = false;
			if (ImGui::Checkbox("Enable DPI Awareness", &bDPIAware))
			{
				ToggleDPIAwareness(bDPIAware);
				bCheckedInitialState = false;
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "If the UI is too large, enable DPI awareness to improve the scale. Requires a restart to see changes");
			ImGui::Indent(-10);
			ImGui::PopItemWidth();

			ImGui::PushItemWidth(-10);
			//ImGui::Text("");
			ImGui::Spacing();
			ImGui::Text("Game Editing Advanced Settings");
			ImGui::Indent(10);

			bTmp = pref.iEnableAxisRotationShortcuts;
			if (ImGui::Checkbox("Allow Axis Rotation Shortcuts", &bTmp)) {
				pref.iEnableAxisRotationShortcuts = bTmp;
				bCheckedInitialState = false;
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Allows you to rotate a selected object with keys 1 through 6");

			bTmp = pref.iEnableLevelEditorOpenAndNew;
			if (ImGui::Checkbox("Level Editor - Allow New And Open Options", &bTmp)) {
				pref.iEnableLevelEditorOpenAndNew = bTmp;
				bCheckedInitialState = false;
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Allows you to use menu options new and open from level editor");

			//New and Open options.


			bTmp = pref.iEnableIdentityProperties;
			if (ImGui::Checkbox("Display Identity in Object Tools", &bTmp)) {
				pref.iEnableIdentityProperties = bTmp;
				bCheckedInitialState = false;
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Display the identity of the selected object in the tools panel");

			bTmp = pref.iStoryboardAdvanced;
			if (ImGui::Checkbox("Game Storyboard", &bTmp)) {
				pref.iStoryboardAdvanced = bTmp;
				bCheckedInitialState = false;
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Show the Advanced Game Storyboard settings");
			bTmp = pref.iTerrainAdvanced;
			if (ImGui::Checkbox("Terrain Generator", &bTmp)) {
				pref.iTerrainAdvanced = bTmp;
				bCheckedInitialState = false;
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Show the Advanced Terrain Generator settings");

			bTmp = pref.iEnableTerrainHeightmaps;
			if (ImGui::Checkbox("Terrain Heightmaps", &bTmp)) {
				pref.iEnableTerrainHeightmaps = bTmp;
				bCheckedInitialState = false;
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Show settings for importing heightmaps during terrain generation");

			bTmp = pref.iObjectEnableAdvanced;
			if (ImGui::Checkbox("Object Tools", &bTmp)) 
			{
				pref.iObjectEnableAdvanced = bTmp;
				bCheckedInitialState = false;
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Show the Advanced Object Tools settings");
			if (pref.iObjectEnableAdvanced)
			{
				ImGui::SameLine();
				bTmp = false;
				if (pref.iObjectEnableAdvanced == 2) bTmp = true;
				if (ImGui::Checkbox("Compact Pos/Rot/Scl", &bTmp))
				{
					if (bTmp==false)
						pref.iObjectEnableAdvanced = 1;
					else
						pref.iObjectEnableAdvanced = 2;
					bCheckedInitialState = false;
				}
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Compact the Position, Rotation and Scale and keep component open");
			}

			bTmp = pref.iEnableAdvancedWater;
			if (ImGui::Checkbox("Water", &bTmp)) {
				pref.iEnableAdvancedWater = bTmp;
				bCheckedInitialState = false;
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Show the Advanced Water customization settings");

			bTmp = pref.iEnableAdvancedCharacterCreator;
			if (ImGui::Checkbox("Character Creator", &bTmp)) {
				pref.iEnableAdvancedCharacterCreator = bTmp;
				bCheckedInitialState = false;
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Show the Advanced Character Creator settings");

			bTmp = pref.iFullscreenPreviewAdvanced;
			if (ImGui::Checkbox("Object Library Preview Details", &bTmp)) {
				pref.iFullscreenPreviewAdvanced = bTmp;
				bCheckedInitialState = false;
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Show more details about an object when viewing in full screen mode in the object library");
			ImGui::Indent(-10);
			ImGui::PopItemWidth();

			ImGui::NextColumn();

			ImGui::PushItemWidth(-10);
			//ImGui::Text("");
			ImGui::Text("Advanced Environmental Effects");
			ImGui::Indent(10);

			 bTmp = pref.iEnableAdvancedSky;
			if (ImGui::Checkbox("Sky", &bTmp)) {
				pref.iEnableAdvancedSky = bTmp;
				bCheckedInitialState = false;
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Show the Advanced Sky customization settings");

			bTmp = pref.iEnableAdvancedPostProcessing;
			if (ImGui::Checkbox("Post Processing", &bTmp)) {
				pref.iEnableAdvancedPostProcessing = bTmp;
				bCheckedInitialState = false;
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Reveal the Post Processing properties section");

			bTmp = pref.iEnableAdvancedShadows;
			if (ImGui::Checkbox("Shadows", &bTmp)) {
				pref.iEnableAdvancedShadows = bTmp;
				bCheckedInitialState = false;
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Reveal the Shadows properties section");
			ImGui::Indent(-10);
			ImGui::PopItemWidth();

			ImGui::Text("");
			ImGui::Indent(10);

			// Determine if all the settings are on/off to check required state change.
			static std::array<int*, 16> advancedsettings = {
			&pref.bAutoOpenMenuItems,
			&pref.iEnableSingleRightPanelAdvanced,
			&pref.iEnableAdvancedEntityList,
			&pref.iEnableAxisRotationShortcuts,
			&pref.iEnableIdentityProperties,
			&pref.iStoryboardAdvanced,
			&pref.iTerrainAdvanced,
			&pref.iObjectEnableAdvanced,
			&pref.iEnableAdvancedWater,
			&pref.iEnableAdvancedCharacterCreator,
			&pref.iFullscreenPreviewAdvanced,
			&pref.iEnableAdvancedSky,
			&pref.iEnableAdvancedPostProcessing,
			&pref.iEnableAdvancedShadows,
			&pref.iImporterDome,
			&pref.iEnableAdvancedGrass
			};

			static bool bAllEnabled = false;
			
			// Check the initial state once, so that the correct button label can be assigned.
			if (!bCheckedInitialState)
			{
				bCheckedInitialState = true;
				int iEnabledSettings = 0;
				for (int i = 0; i < advancedsettings.size(); i++)
				{
					if (*advancedsettings[i] == 1)
						iEnabledSettings++;
				}
				if (iEnabledSettings == advancedsettings.size())
					bAllEnabled = true;		
			}
			
			char title[250];
			if (bAllEnabled)
				strcpy(title, "Turn Off All Advanced Settings");
			else
				strcpy(title, "Turn On All Advanced Settings");

			if (ImGui::StyleButton(title, ImVec2(ImGui::GetContentRegionAvailWidth() - 10, 0.0f))) 
			{
				int iEnabledSettings = 0;
				for (int i = 0; i < advancedsettings.size(); i++)
				{
					if (*advancedsettings[i] == 1)
						iEnabledSettings++;
				}

				int iDesiredValue = 1;
				if (iEnabledSettings == advancedsettings.size())
				{
					iDesiredValue = 0;
					if(bDPIAware)
						ToggleDPIAwareness(!bDPIAware);
				}
				
				for (int i = 0; i < advancedsettings.size(); i++)
					*advancedsettings[i] = iDesiredValue;

				if (iDesiredValue)
				{
					// Handle DPI Awareness differently.
					if (!bDPIAware)
						ToggleDPIAwareness(!bDPIAware);

					bAllEnabled = true;
				}
				else
					bAllEnabled = false;
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toggle all advanced settings on or off");
			ImGui::Indent(-10);

			// Custom Writables Folder
			ImGui::Text("");
			ImGui::Text("Writables Folder Location");
			ImGui::Indent(10);
			float path_gadget_size = ImGui::GetFontSize()*2.0;
			ImGui::PushItemWidth(-10 - path_gadget_size);
			extern char szWriteDir[MAX_PATH];
			static bool bWriteFolderActive = false;
			if (strlen(pref.cCustomWriteFolder) > 0) bWriteFolderActive = true;
			if (ImGui::Checkbox("Allow the Writables Folder to be changed", &bWriteFolderActive)) 
			{
				if (!bWriteFolderActive)
				{
					strcpy(pref.cCustomWriteFolder, "");
					SetUpdaterWritePathFile(pref.cCustomWriteFolder);		
					strcpy(cPreferencesMessage, "Please restart MAX for this change to take effect!");
				}
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Choose a new writables location where your projects are to be saved by default");
			if (bWriteFolderActive)
			{
				if (strlen(pref.cCustomWriteFolder) == 0)
					ImGui::InputText("##InputCustomWriteFolder", &szWriteDir[0], 250, ImGuiInputTextFlags_ReadOnly);
				else
					ImGui::InputText("##InputCustomWriteFolder", &pref.cCustomWriteFolder[0], 250, ImGuiInputTextFlags_ReadOnly);

				if (!pref.iTurnOffEditboxTooltip && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Current Writables Folder");
				if (ImGui::MaxIsItemFocused()) bImGuiGotFocus = true;

				ImGui::SameLine();
				ImGui::PushItemWidth(path_gadget_size);
				if (ImGui::StyleButton("...##pathCustomWriteFolder")) 
				{
					//PE: filedialogs change dir so.
					cStr tOldDir = GetDir();
					char * cFileSelected;
					cstr fulldir = pref.cCustomWriteFolder;
					cFileSelected = (char *)noc_file_dialog_open(NOC_FILE_DIALOG_DIR, "All\0*.*\0", fulldir.Get(), NULL);
					SetDir(tOldDir.Get());
					if (cFileSelected && strlen(cFileSelected) > 0) 
					{
						strcpy(pref.cCustomWriteFolder, cFileSelected);
						SetUpdaterWritePathFile(pref.cCustomWriteFolder);
						if (pref.cCustomWriteFolder[strlen(pref.cCustomWriteFolder) - 1] != '\\') strcat(pref.cCustomWriteFolder, "\\");
						strcpy(cPreferencesMessage, "Please restart MAX for this change to take effect!");
					}
				}
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("Choose a new writables location where your projects are to be saved by default");
				ImGui::PopItemWidth();
			}
			ImGui::PopItemWidth();

			ImGui::Text("");
			ImGui::Indent(-10);
			ImGui::Text("Default Import Folder Location");

			ImGui::Indent(10);
			path_gadget_size = ImGui::GetFontSize()*2.0;
			ImGui::PushItemWidth(-10 - path_gadget_size);
			ImGui::InputText("##InputDefaultImportFolder", &pref.cDefaultImportPath[0], 250, ImGuiInputTextFlags_ReadOnly);
			ImGui::PopItemWidth();
			if (!pref.iTurnOffEditboxTooltip && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Select a new folder path from where to import models");
			if (ImGui::MaxIsItemFocused()) bImGuiGotFocus = true;

			ImGui::SameLine();
			ImGui::PushItemWidth(path_gadget_size);
			if (ImGui::StyleButton("...##pathDefaultImportFolder")) {
				//PE: filedialogs change dir so.
				cStr tOldDir = GetDir();
				char * cFileSelected;
				cstr fulldir = pref.cDefaultImportPath;
				cFileSelected = (char *)noc_file_dialog_open(NOC_FILE_DIALOG_DIR, "All\0*.*\0", fulldir.Get(), NULL);
				SetDir(tOldDir.Get());
				if (cFileSelected && strlen(cFileSelected) > 0) {
					strcpy(pref.cDefaultImportPath, cFileSelected);
					if (pref.cDefaultImportPath[strlen(pref.cDefaultImportPath) - 1] != '\\')
						strcat(pref.cDefaultImportPath, "\\");
					sDefaultImportPath = pref.cDefaultImportPath;
				}
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Select a new folder path from where to import models");
			ImGui::PopItemWidth();

			bool bTemp = pref.iImporterDome;
			if (ImGui::Checkbox("Show the Importer 'Dome' environment", &bTemp))
			{
				pref.iImporterDome = bTemp;
				bCheckedInitialState = false;
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("During the import process, a dome shape surrounds the imported model, use this to turn it off");

			ImGui::Indent(-10);
			ImGui::Columns(1);
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem(" Developer "))
		{
			iCurrentTab = 2;

			static int iDevCountDown = 10;
			if (iDevCountDown > 0) 
			{
				iDevCountDown--;
				if (iDevCountDown == 0)
				{
					if (pref.iObjectEnableAdvanced == 2)
					{
						// skip message warnings when advanced user is in compact mode
					}
				}
			}

			ImGui::Columns(2, "preferencestoolscolumns2", false);  //false no border

			ImGui::Text("Developer Modules");
			ImGui::Indent(10);
			extern int g_iDevToolsOpen;
			bool bT = (bool)g_iDevToolsOpen;
			if (ImGui::Checkbox("Show Additional Developer Options", &bT))
			{
				g_iDevToolsOpen = bT;
				pref.iDevToolsOpen = g_iDevToolsOpen;
				wiProfiler::SetEnabled(bProfilerEnable);
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Enable additional settings (auto-refresh library folders, physics debugging, developer mode tools, save anim templates)");
			
			if (ImGui::Checkbox("Enable the 3D Editor Profiler", &bProfilerEnable))
			{
				wiProfiler::SetEnabled(bProfilerEnable);
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Display the rendering profile data in the level editor");
			ImGui::Indent(-10);

			ImGui::Text("");
			ImGui::Text("Multi-Monitor Support");
			ImGui::Indent(10);
			bool bTmp = pref.iAllowUndocking;
			if (ImGui::Checkbox("Enable Window Undocking", &bTmp)) {
				pref.iAllowUndocking = bTmp;
				if (pref.iAllowUndocking)
				{
					//PE: When enabling we need to reset layout, or docked windows display with no tabbar, and docking node can hide.
					//PE: This is caused by window where we set ImGuiDockNodeFlags_NoTabBar. so need reset nodes.
					refresh_gui_docking = 0;
					MaximiseWindow();
				}
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Allows you to move and dock/undock the various level editor windows");

			bTmp = pref.iDisableObjectLibraryViewport;
			if (ImGui::Checkbox("Disable Multi-Viewport", &bTmp)) {
				pref.iDisableObjectLibraryViewport = bTmp;
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Don't allow windows to be moved to another monitor or viewport");
			ImGui::Indent(-10);

			//

			ImGui::Text("");
			ImGui::Text("VR Support");
			ImGui::Indent(10);
			ImGui::Text("Ensure you have correctly configured your OpenXR runtime");
			ImGui::Text("as OpenXR needs to know which VR device you are using:");
			bool bVRFlag = false;
			if (g.gvrmode > 0) bVRFlag = true;
			if (ImGui::Checkbox("Enable Virtual Reality Support", &bVRFlag))
			{
				// toggle vr support
				if (bVRFlag == true) 
					g.gvrmode = 2;
				else
					g.gvrmode = 0;

				// save setting
				FPSC_SaveSETUPVRINI();

				// if been activated, init system (no need to relaunch with OpenXR)
				extern void vr_init(void);
				vr_init();
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Activates experimental support for VR via OpenXR");
			ImGui::Indent(-10);

			//

			if (g_iDevToolsOpen != 0)
			{
				ImGui::Text("");
				ImGui::Text("Developer Mode Tools");

				float save_gadget_size = ImGui::GetFontSize()*10.0;
				float w = ImGui::GetWindowContentRegionWidth();

				ImGui::Indent(10);
				w = ImGui::GetContentRegionAvailWidth();
				if (ImGui::StyleButton("Reset Auto Start Videos", ImVec2(save_gadget_size*1.3, 0))) 
				{
					CloseAllOpenTools();
					pref.iResetAutoRunVideosOnNextStartup = 1;
					bPreferences_Window = false;
				}
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("Reset introduction videos for so they play automatically once as though a new user");
				ImGui::Indent(-10);
			}

			ImGui::NextColumn();
			ImGui::Indent(10);

			//ImGui::Text("");
			ImGui::Text("Object Management Options");
			ImGui::Indent(10);
			bTmp = pref.iDragCameraMovement;
			if (ImGui::Checkbox("Use Drag Camera Movement", &bTmp)) 
			{
				pref.iDragCameraMovement = bTmp;
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Left click and drag the terrain to move the camera position around in the Level Editor");

			bTmp = pref.iEnableDragDropEntityMode;
			if (ImGui::Checkbox("Drag/Drop Entity Mode", &bTmp)) {
				pref.iEnableDragDropEntityMode = bTmp;
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "When ticked you can drag and drop objects, unticked you have to click to select an object then again to place it");

			if (pref.iEnableDragDropEntityMode)
			{
				ImGui::Indent(20);
				bTmp = pref.iEnableDragDropWidgetSelect;
				if (ImGui::Checkbox("Enable Widget When Needed", &bTmp)) {
					pref.iEnableDragDropWidgetSelect = bTmp;
				}
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Show the 3D widget for movement, rotation and scaling");
				ImGui::Indent(-20);

				ImGui::Indent(20);
				bTmp = pref.iEnableDragDropStopSelectFromInside;
				if (ImGui::Checkbox("Stop object from being moved when camera is inside", &bTmp)) {
					pref.iEnableDragDropStopSelectFromInside = bTmp;
				}
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Tick this if you want to avoid selecting objects (eg buildings) when viewed inside of them");
				ImGui::Indent(-20);
			}

			bTmp = pref.iEnableEditorOutlineSelection;
			if (ImGui::Checkbox("Use Outline For Selected Editor Objects", &bTmp)) {
				pref.iEnableEditorOutlineSelection = bTmp;
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Add a yellow outline around the currently selected object");

			if (pref.iEnableEditorOutlineSelection)
			{
				ImGui::Indent(20);
				ImGui::SliderFloat("##OutlineThicknessSmall", &pref.fHighLightThickness, 0.0, 6.0, "%.2f");
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Set the outline's thickness with this slider");
				ImGui::Indent(-20);
			}

			ImGui::Text("");
			ImGui::Text("Other Developer Options");

			bTmp = pref.iEnableFpsMemMonitor;
			if (ImGui::Checkbox("Show Live FPS And Memory Stats", &bTmp)) {
				pref.iEnableFpsMemMonitor = bTmp;
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Show Live FPS And Memory Stats");

			// new decal particles effect system (Preben may remove this if we can speed up/preload the loading here)
			if (ImGui::Checkbox("Disable Full Decal Effect Loading (Temporary)", &g_bTemporarilyDisableFullDecalEffectLoading)) 
			{
				// Either keep this option in the prefs, or better to speed up the loading of these effects
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Improved decal particle system takes time to load effects, can skip this temporarily");

			bTmp = pref.iEnableAutoExposureInEditor;
			if (ImGui::Checkbox("Use Auto Exposure in Editor", &bTmp)) {
				if (!pref.iEnableAutoExposureInEditor)
				{
					//Set autoexposure on if first time. if set in visuals.
					master_renderer->setEyeAdaptionEnabled(t.visuals.bAutoExposure);
				}
				pref.iEnableAutoExposureInEditor = bTmp;
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("The auto exposure effect can be turned on and off during editing of a level");

			bTmp = pref.iEnableDeveloperObjectTools;
			if (ImGui::Checkbox("Display Object Tools Developer Mode", &bTmp)) 
			{
				pref.iEnableDeveloperObjectTools = bTmp;
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Shows the developer section in the objects panel");
			
			bTmp = pref.iEnableDeveloperProperties;
			if (ImGui::Checkbox("Display AI Management in Test Level", &bTmp)) 
			{
				pref.iEnableDeveloperProperties = bTmp;
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Shows the AI management options and gives access to Behavior Editing");

			bTmp = pref.iDisableProjectAutoSave;
			if (ImGui::Checkbox("Disable Storyboard Auto Save", &bTmp)) {
				pref.iDisableProjectAutoSave = bTmp;
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Turn on and off the auto save system");

			ImGui::Text("");
			ImGui::Text("New Objects Placed (objects already placed keep their own settings)");
			const char* pDefaultChoices[] = { "Automatic", "On", "Off" };
			ImGui::PushItemWidth(ImGui::GetFontSize() * 8.0f);
			ImGui::Combo("Allow Bullet Holes, if the .fpe does not say", &g_iDefaultBulletHoles, pDefaultChoices, 3);
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "How the Allow Bullet Holes box starts on objects placed from now on whose .fpe has no allowbulletholes line; objects already placed keep theirs. Automatic: static objects only. On: every object but characters");
			ImGui::Combo("Clear Grass and Trees, if the .fpe does not say", &g_iDefaultClearVegetation, pDefaultChoices, 3);
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "How the Clear Grass and Trees box starts on objects placed from now on whose .fpe has no clearvegetation line; objects already placed keep theirs. Automatic: static objects at least 3m across and 2.5m tall. On: every object but markers, characters and decals");
			ImGui::PopItemWidth();

			if (g_iDevToolsOpen)
			{
				bTmp = pref.iAdvancedGridModeSettings;// iTerrainDebugMode;
				if (ImGui::Checkbox("Enable Advanced Grid Mode", &bTmp)) {
					pref.iAdvancedGridModeSettings = bTmp;// iTerrainDebugMode = bTmp;
				}
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Enables advanced functions and multi-axis settings in grid mode");
			}
			else
			{
				pref.iAdvancedGridModeSettings = 0;// iTerrainDebugMode = 0;
			}

			
			ImGui::Indent(-10);
			ImGui::Columns(1);
			ImGui::EndTabItem();
		}


		ImGui::EndTabBar();
	}

	//ImGui::Separator();
	ImGui::Text("");
	if (strlen(cPreferencesMessage) > 0)
	{
		ImGui::SetWindowFontScale(1.25);
		ImGui::TextCenter(cPreferencesMessage);
		ImGui::SetWindowFontScale(1.0);
	}
	ImVec2 ws = ImGui::GetWindowSize();
	ImGui::Indent();
	if (ImGui::GetCursorPosY() < ws.y - (fs * 4))
		ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX(), ws.y - (fs * 4) + ImGui::GetScrollY()));

	float save_gadget_size = ImGui::GetFontSize()*10.0;

	if (ImGui::StyleButton("Reset to Defaults", ImVec2(save_gadget_size,0.0f)) )
	{
		int iAction = askBoxCancel("This will change all your settings back to the defaults, are you sure?", "Confirmation"); //1==Yes 2=Cancel 0=No
		if (iAction == 1)
		{
			pref.current_style = 25; // 0

			pref.tint_style = ImVec4(1.0, 1.0, 1.0, 1.0);
			pref.shade_style = ImVec4(0.0, 0.0, 0.0, 0.0);
			pref.title_style = ImVec4(0.0, 0.0, 0.0, 0.0);

			pref.iMaximized = 1;
			pref.bHideTutorials = false;
			pref.bMultiplyOpenHeaders = false;
			pref.bAutoClosePropertySections = true;
			pref.bAutoOpenMenuItems = true;
			pref.bDisableMultipleViewport = false;
			pref.iTurnOffUITransparent = false;
			pref.iEnableCustomColors = 0;
			pref.iEnableAdvancedSky = 0;
			pref.iEnableAdvancedWater = 0;
			pref.iEnableAdvancedPostProcessing = 0;
			pref.iEnableAdvancedShadows = 0;
			pref.iObjectEnableAdvanced = 0;
			pref.iEnableArcRelationshipLines = 1;
			pref.iAllowUndocking = 0;
			pref.iEnableAxisRotationShortcuts = 0;

			pref.iEnableDragDropWidgetSelect = 0;

			pref.iEnableEditorOutlineSelection = 1;
			pref.iEnableSingleRightPanelAdvanced = 0;

			pref.iEnableDragDropEntityMode = 1;

			strcpy(pref.cCustomWriteFolder, "");
			SetUpdaterWritePathFile(pref.cCustomWriteFolder);
			strcpy(pref.cDefaultImportPath, "");
			strcpy(pref.cDefaultStandalonePath, "");
			strcpy(pref.cRememberLastSearchObjects, "");

			for (int i = 0; i < 10; i++)
				pref.iCheckboxFilters[i] = 1;

			pref.iEnableDeveloperProperties = 0;
			pref.iEnableIdentityProperties = 1;
			pref.iDragCameraMovement = 1;

			pref.iGameCreaterStore = 0;
			pref.iFullscreenPreviewAdvanced = 0;
			pref.iDisplayWelcomeScreen = 1;
			pref.iDisplayIntroScreen = 1;
			pref.iImporterDome = 1;
			pref.iTerrainAdvanced = 0;
			pref.iAdvancedGridModeSettings = 0;// iTerrainDebugMode = 0;
			pref.iEnableAdvancedCharacterCreator = 0;
			pref.iStoryboardAdvanced = 0;
			pref.iDisableProjectAutoSave = 0;
			pref.iDisableLevelAutoSave = 0;

			pref.iSplashStartMessage = 0;
			pref.iTitleStartMessage = 0;
			pref.iLoadingStartMessage = 0;
			pref.iGameWonStartMessage = 0;
			pref.iGameOverStartMessage = 0;

			pref.iHideKeyboardShortcuts = 0;
			pref.iEnableDeveloperObjectTools = 0;
			pref.iEnableTerrainHeightmaps = 0;

			for (int il = 0;il < 16; il++)
			{
				pref.vSaved_Light_Palette_R[il] = 1.0f;
				pref.vSaved_Light_Palette_G[il] = 1.0f;
				pref.vSaved_Light_Palette_B[il] = 1.0f;
				pref.iSaved_Light_Type[il] = -1;
				pref.iSaved_Light_Range[il] = 0;
				pref.fSaved_Light_ProbeScale[il] = 1.0f;
				pref.iSaved_Light_Radius[il] = 45;
				pref.fSaved_Light_AngX[il] = 90;
				pref.fSaved_Light_AngY[il] = 0;
				pref.fSaved_Light_AngZ[il] = 0;
			}

			for (int il = 0;il < 16; il++)
			{
				strcpy(pref.Saved_Particle_Name[il], "");
				pref.Saved_bParticle_Preview[il] = true;
				pref.Saved_bParticle_Show_At_Start[il] = true;
				pref.Saved_bParticle_Looping_Animation[il] = true;
				pref.Saved_bParticle_Full_Screen[il] = false;
				pref.Saved_fParticle_Fullscreen_Duration[il] = 10.0f;
				pref.Saved_fParticle_Fullscreen_Fadein[il] = 1.0f;
				pref.Saved_fParticle_Fullscreen_Fadeout[il] = 1.0f;
				strcpy(pref.Saved_Particle_Fullscreen_Transition[il], "");
				pref.Saved_fParticle_Speed[il] = 1.0f;
				pref.Saved_fParticle_Opacity[il] = 1.0f;
			}

			pref.iEnableAdvancedEntityList = 0;
			pref.fHighLightThickness = 1.0;
			pref.iTurnOffEditboxTooltip = false;
			pref.iEnableAutoExposureInEditor = 0;
			pref.iSetColumnsEntityLib = 3;

			pref.iDisableObjectLibraryViewport = 1;
			strcpy(pref.cLastUsedStoryboardProject, "");

			pref.iEnableLevelEditorOpenAndNew = 0;
			pref.iDisplayTerrainGeneratorWelcome = 1;
			pref.iTestGameGraphicsQuality = 2;
			pref.iEnableAutoFlattenSystem = 1;

			pref.iEnableAdvancedGrass = 0;
			pref.iEnableFpsMemMonitor = 0;
			pref.iEnableAutoFlattenSystem = 1;
			pref.iProjectSortMode = 0;

			pref.iEnableDragDropStopSelectFromInside = 0;

			if (pref.current_style == 0 || pref.current_style >= 10 && pref.current_style != 25) {
				//myStyle2_colors_only();
				myStyle2(NULL);
			}
			if (pref.current_style == 1) {
				void DarkColorsNoTransparent(void);
				myStyle2(NULL);
				DarkColorsNoTransparent();
			}
			if (pref.current_style == 9)
			{
				myDarkStyle(NULL);
			}

			if (pref.current_style == 3) {
				myLightStyle(NULL);
			}
			if (pref.current_style == 25) {
				myStyleBlue(NULL);
			}

			SetIconSet();

			refresh_gui_docking = 0;
			if (bProfilerEnable) {
				bProfilerEnable = false;
				wiProfiler::SetEnabled(bProfilerEnable);
			}
			MaximiseWindow();
		}
	}
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("Reset everything to default values");

	vLastWindowSize = ImGui::GetWindowSize();
	fLastContentWidth = ImGui::GetContentRegionAvailWidth();

	bImGuiGotFocus = true;

	ImGui::End();
	if (bDigAHoleToHWND && bwindow)
		bwindow->DrawList->AddCallback((ImDrawCallback)11, NULL); //disable force render.

}

void CloseAllOpenTools(bool bTerrainTools)
{
	if (bWaypointDrawmode || bWaypoint_Window) { bWaypointDrawmode = false; bWaypoint_Window = false; }
	if (bImporter_Window) { importer_quit(); bImporter_Window = false; }
	if (g_bCharacterCreatorPlusActivated) g_bCharacterCreatorPlusActivated = false;
	if (bEntity_Properties_Window) bEntity_Properties_Window = false;
	if (t.ebe.on == 1) ebe_hide();
	if (bTerrainTools)
	{
		if (bTerrain_Tools_Window) bTerrain_Tools_Window = false;
	}
	if (Shooter_Tools_Window)
		Shooter_Tools_Window = false;
}

void CloseAllOpenToolsThatNeedSave(void)
{
	if (bImporter_Window) { importer_quit(); bImporter_Window = false; }
	if (g_bCharacterCreatorPlusActivated) g_bCharacterCreatorPlusActivated = false;
	if (t.ebe.on == 1) ebe_hide();
}

void imgui_shooter_tools(void)
{
	if (ImGui::windowTabVisible())
	{
		Shooter_Tools_Window_Active = true;

		// if have selection selected
		bool bRubberBand = false;
		bool bToolsOpen = false;
		int iEntID = 0;
		int iEntIndex = t.widget.pickedEntityIndex;
		if (!pref.iEnableRelationPopupWindow && bDotMiddleWindow && g_selected_middle_dot_pobject)
		{
			int iDotMiddleIndex = g_selected_middle_dot_pobject->dwObjectNumber - DOTMIDDLEOBJECTID;
			if (iDotMiddleIndex >= 0 && iDotMiddleIndex < MAXDOTMIDDLE)
			{
				if (ImGui::StyleCollapsingHeader("Shooter Connections", ImGuiTreeNodeFlags_DefaultOpen))
				{
					ImGui::Indent(10);
					DisplayRelationshipMenu(iDotMiddleIndex, 0);
					ImGui::Indent(-10);
				}
			}
		}
		else if (t.widget.pickedObject != 0)
		{
			if (iEntIndex > 0)
			{
				iEntID = t.entityelement[iEntIndex].bankindex;
				if (iEntID > 0)
					if (t.entityprofile[iEntID].ischaracter > 0)
						bToolsOpen = true;
			}
			// rubber band
			if (g.entityrubberbandlist.size() > 0)
			{
				bRubberBand = true;
				//Any chars selected.
				for (int i = 0; i < (int)g.entityrubberbandlist.size(); i++)
				{
					int e = g.entityrubberbandlist[i].e;
					if (e > 0)
					{
						int tobj = t.entityelement[e].obj;
						if (t.entityprofile[t.entityelement[e].bankindex].ischaracter > 0)
						{
							bToolsOpen = true;
						}
					}
				}
			}
			if (!bRubberBand && iEntIndex <= 0)
				bToolsOpen = false;
		}
	}
	else
	{
		Shooter_Tools_Window_Active = false;
	}
}

void DeleteWaypointsAddedToCurrentCursor(void)
{
	//PE: Delete any waypoints added to current cursor.
	if (t.grideditselect == 5 && t.gridentity > 0 && t.grideleprof.trigger.waypointzoneindex > 0)
	{
		t.waypointindex = t.grideleprof.trigger.waypointzoneindex;
		if (t.waypointindex > 0)
		{
			t.w = t.waypoint[t.waypointindex].start;
			waypoint_delete();
		}
		t.grideleprof.trigger.waypointzoneindex = 0;
	}
}

// New Logic System - Visual Relational Lines

std::multimap<std::int32_t, std::int32_t> arcs_relations;
struct NodeConnection
{
	std::int32_t from;
	std::int32_t to;
	std::int32_t nodeconnectionid;
	std::int32_t middle;
};
std::vector<NodeConnection> nodeconnections;
void AddDotObjectRelation(int sobj, int dobj);
void FindDotObjectRelation(int sobj, int dobj,int middle_index);
void GetMiddleEntityIdAndRelationshipId(int sobj, int dobj, int &Entid, int &ReleationshipId, int& NodeConnectionId, int* NodeConnectionIndex = nullptr);
void DrawObjectRelation(int from, int to, bool bDrawMiddleBut, int forceVertUpdate = 0);
void CreateDotMiddleObject(int obj);
void deleterelationobjects();

float fHighlightCount = 0.0f;
int iTotalArcs = 0;
int iTotalRelationObjects = 0; // The first node connection object is reserved for editing with cursor movement.
int iTotalMiddle = 0;
int iLargestArcs = 0;
int iLargestMiddle = 0;
int iCurrentDotCount = 0;
float fRelationUVs[24] = { 0 };
float fRelationUVsStorage[24] = { 0 };
bool bUVsAlreadySet = false;
float fUVCounter = 0.0f;

int GenerateRelationshipUniqueLinkID (void)
{
	// search all current entities and relationship links to find unique ID not used
	int iUniqueID = 42000;
	for (int e = 1; e <= g.entityelementlist; e++)
	{
		int iBankIndex = t.entityelement[e].bankindex;
		if (iBankIndex > 0)
		{
			int iObj = t.entityelement[e].obj;
			if (iObj > 0)
			{
				int iLinkID = t.entityelement[e].eleprof.iObjectLinkID;
				if (iLinkID > iUniqueID) iUniqueID = iLinkID;
				for (int i = 0; i < 10; i++)
				{
					iLinkID = t.entityelement[e].eleprof.iObjectRelationships[i];
					if (iLinkID > iUniqueID) iUniqueID = iLinkID;
				}
			}
		}
	}
	iUniqueID += 1;
	return iUniqueID;
}

void GetRelationshipObject (int iFindLinkID, int* piEntityID, int* piObj)
{
	// extra feature that if 'piObj' passes a value, discover correct 'piEntityID' (as in the past some corruption caused duplicate 'iFindLinkID' values)
	int iMatchPassedInObjID = 0;
	if (*piObj > 0) iMatchPassedInObjID = *piObj;

	*piEntityID = 0;
	*piObj = 0;
	if (iFindLinkID > 0)
	{
		for (int e = 1; e <= g.entityelementlist; e++)
		{
			int iObj = t.entityelement[e].obj;
			if (iObj > 0)
			{
				int iLinkID = t.entityelement[e].eleprof.iObjectLinkID;
				if (iLinkID == iFindLinkID && (iMatchPassedInObjID==0 || (iMatchPassedInObjID>0 && iMatchPassedInObjID == iObj)))
				{
					*piEntityID = e;
					*piObj = iObj;
					return;
				}
			}
		}
	}
}

void DrawLogicNodes(bool bVisible)
{
	//PE: Use 110000 for dot objects.
	//PE: Use 130000 for arcs.
	if (iCursorDotObject <= 0)
	{
		//PE: No collision on this one.
		WickedCall_PresetObjectRenderLayer(GGRENDERLAYERS_CURSOROBJECT);
		CreateDotObject(DOTCURSOROBJECTID);
		WickedCall_PresetObjectRenderLayer(GGRENDERLAYERS_NORMAL);
		HideObject(DOTCURSOROBJECTID);
		iCursorDotObject = DOTCURSOROBJECTID;
	}

	MoveSelectedDotObject();

	iTotalMiddle = 0;

	if (arcs_relations.size() > 0)
	{
		//Hide dot objects.
		for (std::multimap<std::int32_t, std::int32_t>::iterator it = arcs_relations.begin(); it != arcs_relations.end(); ++it)
		{
			if (it->first > 0 && it->second > 0)
			{
				HideObject(it->first);
				HideObject(it->second);
			}
		}
	}

	arcs_relations.clear();

	for (int i = 0; i < iLargestMiddle; i++)
	{
		HideObject(DOTMIDDLEOBJECTID + i);
		HideObject(RELATIONOBJECTID + i);
	}

	if (iLargestDotCount != iCurrentDotCount)
	{
		//Something has changed, cleanup.
		iLargestDotCount = iCurrentDotCount;
		int iKeepInMem = 1000; //PE: Always keep upto 1000, delete the rest.
		int iStartObj = 70001;
		for (int i = iStartObj; i < iStartObj + MAXDOTARCSOBJECTS; i++)
		{
			if (ObjectExist(i + DOTOBJECTIDADD))
			{
				if (iKeepInMem-- > 0)
					HideObject(i + DOTOBJECTIDADD);
				else
					DeleteObject(i + DOTOBJECTIDADD);
			}
		}
	}

	int iCurrentLargestDotObjectID = 70001;
	iCurrentDotCount = 0;

	for (int iEntityID = 1; iEntityID <= g.entityelementlist; iEntityID++)
	{
		int iBankIndex = t.entityelement[iEntityID].bankindex;
		int iMasterObject = g.entitybankoffset + t.entityelement[iEntityID].bankindex;
		int iObjectLinkID = t.entityelement[iEntityID].eleprof.iObjectLinkID;
		int iEntityObject = t.entityelement[iEntityID].obj;
		if (iEntityObject > 0)
		{
			if (iEntityObject > iCurrentLargestDotObjectID)
				iCurrentLargestDotObjectID = iEntityObject;
			if (iEntityObject > iLargestDotObjectID)
				iLargestDotObjectID = iEntityObject;
			if (ObjectExist(iEntityObject) == 1)
			{
				iCurrentDotCount++;

				bool bObjectNeedDot = false;
				int iColorType = -1;
				if (t.entityelement[iEntityID].staticflag == 0)
				{
					// only dynamic objects can interact and have dot!
					if (t.entityprofile[iBankIndex].ischaracter == 1)
					{
						iColorType = 1;
						bObjectNeedDot = true;
					}
					else if (t.entityprofile[iBankIndex].ismarker == 11) //Flags
					{
						iColorType = 2;
						bObjectNeedDot = true;
					}
					else if (t.entityprofile[iBankIndex].ismarker == 3) //Trigger zone
					{
						iColorType = 3;
						bObjectNeedDot = true;
					}
					else
					{
						//PE: Everything else is a object type.
						//PE: Adding dots to these would be a problem , like stacked boxes ...
						iColorType = 4;
						bObjectNeedDot = true;
					}
				}

				if (bObjectNeedDot)
				{
					CreateDotObject(iEntityObject + DOTOBJECTIDADD);
					PositionDotObject(iEntityObject + DOTOBJECTIDADD);
					if (bVisible)
					{
						int iConnectionsCount = 0;
						for (int i = 0; i < 10; i++)
						{
							if (t.entityelement[iEntityID].eleprof.iObjectRelationships[i] > 0)
							{
								int iRelationShipObject = 0, iRelationShipEntityID = 0;
								GetRelationshipObject(t.entityelement[iEntityID].eleprof.iObjectRelationships[i], &iRelationShipEntityID, &iRelationShipObject);
								int iRelationshipLinkID = t.entityelement[iEntityID].eleprof.iObjectRelationships[i];
								if (iObjectLinkID != iRelationshipLinkID)
								{
									//PE: If we got a reverse already dont add to arcs list.
									bool bAlreadyThere = false;
									for (std::multimap<std::int32_t, std::int32_t>::iterator it = arcs_relations.begin(); it != arcs_relations.end(); ++it)
									{
										if (it->first == iRelationShipObject + DOTOBJECTIDADD && it->second == iEntityObject + DOTOBJECTIDADD)
										{
											bAlreadyThere = true;
											break;
										}
									}
									if (!bAlreadyThere)
									{
										//Validate reverse relation.
										int iEntIDd = 0;
										int iRelationIDd = 0;
										int iNodeConnectionIDd = 0;
										GetMiddleEntityIdAndRelationshipId(iRelationShipObject, iEntityObject, iEntIDd, iRelationIDd, iNodeConnectionIDd);
										if (iEntIDd > 0)
										{
											//PE: ok
											arcs_relations.insert(std::make_pair(iEntityObject + DOTOBJECTIDADD, iRelationShipObject + DOTOBJECTIDADD));
										}
									}
								}
								iConnectionsCount++;
							}
						}
						ShowObject(iEntityObject + DOTOBJECTIDADD);
					}
					else
					{
						HideObject(iEntityObject + DOTOBJECTIDADD);
					}
				}
			}
		}
	}
	
	if (iLargestDotObjectID > iCurrentLargestDotObjectID)
	{
		//PE: We got a deleted object, make sure to release all not used dots.
		for (int i = iCurrentLargestDotObjectID; i < iLargestDotObjectID; i++)
		{
			if (i >= 70001 && i <= 90000) //Secure range.
			{
				if (ObjectExist(i + DOTOBJECTIDADD))
				{
					DeleteObject(i + DOTOBJECTIDADD);
				}
			}
		}
		iLargestDotObjectID = iCurrentLargestDotObjectID;
	}

	// Update the reference UVs for the node connection objects.
	if (fUVCounter >= 1.0f)
	{
		for (int i = 0; i < 24; i++)
		{
			fRelationUVs[i] = fRelationUVsStorage[i];
		}

		fUVCounter = 0.0f;
	}

	for (int i = 0; i < 24; i++)
	{
		if (i < 12)
			fRelationUVs[i] += (0.2f * ImGui::GetIO().DeltaTime);
		else
			fRelationUVs[i] -= (0.2f * ImGui::GetIO().DeltaTime);
	}

	fUVCounter += (0.2f * ImGui::GetIO().DeltaTime);

	if (arcs_relations.size() > 0)
	{
		// When an object gets deleted and another takes its place, it can have the same object number...
		// ...which can prevent the relation object vertices from being updated
		static int vertUpdateCounter = 0;
		vertUpdateCounter++;
		if (vertUpdateCounter > 30)
			vertUpdateCounter = 0;

		//Draw connections.
		for (std::multimap<std::int32_t, std::int32_t>::iterator it = arcs_relations.begin(); it != arcs_relations.end(); ++it)
		{
			if (it->first > 0 && it->second > 0)
			{	
				if (vertUpdateCounter < 30)
				{
					DrawObjectRelation(it->first, it->second, true);
				}
				else
				{
					// vertUpdateCounter forces all the verts to be updated.
					DrawObjectRelation(it->first, it->second, true, 1);
				}
			}
		}
	}

	// Hide any relation objects that are no longer used.
	if (arcs_relations.empty())
	{
		for (int i = 0; i < nodeconnections.size(); i++)
		{
			HideObject(RELATIONOBJECTID + iTotalMiddle + 1 + i);
		}
	}
	else if (arcs_relations.size() < nodeconnections.size())
	{
		for (int i = arcs_relations.size() - 1; i < nodeconnections.size(); i++)
		{
			// Hide any relation objects no longer used (but don't delete so they can be reused).
			HideObject(RELATIONOBJECTID + iTotalMiddle + 1 + i);
		}
	}

	if (g_source_dot_pobject && bDotObjectDragging && !bDraggingActive)
		DrawObjectRelation(g_source_dot_pobject->dwObjectNumber, DOTCURSOROBJECTID, false);
	else if (!bDotObjectDragging)
		HideObject(RELATIONOBJECTID);
}

void PositionDotObject(int obj)
{
	if (ObjectExist(obj))
	{
		int iRealObjID = obj - DOTOBJECTIDADD;
		if (ObjectExist(iRealObjID))
		{
			bool bRestoreDotTOObjectCenter = true;
			if (g_hovered_pobject)
			{
				if (iRealObjID == g_hovered_pobject->dwObjectNumber)
				{
					PositionObject(obj, fLastHitPosition[0], fLastHitPosition[1], fLastHitPosition[2]);
					bRestoreDotTOObjectCenter = false;
				}
			}
			else if (g_hovered_dot_pobject)
			{
				if (obj == g_hovered_dot_pobject->dwObjectNumber)
				{
					// instruction what to do 
					pastebitmapfontcenter("Connect To Create Logic", GetScreenX(obj), GetScreenY(obj) + 10, 2, 255);
					bRestoreDotTOObjectCenter = false;
				}
			}
			if (bRestoreDotTOObjectCenter == true )
			{
				PositionObject(obj, ObjectPositionX(iRealObjID) + GetObjectCollisionCenterX(iRealObjID), ObjectPositionY(iRealObjID) + GetObjectCollisionCenterY(iRealObjID), ObjectPositionZ(iRealObjID) + GetObjectCollisionCenterZ(iRealObjID));
			}
		}
	}
}

void CreateDotMiddleObject(int obj)
{
	if (ObjectExist(obj) == 0)
	{
		if (ObjectExist(g.gameplayparentobjects + 2) == 0)
		{
			LoadObject("editors\\uiv3\\brain_logic_marker.dbo", g.gameplayparentobjects + 2);
			ScaleObject(g.gameplayparentobjects + 2, 25, 25, 25);
			HideObject(g.gameplayparentobjects + 2);
		}
		CloneObject(obj, g.gameplayparentobjects + 2);
		DisableObjectZRead(obj);
		HideObject(obj);
		SetObjectMask(obj, 1);
		SetObjectTransparency(obj, 6);
		SetObjectEffect (obj, g.decaleffectoffset);
		DisableObjectZDepth (obj);
		SetObjectCull(obj, 0);
		TextureObject(obj, UI3D_DOTMIDDLEOBJECTS);
		sObject* pDotObject = GetObjectData(obj);
		if (pDotObject)
		{
			WickedCall_SetObjectCastShadows(pDotObject, false);
			WickedCall_SetObjectLightToUnlit(pDotObject, (int)wiScene::MaterialComponent::SHADERTYPE_UNLIT);
		}
	}
}

void CreateDotObject(int obj)
{
	if (ObjectExist(obj) == 0)
	{
		if (ObjectExist(g.gameplayparentobjects + 0) == 0)
		{
			MakeObjectSphere(g.gameplayparentobjects + 0, 5, 5, 5);
			HideObject(g.gameplayparentobjects + 0); //PE: Hide object its visible on maps.
		}
		CloneObject(obj, g.gameplayparentobjects + 0);
		HideObject(obj);
		SetObjectMask(obj, 1);
		SetObjectEffect(obj, g.guishadereffectindex);
		SetObjectMask(obj, 1);
		
		DisableObjectZDepth (obj);
		SetObjectCull(obj, 0);
		TextureObject(obj, UI3D_DOTOBJECTS);// UI3D_DOTMIDDLEOBJECTS);

		sObject* pDotObject = GetObjectData(obj);
		if (pDotObject)
		{
			WickedCall_SetObjectCastShadows(pDotObject, false);
			WickedCall_SetObjectLightToUnlit(pDotObject, (int)wiScene::MaterialComponent::SHADERTYPE::SHADERTYPE_UNLIT);
		}
	}
}

void CreateDotArcObject(int obj)
{
	if (ObjectExist(obj) == 0)
	{
		float fSphereSize = 15.0f;
		WickedCall_PresetObjectRenderLayer(GGRENDERLAYERS_CURSOROBJECT);
		if (ObjectExist(g.gameplayparentobjects + 1) == 0)
		{		
			//LoadObject("editors\\uiv3\\dotpipe.dbo", g.gameplayparentobjects + 1);
			LoadObject("editors\\uiv3\\dotobject.dbo", g.gameplayparentobjects + 1);
			ScaleObject(g.gameplayparentobjects + 1, 25, 25, 25);
		}
		CloneObject(obj, g.gameplayparentobjects + 1);
		WickedCall_PresetObjectRenderLayer(GGRENDERLAYERS_NORMAL);
		DisableObjectZDepth (obj);
		HideObject(obj);
		SetObjectMask(obj, 1);
		SetObjectEffect(obj, g.guishadereffectindex);
		TextureObject(obj, UI3D_DOTOBJECTS);
		SetObjectMask(obj, 1);
		sObject* pDotObject = GetObjectData(obj);
		if (pDotObject)
		{
			WickedCall_SetObjectCastShadows(pDotObject, false);
		}
	}
}

