int autosave_storyboard_project(void)
{
	int iAction = 0;
	if (!Storyboard.iChanged) return iAction;
	if (!pref.iDisableProjectAutoSave && strlen(Storyboard.gamename) > 0)
	{
		save_storyboard(Storyboard.gamename, false);
	}
	else
	{
		if (strlen(Storyboard.gamename) > 0)
		{
			iAction = askBoxCancel(STORYBOARD_SAVE_MESSAGE, "Confirmation"); //1==Yes 2=Cancel 0=No
			if (iAction == 1)
			{
				save_storyboard(Storyboard.gamename, false);
			}
		}
	}
	return iAction;
}

int GetStoryboardCustomScreenNode(char *page)
{
	if( pestrcasestr(page, "custom") && strlen(page) <= 8 )
	{
		int iNode = FindLuaScreenNode(page);
		if (iNode >= 0 && strlen(Storyboard.gamename) > 0)
		{
			return(iNode);
		}
	}
	return -1;
}

void GetStoryboardCustomScreenNodeName(int iNode, char* pRealNameStr)
{
	strcpy(pRealNameStr, Storyboard.Nodes[iNode].lua_name);
	if (strnicmp(pRealNameStr + strlen(pRealNameStr) - 4, ".lua", 4) == NULL)
	{
		// chop any .lua extension
		pRealNameStr[strlen(pRealNameStr) - 4] = 0;
	}
}

bool bTempDisableRain = false;
bool bTempDisableSnow = false;

void update_per_frame_effects(void)
{
}

#ifdef STANDALONENOTICE
void early_access_strandalone_welcome( void )
{
	void StartForceRender(void);
	bool quit = false;
	bool bBrowserStarted = false;
	image_setlegacyimageloading(true);
	LoadImage("editors\\uiv3\\standalone_ea-ea.png", SKYBOX_ICONS+99);
	image_setlegacyimageloading(false);

	while (!quit)
	{
		if (!bRenderTabTab && !bImGuiFrameState)
		{
			//We need a new frame.
			ImGui_ImplDX11_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();
			bRenderTabTab = true;
			bBlockImGuiUntilNewFrame = false;
			extern bool bSpriteWinVisible;
			bSpriteWinVisible = false;
			bImGuiRenderWithNoCustomTextures = false;
		}
		//From lua, we need to update imgui mousepos ...
		int mclick = 0;
		ImGuiIO& io = ImGui::GetIO();
		io.MouseDown[0] = 0;
		io.MouseDown[1] = 0;
		io.MouseDown[2] = 0;
		io.MouseDown[3] = 0;

		mclick = MouseClick();

		if (mclick == 1) io.MouseDown[0] = 1;
		if (mclick == 2) io.MouseDown[1] = 1;
		if (mclick == 4) io.MouseDown[2] = 1;

		ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowViewport(viewport->ID);
		ImVec2 viewPortPos = ImGui::GetMainViewport()->Pos;
		ImVec2 viewPortSize = ImGui::GetMainViewport()->Size;
		ImGui::SetNextWindowPos(viewPortPos, ImGuiCond_Always);
		ImGui::SetNextWindowSize(viewPortSize, ImGuiCond_Always);

		ImGui::Begin("##Early Access Version", &quit, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoResize);

		ImVec2 cpos = ImGui::GetCursorPos();
		float img_w = 1260;
		float img_h = 900;
		ImVec2 img_pos = ImVec2(0.0, 0.0);

		ID3D11ShaderResourceView* lpTexture = GetImagePointerView(SKYBOX_ICONS + 99);
		if (lpTexture)
		{
			//Support center,stretch,zoom
			//Storyboard.Nodes[nodeid].screen_backdrop_placement
			//Center;

			img_w = ImageWidth(SKYBOX_ICONS + 99);
			img_h = ImageHeight(SKYBOX_ICONS + 99);

			ImVec2 vSize = ImGui::GetMainViewport()->Size;

			if (img_w > vSize.x || img_h > vSize.y) {
				float fRatio = 1.0f / (img_w / img_h);
				img_w = vSize.x;
				img_h = vSize.x * fRatio;
				if (img_h > vSize.y) {
					float fRatio = 1.0f / (img_h / img_w);
					img_h = vSize.y;
					img_w = vSize.y * fRatio;
				}
			}
			img_pos = ImVec2(0.0, 0.0);
			img_pos.x += (vSize.x - img_w) * 0.5;
			img_pos.y += (vSize.y - img_h) * 0.5;
			ImGuiWindow* window = ImGui::GetCurrentWindow();
			window->DrawList->AddImage((ImTextureID)lpTexture, img_pos, img_pos + ImVec2(img_w, img_h), ImVec2(0, 0), ImVec2(1, 1), ImGui::GetColorU32(ImVec4(1.0, 1.0, 1.0, 1.0)));
			cpos = img_pos + ImVec2(0, img_h-260.0);
		}
		else
		{
			ImGui::Dummy(ImVec2(1250, 900));
		}
		ImGui::SetCursorPos(cpos);

		ImGui::SetNextWindowPos(img_pos + ImVec2(0, img_h - 280.0), ImGuiCond_Always);
		ImGui::BeginChild("##DummyEAwelcomechild", ImVec2(img_w-20,280.0), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground);
		ImGui::Indent(20);
		ImGui::Text("");
		ImGui::SetWindowFontScale(1.3);
		//ImGui::TextWrapped("PLEASE NOTE\nThis game project has been exported from the Early Access version of GameGuru MAX and is for test purposes only. You should not have been charged to play this game and the quality of the game and performance does not reflect the final quality of GameGuru MAX games.");
		ImGui::TextWrapped("IMPORTANT - PLEASE NOTE!\nDuring the Early Access development phase of GameGuru MAX, all games saved as standalone are considered test games and must not be sold. The games will include a startup message informing users that the game has been made with an Early Access version. This restriction will be lifted once we leave Early Access.\n\nIf you develop a full game and would like to apply for it to be published before the full release of GameGuru MAX then please follow this LINK and fill out the short form. \n\nPlease ensure that anyone you share your standalone game with has a PC system that meets the minimum requirements of GameGuru MAX.");
		ImVec2 prevCursor = ImGui::GetCursorPos();
		ImGui::SetCursorPos(prevCursor + ImVec2(1006, -96));
		if (ImGui::HyberlinkButton("", ImVec2(ImGui::GetFontSize() * 2.2f, ImGui::GetFontSize() * 1.5f)))
		{
			ExecuteFile("https://www.game-guru.com/publish-request", 0, 0);
		}
		ImGui::SetCursorPos(prevCursor);
		ImGui::Text("");
		ImGui::SetWindowFontScale(1.6);
		float buttonwide = ImGui::GetContentRegionAvail().x*0.5 - 10.0f;
		if (ImGui::StyleButton("VISIT GAMEGURU MAX WEBSITE", ImVec2(buttonwide, 0)))
		{
			/*ExecuteFile("https://www.game-guru.com/max", "", "", 0);*/
			ExecuteFile("https://bit.ly/MAXWebsite", "", "", 0);
		
			Sleep(1000); //Give it time to startup.
			bBrowserStarted = true;
		}
		ImGui::SameLine();
		ImGui::SetCursorPos(ImGui::GetCursorPos() +ImVec2(10.0,0.0));
		if (ImGui::StyleButton("CONTINUE", ImVec2(buttonwide, 0)))
		{
			quit = true;
		}

		ImGui::SetWindowFontScale(1.0);
		ImGui::RenderMouseCursor(ImVec2(io.MousePos.x, io.MousePos.y), 1.0);
		ImGui::Indent(-20);
		ImGui::EndChild();
		ImGui::End();

		StartForceRender();
		if (bBrowserStarted) Sleep(15); //Hand over time to browser.
	}
	if (ImageExist(SKYBOX_ICONS + 99))
	{
		StartForceRender();
		DeleteImage(SKYBOX_ICONS + 99);
	}
	bBlockImGuiUntilNewFrame = true;

}
#endif




