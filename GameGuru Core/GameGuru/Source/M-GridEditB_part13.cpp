void DisplayFPEGeneral(bool readonly, int entid, entityeleproftype *edit_grideleprof, int elementID)
{
	ImGui::Indent(10);

	if (!edit_grideleprof)
	{
		edit_grideleprof = &t.grideleprof;
	}
	t.group = 0;

	bool bRubberbandActive = false;
	if (g.entityrubberbandlist.size() > 0) bRubberbandActive = true;
	bool bAllGotPhysics = false;

	// Character allegiance.
	if (t.entityprofile[entid].ischaracter)
	{
		ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(0.0f, 6.0f));
		ImGui::Text("Allegiance");
		ImGui::SameLine();
		ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX(), ImGui::GetCursorPosY() - 3));
		ImGui::SetCursorPos(ImVec2(fPropertiesColoumWidth, ImGui::GetCursorPosY()));
		ImGui::PushItemWidth(-10);

		if (g.entityrubberbandlist.size() == 0)
		{
			const char* items_combo[] = { "Enemy", "Ally", "Neutral" };
			ImGui::Combo("##ShooteriCharAlliance", &edit_grideleprof->iCharAlliance, items_combo, IM_ARRAYSIZE(items_combo));
			if (ImGui::IsItemHovered() && iSkibFramesBeforeLaunch == 0) ImGui::SetTooltip("%s", "Set Character Allegiance");
			ImGui::PopItemWidth();
		}
		else
		{
			const char* items_combo[] = { "(change all)", "Enemies", "Allies", "Neutral" };

			int new_selection = t.entityelement[g.entityrubberbandlist[0].e].eleprof.iCharAlliance;
			bool bGotSameSelection = true;
			//Check if all selection are the same.
			for (int i = 0; i < (int)g.entityrubberbandlist.size(); i++)
			{
				int e = g.entityrubberbandlist[i].e;
				if (e > 0 && t.entityprofile[t.entityelement[e].bankindex].ischaracter > 0)
				{
					if (t.entityelement[e].eleprof.iCharAlliance != new_selection)
					{
						bGotSameSelection = false;
						break;
					}
				}
			}
			if (bGotSameSelection)
				new_selection++;
			else
				new_selection = 0;

			if (ImGui::Combo("##ShooteriCharAlliancem", &new_selection, items_combo, IM_ARRAYSIZE(items_combo)))
			{
				if (new_selection > 0)
				{
					new_selection--;
					for (int i = 0; i < (int)g.entityrubberbandlist.size(); i++)
					{
						int e = g.entityrubberbandlist[i].e;
						if (e > 0 && t.entityprofile[t.entityelement[e].bankindex].ischaracter > 0)
						{
							t.entityelement[e].eleprof.iCharAlliance = new_selection;
						}
					}
				}
			}
			if (ImGui::IsItemHovered() && iSkibFramesBeforeLaunch == 0) ImGui::SetTooltip("%s", "Set Character Allegiance");
		}
	}

	if (t.entityprofile[entid].ismarker == 0 || t.entityprofile[entid].islightmarker == 1)
	{
		if (g.gentitytogglingoff == 0)
		{
			int iShape = t.entityprofile[entid].collisionmode;
			if (edit_grideleprof->iOverrideCollisionMode != -1) iShape = edit_grideleprof->iOverrideCollisionMode;
			if (iShape == 9 || iShape == 10 )
			{
				// static only - hulls cannot be made dynamic at this time!
				const char* items[] = { "Static Hull" };
				int item_current = 0;
				ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX(), ImGui::GetCursorPosY() + 3));
				ImGui::Text("Static mode");
				ImGui::SameLine();
				ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX(), ImGui::GetCursorPosY() - 3));
				ImGui::SetCursorPos(ImVec2(fPropertiesColoumWidth, ImGui::GetCursorPosY()));
				ImGui::PushItemWidth(-10);
				if (ImGui::Combo("##combostaticPhysics3", &item_current, items, IM_ARRAYSIZE(items)))
				{
					// nothing to choose
				}
				ImGui::PopItemWidth();
			}
			else
			{
				t.tokay = 1;
				if (ObjectExist(g.entitybankoffset + entid) == 1)
				{
					if (GetNumberOfFrames(g.entitybankoffset + entid) > 0)
					{
						t.tokay = 0;
					}
				}
				if (t.tokay == 1 || bRubberbandActive)
				{
					//Static , physics on , physics off.
					const char* items[] = { "Static", "Physics on", "Physics off" };
					const char* itemsAll[] = { "Change All To" , "Static", "Physics on", "Physics off" };
					const char** Selected = items;
					int iArraySize = 3;

					int item_current = 0;
					if (t.entityelement[elementID].staticflag == 1)
						item_current = 0;
					else if (edit_grideleprof->physics)
						item_current = 1;
					else
						item_current = 2;

					int iIndexCount = 0;
					bool bAllTheSame = true;
					if (bRubberbandActive)
					{
						//Check if all selected have the same settings.
						for (int i = 0; i < (int)g.entityrubberbandlist.size(); i++)
						{
							int e = g.entityrubberbandlist[i].e;
							int item_test = 0;
							if (t.entityelement[e].staticflag == 1)
								item_test = 0;
							else if (t.entityelement[e].eleprof.physics)
								item_test = 1;
							else
								item_test = 2;
							if (item_test != item_current)
							{
								bAllTheSame = false;
								iIndexCount = 1;
								Selected = itemsAll;
								iArraySize = 4;
								item_current = 0;
								break;
							}
						}
					}
					if (bAllTheSame && item_current == 1) bAllGotPhysics = true;

					ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX(), ImGui::GetCursorPosY() + 3));

					ImGui::Text("Static mode");
					ImGui::SameLine();
					ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX(), ImGui::GetCursorPosY() - 3));
					ImGui::SetCursorPos(ImVec2(fPropertiesColoumWidth, ImGui::GetCursorPosY()));
					ImGui::PushItemWidth(-10);
					if (ImGui::Combo("##combostaticPhysics", &item_current, Selected, iArraySize))
					{
						//Change.
						if (iIndexCount == 1 && item_current == 0)
						{
							//Ignore change all.
						}
						else if (item_current == 0 + iIndexCount && t.tokay == 1) //Only non animated can get static.
						{
							t.entityelement[elementID].staticflag = 1;
							edit_grideleprof->physics = 1;
						}
						else if (item_current == 0 + iIndexCount && t.tokay == 0)
						{
							//Ignore Animated and Static.
						}
						else if (item_current == 1 + iIndexCount)
						{
							t.entityelement[elementID].staticflag = 0;
							edit_grideleprof->physics = 1;
						}
						else
						{
							//No physics
							t.entityelement[elementID].staticflag = 0;
							edit_grideleprof->physics = 0;
						}

						if (bRubberbandActive)
						{
							//Check if all selected have the same settings.
							for (int i = 0; i < (int)g.entityrubberbandlist.size(); i++)
							{
								int e = g.entityrubberbandlist[i].e;
								int masterid = t.entityelement[e].bankindex;
								int ok = 1;
								if (ObjectExist(g.entitybankoffset + masterid) == 1)
								{
									if (GetNumberOfFrames(g.entitybankoffset + masterid) > 0)
									{
										ok = 0;
									}
								}
								if (iIndexCount == 1 && item_current == 0)
								{
									//Ignore change all.
								}
								else if (item_current == 0 + iIndexCount && ok == 1) //Only non animated can get static.
								{
									t.entityelement[e].staticflag = 1;
									t.entityelement[e].eleprof.physics = 1;
								}
								else if (item_current == 0 + iIndexCount && ok == 0)
								{
									//Ignore Animated and Static.
								}
								else if (item_current == 1 + iIndexCount)
								{
									t.entityelement[e].staticflag = 0;
									t.entityelement[e].eleprof.physics = 1;
								}
								else
								{
									//No physics
									t.entityelement[e].staticflag = 0;
									t.entityelement[e].eleprof.physics = 0;
								}

							}
						}
					}
					ImGui::PopItemWidth();
				}
				else
				{
					
					// Only display physics for characters in advanced mode.
					bool bDisplayPhysics = true;
					if (t.entityprofile[entid].ischaracter == 1)
						bDisplayPhysics = false;

					if (bDisplayPhysics)
					{
						//Animated Only physics on/off cant be static.
						const char* items[] = { "Physics on", "Physics off" };
						int item_current = 0;
						if (edit_grideleprof->physics)
							item_current = 0;
						else
							item_current = 1;

						ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX(), ImGui::GetCursorPosY() + 3));
						ImGui::Text("Physics");
						ImGui::SameLine();
						ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX(), ImGui::GetCursorPosY() - 3));
						ImGui::SetCursorPos(ImVec2(fPropertiesColoumWidth, ImGui::GetCursorPosY()));
						ImGui::PushItemWidth(-10);
						if (ImGui::Combo("##combostaticPhysics2", &item_current, items, IM_ARRAYSIZE(items)))
						{
							//Change.
							if (item_current == 0)
							{
								t.entityelement[elementID].staticflag = 0;
								edit_grideleprof->physics = 1;
							}
							else
							{
								//No physics
								t.entityelement[elementID].staticflag = 0;
								edit_grideleprof->physics = 0;
							}
						}
						ImGui::PopItemWidth();
					}
				}
			}
		}
	}

	if (t.tflagspawn == 1)
	{
		if (bRubberbandActive)
		{
			bool bAllTheSame = true;
			//Check if all selected have the same settings.
			for (int i = 0; i < (int)g.entityrubberbandlist.size(); i++)
			{
				int e = g.entityrubberbandlist[i].e;
				if (t.entityelement[e].eleprof.spawnatstart != edit_grideleprof->spawnatstart)
				{
					bAllTheSame = false;
					break;
				}
			}
			if (bAllTheSame)
			{
				int spawnatstart = edit_grideleprof->spawnatstart;
				edit_grideleprof->spawnatstart = imgui_setpropertylist2_v2(t.group, t.controlindex, Str(edit_grideleprof->spawnatstart), "Show at start of level (Change All)", t.strarr_s[563].Get(), 0, readonly); //t.strarr_s[562].Get()
				if (spawnatstart != edit_grideleprof->spawnatstart)
				{
					for (int i = 0; i < (int)g.entityrubberbandlist.size(); i++)
					{
						int e = g.entityrubberbandlist[i].e;
						t.entityelement[e].eleprof.spawnatstart = edit_grideleprof->spawnatstart;
					}
				}
			}
		}
		else
		{
			edit_grideleprof->spawnatstart = imgui_setpropertylist2_v2(t.group, t.controlindex, Str(edit_grideleprof->spawnatstart), "Show at start of level", t.strarr_s[563].Get(), 0, readonly); //t.strarr_s[562].Get()
		}
	}

	if (g_bEnableAutoFlattenSystem && t.entityprofile[entid].autoflatten != 0 && !bRubberbandActive)
	{
		bool bOld = edit_grideleprof->bAutoFlatten;
		edit_grideleprof->bAutoFlatten = imgui_setpropertylist2_v2(t.group, t.controlindex, Str(edit_grideleprof->bAutoFlatten), "Auto Flatten", "Flattens the terrain under the object and clears any trees and grass", 0, readonly);
		if (bOld != edit_grideleprof->bAutoFlatten)
		{
			if (elementID > 0)
			{
				if (!edit_grideleprof->bAutoFlatten && edit_grideleprof->iFlattenID != -1)
				{
					//PE: Disabled remove any flatten.
					GGTerrain_RemoveFlatArea(edit_grideleprof->iFlattenID);
					t.entityelement[elementID].eleprof.iFlattenID = edit_grideleprof->iFlattenID = -1;
				}
				if (!edit_grideleprof->bAutoFlatten)
				{
					// Clear Grass and Trees may now apply (entity_clearsvegetation)
					entity_autoFlattenWhenAdded(elementID);
					edit_grideleprof->iFlattenID = t.entityelement[elementID].eleprof.iFlattenID;
				}
				if (edit_grideleprof->bAutoFlatten)
				{
					if (edit_grideleprof->iFlattenID == -1)
						entity_autoFlattenWhenAdded(elementID);
					else
						entity_updateautoflatten(elementID);
					edit_grideleprof->iFlattenID = t.entityelement[elementID].eleprof.iFlattenID;
					g.projectmodified = 1;
				}
			}
		}
	}

	if (edit_grideleprof->physics == 1 && t.entityelement[elementID].staticflag == 0)
	{
		bool bDisplayPhysics = true;
		if (t.entityprofile[entid].ischaracter == 1)
			bDisplayPhysics = false;

		if (bDisplayPhysics)
		{
			//Affected by gravity
			if (bRubberbandActive)
			{
				if (bAllGotPhysics)
				{
					int iAffectedByGravity = edit_grideleprof->iAffectedByGravity;
					edit_grideleprof->iAffectedByGravity = imgui_setpropertylist2_v2(t.group, t.controlindex, Str(edit_grideleprof->iAffectedByGravity), "Affected by gravity (Change All)", "If set this object will be affected by gravity", 0, readonly); //t.strarr_s[562].Get()
					if (edit_grideleprof->iAffectedByGravity != iAffectedByGravity)
					{
						for (int i = 0; i < (int)g.entityrubberbandlist.size(); i++)
						{
							int e = g.entityrubberbandlist[i].e;
							t.entityelement[e].eleprof.iAffectedByGravity = edit_grideleprof->iAffectedByGravity;
						}
					}
				}
			}
			else
				edit_grideleprof->iAffectedByGravity = imgui_setpropertylist2_v2(t.group, t.controlindex, Str(edit_grideleprof->iAffectedByGravity), "Affected by gravity", "If set this object will be affected by gravity", 0, readonly); //t.strarr_s[562].Get()
		}
	}

	// strength of an entity is standard
	if (t.entityprofile[entid].ischaracter == 0)
	{
		ImGui::TextCenter("Strength");
		ImGui::PushItemWidth(-10);
		ImGui::MaxSliderInputInt("##StrengthSimpleInput", &edit_grideleprof->strength, 1, 500, "Sets the strength of the object");
		ImGui::PopItemWidth();
	}
	else
	{
		// Only for characters do we need these.
		ImGui::TextCenter("Health");
		ImGui::PushItemWidth(-10);
		ImGui::MaxSliderInputInt("##HealthSimpleInput", &edit_grideleprof->strength, 1, 500, "Sets the health of the character");
		ImGui::PopItemWidth();

		if (pref.iObjectEnableAdvanced)
		{
			ImGui::TextCenter("Speed");
			ImGui::PushItemWidth(-10);
			ImGui::MaxSliderInputInt("##AnimationSpeedSimpleInput", &edit_grideleprof->iMoveSpeed, 1, 500, "Sets the animation and/or move speed of the character");
			ImGui::PopItemWidth();

			ImGui::TextCenter("Turn Speed");
			ImGui::PushItemWidth(-10);
			ImGui::MaxSliderInputInt("##TurnSpeedSimpleInput", &edit_grideleprof->iTurnSpeed, 1, 500, "Sets the turn speed of the character");
			ImGui::PopItemWidth();
		}
		
		ImGui::TextCenter("View Angle");
		ImGui::PushItemWidth(-10);
		int iValue = edit_grideleprof->coneangle;
		ImGui::MaxSliderInputInt("##ViewAngleSimpleInput", &iValue, 1, 180, "Sets the viewing angle of the character");
		edit_grideleprof->coneangle = iValue;
		ImGui::PopItemWidth();
		ImGui::TextCenter("View Range");
		ImGui::PushItemWidth(-10);
		iValue = edit_grideleprof->conerange;
		ImGui::MaxSliderInputInt("##ViewRangeSimpleInput", &iValue, 100, 2000, "Sets the range at which the character can see");
		edit_grideleprof->conerange = iValue;
		ImGui::PopItemWidth();
	}

	if (!t.entityprofile[entid].ischaracter)
	{
		ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX(), ImGui::GetCursorPosY() + 15));
		ImGui::Text("Collision Shape");
		ImGui::SameLine();
		ImGui::Indent(100);
		ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX(), ImGui::GetCursorPosY() - 3));
		ImGui::PushItemWidth(-10);
		char* pCollisionShapes[10] = { "Box","Polygon","Sphere","Cylinder","Convex Hull","Character Collision","Tree Collision","No Collision","Hull Decomp","Collision Mesh" };
		char pSelectedCollision[64];

		ImGui::PushItemFlag(ImGuiItemFlags_Disabled, false);
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 1.0f);

		int iShape = t.entityprofile[entid].collisionmode;
		if (edit_grideleprof->iOverrideCollisionMode != -1)	iShape = edit_grideleprof->iOverrideCollisionMode;
		switch (iShape)
		{
		case 0:
			strcpy(pSelectedCollision, pCollisionShapes[0]);
			break;
		case 1:
			strcpy(pSelectedCollision, pCollisionShapes[1]);
			break;
		case 2:
			strcpy(pSelectedCollision, pCollisionShapes[2]);
			break;
		case 3:
			strcpy(pSelectedCollision, pCollisionShapes[3]);
			break;
		case 9:
			strcpy(pSelectedCollision, pCollisionShapes[4]);
			break;
		case 21:
			strcpy(pSelectedCollision, pCollisionShapes[5]);
			break;
		case 50:
			strcpy(pSelectedCollision, pCollisionShapes[6]);
			break;
		case 11:
			strcpy(pSelectedCollision, pCollisionShapes[7]);
			break;
		case 10:
			strcpy(pSelectedCollision, pCollisionShapes[8]);
			break;
		case 8:
			strcpy(pSelectedCollision, pCollisionShapes[9]);
			break;
		default:
			break;
		}
		if (ImGui::BeginCombo("##ImporterCollisionShape", &pSelectedCollision[0], ImGuiComboFlags_PopupAlignLeft | ImGuiComboFlags_HeightLarge))
		{
			for (int i = 0; i < 10; i++)
			{
				//PE: Users are relying on this feature so they can, set a polygon collision object to have "behaviour".
				//PE: Used by many where polygon is needed with a "behaviour" , platforms ... explodeable ... isimmobile == 1 ... Is Collectable ...
				//PE: https://github.com/TheGameCreators/GameGuruMAX/commit/a1929f0a832db7b799d53a01955837b15a8d2d5c

				// Don't display certtain collision modes for dynamic objects!
				if (i == 1 && t.entityelement[elementID].staticflag == 0) continue;

				// get collision shape name
				char* pCollisionShapeName = pCollisionShapes[i];

				// assign correct item based on pSelectedCollision
				bool is_selected = false;
				if (strcmp(pSelectedCollision, pCollisionShapeName) == NULL)
				{
					is_selected = true;
				}
				if (ImGui::Selectable(pCollisionShapeName, is_selected))
				{
					strcpy(pSelectedCollision, pCollisionShapeName);
					int iCollisionSelection = -1;
					switch (i)
					{
						case 0: iCollisionSelection = 0; break;
						case 1: iCollisionSelection = 1; break;
						case 2: iCollisionSelection = 2; break;
						case 3: iCollisionSelection = 3; break;
						case 4: iCollisionSelection = 9; break;
						case 5: iCollisionSelection = 21; break;
						case 6: iCollisionSelection = 50; break;
						case 7: iCollisionSelection = 11; break;
						case 8: iCollisionSelection = 10; break;
						case 9: iCollisionSelection = 8; break;
					}
					edit_grideleprof->iOverrideCollisionMode = iCollisionSelection;
					if (iCollisionSelection == 8 || iCollisionSelection == 9 || iCollisionSelection == 10)
					{
						t.entityelement[elementID].staticflag = 1;
						edit_grideleprof->physics = 1;
					}
				}
				if (is_selected) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Set the collision type the object will use for physics");
		ImGui::Indent(-100);
		ImGui::PopItemWidth();
		ImGui::PopItemFlag();
		ImGui::PopStyleVar();
	}
	ImGui::Indent(-10);

	// Is Immobile a useful tick to have in general and especially for freezing character positions in place for specific animations to work
	ImGui::Indent(10);
	edit_grideleprof->isimmobile = imgui_setpropertylist2(t.group, t.controlindex, Str(edit_grideleprof->isimmobile), t.strarr_s[457].Get(), t.strarr_s[247].Get(), 0);
	ImGui::Indent(-10);

	// Character Has Weapon
	// moved to detection within LUA script via 'bShootingWeaponMentioned'
	if (ImGui::IsAnyItemFocused()) bImGuiGotFocus = true;

	// if not static, we may explode it
	if (t.entityelement[elementID].staticflag == 0)
	{
		ImGui::Indent(10);
		edit_grideleprof->explodable = imgui_setpropertylist2_v2(t.group, t.controlindex, Str(edit_grideleprof->explodable), "Explodable", "If set this object will explode when destroyed", 0, readonly);
		if (edit_grideleprof->explodable != 0)
		{
			ImGui::TextCenter("Explosion Damage");
			ImGui::MaxSliderInputInt("##ExplodeDamageSimpleInput", &edit_grideleprof->explodedamage, 0, 500, "Sets the damage dealt when this object explodes");
			ImGui::TextCenter("Explosion Height");
			ImGui::MaxSliderInputInt("##damagephysicsheight", &edit_grideleprof->explodeheight, 0, 100, "Set the optional height at which the explosion occurs above default object center");

			//PE: Custom explosions.
			edit_grideleprof->explodable_decalname = imgui_setpropertylist2c_v2(t.group, t.controlindex, edit_grideleprof->explodable_decalname.Get(), "Custom Explosion", "Use custom effect for explosion.", 31, readonly, false, false, false, 0);

			//PE: TODO - Bind to mesh id , perhaps rotate area emitter to emit away from bullet. Select old particle effects.
			if (edit_grideleprof->explodable_decalname.Len() > 0)
			{
				//PE: Custom explosion sound setup. always use soundset6_s.
				edit_grideleprof->soundset6_s = imgui_setpropertyfile2_v2(t.group, edit_grideleprof->soundset6_s.Get(), "Custom Explosion Sound", t.strarr_s[254].Get(), "audiobank\\", readonly);

			}
		}
		ImGui::Indent(-10);
	}

	// Moved Always Active to general properties
	ImGui::Indent(10);
	bool btmp = edit_grideleprof->phyalways;
	ImGui::Checkbox("Always Active?", &btmp);
	edit_grideleprof->phyalways = btmp;
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("If set the object will always be active, no matter how far away the player might be");
	ImGui::Indent(-10);

	ImGui::Indent(10);
	bool bObjective = false;
	if (edit_grideleprof->isobjective != 0) bObjective = true;
	ImGui::Checkbox("Is Objective?", &bObjective);
	if (bObjective == true)
	{
		if (t.entityprofile[entid].ismarker == 3)
			edit_grideleprof->isobjective = 2;
		else
		{
			edit_grideleprof->isobjective = 1;
		}
		ImGui::SameLine();
		bool bTmp = edit_grideleprof->isobjective_alwaysactive;
		ImGui::Checkbox("Always Visible", &bTmp);
		edit_grideleprof->isobjective_alwaysactive = bTmp;
	}
	else
	{
		edit_grideleprof->isobjective = 0;
	}
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("If set the object will be an objective for the player");
	ImGui::Indent(-10);

	// Is Collectable general properties (if dynamic)
	if (t.entityelement[elementID].staticflag == 0)
	{
		ImGui::Indent(10);
		bool bCollectable = false;
		if (edit_grideleprof->iscollectable != 0) bCollectable = true;
		if (ImGui::Checkbox("Is Collectable?", &bCollectable))
		{
			if (bCollectable == true)
				edit_grideleprof->iscollectable = 1;
			else
				edit_grideleprof->iscollectable = 0;
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("If set the object will be collectable by the player and added to the inventory");
		ImGui::Indent(-10);

		if (edit_grideleprof->iscollectable != 0)
		{
			ImGui::Indent(10);
			bool bCollectableResource = false;
			if (edit_grideleprof->iscollectable == 2) bCollectableResource = true;
			if (ImGui::Checkbox("Is Resource?", &bCollectableResource))
			{
				if (bCollectableResource == true)
					edit_grideleprof->iscollectable = 2;
				else
					edit_grideleprof->iscollectable = 1;
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("If set the collectable is a resource and can be merged with similar objects");
			ImGui::Indent(-10);
		}


		if(elementID > 0 && entid > 0 && entid < t.entityprofile.size() && elementID < t.entityelement.size() && t.entityprofile[entid].isweapon_s.Len() <= 0 && strlen(pref.cLastUsedStoryboardProject) > 0)
		{
			int iEntityIndex = elementID;
			int iMasterID = entid;
			int iCollectionItemIndex = -1;
			bool bHaveProjectGlobalObject = false;
			for (int n = 0; n < g_collectionList.size(); n++)
			{
				if (g_collectionList[n].collectionFields.size() > 0)
				{
					//t.entityelement[e].eleprof.name_s.Get()
					if (g_collectionList[n].collectionFields[0] == t.entityelement[elementID].eleprof.name_s)
					{
						iCollectionItemIndex = n;
						bHaveProjectGlobalObject = true;
						break;
					}
				}
			}

			bool g_bChangedGameCollectionList = false;
			ImGui::Indent(10);
			bool bProjectWide = edit_grideleprof->isProjectGlobal;
			if (ImGui::Checkbox("Is Project Global?", &bProjectWide))
			{
				if (bProjectWide == true)
				{
					edit_grideleprof->isProjectGlobal = 1;
					{
						// create an item entry
						collectionItemType item;
						fill_rpg_item_defaults(&item, iMasterID, iEntityIndex);

						//PE: Check if already added.
						bool bNewItemIsUnqiue = true;
						for (int n = 0; n < g_collectionList.size(); n++)
						{
							if (item.collectionFields.size() > 0)
							{
								if (g_collectionList[n].collectionFields.size() > 0)
								{
									//t.entityelement[e].eleprof.name_s.Get()
									if (g_collectionList[n].collectionFields[0] == item.collectionFields[0])
									{
										bNewItemIsUnqiue = false;
										break;
									}
								}
							}
						}
						if (bNewItemIsUnqiue == true)
						{
							g_collectionList.push_back(item);
							g_bChangedGameCollectionList = true;
						}

					}
				}
				else
				{
					edit_grideleprof->isProjectGlobal = 0;
				}
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("If set this object will be available on all levels, but must be controlled by Lua");

			if (bHaveProjectGlobalObject && iCollectionItemIndex >= 0)
			{
				//PE: Delete Project Global Object
				float but_gadget_size = ImGui::GetFontSize() * 12.0;
				float w = ImGui::GetWindowContentRegionWidth() - 10.0;
				ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2((w * 0.5) - (but_gadget_size * 0.5), 0.0f));
				LPSTR pCreateButtonLabel = "Delete Project Global Object";
				if (ImGui::StyleButton(pCreateButtonLabel, ImVec2(but_gadget_size, 0)))
				{
					std::vector<collectionItemType> newCollectionList;
					for (int ci = 0; ci < g_collectionList.size(); ci++)
					{
						if (ci != iCollectionItemIndex)
						{
							newCollectionList.push_back(g_collectionList[ci]);
						}
					}
					g_collectionList = newCollectionList;
					g_bChangedGameCollectionList = true;
				}
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("Delete this project global object item from the main collection list of the game project");
			}
			// save any changes to game collection list 
			if (g_bChangedGameCollectionList == true)
			{
				// go through all object parents to ensure
				refresh_rpg_parents_of_items();

				// save collection item list out
				save_rpg_system(pref.cLastUsedStoryboardProject, true);
				g_bChangedGameCollectionList = false;
			}

			DisplayAllowBulletHolesCheckbox(elementID);
			DisplayClearVegetationCheckbox(elementID);

			ImGui::Indent(-10);
		}
	}
	else
	{
		ImGui::Indent(10);
		ImGui::Text("NOTE: Static objects are always immobile");
		ImGui::Text("and cannot be collected by the player");
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("When the object is set to static, it cannot be set as a collectable or move in any way");
		DisplayAllowBulletHolesCheckbox(elementID);
		DisplayClearVegetationCheckbox(elementID);
		ImGui::Indent(-10);
	}
}

void DisplayFPEAdvanced(bool readonly, int entid, entityeleproftype *edit_grideleprof, int elementID)
{
	int tflagtext = 0, tflagimage = 0; //PE: These is not used in VRTECH ?
	if (t.entityprofile[entid].ismarker == 3)
	{
		if (!t.entityprofile[entid].markerindex <= 1)
		{
		}
	}

	if (!edit_grideleprof)
	{
		edit_grideleprof = &t.grideleprof;
	}

	int adv_flasgs = ImGuiTreeNodeFlags_DefaultOpen;
	if (g.vrqcontrolmode > 0)
	{
		//Simple version.
		adv_flasgs = ImGuiTreeNodeFlags_None;
	}

	bool bAdvencedOpen = true;
	if (bAdvencedOpen)
	{
		ImGui::Indent(10);

		t.group = 0;

		static int current_loaded_script = -1;
		static int current_selected_script = 0;
		static bool current_loaded_script_has_dlua = false;
		int speech_entries = 0;

		t.group = 0;
		if (t.tflagchar == 0 && t.tflagvis == 1 && t.tflagsimpler == 0)
		{
			ImGui::TextCenter(t.strarr_s[412].Get());
			{
				// 101016 - Additional General Parameters
				edit_grideleprof->isocluder = imgui_setpropertylist2_v2(t.group, t.controlindex, Str(edit_grideleprof->isocluder), "Occluder", "Set to YES makes this object an occluder", 0, readonly);
				edit_grideleprof->isocludee = imgui_setpropertylist2_v2(t.group, t.controlindex, Str(edit_grideleprof->isocludee), "Occludee", "Set to YES makes this object an occludee", 0, readonly);
			}
		}

		t.group = 1;
		ImGui::TextCenter(t.strarr_s[415].Get());
		{
			//  Basic AI
			if (t.tflagai == 1)
			{
				// can redirect to better folders if in g.quickparentalcontrolmode
				LPSTR pAIRoot = "scriptbank\\";
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
				cstr tmpvalue;
				tmpvalue = imgui_setpropertyfile2_v2(t.group, edit_grideleprof->aimain_s.Get(), t.strarr_s[417].Get(), t.strarr_s[207].Get(), pAIRoot,readonly);
				if (edit_grideleprof->aimain_s != tmpvalue)
				{
					edit_grideleprof->aimain_s = tmpvalue;
					current_loaded_script = -1;
				}
			}
			
			// Has Weapon
			if (t.tflaghasweapon == 1 && t.playercontrol.thirdperson.enabled == 0 && g.quickparentalcontrolmode != 2 )//&& edit_grideleprof->lives == 0)
			{
				edit_grideleprof->hasweapon_s = imgui_setpropertylist2c_v2(t.group, t.controlindex, edit_grideleprof->hasweapon_s.Get(), t.strarr_s[419].Get(), t.strarr_s[209].Get(), 1,readonly, true, true, true, 0);
			}

			//  Is Weapon (FPGC - 280809 - filtered fpgcgenre=1 is shooter genre)
			if (t.tflagweap == 1 && g.fpgcgenre == 1)
			{
				edit_grideleprof->damage = atol(imgui_setpropertystring2_v2(t.group, Str(edit_grideleprof->damage), t.strarr_s[420].Get(), t.strarr_s[210].Get(), readonly));
				edit_grideleprof->accuracy = atol(imgui_setpropertystring2_v2(t.group, Str(edit_grideleprof->accuracy), t.strarr_s[421].Get(), "Increases the inaccuracy of conical distribution by 1/100th of t.a degree", readonly));
				if (edit_grideleprof->weaponisammo == 0)
				{
					edit_grideleprof->reloadqty = atol(imgui_setpropertystring2_v2(t.group, Str(edit_grideleprof->reloadqty), t.strarr_s[422].Get(), t.strarr_s[212].Get(),readonly));
					edit_grideleprof->fireiterations = atol(imgui_setpropertystring2_v2(t.group, Str(edit_grideleprof->fireiterations), t.strarr_s[423].Get(), t.strarr_s[213].Get(),readonly));
					edit_grideleprof->range = atol(imgui_setpropertystring2_v2(t.group, Str(edit_grideleprof->range), "Range", "Maximum range of bullet travel",readonly));
					edit_grideleprof->dropoff = atol(imgui_setpropertystring2_v2(t.group, Str(edit_grideleprof->dropoff), "Dropoff", "Amount in inches of vertical dropoff per 100 feet of bullet travel",readonly));
					edit_grideleprof->clipcapacity = atol(imgui_setpropertystring2_v2(t.group, Str(edit_grideleprof->clipcapacity), "Clip Capacity", "The total maximum number of clips the player can carry for this weapon", readonly));
				}
				else
				{
					edit_grideleprof->lifespan = atol(imgui_setpropertystring2_v2(t.group, Str(edit_grideleprof->lifespan), t.strarr_s[424].Get(), t.strarr_s[214].Get(),readonly));
					edit_grideleprof->throwspeed = atol(imgui_setpropertystring2_v2(t.group, Str(edit_grideleprof->throwspeed), t.strarr_s[425].Get(), t.strarr_s[215].Get(),readonly));
					edit_grideleprof->throwangle = atol(imgui_setpropertystring2_v2(t.group, Str(edit_grideleprof->throwangle), t.strarr_s[426].Get(), t.strarr_s[216].Get(),readonly));
					edit_grideleprof->bounceqty = atol(imgui_setpropertystring2_v2(t.group, Str(edit_grideleprof->bounceqty), t.strarr_s[427].Get(), t.strarr_s[217].Get(),readonly));
					edit_grideleprof->explodeonhit = imgui_setpropertylist2_v2(t.group, t.controlindex, Str(edit_grideleprof->explodeonhit), t.strarr_s[428].Get(), t.strarr_s[218].Get(), 0,readonly);
				}
				if (t.tflagsimpler == 0)
				{
					edit_grideleprof->usespotlighting = imgui_setpropertylist2_v2(t.group, t.controlindex, Str(edit_grideleprof->usespotlighting), "Spot Lighting", "Set whether emits dynamic spot lighting", 0,readonly);
				}
			}

			//  Is Character
			if (t.tflagchar == 1)
			{
				if (t.tflagsimpler == 0)
				{
					// special check to avoid offering can take weapon if no HUD.X
					t.tfile_s = cstr("gamecore\\guns\\") + edit_grideleprof->hasweapon_s + cstr("\\HUD.X");
					if (FileExist(t.tfile_s.Get()) == 1)
					{
						edit_grideleprof->cantakeweapon = imgui_setpropertylist2_v2(t.group, t.controlindex, Str(edit_grideleprof->cantakeweapon), t.strarr_s[429].Get(), t.strarr_s[219].Get(), 0,readonly);
						//Take Weapon's Ammo
						cstr fieldname = t.strarr_s[430];
						if (fieldname == "Take Weapon's Ammo") fieldname = "Take Weapon Ammo"; //Need to be shorter.
						edit_grideleprof->quantity = atol(imgui_setpropertystring2_v2(t.group, Str(edit_grideleprof->quantity), fieldname.Get(), t.strarr_s[220].Get(),readonly));
					}
					edit_grideleprof->rateoffire = atol(imgui_setpropertystring2_v2(t.group, Str(edit_grideleprof->rateoffire), t.strarr_s[431].Get(), t.strarr_s[221].Get(),readonly));
				}
			}
			if (t.tflagquantity == 1 && g.quickparentalcontrolmode != 2 )//&& edit_grideleprof->lives == 0)
			{
				edit_grideleprof->quantity = atol(imgui_setpropertystring2_v2(t.group, Str(edit_grideleprof->quantity), t.strarr_s[432].Get(), t.strarr_s[222].Get(),readonly));
			}

			//  AI Extra
			if (t.tflagvis == 1 && t.tflagai == 1)
			{
				if (t.tflagchar == 1)
				{
					edit_grideleprof->coneangle = atol(imgui_setpropertystring2_v2(t.group, Str(edit_grideleprof->coneangle), t.strarr_s[434].Get(), t.strarr_s[224].Get(),readonly));
					edit_grideleprof->conerange = atol(imgui_setpropertystring2_v2(t.group, Str(edit_grideleprof->conerange), "View Range", "The range within which the AI may see the player. Zero triggers the characters default range.",readonly));
					if (t.entityprofile[entid].ischaracter != 1)
					{
						// but not characters which now use IFUSED to contain LOOT information
						edit_grideleprof->ifused_s = imgui_setpropertystring2_v2(t.group, edit_grideleprof->ifused_s.Get(), t.strarr_s[437].Get(), t.strarr_s[226].Get(), readonly);
					}
					edit_grideleprof->hasweapon_s = imgui_setpropertystring2_v2(t.group, edit_grideleprof->hasweapon_s.Get(), "Has Weapon", "The weapon assigned to this object", readonly);
					if (g.quickparentalcontrolmode != 2)
					{
						// this one you see in Developer Settings UI
						edit_grideleprof->isviolent = imgui_setpropertylist2_v2(t.group, t.controlindex, Str(edit_grideleprof->isviolent), "Blood Effects", "Sets whether blood and screams should be used", 0, readonly);
					}
					if (t.tflagsimpler == 0)
					{
						edit_grideleprof->colondeath = imgui_setpropertylist2_v2(t.group, t.controlindex, Str(edit_grideleprof->colondeath), "End Collision", "Set to NO switches off collision when die", 0,readonly);
					}
				}
				else
				{
					if (t.tflagweap == 0 && t.tflagammo == 0)
					{
						//t.propfield[t.group] = t.controlindex;
						//++t.group; startgroup(t.strarr_s[435].Get()); t.controlindex = 0;
						edit_grideleprof->usekey_s = imgui_setpropertystring2_v2(t.group, edit_grideleprof->usekey_s.Get(), t.strarr_s[436].Get(), t.strarr_s[225].Get(),readonly);
						if (t.tflagsimpler != 0 & t.entityprofile[entid].ismarker == 3 && t.entityprofile[entid].trigger.stylecolor == 1)
						{
							// only one level - no winzone chain option
						}
						else
						{
							if (t.entityprofile[entid].ischaracter != 1)
							{
								// but not characters which now use IFUSED to contain LOOT information
								edit_grideleprof->ifused_s = imgui_setpropertystring2_v2(t.group, edit_grideleprof->ifused_s.Get(), t.strarr_s[437].Get(), t.strarr_s[226].Get(), readonly);
							}
						}
					}
				}
			}
			if (t.tflagifused == 1)
			{
				if (t.tflagusekey == 1)
				{
					edit_grideleprof->usekey_s = imgui_setpropertystring2_v2(t.group, edit_grideleprof->usekey_s.Get(), t.strarr_s[436].Get(), t.strarr_s[225].Get(),readonly);
				}
				if (t.tflagsimpler != 0 & t.entityprofile[entid].ismarker == 3 && t.entityprofile[entid].trigger.stylecolor == 1)
				{
					// only one level - no winzone chain option
				}
				else
				{
					if (t.entityprofile[entid].ischaracter != 1)
					{
						// but not characters which now use IFUSED to contain LOOT information
						edit_grideleprof->ifused_s = imgui_setpropertystring2_v2(t.group, edit_grideleprof->ifused_s.Get(), t.strarr_s[438].Get(), t.strarr_s[227].Get(), readonly);
					}
				}
			}

		}

		if (t.tflagspawn == 1)
		{
			t.group = 1;
			ImGui::TextCenter(t.strarr_s[439].Get());
			{
				edit_grideleprof->spawnatstart = imgui_setpropertylist2_v2(t.group, t.controlindex, Str(edit_grideleprof->spawnatstart), t.strarr_s[562].Get(), t.strarr_s[563].Get(), 0,readonly);
			}
		}


		//  Statistics
		if ((t.tflagvis == 1 || t.tflagobjective == 1 || t.tflaglives == 1 || t.tflagstats == 1) && t.tflagweap == 0 && t.tflagammo == 0)
		{
			t.group = 1;
			ImGui::TextCenter(t.strarr_s[451].Get());
			{
				if (t.tflaglives == 1)
				{
					//LB: Decision here: https://github.com/Dark-Basic-Software-Limited/GameGuruRepo/issues/6007
					// see if this level has any checkpoints to stave off lives logic
					//bool bUsingCheckpoint = false;
					//for ( int e = 1; e <= g.entityelementlist; e++)
					//{
					//	int entid = t.entityelement[e].bankindex;
					//	if (t.entityprofile[entid].ismarker == 6)
					//	{
					//		bUsingCheckpoint = true;
					//		break;
					//	}
					//}
					//if (bUsingCheckpoint==true)
					//{
					//	ImGui::TextCenter("Lives");
					//	ImGui::TextCenter("NOTE: Checkpoint detected, infinite retries");
					//	edit_grideleprof->lives = 0;
					//}
					//else
					//{
					edit_grideleprof->lives = atol(imgui_setpropertystring2_v2(t.group, Str(edit_grideleprof->lives), t.strarr_s[452].Get(), "Specifies how many lives the player starts with. Enter zero for infinite lives.", readonly));
					//}
				}
				if (t.tflagvis == 1 || t.tflagstats == 1)
				{
					if (t.tflaglives == 1)
					{
						edit_grideleprof->strength = atol(imgui_setpropertystring2_v2(t.group, Str(edit_grideleprof->strength), t.strarr_s[453].Get(), t.strarr_s[243].Get(),readonly));
					}
					else
					{
						if (t.tflagnotionofhealth == 1)
						{
							edit_grideleprof->strength = atol(imgui_setpropertystring2_v2(t.group, Str(edit_grideleprof->strength), t.strarr_s[454].Get(), t.strarr_s[244].Get(),readonly));
						}
					}
					if (t.tflagplayersettings == 1)
					{
						if (g.quickparentalcontrolmode != 2)
						{
							edit_grideleprof->isviolent = imgui_setpropertylist2_v2(t.group, t.controlindex, Str(edit_grideleprof->isviolent), "Blood Effects", "Sets whether blood and screams should be used", 0,readonly);
						}
						if (t.tflagnotionofhealth == 1 )//&& edit_grideleprof->lives == 0)
						{
							t.playercontrol.regenrate = atol(imgui_setpropertystring2_v2(t.group, Str(t.playercontrol.regenrate), "Regeneration Rate", "Sets the increase value at which the players health will restore",readonly));
							t.playercontrol.regenspeed = atol(imgui_setpropertystring2_v2(t.group, Str(t.playercontrol.regenspeed), "Regeneration Speed", "Sets the speed in milliseconds at which the players health will regenerate",readonly));
							t.playercontrol.regendelay = atol(imgui_setpropertystring2_v2(t.group, Str(t.playercontrol.regendelay), "Regeneration Delay", "Sets the delay in milliseconds after last damage hit before health starts regenerating",readonly));
						}
						edit_grideleprof->usespotlighting = imgui_setpropertylist2_v2(t.group, t.controlindex, Str(edit_grideleprof->usespotlighting), "Flashlight Disabled", "Sets whether the flashlight is disabled for the player", 0, readonly);
					}

					edit_grideleprof->speed = atol(imgui_setpropertystring2_v2(t.group, Str(edit_grideleprof->speed), t.strarr_s[455].Get(), t.strarr_s[245].Get(),readonly));
					if (t.playercontrol.thirdperson.enabled == 1)
					{
						t.tanimspeed_f = t.entityelement[t.playercontrol.thirdperson.charactere].eleprof.animspeed;
					}
					else
					{
						t.tanimspeed_f = edit_grideleprof->animspeed;
					}
					t.tanimspeed_f = atof(imgui_setpropertystring2_v2(t.group, Str(t.tanimspeed_f), "Anim Speed", "Sets the default speed of any animation associated with this entity",readonly));

					if (t.playercontrol.thirdperson.enabled == 1)
					{
						t.entityelement[t.playercontrol.thirdperson.charactere].eleprof.animspeed = t.tanimspeed_f;
					}
					else
					{
						edit_grideleprof->animspeed = t.tanimspeed_f;
					}
				}
				if (t.tflaghurtfall == 1) 
				{
					edit_grideleprof->hurtfall = atol(imgui_setpropertystring2_v2(t.group, Str(edit_grideleprof->hurtfall), t.strarr_s[456].Get(), t.strarr_s[246].Get(),readonly));
				}
				if (t.tflagplayersettings == 1)
				{
					t.playercontrol.jumpmax_f = atof(imgui_setpropertystring2_v2(t.group, Str(t.playercontrol.jumpmax_f), "Jump Speed", "Sets the jump speed of the player which controls overall jump height",readonly));
					t.playercontrol.gravity_f = atof(imgui_setpropertystring2_v2(t.group, Str(t.playercontrol.gravity_f), "Gravity", "Sets the modified force percentage of the players own gravity",readonly));
					t.playercontrol.fallspeed_f = atof(imgui_setpropertystring2_v2(t.group, Str(t.playercontrol.fallspeed_f), "Fall Speed", "Sets the maximum speed percentage at which the player will fall",readonly));
					t.playercontrol.fFallDamageMultiplier = atof(imgui_setpropertystring2_v2(t.group, Str(t.playercontrol.fFallDamageMultiplier), "Fall Damage Multiplier", "Modifies the damage dealt to the player when landing from a fall", readonly));
					t.playercontrol.climbangle_f = atof(imgui_setpropertystring2_v2(t.group, Str(t.playercontrol.climbangle_f), "Climb Angle", "Sets the maximum angle permitted for the player to ascend a slope",readonly));
					if (t.playercontrol.thirdperson.enabled == 0)
					{
						t.playercontrol.wobblespeed_f = atof(imgui_setpropertystring2_v2(t.group, Str(t.playercontrol.wobblespeed_f), "Wobble Speed", "Sets the rate of motion applied to the camera when moving",readonly));
						t.playercontrol.wobbleheight_f = atof(imgui_setpropertystring2_v2(t.group, Str(t.playercontrol.wobbleheight_f * 100), "Wobble Height", "Sets the degree of motion applied to the camera when moving",readonly)) / 100.0f;
						t.playercontrol.footfallpace_f = atof(imgui_setpropertystring2_v2(t.group, Str(t.playercontrol.footfallpace_f * 100), "Footfall Pace", "Sets the rate at which the footfall sound is played when moving",readonly)) / 100.0f;
					}
					t.playercontrol.accel_f = atof(imgui_setpropertystring2_v2(t.group, Str(t.playercontrol.accel_f * 100), "Acceleration", "Sets the acceleration curve used when t.moving from t.a stood position",readonly)) / 100.0f;
				}
				if (t.tflagmobile == 1)
				{
					edit_grideleprof->isimmobile = imgui_setpropertylist2_v2(t.group, t.controlindex, Str(edit_grideleprof->isimmobile), t.strarr_s[457].Get(), t.strarr_s[247].Get(), 0,readonly);
				}
				if (t.tflagmobile == 1)
				{
					if (t.tflagsimpler == 0)
					{
						edit_grideleprof->lodmodifier = atol(imgui_setpropertystring2_v2(t.group, Str(edit_grideleprof->lodmodifier), "LOD Modifier", "Modify when the LOD transition takes effect. The default value is 0, increase this to a percentage reduce the LOD effect.",readonly));
					}
				}
			}
		}

		// physics for FPE
		DisplayFPEPhysics(false, entid, edit_grideleprof);

		//  Ammo data (FPGC - 280809 - filtered fpgcgenre=1 is shooter genre
		if (g.fpgcgenre == 1)
		{
			if (t.tflagammo == 1 || t.tflagammoclip == 1)
			{
				//if (ImGui::StyleCollapsingHeader(t.strarr_s[459].Get(), ImGuiTreeNodeFlags_DefaultOpen)) 
				ImGui::TextCenter(t.strarr_s[459].Get());
				{
					edit_grideleprof->quantity = atol(imgui_setpropertystring2_v2(t.group, Str(edit_grideleprof->quantity), t.strarr_s[460].Get(), t.strarr_s[249].Get(),readonly));
				}

			}
		}

		//  Light data
		if (t.tflaglight == 1)
		{
			bool bIsLightProbe = false;
			if (edit_grideleprof->light.fLightHasProbe >= 50.0f) bIsLightProbe = true;
			if (bIsLightProbe == false)
			{
				int iPrevValue = -1;
				bool bUpdateLight = false;
				float colors[5];

				ImGui::TextCenter(t.strarr_s[461].Get());
				iPrevValue = edit_grideleprof->light.range;
				edit_grideleprof->light.range = atol(imgui_setpropertystring2_v2(t.group, Str(edit_grideleprof->light.range), t.strarr_s[462].Get(), t.strarr_s[250].Get(), readonly)); //PE: 462=Light Range
				if (iPrevValue != edit_grideleprof->light.range)
					bUpdateLight = true;

				ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX(), ImGui::GetCursorPosY() + 3));
				ImGui::Text(t.strarr_s[463].Get());
				ImGui::SameLine();
				ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX(), ImGui::GetCursorPosY() - 3));
				ImGui::SetCursorPos(ImVec2(fPropertiesColoumWidth, ImGui::GetCursorPosY()));
				ImGui::PushItemWidth(-10);

				colors[3] = ((edit_grideleprof->light.color & 0xff000000) >> 24) / 255.0f;
				colors[0] = ((edit_grideleprof->light.color & 0x00ff0000) >> 16) / 255.0f;
				colors[1] = ((edit_grideleprof->light.color & 0x0000ff00) >> 8) / 255.0f;
				colors[2] = (edit_grideleprof->light.color & 0x000000ff) / 255.0f;
				if (ImGui::ColorEdit3("##LightColorSetupField", colors, 0))
				{
					colors[0] *= 255.0f;
					colors[1] *= 255.0f;
					colors[2] *= 255.0f;
					colors[3] *= 255.0f;
					edit_grideleprof->light.color = 0xff000000 + ((unsigned int)colors[0] << 16) + ((unsigned int)colors[1] << 8) + +((unsigned int)colors[2]);
					bUpdateLight = true;
				}
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", t.strarr_s[251].Get());
				ImGui::PopItemWidth();

				if (t.tflagsimpler == 0)
				{
					iPrevValue = edit_grideleprof->usespotlighting;
					edit_grideleprof->usespotlighting = imgui_setpropertylist2_v2(t.group, t.controlindex, Str(edit_grideleprof->usespotlighting), "Spot Lighting", "Change dynamic light to spot lighting", 0, readonly);
					if (edit_grideleprof->usespotlighting != iPrevValue)
						bUpdateLight = true;
				}
				if (bUpdateLight)
				{
					float lightx = t.entityelement[elementID].x;
					float lighty = t.entityelement[elementID].y;
					float lightz = t.entityelement[elementID].z;
					float lightax = t.entityelement[elementID].rx;
					float lightay = t.entityelement[elementID].ry;
					float lightaz = t.entityelement[elementID].rz;
					float lightrange = edit_grideleprof->light.range;
					float spotlightradius = edit_grideleprof->light.offsetup;
					float fLightHasProbe = edit_grideleprof->light.fLightHasProbe;
					int colr = colors[0];
					int colg = colors[1];
					int colb = colors[2];
					bool bCanShadow = edit_grideleprof->castshadow;
					int iLightIndex = edit_grideleprof->light.index;
					t.infinilight[iLightIndex].x = lightx;
					t.infinilight[iLightIndex].y = lighty;
					t.infinilight[iLightIndex].z = lightz;
					t.infinilight[iLightIndex].f_angle_x = lightax;
					t.infinilight[iLightIndex].f_angle_y = lightay;
					t.infinilight[iLightIndex].f_angle_z = lightaz;
					t.infinilight[iLightIndex].range = lightrange;
					t.infinilight[iLightIndex].spotlightradius = spotlightradius;
					t.infinilight[iLightIndex].fLightHasProbe = fLightHasProbe;
					t.infinilight[iLightIndex].colrgb.r = colr;
					t.infinilight[iLightIndex].colrgb.g = colg;
					t.infinilight[iLightIndex].colrgb.b = colb;
					t.infinilight[iLightIndex].bCanShadow = bCanShadow;
					uint64_t iWickedLightIndex = t.infinilight[iLightIndex].wickedlightindex;
					WickedCall_UpdateLight(iWickedLightIndex, lightx, lighty, lightz, lightax, lightay, lightaz, lightrange, spotlightradius, colr, colg, colb, bCanShadow);
				}
			}
		}

		//  Decal data
		if (t.tflagtdecal == 1)
		{
			t.propfield[t.group] = t.controlindex;

			//  Decal Particle data
			if (t.tflagdecalparticle == 1)
			{
				//++t.group; startgroup("Decal Particle"); t.controlindex = 0;
				ImGui::TextCenter("Decal Particle");
				{
					edit_grideleprof->particleoverride = imgui_setpropertylist2_v2(t.group, t.controlindex, Str(edit_grideleprof->particleoverride), "Custom Settings", "Whether you wish to override default settings", 0,readonly);
					edit_grideleprof->particle.offsety = atol(imgui_setpropertystring2_v2(t.group, Str(edit_grideleprof->particle.offsety), "OffsetY", "Vertical adjustment of start position",readonly));
					edit_grideleprof->particle.scale = atol(imgui_setpropertystring2_v2(t.group, Str(edit_grideleprof->particle.scale), "Scale", "A value from 0 to 100, denoting size of particle",readonly));
					edit_grideleprof->particle.randomstartx = atol(imgui_setpropertystring2_v2(t.group, Str(edit_grideleprof->particle.randomstartx), "Random Start X", "Random start area",readonly));
					edit_grideleprof->particle.randomstarty = atol(imgui_setpropertystring2_v2(t.group, Str(edit_grideleprof->particle.randomstarty), "Random Start Y", "Random start area",readonly));
					edit_grideleprof->particle.randomstartz = atol(imgui_setpropertystring2_v2(t.group, Str(edit_grideleprof->particle.randomstartz), "Random Start Z", "Random start area",readonly));
					edit_grideleprof->particle.linearmotionx = atol(imgui_setpropertystring2_v2(t.group, Str(edit_grideleprof->particle.linearmotionx), "Linear Motion X", "Constant motion direction",readonly));
					edit_grideleprof->particle.linearmotiony = atol(imgui_setpropertystring2_v2(t.group, Str(edit_grideleprof->particle.linearmotiony), "Linear Motion Y", "Constant motion direction",readonly));
					edit_grideleprof->particle.linearmotionz = atol(imgui_setpropertystring2_v2(t.group, Str(edit_grideleprof->particle.linearmotionz), "Linear Motion Z", "Constant motion direction",readonly));
					edit_grideleprof->particle.randommotionx = atol(imgui_setpropertystring2_v2(t.group, Str(edit_grideleprof->particle.randommotionx), "Random Motion X", "Random motion direction",readonly));
					edit_grideleprof->particle.randommotiony = atol(imgui_setpropertystring2_v2(t.group, Str(edit_grideleprof->particle.randommotiony), "Random Motion Y", "Random motion direction",readonly));
					edit_grideleprof->particle.randommotionz = atol(imgui_setpropertystring2_v2(t.group, Str(edit_grideleprof->particle.randommotionz), "Random Motion Z", "Random motion direction",readonly));
					edit_grideleprof->particle.mirrormode = atol(imgui_setpropertystring2_v2(t.group, Str(edit_grideleprof->particle.mirrormode), "Mirror Mode", "Set to one to reverse the particle",readonly));
					edit_grideleprof->particle.camerazshift = atol(imgui_setpropertystring2_v2(t.group, Str(edit_grideleprof->particle.camerazshift), "Camera Z Shift", "Shift t.particle towards camera",readonly));
					edit_grideleprof->particle.scaleonlyx = atol(imgui_setpropertystring2_v2(t.group, Str(edit_grideleprof->particle.scaleonlyx), "Scale Only X", "Percentage X over Y scale",readonly));
					edit_grideleprof->particle.lifeincrement = atol(imgui_setpropertystring2_v2(t.group, Str(edit_grideleprof->particle.lifeincrement), "Life Increment", "Control lifespan of particle",readonly));
					edit_grideleprof->particle.alphaintensity = atol(imgui_setpropertystring2_v2(t.group, Str(edit_grideleprof->particle.alphaintensity), "Alpha Intensity", "Control alpha percentage of particle",readonly));
					//  V118 - 060810 - knxrb - Decal animation setting (Added animation choice setting).
					edit_grideleprof->particle.animated = imgui_setpropertylist2_v2(t.group, t.controlindex, Str(edit_grideleprof->particle.animated), "Animated Particle", "Sets whether the t.particle t.decal Texture is animated or static.", 0,readonly);
				}
			}
		}

		// New Particle Component
		if (t.tflagnewparticle == 1)
		{
			cstr newfile_s = imgui_setpropertyfile2_v2(t.group, edit_grideleprof->newparticle.emittername.Get(), "Particle File", "Select a particle file (this is temporary and not the final design)", "particlesbank\\",readonly);
			if (strnicmp(newfile_s.Get() + strlen(newfile_s.Get()) - 4, ".arx", 4) == NULL) newfile_s = Left(newfile_s.Get(), Len(newfile_s.Get()) - 4);
			if (newfile_s != edit_grideleprof->newparticle.emittername)
			{
				edit_grideleprof->newparticle.emittername = newfile_s;
				if (edit_grideleprof->newparticle.emitterid != -1)
				{
					gpup_deleteEffect(edit_grideleprof->newparticle.emitterid);
					edit_grideleprof->newparticle.emitterid = -1;
				}
				//PE: Activate instantly.
				if (elementID > 0)
				{
					t.entityelement[elementID].eleprof.newparticle.emittername = edit_grideleprof->newparticle.emittername;
					t.entityelement[elementID].eleprof.newparticle.emitterid = -1;
					entity_updateparticleemitter(elementID);
					edit_grideleprof->newparticle.emitterid = t.entityelement[elementID].eleprof.newparticle.emitterid;
				}
			}
		}

		// Sound
		if (t.tflagsound == 1 || t.tflagsoundset == 1 || tflagtext == 1 || tflagimage == 1)
		{
			cstr group_text;
			if (tflagtext == 1 || tflagimage == 1)
			{
				if (tflagtext == 1) group_text = "Text";
				if (tflagimage == 1) group_text = "Image";
			}
			else
			{
				group_text = "Media";
			}

			if (speech_entries > 0)
			{
			}

			ImGui::TextCenter(group_text.Get());
			{
				if (g.fpgcgenre == 1)
				{
					if (t.entityprofile[entid].ischaracter > 0)
					{
						edit_grideleprof->soundset_s = imgui_setpropertyfile2_v2(t.group, edit_grideleprof->soundset_s.Get(), "Sound0", t.strarr_s[254].Get(), "audiobank\\",readonly);
					}
					else
					{
						if (g.vrqcontrolmode != 0)
						{
							if (t.tflagsound == 1 && t.tflagsoundset != 1)
							{
								//PE: changed from 469 to 467 , should be sound0
								edit_grideleprof->soundset_s = imgui_setpropertyfile2_v2(t.group, edit_grideleprof->soundset_s.Get(), t.strarr_s[467].Get(), t.strarr_s[253].Get(), "audiobank\\",readonly);
							}
						}
						else
						{
							if (t.tflagsound == 1 && t.tflagsoundset != 1)
							{
								edit_grideleprof->soundset_s = imgui_setpropertyfile2_v2(t.group, edit_grideleprof->soundset_s.Get(), t.strarr_s[467].Get(), t.strarr_s[253].Get(), "audiobank\\",readonly);
							}
						}
						if (t.tflagsoundset == 1)
						{
							edit_grideleprof->soundset_s = imgui_setpropertyfile2_v2(t.group, edit_grideleprof->soundset_s.Get(), t.strarr_s[469].Get(), t.strarr_s[255].Get(), "audiobank\\voices\\",readonly);
						}
						if (tflagtext == 1)
						{
							edit_grideleprof->soundset_s = imgui_setpropertystring2_v2(t.group, edit_grideleprof->soundset_s.Get(), "Text to Appear", "Enter text to appear in-game",readonly);
						}
						if (tflagimage == 1)
						{
							edit_grideleprof->soundset_s = imgui_setpropertyfile2_v2(t.group, edit_grideleprof->soundset_s.Get(), "Image File", "Select image to appear in-game", "imagebank\\", readonly);
						}
					}

					if (t.tflagnosecond == 0)
					{
						if (t.tflagsound == 1 || t.tflagsoundset == 1)
						{
							//We got some missing translations.
							if (t.strarr_s[468] == "") t.strarr_s[468] = "Sound1";
							if (t.strarr_s[480] == "") t.strarr_s[480] = "Sound2";
							if (t.strarr_s[481] == "") t.strarr_s[481] = "Sound3";
							//if (t.strarr_s[482] == "") t.strarr_s[482] = "Sound4";
							edit_grideleprof->soundset1_s = imgui_setpropertyfile2_v2(t.group, edit_grideleprof->soundset1_s.Get(), t.strarr_s[468].Get(), t.strarr_s[254].Get(), "audiobank\\",readonly);
							edit_grideleprof->soundset2_s = imgui_setpropertyfile2_v2(t.group, edit_grideleprof->soundset2_s.Get(), t.strarr_s[480].Get(), t.strarr_s[254].Get(), "audiobank\\",readonly);
							edit_grideleprof->soundset3_s = imgui_setpropertyfile2_v2(t.group, edit_grideleprof->soundset3_s.Get(), t.strarr_s[481].Get(), t.strarr_s[254].Get(), "audiobank\\",readonly);
							//ImGui::TextCenter("Sound4");
							//ImGui::TextCenter("(repurposed)");
							edit_grideleprof->soundset4a_s = imgui_setpropertyfile2_v2(t.group, edit_grideleprof->soundset4a_s.Get(), "Sound4", t.strarr_s[254].Get(), "audiobank\\", readonly);
							edit_grideleprof->soundset5_s = imgui_setpropertyfile2_v2(t.group, edit_grideleprof->soundset5_s.Get(), "Sound5", t.strarr_s[254].Get(), "audiobank\\", readonly);
							edit_grideleprof->soundset6_s = imgui_setpropertyfile2_v2(t.group, edit_grideleprof->soundset6_s.Get(), "Sound6", t.strarr_s[254].Get(), "audiobank\\", readonly);
						}
					}
				}
				else
				{
					if (t.tflagsoundset == 1)
					{
						edit_grideleprof->soundset_s = imgui_setpropertyfile2_v2(t.group, edit_grideleprof->soundset_s.Get(), t.strarr_s[469].Get(), t.strarr_s[255].Get(), "audiobank\\voices\\",readonly);
					}
					else
					{
						edit_grideleprof->soundset_s = imgui_setpropertyfile2_v2(t.group, edit_grideleprof->soundset_s.Get(), t.strarr_s[467].Get(), t.strarr_s[253].Get(), "audiobank\\",readonly); ++t.controlindex;
					}
					edit_grideleprof->soundset1_s = imgui_setpropertyfile2_v2(t.group, edit_grideleprof->soundset1_s.Get(), t.strarr_s[468].Get(), t.strarr_s[254].Get(), "audiobank\\",readonly); ++t.controlindex;
				}
			}
		}

		// Video
		if (t.tflagvideo == 1)
		{
			if (ImGui::StyleCollapsingHeader(t.strarr_s[597].Get(), ImGuiTreeNodeFlags_DefaultOpen)) {

				edit_grideleprof->soundset1_s = imgui_setpropertyfile2_v2(t.group, edit_grideleprof->soundset1_s.Get(), "Video Slot", t.strarr_s[601].Get(), "videobank\\",readonly);
			}
		}

		//  Third person settings
		if (t.tflagplayersettings == 1 && t.playercontrol.thirdperson.enabled == 1)
		{
			if (ImGui::StyleCollapsingHeader("Third Person", ImGuiTreeNodeFlags_DefaultOpen)) {

				t.livegroupforthirdperson = t.group;
				t.playercontrol.thirdperson.cameralocked = imgui_setpropertylist2_v2(t.group, t.controlindex, Str(t.playercontrol.thirdperson.cameralocked), "Camera Locked", "Fixes camera height and angle for third person view", 0,readonly);
				t.playercontrol.thirdperson.cameradistance = atol(imgui_setpropertystring2_v2(t.group, Str(t.playercontrol.thirdperson.cameradistance), "Camera Distance", "Sets the distance of the third person camera",readonly));
				t.playercontrol.thirdperson.camerashoulder = atol(imgui_setpropertystring2_v2(t.group, Str(t.playercontrol.thirdperson.camerashoulder), "Camera X Offset", "Sets the distance to shift the camera over shoulder",readonly));
				t.playercontrol.thirdperson.cameraheight = atol(imgui_setpropertystring2_v2(t.group, Str(t.playercontrol.thirdperson.cameraheight), "Camera Y Offset", "Sets the vertical height of the third person camera. If more than twice the camera distance, camera collision disables",readonly));
				t.playercontrol.thirdperson.camerafocus = atol(imgui_setpropertystring2_v2(t.group, Str(t.playercontrol.thirdperson.camerafocus), "Camera Focus", "Sets the camera X angle offset to align focus of the third person camera",readonly));
				t.playercontrol.thirdperson.cameraspeed = atol(imgui_setpropertystring2_v2(t.group, Str(t.playercontrol.thirdperson.cameraspeed), "Camera Speed", "Sets the retraction speed percentage of the third person camera",readonly));
				t.playercontrol.thirdperson.camerafollow = imgui_setpropertylist2_v2(t.group, t.controlindex, Str(t.playercontrol.thirdperson.camerafollow), "Run Mode", "If set to yes, protagonist uses WASD t.movement mode", 0,readonly);
				t.playercontrol.thirdperson.camerareticle = imgui_setpropertylist2_v2(t.group, t.controlindex, Str(t.playercontrol.thirdperson.camerareticle), "Show Reticle", "Show the third person 'crosshair' reticle Dot ( ", 0,readonly);
			}
		}
		//Advenced open
		ImGui::Indent(-10);
	}
}

