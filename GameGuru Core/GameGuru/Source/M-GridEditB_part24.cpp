#include "GGTerrain\GGTrees.h" // 2026-07-28: tree shadow sliders moved into the Shadows panel (include-guarded, safe in fragment)

bool Graphics_Performance_Settings(float fTabColumnWidth, bool bVisualUpdated)
{
	int wflags = ImGuiTreeNodeFlags_None;
	static bool bBoxDebug = false;
	static int iObjects = 0;
	static int iFrustumCulled = 0;
	// "Debug Bounding Box" (dev tools only): draw a yellow world-AABB box around every visible object.
	// Re-issued every frame the panel is up (wiRenderer's debug-box list drains per frame).
	if (bBoxDebug && g_iDevToolsOpen >= 1)
	{
		int DrawOccludedObjects(bool bDebug, bool bBox = false, int* bHiddenObjects = nullptr, int* spot = nullptr, int* point = nullptr);
		DrawOccludedObjects(false, true, nullptr, nullptr, nullptr);
	}

	if (pref.bAutoClosePropertySections && iLastOpenHeader != 16)
		ImGui::SetNextItemOpen(false, ImGuiCond_Always);

	//PE: Optimizing
	if (ImGui::StyleCollapsingHeader("Graphics and Performance", wflags))
	{
		ImGui::Indent(10);
		iLastOpenHeader = 16;

		// graphics options mode
		ImGui::PushItemWidth(-10);
		char* current_gfx_mode = "";
		int iGFXMode = iGFXMode = t.visuals.shaderlevels.entities - 1;
		const char* gfx_mode_combo[] = { "Highest (best for quality)", "Custom (tailored)" , "Low (best for performance)" };
		if (iGFXMode == 0) current_gfx_mode = (char*)gfx_mode_combo[0];
		if (iGFXMode == 1) current_gfx_mode = (char*)gfx_mode_combo[1];
		if (iGFXMode == 2) current_gfx_mode = (char*)gfx_mode_combo[2];
		if (ImGui::Combo("##ComboGFX_mode_combo", &iGFXMode, gfx_mode_combo, IM_ARRAYSIZE(gfx_mode_combo)))
		{
			if (iGFXMode == 0) visuals_shaderlevels_setlevel(1, true);
			if (iGFXMode == 1) visuals_shaderlevels_setlevel(3, true);
			if (iGFXMode == 2) visuals_shaderlevels_setlevel(4, true);
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Choose the ideal graphics mode for this level, or set custom settings");
		ImGui::PopItemWidth();

		// optimizations
		extern bool bEnable30FpsAnimations;
		extern bool g_bDelayedShadows;
		extern bool g_bDelayedShadowsLaptop;
		ImGui::PushItemWidth(-10);
		if (ImGui::Checkbox("Lower Animation & LUA Speed##Animationsculling", &bEnable30FpsAnimations))
		{
			t.gamevisuals.bEnable30FpsAnimations = t.visuals.bEnable30FpsAnimations = bEnable30FpsAnimations;
			g.projectmodified = 1;

		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Enabling Lower Animation Speed will lower the updating of animation & LUA to 30 FPS for increased speed when using many animations.");

		// GGMAX 3.25: Reduction Scale. A SUB-CONTROL of the tick box above (Lee's call), so it
		// greys out when that is off and one low-spec decision stays in one place.
		//
		// What it does that the tick box does not: the 30fps throttle only skips animation
		// EVALUATION on the CPU. The skinning dispatch that turns bone matrices into vertices
		// runs every frame for every skinned mesh in the scene regardless - wiRenderer.cpp loops
		// over scene->meshes with no distance or visibility test at all - which is why the
		// "Skinning and Morph" GPU cost did not move when Lee ticked the box on a level whose
		// only nearby animation was the player's weapon. This slider skips BOTH, and skips the
		// skinning dispatch with them, scaled by how far away the thing is.
		if (bEnable30FpsAnimations)
		{
			int reduction = t.visuals.iAnimReductionScale;
			if (reduction < 1) reduction = 1;
			ImGui::Text("Reduction Scale");
			if (ImGui::SliderInt("##gg_anim_reduction", &reduction, 1, 100, reduction <= 1 ? "Off" : "%d"))
			{
				t.gamevisuals.iAnimReductionScale = t.visuals.iAnimReductionScale = reduction;
				g.projectmodified = 1;
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("How much animation work to skip on things that are far enough away for you not to notice. 1 is no reduction. At 50, something 1000 units away has 5 frames skipped between animation updates, and one 2000 units away has 10 - the skip grows with distance. Nothing closer than 1000 units is ever skipped, so anything you are actually looking at animates at full rate. This also skips the graphics card work that poses the character, which the tick box above does not.\n\nAround 20 to 25 is the sweet spot - past that the saving barely moves while the stutter on mid-distance characters keeps getting worse, and above 50 buys you almost nothing.");
		}
		else
		{
			ImGui::TextDisabled("Reduction Scale");
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Tick Lower Animation & LUA Speed above to use this. It sets how much animation work to skip on distant objects.");
		}

		// ★ GGMAX 3.28: Occlusion Cull Delay.
		//
		// Lee reported Opaque Scene and Z-Prepass ballooning whenever the camera was rotated -
		// 5->40 ms on a 6-year-old AMD card. The cause was that occlusion culling could not engage
		// AT ALL while the camera turned: an object had to read occluded for 32 CONSECUTIVE frames
		// before it would be skipped, and anything sweeping through the frustum never gets that
		// long. Measured on TESTPRO2 at 180 deg/s: 602 draws a frame at 32, 222 at 8.
		int occdelay = t.visuals.iOcclusionCullDelay;
		if (occdelay < 1 || occdelay > 32) occdelay = 8;
		ImGui::Text("Occlusion Cull Delay");
		if (ImGui::SliderInt("##gg_occlusion_delay", &occdelay, 1, 32, "%d frames"))
		{
			t.gamevisuals.iOcclusionCullDelay = t.visuals.iOcclusionCullDelay = occdelay;
			g.projectmodified = 1;
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("How many frames in a row something must be "
			"completely hidden behind something else before the engine stops drawing it.\n\n"
			"Lower is faster. The saving is biggest while you are turning the camera, because a "
			"long delay means anything that has just swung into view gets drawn regardless of "
			"whether a wall is in front of it. On a test level, turning at speed, 32 frames drew "
			"602 objects per frame and 8 frames drew 222 - and standing still it was 51 against "
			"10.\n\n"
			"8 is the default and is a good balance. Raise it if you ever see something vanish "
			"that should be visible; 32 restores the old behaviour, where this optimisation did "
			"nothing whenever the camera moved.");

		// ★ GGMAX 3.29: Shadow Detail Steps.
		//
		// Lee: Shadowmap rendering 6 -> 25 ms on a 6-year-old AMD card whenever he walked forward or
		// back, dropping straight back the moment he stopped. Every local light's shadow rect is
		// sized from its DISTANCE to the camera, and the shadow cache keys on that rect - so one
		// texel of change re-renders every local shadow. Walking changes every distance every frame.
		// Measured on TESTPRO2: moving cost 4.21 ms continuous, 1.84 ms at 4 steps.
		int shsteps = t.visuals.iShadowResSteps;
		if (shsteps < 0 || shsteps > 16) shsteps = 4;
		ImGui::Text("Shadow Detail Steps");
		if (ImGui::SliderInt("##gg_shadow_res_steps", &shsteps, 0, 16,
			shsteps == 0 ? "Off (smooth)" : "%d steps"))
		{
			t.gamevisuals.iShadowResSteps = t.visuals.iShadowResSteps = shsteps;
			g.projectmodified = 1;
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Shadows from lamps and torches get sharper as "
			"you walk towards them. This sets how many steps they take to do it.\n\n"
			"Off means perfectly smooth - and it is the slow option, because the shadow changes size "
			"every single frame you move, and the engine has to redraw every nearby shadow every "
			"frame as a result. Standing still it redraws none of them. That is why shadows can cost "
			"several times more while you are walking than while you are stood still.\n\n"
			"Fewer steps means shadows hold one size for longer, so the engine can reuse them and "
			"only redraws when you cross from one step to the next. On a test level, walking, this "
			"took the shadow cost from 4.2ms to 1.8ms.\n\n"
			"4 is the default. Lower is faster still, but every shadow is then rounded up to a bigger "
			"size than it needs, which can crowd the shadow atlas on levels with a lot of lights and "
			"end up shrinking all of them. Shadows are never made blurrier than the Off setting would "
			"have made them.");
		ImGui::PopItemWidth();

		extern bool bEnableDelayPointShadow;
		extern float pointShadowScaler;

		ImGui::PushItemWidth(-10);
		if (ImGui::Checkbox("Delayed Shadows##Animationsculling", &g_bDelayedShadows))
		{
			t.gamevisuals.g_bDelayedShadows = t.visuals.g_bDelayedShadows = g_bDelayedShadows;
			g.projectmodified = 1;
			// DX12: actually drive the engine's directional cascade staggering (delta 1.11). OFF =
			// every-frame cascades = no shadow-lag flicker under camera movement. The point-shadow
			// booleans below are the legacy DX11 path and do NOT touch the directional cascades.
			wiRenderer::SetDelayedShadowCascadesEnabled(g_bDelayedShadows);
			// Also the shadow LOD override (terrain casts shadows at a threshold-oscillating per-cascade
			// LOD -> "two terrain shapes" flicker). OFF = shadows use the stable main-view LOD.
			wiRenderer::SetShadowLODOverrideEnabled(g_bDelayedShadows);
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
		}
		// ★ GGMAX 3.32: the measured numbers, in the tooltip. This is the biggest single saving found
		// in the DX11-parity round, and it was sitting behind a one-line description that gave nobody a
		// reason to try it.
		//
		// ⚠ GGMAX 3.37: the DX11 claim above was WRONG and has been removed from the tooltip. It read
		// "DX11 shipped this feature ON and DX12 ships it OFF", which is true only of the CONSTRUCTOR
		// defaults (Types.h, and the two M-Visuals reset sites: DX11 true, DX12 false). It is NOT true
		// of what a user actually gets, because the quality-preset ladder overrides those defaults and
		// the two ladders are IDENTICAL: HIGHEST forces false and LOW forces true in BOTH renderers
		// (DX12 M-Visuals_part1.cpp:654/678, DX11 M-Visuals.cpp:2451/2480). The default preset is
		// HIGHEST (M-Visuals_part0.cpp:30), so DX11 users on the default had this OFF too - the four
		// divergent sites only survive for CUSTOM projects, where the ladder's "do not override"
		// branch runs. Verified against the read-only DX11 tree, 2026-09-14.
		//
		// ★ The saving itself is real and measured; only the DX11 provenance was overstated. Do not
		// re-open this as a "flip the default to match DX11" task - see NIGHT_INVESTIGATIONS §3.37.
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Spreads the sun's shadow updates over several "
			"frames instead of redrawing all of them every frame.\n\n"
			"The sun casts five shadow layers, from the ground at your feet out to the horizon. Redrawing "
			"all five every frame is most of what sun shadows cost, and the distant ones barely change "
			"from frame to frame. With this on, the nearest layer keeps updating every frame and the "
			"further ones take turns.\n\n"
			"Measured on a test level: sun shadow time dropped from 2.15ms to 0.89ms, and total graphics "
			"card time for the whole frame fell by a quarter. It is the single biggest saving on this "
			"panel.\n\n"
			"The trade is that shadows in the distance can lag very slightly behind a fast-moving sun or "
			"camera. Turn it off again if you notice that. Worth trying first on any slower machine.\n\n"
			"Note: the graphics mode above sets this for you - Highest turns it off, Low turns it on, "
			"and Custom leaves it exactly as you set it here. Your choice is saved with the level.");
		if (g_bDelayedShadows)
		{
			ImGui::SameLine();
			if (ImGui::Checkbox("Laptop##Animationsculling", &g_bDelayedShadowsLaptop))
			{
				t.gamevisuals.g_bDelayedShadowsLaptop = t.visuals.g_bDelayedShadowsLaptop = g_bDelayedShadowsLaptop;
				g.projectmodified = 1;
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
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Laptop mode: twice as aggressive as Delayed Shadows - the distant shadow cascades refresh every 4th frame instead of every 2nd, for extra FPS on weaker hardware (a little more shadow lag when the camera moves).");
		}
		ImGui::PopItemWidth();

		extern bool bEnableObjectCulling;
		// "Debug Bounding Box" (dev tools only) sits on its own line ABOVE Occlusion Culling, so the
		// Occlusion Culling checkbox + the sub-tickboxes it reveals below read as one coherent group.
		if (g_iDevToolsOpen >= 1)
		{
			ImGui::Checkbox("Debug Bounding Box", &bBoxDebug);
		}
		ImGui::PushItemWidth(-10);
		if (ImGui::Checkbox("Occlusion Culling##bOcclusionCulling", &t.visuals.bOcclusionCulling))
		{
			t.gamevisuals.bOcclusionCulling = t.visuals.bOcclusionCulling;
			g.projectmodified = 1;
			if (t.visuals.bOcclusionCulling)
			{
				bEnableObjectCulling = true;
				t.gamevisuals.bEnableObjectCulling = t.visuals.bEnableObjectCulling = bEnableObjectCulling;
			}
		}
		if (wiRenderer::GetOcclusionCullingEnabled() != t.visuals.bOcclusionCulling)
		{
			wiRenderer::SetOcclusionCullingEnabled(t.visuals.bOcclusionCulling);
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Enabling Occlusion Culling will cull objects behind other object for less drawcalls");
		ImGui::PopItemWidth();

		if (t.visuals.bOcclusionCulling)
		{
			// GGMAX DX12: the five per-category cull checkboxes below are DEAD — the Wicked engine now does
			// this work natively so none of these globals has a live consumer:
			//   * Object + frustum culling run every frame in UpdateVisibility (that's what the
			//     "Frustum/Apparent Culled" readout counts).
			//   * Animation culling is always-on inside RunAnimationUpdateSystem.
			//   * Terrain chunks are culled by the native SVT terrain.
			//   * Point/Spot shadow management moved to the local shadow-budget (top-N caster) system.
			// The PARENT "Occlusion Culling" (engine GPU occlusion queries) is still wired and stays visible;
			// it does useful work on GPU-bound / occluder-dense levels. Commented out 2026-07-24 — trivially
			// re-enable a category here if it ever gets a real DX12 consumer.
#if 0
			extern bool bEnableTerrainChunkCulling;
			extern bool bEnablePointShadowCulling;
			extern bool bEnableSpotShadowCulling;
			extern bool bEnableAnimationCulling;

			if (ImGui::Checkbox("Terrain Chunk Culling", &bEnableTerrainChunkCulling))
			{
				t.gamevisuals.bEnableTerrainChunkCulling = t.visuals.bEnableTerrainChunkCulling = bEnableTerrainChunkCulling;
				g.projectmodified = 1;
				if (bEnableTerrainChunkCulling && !t.visuals.bOcclusionCulling)
				{
					t.gamevisuals.bOcclusionCulling = t.visuals.bOcclusionCulling = true;
				}
			}
			if (ImGui::Checkbox("Point Shadow Culling", &bEnablePointShadowCulling))
			{
				t.gamevisuals.bEnablePointShadowCulling = t.visuals.bEnablePointShadowCulling = bEnablePointShadowCulling;
				g.projectmodified = 1;
				if (bEnablePointShadowCulling && !t.visuals.bOcclusionCulling)
				{
					t.gamevisuals.bOcclusionCulling = t.visuals.bOcclusionCulling = true;
				}
			}

			if (ImGui::Checkbox("Spot Shadow Culling", &bEnableSpotShadowCulling))
			{
				t.gamevisuals.bEnableSpotShadowCulling = t.visuals.bEnableSpotShadowCulling = bEnableSpotShadowCulling;
				g.projectmodified = 1;
				if (bEnableSpotShadowCulling && !t.visuals.bOcclusionCulling)
				{
					t.gamevisuals.bOcclusionCulling = t.visuals.bOcclusionCulling = true;
				}
			}

			if (ImGui::Checkbox("Object Culling", &bEnableObjectCulling))
			{
				t.gamevisuals.bEnableObjectCulling = t.visuals.bEnableObjectCulling = bEnableObjectCulling;
				g.projectmodified = 1;
				if (bEnableObjectCulling && !t.visuals.bOcclusionCulling)
				{
					t.gamevisuals.bOcclusionCulling = t.visuals.bOcclusionCulling = true;
				}
			}

			if (ImGui::Checkbox("Animation Culling", &bEnableAnimationCulling))
			{
				t.gamevisuals.bEnableAnimationCulling = t.visuals.bEnableAnimationCulling = bEnableAnimationCulling;
				g.projectmodified = 1;
				if (bEnableAnimationCulling && !t.visuals.bOcclusionCulling)
				{
					t.gamevisuals.bOcclusionCulling = t.visuals.bOcclusionCulling = true;
				}
			}
#endif

			wiScene::Scene* pScene = &wiScene::GetScene();
			if (pScene)
			{
				iObjects = pScene->objects.GetCount();
				// DX12 fix: was hardcoded 0. GameGuru's legacy CPU frustum cull is gone; the live
				// terrain/object path is Wicked, which frustum-culls into visibility_main.visibleObjects.
				// Frustum-culled = total scene objects - objects that survived the main-camera frustum.
				int iVisible = WickedCall_GetFrustumVisibleObjects();
				iFrustumCulled = iObjects - iVisible;
				if (iFrustumCulled < 0) iFrustumCulled = 0;
			}
			ImGui::Text("Total Objects: %d", iObjects);
			ImGui::Text("Frustum/Apparent Culled: %d", iFrustumCulled);

			// DX12 fix: the old iCulled*/iRendered* shadow counters were never written (always 0, and the
			// "occluded shadows" concept is DX11-legacy). Show the REAL local-shadow data instead — per-type
			// shadow-casting light counts + the local shadow-budget (top-N caster) stats. Reads 0 on levels
			// with no point/spot lights (e.g. the sun-only island); non-zero on torch/lamp-lit levels.
			int shGranted = 0, shCapped = 0, shRendered = 0;
			wiRenderer::GetLocalShadowStats(shGranted, shCapped, shRendered);
			ImGui::Text("Point Shadow Lights: %d   Spot Shadow Lights: %d", WickedCall_GetCubeShadowLights(false), WickedCall_GetSpotShadowLights(false));
			ImGui::Text("Local Shadows: %d granted, %d capped, %d rendered", shGranted, shCapped, shRendered);
		}

		extern float maxApparentSize;
		ImGui::PushItemWidth(-10);
		float fASize = t.visuals.ApparentSize * 10000.0f;
		tab_tab_Column_text("Apparent Size", fTabColumnWidth);
		if (ImGui::SliderFloat("##maxApparentSize", &fASize, 0.02f, 2.0f, "%.2f", 1.0f))
		{
			maxApparentSize = fASize / 10000.0f;
			t.gamevisuals.ApparentSize = t.visuals.ApparentSize = maxApparentSize;
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Max Object Apparent Size will cull objects when they get smaller on screen");
		ImGui::PopItemWidth();


		// LOD Multiplier: the engine's LOD distance step (wi::scene::gg_lod_step, pushed in MasterRenderer::Update),
		// the DX11 rule of 200 units a step. Hidden 2026-07-25 as unconsumed, wired 2026-10-19.
		extern float fLODMultiplier;
		//ImGui::Text("LOD Multiplier");
		tab_tab_Column_text("LOD Multiplier", fTabColumnWidth);
		ImGui::PushItemWidth(-10);
		if (ImGui::SliderFloat("##fLODMultiplier", &fLODMultiplier, 0.0f, 15.0f, "%.2f", 1.0f))
		{
			if (fLODMultiplier < 0)
				fLODMultiplier = 0;
			t.gamevisuals.fLODMultiplier = t.visuals.fLODMultiplier = fLODMultiplier;
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Change LOD distance before switching from normal object to LOD");
		ImGui::PopItemWidth();

		extern int g_iUseLODObjects;
		extern bool bDisableLODLoad;
		// Disable LOD Load — KEPT: genuinely WIRED in DX12. Drives GameGuru's own LOD path (loads the external
		// *_lod.dbo substitute model at entity load — M-Entity_part0.cpp:1095 — and gates in-model LOD-frame
		// selection — M-Entity_part5.cpp:1296). This is GG's LOD system, independent of Wicked's screen-size LOD.
		ImGui::Checkbox("Disable LOD Load", &bDisableLODLoad);
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Forces full-detail models by ignoring pre-baked _lod files (debug use, not a performance gain - applies to newly loaded entities only)");
		(void)g_iUseLODObjects;

		// Reflections Use Fastest LOD: wired 2026-10-19 (wi::renderer::gg_reflection_lowest_lod, read by DrawScene for the
		// planar reflection passes).
		if (g_iUseLODObjects > 0 && !bDisableLODLoad)
		{
			extern bool bReflectionsLowestLOD;
			ImGui::PushItemWidth(-10);
			if (ImGui::Checkbox("Reflections Use Fastest LOD##Animationsculling", &bReflectionsLowestLOD))
			{
				t.gamevisuals.bReflectionsLowestLOD = t.visuals.bReflectionsLowestLOD = bReflectionsLowestLOD;
				g.projectmodified = 1;
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("All reflection rendering is using fastest LOD.");
			ImGui::PopItemWidth();
		}

		// The other "* Use Fastest LOD" children — HIDDEN 2026-07-25: DEAD in DX12. bShadowsLowestLOD / bProbesLowestLOD /
		// bRaycastLowestLOD / bPhysicsLowestLOD are stored + saved but have ZERO consumers
		// (verified by full-tree trace; the DX11 consumers lived in the closed engine DLL). Wicked does shadow/
		// probe/physics LOD its own way, so these per-category "force lowest LOD" overrides don't map.
		// The parent "Disable LOD Load" above stays (it's wired). Re-add a child only when a real DX12 consumer exists.
#if 0
		if (g_iUseLODObjects > 0 && !bDisableLODLoad)
		{
			extern bool bShadowsLowestLOD;
			ImGui::PushItemWidth(-10);
			if (ImGui::Checkbox("Shadows Use Fastest LOD##Animationsculling", &bShadowsLowestLOD))
			{
				t.gamevisuals.bShadowsLowestLOD = t.visuals.bShadowsLowestLOD = bShadowsLowestLOD;
				g.projectmodified = 1;

			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("All shadows will use fastest LOD available to render.");
			ImGui::PopItemWidth();

			extern bool bProbesLowestLOD;
			ImGui::PushItemWidth(-10);
			if (ImGui::Checkbox("Probes Use Fastest LOD##Animationsculling", &bProbesLowestLOD))
			{
				t.gamevisuals.bProbesLowestLOD = t.visuals.bProbesLowestLOD = bProbesLowestLOD;
				g.projectmodified = 1;

			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("All probes will use fastest LOD available to render.");
			ImGui::PopItemWidth();

			extern bool bRaycastLowestLOD;
			ImGui::PushItemWidth(-10);
			if (ImGui::Checkbox("Raycast Use Fastest LOD##Animationsculling", &bRaycastLowestLOD))
			{
				t.gamevisuals.bRaycastLowestLOD = t.visuals.bRaycastLowestLOD = bRaycastLowestLOD;
				g.projectmodified = 1;
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("All raycast will use fastest LOD available for intersect checks.");
			ImGui::PopItemWidth();


			extern bool bPhysicsLowestLOD;
			ImGui::PushItemWidth(-10);
			if (ImGui::Checkbox("Physics Use Fastest LOD##Animationsculling", &bPhysicsLowestLOD))
			{
				t.gamevisuals.bPhysicsLowestLOD = t.visuals.bPhysicsLowestLOD = bPhysicsLowestLOD;
				g.projectmodified = 1;
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("All physics objects is created using fastest LOD.");
			ImGui::PopItemWidth();

			extern bool bReflectionsLowestLOD;
			ImGui::PushItemWidth(-10);
			if (ImGui::Checkbox("Reflections Use Fastest LOD##Animationsculling", &bReflectionsLowestLOD))
			{
				t.gamevisuals.bReflectionsLowestLOD = t.visuals.bReflectionsLowestLOD = bReflectionsLowestLOD;
				g.projectmodified = 1;
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("All reflection rendering is using fastest LOD.");
			ImGui::PopItemWidth();
		}
#endif

		// FSR Mode (FidelityFX Super Resolution)
		const char* fsr_items_align[] = { "None", "Ultra Quality","Quality", "Balanced", "Performance" };
		int fsr_current_type_selection = t.visuals.iFSRMode;
		tab_tab_Column_text("FSR", fTabColumnWidth);
		ImGui::PushItemWidth(-10);
		if (ImGui::Combo("##setiFSRMode", &fsr_current_type_selection, fsr_items_align, IM_ARRAYSIZE(fsr_items_align)))
		{
			t.visuals.iFSRMode = fsr_current_type_selection;
			t.gamevisuals.iFSRMode = t.visuals.iFSRMode;
			if (t.visuals.iFSRMode == 1)
			{
				// TODO: Set3DResolution removed, use resolutionScale instead
				//master.masterrenderer.Set3DResolution(master.masterrenderer.GetPhysicalWidth(), master.masterrenderer.GetPhysicalHeight(), false);
				// GGREDUCED DX12: FSR1 renders the 3D scene at 1/factor internal res; the engine EASU+RCAS
				// pass upscales rtFSR to display res. resolutionScale<1.0 is what actually arms Postprocess_FSR
				// (ResizeBuffers re-invokes setFSREnabled after recomputing internal res).
				master.masterrenderer.resolutionScale = 1.0f / 1.3f; // Ultra Quality
				master.masterrenderer.setFSREnabled(true);
				master.masterrenderer.ResizeBuffers(); //PE: Force resizebuffers.
				master.masterrenderer.setFSRSharpness(t.visuals.fFSRSharpness);
				t.gamevisuals.bFXAAEnabled = t.visuals.bFXAAEnabled = true; //PE: need FXAA or FSR dont work.
				if (master_renderer)
					master_renderer->setFXAAEnabled(t.visuals.bFXAAEnabled);
			}
			else if (t.visuals.iFSRMode == 2)
			{
				// TODO: Set3DResolution removed, use resolutionScale instead
				//master.masterrenderer.Set3DResolution(master.masterrenderer.GetPhysicalWidth(), master.masterrenderer.GetPhysicalHeight(), false);
				// GGREDUCED DX12: FSR1 internal res = 1/1.5 native; engine EASU+RCAS upscales.
				master.masterrenderer.resolutionScale = 1.0f / 1.5f; // Quality
				master.masterrenderer.setFSREnabled(true);
				master.masterrenderer.ResizeBuffers(); //PE: Force resizebuffers.
				master.masterrenderer.setFSRSharpness(t.visuals.fFSRSharpness);
				t.gamevisuals.bFXAAEnabled = t.visuals.bFXAAEnabled = true; //PE: need FXAA or FSR dont work.
				if (master_renderer)
					master_renderer->setFXAAEnabled(t.visuals.bFXAAEnabled);
			}
			else if (t.visuals.iFSRMode == 3)
			{
				// TODO: Set3DResolution removed, use resolutionScale instead
				//master.masterrenderer.Set3DResolution(master.masterrenderer.GetPhysicalWidth(), master.masterrenderer.GetPhysicalHeight(), false);
				// GGREDUCED DX12: FSR1 internal res = 1/1.7 native; engine EASU+RCAS upscales.
				master.masterrenderer.resolutionScale = 1.0f / 1.7f; // Balanced
				master.masterrenderer.setFSREnabled(true);
				master.masterrenderer.ResizeBuffers(); //PE: Force resizebuffers.
				master.masterrenderer.setFSRSharpness(t.visuals.fFSRSharpness);
				t.gamevisuals.bFXAAEnabled = t.visuals.bFXAAEnabled = true; //PE: need FXAA or FSR dont work.
				if (master_renderer)
					master_renderer->setFXAAEnabled(t.visuals.bFXAAEnabled);
			}
			else if (t.visuals.iFSRMode == 4)
			{
				// TODO: Set3DResolution removed, use resolutionScale instead
				//master.masterrenderer.Set3DResolution(master.masterrenderer.GetPhysicalWidth(), master.masterrenderer.GetPhysicalHeight(), false);
				// GGREDUCED DX12: FSR1 internal res = 1/2.0 native; engine EASU+RCAS upscales.
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
				// GGREDUCED DX12: restore native internal resolution and tear down FSR.
				master.masterrenderer.resolutionScale = 1.0f;
				master.masterrenderer.setFSREnabled(false);
				master.masterrenderer.ResizeBuffers(); //PE: Force resizebuffers.
			}

			//PE: change.
			g.projectmodified = 1;
		}

		ImGui::PopItemWidth();

		if (master.masterrenderer.getFSREnabled())
		{
			ImGui::Text("FSR Sharpness");
			ImGui::PushItemWidth(-10);
			if (ImGui::SliderFloat("##fFSRSharpness", &t.visuals.fFSRSharpness, 0.0f, 2.0f, "%.2f", 1.0f))
			{
				if (t.visuals.fFSRSharpness < 0)
					t.visuals.fFSRSharpness = 0;
				t.gamevisuals.fFSRSharpness = t.visuals.fFSRSharpness;
				master.masterrenderer.setFSRSharpness(t.visuals.fFSRSharpness);
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Change FSR Sharpness");
			ImGui::PopItemWidth();
		}

		// GGMAX: the 4GB preset as a per-level setting (saved in the FPM). The effective state
		// is OR'd with the machine-wide setup.ini `lowvram` key — either switch turns it on.
		ImGui::PushItemWidth(-10);
		if (ImGui::Checkbox("Low VRAM Mode (4GB cards)##bLowVRAM", &t.visuals.bLowVRAM))
		{
			t.gamevisuals.bLowVRAM = t.visuals.bLowVRAM;
			g.projectmodified = 1;
			extern void GGSetLowVRAMLevel(int);
			GGSetLowVRAMLevel(t.visuals.bLowVRAM ? 1 : 0);
			extern void GGApplyVisualsNow();
			GGApplyVisualsNow(); // SSR + shadow-cap members apply immediately; grass on reload
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Trades visual reach for video memory so this level fits a 4GB graphics card: caps grass draw distance at 750, thins grass density to 75%%, caps shadow cascade resolution at 1024 and turns off Screen Space Reflections. All of it applies immediately - the grass rebuilds over a second or two, nearest first. Players with small cards can also force this on for ALL levels with lowvram=1 in setup.ini.");
		ImGui::PopItemWidth();

		// ====================================================================================
		// GGMAX 2.94: BRUTAL OFF-SWITCHES (Phase 2 performance work).
		//
		// These are not "stop drawing it" toggles. Each one removes the subsystem's elements
		// from the scene entirely, so they stop costing culling, Scene::Update transform/AABB
		// work, shadow casting, virtual-texture feedback and video memory. A renderable=false
		// hide leaves every one of those costs behind.
		//
		// SESSION-SCOPED on purpose: the state is a global, not a per-level FPM field, so it
		// survives level loads within a session and never writes anything into the user's
		// levels. Players on weak cards can set the same four switches machine-wide in
		// setup.ini (noterrain / notrees / nograss / nowater); either source turns it off.
		// ====================================================================================
		{
			extern bool gg_no_terrain, gg_no_trees, gg_no_grass, gg_no_water;
			// GGMAX 3.25: Terrain Off and Water Off became Terrain Bake and Water Bake. Both
			// still perform the same removal underneath - that is where the saving is - but
			// each now leaves a cheap stand-in behind instead of a hole in the world.
			extern bool gg_terrain_bake;
			extern bool gg_water_bake;
			extern void GGSetNoTerrainLevel(int);
			extern void GGSetNoTreesLevel(int);
			extern void GGSetNoGrassLevel(int);
			extern void GGSetNoWaterLevel(int);
			extern void GGApplyVisualsNow();

			ImGui::Separator();
			ImGui::TextDisabled("Brutal off-switches (weak GPU)");

			bool bOff;
			ImGui::PushItemWidth(-10);

			bOff = gg_terrain_bake;
			if (ImGui::Checkbox("Terrain Bake##gg_terrain_bake", &bOff))
			{
				gg_terrain_bake = bOff;
				t.gamevisuals.bTerrainBake = t.visuals.bTerrainBake = bOff;
				g.projectmodified = 1;
			}
			// ⚠ GGMAX 3.34: this tooltip belongs to the Terrain Bake checkbox above and has to stay
			// DIRECTLY under it. 3.31 inserted the Super Quick block between the two, so IsItemHovered
			// resolved to the Super Quick checkbox instead - Terrain Bake lost its tooltip and Super
			// Quick showed the terrain one, overwriting its own. Lee's AMD-card screenshot caught it.
			// Any new control added here goes AFTER this line, never between a widget and its tooltip.
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Converts the terrain into ordinary meshes and ordinary textures, then removes the live terrain system entirely - the chunk entities, the virtual-texture atlas and the per-frame page streaming that goes with them. You keep a terrain you can see and walk on, drawn by a plain pass that costs a fraction of the real one. Surfaces get softer close up and terrain editing is paused while it is on. Untick to bring the real terrain straight back. This is the biggest single saving on an open outdoor level.");

			{
				// ★★★ GGMAX 3.34: Super Quick Objects - now a real cut-down opaque shader.
				//
				// 3.31 only collapsed exotic material permutations onto base PBR, which on a scene
				// whose materials are already PBR does nothing whatsoever - and that is exactly what
				// Lee measured on the AMD card. The box now selects one of three genuinely cut-down
				// opaque pixel shaders (objectHF.hlsli, GG_SQ_FLAT / GG_SQ_AMBIENT / GG_SQ_LIT).
				bool bSQ = t.visuals.bSuperQuickObjects;
				if (ImGui::Checkbox("Super Quick Objects##gg_super_quick", &bSQ))
				{
					t.gamevisuals.bSuperQuickObjects = t.visuals.bSuperQuickObjects = bSQ;
					g.projectmodified = 1;
				}
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("Strips the object shader back to almost "
					"nothing, so the graphics card spends its time putting shapes on screen instead of "
					"working out what every pixel of them should look like.\n\n"
					"Normally each pixel of every object reads a colour texture, a bump texture, a "
					"roughness texture, a glow texture and a shine texture, then adds decals, ambient "
					"shadowing, reflections and every nearby light with its own shadow. This drops all of "
					"it and lets you add it back one step at a time with the slider underneath.\n\n"
					"Your level will look plain - that is the point. Use it to find out how much of your "
					"frame time is really object detail, then climb back up until it costs too much "
					"again.\n\n"
					"Watch the Opaque Objects line in the performance panel as you change the slider. "
					"Terrain, trees, grass and water are drawn separately and are not affected.");

				// The rung. Only meaningful while the box is ticked, so only shown then - a slider that
				// visibly does nothing is the 3.19 Texture Detail mistake, and it cost a bug report.
				if (t.visuals.bSuperQuickObjects)
				{
					int sql = t.visuals.iSuperQuickLevel;
					if (sql < 1 || sql > 3) sql = 3;   // a level saved before 3.34 parses as 0 (3.35b: default 3)
					const char* sqfmt =
						(sql <= 1) ? "1: Shapes only (fastest)" :
						(sql == 2) ? "2: Shapes and colour" :
						             "3: Shapes, colour and light";
					if (ImGui::SliderInt("##gg_super_quick_level", &sql, 1, 3, sqfmt))
					{
						t.gamevisuals.iSuperQuickLevel = t.visuals.iSuperQuickLevel = sql;
						g.projectmodified = 1;
					}
					if (ImGui::IsItemHovered()) ImGui::SetTooltip("How much detail to put back.\n\n"
						"1 - Shapes only. No textures at all. Every object is a flat block of its own "
						"colour, lit only by the sky and the sun direction. This is as fast as the objects "
						"in your level can possibly be drawn.\n\n"
						"2 - Shapes and colour. Adds the colour texture back, plus distance fog. Things "
						"become recognisable again but stay unlit and flat.\n\n"
						"3 - Shapes, colour and light. Adds lamps, torches and their shadows. Still no bump "
						"or shine detail, no decals, no ambient shadowing, no reflections.\n\n"
						"The useful thing is the jump between two settings. If going from 1 to 2 costs you "
						"a lot, your textures are the problem. If 2 to 3 costs you a lot, it is your "
						"lights. If nothing changes much, the cost is not the objects at all.");
				}
			}

			// GGMAX 3.25l: near-tier detail, a sub-control of Terrain Bake. Distant chunks are
			// always baked small (they are never seen close up), so this dial only sets how sharp
			// the ground is where the level's objects are - which is where the player walks and
			// the only place texel density is noticeable.
			//
			// ★ It REBUILDS IMMEDIATELY when changed while the bake is on. A setting that only
			// took effect "next time" would be indistinguishable from a broken one - the exact
			// complaint Texture Detail earned in 3.12, and why 3.19 had to make that one live too.
			{
				extern int gg_terrain_bake_res_near;
				extern void GGTerrainBake_Clear();
				static const int kRes[6] = { 256, 512, 1024, 2048, 4096, 8192 };
				int idx = 5;
				for (int i = 0; i < 6; i++) if (kRes[i] == t.visuals.iTerrainBakeResNear) { idx = i; break; }
				char lbl[64];
				sprintf_s(lbl, sizeof(lbl), "%d x %d", kRes[idx], kRes[idx]);
				ImGui::Text("Terrain Bake Detail (under your feet)");
				if (ImGui::SliderInt("##gg_terrain_bake_res_near", &idx, 0, 5, lbl))
				{
					if (idx < 0) idx = 0;
					if (idx > 5) idx = 5;
					t.gamevisuals.iTerrainBakeResNear = t.visuals.iTerrainBakeResNear = kRes[idx];
					gg_terrain_bake_res_near = kRes[idx];
					g.projectmodified = 1;
					// Live: drop the baked set so the state machine rebuilds at the new detail.
					// Clear() puts the real terrain back first, so there is no hole in between.
					if (gg_terrain_bake) GGTerrainBake_Clear();
				}
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("How sharp the baked ground is in the part of the level where your objects are - the area the player actually walks around in. Distant terrain is always baked small because you never see it close up, so this only costs memory for the ground near your content. 8192 is about half a world unit per pixel and looks like the real terrain underfoot; 1024 is softer but frees a lot of memory on a small card. Frame rate is barely affected either way - this setting spends video memory, not speed. Changing it rebuilds the baked terrain straight away, which takes a moment.");
			}

			bOff = gg_no_trees;
			if (ImGui::Checkbox("Trees Off##gg_no_trees", &bOff))
			{
				GGSetNoTreesLevel(bOff ? 1 : 0);
				t.gamevisuals.bNoTrees = t.visuals.bNoTrees = bOff;
				g.projectmodified = 1;
				// GGMAX 3.00: re-apply visuals so ggtrees_global_params.draw_enabled is
				// recomputed now. Without this the legacy tree machinery only noticed the
				// switch when some OTHER control happened to re-apply visuals.
				GGApplyVisualsNow();
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Releases the whole tree pool, its shadow proxies and the per-type LOD models. Tree collision is switched off with them, so you will not bump into trees you cannot see. Your placed trees are not touched and come back when you untick this.");

			bOff = gg_no_grass;
			if (ImGui::Checkbox("Grass Off##gg_no_grass", &bOff))
			{
				GGSetNoGrassLevel(bOff ? 1 : 0);
				t.gamevisuals.bNoGrass = t.visuals.bNoGrass = bOff;
				g.projectmodified = 1;
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Removes every blade of grass from the scene, including the per-strand simulation that runs on the graphics card each frame. Your painted grass is kept and reappears when you untick this.");

			// GGMAX 3.116: Grass Density for the whole level. Shows the LIVE level value (as Particle Density
			// shows its engine value), so a Test Game round trip cannot desync the control. The grass follows
			// a moment after the slider stops (GGTerrainWicked.cpp density latch), nearest chunks first.
			{
				extern int GGGetGrassDensityLevel(); extern void GGSetGrassDensityLevel(int);
				extern float GGGrass_DX11DensityPerM2(); extern float GGGrass_NearDensityPerM2();
				int gd = GGGetGrassDensityLevel();
				ImGui::Text("Grass Density (whole level)");
				if (ImGui::SliderInt("##gg_grass_density", &gd, 10, 200, "%d%%"))
				{
					GGSetGrassDensityLevel(gd);
					t.gamevisuals.iGrassDensityPct = t.visuals.iGrassDensityPct = GGGetGrassDensityLevel();
					g.projectmodified = 1;
				}
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("How thickly grass grows on painted ground, for the whole level. 100%% matches the classic (DX11) GameGuru MAX density for this level's Grass Draw Distance - as there, a longer draw distance spreads the grass thinner. Above 100%% is denser, below is sparser; grass video memory and drawing cost go up and down with it. The grass updates a moment after you let go. Your painted grass is not changed.");
				// wrapped: the panel is narrower than the sentence (it clipped at "(classi" unwrapped)
				ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
				ImGui::TextWrapped("Near you: %.1f per square metre. Classic GameGuru MAX at this draw distance: %.1f.",
					GGGrass_NearDensityPerM2(), GGGrass_DX11DensityPerM2());
				ImGui::PopStyleColor();
			}

			bOff = gg_water_bake;
			if (ImGui::Checkbox("Water Bake##gg_water_bake", &bOff))
			{
				gg_water_bake = bOff;
				t.gamevisuals.bWaterBake = t.visuals.bWaterBake = bOff;
				g.projectmodified = 1;
				// ⚠ Deliberately NO GGApplyVisualsNow() here. gg_no_water - the flag the apply
				// reads - is not written until GGWaterBake_Update runs, so applying from this
				// handler always used the PREVIOUS state: ticking left the ocean on and unticking
				// never brought it back. GGWaterBake_Update now writes the flag and applies, in
				// that order.
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Replaces the animated water with a flat coloured plane at the same height. This removes the reflection pass that redraws the whole scene a second time from a mirrored camera, and the wave simulation with it, while leaving something that still reads as water from the shore. Usually the single biggest saving here on any level that has water.");

			// ------------------------------------------------------------------------------
			// GGMAX 3.08: round 2. The four above remove CONTENT; these remove per-frame
			// RENDERING WORK, so they still help on a level that has no terrain, trees, grass
			// or water left to strip - an indoor level being the obvious case.
			// Same session scope and the same setup.ini OR panel arrangement.
			// ------------------------------------------------------------------------------
			// GGMAX 3.25: "Post Effects Off" and "Simple Sky" REMOVED from the panel on Lee's
			// instruction after testing on a 6-year-old AMD card - neither did anything visible.
			// The globals and their setup.ini keys (nopostfx / simplesky) are deliberately LEFT
			// IN PLACE: they are read by GGApplyLowSpecSwitches and by the harness, and removing
			// a machine-wide ini key is a separate, user-visible decision. What is gone is the
			// panel control, because a control that does nothing is worse than no control.
			extern bool gg_no_ao, gg_no_shadows;
			extern void GGSetNoAOLevel(int);
			extern void GGSetNoShadowsLevel(int);

			bOff = gg_no_ao;
			if (ImGui::Checkbox("Ambient Occlusion Off##gg_no_ao", &bOff))
			{
				GGSetNoAOLevel(bOff ? 1 : 0);
				t.gamevisuals.bNoAO = t.visuals.bNoAO = bOff;
				g.projectmodified = 1;
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Switches off the pass that darkens creases and corners where surfaces meet. A full screen pass every frame, so it costs the same whether your level is a cupboard or a continent.");

			bOff = gg_no_shadows;
			if (ImGui::Checkbox("Shadows Off##gg_no_shadows", &bOff))
			{
				GGSetNoShadowsLevel(bOff ? 1 : 0);
				t.gamevisuals.bNoShadows = t.visuals.bNoShadows = bOff;
				g.projectmodified = 1;
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Switches off every shadow in the level. The scene has to be drawn again from each shadow casting light, so this is usually the biggest single saving here - and the most obvious one to look at. Try the Shadows section first if you only want to soften the cost.");

			// GGMAX 3.09
			extern bool gg_no_occlusion; extern int gg_particle_pct;
			extern void GGSetNoOcclusionLevel(int);
			extern void GGSetParticlePctLevel(int);

			bOff = gg_no_occlusion;
			if (ImGui::Checkbox("Occlusion Culling Off##gg_no_occlusion", &bOff))
			{
				GGSetNoOcclusionLevel(bOff ? 1 : 0);
				t.gamevisuals.bNoOcclusionCull = t.visuals.bNoOcclusionCull = bOff;
				g.projectmodified = 1;
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Stops asking the graphics card which objects are hidden behind other objects. This is an experiment rather than a saving and it can go either way: the asking is itself work, so on an open scene where little is hidden it costs more than it saves and turning it off is faster. Indoors, where whole rooms are hidden from you, leave it on. Worth trying both ways on your own level.");

			int pct = gg_particle_pct;
			ImGui::Text("Particle Density");
			if (ImGui::SliderInt("##gg_particle_pct", &pct, 0, 100, "%d%%"))
			{
				GGSetParticlePctLevel(pct);
				t.gamevisuals.iParticlePct = t.visuals.iParticlePct = pct;
				g.projectmodified = 1;
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("How many particles to emit, as a percentage of what the level asked for. 50%% halves every smoke plume, spark shower and waterfall; 0%% stops them emitting altogether. Particles already in the air live out their lifetime, so the change eases in over a second rather than popping. Affects the modern particle system only.");

			// GGMAX 3.11
			extern float gg_object_cull_dist;
			extern void GGSetObjectCullDistLevel(int);
			int cull = (int)gg_object_cull_dist;
			ImGui::Text("Object Detail Distance");
			if (ImGui::SliderInt("##gg_object_cull_dist", &cull, 0, 40000, cull == 0 ? "Off" : "%d"))
			{
				GGSetObjectCullDistLevel(cull);
				t.gamevisuals.iObjectCullDist = t.visuals.iObjectCullDist = cull;
				g.projectmodified = 1;
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("How far away objects are still drawn, in world units. Off draws everything however distant it is. 20000 is about 500 metres, 8000 about 200 metres and very aggressive. Objects do not pop out - each dissolves away over roughly its own size - and they come back when you raise it. This is the bluntest control here and on a large outdoor level the most effective, because it removes the draw calls, the triangles and the shadow casting together. Trees keep their own distance system and are not affected.");

			// GGMAX 3.12: texture detail. LOAD-TIME, so it is a combo rather than a live slider.
			{
				// GGMAX 3.19: LIVE. The combo used to only affect the next level you loaded, which
				// from the chair looked like a control that did nothing. GGSetTextureDivideLive
				// arms a re-create that the main thread performs on the next update.
				extern void GGSetTextureDivideLive(int);
				const char* texdiv_items[] = { "Full", "Half", "Quarter" };
				int cur = (wi::resourcemanager::gg_texture_divide >= 4) ? 2 : ((wi::resourcemanager::gg_texture_divide == 2) ? 1 : 0);
				ImGui::Text("Texture Detail");
				if (ImGui::Combo("##gg_texture_divide", &cur, texdiv_items, IM_ARRAYSIZE(texdiv_items)))
				{
					const int divide = (cur == 2 ? 4 : (cur == 1 ? 2 : 1));
					GGSetTextureDivideLive(divide);
					t.gamevisuals.iTextureDivide = t.visuals.iTextureDivide = divide;
					g.projectmodified = 1;
				}
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("Loads every texture at a fraction of its authored size. Half turns a 1024x1024 into a 512x512, Quarter into a 256x256. This is the one to reach for on a card short of video memory or memory bandwidth: it cuts texture memory about four times at Half and sixteen times at Quarter, and makes every texture read cheaper as well. Surfaces get softer up close, which is the whole trade. The change applies to the level you are looking at, terrain included - expect a short pause while every texture is rebuilt.");
			}

			// ★ GGMAX 3.89: WATER REFLECTION SIZE.
			//
			// The planar reflection target is sized in the engine at internal resolution / 4 -
			// measured 384x200 on a 1536x801 window with DUMP_REFLECTION - and flat water swims
			// when the camera rotates slightly, as though the sample were crossing two very
			// different texels. This is the DIAL to find out, not a fix: index 0 is Auto and
			// reproduces the stock /4 bit for bit, so no existing level moves until it is touched.
			{
				static const int kReflW[6] = { 0, 512, 768, 1024, 1536, 2048 };
				int ridx = 0;
				for (int i = 0; i < 6; i++) if (kReflW[i] == t.visuals.iReflectionWidth) { ridx = i; break; }
				char rlbl[64];
				if (kReflW[ridx] == 0)
				{
					sprintf_s(rlbl, sizeof(rlbl), "Auto (quarter of screen)");
				}
				else
				{
					// ⚠ The reflection target is NOT square - it follows the main camera's aspect
					// (the measured 384x200 is 1.92). Derive the height rather than printing
					// "N x N" as Terrain Bake Detail does, or the label lies about the texture.
					int rh = 0;
					if (master_renderer)
					{
						const int irx = (int)master_renderer->GetInternalResolution().x;
						const int iry = (int)master_renderer->GetInternalResolution().y;
						if (irx > 0) rh = (int)(((long long)kReflW[ridx] * iry) / irx);
					}
					if (rh > 0) sprintf_s(rlbl, sizeof(rlbl), "%d x %d", kReflW[ridx], rh);
					else        sprintf_s(rlbl, sizeof(rlbl), "%d wide", kReflW[ridx]);
				}
				ImGui::Text("Water Reflection Size");
				if (ImGui::SliderInt("##gg_reflection_width", &ridx, 0, 5, rlbl))
				{
					if (ridx < 0) ridx = 0;
					if (ridx > 5) ridx = 5;
					// ⚠ BOTH copies, in one statement. The .fpm is saved FROM gamevisuals
					// (M-MapFile_part0.cpp:242), so a control that writes only t.visuals appears to
					// work all session and never reaches the level file.
					t.gamevisuals.iReflectionWidth = t.visuals.iReflectionWidth = kReflW[ridx];
					g.projectmodified = 1;
					// Live: the per-frame push in MasterRenderer::Update sees the change next frame and
					// re-creates the four reflection targets. Nothing to call from here.
				}
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("How big the picture is that the water uses for its reflection. Auto is what the engine has always done - a quarter of your screen's width and height - and it is the first thing to try when still, flat water shimmers or crawls as you turn the camera slowly: at Auto that picture is being stretched four times, so neighbouring pixels in it can be very different from each other. Larger sizes make the reflection sharper and steadier. The cost is video memory and some frame rate, because the whole scene is drawn a second time into that bigger picture - the largest setting is around a hundred megabytes and is meant for finding out what is wrong rather than for shipping. Try one step up first. Only water and other mirror surfaces are affected.");
			}

			// ★ GGMAX 3.90: WATER REFLECTION BLUR - the cheap substitute for the size above.
			//
			// Lee's own sweep of Water Reflection Size settled the cause: much reduced at 768 wide,
			// ZERO swim at 2048. A monotone response like that is the signature of under-sampling,
			// and blur attacks the same thing for a fraction of the cost - one pass over 76,800
			// texels instead of a 5x larger second render of the whole scene.
			{
				int rb = t.visuals.iReflectionBlur;
				if (rb < 0) rb = 0;
				if (rb > 3) rb = 3;
				char blbl[48];
				if (rb == 0) sprintf_s(blbl, sizeof(blbl), "Off");
				else         sprintf_s(blbl, sizeof(blbl), "%d pass%s", rb, rb == 1 ? "" : "es");
				ImGui::Text("Water Reflection Blur");
				if (ImGui::SliderInt("##gg_reflection_blur", &rb, 0, 3, blbl))
				{
					if (rb < 0) rb = 0;
					if (rb > 3) rb = 3;
					// ⚠ BOTH copies - the .fpm saves from gamevisuals (M-MapFile_part0.cpp:242).
					t.gamevisuals.iReflectionBlur = t.visuals.iReflectionBlur = rb;
					g.projectmodified = 1;
				}
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("Softens the reflection picture instead of making it bigger. This is the cheap answer to a still pond that shimmers or crawls as you turn the camera: the reflection is a small picture stretched over the water, and smoothing it removes the crawl for almost no frame rate, where raising Water Reflection Size removes it by drawing the whole scene again at a larger size. Blurry reflections read as natural diffusion on water, so one or two passes often looks better rather than worse. Each step blurs further. ⚠ It affects every mirror surface in the level, not only water, so a polished floor or a puddle meant to look sharp will soften too.");
			}

			ImGui::PopItemWidth();
		}

		// end performance
		ImGui::Indent(-10);
	}
	return(bVisualUpdated);
}

bool AI_Management_Settings(float fTabColumnWidth, bool bVisualUpdated)
{
	int wflags = ImGuiTreeNodeFlags_None;
	float w = ImGui::GetWindowContentRegionWidth();

	if (pref.bAutoClosePropertySections && iLastOpenHeader != 12)
		ImGui::SetNextItemOpen(false, ImGuiCond_Always);

	if (ImGui::StyleCollapsingHeader("AI Management", wflags))
	{
		iLastOpenHeader = 12;
		ImGui::Indent(10);

		ImGui::PushItemWidth(-10);
		extern bool g_bShowRecastDetourDebugVisuals;
		if (ImGui::Checkbox("Show Navigation Debug Visuals", &g_bShowRecastDetourDebugVisuals))
		{
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toggle whether the navigation system debug visuals should be shown (visible in test game, where AI navigation runs)");
		ImGui::PopItemWidth();

		ImGui::PushItemWidth(-10);
		if (ImGui::Checkbox("Show Object Debug Visuals", &t.luaglobal.showobjectdebugvisuals))
		{
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toggle whether the LUA system should show object debug visuals");
		ImGui::PopItemWidth();

		ImGui::PushItemWidth(-10);

		extern bool g_bResetHasForLevelGeneration;
		if (ImGui::Checkbox("Disable Navmesh Generation", &t.visuals.bEnableZeroNavMeshMode))
		{
			t.gamevisuals.bEnableZeroNavMeshMode = t.visuals.bEnableZeroNavMeshMode;
			t.editorvisuals.bEnableZeroNavMeshMode = t.visuals.bEnableZeroNavMeshMode;
			g_bResetHasForLevelGeneration = true;
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toggle navmesh generation (enables system to detecting walkable areas) (needs to rebuild level)");
		ImGui::PopItemWidth();

		//PE: Buttons can only be used in test game Tab Tab.
		if (bRenderTabTab)
		{
			float but_gadget_size = ImGui::GetFontSize() * 10.0;
			ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2((w * 0.5) - (but_gadget_size * 0.5), 0.0f));
			if (ImGui::StyleButton("Edit Behaviors##TabTabEditBehaviors", ImVec2(but_gadget_size, 0)))
			{
				extern bool g_bBehaviorEditorActive;
				g_bBehaviorEditorActive = true;
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Open the Behavior Editor to debug and edit logic live");

			ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2((w * 0.5) - (but_gadget_size * 0.5), 0.0f));
			if (ImGui::StyleButton("View Slowest Logic##TabTabEditBehaviors", ImVec2(but_gadget_size, 0)))
			{
				extern int g_iViewPerformanceTimers;
				g_iViewPerformanceTimers = 1;
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Run a live snapshot of the ten slowest behaviours currently running");

			ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2((w * 0.5) - (but_gadget_size * 0.5), 0.0f));
			if (ImGui::StyleButton("View Playing Sounds##TabTabEditBehaviors", ImVec2(but_gadget_size, 0)))
			{
				extern int g_iViewPlayingSounds;
				g_iViewPlayingSounds = 1;
			}
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Run a live snapshot of the ten currently playing sounds");
		}

		ImGui::Indent(-10);
	}

	return(bVisualUpdated);
}
bool Global_Behaviors_Settings(float fTabColumnWidth, bool bVisualUpdated)
{
	int wflags = ImGuiTreeNodeFlags_None;

	if (pref.bAutoClosePropertySections && iLastOpenHeader != 35)
		ImGui::SetNextItemOpen(false, ImGuiCond_Always);

	if (ImGui::StyleCollapsingHeader("Global Behaviors", wflags))
	{
		static int iSelectecElement = -1;
		iLastOpenHeader = 35;

		ImGui::Indent(10);
		float buttonwide = ImGui::GetContentRegionAvail().x * 0.5 - 10.0f;
		static std::string myscript = "";
		ImGuiWindow* window = ImGui::GetCurrentWindow();
		if (iSelectedLibraryStingReturnID == window->GetID("ScriptSelector##+"))
		{
			//Update Script.
			if (sSelectedLibrarySting != "")
			{
				myscript = sSelectedLibrarySting.Get();
				sSelectedLibrarySting = "";
				iSelectedLibraryStingReturnID = -1; //disable.
				fpe_current_loaded_script = -1; //Reload image and DLUA.
				CloseDownEditorProperties();
				t.inputsys.constructselection = 0;
				iLastEntityOnCursor = 0;

				t.addentityfile_s = "_markers\\BehaviorHidden.fpe";
				if (t.addentityfile_s != "")
				{
					entity_adduniqueentity(false);
					t.tasset = t.entid;
					if (t.talreadyloaded == 0)
					{
						editor_filllibrary();
						iRestoreEntidMaster = -1;
					}
				}
				t.tentid = t.entid;
				t.sourceobj = g.entitybankoffset + t.tentid;

				t.gridentity = t.entid;

				//PE: all t.gridentity... need to be set for this to work correctly.
				t.gridentitystaticmode = t.entityprofile[t.entid].defaultstatic;
				t.gridentityposx_f = 0;
				t.gridentityposy_f = -999999;
				t.gridentityposz_f = 0;
				t.gridentityrotatex_f = 0;
				t.gridentityrotatey_f = 0;
				t.gridentityrotatez_f = 0;
				t.gridentityrotatequatmode = 0;
				t.gridentityrotatequatx_f = 0;
				t.gridentityrotatequaty_f = 0;
				t.gridentityrotatequatz_f = 0;
				t.gridentityrotatequatw_f = 1;
				t.gridentityscalex_f = 10;
				t.gridentityscaley_f = 10;
				t.gridentityscalez_f = 10;
				entity_fillgrideleproffromprofile();

				//PE: InstanceObject - Cursor,Object Tools - objects must always be real clones.
				extern bool bNextObjectMustBeClone;
				bNextObjectMustBeClone = true;
				t.e = 0;
				gridedit_addentitytomap(); //Add it to map set t.e
				bNextObjectMustBeClone = false;

				t.entityelement[t.e].eleprof.aimain_s = myscript.c_str();
				t.entityelement[t.e].eleprof.thumb_aimain_s = "";

				PositionObject(t.entityelement[t.e].obj, t.entityelement[t.e].x, t.entityelement[t.e].y, t.entityelement[t.e].z);
				RotateObject(t.entityelement[t.e].obj, t.entityelement[t.e].rx, t.entityelement[t.e].ry, t.entityelement[t.e].rz);
				HideObject(t.entityelement[t.e].obj);

				// Show elements when placing a new one down, prevents half being hidden and half not.
				t.showeditorelements = 1;
				editor_toggle_element_vis(t.showeditorelements);

				editor_refresheditmarkers();

				t.refreshgrideditcursor = 1;
				current_selected_group = -1;
				t.gridentity = 0;
				t.gridentityposoffground = 0;
				t.gridentityusingsoftauto = 0;
				t.gridentityautofind = 0;
				t.gridentityobj = 0;
				editor_refreshentitycursor();
				t.widget.pickedObject = 0;
				t.gridentityextractedindex = 0;

				t.widget.pickedObject = 0; widget_updatewidgetobject();
				iSelectecElement = t.e - 1;
			}
		}

		if (ImGui::StyleButton("Add", ImVec2(buttonwide, 0)))
		{
			//Select script.
			sStartLibrarySearchString = "global";
			iLastDisplayLibraryType = -1;
			bExternal_Entities_Window = true;
			iDisplayLibraryType = 4;
			iLibraryStingReturnToID = window->GetID("ScriptSelector##+");
		}
		ImGui::SameLine();
		int iDeleteIfFound = 0;
		if (ImGui::StyleButton("Delete", ImVec2(buttonwide, 0)))
		{
			if (iSelectecElement >= 0)
			{
				//PE: Delete.
				int iAction = askBoxCancel("This will delete all your visual changes, are you sure?", "Confirmation"); //1==Yes 2=Cancel 0=No
				if (iAction == 1)
				{
					iDeleteIfFound = iSelectecElement;
				}
			}
		}

		uint32_t uniqueId = 4000;
		uniqueId += 28000; //PE: from lib uniqueId += 24000;

		int iDefaultTexture = FILETYPE_SCRIPT;
		float w = ImGui::GetContentRegionAvailWidth();
		ImGui::Columns(2, "GlobalBehaviors2elements", false);  //false no border
		ImGui::SetColumnWidth(0, w * 0.5f);
		ImGui::SetColumnWidth(1, w * 0.5f);
		bool bFoundSelected = false;
		for (t.e = 1; t.e <= g.entityelementlist; t.e++)
		{
			t.entid = t.entityelement[t.e].bankindex;
			if (t.entid > 0 && t.entityprofile[t.entid].ismarker == 12)
			{
				int image = FILETYPE_SCRIPT;
				if (t.entityelement[t.e].eleprof.aimain_s.Len() > 0)
				{
					//PE: check if we need to update icons.
					if (t.entityelement[t.e].eleprof.aimain_s != t.entityelement[t.e].eleprof.thumb_aimain_s ||
						t.entityelement[t.e].eleprof.thumb_id != uniqueId)
					{
						//PE: Load image.
						std::string sFile = Left(t.entityelement[t.e].eleprof.aimain_s.Get(), Len(t.entityelement[t.e].eleprof.aimain_s.Get()) - 4);
						std::string sImgName = "scriptbank\\" + sFile;
						if (pref.current_style == 25 || pref.current_style == 3)
							sImgName += ".png";
						else
							sImgName += "2.png";

						if (ImageExist(uniqueId) == 1) DeleteImage(uniqueId);
						image_setlegacyimageloading(true);
						LoadImage((char*)sImgName.c_str(), uniqueId);
						image_setlegacyimageloading(false);
						t.entityelement[t.e].eleprof.thumb_aimain_s = t.entityelement[t.e].eleprof.aimain_s;
						t.entityelement[t.e].eleprof.thumb_id = uniqueId;
					}
					if (ImageExist(uniqueId))
					{
						image = uniqueId;
					}
					uniqueId++;
				}


				int iTextureID = image;

				ImVec2 ImageSize = ImVec2(buttonwide, ImGui::GetFontSize());
				if (!(ImGui::GetColumnIndex() % 2))
					ImageSize.x -= 2;

				ID3D11ShaderResourceView* lpTexture = GetImagePointerView(iTextureID);
				if (lpTexture)
				{
					float img_w = ImageWidth(iTextureID);
					float img_h = ImageHeight(iTextureID);
					ImageSize.y = img_h * (ImageSize.x / img_w);
				}
				ImVec2 vImagePos = ImGui::GetCursorPos();
				ImGui::Dummy(ImageSize);
				ImVec4 color = ImVec4(1.0, 1.0, 1.0, 1.0);
				ImVec4 back_color = ImVec4(0.2, 0.2, 0.2, 0.75);

				if (ImGui::IsItemHovered())
				{
					color.w = 0.75;
					if (ImGui::IsMouseReleased(0))
					{
						iSelectecElement = t.e;
						fpe_current_loaded_script = -1; //Reload image and DLUA.

					}
				}

				ImVec2 img_pos = ImGui::GetWindowPos() + vImagePos;
				img_pos.x -= 3; //PE: Fit under buttons.
				if ((ImGui::GetColumnIndex() % 2))
					img_pos.x -= 3;
				img_pos.y -= ImGui::GetScrollY();
				window->DrawList->AddRectFilled(img_pos, img_pos + ImageSize, ImGui::GetColorU32(back_color));
				if (lpTexture)
				{
					window->DrawList->AddImage((ImTextureID)lpTexture, img_pos, img_pos + ImageSize, ImVec2(0, 0), ImVec2(1, 1), ImGui::GetColorU32(color));
				}
				if (iSelectecElement == t.e)
				{
					bFoundSelected = true;
					ImVec4 bg_col = ImGui::GetStyle().Colors[ImGuiCol_PlotHistogram]; // { 0.0, 0.0, 0.0, 1.0 };
					window->DrawList->AddRect(img_pos, img_pos + ImageSize - ImVec2(0, 0), ImGui::GetColorU32(bg_col), 0, 15, 3.0f);
				}

				std::string name = t.entityelement[t.e].eleprof.aimain_s.Get();
				std::size_t slash = name.find_last_of("/\\");
				if (slash > 0)
				{
					name = name.substr(slash + 1);
				}
				ImGui::TextCenter(" %s", name.c_str());

				ImGui::NextColumn();

			}
		}
		ImGui::Columns(1);

		ImGui::Separator();
		if (bFoundSelected && iSelectecElement >= 0)
		{
			int iEntityIndex = iSelectecElement;
			int iMasterID = t.entityelement[iEntityIndex].bankindex;
			if (iMasterID > 0)
			{
				DisplayFPEBehavior(false, iMasterID, &t.entityelement[iEntityIndex].eleprof, iEntityIndex, true);
				ImGui::Separator();
				bool btmp = t.entityelement[iEntityIndex].eleprof.systemwide_lua;

				float fPropertiesColoumWidth = ImGui::GetCursorPosX() + 110.0f;
				ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX(), ImGui::GetCursorPosY() + 3));
				ImGui::Text("Active All Levels");
				ImGui::SameLine();
				ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX(), ImGui::GetCursorPosY() - 3));
				ImGui::SetCursorPos(ImVec2(fPropertiesColoumWidth, ImGui::GetCursorPosY()));
				if (ImGui::Checkbox("##Active All Levels", &btmp))
				{
					t.entityelement[iEntityIndex].eleprof.systemwide_lua = btmp;
				}
			}
		}

		if (bFoundSelected && iDeleteIfFound > 0)
		{
			t.obj = t.entityelement[iDeleteIfFound].obj;
			if (t.obj > 0)
			{
				if (ObjectExist(t.obj) == 1)
				{
					DeleteObject(t.obj);
				}
			}
			t.entityelement[iDeleteIfFound].obj = 0;
			t.entityelement[iDeleteIfFound].maintype = 0;
			t.entityelement[iDeleteIfFound].bankindex = 0;
			fpe_current_loaded_script = -1; //Reload image and DLUA.
			iSelectecElement = -1;
		}

		ImGui::Indent(-10);
	}
	return(bVisualUpdated);
}

bool Shadows_Settings(float fTabColumnWidth, bool bVisualUpdated)
{
	int wflags = ImGuiTreeNodeFlags_None;
	if (pref.bAutoClosePropertySections && iLastOpenHeader != 9)
		ImGui::SetNextItemOpen(false, ImGuiCond_Always);

	if (ImGui::StyleCollapsingHeader("Shadows", wflags))
	{
		ImGui::Indent(10);

		iLastOpenHeader = 9;

		ImGui::TextCenter("Shadow Resolution");
		//PE: Change from 1024 to 4096, adds around 2 gb additional gpu mem ?.
		//PE: See comment about point light below.
		const char* shadow_spot_items_align[] = { "Off", "128", "256", "512", "1024", "2048" }; //, "4096" };
		int shadow_cascade_current_type_selection = 0;
		if (t.visuals.iShadowSpotCascadeResolution == 0) shadow_cascade_current_type_selection = 0;
		else if (t.visuals.iShadowSpotCascadeResolution == 128) shadow_cascade_current_type_selection = 1;
		else if (t.visuals.iShadowSpotCascadeResolution == 256) shadow_cascade_current_type_selection = 2;
		else if (t.visuals.iShadowSpotCascadeResolution == 512) shadow_cascade_current_type_selection = 3;
		else if (t.visuals.iShadowSpotCascadeResolution == 1024) shadow_cascade_current_type_selection = 4;
		else if (t.visuals.iShadowSpotCascadeResolution == 2048) shadow_cascade_current_type_selection = 5;
		else shadow_cascade_current_type_selection = 5; //6;
		tab_tab_Column_text("Sun ", fTabColumnWidth);
		ImGui::PushItemWidth(-10);
		if (ImGui::Combo("##setshadow_spotresolution", &shadow_cascade_current_type_selection, shadow_spot_items_align, IM_ARRAYSIZE(shadow_spot_items_align)))
		{
			if (shadow_cascade_current_type_selection == 0) t.visuals.iShadowSpotCascadeResolution = 0;
			else if (shadow_cascade_current_type_selection == 1) t.visuals.iShadowSpotCascadeResolution = 128;
			else if (shadow_cascade_current_type_selection == 2) t.visuals.iShadowSpotCascadeResolution = 256;
			else if (shadow_cascade_current_type_selection == 3) t.visuals.iShadowSpotCascadeResolution = 512;
			else if (shadow_cascade_current_type_selection == 4) t.visuals.iShadowSpotCascadeResolution = 1024;
			else if (shadow_cascade_current_type_selection == 5) t.visuals.iShadowSpotCascadeResolution = 2048;
			else t.visuals.iShadowSpotCascadeResolution = 2048;
			t.gamevisuals.iShadowSpotCascadeResolution = t.visuals.iShadowSpotCascadeResolution;
			bVisualUpdated = true;
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Choose a shadow quality for sun shadows");
		ImGui::PopItemWidth();


		//SPOT
		//const char* shadow_spot_items_align[] = { "Off", "128", "256", "512", "1024", "2048" }; //, "4096" };
		int shadow_spot_current_type_selection = 0;
		if (t.visuals.iShadowSpotResolution == 0) shadow_spot_current_type_selection = 0;
		else if (t.visuals.iShadowSpotResolution == 128) shadow_spot_current_type_selection = 1;
		else if (t.visuals.iShadowSpotResolution == 256) shadow_spot_current_type_selection = 2;
		else if (t.visuals.iShadowSpotResolution == 512) shadow_spot_current_type_selection = 3;
		else if (t.visuals.iShadowSpotResolution == 1024) shadow_spot_current_type_selection = 4;
		else if (t.visuals.iShadowSpotResolution == 2048) shadow_spot_current_type_selection = 5;
		else shadow_spot_current_type_selection = 5; //6;
		tab_tab_Column_text("Spot Lights ", fTabColumnWidth);
		ImGui::PushItemWidth(-10);
		if (ImGui::Combo("##setshadow_spotspotresolution", &shadow_spot_current_type_selection, shadow_spot_items_align, IM_ARRAYSIZE(shadow_spot_items_align)))
		{
			if (shadow_spot_current_type_selection == 0) t.visuals.iShadowSpotResolution = 0;
			else if (shadow_spot_current_type_selection == 1) t.visuals.iShadowSpotResolution = 128;
			else if (shadow_spot_current_type_selection == 2) t.visuals.iShadowSpotResolution = 256;
			else if (shadow_spot_current_type_selection == 3) t.visuals.iShadowSpotResolution = 512;
			else if (shadow_spot_current_type_selection == 4) t.visuals.iShadowSpotResolution = 1024;
			else if (shadow_spot_current_type_selection == 5) t.visuals.iShadowSpotResolution = 2048;
			else t.visuals.iShadowSpotResolution = 2048;
			t.gamevisuals.iShadowSpotResolution = t.visuals.iShadowSpotResolution;
			bVisualUpdated = true;
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Choose a shadow quality spot lights");
		ImGui::PopItemWidth();


		//PE: This drain memory and make everything very slow, limit to max 2048 for now.
		//PE: 1 point light have 6 * 4096x4096 textures. (BIND_DEPTH_STENCIL) DXGI_FORMAT_R16_TYPELESS (2 bytes per pixel).
		//PE: AND 6 * 4096x4096 textures (BIND_RENDER_TARGET) FORMAT_R16G16B16A16_FLOAT (8 bytes per pixel).
		//PE: So 32mb + 128mb. = 160 mb. per texture = 6 * 160mb = 960 mb. PER point light :(
		//PE: Each point light have 12 4096x4096 textures.
		//PE: On my system with 5 point light, it use around 4gb additional mem, when i set 4096 ?
		//PE: This gets allocated as shared GPU mem, and sure tons of swapping is going on.
		//PE: Latest REPO changed from:	desc.Format = FORMAT_R11G11B10_FLOAT; to: desc.Format = FORMAT_R16G16B16A16_FLOAT; (32bit to 64 bit.)
		//PE: https://github.com/turanszkij/WickedEngine/commit/d27ede94cc76a91ac1a1c9e4393db6460262d120#
		//PE: Perhaps limit to 1024 ? - 2048 = 240 mb. per point light , and 1024 = 60mb.

		const char* shadow_point_items_align[] = { "Off" , "128", "256", "512", "1024", "2048" }; //, "4096" };
		int shadow_point_current_type_selection = 0;
		if (t.visuals.iShadowPointResolution == 0) shadow_point_current_type_selection = 0;
		else if (t.visuals.iShadowPointResolution == 128) shadow_point_current_type_selection = 1;
		else if (t.visuals.iShadowPointResolution == 256) shadow_point_current_type_selection = 2;
		else if (t.visuals.iShadowPointResolution == 512) shadow_point_current_type_selection = 3;
		else if (t.visuals.iShadowPointResolution == 1024) shadow_point_current_type_selection = 4;
		else if (t.visuals.iShadowPointResolution == 2048) shadow_point_current_type_selection = 5;
		else shadow_point_current_type_selection = 5; //6;
		tab_tab_Column_text("Point Lights ", fTabColumnWidth);
		ImGui::PushItemWidth(-10);
		if (ImGui::Combo("##setshadow_pointresolution", &shadow_point_current_type_selection, shadow_point_items_align, IM_ARRAYSIZE(shadow_point_items_align)))
		{
			if (shadow_point_current_type_selection == 0) t.visuals.iShadowPointResolution = 0;
			else if (shadow_point_current_type_selection == 1) t.visuals.iShadowPointResolution = 128;
			else if (shadow_point_current_type_selection == 2) t.visuals.iShadowPointResolution = 256;
			else if (shadow_point_current_type_selection == 3) t.visuals.iShadowPointResolution = 512;
			else if (shadow_point_current_type_selection == 4) t.visuals.iShadowPointResolution = 1024;
			else if (shadow_point_current_type_selection == 5) t.visuals.iShadowPointResolution = 2048;
			else t.visuals.iShadowPointResolution = 2048;
			if (t.visuals.iShadowPointResolution > 2048) t.visuals.iShadowPointResolution = 2048;
			t.gamevisuals.iShadowPointResolution = t.visuals.iShadowPointResolution;
			bVisualUpdated = true;
		}

		if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Choose a shadow quality for point lights");
		ImGui::PopItemWidth();

		ImGui::TextCenter("Shadow Quantity");
		const char* shadow_spot_max_align[] = { "0", "4", "8", "12", "16" };
		// GGMAX 2.38: the if-chains below match only 0/4/8/12/16 EXACTLY and have no else, so any
		// other value left the selection at its initialiser 0 and the panel printed "0" — a flat
		// lie about what the renderer was doing, and worse, touching the combo would then write
		// that 0 back and really destroy the setting.
		// This is not hypothetical: SetGlobalGraphicsSettings' LOW preset
		// (M-GridEdit_part0.cpp:1768-1770) sets iShadowPointMax=2 and iShadowSpotMax=1, neither of
		// which is in the list. That is exactly the reported "editor shows 8/16, test game shows
		// 0/0" — measured with DUMP_SHADOWQTY: in test game visuals=1/2 while gamevisuals stayed
		// 8/16, so the counts were never zeroed, only misreported.
		// Seed the selection from the NEAREST listed value so the panel can never misreport.
		auto ggNearestShadowQty = [](int v) -> int
		{
			static const int listed[] = { 0, 4, 8, 12, 16 };
			int best = 0, bestd = abs(v - listed[0]);
			for (int q = 1; q < 5; ++q) { const int d = abs(v - listed[q]); if (d < bestd) { bestd = d; best = q; } }
			return best;
		};
		int shadow_cascade_max_current_type_selection = ggNearestShadowQty(t.visuals.iShadowSpotMax);
		if (t.visuals.iShadowSpotMax == 0) shadow_cascade_max_current_type_selection = 0;
		else if (t.visuals.iShadowSpotMax == 4) shadow_cascade_max_current_type_selection = 1;
		else if (t.visuals.iShadowSpotMax == 8) shadow_cascade_max_current_type_selection = 2;
		else if (t.visuals.iShadowSpotMax == 12) shadow_cascade_max_current_type_selection = 3;
		else if (t.visuals.iShadowSpotMax == 16) shadow_cascade_max_current_type_selection = 4;
		tab_tab_Column_text("Spot Lights ", fTabColumnWidth);
		ImGui::PushItemWidth(-10);
		if (ImGui::Combo("##setshadow_iShadowSpotMax", &shadow_cascade_max_current_type_selection, shadow_spot_max_align, IM_ARRAYSIZE(shadow_spot_max_align)))
		{
			if (shadow_cascade_max_current_type_selection == 0) t.visuals.iShadowSpotMax = 0;
			else if (shadow_cascade_max_current_type_selection == 1) t.visuals.iShadowSpotMax = 4;
			else if (shadow_cascade_max_current_type_selection == 2) t.visuals.iShadowSpotMax = 8;
			else if (shadow_cascade_max_current_type_selection == 3) t.visuals.iShadowSpotMax = 12;
			else if (shadow_cascade_max_current_type_selection == 4) t.visuals.iShadowSpotMax = 16;
			t.gamevisuals.iShadowSpotMax = t.visuals.iShadowSpotMax;
			bForceRefreshLightCount = true;
			bVisualUpdated = true;
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Choose max shadow casters for spot lights");
		ImGui::PopItemWidth();

		// GGMAX 2.38: same nearest-value seed as the spot combo above — the LOW preset's
		// iShadowPointMax=2 is not a listed value and was displaying as "0".
		int shadow_cascade_point_current_type_selection = ggNearestShadowQty(t.visuals.iShadowPointMax);
		if (t.visuals.iShadowPointMax == 0) shadow_cascade_point_current_type_selection = 0;
		else if (t.visuals.iShadowPointMax == 4) shadow_cascade_point_current_type_selection = 1;
		else if (t.visuals.iShadowPointMax == 8) shadow_cascade_point_current_type_selection = 2;
		else if (t.visuals.iShadowPointMax == 12) shadow_cascade_point_current_type_selection = 3;
		else if (t.visuals.iShadowPointMax == 16) shadow_cascade_point_current_type_selection = 4;
		tab_tab_Column_text("Point Lights ", fTabColumnWidth);

		ImGui::PushItemWidth(-10);
		if (ImGui::Combo("##setshadow_iShadowPointMax", &shadow_cascade_point_current_type_selection, shadow_spot_max_align, IM_ARRAYSIZE(shadow_spot_max_align)))
		{
			if (shadow_cascade_point_current_type_selection == 0) t.visuals.iShadowPointMax = 0;
			else if (shadow_cascade_point_current_type_selection == 1) t.visuals.iShadowPointMax = 4;
			else if (shadow_cascade_point_current_type_selection == 2) t.visuals.iShadowPointMax = 8;
			else if (shadow_cascade_point_current_type_selection == 3) t.visuals.iShadowPointMax = 12;
			else if (shadow_cascade_point_current_type_selection == 4) t.visuals.iShadowPointMax = 16;
			t.gamevisuals.iShadowPointMax = t.visuals.iShadowPointMax;
			bForceRefreshLightCount = true;
			bVisualUpdated = true;
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Choose max shadow casters for point lights.");
		ImGui::PopItemWidth();

		// UI AUDIT 2026-07-28: "Transparent shadows" HIDDEN — wiRenderer::SetTransparentShadowsEnabled
		// was removed in the new WickedEngine, so the checkbox only forced a shadow refresh and
		// changed nothing. Field/save/load kept.
		// VRAM FLOOR 2026-08-02 (engine 1.78): the feature is now DROPPED, not merely unexposed.
		// The transparent shadow atlas was pure floor cost — 160 MB on every level and 512 MB on
		// Amazon / Foggy Forest / Disruption / Bounty — and measurably bought nothing visible, so
		// `wi::renderer::gg_transparent_shadows` now defaults false, the atlas is never allocated,
		// and the shadow pass is depth-only. Do NOT un-hide this checkbox: it would set a field
		// nothing reads, and re-enabling the feature for real needs a restart (the object shadow
		// PSOs latch the render-target count at LoadShaders). See GameGuru Core/VRAM_FLOOR.md.
		//if (ImGui::Checkbox("Transparent shadows", &t.visuals.bTransparentShadows))
		//{
		//	t.gamevisuals.bTransparentShadows = t.visuals.bTransparentShadows;
		//	bForceRefreshLightCount = true;
		//	bVisualUpdated = true;
		//}

		// UI AUDIT 2026-07-28: "Front Shadows Priority" HIDDEN — bShadowsInFrontTakesPriority
		// (wickedcalls_part0.cpp) is written here and read by NOTHING; the checkbox never did
		// anything on any build. If wanted, the concept could feed the point-shadow budget scoring.
		//extern bool bShadowsInFrontTakesPriority;
		//ImGui::Checkbox("Front Shadows Priority", &bShadowsInFrontTakesPriority);

		// Tree shadow controls — MOVED here from the Terrain Tools tree panel (2026-07-28,
		// user request) so they can be tuned LIVE in test game. Values live in t.visuals
		// (saved with the level, carried into test game — same mechanism as Transparent
		// Shadows); on-change they push straight into GGTrees, where GGTrees_part2
		// change-detects and applies on the next tree pass: distance re-evaluates each
		// tree's mesh-shadow flag in place; range live-updates proxy/pool cascade masks.
		ImGui::TextCenter("Tree Shadow LOD Distance");
		ImGui::PushItemWidth(-10);
		{
			// ★ GGMAX 3.108 (tester report #5) / 3.108c - Lee 2026-10-12: "allow a maximum of 5000 instead
			// of 2000, and set the default to 2000". [SUPERSEDED by 3.108d below: now 750..2000.]
			// 3.108c made the range a fixed 750..5000: 5000 is the furthest any
			// Graphics Quality setting draws trees in full detail (Ultra's tree pool reach), so it is the
			// honest top - the old 7000 was reached by no setting at all. Tree mesh shadows still cannot reach
			// past the trees drawn as meshes, whose reach the CURRENT quality setting decides (Low 2000,
			// Medium 3000, High 4000, Ultra 5000 - and Test Game often runs a different one from the editor).
			// A value beyond that acts as the reach, and the label says so instead of looking like a value
			// that does something it cannot (the 3.108 dead-range lesson). The stored value is written only
			// when the slider is dragged, so the editor's value is never cut down by a lower Test Game setting.
			// ★ 3.108d - Lee 2026-10-12, after trying 3.108c: "It seems you are capping at 2000 ... best to
			// set max at 2000 and default at this." Range 750..2000: 2000 is the reach of the LOWEST quality
			// setting (Low's tree pool), so every value means the same thing at every setting and the
			// "at this quality" label below can only fire on a non-stock pool cap (harness).
			const float reach = GGTrees::GGTrees_GetMeshShadowReach();
			float shown = t.visuals.fTreeShadowLODDist;
			if (shown > 2000.0f) shown = 2000.0f;
			if (shown < 750.0f) shown = 750.0f;
			char sliderFmt[64];
			if (shown > reach + 0.5f)
				sprintf_s(sliderFmt, sizeof(sliderFmt), "%%.0f (%.0f at this quality)", reach);
			else
				strcpy_s(sliderFmt, sizeof(sliderFmt), "%.0f");
			if (ImGui::SliderFloat("##setshadow_TreeShadowLODDist", &shown, 750, 2000, sliderFmt))
			{
				t.visuals.fTreeShadowLODDist = shown;
				t.gamevisuals.fTreeShadowLODDist = shown;
				GGTrees::ggtrees_global_params.lod_dist_shadow = shown;
				bVisualUpdated = true;
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("%s", "How far trees cast their full, detailed shadow (750 to 2000). Beyond it they cast simple card\n"
					"shadows, and the detailed shadow fades out smoothly from this distance. A lower value saves shadow work.");
			}
		}

		ImGui::TextCenter("Tree Shadow Range");
		if (ImGui::SliderInt("##setshadow_TreeShadowRange", &t.visuals.iTreeShadowRange, 0, 5))
		{
			t.gamevisuals.iTreeShadowRange = t.visuals.iTreeShadowRange;
			GGTrees::ggtrees_global_params.tree_shadow_range = t.visuals.iTreeShadowRange;
			bVisualUpdated = true;
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "How many shadow cascades receive tree shadows (0 = none, 5 = all cascades / farthest reach).");
		ImGui::PopItemWidth();

		ImGui::Indent(-10);
	}
	return bVisualUpdated;
}

void RenderPreviewEmitter(void)
{
	// ★ GGMAX 3.38 (DX11 3e21f674): do not drive the preview emitter while the particles library
	// browser is showing wicked particles - both want the same PreviewWPERoot, and the browser's
	// own preview was being torn down by this one.
	// ⚠ DX11 wraps the whole function body in `if (bExternal_Entities_Window == false) { ... }`.
	// An early return is exactly equivalent - verified there is no code after that block - and
	// avoids re-indenting 30 lines, which is pure risk for no behavioural gain.
	extern bool bExternal_Entities_Window;
	if (bExternal_Entities_Window == true) return;

	static bool bInit = true;
	if (PreviewWPERoot > 0)
	{
		float posx, posy, posz, posxa, posya, posza;
		int GetActiveEditorEntityPos(float* x, float* y, float* z, float* xa, float* ya, float* za);
		int iEntityIndex = GetActiveEditorEntityPos(&posx, &posy, &posz, &posxa, &posya, &posza);
		static int iEntityIndexCurrent = -1;
		if (!bInit && iEntityIndexCurrent != iEntityIndex && PreviewWPERoot != 0)
		{
			iEntityIndexCurrent = iEntityIndex;
			//PE: Delete effects.
			WickedCall_PerformEmitterAction(6, PreviewWPERoot);
			void DeleteEmitterEffects(uint32_t root);
			DeleteEmitterEffects(PreviewWPERoot);
			PreviewWPERoot = 0;
			bPreviewWPE = false;
		}
		if (bInit)
		{
			iEntityIndexCurrent = iEntityIndex;
			bInit = false;
		}
		if (iEntityIndex > 0 && PreviewWPERoot > 0)
		{
			extern float fPreviewYOffset;
			extern float fPreviewXOffset;
			extern float fPreviewZOffset;
			bool WickedCall_ParticleEffectPositionRotation(uint32_t root, float fX, float fY, float fZ, float fXa, float fYa, float fZa);
			WickedCall_ParticleEffectPositionRotation(PreviewWPERoot, posx + fPreviewXOffset, posy + fPreviewYOffset, posz + fPreviewZOffset, 0, posya, 0);
		}
	}
}

void GetActiveMonitorResolution( void )
{
	//PE: Try to get the resolution of the monitor Max is currently active in.
	HMONITOR hMonitor = MonitorFromWindow(g_pGlob->hWnd, MONITOR_DEFAULTTONEAREST);
	CurrentMonitorResolutionX = 0;
	CurrentMonitorResolutionY = 0;

	if (hMonitor == NULL) {
		return;
	}

	//Get the monitor's device name using MONITORINFOEX
	MONITORINFOEX info;
	info.cbSize = sizeof(MONITORINFOEX);

	if (!GetMonitorInfo(hMonitor, &info)) {
		return;
	}

	//Get the physical resolution using EnumDisplaySettings and the device name
	DEVMODE devMode;
	devMode.dmSize = sizeof(DEVMODE);

	if (EnumDisplaySettings(info.szDevice, ENUM_CURRENT_SETTINGS, &devMode)) {
		CurrentMonitorResolutionX = devMode.dmPelsWidth;
		CurrentMonitorResolutionY = devMode.dmPelsHeight;
	}
}