void HandleObjectDeletion()
{
	//  delete selected entity via delete key
	bool bNoDelete = false;
	static bool bWaitOnDelRelease = false;
	if (bWaitOnDelRelease && t.inputsys.kscancode == 211)
		bNoDelete = true;
	else
		bWaitOnDelRelease = false;

	if (!bNoDelete && t.onetimeentitypickup == 0)
	{
		if (t.gridentity != 0)
		{
			if (t.inputsys.kscancode == 211 || iExecuteCTRLkey == ImGuiKey_Delete)
			{
				t.inputsys.mclickreleasestate = 1;
				t.gridentitydelete = 1;
				t.selstage = 1;
				t.inputsys.kscancode = 0;
				t.widget.pickedObject = 0; //dont remove widget object.
				bWaitOnDelRelease = true;
			}
		}
		else
		{
			if (t.inputsys.kscancode == 211 || (t.widget.deletebuttonselected == 1 && t.inputsys.mclick == 0) || iExecuteCTRLkey == ImGuiKey_Delete)
			{
				t.widget.deletebuttonselected = 0;
				bool bContinueWithDelete = true;
				if (t.widget.pickedEntityIndex > 0)
				{
					// specifically avoid deleting child entities if highlighting a parent
					if (g.entityrubberbandlist.size() > 0)
					{
						bool bDisableRubberBandMoving = false;
						if (current_selected_group >= 0 && group_editing_on)
						{
							bDisableRubberBandMoving = true;
						}
						if (!bDisableRubberBandMoving)
						{
							//LB: to ensure cannot delete objects that are part of a group, 
							// check if the group is a parent group (user can delete child groups okay)
							// before make final decision, see if a parent group can hand over control to one of its child groups
							if (current_selected_group >= 0 && vEntityGroupList[current_selected_group][0].iGroupID != -1)
							{
								// this group is a parent, so see if there are any child groups
								int iLookForThisID = vEntityGroupList[current_selected_group][0].iGroupID;
								for (int gi = 0; gi < MAXGROUPSLISTS; gi++)
								{
									if (gi != current_selected_group && vEntityGroupList[gi].size() > 0)
									{
										if (vEntityGroupList[gi][0].iParentGroupID == iLookForThisID)
										{
											// found a child of this parent, switch roles (so parent can be deleted below)
											for (int n = 0; n < vEntityGroupList[gi].size(); n++)
											{
												// child becomes the parent
												vEntityGroupList[gi][n].iGroupID = iLookForThisID;
												vEntityGroupList[gi][n].iParentGroupID = -1;
											}
											for (int n = 0; n < vEntityGroupList[current_selected_group].size(); n++)
											{
												// parent becomes the child (temporarily so they can be deleted)
												vEntityGroupList[current_selected_group][n].iGroupID = -1;
												vEntityGroupList[current_selected_group][n].iParentGroupID = iLookForThisID;
											}
											// also, ensure the original parent group image survives, so copy just before deletions
											iEntityGroupListImage[gi] = iEntityGroupListImage[current_selected_group];
											// only need one child to become parent
											break;
										}
									}
								}
							}
							// prevent deleting them, and instead instruct user to ungroup the objects first
							if (current_selected_group >= 0 && vEntityGroupList[current_selected_group][0].iGroupID != -1)
							{
								// do not delete 'key' objects that are part of group
								strcpy(cTriggerMessage, "Cannot delete a parent group. First ungroup objects, then you can delete them all.");
								bTriggerMessage = true;
								bContinueWithDelete = false;
							}
							else
							{
								// before delete, ensure child groups are ungrouped before the delete
								if (current_selected_group >= 0 && vEntityGroupList[current_selected_group][0].iParentGroupID != -1)
								{
									// pass flag to ensure rubber band list not cleared, so we can delete below
									UnGroupSelected(true);
								}
								// delete all entities in rubber band highlight list
								gridedit_deleteentityrubberbandfrommap();
								gridedit_clearentityrubberbandlist();
								t.widget.pickedEntityIndex = 0;
							}
						}
					}
				}
				if (bContinueWithDelete == true)
				{
					if (t.widget.pickedObject > 0)
					{
						// delete a single entity selected by widget
						if (t.widget.pickedEntityIndex > 0)
						{
							t.tentitytoselect = t.widget.pickedEntityIndex;
							DeleteEntityFromLists(t.tentitytoselect);
							gridedit_deleteentityfrommap();
						}
						t.widget.pickedObject = 0;
						widget_updatewidgetobject();
					}
					bWaitOnDelRelease = true;
					t.tentitytoselect = 0;
				}
			}
			if (t.inputsys.keyspace == 0) t.inputsys.spacekeynotreleased = 0;
			if (t.inputsys.keyspace == 1 && t.inputsys.rubberbandmode == 0 && t.inputsys.spacekeynotreleased == 0)
			{
				// end selection when press SPACE
				gridedit_clearentityrubberbandlist();
				t.widget.pickedEntityIndex = 0;
				if (t.widget.pickedObject > 0)
				{
					t.widget.pickedObject = 0;
					widget_updatewidgetobject();
				}
				t.tentitytoselect = 0;
			}
		}
	}
	else
	{
		if (t.inputsys.mclick == 0)  t.onetimeentitypickup = 0;
	}
}

void ToggleDPIAwareness(bool bDPIAware)
{
	// determine if on or off
	char pDPINotAware[256];
	if (bDPIAware == false)
		strcpy(pDPINotAware, "1");
	else
		strcpy(pDPINotAware, "0");

	// write into registry
	HKEY hKeyNames = 0;
	LPCSTR pSubKeyName = "Software\\GameGuruMAX";
	DWORD dwDisposition;
	DWORD Status = RegCreateKeyExA(HKEY_CURRENT_USER, pSubKeyName, 0L, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS | KEY_WRITE, NULL, &hKeyNames, &dwDisposition);
	if (Status == ERROR_SUCCESS)
	{
		if (dwDisposition == REG_OPENED_EXISTING_KEY)
		{
			RegCloseKey(hKeyNames);
			Status = RegOpenKeyExA(HKEY_CURRENT_USER, pSubKeyName, 0L, KEY_WRITE, &hKeyNames);
		}
	}
	if (hKeyNames != 0)
	{
		if (Status == ERROR_SUCCESS)
		{
			Status = RegSetValueExA(hKeyNames, "DPINotAware", 0, REG_SZ, (LPBYTE)pDPINotAware, (strlen(pDPINotAware) + 1) * sizeof(char));
		}
		RegCloseKey(hKeyNames);
	}
}

void ControlAdvancedSetting(int& setting, const char* tooltip, bool* bLargePreview)
{
	ImGui::Spacing();
	ImGui::Indent(-10);

	bool bState = setting;
	ImVec2 label_size = ImGui::CalcTextSize("View Advanced Settings...", NULL, true);

	// ZJ: The tooltip doesn't work without PushID?
	ImGui::PushID(10000);
	
	ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2((ImGui::GetContentRegionAvailWidth()*0.5) - (label_size.x*0.5) - (ImGui::GetFrameHeight()*0.5), 0.0f));
	if (ImGui::Checkbox("", &bState))
	{
		setting = bState;
	}
	
	char* action = "Show ";
	if (setting)
		action = "Hide ";
	char fullText[MAX_PATH];
	strcpy(fullText, action);
	strcpy(fullText + strlen(action), tooltip);

	if (ImGui::IsItemHovered()) ImGui::SetTooltip(fullText);
	ImGui::PopID();
	ImGui::SameLine();

	if (ImGui::HyberlinkButton("View Advanced Settings##2", ImVec2(label_size.x, 0)))
	{
		extern int iSetSettingsFocusTab;
		extern bool bPreferences_Window;
		iSetSettingsFocusTab = 2;
		bPreferences_Window = true;

		//This is a modal window, so we need to close it to see the settings window.
		if (bLargePreview)
		{
			*bLargePreview = false;
		}
	}

	ImGui::Spacing();
	ImGui::Indent(10);
}

void TestLevel_ToggleBoundary(bool _2d, bool _3d)
{
	if (t.game.gameisexe == 1)
	{
		_2d = false;
		_3d = false;
	}

	if (_2d) 
		ggterrain_global_render_params2.flags2 |= GGTERRAIN_SHADER_FLAG2_SHOW_MAP_SIZE;
	else 
		ggterrain_global_render_params2.flags2 &= ~GGTERRAIN_SHADER_FLAG2_SHOW_MAP_SIZE;

	if(_3d)
		ggterrain_global_render_params2.flags2 |= GGTERRAIN_SHADER_FLAG2_SHOW_MAP_SIZE_3D;
	else 
		ggterrain_global_render_params2.flags2 &= ~GGTERRAIN_SHADER_FLAG2_SHOW_MAP_SIZE_3D;

}

void TestLevel_ToggleTreeVegWater(bool tree, bool veg, bool water, bool terrain)
{
	if (t.game.gameisexe == 0)
	{
		ggtrees_global_params.draw_enabled = tree;
		gggrass_global_params.draw_enabled = veg;
		t.gamevisuals.bWaterEnable = water;
		t.gamevisuals.bEndableTreeDrawing = tree;
		t.gamevisuals.bEndableGrassDrawing = veg;
		t.gamevisuals.bEndableTerrainDrawing = terrain;
	}
}

void LockSelectedObject(bool bLock, int iObjectLockedIndex)
{
	if (bLock)
	{
		int e = t.widget.pickedEntityIndex;
		t.entityelement[e].editorlock = 1 - t.entityelement[e].editorlock;
		sObject* pObject;
		if (t.entityelement[e].obj > 0) {
			pObject = g_ObjectList[t.entityelement[e].obj];
			if (pObject) {
				if (t.entityelement[e].editorlock)
				{
					sRubberBandType vEntityLockedItem;
					vEntityLockedItem.e = e;
					vEntityLockedList.push_back(vEntityLockedItem);
				}
				else 
				{
					//Delete from list.
					for (int i = 0; i < vEntityLockedList.size(); i++)
					{
						if (vEntityLockedList[i].e == e) 
						{
							vEntityLockedList.erase(vEntityLockedList.begin() + i);
							break;
						}
					}
					WickedCall_SetObjectRenderLayer(pObject, GGRENDERLAYERS_NORMAL);
				}
			}
		}
	}
	else
	{
		int e = t.widget.pickedEntityIndex;
		if (iObjectLockedIndex >= 0) 
		{
			t.entityelement[e].editorlock = 0;
			sObject* pObject;
			if (t.entityelement[e].obj > 0) 
			{
				pObject = g_ObjectList[t.entityelement[e].obj];
				if (pObject) 
				{
					WickedCall_SetObjectRenderLayer(pObject, GGRENDERLAYERS_NORMAL);
				}
			}
			vEntityLockedList.erase(vEntityLockedList.begin() + iObjectLockedIndex);
		}
	}

	// any lock/unlock operations resets, avoids issue of duplcating a static object and unable to 'move' it
	t.widget.pickedObject = 0;
}

void InjectIconToExe(char *icon, char *exe,int intresourcenumber)
{
	if (!icon || !exe) return;

	HANDLE hIcon = CreateFileA(icon,
		GENERIC_READ,
		0,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);
	if (!hIcon)
	{
		return;
	}

	LPBYTE lpBuf;
	DWORD dwFileSize, dwBytesRead;

	dwFileSize = GetFileSize(hIcon, NULL);
	lpBuf = (LPBYTE)malloc(dwFileSize);
	if (!lpBuf)
	{
		CloseHandle(hIcon);
		return;
	}

	ReadFile(hIcon, lpBuf, dwFileSize, &dwBytesRead, NULL);
	if (dwBytesRead != dwFileSize)
	{
		free(lpBuf);
		CloseHandle(hIcon);
		return;
	}

	CloseHandle(hIcon);


	HANDLE hUpdateRes;
	hUpdateRes = BeginUpdateResourceA(exe, FALSE);
	if (hUpdateRes == NULL)
	{
		free(lpBuf);
		return;
	}

	//Max english
	#define IDI_GAMEGURUMAX                 102
	if (!UpdateResource(hUpdateRes,
		RT_ICON,
		MAKEINTRESOURCE(intresourcenumber), //MAKEINTRESOURCE(IDI_GAMEGURUMAX),
		MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
		lpBuf+22, dwBytesRead-22)) //PE: Test offset header 22
	{
		free(lpBuf);
		return;
	}

	#pragma pack(push, 1)
	struct myheader
	{
		WORD Reserved;
		WORD ResourceType;
		WORD ImageCount;

		BYTE Width;
		BYTE Height;
		BYTE Colors;
		BYTE Reserved2;
		WORD Planes;
		WORD BitsPerPixel;
		DWORD ImageSize;
		WORD ImageOffset;
	} myheader;
	#pragma pack(pop)
	myheader.Reserved = 0;
	myheader.ResourceType = 1;
	myheader.ImageCount = 1;
	myheader.Width = 0; //256
	myheader.Height = 0; //256
	myheader.Colors = 0;
	myheader.Reserved2 = 0;
	myheader.Planes = 1;
	myheader.BitsPerPixel = 32; //
	myheader.ImageSize = dwBytesRead - 22; //
	myheader.ImageOffset = 1; //

	BOOL b = UpdateResource(hUpdateRes, MAKEINTRESOURCE(RT_GROUP_ICON), MAKEINTRESOURCE(102), MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), &myheader, sizeof(myheader));

	if (!EndUpdateResource(hUpdateRes, FALSE))
	{
		free(lpBuf);
		return;
	}

	free(lpBuf);
}