int imgui_setpropertylist2_v2(int group, int controlindex, char* data_s, char* field_s, char* desc_s, int listtype,bool readonly)
{
	cstr ldata_s = data_s, ldesc_s = desc_s, lfields_s = field_s;

	if (cstr(data_s) == "" || !data_s)  ldata_s = "";
	if (cstr(desc_s) == "" || !desc_s)  ldesc_s = "";
	if (cstr(field_s) == "" || !field_s)  lfields_s = "";

	int current_selection = atoi(ldata_s.Get());


	int listmax = 0;

	listmax = 0;
	if (listtype == 0)
	{
		listmax = 1;
		t.list_s[0] = t.strarr_s[471];
		t.list_s[1] = t.strarr_s[470];
	}
	if (listtype == 1)
	{
		listmax = fillgloballistwithweapons();
	}
	if (listtype == 11)
	{
		listmax = fillgloballistwithbehaviours();
	}
	if (listtype == 21)
	{
		listmax = fillgloballistwithcollectables();
	}

	const char* current_item = t.list_s[current_selection].Get();

	std::string uniquiField = ""; //lfields_s.Get()
	uniquiField = uniquiField + "##" + lfields_s.Get();
	uniquiField = uniquiField + std::to_string(grideleprof_uniqui_id++);

	ImGui::PushItemWidth(-10);
	if (listtype == 0)
	{
		//Checkbox.
		bool bTmp = false;
		if (current_selection == 1)
			bTmp = true;
		ImGui::Checkbox(lfields_s.Get(), &bTmp);
		if (bTmp)
			current_selection = 1;
		else
			current_selection = 0;
	}
	else
	{
		ImGui::TextCenter(lfields_s.Get());
		if (ImGui::BeginCombo(uniquiField.c_str(), current_item)) // The second parameter is the label previewed before opening the combo.
		{
			for (int n = 0; n <= listmax; n++)
			{
				bool is_selected = (current_item == t.list_s[n].Get()); // You can store your selection however you want, outside or inside your objects
				if (ImGui::Selectable(t.list_s[n].Get(), is_selected)) {
					current_selection = n;
					current_item = t.list_s[n].Get();
				}
				if (is_selected)
					ImGui::SetItemDefaultFocus();   // You may set the initial focus when opening the combo (scrolling + for keyboard navigation support)
			}
			ImGui::EndCombo();
		}
	}
	if (ImGui::IsItemHovered())
	{
		if (listtype == 0)
		{
			//When using checkbox, change text.
			std::string newtext = ldesc_s.Get();
			replaceAll(newtext, "Set to YES", "If set");
			replaceAll(newtext, " to YES", "");
			ImGui::SetTooltip("%s", newtext.c_str());
		}
		else
			ImGui::SetTooltip("%s", ldesc_s.Get());
	}

	ImGui::PopItemWidth();
	return current_selection;
}