// We call this from when hot swapping entities, and when a smart object is being loaded to a level (see below 'entity_loadelementsdata' for handling of this)
void ReloadEntityIDInSitu ( int entIndex)
{
	// this entity to refreshload
	t.entid = entIndex;

	// Make a backup of the old parent entity, so that we can check what settings have been changed in the entityelements
	entityprofiletype entityBackup = t.entityprofile[entIndex];

	// delete parent entity object as we are going to reload it.
	int parentObj = g.entitybankoffset + entIndex;
	t.entobj = g.entitybankoffset + entIndex;
	if (ObjectExist(t.entobj) == 1) DeleteObject(t.entobj);

	//PE: Also make sure we clear the master object textures in VRAM so they can be reloaded.
	void WickedCall_FreeImage_By_MasterID(uint32_t masterid);
	WickedCall_FreeImage_By_MasterID(t.entobj);

	// set entity name and reload it in
	t.entdir_s = "";
	//if (strnicmp(t.entitybank_s[entIndex].Get(), "projectbank", 11) != NULL) 
	t.entdir_s = "entitybank\\";
	t.ent_s = t.entitybank_s[entIndex];
	t.entpath_s = getpath(t.ent_s.Get());

	// need to detect and load 'groups'
	extern int g_iAbortedAsEntityIsGroupFileMode;
	g_iAbortedAsEntityIsGroupFileMode = 1;

	// find parent group ID so stub mode can update group data
	int iUniqueGroupID = 0;
	int iParentGroupIndex = -1;
	if (t.entityprofile[entIndex].model_s == "group" && t.entityprofile[entIndex].groupreference == 1)
	{
		cstr sLookFor = cstr("entitybank\\") + t.entitybank_s[entIndex];
		extern int GetGroupIndexFromName (cstr sLookFor);
		iParentGroupIndex = GetGroupIndexFromName(sLookFor);
		if (iParentGroupIndex >=0 && iParentGroupIndex < MAXGROUPSLISTS)
		{
			if (vEntityGroupList[iParentGroupIndex].size() > 0)
			{
				iUniqueGroupID = vEntityGroupList[iParentGroupIndex][0].iGroupID;
				g_iAbortedAsEntityIsGroupFileModeStubOnly = 2 + iParentGroupIndex;
			}
		}

		// delete the hidden elements of this parent group
		if(iUniqueGroupID>0)
		{ 
			for (int ee = 1; ee <= g.entityelementlist; ee++)
			{
				if (t.entityelement[ee].bankindex > 0)
				{
					if (t.entityelement[ee].y <= -48000.0f) // original smart object elements are buried deep and cloned, they do not count as part of level!
					{
						int thisGroupID = t.entityelement[ee].creationOfGroupID;
						if (thisGroupID > 0 && thisGroupID == iUniqueGroupID)
						{
							t.tentitytoselect = ee;
							entity_deleteentityfrommap();
							ee = 1;
						}
					}
				}
			}
		}
	}

	// now load the modified entity parent in
	g_iAbortedAsEntityIsGroupCreate = 1;
	entity_load();
	g_iAbortedAsEntityIsGroupCreate = 0;

	// ensures no new groups are created, just refreshed
	g_iAbortedAsEntityIsGroupFileModeStubOnly = 0;

	// if entity was a group/smart object
	bool bObjectIsASmartObject = false;
	if (t.entityprofile[entIndex].model_s == "group" && t.entityprofile[entIndex].groupreference == 1)
	{
		// populate with correct group IDs
		if (iParentGroupIndex >= 0 && iParentGroupIndex < MAXGROUPSLISTS)
		{
			for (int n = 0; n < vEntityGroupList[iParentGroupIndex].size(); n++)
			{
				int ee = vEntityGroupList[iParentGroupIndex][n].e;
				t.entityelement[ee].creationOfGroupID = iUniqueGroupID;
				vEntityGroupList[iParentGroupIndex][n].iGroupID = iUniqueGroupID;
			}
		}

		// now find all the children of this parent
		std::vector<sRubberBandType> childrenToRemake;
		childrenToRemake.clear();
		for (int iChildGroupIndex = 0; iChildGroupIndex < MAXGROUPSLISTS; iChildGroupIndex++)
		{
			if (vEntityGroupList[iChildGroupIndex].size() > 0)
			{
				int iThisChildsParentGroupID = vEntityGroupList[iChildGroupIndex][0].iParentGroupID;
				if (iThisChildsParentGroupID > 0 && iThisChildsParentGroupID == iUniqueGroupID)
				{
					// found a child of this changed parent group, collect significant data
					// get leading element of this child group to orient new replacement
					int iThisE = vEntityGroupList[iChildGroupIndex][0].e;
					sRubberBandType item;
					item.iGroupID = iChildGroupIndex;// WARN - just nicking this field for now, replaced with -1 below (we use it to remember the origial group list index)
					// clean quat data up
					if (t.entityelement[iThisE].quatmode > 1)
					{
						// can happen from prev corruption
						t.entityelement[iThisE].quatmode = 1;
					}
					if (t.entityelement[iThisE].quatmode == 1)
					{
						// normalize good quat in case of drift
						GGQUATERNION quatRot;
						quatRot.x = t.entityelement[iThisE].quatx;
						quatRot.y = t.entityelement[iThisE].quaty;
						quatRot.z = t.entityelement[iThisE].quatz;
						quatRot.w = t.entityelement[iThisE].quatw;
						GGQuaternionNormalize(&quatRot);
						t.entityelement[iThisE].quatx = quatRot.x;
						t.entityelement[iThisE].quaty = quatRot.y;
						t.entityelement[iThisE].quatz = quatRot.z;
						t.entityelement[iThisE].quatw = quatRot.w;
					}
					else
					{
						// create fresh quat
						entity_updatequatfromeuler(iThisE);
					}
					item.x = t.entityelement[iThisE].x;
					item.y = t.entityelement[iThisE].y;
					item.z = t.entityelement[iThisE].z;
					item.quatmode = t.entityelement[iThisE].quatmode;
					item.quatx = t.entityelement[iThisE].quatx;
					item.quaty = t.entityelement[iThisE].quaty;
					item.quatz = t.entityelement[iThisE].quatz;
					item.quatw = t.entityelement[iThisE].quatw;
					item.iParentGroupID = iUniqueGroupID;
					childrenToRemake.push_back(item);

					// record this groups logic connections safely before erase child elements
					DuplicateLogicConnectionsCopyOriginal (vEntityGroupList[iChildGroupIndex], iChildGroupIndex);

					// delete all the elements of this child group
					for (int eee = 1; eee <= g.entityelementlist; eee++)
					{
						if (t.entityelement[eee].bankindex > 0)
						{
							if (t.entityelement[eee].y > -48000.0f)
							{
								int thisGroupID = t.entityelement[eee].creationOfGroupID;
								if (thisGroupID > 0 && thisGroupID == iUniqueGroupID)
								{
									t.tentitytoselect = eee;
									g_UndoSysObjectIsBeingMoved = true; // preserve linkIDs to external entity elements!
									entity_deleteentityfrommap();
									g_UndoSysObjectIsBeingMoved = false;
									eee = 1;
								}
							}
						}
					}
				}
			}
		}

		// and replace them in the same places but with their new shape and elements
		if (childrenToRemake.size() > 0)
		{
			for (int i = 0; i < childrenToRemake.size(); i++)
			{
				// for each new child to create, make a duplicate of the parent
				bool bAttachToCursor = false;
				int iOriginalGroupIndexForChild = childrenToRemake[i].iGroupID;
				int iAnchorEntityIndex = DuplicateFromListToCursor(vEntityGroupList[iParentGroupIndex], false, iOriginalGroupIndexForChild, bAttachToCursor);

				// place entity elements into the level using same places as old ones
				if(vEntityGroupList[iOriginalGroupIndexForChild].size()>0)
				{ 
					// base vars used to find difference between child and original from grouplist
					float fBasePosX, fBasePosY, fBasePosZ;
					GGQUATERNION qBaseAngle;

					// remembered position
					fBasePosX = childrenToRemake[i].x - vEntityGroupList[iOriginalGroupIndexForChild][0].x;
					fBasePosY = childrenToRemake[i].y - vEntityGroupList[iOriginalGroupIndexForChild][0].y;
					fBasePosZ = childrenToRemake[i].z - vEntityGroupList[iOriginalGroupIndexForChild][0].z;

					// work out original quat angle
					if (vEntityGroupList[iOriginalGroupIndexForChild][0].quatmode == 0)
					{
						// one time fix if quat not calculated for this vEntityGroupList entry
						GGQUATERNION QuatAroundX, QuatAroundY, QuatAroundZ;
						GGQuaternionRotationAxis(&QuatAroundX, &GGVECTOR3(1, 0, 0), GGToRadian(vEntityGroupList[iOriginalGroupIndexForChild][0].rx));
						GGQuaternionRotationAxis(&QuatAroundY, &GGVECTOR3(0, 1, 0), GGToRadian(vEntityGroupList[iOriginalGroupIndexForChild][0].ry));
						GGQuaternionRotationAxis(&QuatAroundZ, &GGVECTOR3(0, 0, 1), GGToRadian(vEntityGroupList[iOriginalGroupIndexForChild][0].rz));
						GGQUATERNION quatNewOrientation = QuatAroundX * QuatAroundY * QuatAroundZ;
						vEntityGroupList[iOriginalGroupIndexForChild][0].quatmode = 1;
						vEntityGroupList[iOriginalGroupIndexForChild][0].quatx = quatNewOrientation.x;
						vEntityGroupList[iOriginalGroupIndexForChild][0].quaty = quatNewOrientation.y;
						vEntityGroupList[iOriginalGroupIndexForChild][0].quatz = quatNewOrientation.z;
						vEntityGroupList[iOriginalGroupIndexForChild][0].quatw = quatNewOrientation.w;
					}
					GGQUATERNION qInvOrigAngle;
					GGQUATERNION qOrigAngle = GGQUATERNION(vEntityGroupList[iOriginalGroupIndexForChild][0].quatx, vEntityGroupList[iOriginalGroupIndexForChild][0].quaty, vEntityGroupList[iOriginalGroupIndexForChild][0].quatz, vEntityGroupList[iOriginalGroupIndexForChild][0].quatw);
					GGQuaternionConjugate(&qInvOrigAngle, &qOrigAngle);

					// remembered angle (as above with pos, subtract original)
					int quatmode = childrenToRemake[i].quatmode;
					float quatx = childrenToRemake[i].quatx;
					float quaty = childrenToRemake[i].quaty;
					float quatz = childrenToRemake[i].quatz;
					float quatw = childrenToRemake[i].quatw;
					qBaseAngle = GGQUATERNION(quatx, quaty, quatz, quatw);
					GGQuaternionMultiply(&qBaseAngle, &qInvOrigAngle, &qBaseAngle);

					// go through all entity elements to set them in correct position and assign quats
					for (int n = 0; n < vEntityGroupList[iOriginalGroupIndexForChild].size(); n++)
					{
						// set start in correct position/rotation
						int ee = vEntityGroupList[iOriginalGroupIndexForChild][n].e;
						t.entityelement[ee].x = fBasePosX + vEntityGroupList[iOriginalGroupIndexForChild][n].x;
						t.entityelement[ee].y = fBasePosY + vEntityGroupList[iOriginalGroupIndexForChild][n].y;
						t.entityelement[ee].z = fBasePosZ + vEntityGroupList[iOriginalGroupIndexForChild][n].z;
						t.entityelement[ee].quatmode = vEntityGroupList[iOriginalGroupIndexForChild][n].quatmode;
						t.entityelement[ee].quatx = vEntityGroupList[iOriginalGroupIndexForChild][n].quatx;
						t.entityelement[ee].quaty = vEntityGroupList[iOriginalGroupIndexForChild][n].quaty;
						t.entityelement[ee].quatz = vEntityGroupList[iOriginalGroupIndexForChild][n].quatz;
						t.entityelement[ee].quatw = vEntityGroupList[iOriginalGroupIndexForChild][n].quatw;

						// and update to correct unique group ID
						t.entityelement[ee].creationOfGroupID = iUniqueGroupID;

						// ensure the object is updated, needed below
						PositionObject(t.entityelement[ee].obj, t.entityelement[ee].x, t.entityelement[ee].y, t.entityelement[ee].z);
					}

					// now rotate all the elements around the first object
					int iFirstE = vEntityGroupList[iOriginalGroupIndexForChild][0].e;
					int iActiveObj = t.entityelement[iFirstE].obj;
					if (ObjectExist(iActiveObj) == 1)
					{
						// rotation event
						GGQUATERNION quatRotationEvent = qBaseAngle;
						RotateObjectQuat(iActiveObj, quatx, quaty, quatz, quatw);
						SetStartPositionsForRubberBand(iActiveObj);
						RotateAndMoveRubberBand(iActiveObj, 0, 0, 0, quatRotationEvent);

						// ensure the root object (active object) also gets its rotation!
						t.entityelement[iFirstE].rx = ObjectAngleX(iActiveObj);
						t.entityelement[iFirstE].ry = ObjectAngleY(iActiveObj);
						t.entityelement[iFirstE].rz = ObjectAngleZ(iActiveObj);

						// update entity quat as the preferred source rotation
						entity_updatequatfromeuler(iFirstE);
					}
				}

				// and we have finished with this temp value
				childrenToRemake[i].iGroupID = -1;
			}
			childrenToRemake.clear();
		}
	}
	else
	{
		// The parent non-smart entity has now been reloaded, so we are free to also reload any entities that use its model
		for (int e = 1; e < t.entityelement.size(); e++)
		{
			if (t.entityelement[e].bankindex == entIndex)
			{
				// The model for this entity uses the model that we just reloaded, so we also need to reload this entities obj
				// we deliberately NOT change/reset the properties of this entity element as they are user-defined values
				// we only swap in the new model and textures as this is often what an artist is tweaking!
				t.e = e;
				t.tte = e;
				t.tupdatee = e;
				t.tentid = entIndex;
				t.tobj = t.entityelement[e].obj;
				entity_updateentityobj();
			}
		}
	}

	//PE: bCustomWickedMaterialActive = false only if dbo changes.
	// as a final step, ensure all custom material settings are removed as the replaced object may not line up with old one
	for (int e = 1; e < t.entityelement.size(); e++)
	{
		if (t.entityelement[e].bankindex == entIndex)
		{
			if (t.entityelement[e].eleprof.bUseFPESettings)
			{
				int iMasterID = t.entityelement[e].bankindex;
				if (iMasterID > 0 && iMasterID < t.entityprofile.size())
				{
					sObject* pMasterObject = g_ObjectList[g.entitybankoffset + iMasterID];
					if (pMasterObject)
					{
						Wicked_Copy_Material_To_Grideleprof((void*)pMasterObject, 0, &t.entityelement[e].eleprof);
						int tobj = t.entityelement[e].obj;
						if (tobj > 0)
						{
							if (t.entityprofile[iMasterID].WEMaterial.dwBaseColor[0] == -1)
								SetObjectDiffuse(tobj, Rgb(255, 255, 255));
							sObject* pObject = g_ObjectList[tobj];
							Wicked_Set_Material_From_grideleprof((void*)pObject, 0, &t.entityelement[e].eleprof);
							t.entityelement[e].eleprof.WEMaterial.MaterialActive = false;
						}
					}
				}
			}
			//PE: Moved to bUseFPESettings.
			//t.entityelement[e].eleprof.bCustomWickedMaterialActive = false;
		}
	}
}

// Check if any files have been modified since we launched Max (note: this only checks files in MainEntityList) - any other files will not be detected.
void CheckExistingFilesModified(bool bResetTimeStamp)
{
	// Users can turn this feature off if it causes slowdowns.
	if (pref.iCheckFilesModifiedOnFocus == 0 || t.game.gameisexe == 1 || bImGuiInTestGame == true )
		return;

	// lists to monitor level media changes (cannot use folderfiles structure as DBOs are not listed)
	std::vector<int> currentLevelObjectID;
	std::vector<std::string> currentLevelFiles;
	std::vector<std::string> currentLevelFilesAbsolute;

	// static map for retaining timestamps of last collection
	static std::map<std::string, time_t> currentLevelTimeStamp;
	if (bResetTimeStamp == true) currentLevelTimeStamp.clear();

	// scan for all media to find FPEs used in current level
	extern cFolderItem MainEntityList;
	cFolderItem* pFolder = &MainEntityList;
	if (pFolder)
	{
		while (pFolder)
		{
			if (pFolder->m_pFirstFile)
			{
				char folderPath[MAX_PATH];
				strcpy(folderPath, pFolder->m_sFolderFullPath.Get());
				char folderRelPath[MAX_PATH];
				//PE: Crash here if folder only listing. 
				if(pFolder->m_iEntityOffset+1 < pFolder->m_sFolderFullPath.Len())
					strcpy(folderRelPath, pFolder->m_sFolderFullPath.Get() + pFolder->m_iEntityOffset + 1);
				else
					strcpy(folderRelPath, pFolder->m_sFolderFullPath.Get());
				cFolderItem::sFolderFiles* pFolderFile = pFolder->m_pFirstFile;
				pFolderFile = pFolder->m_pFirstFile->m_pNext;
				while (pFolderFile)
				{
					if (strcmp(pFolderFile->m_sName.Get() + strlen(pFolderFile->m_sName.Get()) - 3, "fpe") == 0)
					{
						// construct matching path and entity name
						char pSearchPattern[MAX_PATH];
						strcpy(pSearchPattern, folderRelPath);
						strcat(pSearchPattern, "\\");
						strcat(pSearchPattern, pFolderFile->m_sName.Get());

						// found an FPE, determine if in current level
						for (int entid = 0; entid < t.entitybank_s.size(); entid++)
						{
							if (stricmp(pSearchPattern, t.entitybank_s[entid].Get()) == NULL)
							{
								// add FPE for check
								char pFullFileAbsPath[MAX_PATH];
								strcpy(pFullFileAbsPath, folderPath);
								strcat(pFullFileAbsPath, "\\");
								strcat(pFullFileAbsPath, pFolderFile->m_sName.Get());
								if (FileExist(pFullFileAbsPath))
								{
									currentLevelFilesAbsolute.push_back(pFullFileAbsPath);
									char pFileToAdd[MAX_PATH];
									strcpy(pFileToAdd, folderRelPath);
									strcat(pFileToAdd, "\\");
									strcat(pFileToAdd, pFolderFile->m_sName.Get());
									currentLevelObjectID.push_back(entid); currentLevelFiles.push_back(pFileToAdd);

									// add model DBO inside FPE
									if (strlen(t.entityprofile[entid].model_s.Get()) > 0)
									{
										strcpy(pFileToAdd, folderRelPath);
										strcat(pFileToAdd, "\\");
										strcat(pFileToAdd, t.entityprofile[entid].model_s.Get());
										currentLevelObjectID.push_back(entid); currentLevelFiles.push_back(pFileToAdd);
										strcpy(pFullFileAbsPath, folderPath);
										strcat(pFullFileAbsPath, "\\");
										strcat(pFullFileAbsPath, t.entityprofile[entid].model_s.Get());
										currentLevelFilesAbsolute.push_back(pFullFileAbsPath);
									}

									// add any textures used by FPE
									if (strlen(t.entityprofile[entid].texd_s.Get()) > 0)
									{
										strcpy(pFileToAdd, folderRelPath);
										strcat(pFileToAdd, "\\");
										strcat(pFileToAdd, t.entityprofile[entid].texd_s.Get());
										currentLevelObjectID.push_back(entid); currentLevelFiles.push_back(pFileToAdd);
										strcpy(pFullFileAbsPath, folderPath);
										strcat(pFullFileAbsPath, "\\");
										strcat(pFullFileAbsPath, t.entityprofile[entid].texd_s.Get());
										currentLevelFilesAbsolute.push_back(pFullFileAbsPath);
									}

									// and include all textures referenced with new PBR texture sets
									for (int iAllObjectTexturesIndex = 0; iAllObjectTexturesIndex <= 5; iAllObjectTexturesIndex++)
									{
										for (int i = 0; i < MAXMESHMATERIALS; i++)
										{
											// work out pImgFileRef
											LPSTR pImgFileRef = "";
											if (iAllObjectTexturesIndex == 0) pImgFileRef = t.entityprofile[entid].WEMaterial.baseColorMapName[i].Get();
											if (iAllObjectTexturesIndex == 1) pImgFileRef = t.entityprofile[entid].WEMaterial.normalMapName[i].Get();
											if (iAllObjectTexturesIndex == 2) pImgFileRef = t.entityprofile[entid].WEMaterial.surfaceMapName[i].Get();
											if (iAllObjectTexturesIndex == 3) pImgFileRef = t.entityprofile[entid].WEMaterial.emissiveMapName[i].Get();
											if (iAllObjectTexturesIndex == 4) pImgFileRef = t.entityprofile[entid].WEMaterial.displacementMapName[i].Get();
											#ifndef DISABLEOCCLUSIONMAP
											if (iAllObjectTexturesIndex == 5) pImgFileRef = t.entityprofile[entid].WEMaterial.occlusionMapName[i].Get();
											#endif
											if (pImgFileRef && strlen(pImgFileRef) > 0)
											{
												// add DDS texture file
												strcpy(pFileToAdd, folderRelPath);
												strcat(pFileToAdd, "\\");
												strcat(pFileToAdd, pImgFileRef);
												currentLevelObjectID.push_back(entid); currentLevelFiles.push_back(pFileToAdd);
												strcpy(pFullFileAbsPath, folderPath);
												strcat(pFullFileAbsPath, "\\");
												strcat(pFullFileAbsPath, pImgFileRef);
												currentLevelFilesAbsolute.push_back(pFullFileAbsPath);
											}
										}
									}
								}
							}
						}
					}
					pFolderFile = pFolderFile->m_pNext;
				}
			}
			pFolder = pFolder->m_pNext;
		}
	}

	// special static map can track timestamp changes
	if (currentLevelTimeStamp.size() == 0)
	{
		for (int iIndex = 0; iIndex < currentLevelFilesAbsolute.size(); iIndex++)
		{
			LPSTR filePath = (LPSTR)currentLevelFilesAbsolute[iIndex].c_str();
			bool bFoundInMap = false;
			std::map<std::string, time_t>::iterator it = currentLevelTimeStamp.begin();
			while (it != currentLevelTimeStamp.end())
			{
				if (stricmp(filePath, it->first.c_str()) == NULL)
				{
					bFoundInMap = true;
					break;
				}
				it++;
			}
			if (bFoundInMap == false)
			{
				struct stat sb;
				stat(filePath, &sb);
				currentLevelTimeStamp.insert(std::make_pair(filePath, sb.st_mtime));
			}
		}
	}

	// scan all current level files, detect any changes
	std::vector<int> modifiedEntityObject;
	for (int iIndex = 0; iIndex < currentLevelFilesAbsolute.size(); iIndex++)
	{
		// Get the time that the file was last modified.
		struct stat sb;
		LPSTR pFileToCheck = (LPSTR)currentLevelFilesAbsolute[iIndex].c_str();
		if (stat(pFileToCheck, &sb) == 0)
		{
			bool bFoundInMap = false;
			std::map<std::string, time_t>::iterator it = currentLevelTimeStamp.find(pFileToCheck);
			if (it != currentLevelTimeStamp.end())
			{
				if (stricmp(pFileToCheck, it->first.c_str()) == NULL)
				{
					bFoundInMap = true;
					if (sb.st_mtime != it->second)
					{
						// This file has been modified. Add to list of modified files (we will update any entities using them later)
						it->second = sb.st_mtime;
						modifiedEntityObject.push_back(currentLevelObjectID[iIndex]);

						// can take extra actions for specific media to be updated
						if (stricmp(pFileToCheck + strlen(pFileToCheck) - 3, "dds") == NULL)
						{
							// textures will not update, they will reference previously loaded, so delete the image from that list
							char pImageFile[MAX_PATH];
							strcpy(pImageFile, "");
							LPSTR pFile = (LPSTR)currentLevelFiles[iIndex].c_str();
							//if (strnicmp(pFile, "projectbank", 11) != NULL) 
							strcat(pImageFile, "entitybank\\");
							strcat(pImageFile, pFile);
							WickedCall_DeleteImage(pImageFile);
						}
					}
				}
			}
			if (bFoundInMap == false)
			{
				// new file to add to timestamp map
				currentLevelTimeStamp.insert(std::make_pair(pFileToCheck, sb.st_mtime));
			}
		}
	}

	// Remove all duplicate entIDs and add to new sorted list
	std::vector<int> modifiedEntityObjectReduced;
	for (int i = 0; i < modifiedEntityObject.size(); i++)
	{
		int iUniqueEntityID = modifiedEntityObject[i];
		if (iUniqueEntityID > 0)
		{
			for (int i2 = 0; i2 < modifiedEntityObject.size(); i2++)
			{
				if (modifiedEntityObject[i2] == iUniqueEntityID)
				{
					modifiedEntityObject[i2] = 0;
					i = 0;
				}
			}
			modifiedEntityObjectReduced.push_back(iUniqueEntityID);
		}
	}

	// Update any entities that use the modified files
	for (auto& entIndex : modifiedEntityObjectReduced)
	{
		// update this entity
		ReloadEntityIDInSitu(entIndex);
	}

	// final prompt to inform user of the updated media
	if (modifiedEntityObjectReduced.size() > 0)
	{
		bTriggerMessage = true;
		sprintf(cTriggerMessage, "External media changes detected. %d items of media updated!", modifiedEntityObjectReduced.size());
		modifiedEntityObjectReduced.clear();
	}
}