char* imgui_setpropertylist2c_v2(int group, int controlindex, char* data_s, char* field_s, char* desc_s, int listtype,bool readonly, bool forcharacters, bool bForShooting, bool bForMelee, int iSpecialValue)
{
	cstr ldata_s = data_s, ldesc_s = desc_s, lfields_s = field_s;
	if (cstr(data_s) == "" || !data_s)  ldata_s = "";
	if (cstr(desc_s) == "" || !desc_s)  ldesc_s = "";
	if (cstr(field_s) == "" || !field_s)  lfields_s = "";
	int current_selection = atoi(ldata_s.Get());
	int listmax = 0;
	bool bIgnoreTitleText = false;
	bool bIncludeSlotNotUsedChoice = false;
	if (listtype == 61)
	{
		listtype = 1;
		bIgnoreTitleText = true;
		bIncludeSlotNotUsedChoice = true;
	}
	if (listtype == 0)
	{
		listmax = 1;
		t.list_s[0] = t.strarr_s[471];
		t.list_s[1] = t.strarr_s[470];
	}
	if (listtype == 1)
	{
		// newer system for MAX weapons - easier to read for users
		listmax = fillgloballistwithweaponsQuick(forcharacters, bForShooting, bForMelee, bIncludeSlotNotUsedChoice);
		for (int n = -1; n <= listmax; n++)
		{
			cstr thisLabel;
			if (n == -1) 
				thisLabel = ldata_s;
			else
				thisLabel = t.list_s[n];
			if (bIgnoreTitleText == true)
			{
				if (strlen (thisLabel.Get()) == 0) thisLabel = "No Preference";
			}
			else
			{
				if (strlen (thisLabel.Get()) == 0) thisLabel = "No Weapon";
			}
			thisLabel = gun_names_tonormal(thisLabel.Get());
			if (n == -1)
				ldata_s = thisLabel;
			else
				t.list_s[n] = thisLabel;
		}
		for (int n = 0; n <= listmax; n++)
		{
			if (stricmp(ldata_s.Get(), t.list_s[n].Get()) == NULL)
			{
				current_selection = n;
				break;
			}
		}
	}
	if (listtype == 2)
	{
		listmax = fillgloballistwithCharAnimSetsQuick(iSpecialValue);
		for (int n = -1; n <= listmax; n++)
		{
			cstr thisLabel;
			if (n == -1)
				thisLabel = ldata_s;
			else
				thisLabel = t.list_s[n];
			if (strlen (thisLabel.Get()) == 0) thisLabel = "Default Animation";
			if (strlen (thisLabel.Get()) == 1) thisLabel = "Original Animation";
			if (stricmp (thisLabel.Get(), "charactercreatorplus\\animations\\sets\\adult male\\default animations.dbo") == NULL) thisLabel = "Adult Male Pistol";
			if (stricmp (thisLabel.Get(), "charactercreatorplus\\animations\\sets\\adult male\\default animations-rifle.dbo") == NULL) thisLabel = "Adult Male Rifle";
			if (stricmp (thisLabel.Get(), "charactercreatorplus\\animations\\sets\\adult male\\default animations-pistol-lowered.dbo") == NULL) thisLabel = "Adult Male Pistol Lowered";
			if (stricmp (thisLabel.Get(), "charactercreatorplus\\animations\\sets\\adult male\\default animations-rifle-lowered.dbo") == NULL) thisLabel = "Adult Male Rifle Lowered";
			if (stricmp (thisLabel.Get(), "charactercreatorplus\\animations\\sets\\adult male\\default animations-shotgun-lowered.dbo") == NULL) thisLabel = "Adult Male Shotgun Lowered";
			if (stricmp (thisLabel.Get(), "charactercreatorplus\\animations\\sets\\adult male\\default animations-melee.dbo") == NULL) thisLabel = "Adult Male Melee";
			if (stricmp (thisLabel.Get(), "charactercreatorplus\\animations\\sets\\adult male\\default animations-axe.dbo") == NULL) thisLabel = "Adult Male Axe";
			if (stricmp (thisLabel.Get(), "charactercreatorplus\\animations\\sets\\adult male\\default animations-spear.dbo") == NULL) thisLabel = "Adult Male Spear";

			if (stricmp (thisLabel.Get(), "charactercreatorplus\\animations\\sets\\adult female\\default animations.dbo") == NULL) thisLabel = "Adult Female Pistol";
			if (stricmp (thisLabel.Get(), "charactercreatorplus\\animations\\sets\\adult female\\default animations-rifle.dbo") == NULL) thisLabel = "Adult Female Rifle";
			if (stricmp (thisLabel.Get(), "charactercreatorplus\\animations\\sets\\adult female\\default animations-pistol-lowered.dbo") == NULL) thisLabel = "Adult Female Pistol Lowered";
			if (stricmp (thisLabel.Get(), "charactercreatorplus\\animations\\sets\\adult female\\default animations-rifle-lowered.dbo") == NULL) thisLabel = "Adult Female Rifle Lowered";
			if (stricmp (thisLabel.Get(), "charactercreatorplus\\animations\\sets\\adult female\\default animations-shotgun-lowered.dbo") == NULL) thisLabel = "Adult Female Shotgun Lowered";
			if (stricmp (thisLabel.Get(), "charactercreatorplus\\animations\\sets\\adult female\\default animations-melee.dbo") == NULL) thisLabel = "Adult Female Melee";
			if (stricmp (thisLabel.Get(), "charactercreatorplus\\animations\\sets\\adult female\\default animations-axe.dbo") == NULL) thisLabel = "Adult Female Axe";
			if (stricmp (thisLabel.Get(), "charactercreatorplus\\animations\\sets\\adult female\\default animations-spear.dbo") == NULL) thisLabel = "Adult Female Spear";

			if (stricmp (thisLabel.Get(), "charactercreatorplus\\animations\\sets\\zombie male\\default animations.dbo") == NULL) thisLabel = "Zombie Male";
			if (stricmp (thisLabel.Get(), "charactercreatorplus\\animations\\sets\\zombie female\\default animations.dbo") == NULL) thisLabel = "Zombie Female";

			if (stricmp(thisLabel.Get(), "charactercreatorplus\\animations\\sets\\low poly\\default animations-melee.dbo") == NULL) thisLabel = "Low Poly Melee";
			if (stricmp(thisLabel.Get(), "charactercreatorplus\\animations\\sets\\low poly\\default animations-axe.dbo") == NULL) thisLabel = "Low Poly Axe";
			if (stricmp(thisLabel.Get(), "charactercreatorplus\\animations\\sets\\low poly\\default animations-spear.dbo") == NULL) thisLabel = "Low Poly Spear";

			if (n == -1)
				ldata_s = thisLabel;
			else
				t.list_s[n] = thisLabel;
		}
		for (int n = 0; n <= listmax; n++)
		{
			if (ldata_s == t.list_s[n])
			{
				current_selection = n;
				break;
			}
		}
	}
	if (listtype == 3)
	{
		// new system for MAX weapons - interchangable hands
		listmax = fillgloballistwithHands();
		for (int n = -1; n <= listmax; n++)
		{
			cstr thisLabel = ldata_s;
			if (n != -1) thisLabel = t.list_s[n];
			if (strlen (thisLabel.Get()) == 0) thisLabel = "No Preference";
			if (n == -1)
				ldata_s = thisLabel;
			else
				t.list_s[n] = thisLabel;
		}
		for (int n = 0; n <= listmax; n++)
		{
			if (ldata_s == t.list_s[n])
			{
				current_selection = n;
				break;
			}
		}
	}
	if (listtype == 11)
	{
		listmax = fillgloballistwithbehaviours();
		for (int n = 0; n <= listmax; n++)
		{
			if (ldata_s == t.list_s[n]) 
			{
				current_selection = n;
				break;
			}
		}
	}
	if (listtype == 21)
	{
		listmax = fillgloballistwithcollectables();
		for (int n = 0; n <= listmax; n++)
		{
			if (ldata_s == t.list_s[n]) 
			{
				current_selection = n;
				break;
			}
		}
	}

	if (listtype == 31)
	{
		listmax = fillgloballistwithdecals(t.list_s);
		for (int n = 0; n < listmax; n++)
		{
			if (ldata_s == t.list_s[n])
			{
				current_selection = n;
				break;
			}
		}
	}

	if (listtype == 32)
	{
		listmax = fillgloballistwithvoices(t.list_s);
		for (int n = 0; n <= listmax; n++)
		{
			if (ldata_s == t.list_s[n])
			{
				current_selection = n;
				break;
			}
		}
	}

	const char* current_item = t.list_s[current_selection].Get();

	std::string uniquiField = "";
	uniquiField = uniquiField + "##" + lfields_s.Get();
	uniquiField = uniquiField + std::to_string(grideleprof_uniqui_id++);

	if (bIgnoreTitleText == false)
	{
		ImGui::TextCenter(lfields_s.Get());
		ImGui::PushItemWidth(-10);
	}
	if (ImGui::BeginCombo(uniquiField.c_str(), current_item))
	{
		for (int n = 0; n <= listmax; n++)
		{
			bool is_selected = (current_item == t.list_s[n].Get());
			if (ImGui::Selectable(t.list_s[n].Get(), is_selected))
			{
				current_selection = n;
				current_item = t.list_s[n].Get();
			}
			if (is_selected) ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}
	if (bIgnoreTitleText == false)
	{
		ImGui::PopItemWidth();
	}

	// newer system for MAX weapons - easier to read for users
	if (listtype == 2)
	{
		for (int n = -1; n <= listmax; n++)
		{
			cstr thisLabel;
			if (n == -1)
				thisLabel = ldata_s;
			else
				thisLabel = t.list_s[n];
			if (stricmp (thisLabel.Get(), "Default Animation") == NULL) thisLabel = "";
			if (stricmp (thisLabel.Get(), "Original Animation") == NULL) thisLabel = "-";
			if (stricmp (thisLabel.Get(), "Adult Male Pistol") == NULL) thisLabel = "charactercreatorplus\\animations\\sets\\adult male\\default animations.dbo";
			if (stricmp (thisLabel.Get(), "Adult Male Rifle") == NULL) thisLabel = "charactercreatorplus\\animations\\sets\\adult male\\default animations-rifle.dbo";
			if (stricmp (thisLabel.Get(), "Adult Male Pistol Lowered") == NULL) thisLabel = "charactercreatorplus\\animations\\sets\\adult male\\default animations-pistol-lowered.dbo";
			if (stricmp (thisLabel.Get(), "Adult Male Rifle Lowered") == NULL) thisLabel = "charactercreatorplus\\animations\\sets\\adult male\\default animations-rifle-lowered.dbo";
			if (stricmp (thisLabel.Get(), "Adult Male Shotgun Lowered") == NULL) thisLabel = "charactercreatorplus\\animations\\sets\\adult male\\default animations-shotgun-lowered.dbo";
			if (stricmp (thisLabel.Get(), "Adult Male Melee") == NULL) thisLabel = "charactercreatorplus\\animations\\sets\\adult male\\default animations-melee.dbo";
			if (stricmp (thisLabel.Get(), "Adult Male Axe") == NULL) thisLabel = "charactercreatorplus\\animations\\sets\\adult male\\default animations-axe.dbo";
			if (stricmp (thisLabel.Get(), "Adult Male Spear") == NULL) thisLabel = "charactercreatorplus\\animations\\sets\\adult male\\default animations-spear.dbo";

			if (stricmp (thisLabel.Get(), "Adult Female Pistol") == NULL) thisLabel = "charactercreatorplus\\animations\\sets\\adult female\\default animations.dbo";
			if (stricmp (thisLabel.Get(), "Adult Female Rifle") == NULL) thisLabel = "charactercreatorplus\\animations\\sets\\adult female\\default animations-rifle.dbo";
			if (stricmp (thisLabel.Get(), "Adult Female Pistol Lowered") == NULL) thisLabel = "charactercreatorplus\\animations\\sets\\adult female\\default animations-pistol-lowered.dbo";
			if (stricmp (thisLabel.Get(), "Adult Female Rifle Lowered") == NULL) thisLabel = "charactercreatorplus\\animations\\sets\\adult female\\default animations-rifle-lowered.dbo";
			if (stricmp (thisLabel.Get(), "Adult Female Shotgun Lowered") == NULL) thisLabel = "charactercreatorplus\\animations\\sets\\adult female\\default animations-shotgun-lowered.dbo";
			if (stricmp (thisLabel.Get(), "Adult Female Melee") == NULL) thisLabel = "charactercreatorplus\\animations\\sets\\adult female\\default animations-melee.dbo";
			if (stricmp (thisLabel.Get(), "Adult Female Axe") == NULL) thisLabel = "charactercreatorplus\\animations\\sets\\adult female\\default animations-axe.dbo";
			if (stricmp (thisLabel.Get(), "Adult Female Spear") == NULL) thisLabel = "charactercreatorplus\\animations\\sets\\adult female\\default animations-spear.dbo";

			if (stricmp (thisLabel.Get(), "Zombie Male") == NULL) thisLabel = "charactercreatorplus\\animations\\sets\\zombie male\\default animations.dbo";
			if (stricmp (thisLabel.Get(), "Zombie Female") == NULL) thisLabel = "charactercreatorplus\\animations\\sets\\zombie female\\default animations.dbo";

			if (stricmp(thisLabel.Get(), "Low Poly Melee") == NULL) thisLabel = "charactercreatorplus\\animations\\sets\\low poly\\default animations-melee.dbo";
			if (stricmp(thisLabel.Get(), "Low Poly Axe") == NULL) thisLabel = "charactercreatorplus\\animations\\sets\\low poly\\default animations-axe.dbo";
			if (stricmp(thisLabel.Get(), "Low Poly Spear") == NULL) thisLabel = "charactercreatorplus\\animations\\sets\\low poly\\default animations-spear.dbo";

			if (n == -1)
				ldata_s = thisLabel;
			else
				t.list_s[n] = thisLabel;
		}
	}
	else
	{
		if (listtype == 31)
		{
			if (current_selection == 0)
				return("");
		}
		else
		{
			for (int n = -1; n <= listmax; n++)
			{
				cstr thisLabel;
				if (n == -1)
					thisLabel = ldata_s;
				else
					thisLabel = t.list_s[n];
				thisLabel = gun_names_tointernal(thisLabel.Get());

				if (n == -1)
					ldata_s = thisLabel;
				else
					t.list_s[n] = thisLabel;
			}
		}
	}
	return t.list_s[current_selection].Get();
}

char* imgui_setpropertyfile2_v2(int group, char* data_s, char* field_s, char* desc_s, char* within_s,bool readonly, char *startsearch)
{
	char *cRet;
	cstr ldata_s = data_s, ldesc_s = desc_s, lfields_s = field_s, lwithin_s = within_s;

	if (cstr(data_s) == "" || !data_s)  ldata_s = "";
	if (cstr(desc_s) == "" || !desc_s)  ldesc_s = "";
	if (cstr(field_s) == "" || !field_s)  lfields_s = "";
	if (cstr(within_s) == "" || !within_s)  lwithin_s = "";

	std::string uniquiField = "";
	uniquiField = uniquiField + "##" + lfields_s.Get();
	uniquiField = uniquiField + std::to_string(grideleprof_uniqui_id++);

	if (lfields_s != "") 
	{
		ImGui::TextCenter(lfields_s.Get());
	}
	strcpy(cTmpInput, ldata_s.Get());

	bool bSoundSet = false;
	if (pestrcasestr(lfields_s.Get(), "soundset") || lfields_s == "Type") 
	{
		bSoundSet = true;
	}
	if (bSoundSet && t.entityprofile[t.gridentity].ischaracter > 0) 
	{
		//Only displayt Male,FeMale selection.
		ImGui::PushItemWidth(-10);
		const char* items[] = { "Male", "Female" };
		int item_current_type_selection = 0; //Default Custom.
		if (pestrcasestr(cTmpInput, "Female")) 
		{
			item_current_type_selection = 1;
		}
		if (ImGui::Combo(uniquiField.c_str(), &item_current_type_selection, items, IM_ARRAYSIZE(items))) 
		{
			strcpy(cTmpInput, items[item_current_type_selection]);
		}
		if (ImGui::IsItemHovered() && ldesc_s != "") ImGui::SetTooltip("%s", ldesc_s.Get());
		ImGui::PopItemWidth();
		return &cTmpInput[0];
	}

	if(!readonly)
		ImGui::PushItemWidth(-10 - (ImGui::GetFontSize()*2.0)); //-6 padding.
	else
		ImGui::PushItemWidth(-10);

	ImGui::InputText(uniquiField.c_str(), &cTmpInput[0], MAXTEXTINPUT);
	if (!pref.iTurnOffEditboxTooltip && ImGui::IsItemHovered() && ldesc_s != "") ImGui::SetTooltip("%s", ldesc_s.Get());
	if (ImGui::MaxIsItemFocused()) bImGuiGotFocus = true;

	ImGui::PopItemWidth();

	if (!readonly)
	{
		ImGui::SameLine();

		uniquiField = "...";
		uniquiField = uniquiField + "##";
		uniquiField = uniquiField + std::to_string(grideleprof_uniqui_id++);

		ImGui::PushItemWidth(ImGui::GetFontSize()*2.0);

		bool bAudio = false;
		bool bImage = false;
		bool bVideo = false;
		bool bScript = false;
		bool bParticle = false;
		bool bAnimation = false;
		bool bUseNewSelectionWindow = false;

		#ifdef USENEWMEDIASELECTWINDOWS
		if (pestrcasestr(lwithin_s.Get(), "charactercreatorplus\\animations"))
		{
			bUseNewSelectionWindow = true;
			bAnimation = true;
		}
		if (pestrcasestr(lwithin_s.Get(), "audiobank"))
		{
			bUseNewSelectionWindow = true;
			bAudio = true;
		}
		if (pestrcasestr(lwithin_s.Get(), "\\imagesinzone"))
		{
			bUseNewSelectionWindow = true;
			bImage = true;
		}
		if (pestrcasestr(lwithin_s.Get(), "imagebank"))
		{
			bUseNewSelectionWindow = true;
			bImage = true;
		}
		if (pestrcasestr(lwithin_s.Get(), "videobank"))
		{
			bUseNewSelectionWindow = true;
			bVideo = true;
		}
		if (pestrcasestr(lwithin_s.Get(), "scriptbank"))
		{
			bUseNewSelectionWindow = true;
			bScript = true;
		}
		if (pestrcasestr(lwithin_s.Get(), "particlesbank"))
		{
			bUseNewSelectionWindow = true;
			bParticle = true;
		}
		#endif

		if (bUseNewSelectionWindow)
		{
			ImGuiWindow* window = ImGui::GetCurrentWindow();
			if (ImGui::StyleButton(uniquiField.c_str(), ImVec2(ImGui::GetFontSize()*1.48, 0)) || iSelectedLibraryStingReturnID == window->GetID(uniquiField.c_str()))
			{
				cStr tOldDir = GetDir();
				if (iSelectedLibraryStingReturnID == window->GetID(uniquiField.c_str()))
				{
					char * cFileSelected = sSelectedLibrarySting.Get();
					SetDir(tOldDir.Get());
					if (cFileSelected && strlen(cFileSelected) > 0) 
					{
						std::string relative = cFileSelected;
						std::string fullpath = tOldDir.Get();

						std::transform(relative.begin(), relative.end(), relative.begin(), [](unsigned char c) { return ::tolower(c); });
						std::transform(fullpath.begin(), fullpath.end(), fullpath.begin(), [](unsigned char c) { return ::tolower(c); });

						fullpath += "\\";
						if (bSoundSet || pestrcasestr(lwithin_s.Get(), "scriptbank")) 
						{
							if (pestrcasestr(cFileSelected, ".lua"))
								fullpath += "scriptbank\\"; //lwithin_s.Get(); PE: This can change in parent mode 2
						}

						if (pestrcasestr(lwithin_s.Get(), "charactercreatorplus"))
						{
							// animations must pass back as DBOs (not FPEs)
							char pReplaceWithDBO[MAX_PATH];
							strcpy(pReplaceWithDBO, relative.c_str());
							pReplaceWithDBO[strlen(pReplaceWithDBO) - 4] = 0;
							strcat(pReplaceWithDBO, ".dbo");
							relative = pReplaceWithDBO;
						}

						replaceAll(relative, fullpath, "");
						strcpy(cTmpInput, relative.c_str());

						if (bSoundSet) 
						{
							char *found = (char *)pestrcasestr(cTmpInput, "audiobank\\voices\\");
							if (found) 
							{
								found += 17;
								strcpy(cTmpInput, found);
							}
							found = (char *)pestrcasestr(cTmpInput, "\\");
							if (found)
								found[0] = 0;
						}
					}
					iSelectedLibraryStingReturnID = -1; //disable.
					sSelectedLibrarySting = "";
				}
				else
				{
					bExternal_Entities_Window = true;
					iDisplayLibraryType = 0;
					iDisplayLibrarySubType = 0;
					if(bAudio)
						iDisplayLibraryType = 1;
					if (bImage)
						iDisplayLibraryType = 2;
					if(bVideo)
						iDisplayLibraryType = 3;
					if(bScript)
						iDisplayLibraryType = 4;
					if (bParticle)
						iDisplayLibraryType = 5;
					if (bAnimation)
					{
						// uses object library system but subtypes to choose animations only
						iDisplayLibraryType = 0;
						iDisplayLibrarySubType = 1;
					}
					
					if (startsearch)
					{
						sStartLibrarySearchString = startsearch;
					}
					iLibraryStingReturnToID = window->GetID(uniquiField.c_str());
					if (iDisplayLibraryType > 0)
					{
						if (strlen(cTmpInput) > 0)
						{
							sMakeDefaultSelecting = cTmpInput;
							bSelectLibraryViewAll = true;
						}
					}
				}
			}
		}
		else
		{
			if (ImGui::StyleButton(uniquiField.c_str(), ImVec2(ImGui::GetFontSize()*1.48, 0)))
			{
				//PE: filedialogs change dir so.
				cStr tOldDir = GetDir();
				char * cFileSelected;
				cFileSelected = (char *)noc_file_dialog_open(NOC_FILE_DIALOG_OPEN, "All\0*.*\0", lwithin_s.Get(), NULL);

				SetDir(tOldDir.Get());

				if (cFileSelected && strlen(cFileSelected) > 0) 
				{
					std::string relative = cFileSelected;
					std::string fullpath = tOldDir.Get();

					std::transform(relative.begin(), relative.end(), relative.begin(), [](unsigned char c) { return ::tolower(c); });
					std::transform(fullpath.begin(), fullpath.end(), fullpath.begin(), [](unsigned char c) { return ::tolower(c); });

					fullpath += "\\";

					if (bSoundSet || pestrcasestr(lwithin_s.Get(), "scriptbank")) 
					{
						if (pestrcasestr(cFileSelected, ".lua"))
							fullpath += "scriptbank\\"; //lwithin_s.Get(); PE: This can change in parent mode 2
					}

					replaceAll(relative, fullpath, "");
					strcpy(cTmpInput, relative.c_str());

					if (bSoundSet) 
					{
						char *found = (char *)pestrcasestr(cTmpInput, "audiobank\\voices\\");
						if (found) 
						{
							found += 17;
							strcpy(cTmpInput, found);
						}
						found = (char *)pestrcasestr(cTmpInput, "\\");
						if (found)
							found[0] = 0;
					}
				}
			}
		}
		ImGui::PopItemWidth();
	}
	return &cTmpInput[0];
}

char * imgui_setpropertystring2_v2(int group, char* data_s, char* field_s, char* desc_s,bool readonly)
{
	char *cRet;
	cstr ldata_s = data_s, ldesc_s = desc_s, lfields_s = field_s;

	if (cstr(data_s) == "" || !data_s)  ldata_s = "";
	if (cstr(desc_s) == "" || !desc_s)  ldesc_s = "";
	if (cstr(field_s) == "" || !field_s)  lfields_s = "";

	std::string uniquiField = ""; //lfields_s.Get();
	uniquiField = uniquiField + "##" + lfields_s.Get();
	uniquiField = uniquiField + std::to_string(grideleprof_uniqui_id++);

	if (lfields_s != "") {
		ImGui::TextCenter(lfields_s.Get());
	}
	ImGui::PushItemWidth(-10);

	strcpy(cTmpInput, ldata_s.Get());
	int inputFlags = 0;
	if (readonly == true)
	{
		inputFlags != ImGuiInputTextFlags_ReadOnly;
		ImGui::Text(&cTmpInput[0]);
		if (ImGui::IsItemHovered() && ldesc_s != "") ImGui::SetTooltip("%s", ldesc_s.Get());
	}
	else
	{
		if (ImGui::InputText(uniquiField.c_str(), &cTmpInput[0], MAXTEXTINPUT, inputFlags)) {
			bImGuiGotFocus = true;
		}
		if (!pref.iTurnOffEditboxTooltip && ImGui::IsItemHovered() && ldesc_s != "") ImGui::SetTooltip("%s", ldesc_s.Get());
	}
	if (ImGui::MaxIsItemFocused()) bImGuiGotFocus = true;

	ImVec2 cpos = ImGui::GetCursorPos();
	ImGui::SetItemAllowOverlap();
	ImGui::SetCursorPos(ImVec2(cpos.x + ImGui::GetContentRegionAvail().x - 33.0f, cpos.y - (ImGui::GetFontSize()*1.5) - 6.0));
	ImGui::ImgBtn(TOOL_PENCIL, ImVec2(16, 16), ImColor(255, 255, 255, 0));
	if (ImGui::IsItemHovered() && ldesc_s != "") ImGui::SetTooltip("%s", ldesc_s.Get());
	if (ImGui::IsItemFocused()) bImGuiGotFocus = true;
	ImGui::SetCursorPos(cpos);

	ImGui::PopItemWidth();

	return &cTmpInput[0];
}