//#pragma optimize("", off)
int DrawOccludedObjects(bool bDebug,bool bBox, int* iHiddenObjects, int* spot, int* point)
{
	int total = 0;
	if(iHiddenObjects)
		*iHiddenObjects = 0;
	if (point)
		*point = 0;
	if(spot)
		*spot = 0;
	for (t.e = 1; t.e <= g.entityelementlist; t.e++)
	{
		if (t.entityelement[t.e].obj > 0)
		{
			t.obj = t.entityelement[t.e].obj;
			sObject* pObject = g_ObjectList[t.obj];
			if (pObject)
			{
				if (pObject->bVisible)
				{
					for (int i = 0; i < pObject->iFrameCount; i++)
					{
						sFrame* pFrame = pObject->ppFrameList[i];
						if (pFrame && pFrame->pMesh)
						{
							uint64_t rootEntity = pFrame->wickedobjindex;
							ObjectComponent* object = wiScene::GetScene().objects.GetComponent(rootEntity);
							if (object)
							{
								//if (object->IsOccluded() || object->IsCulled()) // REMOVED
							if (false)
								{
									//if(object->IsOccluded()) // REMOVED
									//	total++;
									if (bDebug)
									{

										XMFLOAT3 center = object->center; // aabb.getCenter();
										void DrawDot(char* text, float x, float y, float z);

										if(t.entityelement[t.e].bankindex > 0 && t.entityprofile[t.entityelement[t.e].bankindex].ischaracter)
											DrawDot("*", center.x, center.y, center.z);
										//else if(object->IsCulled()) // REMOVED
										else if(false)
											DrawDot(".", center.x, center.y, center.z);
										else
											DrawDot("-", center.x, center.y, center.z);
									}
								}
								else
								{
									if (bBox)
									{
										// GGMAX DX12 REVIVED: draw the object's world-space AABB straight from the engine's
										// per-frame cull stream (aabb_objects). objects.GetIndex returns INVALID_INDEX (~0ull)
										// if absent, which fails the size() bound. The old entity-lookup/mesh-fallback block
										// below is dead (kept as history — aabb_objects lost per-entity lookup + transform_index).
										size_t oi = wiScene::GetScene().objects.GetIndex(rootEntity);
										if (oi < wiScene::GetScene().aabb_objects.size())
											wiRenderer::DrawBox(wiScene::GetScene().aabb_objects[oi], XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f));
										//AABB* aabb = wiScene::GetScene().aabb_objects.GetComponent(rootEntity); // aabb_objects is now a plain vector
									// Entire block disabled: aabb_objects no longer supports entity lookup, transform_index removed
									/*
										if (aabb)
										{
											float sizeX = aabb->_max.x - aabb->_min.x;
											float sizeY = aabb->_max.y - aabb->_min.y;
											float sizeZ = aabb->_max.z - aabb->_min.z;
											bool bMeshWorked = false;
											XMFLOAT4X4 hoverBox;
											//PE: Strange gamecore\ammo\enhanced\762x39\762x39ammo.x aabb is wrong in wicked ?
											if (sizeX > 20000.0f || sizeY > 20000.0f || sizeZ > 20000.0f)
											{
												//PE: Try mesh.
												if (object->mesh_index != 0 && (int)object->mesh_index != -1)
												{
													if (object->transform_index != 0 && object->transform_index != -1)
													{
														MeshComponent* mesh = &wiScene::GetScene().meshes[object->mesh_index];
														TransformComponent* transform = &wiScene::GetScene().transforms[object->transform_index];
														if (mesh)
														{
															AABB aabb = mesh->aabb;
															aabb._min.x += transform->world._41;
															aabb._min.y += transform->world._42;
															aabb._min.z += transform->world._43;
															aabb._max.x += transform->world._41;
															aabb._max.y += transform->world._42;
															aabb._max.z += transform->world._43;

															XMStoreFloat4x4(&hoverBox, aabb.getAsBoxMatrix());
															wiRenderer::DrawBox(hoverBox, XMFLOAT4(1.0f, 0.7f, 0.0f, 1.0f));
															bMeshWorked = true;
														}
													}
												}
											}
											if (!bMeshWorked)
											{
												XMStoreFloat4x4(&hoverBox, aabb->getAsBoxMatrix());
												wiRenderer::DrawBox(hoverBox, XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f));
											}
										}
									*/
									}
								}
								//break;
							}
						}
					}
				}
				else
				{
					if (iHiddenObjects)
						*iHiddenObjects = *iHiddenObjects + 1;
				}
			}
		}
	}
	if (bDebug)
	{
		int lsize = wiScene::GetScene().lights.GetCount();
		for (int i = 0; i < lsize; i++)
		{
			LightComponent& light = wiScene::GetScene().lights[i];
			if (light.IsCastingShadow())
			{
				if (light.GetType() == LightComponent::POINT)
				{
					if (point)
						*point = *point + 1;
					AABB aabb = wiScene::GetScene().aabb_lights[i];
					XMFLOAT3 center = aabb.getCenter();
					void DrawDot(char* text, float x, float y, float z);
					//if (light.history == 0) // light.history removed
					{
						DrawDot("p", center.x, center.y, center.z);
					}
					/*else
					{
						t.tdiffx_f = center.x - CameraPositionX();
						t.tdiffy_f = center.y - CameraPositionY();
						t.tdiffz_f = center.z - CameraPositionZ();
						float dist = Sqrt(abs(t.tdiffx_f * t.tdiffx_f) + abs(t.tdiffy_f * t.tdiffy_f) + abs(t.tdiffz_f * t.tdiffz_f));
						std::string sdist = "P: " + std::to_string((int)dist);
						DrawDot( (char *) sdist.c_str(), center.x, center.y, center.z);
					}*/
				}
				if (light.GetType() == LightComponent::SPOT)
				{
					if (spot)
						*spot = *spot + 1;
					AABB aabb = wiScene::GetScene().aabb_lights[i];
					XMFLOAT3 center = aabb.getCenter();
					void DrawDot(char* text, float x, float y, float z);
					//if (light.history == 0) // light.history removed
					{
						DrawDot("s", center.x, center.y, center.z);
					}
					/*else
					{
						t.tdiffx_f = center.x - CameraPositionX();
						t.tdiffy_f = center.y - CameraPositionY();
						t.tdiffz_f = center.z - CameraPositionZ();
						float dist = Sqrt(abs(t.tdiffx_f * t.tdiffx_f) + abs(t.tdiffy_f * t.tdiffy_f) + abs(t.tdiffz_f * t.tdiffz_f));
						std::string sdist = "S: " + std::to_string((int)dist);
						DrawDot((char *)sdist.c_str(), center.x, center.y, center.z);
					}*/
				}
			}
		}
	}
	return total;
}
//#pragma optimize("", on)

void DrawDot(char* text, float x, float y, float z)
{
	ImGuiContext& g = *GImGui;
	ImGuiViewport* mainviewport = ImGui::GetMainViewport();
	ImDrawList* dl = ImGui::GetForegroundDrawList(mainviewport);
	if (dl)
	{
		float fontscale = 1.25;
		ImVec2 v2DPos = Convert3DTo2D(x, y, z);
		dl->AddText(g.Font, g.FontSize * fontscale, v2DPos, ImGui::GetColorU32(ImVec4(1.0, 1.0, 0.4, 1.0)), text); // ImGui::GetColorU32(ImGuiCol_Text)
	}

}

void tmpdebugfunc(void)
{
	ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImVec2 window_pos = ImVec2((viewport->Pos.x + viewport->Size.x - 10.0f), (viewport->Pos.y + 10.0f));
	ImDrawList* draw = ImGui::GetForegroundDrawList();
	extern ImFont* customfont;
	if (draw && customfont)
	{
		extern bool bProfilerEnable;
		if (bProfilerEnable == false)
		{
			bProfilerEnable = true;
			wiProfiler::SetEnabled(true);
		}

		wiScene::Scene* pScene = &wiScene::GetScene();
		int iObjects = pScene->objects.GetCount();
		// DX12 fix: was hardcoded 0 - show the real main-camera frustum-culled count (Wicked culls internally).
		int iFrustumCulled = iObjects - WickedCall_GetFrustumVisibleObjects();
		if (iFrustumCulled < 0) iFrustumCulled = 0;
		int dc = 0;
		int iHiddenObjects = 0;
		int spot = 0, point = 0;
		int occ = DrawOccludedObjects(true,false,&iHiddenObjects,&spot,&point);

		//ImGui::Text("DrawCalls: %d", dc);
		char memtmp[255];
		float wide = 200;// 160;
		sprintf(memtmp, "DC: %d OC: %d FC: %d T: %d", dc, occ, iFrustumCulled,iObjects);
		draw->AddText(customfont, 15, ImVec2(window_pos.x - wide, viewport->Pos.y + 23.0), IM_COL32(255, 255, 255, 255), memtmp);

		extern uint32_t iCulledPointShadows;
		int tpoint = WickedCall_GetCubeShadowLights(true);
		sprintf(memtmp, "T: %d PointShadows: %d", tpoint,iCulledPointShadows);
		draw->AddText(customfont, 15, ImVec2(window_pos.x - wide, viewport->Pos.y + 35.0), IM_COL32(255, 255, 255, 255), memtmp);

		extern uint32_t iCulledSpotShadows;
		int iSpot = WickedCall_GetSpotShadowLights(true);
		sprintf(memtmp, "T: %d SpotShadows: %d", iSpot, iCulledSpotShadows);
		draw->AddText(customfont, 15, ImVec2(window_pos.x - wide, viewport->Pos.y + 47.0), IM_COL32(255, 255, 255, 255), memtmp);
	}
}

void storyboard_openproject(float preview_size_x, float fNodeWidth, float fNodeHeight, int mode)
{
	bool bReadyToOpen = false;
	if (bTriggerOpenProject)
	{
		if (iDelayTriggerOpenProject > 0)
		{
			iDelayTriggerOpenProject--;
			if (iDelayTriggerOpenProject == 0)
			{
				strcpy(cNextWindowFocus, "Open Project##Storyboard");
				iSkibFramesBeforeLaunch = 2;
				iLaunchAfterSync = 81; //Delayed window focus.
			}
		}
		else
		{
			bReadyToOpen = true;
		}
	}
	if (bReadyToOpen)
	{
		//Open Project window.
		static char OpenProjectName[256] = "\0";
		static char OpenProjectError[256] = "\0";

		ImGui::OpenPopup("Open Project##Storyboard");
		ImGui::SetNextWindowSize(ImVec2(0, 532), ImGuiCond_Once);
		static int popwinheight = 0;
		if (popwinheight > 800 || iSkibFramesBeforeLaunch > 0)
		{
			ImGui::SetNextWindowSize(ImVec2(0, 532), ImGuiCond_Always);
		}
		ImGui::SetNextWindowPosCenter(ImGuiCond_Always);
		bool bOpenWindow = true;
		//PE: Somehow cant get this window ontop ?
		if (ImGui::BeginPopupModal("Open Project##Storyboard", &bOpenWindow, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
		{
			popwinheight = ImGui::GetWindowSize().y;
			ImGui::Indent(10);
			ImGui::Text("");
			ImGui::SetWindowFontScale(1.4);
			ImGui::TextCenter("Open Game Project");
			ImGui::Separator();

			ImGui::SetWindowFontScale(1.0);
			ImGui::Text("");
			ImGui::Text("Select the project to open and click 'Open Project'");
			ImGui::Text("Or click 'Import  Project' to find a valid Project to import");
			ImGui::SameLine(); ImGui::Text(" ");
			ImGui::Text("");
			if (strlen(OpenProjectError) > 0)
			{
				ImGui::Text(OpenProjectError);
				ImGui::Text("");
			}
			//Ignore _backup files.

			// when in a remote project, need to rebuild the latest writables based project list
			GG_SetWritablesToRoot(true);
			GetProjectList("projectbank\\");
			GG_SetWritablesToRoot(false);

			static std::string current_project_selected = "";
			ImVec2 size = { ImGui::GetContentRegionAvailWidth(),0 };

			float fHeight = ImGui::GetFontSize() * 10.0;

			ImGui::Text("Projects");
			ImGui::SameLine();
			static bool bDisplayBackups = false;
			float fBoxWidth = ImGui::CalcTextSize("Display Backups").x;
			ImGui::SetCursorPosX((ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x) - 10.0 - 30.0 - fBoxWidth);
			ImGui::Checkbox("Display Backups", &bDisplayBackups);
			ImGui::BeginChild("Projects##FileOpenStoryboard", ImVec2(ImGui::GetContentRegionAvail().x - 10.0, fHeight), true, iGenralWindowsFlags);
			bool bTriggerLoad = false;
			if (projectbank_list.size() > 0)
			{
				float fRegAvail = ImGui::GetContentRegionAvailWidth() - 10.0;
				for (int i = 0; i < projectbank_list.size(); i++)
				{
					if (bDisplayBackups || !pestrcasestr((char*)projectbank_list[i].c_str(), "_backup_"))
					{
						bool bSelected = false;
						if (current_project_selected == projectbank_list[i]) bSelected = true;
						if (ImGui::Selectable(projectbank_list[i].c_str(), bSelected))
						{
							current_project_selected = projectbank_list[i];
						}
						if (ImGui::IsItemHovered())
						{
							if (ImGui::IsMouseDoubleClicked(0))
							{
								current_project_selected = projectbank_list[i];
								bTriggerLoad = true;
							}
						}
					}
				}
			}
			else
			{
				ImGui::Text("No Projects Found.");
			}
			ImGui::EndChild();
			ImGui::PushItemWidth(-10);
			ImGui::InputText("##OpenProjectStoryboardText", (char*)current_project_selected.c_str(), 250, ImGuiInputTextFlags_ReadOnly); //ImGuiInputTextFlags_None
			ImGui::PopItemWidth();

			ImGui::Text("");

			ImGui::SetWindowFontScale(1.4);
			//Import  Project
			if (ImGui::StyleButton("Import Project", ImVec2(ImGui::GetContentRegionAvail().x - 10.0f, 0.0f)))
			{
				cStr tOldDir = GetDir();
				char* cFileSelected;
				cstr fulldir = "c:\\dropbox";

				cFileSelected = (char*)noc_file_dialog_open(NOC_FILE_DIALOG_DIR, "All\0*.*\0", fulldir.Get(), NULL);

				SetDir(tOldDir.Get());

				if (cFileSelected && strlen(cFileSelected) > 0) {
					char projectfolder[MAX_PATH];
					std::string projectname;
					std::string projectpath;
					strcpy(projectfolder, cFileSelected);
					projectname = cFileSelected;
					projectpath = cFileSelected;

					bool bValid = false;
					std::size_t slash = projectname.find_last_of("/\\");
					if (slash > 0)
					{
						projectpath = projectname.substr(0,slash + 1);
						projectname = projectname.substr(slash + 1);

						std::string checkproject = projectpath + projectname + "\\Files"; //PE: Must exists.
						if (PathExist((char *) checkproject.c_str()))
						{
							checkproject = checkproject + "\\projectbank\\" + projectname + "\\project.dat"; //PE: Must exists.
							if (FileExist((char *) checkproject.c_str()))
							{
								//PE: Check if already exists.
								bool bFound = false;
								for (int i = 0; i < projectbank_list.size(); i++)
								{
									if (stricmp(projectname.c_str(), projectbank_list[i].c_str()) == NULL)
									{
										bFound = true;
										break;
									}
								}
								if (bFound)
								{
									BoxerInfo("Selected project already exists.", "Information!");
									bValid = true;
								}
								else
								{
									//PE: Add project to docwrite folder.
									bValid = true;
									//remoteproject.txt
									char pRemoteProject[MAX_PATH];
									strcpy(pRemoteProject, "projectbank\\");
									strcat(pRemoteProject, projectname.c_str());
									strcat(pRemoteProject, "\\remoteproject.txt");
									GG_GetRealPath(pRemoteProject, 1);

									OpenToWrite(1, pRemoteProject);
									WriteString(1, (char *) projectpath.c_str());
									CloseFile(1);
									//PE: Add to list for selection.
									projectbank_list.push_back(projectname);

									//PE: Make sure to update project thumbs, after adding a project!
									GetProjectThumbnails();

									BoxerInfo("Project has been imported.", "Information!");
									current_project_selected = projectname;
									bTriggerLoad = true;
								}
							}
						}
					}
					if (!bValid)
					{
						BoxerInfo("Selected folder is not a valid project.", "Information!");
					}

				}

			}
			if (bTriggerLoad || ImGui::StyleButton("Open Project", ImVec2(ImGui::GetContentRegionAvail().x * 0.5 - 20.0f, 0.0f)))
			{

				if(mode == 1)
				{
					//Load and start storyboard.
					TriggerLoadGameProject = current_project_selected.c_str();
					bWelcomeScreen_Window = false;
					bStoryboardWindow = true;
					bTriggerOpenProject = false;

				}
				else
				{
					// and in case this was a remote project, restore to writables regular
					extern void switch_to_regular_projects(void);
					switch_to_regular_projects();

					//Open
					load_storyboard((char*)current_project_selected.c_str());
					iGamePausedNodeID = storyboard_add_missing_nodex(8, preview_size_x, fNodeWidth, fNodeHeight + 20.0, false);
					iLoadGameNodeID = storyboard_add_missing_nodex(3, preview_size_x, fNodeWidth, fNodeHeight + 20.0, false);
					iSaveGameNodeID = storyboard_add_missing_nodex(9, preview_size_x, fNodeWidth, fNodeHeight + 20.0, false);
					iGraphicsNodeID = storyboard_add_missing_nodex(10, preview_size_x, fNodeWidth, fNodeHeight + 20.0, false);
					iSoundsNodeID = storyboard_add_missing_nodex(11, preview_size_x, fNodeWidth, fNodeHeight + 20.0, false);
					iControlNodeID = storyboard_add_missing_nodex(12, preview_size_x, fNodeWidth, fNodeHeight + 20.0, false);
					iLoadingScreenNodeID = storyboard_add_missing_nodex(2, preview_size_x, fNodeWidth, fNodeHeight + 20.0, false);
					iHUDScreenNodeID = storyboard_add_missing_nodex(13, preview_size_x, fNodeWidth, fNodeHeight + 20.0, false);

					bTriggerOpenProject = false;
					bOpenProjectsFromWelcome = false;
				}
			}
			ImGui::SameLine();
			if (ImGui::StyleButton("Cancel", ImVec2(ImGui::GetContentRegionAvail().x - 10.0f, 0.0f)))
			{
				//Cancel.
				bTriggerOpenProject = false;
				if (bOpenProjectsFromWelcome)
				{
					bWelcomeScreen_Window = true;
					bStoryboardWindow = false;
					bOpenProjectsFromWelcome = false;
				}
			}

			ImGui::SetWindowFontScale(1.0);
			ImGui::Text("");

			bImGuiGotFocus = true;
			ImGui::Indent(-10);
			ImGui::EndPopup();

			bBlockNextMouseCheck = true;
		}
	}
}

bool PostProcess_Settings(float fTabColumnWidth, bool bVisualUpdated)
{
	bool bSetSimpleSky = false;
	int wflags = ImGuiTreeNodeFlags_None;

	if (pref.bAutoClosePropertySections && iLastOpenHeader != 8)
		ImGui::SetNextItemOpen(false, ImGuiCond_Always);

	if (ImGui::StyleCollapsingHeader("Post Processing", wflags))
	{
		ImGui::Indent(10);
		iLastOpenHeader = 8;

		// only show option if not disabled VSYNC in SETUP.INI
		if (g.gvsync != 0)
		{
			ImGui::PushItemWidth(-10);
			if (ImGui::Checkbox("VSync##setVSyncEnabled", &t.visuals.bLevelVSyncEnabled))
			{
				t.gamevisuals.bLevelVSyncEnabled = t.visuals.bLevelVSyncEnabled;
				gridedit_setvsync(t.visuals.bLevelVSyncEnabled);
				g.projectmodified = 1;
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Enabling Vertical Sync will prevent screen tearing and cap FPS in game to your monitors refresh rate");
			ImGui::PopItemWidth();
		}

		//Bloom
		ImGui::PushItemWidth(-10);
		if (ImGui::Checkbox("Bloom Enabled##setBloomEnabled", &t.visuals.bBloomEnabled))
		{
			t.gamevisuals.bBloomEnabled = t.visuals.bBloomEnabled;
			if (master_renderer)
				master_renderer->setBloomEnabled(t.visuals.bBloomEnabled);
			g.projectmodified = 1;
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Setting Bloom enabled will cause bright objects or locations to appear to emit more light");

		ImGui::PopItemWidth();
		if (master_renderer && master_renderer->getBloomEnabled())
		{
			ImGui::PushItemWidth(-10);
			if (ImGui::SliderFloat("##WickedsetBloomThreshold", &t.visuals.fsetBloomThreshold, 0.1f, 10.0f, "%.2f", 2.0f))
			{
				t.gamevisuals.fsetBloomThreshold = t.visuals.fsetBloomThreshold;
				if (master_renderer) {
					master_renderer->setBloomThreshold(t.visuals.fsetBloomThreshold);
				}
				g.projectmodified = 1;
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Bloom Threshold is a measure of how bright an object or area must be before the bloom effect is applied");

			if (ImGui::SliderFloat("##WickedsetBloomStrength", &t.visuals.fsetBloomStrength, 0.1f, 3.0f, "%.2f", 1.0f))
			{
				t.gamevisuals.fsetBloomStrength = t.visuals.fsetBloomStrength;
				if (master_renderer) {
					master_renderer->setBloomStrength(t.visuals.fsetBloomStrength);
				}
				g.projectmodified = 1;
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Bloom Strength is a measure of how strongly the bloom is applied to the scene");

			ImGui::PopItemWidth();
		}

		ImGui::PushItemWidth(-10);
		if (ImGui::Checkbox("SSR##setSSREnabled", &t.visuals.bSSREnabled)) {
			t.gamevisuals.bSSREnabled = t.visuals.bSSREnabled;
			if (master_renderer)
				master_renderer->setSSREnabled(t.visuals.bSSREnabled);
			g.projectmodified = 1;
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Screen Space Reflections use data from lights and objects shown on-screen to produce reflections");

		ImGui::PopItemWidth();

		ImGui::PushItemWidth(-10);
		if (ImGui::Checkbox("Reflections##setReflectionsEnabled", &t.visuals.bReflectionsEnabled)) {
			t.gamevisuals.bReflectionsEnabled = t.visuals.bReflectionsEnabled;
			if (master_renderer)
				master_renderer->setReflectionsEnabled(t.visuals.bReflectionsEnabled);
			g.projectmodified = 1;
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Reflections are calculated by taking data from lights in the scene and determining how they react with the sky and terrain");

		ImGui::PopItemWidth();

		ImGui::PushItemWidth(-10);
		if (ImGui::Checkbox("FXAA##setFXAAEnabled", &t.visuals.bFXAAEnabled)) {
			t.gamevisuals.bFXAAEnabled = t.visuals.bFXAAEnabled;
			if (master_renderer)
				master_renderer->setFXAAEnabled(t.visuals.bFXAAEnabled);
			g.projectmodified = 1;
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("FXAA can smooth out edges on-screen that look pixelated, at the cost of a slight blur");
		ImGui::PopItemWidth();

		ImGui::PushItemWidth(-10);
		if (ImGui::Checkbox("Light Shafts##setLightShaftsEnabled", &t.visuals.bLightShafts)) {
			t.gamevisuals.bLightShafts = t.visuals.bLightShafts;
			if (master_renderer)
				master_renderer->setLightShaftsEnabled(t.visuals.bLightShafts);
			g.projectmodified = 1;
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Enables light rays to cast from the sun");
		ImGui::PopItemWidth();

		// LB: aded lens flare
		ImGui::PushItemWidth(-10);
		if (ImGui::Checkbox("Lens Flare##setLensFlareEnabled", &t.visuals.bLensFlare))
		{
			t.gamevisuals.bLensFlare = t.visuals.bLensFlare;
			if (master_renderer)
				master_renderer->setLensFlareEnabled(t.visuals.bLensFlare);
			g.projectmodified = 1;
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Enables lens flare from light cast from the sun");
		ImGui::PopItemWidth();

		ImGui::PushItemWidth(-10);
		if (ImGui::Checkbox("Auto Exposure##setAutoExposureEnabled", &t.visuals.bAutoExposure))
		{
			t.gamevisuals.bAutoExposure = t.visuals.bAutoExposure;
			if (master_renderer)
				master_renderer->setEyeAdaptionEnabled(t.visuals.bAutoExposure);
			g.projectmodified = 1;
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Automatically adjusts the appearance of the light intensity when the brightness of an area changes");
		ImGui::PopItemWidth();

		if (t.visuals.bAutoExposure)
		{
			tab_tab_Column_text("Auto Exp Rate", fTabColumnWidth);
			ImGui::PushItemWidth(-10);
			if (ImGui::SliderFloat("##fAutoExpRate:", &t.visuals.fAutoExposureRate, 0.01, 4.0)) {
				t.gamevisuals.fAutoExposureRate = t.visuals.fAutoExposureRate;
				master_renderer->setEyeAdaptionRate(t.visuals.fAutoExposureRate);
				g.projectmodified = 1;
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Sets how fast the brightness is adjusted");

			tab_tab_Column_text("Auto Exp Level", fTabColumnWidth);
			ImGui::PushItemWidth(-10);
			if (ImGui::SliderFloat("##fAutoExpKey:", &t.visuals.fAutoExposureKey, 0.01, 0.5)) {
				t.gamevisuals.fAutoExposureKey = t.visuals.fAutoExposureKey;
				master_renderer->setEyeAdaptionKey(t.visuals.fAutoExposureKey);
				g.projectmodified = 1;
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Set Auto Exposure Level - lower values are darker, higher values are lighter");
		}

		//PE: Added DOF.
		ImGui::PushItemWidth(-10);
		if (ImGui::Checkbox("Depth Of Field (DOF)##DOF", &t.visuals.bDOF)) {
			t.gamevisuals.bDOF = t.visuals.bDOF;
			if (master_renderer)
			{
				if (t.visuals.bDOF)
				{
					wiScene::Scene& scene = wiScene::GetScene();
					wiScene::CameraComponent& camera = wiScene::GetCamera();
					camera.aperture_size = t.visuals.fDOFApertureSize;
					camera.focal_length = t.visuals.fDOFFocalLength;
					master_renderer->setDepthOfFieldStrength(t.visuals.fDOFStrength);
				}
				master_renderer->setDepthOfFieldEnabled(t.visuals.bDOF);
			}
			g.projectmodified = 1;
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Depth Of Field (DOF): Is the area of acceptable sharpness in front of and behind the subject which the camera lens is focused.");
		ImGui::PopItemWidth();

		if (t.visuals.bDOF)
		{
			tab_tab_Column_text("DOF Strength", fTabColumnWidth);
			ImGui::PushItemWidth(-10);
			if (ImGui::SliderFloat("##DOF Strength", &t.visuals.fDOFStrength, 1.0f, 20.0f))
			{
				t.gamevisuals.fDOFStrength = t.visuals.fDOFStrength;
				if (master_renderer)
					master_renderer->setDepthOfFieldStrength(t.visuals.fDOFStrength);
			}
			ImGui::PopItemWidth();

			tab_tab_Column_text("DOF ApertureSize", fTabColumnWidth);
			ImGui::PushItemWidth(-10);
			if (ImGui::SliderFloat("##DOF fDOFApertureSize", &t.visuals.fDOFApertureSize, 0.0f, 1.0f))
			{
				t.gamevisuals.fDOFApertureSize = t.visuals.fDOFApertureSize;
				if (master_renderer)
				{
					wiScene::Scene& scene = wiScene::GetScene();
					wiScene::CameraComponent& camera = wiScene::GetCamera();
					camera.aperture_size = t.visuals.fDOFApertureSize;
					camera.UpdateCamera();
					camera.SetDirty();
				}
			}
			ImGui::PopItemWidth();

			tab_tab_Column_text("DOF Focal Length", fTabColumnWidth);
			ImGui::PushItemWidth(-10);
			if (ImGui::SliderFloat("##DOF focal_length", &t.visuals.fDOFFocalLength, 0.001f, 800.0f))
			{
				t.gamevisuals.fDOFFocalLength = t.visuals.fDOFFocalLength;
				if (master_renderer)
				{
					wiScene::Scene& scene = wiScene::GetScene();
					wiScene::CameraComponent& camera = wiScene::GetCamera();
					camera.focal_length = t.visuals.fDOFFocalLength;
					camera.UpdateCamera();
					camera.SetDirty();
				}
			}
			ImGui::PopItemWidth();
		}

		tab_tab_Column_text("Gamma", fTabColumnWidth);
		ImGui::PushItemWidth(-10);
		if (ImGui::SliderFloat("##fGamma:", &t.visuals.fGamma, 0.1, 10.0))
		{
			t.gamevisuals.fGamma = t.visuals.fGamma;
			// UI AUDIT 2026-07-28: wiRenderer::SetGamma is gone in the new WickedEngine —
			// approximate with the tonemap brightness offset, neutral at the 2.2 default
			// (also applied on load in Wicked_Update_Visuals). NOTE: a TRUE pow-curve gamma
			// (engine delta 1.54) was attempted and PARKED — the pushed exponent read wrong
			// in the live path, needs a PIX session; see WICKED_ENGINE_CHANGES.md.
			if (master_renderer) master_renderer->setBrightness((t.visuals.fGamma - 2.2f) * 0.15f);
			g.projectmodified = 1;
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Gamma Correction alters how bright colors are perceived");

		tab_tab_Column_text("De Saturate", fTabColumnWidth);
		ImGui::PushItemWidth(-10);
		if (ImGui::SliderFloat("##fDeSaturate:", &t.visuals.fDeSaturate, 0.0, 1.0))
		{
			t.gamevisuals.fDeSaturate = t.visuals.fDeSaturate;
			// UI AUDIT 2026-07-28: wiRenderer::SetDeSaturate is gone — map straight onto the
			// tonemap saturation. DX11 semantics: 1 = full color (default), 0 = grayscale.
			if (master_renderer) master_renderer->setSaturation(t.visuals.fDeSaturate);
			g.projectmodified = 1;
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Color saturation: 1 = full color, 0 = black and white");

		ImGui::PopItemWidth();

		const char* msaa_items_align[] = { "1 (disabled)", "2", "4", "8" };
		int msaa_current_type_selection = 0;
		if (t.visuals.iMSAASampleCount == 1) msaa_current_type_selection = 0;
		else if (t.visuals.iMSAASampleCount == 2) msaa_current_type_selection = 1;
		else if (t.visuals.iMSAASampleCount == 4) msaa_current_type_selection = 2;
		else msaa_current_type_selection = 3;
		tab_tab_Column_text("MSAA", fTabColumnWidth);
		ImGui::PushItemWidth(-10);
		if (ImGui::Combo("##setMSAASampleCount", &msaa_current_type_selection, msaa_items_align, IM_ARRAYSIZE(msaa_items_align)))
		{
			if (msaa_current_type_selection == 0) t.visuals.iMSAASampleCount = 1;
			else if (msaa_current_type_selection == 1) t.visuals.iMSAASampleCount = 2;
			else if (msaa_current_type_selection == 2) t.visuals.iMSAASampleCount = 4;
			else t.visuals.iMSAASampleCount = 8;
			t.gamevisuals.iMSAASampleCount = t.visuals.iMSAASampleCount;

			if (master_renderer)
			{
				master_renderer->setMSAASampleCount(t.visuals.iMSAASampleCount);
				old_iMSAASampleCount = t.visuals.iMSAASampleCount;
			}
			g.projectmodified = 1;
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("MSAA can smooth out edges detected on objects. The higher the number of samples the greater the performance cost");

		ImGui::PopItemWidth();

		// SSAO
		const char* ao_options[] = { "Disabled", "Enabled" };
		tab_tab_Column_text("SSAO", fTabColumnWidth);
		ImGui::PushItemWidth(-10);
		if (ImGui::Combo("##setAmbientOcclusion", &t.visuals.iMSAO, ao_options, IM_ARRAYSIZE(ao_options)))
		{
			t.gamevisuals.iMSAO = master.iAOSetting = t.visuals.iMSAO;
			//master.masterrenderer.setAO( (RenderPath3D::AO) master.iAOSetting );
			if (master.iAOSetting > 0) master.masterrenderer.setAO(RenderPath3D::AO_MSAO);
			else master.masterrenderer.setAO(RenderPath3D::AO_DISABLED);
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Ambient Occlusion makes objects in corners or holes receive less ambient light");
		ImGui::PopItemWidth();

		if (t.visuals.iMSAO > 0)
		{
			tab_tab_Column_text("AO Power", fTabColumnWidth);
			ImGui::PushItemWidth(-10);
			if (ImGui::SliderFloat("##setAmbientOcclusionPower", &t.visuals.fMSAOPower, -5.0f, 5.0f, "%.2f", 2.0f))
			{
				t.gamevisuals.fMSAOPower = master.fAOPower = t.visuals.fMSAOPower;
				master.masterrenderer.setAOPower(master.fAOPower);
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Set Ambient Occlusion power (default=1.0)");
			ImGui::PopItemWidth();
		}

		// end post processing
		ImGui::Indent(-10);
	}
	return(bVisualUpdated);
}

