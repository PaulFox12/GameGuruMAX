//----------------------------------------------------
//--- GAMEGURU - M-Ragdoll
//----------------------------------------------------

// Includes
#include "stdafx.h"
#include "gameguru.h"
#include <map>
#include <vector>

// Globals
bool g_bDebugRagdoll = false;// connect debug ragdoll and real object and we are golden :)

void ragdoll_init ( void )
{
	//  Create Joint Rotations and Limits
	g.KneeJoint.rotationVec3 = newvector3(90.0, 90.0, 0.0);
	g.KneeJoint.limitsVec2 = newvector2(10.0, 140.0);
	
	g.RightElbowJoint.rotationVec3 = newvector3(80.0, 90.0, 0.0);
	g.RightElbowJoint.limitsVec2 = newvector2(10.0, 100.0);

	g.LeftElbowJoint.rotationVec3 = newvector3(80.0, 90.0, 0.0);
	g.LeftElbowJoint.limitsVec2 = newvector2(10.0, 100.0);

	g.rightWristJoint.rotationVec3 = newvector3(0, 45, 90);
	g.rightWristJoint.limitsVec2 = newvector2(-5, 10);

	g.leftWristJoint.rotationVec3 = newvector3(0, 45, 90.0);
	g.leftWristJoint.limitsVec2 = newvector2(5, 10);

	g.WaistJoint.rotationVec3 = newvector3(90.0, 0.0, 0.0);
	g.WaistJoint.limitsVec2 = newvector2(0, 50);

	g.UpperBodyJoint.rotationVec3 = newvector3(0.0, 0.0, 0.0);
	g.UpperBodyJoint.limitsVec2 = newvector2(0, 5);

	g.HipJoint.rotationVec3 = newvector3(90, 0, 90);
	g.HipJoint.limitsVec3 = newvector3(15.0, 15.0, 15.0);
	
	g.RightShoulderJoint.rotationVec3 = newvector3(0, 0, 0);
	g.RightShoulderJoint.limitsVec3 = newvector3(10, 10, 10);
	g.LeftShoulderJoint.rotationVec3 = newvector3(0, 0, 0);
	g.LeftShoulderJoint.limitsVec3 = newvector3(10, 10, 10);

	g.NeckJoint.rotationVec3 = newvector3(0.0, 90.0, 0.0);
	g.NeckJoint.limitsVec3 = newvector3(30, 10, 10);

	// new ragdoll array
	g.RagdollsMax=0;
	Dim (  t.Ragdolls,g.RagdollsMax );
	t.Ragdolls[g.RagdollsMax].obj=0;
}

void ragdoll_free ( void )
{
	// Create Joint Rotations and Limits
	if ( vector3exist(g.KneeJoint.rotationVec3) ) 
	{
		t.result = deletevector3(g.KneeJoint.rotationVec3);
		t.result = deletevector3(g.KneeJoint.limitsVec2);
		t.result = deletevector3(g.RightElbowJoint.rotationVec3);
		t.result = deletevector3(g.RightElbowJoint.limitsVec2);
		t.result = deletevector3(g.LeftElbowJoint.rotationVec3);
		t.result = deletevector3(g.LeftElbowJoint.limitsVec2);
		t.result = deletevector3(g.rightWristJoint.rotationVec3);
		t.result = deletevector3(g.rightWristJoint.limitsVec2);
		t.result = deletevector3(g.leftWristJoint.rotationVec3);
		t.result = deletevector3(g.leftWristJoint.limitsVec2);
		t.result = deletevector3(g.WaistJoint.rotationVec3);
		t.result = deletevector3(g.WaistJoint.limitsVec2);
		t.result = deletevector3(g.UpperBodyJoint.rotationVec3);
		t.result = deletevector3(g.UpperBodyJoint.limitsVec2);
		t.result = deletevector3(g.HipJoint.rotationVec3);
		t.result = deletevector3(g.HipJoint.limitsVec3);
		t.result = deletevector3(g.RightShoulderJoint.rotationVec3);
		t.result = deletevector3(g.RightShoulderJoint.limitsVec3);
		t.result = deletevector3(g.LeftShoulderJoint.rotationVec3);
		t.result = deletevector3(g.LeftShoulderJoint.limitsVec3);
		t.result = deletevector3(g.NeckJoint.rotationVec3);
		t.result = deletevector3(g.NeckJoint.limitsVec3);
	}
}

void ragdoll_setcollisionmask ( int iMode )
{
	// FLAGS (1-terrain, 2-objects, 4-capsule, 8-dynamic object)
	g.tphys_CollisionGroup.group0 = 8; // new terrain physics does not allow terrain vs terrain, so use new dynamic object ID to mark head/body/upper body extremities
	g.tphys_CollisionGroup.group1 = 2;
	g.tphys_CollisionGroup.group2 = 4;
	if ( iMode == 1 )
	{
		// ragdoll collides with terrain, other entities and player
		g.tphys_CollisionMask.mask0 = 1;
		g.tphys_CollisionMask.mask1 = 2;
		g.tphys_CollisionMask.mask2 = 0; // do not collide ragdoll with player (so player can run through ragdoll)
	}
	else
	{
		// ragdoll collides with terrain, other entities but not player
		g.tphys_CollisionMask.mask0 = 1;
		g.tphys_CollisionMask.mask1 = 2;
		g.tphys_CollisionMask.mask2 = 0;
	}
}

// GG: each ragdoll's limbs as it was made (local position, rotation and scale; world matrix), by its object
struct sRagdollMadeLimbs
{
	std::vector<float> locals;
	std::vector<float> worlds;
};
static std::map<int, sRagdollMadeLimbs> g_RagdollMadeLimbs;

void CalculateCombinedFromQuatsInOriginal(sFrame* pFrame, GGMATRIX* parentMatrix)
{
	if (pFrame)
	{
		// get local quat for this rotation
		GGQUATERNION localInitialPoseQuat = GGQUATERNION(pFrame->matOriginal._11, pFrame->matOriginal._12, pFrame->matOriginal._13, pFrame->matOriginal._14);

		// calc matrix from this rotation
		GGMATRIX matRot;
		GGMatrixRotationQuaternion(&matRot, &localInitialPoseQuat);

		// create world matrix from parent and this local rot
		GGMatrixMultiply(&pFrame->matCombined, &matRot, parentMatrix);

		// do rest of hierarchy
		if (pFrame->pChild) CalculateCombinedFromQuatsInOriginal(pFrame->pChild, &pFrame->matCombined);
		if (pFrame->pSibling) CalculateCombinedFromQuatsInOriginal(pFrame->pSibling, parentMatrix);
	}
}

static void ragdoll_create_core ( void )
{
	// try different BIP designations (ideally BIP01_)
	bool bProducedRagdoll = false;
	for ( int iTryBips = 0; iTryBips < 2; iTryBips++ )
	{
		cstr pBitPrefix = "";
		if ( iTryBips==0 ) pBitPrefix = "Bip01";
		if ( iTryBips==1 ) pBitPrefix = "Bip002";

		//  first ensure the object is ragdollable
		t.tbip01pelvis=getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_Pelvis")).Get() );
		t.tbip01spine=getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_Spine")).Get() );

		//  receives tphyobj
		if (  BPhys_RagdollExist(t.tphyobj) == 0 && t.tbip01pelvis >= 0 && t.tbip01spine >= 0 ) 
		{
			// we have what we need for a ragdoll
			bProducedRagdoll = true;

			// StopAnimation ( immediately and record frame )
			if ( GetPlaying(t.tphyobj) == 1 ) StopObject ( t.tphyobj );

			// before can recreate pose for ragdoll, must extract current animation positions of bones and place
			// back in the object so the code below can extract it to form the ragdoll bone structure
			sObject* pObject = GetObjectData(t.tphyobj);
			if (pObject)
			{
				for (int f = 0; f < pObject->iFrameCount; f++)
				{
					sFrame* pFrame = pObject->ppFrameList[f];
					if (pFrame)
					{
						float fX, fY, fZ;
						float fQuatX, fQuatY, fQuatZ, fQuatW;
						WickedCall_GetLimbLocalPosAndRot(pObject, pFrame->iID, &fX, &fY, &fZ, &fQuatX, &fQuatY, &fQuatZ, &fQuatW);
						RotateLimbQuat (t.tobj, pFrame->iID, fX, fY, fZ, fQuatX, fQuatY, fQuatZ, fQuatW);
					}
				}

				// GG: and each limb as the ragdoll is made from it, for a hold drawn from another pose (ragdoll_setholdreference)
				sRagdollMadeLimbs& made = g_RagdollMadeLimbs[t.tphyobj];
				made.locals.resize(pObject->iFrameCount * 10);
				made.worlds.resize(pObject->iFrameCount * 16);
				for (int f = 0; f < pObject->iFrameCount; f++)
					WickedCall_GetLimbLocalAndWorld(pObject, f, &made.locals[f * 10], &made.worlds[f * 16]);
			}
			// code below will use vecDirection (from matCombined) to work out limb initial rotations
			// so we need to make sure matCombined are set up to reflect the world matrices for the model
			// using the matOriginal quat locals we gathered above to reproduce the pose initial setup
			GGMATRIX matWorld;
			GGMatrixIdentity(&matWorld);
			CalculateCombinedFromQuatsInOriginal(pObject->pFrame, &matWorld);

			// fix pivot to normal and reset manual limb adjustments (done inside 'reset object pivot')
			ResetObjectPivot (t.tphyobj);
			RotateObject (t.tphyobj, 0, ObjectAngleY(t.tphyobj) + 180.0, 0);

			//  switch OFF LOD as ragdoll deforms for some reason (fix in future)
			SetObjectLOD (  t.tphyobj,1,50000 );
			SetObjectLOD (  t.tphyobj,2,50000 );

			//  create the ragdoll bones using the original objects bone positions then
			//  add all the original objects bones that will be moved by the ragdoll bone,
			//  set collisions groups so bones won't collide with other bones.
			Dim ( t.limidrecord , 500) ; for ( t.c = 0 ; t.c <= 500 ; t.c++ ) t.limidrecord[t.c]=0;

			// create ragdoll, and pass in weight from character
			float fWeight = t.entityelement[t.tphye].eleprof.phyweight;
			float fRagdollKg = 175.0f * (fWeight/100.0f);
			// GG: or the kilos SetEntityMass gave the character, or else its .fpe physicsmass
			extern bool ODEGetEntityMassKg(int iEntity, float* pfKg);
			float fSetKg = 0;
			float fProfileKg = t.entityprofile[t.entityelement[t.tphye].bankindex].physicsmass;
			if ( ODEGetEntityMassKg ( t.tphye, &fSetKg ) ) fRagdollKg = fSetKg;
			else if ( fProfileKg > 0 ) fRagdollKg = fProfileKg;
			BPhys_RagDollBegin ( t.tphyobj, fRagdollKg );

			// determine if a crotch exists
			if ( getlimbbyname(t.tphyobj, "Crotch") == -1 ) 
			{
				// if not, add pelvis->spine bone
				if ( t.tbip01pelvis != -1 && t.tbip01spine != -1 ) 
				{
					//g.Pelvis = BPhys_RagDollAddBone(t.tbip01pelvis, t.tbip01spine, 10, g.tphys_CollisionGroup.group2, g.tphys_CollisionMask.mask0 + g.tphys_CollisionMask.mask1 + g.tphys_CollisionMask.mask2);
					g.Pelvis = BPhys_RagDollAddBone(t.tbip01pelvis, t.tbip01spine, 8, g.tphys_CollisionGroup.group2, g.tphys_CollisionMask.mask0 + g.tphys_CollisionMask.mask1 + g.tphys_CollisionMask.mask2);
					t.tlmb = getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_Pelvis")).Get()); 	if (  t.tlmb != -1 ) { BPhys_RagDollBoneAddLimbID (  g.Pelvis, t.tlmb  ) ; t.limidrecord[t.tlmb] = 1; }
					t.tlmb = getlimbbyname(t.tphyobj, pBitPrefix.Get()); if (  t.tlmb != -1 ) { BPhys_RagDollBoneAddLimbID (  g.Pelvis, t.tlmb  ) ; t.limidrecord[t.tlmb] = 1; }
				}
			}
			else
			{
				// if so, add crotch->spine bone
				g.Pelvis = BPhys_RagDollAddBone(getlimbbyname(t.tphyobj, "Crotch"), getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_Spine")).Get()),10,g.tphys_CollisionGroup.group2,g.tphys_CollisionMask.mask0 + g.tphys_CollisionMask.mask1 + g.tphys_CollisionMask.mask2);
				t.tlmb = getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_Pelvis")).Get()) ; if (  t.tlmb != -1 ) { BPhys_RagDollBoneAddLimbID (  g.Pelvis, t.tlmb  ) ; t.limidrecord[t.tlmb] = 1; }
				t.tlmb = getlimbbyname(t.tphyobj, "Crotch") ; if (  t.tlmb != -1 ) { BPhys_RagDollBoneAddLimbID (  g.Pelvis, t.tlmb  ) ; t.limidrecord[t.tlmb] = 1; }
				t.tlmb = getlimbbyname(t.tphyobj, pBitPrefix.Get()) ; if (  t.tlmb != -1 ) { BPhys_RagDollBoneAddLimbID (  g.Pelvis, t.tlmb  ) ; t.limidrecord[t.tlmb] = 1; }
			}

			// check if got spine and neck
			if ( getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_Spine")).Get()) != -1 && getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_Neck")).Get()) != -1 ) 
			{
				// add spine->neckbone
				t.Torso = BPhys_RagDollAddBone(getlimbbyname(t.tphyobj, (pBitPrefix + cstr("_Spine")).Get()), getlimbbyname(t.tphyobj, (pBitPrefix + cstr("_Neck")).Get()), 12, g.tphys_CollisionGroup.group0, g.tphys_CollisionMask.mask0 + g.tphys_CollisionMask.mask1 + g.tphys_CollisionMask.mask2);
				t.UpperSpine = t.Torso;
				t.tlmb = getlimbbyname(t.tphyobj, (pBitPrefix + cstr("_Spine")).Get()); if (t.tlmb != -1) { BPhys_RagDollBoneAddLimbID (t.Torso, t.tlmb); t.limidrecord[t.tlmb] = 1; }
				t.tlmb = getlimbbyname(t.tphyobj, (pBitPrefix + cstr("_Spine1")).Get()); if (t.tlmb != -1) { BPhys_RagDollBoneAddLimbID (t.Torso, t.tlmb); t.limidrecord[t.tlmb] = 1; }
				t.tlmb = getlimbbyname(t.tphyobj, (pBitPrefix + cstr("_Spine2")).Get()); if (t.tlmb != -1) { BPhys_RagDollBoneAddLimbID (t.Torso, t.tlmb); t.limidrecord[t.tlmb] = 1; }
				t.tlmb = getlimbbyname(t.tphyobj, (pBitPrefix + cstr("_L_Clavicle")).Get()); if (t.tlmb != -1) { BPhys_RagDollBoneAddLimbID (t.Torso, t.tlmb); t.limidrecord[t.tlmb] = 1; }
				t.tlmb = getlimbbyname(t.tphyobj, (pBitPrefix + cstr("_R_Clavicle")).Get()); if (t.tlmb != -1) { BPhys_RagDollBoneAddLimbID (t.Torso, t.tlmb); t.limidrecord[t.tlmb] = 1; }
			}

			// add neck->headnub bone
			if ( getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_Neck")).Get()) != -1 ) 
			{
				if ( getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_HeadNub")).Get()) != -1 ) 
				{
					t.Head = BPhys_RagDollAddBone(getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_Neck")).Get()), getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_HeadNub")).Get()), 9,g.tphys_CollisionGroup.group0,g.tphys_CollisionMask.mask0 + g.tphys_CollisionMask.mask1 + g.tphys_CollisionMask.mask2);
					t.tlmb = getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_HeadNub")).Get()) ; if (  t.tlmb != -1 ) { BPhys_RagDollBoneAddLimbID (  t.Head, t.tlmb  ); t.limidrecord[t.tlmb] = 1; }
				}
				else
				{
					t.Head = BPhys_RagDollAddBone(getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_Neck")).Get()), getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_Head")).Get()), 9,g.tphys_CollisionGroup.group0,g.tphys_CollisionMask.mask0 + g.tphys_CollisionMask.mask1 + g.tphys_CollisionMask.mask2);
				}
				t.tlmb = getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_Neck")).Get()) ; if (  t.tlmb != -1 ) { BPhys_RagDollBoneAddLimbID (  t.Head, t.tlmb  ); t.limidrecord[t.tlmb] = 1; }
				t.tlmb = getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_Head")).Get()) ; if (  t.tlmb != -1 ) { BPhys_RagDollBoneAddLimbID (  t.Head, t.tlmb  ); t.limidrecord[t.tlmb] = 1; }
				if ( getlimbbyname(t.tphyobj, "Head_Bone") != -1 ) 
				{
					t.tlmb = getlimbbyname(t.tphyobj, "Head_Bone") ; if (  t.tlmb != -1 ) { BPhys_RagDollBoneAddLimbID (  t.Head, t.tlmb  ); t.limidrecord[t.tlmb] = 1; }
				}
				if ( getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_Ponytail1")).Get()) != -1 ) 
				{
					t.tlmb = getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_Ponytail1")).Get()) ; if (  t.tlmb != -1 ) { BPhys_RagDollBoneAddLimbID (  t.Head, t.tlmb  ); t.limidrecord[t.tlmb] = 1; }
				}
				if ( getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_Ponytail1Nub")).Get()) != -1 ) 
				{
					t.tlmb = getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_Ponytail1Nub")).Get()) ; if (  t.tlmb != -1 ) { BPhys_RagDollBoneAddLimbID (  t.Head, t.tlmb ); t.limidrecord[t.tlmb] = 1; }
				}
			}
			t.tlmb = getlimbbyname(t.tphyobj, "Jaw_Bone") ; if (  t.tlmb != -1 ) { BPhys_RagDollBoneAddLimbID (  t.Head, t.tlmb  ) ; t.limidrecord[t.tlmb] = 1; }
			t.tlmb = getlimbbyname(t.tphyobj, "Jaw_End") ; if (  t.tlmb != -1 ) { BPhys_RagDollBoneAddLimbID (  t.Head, t.tlmb  ) ; t.limidrecord[t.tlmb] = 1; }
			t.tlmb = getlimbbyname(t.tphyobj, "L_Mouth_Down") ; if (  t.tlmb != -1 ) { BPhys_RagDollBoneAddLimbID (  t.Head, t.tlmb  ) ; t.limidrecord[t.tlmb] = 1; }
			t.tlmb = getlimbbyname(t.tphyobj, "R_Mouth_Down") ; if (  t.tlmb != -1 ) { BPhys_RagDollBoneAddLimbID (  t.Head, t.tlmb  ) ; t.limidrecord[t.tlmb] = 1; }
			t.tlmb = getlimbbyname(t.tphyobj, "M_Mouth_Down") ; if (  t.tlmb != -1 ) { BPhys_RagDollBoneAddLimbID (  t.Head, t.tlmb  ) ; t.limidrecord[t.tlmb] = 1; }
			t.tlmb = getlimbbyname(t.tphyobj, "L_Mouth_Up") ; if (  t.tlmb != -1 ) { BPhys_RagDollBoneAddLimbID (  t.Head, t.tlmb  ) ; t.limidrecord[t.tlmb] = 1; }
			t.tlmb = getlimbbyname(t.tphyobj, "R_Mouth_Up") ; if (  t.tlmb != -1 ) { BPhys_RagDollBoneAddLimbID (  t.Head, t.tlmb  ) ; t.limidrecord[t.tlmb] = 1; }
			t.tlmb = getlimbbyname(t.tphyobj, "M_Mouth_Up") ; if (  t.tlmb != -1 ) { BPhys_RagDollBoneAddLimbID (  t.Head, t.tlmb  ) ; t.limidrecord[t.tlmb] = 1; }
			t.tlmb = getlimbbyname(t.tphyobj, "L_Cheek") ; if (  t.tlmb != -1 ) { BPhys_RagDollBoneAddLimbID (  t.Head, t.tlmb  ) ; t.limidrecord[t.tlmb] = 1; }
			t.tlmb = getlimbbyname(t.tphyobj, "R_Cheek") ; if (  t.tlmb != -1 ) { BPhys_RagDollBoneAddLimbID (  t.Head, t.tlmb  ) ; t.limidrecord[t.tlmb] = 1; }
			t.tlmb = getlimbbyname(t.tphyobj, "L_Brow_inner") ; if (  t.tlmb != -1 ) { BPhys_RagDollBoneAddLimbID (  t.Head, t.tlmb  ) ; t.limidrecord[t.tlmb] = 1; }
			t.tlmb = getlimbbyname(t.tphyobj, "R_brow_inner") ; if (  t.tlmb != -1 ) { BPhys_RagDollBoneAddLimbID (  t.Head, t.tlmb  ) ; t.limidrecord[t.tlmb] = 1; }
			t.tlmb = getlimbbyname(t.tphyobj, "L_Brow_outer") ; if (  t.tlmb != -1 ) { BPhys_RagDollBoneAddLimbID (  t.Head, t.tlmb  ) ; t.limidrecord[t.tlmb] = 1; }
			t.tlmb = getlimbbyname(t.tphyobj, "R_brow_outer") ; if (  t.tlmb != -1 ) { BPhys_RagDollBoneAddLimbID (  t.Head, t.tlmb  ) ; t.limidrecord[t.tlmb] = 1; }
			t.tlmb = getlimbbyname(t.tphyobj, "L_Mouth") ; if (  t.tlmb != -1 ) { BPhys_RagDollBoneAddLimbID (  t.Head, t.tlmb  ) ; t.limidrecord[t.tlmb] = 1; }
			t.tlmb = getlimbbyname(t.tphyobj, "R_Mouth") ; if (  t.tlmb != -1 ) { BPhys_RagDollBoneAddLimbID (  t.Head, t.tlmb  ) ; t.limidrecord[t.tlmb] = 1; }
			t.tlmb = getlimbbyname(t.tphyobj, "R_eyelid_up") ; if (  t.tlmb != -1 ) { BPhys_RagDollBoneAddLimbID (  t.Head, t.tlmb  ) ; t.limidrecord[t.tlmb] = 1; }
			t.tlmb = getlimbbyname(t.tphyobj, "L_eyelid_up") ; if (  t.tlmb != -1 ) { BPhys_RagDollBoneAddLimbID (  t.Head, t.tlmb  ) ; t.limidrecord[t.tlmb] = 1; }
			t.tlmb = getlimbbyname(t.tphyobj, "R_eyelid_down") ; if (  t.tlmb != -1 ) { BPhys_RagDollBoneAddLimbID (  t.Head, t.tlmb  ) ; t.limidrecord[t.tlmb] = 1; }
			t.tlmb = getlimbbyname(t.tphyobj, "L_eyelid_down") ; if (  t.tlmb != -1 ) { BPhys_RagDollBoneAddLimbID (  t.Head, t.tlmb  ) ; t.limidrecord[t.tlmb] = 1; }

			// add upperarm->forearm bone
			if ( getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_L_UpperArm")).Get()) != -1 && getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_L_Forearm")).Get()) != -1 ) 
			{
				t.L_UpperArm = BPhys_RagDollAddBone(getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_L_UpperArm")).Get()), getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_L_Forearm")).Get()), 9,g.tphys_CollisionGroup.group0,g.tphys_CollisionMask.mask0 + g.tphys_CollisionMask.mask1 );
				t.tlmb = getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_L_UpperArm")).Get()) ; if (  t.tlmb != -1 ) { BPhys_RagDollBoneAddLimbID (  t.L_UpperArm, t.tlmb  ); t.limidrecord[t.tlmb] = 1; }
			}

			// add forearm->hand bone
			if ( getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_L_Forearm")).Get()) != -1 && getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_L_Hand")).Get()) != -1 ) 
			{
				// make lower arms heavy!
				g.L_Forearm = BPhys_RagDollAddBone2(getlimbbyname(t.tphyobj, (pBitPrefix + cstr("_L_Forearm")).Get()), getlimbbyname(t.tphyobj, (pBitPrefix + cstr("_L_Hand")).Get()), 7, -2, g.tphys_CollisionGroup.group0, g.tphys_CollisionMask.mask0 + g.tphys_CollisionMask.mask2);
				t.tlmb = getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_L_Forearm")).Get()) ; if (  t.tlmb != -1 ) { BPhys_RagDollBoneAddLimbID (  g.L_Forearm, t.tlmb  ) ; t.limidrecord[t.tlmb] = 1; }
			}

			// add hand->finger bone
			if ( getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_L_Hand")).Get()) != -1 ) 
			{
				if ( getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_L_Finger0Nub")).Get()) != -1 ) 
				{
					// make hands heavy!
					g.L_Hand = BPhys_RagDollAddBone2(getlimbbyname(t.tphyobj, (pBitPrefix + cstr("_L_Hand")).Get()), getlimbbyname(t.tphyobj, (pBitPrefix + cstr("_L_Finger0Nub")).Get()), 5, -2, g.tphys_CollisionGroup.group0, g.tphys_CollisionMask.mask0 + g.tphys_CollisionMask.mask1 + g.tphys_CollisionMask.mask2);
					t.tlmb = getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_L_Finger0Nub")).Get()) ; if (  t.tlmb != -1 ) { BPhys_RagDollBoneAddLimbID (  g.L_Hand, t.tlmb  ) ; t.limidrecord[t.tlmb] = 1; }
				}
				else
				{
					if (  getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_L_Finger0")).Get()) != -1 ) 
					{
						// make hands heavy!
						g.L_Hand = BPhys_RagDollAddBone2(getlimbbyname(t.tphyobj, (pBitPrefix + cstr("_L_Hand")).Get()), getlimbbyname(t.tphyobj, (pBitPrefix + cstr("_L_Finger0")).Get()), 5, -2, g.tphys_CollisionGroup.group0, g.tphys_CollisionMask.mask0 + g.tphys_CollisionMask.mask1 + g.tphys_CollisionMask.mask2);
					}
					else
					{
						if (  getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_L_Finger")).Get()) != -1 ) 
						{
							g.L_Hand = BPhys_RagDollAddBone(getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_L_Hand")).Get()), getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_L_Finger")).Get()), 5,g.tphys_CollisionGroup.group0,g.tphys_CollisionMask.mask0 + g.tphys_CollisionMask.mask1 + g.tphys_CollisionMask.mask2);
						}
						else
						{
							g.L_Hand = BPhys_RagDollAddBone(getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_L_Hand")).Get()), getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_L_Finger1")).Get()), 5,g.tphys_CollisionGroup.group0,g.tphys_CollisionMask.mask0 + g.tphys_CollisionMask.mask1 + g.tphys_CollisionMask.mask2);
						}
					}
				}
				if ( g.L_Hand != -1 )
				{
					t.tlmb = getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_L_Hand")).Get()) ; if (  t.tlmb != -1 ) { BPhys_RagDollBoneAddLimbID (  g.L_Hand, t.tlmb  ) ; t.limidrecord[t.tlmb] = 1; }
					t.tlmb = getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_L_Finger0")).Get()) ; if (  t.tlmb != -1 ) { BPhys_RagDollBoneAddLimbID (  g.L_Hand, t.tlmb ) ; t.limidrecord[t.tlmb] = 1; }
					t.tlmb = getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_L_Finger")).Get()) ; if (  t.tlmb != -1 ) { BPhys_RagDollBoneAddLimbID (  g.L_Hand, t.tlmb ) ; t.limidrecord[t.tlmb] = 1; }
				}
			}

			// add calf->thigh bone
			if ( getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_L_Calf")).Get()) != -1 && getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_L_Thigh")).Get()) != -1 ) 
			{
				t.L_Thigh = BPhys_RagDollAddBone(getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_L_Calf")).Get() ), getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_L_Thigh")).Get() ), 8,g.tphys_CollisionGroup.group2,g.tphys_CollisionMask.mask0 + g.tphys_CollisionMask.mask2);
				t.tlmb = getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_L_Thigh")).Get()) ; if (  t.tlmb != -1 ) { BPhys_RagDollBoneAddLimbID (  t.L_Thigh, t.tlmb  ) ; t.limidrecord[t.tlmb] = 1; }
			}

			// check if got heel
			if ( getlimbbyname(t.tphyobj, "l_Heel") == -1 ) 
			{
				// if not, add foot->calf bone
				if ( getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_L_Foot")).Get()) != -1 && getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_L_Calf")).Get()) != -1 ) 
				{
					g.L_Calf = BPhys_RagDollAddBone2(getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_L_Foot")).Get()), getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_L_Calf")).Get()), 8, 1.1f, g.tphys_CollisionGroup.group2,g.tphys_CollisionMask.mask0 + g.tphys_CollisionMask.mask1 + g.tphys_CollisionMask.mask2);
					t.tlmb = getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_L_Calf")).Get()) ; if (  t.tlmb != -1 ) { BPhys_RagDollBoneAddLimbID (  g.L_Calf, t.tlmb  ) ; t.limidrecord[t.tlmb] = 1; }
					t.tlmb = getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_L_Foot")).Get()) ; if (  t.tlmb != -1 ) { BPhys_RagDollBoneAddLimbID (  g.L_Calf, t.tlmb  ) ; t.limidrecord[t.tlmb] = 1; }
				}
			}
			else
			{
				// add heel->calf bone
				if (  getlimbbyname(t.tphyobj, "l_Heel") != -1 && getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_L_Calf")).Get()) != -1 ) 
				{
					g.L_Calf = BPhys_RagDollAddBone(getlimbbyname(t.tphyobj, "l_Heel"), getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_L_Calf")).Get()), 6,g.tphys_CollisionGroup.group2,g.tphys_CollisionMask.mask0 + g.tphys_CollisionMask.mask1 + g.tphys_CollisionMask.mask2);
					t.tlmb = getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_L_Calf")).Get()) ; if (  t.tlmb != -1 ) { BPhys_RagDollBoneAddLimbID (  g.L_Calf, t.tlmb  ) ; t.limidrecord[t.tlmb] = 1; }
					t.tlmb = getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_L_Foot")).Get()) ; if (  t.tlmb != -1 ) { BPhys_RagDollBoneAddLimbID (  g.L_Calf, t.tlmb  ) ; t.limidrecord[t.tlmb] = 1; }
					t.tlmb = getlimbbyname(t.tphyobj, "l_Heel") ; if (  t.tlmb != -1 ) { BPhys_RagDollBoneAddLimbID (  g.L_Calf, t.tlmb  ) ; t.limidrecord[t.tlmb] = 1; }
				}
			}

			// add upperarm->forearm bone
			if ( getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_R_UpperArm")).Get()) != -1 && getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_R_Forearm")).Get()) != -1 ) 
			{
				t.R_UpperArm = BPhys_RagDollAddBone(getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_R_UpperArm")).Get()), getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_R_Forearm")).Get()), 9,g.tphys_CollisionGroup.group0,g.tphys_CollisionMask.mask0 + g.tphys_CollisionMask.mask1 );
				t.tlmb = getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_R_UpperArm")).Get()) ; if (  t.tlmb != -1 ) { BPhys_RagDollBoneAddLimbID (  t.R_UpperArm, t.tlmb  ) ; t.limidrecord[t.tlmb] = 1; }
			}

			// add forearm->hand bone
			if (  getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_R_Forearm")).Get()) != -1 && getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_R_Hand")).Get()) != -1 ) 
			{
				// male lower arms heavy!
				t.R_Forearm = BPhys_RagDollAddBone2(getlimbbyname(t.tphyobj, (pBitPrefix + cstr("_R_Forearm")).Get()), getlimbbyname(t.tphyobj, (pBitPrefix + cstr("_R_Hand")).Get()), 7, -2, g.tphys_CollisionGroup.group0, g.tphys_CollisionMask.mask0 + g.tphys_CollisionMask.mask2);
				t.tlmb = getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_R_Forearm")).Get()) ; if (  t.tlmb != -1 ) { BPhys_RagDollBoneAddLimbID (  t.R_Forearm, t.tlmb  ) ; t.limidrecord[t.tlmb] = 1; }
			}

			// addhand->finger bone
			if ( getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_R_Hand")).Get()) != -1 ) 
			{
				if ( getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_R_Finger0Nub")).Get()) != -1 ) 
				{
					// make hands heavy!
					t.R_Hand = BPhys_RagDollAddBone2(getlimbbyname(t.tphyobj, (pBitPrefix + cstr("_R_Hand")).Get()), getlimbbyname(t.tphyobj, (pBitPrefix + cstr("_R_Finger0Nub")).Get()), 5, -2, g.tphys_CollisionGroup.group0, g.tphys_CollisionMask.mask0 + g.tphys_CollisionMask.mask1 + g.tphys_CollisionMask.mask2);
					t.tlmb = getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_R_Finger0Nub")).Get()) ; if (  t.tlmb != -1 ) { BPhys_RagDollBoneAddLimbID (  t.R_Hand, t.tlmb  ) ; t.limidrecord[t.tlmb] = 1; }
				}
				else
				{
					if ( getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_R_Finger0")).Get()) != -1 ) 
					{
						// make hands heavy!
						t.R_Hand = BPhys_RagDollAddBone2(getlimbbyname(t.tphyobj, (pBitPrefix + cstr("_R_Hand")).Get()), getlimbbyname(t.tphyobj, (pBitPrefix + cstr("_R_Finger0")).Get()), 5, -2, g.tphys_CollisionGroup.group0, g.tphys_CollisionMask.mask0 + g.tphys_CollisionMask.mask1 + g.tphys_CollisionMask.mask2);
					}
					else
					{
						if ( getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_R_Finger")).Get()) != -1 ) 
						{
							t.R_Hand = BPhys_RagDollAddBone(getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_R_Hand")).Get()), getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_R_Finger")).Get()), 5,g.tphys_CollisionGroup.group0,g.tphys_CollisionMask.mask0 + g.tphys_CollisionMask.mask1 + g.tphys_CollisionMask.mask2);
						}
						else
						{
							t.R_Hand = BPhys_RagDollAddBone(getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_R_Hand")).Get()), getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_R_Finger1")).Get()), 5,g.tphys_CollisionGroup.group0,g.tphys_CollisionMask.mask0 + g.tphys_CollisionMask.mask1 + g.tphys_CollisionMask.mask2);
						}
					}
				}
				if ( t.R_Hand != -1 )
				{
					t.tlmb = getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_R_Hand")).Get()) ; if (  t.tlmb != -1 ) { BPhys_RagDollBoneAddLimbID (  t.R_Hand, t.tlmb  ) ; t.limidrecord[t.tlmb] = 1; }
					t.tlmb = getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_R_Finger0")).Get()) ; if (  t.tlmb != -1 ) { BPhys_RagDollBoneAddLimbID (  t.R_Hand, t.tlmb ) ; t.limidrecord[t.tlmb] = 1; };
					if ( getlimbbyname(t.tphyobj, "FIRESPOT") != -1 ) 
					{
						t.tlmb = getlimbbyname(t.tphyobj, "FIRESPOT") ; if (  t.tlmb != -1 ) { BPhys_RagDollBoneAddLimbID (  t.R_Hand, t.tlmb ) ; t.limidrecord[t.tlmb] = 1; }
					}
				}
			}

			// add calf->thigh bone
			if ( getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_R_Calf")).Get()) != -1 ) 
			{
				t.R_Thigh = BPhys_RagDollAddBone(getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_R_Calf")).Get() ), getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_R_Thigh")).Get()), 8,g.tphys_CollisionGroup.group2,g.tphys_CollisionMask.mask0 + g.tphys_CollisionMask.mask2);
				t.tlmb = getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_R_Thigh")).Get()) ; if (  t.tlmb != -1 ) { BPhys_RagDollBoneAddLimbID (  t.R_Thigh, t.tlmb  ) ; t.limidrecord[t.tlmb] = 1; }
			}

			// add heel or foot->calf bone
			if (  getlimbbyname(t.tphyobj, "r_Heel") == -1 ) 
			{
				if (  getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_R_Foot")).Get()) != -1 && getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_R_Calf")).Get()) != -1 ) 
				{
					t.R_Calf = BPhys_RagDollAddBone2(getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_R_Foot")).Get()), getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_R_Calf")).Get()), 8, 1.1f, g.tphys_CollisionGroup.group2,g.tphys_CollisionMask.mask0 + g.tphys_CollisionMask.mask1 + g.tphys_CollisionMask.mask2);
					t.tlmb = getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_R_Calf")).Get()) ; if (  t.tlmb != -1 ) { BPhys_RagDollBoneAddLimbID (  t.R_Calf, t.tlmb  ) ; t.limidrecord[t.tlmb] = 1; }
					t.tlmb = getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_R_Foot")).Get()) ; if (  t.tlmb != -1 ) { BPhys_RagDollBoneAddLimbID (  t.R_Calf, t.tlmb  ) ; t.limidrecord[t.tlmb] = 1; }
				}
			}
			else
			{
				t.R_Calf = BPhys_RagDollAddBone(getlimbbyname(t.tphyobj, "r_Heel"), getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_R_Calf")).Get()), 6,g.tphys_CollisionGroup.group2,g.tphys_CollisionMask.mask0 + g.tphys_CollisionMask.mask1 + g.tphys_CollisionMask.mask2);
				t.tlmb = getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_R_Calf")).Get()) ; if (  t.tlmb != -1 ) { BPhys_RagDollBoneAddLimbID (  t.R_Calf, t.tlmb  ) ; t.limidrecord[t.tlmb] = 1; }
				t.tlmb = getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_R_Foot")).Get()) ; if (  t.tlmb != -1 ) { BPhys_RagDollBoneAddLimbID (  t.R_Calf, t.tlmb  ) ; t.limidrecord[t.tlmb] = 1; }
				t.tlmb = getlimbbyname(t.tphyobj, "r_Heel") ; if (  t.tlmb != -1 ) { BPhys_RagDollBoneAddLimbID (  t.R_Calf, t.tlmb  ) ; t.limidrecord[t.tlmb] = 1; }
			}

			//  Detect limbs not known by name, and add to hierarchy where they where found
			PerformCheckListForLimbs (  t.tphyobj );
			t.modellastknownlimb=g.Pelvis;
			for ( t.c = 1 ; t.c<=  ChecklistQuantity()-1; t.c++ )
			{
				// track known limb names
				t.tokay=-1 ; t.tname_s=Lower(ChecklistString(t.c));
				if (  t.tname_s == cstr(Lower(pBitPrefix.Get()))  )  t.tokay = g.Pelvis;
				if (  t.tname_s == cstr(Lower((pBitPrefix+cstr("_Pelvis")).Get()))  )  t.tokay = g.Pelvis;
				if (  t.tname_s == cstr(Lower((pBitPrefix+cstr("_Spine")).Get()))  )  t.tokay = t.Torso;
				if (  t.tname_s == cstr(Lower((pBitPrefix+cstr("_Spine1")).Get()))  )  t.tokay = t.Torso;
				if (  t.tname_s == cstr(Lower((pBitPrefix+cstr("_Spine2")).Get()))  )  t.tokay = t.Torso;
				if (  t.tname_s == cstr(Lower((pBitPrefix+cstr("_L_Clavicle")).Get()))  )  t.tokay = t.UpperSpine;//t.Torso;
				if (  t.tname_s == cstr(Lower((pBitPrefix+cstr("_R_Clavicle")).Get()))  )  t.tokay = t.UpperSpine;//t.Torso;
				if (  t.tname_s == cstr(Lower((pBitPrefix+cstr("_Neck")).Get()))  )  t.tokay = t.Head;
				if (  t.tname_s == cstr(Lower((pBitPrefix+cstr("_Head")).Get()))  )  t.tokay = t.Head;
				if (  t.tname_s == cstr(Lower((pBitPrefix+cstr("_HeadNub")).Get()))  )  t.tokay = t.Head;
				if (  t.tname_s == cstr(Lower((pBitPrefix+cstr("_Ponytail1")).Get()))  )  t.tokay = t.Head;
				if (  t.tname_s == cstr(Lower((pBitPrefix+cstr("_Ponytail1Nub")).Get()))  )  t.tokay = t.Head;
				if (  t.tname_s == cstr(Lower((pBitPrefix+cstr("_L_UpperArm")).Get()))  )  t.tokay = t.L_UpperArm;
				if (  t.tname_s == cstr(Lower((pBitPrefix+cstr("_L_Forearm")).Get()))  )  t.tokay = t.L_UpperArm;
				if (  t.tname_s == cstr(Lower((pBitPrefix+cstr("_L_Hand")).Get()))  )  t.tokay = g.L_Hand;
				if (  t.tname_s == cstr(Lower((pBitPrefix+cstr("_L_Finger0")).Get()))  )  t.tokay = g.L_Hand;
				if (  t.tname_s == cstr(Lower((pBitPrefix+cstr("_L_Finger0Nub")).Get()))  )  t.tokay = g.L_Hand;
				if (  t.tname_s == cstr(Lower((pBitPrefix+cstr("_L_Thigh")).Get()))  )  t.tokay = t.L_Thigh;
				if (  t.tname_s == cstr(Lower((pBitPrefix+cstr("_L_Calf")).Get()))  )  t.tokay = g.L_Calf;
				if (  t.tname_s == cstr(Lower((pBitPrefix+cstr("_L_Foot")).Get()))  )  t.tokay = g.L_Calf;
				if (  t.tname_s == cstr(Lower("L_Mouth_Up"))  )  t.tokay = t.Head;
				if (  t.tname_s == cstr(Lower("L_Mouth_Down"))  )  t.tokay = t.Head;
				if (  t.tname_s == cstr(Lower("L_Cheek"))  )  t.tokay = t.Head;
				if (  t.tname_s == cstr(Lower("L_Brow_inner"))  )  t.tokay = t.Head;
				if (  t.tname_s == cstr(Lower("L_Brow_outer"))  )  t.tokay = t.Head;
				if (  t.tname_s == cstr(Lower("L_Mouth"))  )  t.tokay = t.Head;
				if (  t.tname_s == cstr(Lower("L_eyelid_up"))  )  t.tokay = t.Head;
				if (  t.tname_s == cstr(Lower("L_eyelid_down"))  )  t.tokay = t.Head;
				if (  t.tname_s == cstr(Lower("l_Heel"))  )  t.tokay = g.L_Calf;
				if (  t.tname_s == cstr(Lower((pBitPrefix+cstr("_R_UpperArm")).Get()))  )  t.tokay = t.R_UpperArm;
				if (  t.tname_s == cstr(Lower((pBitPrefix+cstr("_R_Forearm")).Get()))  )  t.tokay = t.R_Forearm;
				if (  t.tname_s == cstr(Lower((pBitPrefix+cstr("_R_Hand")).Get()))  )  t.tokay = t.R_Hand;
				if (  t.tname_s == cstr(Lower((pBitPrefix+cstr("_R_Finger0")).Get()))  )  t.tokay = t.R_Hand;
				if (  t.tname_s == cstr(Lower((pBitPrefix+cstr("_R_Finger0Nub")).Get()))  )  t.tokay = t.R_Hand;
				if (  t.tname_s == cstr(Lower((pBitPrefix+cstr("_R_Thigh")).Get()))  )  t.tokay = t.R_Thigh;
				if (  t.tname_s == cstr(Lower((pBitPrefix+cstr("_R_Calf")).Get()))  )  t.tokay = t.R_Calf;
				if (  t.tname_s == cstr(Lower((pBitPrefix+cstr("_R_Foot")).Get()))  )  t.tokay = t.R_Calf;
				if (  t.tname_s == cstr(Lower("R_Mouth_Up"))  )  t.tokay = t.Head;
				if (  t.tname_s == cstr(Lower("R_Mouth_Down"))  )  t.tokay = t.Head;
				if (  t.tname_s == cstr(Lower("R_Cheek"))  )  t.tokay = t.Head;
				if (  t.tname_s == cstr(Lower("R_brow_inner"))  )  t.tokay = t.Head;
				if (  t.tname_s == cstr(Lower("R_brow_outer"))  )  t.tokay = t.Head;
				if (  t.tname_s == cstr(Lower("R_Mouth"))  )  t.tokay = t.Head;
				if (  t.tname_s == cstr(Lower("R_eyelid_up"))  )  t.tokay = t.Head;
				if (  t.tname_s == cstr(Lower("R_eyelid_down"))  )  t.tokay = t.Head;
				if (  t.tname_s == cstr(Lower("r_Heel"))  )  t.tokay = t.R_Calf;
				if (  t.tname_s == cstr(Lower("Crotch"))  )  t.tokay = g.Pelvis;
				if (  t.tname_s == cstr(Lower("Head_Bone"))  )  t.tokay = t.Head;
				if (  t.tname_s == cstr(Lower("Jaw_Bone"))  )  t.tokay = t.Head;
				if (  t.tname_s == cstr(Lower("Jaw_End"))  )  t.tokay = t.Head;
				if (  t.tname_s == cstr(Lower("M_Mouth_Up"))  )  t.tokay = t.Head;
				if (  t.tname_s == cstr(Lower("M_Mouth_Down"))  )  t.tokay = t.Head;
				if (  t.tname_s == cstr(Lower("FIRESPOT"))  )  t.tokay = t.R_Hand;
				if (  t.tokay != -1 ) 
				{
					//  limb context recorded
					t.modellastknownlimb=t.tokay;
				}
				else
				{
					//  if limb not recognised, add to complete ragdoll
					t.tlmb = getlimbbyname(t.tphyobj, t.tname_s.Get()) ; if (  t.tlmb != -1 ) { BPhys_RagDollBoneAddLimbID (  t.modellastknownlimb, t.tlmb  ) ; t.limidrecord[t.tlmb] = 1; }
				}
					// 221215 - this code below should NEVER work, check for zero, then check for one..
				if (  t.limidrecord[t.c-1] == 0 ) 
				{
					t.tlmb = t.c-1 ; if (  t.limidrecord[t.tlmb] == 1  )  BPhys_RagDollBoneAddLimbID (  t.modellastknownlimb,t.tlmb );
				}
			}

			//  Now we create a joint between the bones we created, joints are positioned at the original objects joints and set the rotation and limits (pBitPrefix handles mulitle bone name prefixes)
			if ( getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_Spine")).Get()) != -1  )  BPhys_RagDollAddHingeJoint ( g.Pelvis, t.Torso, getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_Spine")).Get()), g.WaistJoint.rotationVec3, g.WaistJoint.limitsVec2 );
			if (getlimbbyname(t.tphyobj, (pBitPrefix + cstr("_Neck")).Get()) != -1)  BPhys_RagDollAddTwistJoint (t.Torso, t.Head, getlimbbyname(t.tphyobj, (pBitPrefix + cstr("_Neck")).Get()), g.NeckJoint.rotationVec3, g.NeckJoint.limitsVec3);
			if ( getlimbbyname(t.tphyobj, (pBitPrefix + cstr("_L_UpperArm")).Get()) != -1)  BPhys_RagDollAddTwistJoint (t.Torso, t.L_UpperArm, getlimbbyname(t.tphyobj, (pBitPrefix + cstr("_L_UpperArm")).Get()), g.LeftShoulderJoint.rotationVec3, g.LeftShoulderJoint.limitsVec3);
			if ( getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_L_Forearm")).Get()) != -1  )  BPhys_RagDollAddHingeJoint (  t.L_UpperArm, g.L_Forearm, getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_L_Forearm")).Get()), g.LeftElbowJoint.rotationVec3, g.LeftElbowJoint.limitsVec2 );
			if ( getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_R_Forearm")).Get()) != -1  )  BPhys_RagDollAddHingeJoint (  t.R_UpperArm, t.R_Forearm, getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_R_Forearm")).Get()), g.RightElbowJoint.rotationVec3, g.RightElbowJoint.limitsVec2 );
			if ( getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_L_Hand")).Get()) != -1  )  BPhys_RagDollAddHingeJoint (  g.L_Forearm, g.L_Hand, getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_L_Hand")).Get()), g.leftWristJoint.rotationVec3, g.leftWristJoint.limitsVec2 );
			if ( getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_R_Hand")).Get()) != -1  )  BPhys_RagDollAddHingeJoint (  t.R_Forearm, t.R_Hand, getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_R_Hand")).Get()), g.rightWristJoint.rotationVec3, g.rightWristJoint.limitsVec2 );
			if ( getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_L_Thigh")).Get()) != -1  )  BPhys_RagDollAddTwistJoint (  g.Pelvis, t.L_Thigh, getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_L_Thigh")).Get()), g.HipJoint.rotationVec3, g.HipJoint.limitsVec3 );
			if (getlimbbyname(t.tphyobj, (pBitPrefix + cstr("_R_UpperArm")).Get()) != -1)  BPhys_RagDollAddTwistJoint (t.Torso, t.R_UpperArm, getlimbbyname(t.tphyobj, (pBitPrefix + cstr("_L_UpperArm")).Get()), g.RightShoulderJoint.rotationVec3, g.RightShoulderJoint.limitsVec3);
			if ( getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_R_Thigh")).Get()) != -1  )  BPhys_RagDollAddTwistJoint (  g.Pelvis, t.R_Thigh, getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_R_Thigh")).Get()), g.HipJoint.rotationVec3, g.HipJoint.limitsVec3 );
			if ( getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_L_Calf")).Get()) != -1  )  BPhys_RagDollAddHingeJoint (  t.L_Thigh, g.L_Calf, getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_L_Calf")).Get()), g.KneeJoint.rotationVec3, g.KneeJoint.limitsVec2 );
			if ( getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_R_Calf")).Get()) != -1  )  BPhys_RagDollAddHingeJoint (  t.R_Thigh, t.R_Calf, getlimbbyname(t.tphyobj, (pBitPrefix+cstr("_R_Calf")).Get()), g.KneeJoint.rotationVec3, g.KneeJoint.limitsVec2 );

			// Remaining ragdoll settings (LB: ragdoll could freeze mid-slide, looks daft)
			BPhys_RagDollSetDamping (0.08f, 0.95f);
			BPhys_RagDollSetSleepingThresholds (1.8f, 2.8f);
			BPhys_RagDollSetDeactivationTime (0.2f);

			bool bForTestingInstentFreeze = true;
			if (g_bDebugRagdoll == false) bForTestingInstentFreeze = false;
			if (bForTestingInstentFreeze == true)
			{
				HideObject(t.tphyobj);
			}

			//  Complete ragdoll creation
			BPhys_RagDollEnd (  );
			UnDim (  t.limidrecord );

			// GG: each body's turn with the character facing angle 0, for holding a body (ragdoll_hold)
			BPhys_RagDollSetRestYaw ( t.tphyobj, t.entityelement[t.tphye].ry );

			// ensure never going into the floor!
			if (bForTestingInstentFreeze == false)
			{
				// if not testing ragdoll
				BPhys_RagDollApplyForce (t.tphyobj, -1, 0, 0, 0, 0, 100, 0, 2000000);
			}

			//  add to ragdoll database
			for ( t.ri = 0 ; t.ri<=  g.RagdollsMax; t.ri++ )
			{
				if (  t.Ragdolls[t.ri].obj == 0  )  break;
			}
			if (  t.ri>g.RagdollsMax ) 
			{
				++g.RagdollsMax;
				g.RagdollsMax=g.RagdollsMax*2;
				Dim (  t.Ragdolls,g.RagdollsMax );
			}
			t.Ragdolls[t.ri].obj = t.tphyobj;
		
			//  hide debug ragdoll capsules
			if ( g_bDebugRagdoll == true )
				t.temp = BPhys_RagDollShowBones(t.tphyobj);
			else
				t.temp = BPhys_RagDollHideBones(t.tphyobj);
		}

		// once we have a working ragdoll, we leave
		if ( bProducedRagdoll==true ) return;
	}
}

// GG: each creation timed into the log (a blast's deaths can make several in one frame)
void ragdoll_create ( void )
{
	LARGE_INTEGER freq, start, end;
	QueryPerformanceFrequency ( &freq );
	QueryPerformanceCounter ( &start );
	const int iObj = t.tphyobj;
	const bool bHad = BPhys_RagdollExist ( iObj ) != 0;
	ragdoll_create_core ();
	QueryPerformanceCounter ( &end );
	if ( bHad ) return;
	char pMsg[128];
	sprintf_s ( pMsg, "ragdoll_create obj %d: %s in %.2f ms", iObj, BPhys_RagdollExist ( iObj ) ? "made" : "none", (double)(end.QuadPart - start.QuadPart) * 1000.0 / (double)freq.QuadPart );
	timestampactivity ( 0, pMsg );
}

void ragdoll_destroy ( void )
{
	// receives tphyobj
	if ( t.tphyobj>0 )
	{
		g_RagdollMadeLimbs.erase ( t.tphyobj );
		if ( ObjectExist(t.tphyobj) == 1 ) ResetObjectPivot ( t.tphyobj );
		for ( t.ri = 0 ; t.ri <= g.RagdollsMax; t.ri++ )
		{
			if ( t.tphyobj == t.Ragdolls[t.ri].obj ) 
			{
				// also need to free wicked object so the bones are not being overridden
				sObject* pObject = GetObjectData(t.tphyobj);
				if (pObject)
				{
					for (int iF = 1; iF < pObject->iFrameCount; iF++)
					{
						sFrame* pFrame = pObject->ppFrameList[iF];
						if (pFrame)
						{
							WickedCall_OverrideLimbOff(pObject, pFrame);
						}
					}
				}

				// delete ragdoll from bullet
				if (BPhys_DeleteRagdoll(t.tphyobj) == 1)
				{
					t.Ragdolls[t.ri].obj = 0; 
					t.ri = g.RagdollsMax; 
					break;
				}
			}
		}
	}
}

int newvector3 ( int x_f, int y_f, int z_f )
{
	int r = 0;
	++g.ragdollvectorindex;
	r=MakeVector3(g.ragdollvectorindex);
	if (  ArrayCount(t.ragdollvector) < g.ragdollvectorindex ) 
	{
		Dim (  t.ragdollvector,g.ragdollvectorindex  );
	}
	t.ragdollvector[g.ragdollvectorindex]=1;
	SetVector3 (  g.ragdollvectorindex,x_f,y_f,z_f );
	return g.ragdollvectorindex;
}

int newvector2 ( int x_f, int y_f )
{
	int r = 0;
	++g.ragdollvectorindex;
	r=MakeVector2(g.ragdollvectorindex);
	if (  ArrayCount(t.ragdollvector) < g.ragdollvectorindex ) 
	{
		Dim (  t.ragdollvector,g.ragdollvectorindex  );
	}
	t.ragdollvector[g.ragdollvectorindex]=2;
	SetVector2 (  g.ragdollvectorindex,x_f,y_f );
	return g.ragdollvectorindex;
}

int vector3exist ( int vec )
{
	int exists = 0;
	exists=0;
	if (  vec <= ArrayCount(t.ragdollvector) ) 
	{
		if (  t.ragdollvector[vec] != 0 ) 
		{
			exists=1;
		}
	}
	return exists;
}

int deletevector3 ( int vec )
{
	int result = 0;
	if (  t.ragdollvector[vec] == 1 ) 
	{
		result=DeleteVector3(vec);
	}
	if (  t.ragdollvector[vec] == 2 ) 
	{
		result=DeleteVector2(vec);
	}
	t.ragdollvector[vec]=0;
	return result;
}

int getlimbbyname ( int objid, char* limbname_s )
{	
	int limbindex = 0;
	int c = 0;
	limbindex=-1;
	cstr limbname;
	limbname=Lower(limbname_s);
	PerformCheckListForLimbs (  objid );
	for ( c = 1 ; c<=  ChecklistQuantity(); c++ )
	{
		if (  cstr(Lower(ChecklistString(c))) == limbname ) 
		{
			limbindex=c-1 ; return limbindex;
		}
	}
	return limbindex;
}

// GG: a living character made a ragdoll keeps its health and script (SetEntityRagdoll). ragdoll_create writes the pose
// into each frame's original matrix and resets the pivot, so what it changes is kept here and put back at the wake
struct sRagdollKnockoutFrame
{
	GGMATRIX matOriginal;
	GGMATRIX matTransformed;
	GGMATRIX matCombined;
	GGMATRIX matUserMatrix;
	GGVECTOR3 vecDirection;
};

struct sRagdollKnockout
{
	int iObj;
	int iDisableAsCharacter;
	bool bHadPhysics;
	std::vector<sRagdollKnockoutFrame> frames;
	bool bApplyPivot;
	GGMATRIX matPivot;
	sCollisionData collision;
};

static std::map<int, sRagdollKnockout> g_RagdollKnockouts;

// a limb of the ragdoll skeleton, Bip01_ or Bip002_ as ragdoll_create takes them
static int ragdoll_skeletonlimb ( int iObj, const char* pPart )
{
	char pLimbName[64];
	sprintf ( pLimbName, "Bip01%s", pPart );
	int iLimb = getlimbbyname ( iObj, pLimbName );
	if ( iLimb >= 0 ) return iLimb;
	sprintf ( pLimbName, "Bip002%s", pPart );
	return getlimbbyname ( iObj, pLimbName );
}

// where the ragdoll body moving a limb is
static bool ragdoll_limbposition ( int iObj, int iLimb, GGVECTOR3* pPosition )
{
	if ( iLimb < 0 ) return false;
	int iBone = BPhys_RagDollFindBone ( iObj, iLimb );
	if ( iBone < 0 ) return false;
	return BPhys_RagDollGetBonePosition ( iObj, iBone, &pPosition->x, &pPosition->y, &pPosition->z );
}

static int ragdoll_charanimindexof ( int e )
{
	for ( int i = 1; i <= g.charanimindexmax; i++ )
		if ( t.charanimstates[i].e == e ) return i;
	for ( int i = 1; i <= g.charanimindexmax; i++ )
		if ( t.charanimstates[i].originale == e ) return i;
	return -1;
}

bool ragdoll_isknockedout ( int e )
{
	return g_RagdollKnockouts.find ( e ) != g_RagdollKnockouts.end();
}

// a living character becomes a ragdoll as at a death, but keeps its health and script: out of the character system and
// without its capsule, its object following the pelvis. Pushed along (fPushX, fPushY, fPushZ) by fPushForce (as
// SetEntityRagdollForce's). 1 when made
int ragdoll_knockout ( int e, float fPushX, float fPushY, float fPushZ, float fPushForce )
{
	if ( e <= 0 || e > g.entityelementlist ) return 0;
	int iObj = t.entityelement[e].obj;
	int iEntID = t.entityelement[e].bankindex;
	if ( iObj <= 0 || ObjectExist ( iObj ) == 0 || iEntID <= 0 ) return 0;
	if ( t.entityprofile[iEntID].ischaracter == 0 || t.entityprofile[iEntID].ragdoll == 0 || t.entityelement[e].health <= 0 ) return 0;
	if ( BPhys_RagdollExist ( iObj ) == 1 ) return 0;
	sObject* pObject = GetObjectData ( iObj );
	if ( !pObject ) return 0;

	// what the ragdoll changes in the object, for the wake
	sRagdollKnockout knockout;
	knockout.iObj = iObj;
	knockout.frames.resize ( pObject->iFrameCount );
	for ( int f = 0; f < pObject->iFrameCount; f++ )
	{
		sFrame* pFrame = pObject->ppFrameList[f];
		if ( !pFrame ) continue;
		knockout.frames[f].matOriginal = pFrame->matOriginal;
		knockout.frames[f].matTransformed = pFrame->matTransformed;
		knockout.frames[f].matCombined = pFrame->matCombined;
		knockout.frames[f].matUserMatrix = pFrame->matUserMatrix;
		knockout.frames[f].vecDirection = pFrame->vecDirection;
	}
	knockout.bApplyPivot = pObject->position.bApplyPivot;
	knockout.matPivot = pObject->position.matPivot;
	knockout.collision = pObject->collision;

	// the ragdoll, from the pose he has now (ragdoll_create's limb rotations use tobj)
	int iStoreTObj = t.tobj;
	ragdoll_setcollisionmask ( t.entityelement[e].eleprof.colondeath );
	t.tphye = e; t.tphyobj = iObj; t.tobj = iObj;
	ragdoll_create ( );
	t.tobj = iStoreTObj;
	if ( BPhys_RagdollExist ( iObj ) == 0 ) return 0;

	// out of the character system, without the capsule the ragdoll's bodies would be made inside
	knockout.iDisableAsCharacter = t.entityelement[e].eleprof.disableascharacter;
	t.entityelement[e].eleprof.disableascharacter = 1;
	knockout.bHadPhysics = t.entityelement[e].usingphysicsnow != 0;
	if ( knockout.bHadPhysics )
	{
		t.tphyobj = iObj; physics_disableobject ( );
		t.entityelement[e].usingphysicsnow = 0;
	}
	int iCharIndex = ragdoll_charanimindexof ( e );
	if ( iCharIndex > 0 )
	{
		// no path left to walk or turn to finish when he wakes
		t.charanimstates[iCharIndex].pathPointCount = 0;
		t.charanimstates[iCharIndex].moveToMode = 0;
		t.charanimstates[iCharIndex].requiremovementnow = 0;
		t.charanimstates[iCharIndex].remainingMoveDistanceOnPath_f = 0;
		t.charanimstates[iCharIndex].movespeed_f = 0;
		t.charanimstates[iCharIndex].currentangleslowlyspeed_f = 0;
	}
	// nothing left in the smooth animation system to play or stop an animation under the ragdoll
	t.smoothanim[iObj].transition = 0;
	t.smoothanim[iObj].playflag = 0;

	// the push, given over the next frames as a death's is (entity loop); a force kept from an earlier hit is dropped
	t.entityelement[e].ragdollified = 1;
	t.entityelement[e].ragdollifiedforcex_f = fPushX;
	t.entityelement[e].ragdollifiedforcey_f = fPushY;
	t.entityelement[e].ragdollifiedforcez_f = fPushZ;
	t.entityelement[e].ragdollifiedforcelimb = -1;
	bool bPush = fPushForce > 0.0f && ( fPushX != 0.0f || fPushY != 0.0f || fPushZ != 0.0f );
	t.entityelement[e].ragdollifiedforcevalue_f = bPush ? fPushForce * 8000.0f : 0.0f;

	// his object, and so the entity's position, stays with the pelvis
	BPhys_RagDollSetFollowBone ( iObj, BPhys_RagDollFindBone ( iObj, ragdoll_skeletonlimb ( iObj, "_Pelvis" ) ) );
	g_RagdollKnockouts[e] = knockout;
	return 1;
}

// where a ragdoll lies: the pelvis, the angle from it to the head, face up (chest to the sky), all bodies still, held
bool ragdoll_getpose ( int e, float* pX, float* pY, float* pZ, float* pHeadAngle, int* pFaceUp, int* pResting, int* pHeld )
{
	if ( e <= 0 || e > g.entityelementlist ) return false;
	int iObj = t.entityelement[e].obj;
	if ( iObj <= 0 || ObjectExist ( iObj ) == 0 || BPhys_RagdollExist ( iObj ) == 0 ) return false;
	GGVECTOR3 vecPelvis, vecHead, vecLeft, vecRight;
	if ( !ragdoll_limbposition ( iObj, ragdoll_skeletonlimb ( iObj, "_Pelvis" ), &vecPelvis ) ) return false;
	*pX = vecPelvis.x;
	*pY = vecPelvis.y;
	*pZ = vecPelvis.z;
	*pHeadAngle = 0.0f;
	*pFaceUp = 0;
	if ( ragdoll_limbposition ( iObj, ragdoll_skeletonlimb ( iObj, "_Head" ), &vecHead ) )
	{
		GGVECTOR3 vecUp = vecHead - vecPelvis;
		*pHeadAngle = GGToDegree ( atan2 ( vecUp.x, vecUp.z ) );
		bool bSides = ragdoll_limbposition ( iObj, ragdoll_skeletonlimb ( iObj, "_L_UpperArm" ), &vecLeft ) && ragdoll_limbposition ( iObj, ragdoll_skeletonlimb ( iObj, "_R_UpperArm" ), &vecRight );
		if ( !bSides ) bSides = ragdoll_limbposition ( iObj, ragdoll_skeletonlimb ( iObj, "_L_Thigh" ), &vecLeft ) && ragdoll_limbposition ( iObj, ragdoll_skeletonlimb ( iObj, "_R_Thigh" ), &vecRight );
		if ( bSides )
		{
			// standing, his right (X) by his up (Y) is the way he faces (Z), so the chest's way is right x up
			GGVECTOR3 vecRightward = vecRight - vecLeft;
			float fChestY = ( vecRightward.z * vecUp.x ) - ( vecRightward.x * vecUp.z );
			*pFaceUp = fChestY > 0.0f ? 1 : 0;
		}
	}
	*pResting = BPhys_RagDollIsResting ( iObj );
	*pHeld = BPhys_RagDollIsHeld ( iObj );
	return true;
}

// a knocked out character stands where his body lies: the ragdoll goes, the object is put back as before it, and the
// character system and capsule are his again. Facing fAngle, or the way he gets up (face up, toward his feet; face down,
// toward his head). 1 when woken; the caller then plays the animation he gets up with
int ragdoll_wake ( int e, bool bAngleGiven, float fAngle )
{
	std::map<int, sRagdollKnockout>::iterator it = g_RagdollKnockouts.find ( e );
	if ( it == g_RagdollKnockouts.end() ) return 0;
	float fX, fY, fZ, fHeadAngle;
	int iFaceUp, iResting, iHeld;
	if ( t.entityelement[e].obj != it->second.iObj || !ragdoll_getpose ( e, &fX, &fY, &fZ, &fHeadAngle, &iFaceUp, &iResting, &iHeld ) ) return 0;
	sRagdollKnockout knockout = it->second;
	g_RagdollKnockouts.erase ( it );
	int iObj = knockout.iObj;
	float fStandAngle = bAngleGiven ? fAngle : ( iFaceUp == 1 ? fHeadAngle + 180.0f : fHeadAngle );

	// the ragdoll goes (and any hold with it), and the object is as before it
	t.tphyobj = iObj; ragdoll_destroy ( );
	sObject* pObject = GetObjectData ( iObj );
	if ( pObject )
	{
		if ( (int)knockout.frames.size() == pObject->iFrameCount )
		{
			for ( int f = 0; f < pObject->iFrameCount; f++ )
			{
				sFrame* pFrame = pObject->ppFrameList[f];
				if ( !pFrame ) continue;
				pFrame->matOriginal = knockout.frames[f].matOriginal;
				pFrame->matTransformed = knockout.frames[f].matTransformed;
				pFrame->matCombined = knockout.frames[f].matCombined;
				pFrame->matUserMatrix = knockout.frames[f].matUserMatrix;
				pFrame->vecDirection = knockout.frames[f].vecDirection;
			}
		}
		pObject->position.bApplyPivot = knockout.bApplyPivot;
		pObject->position.matPivot = knockout.matPivot;
		pObject->collision = knockout.collision;
	}
	t.entityelement[e].ragdollified = 0;
	t.entityelement[e].ragdollifiedforcevalue_f = 0.0f;

	// on the surface under the pelvis (the ray goes after the ragdoll, so its bodies can't be hit)
	float fGroundY = BT_GetGroundHeight ( 0, fX, fZ );
	int iCollisionMode = (1 << (0)) | (1 << (1)) | (1 << (3)); //COL_TERRAIN | COL_OBJECT | COL_OBJECT_DYNAMIC;
	if ( ODERayTerrainEx ( fX, fY + 50.0f, fZ, fX, fY - 500.0f, fZ, iCollisionMode, false ) == 1 ) fGroundY = ODEGetRayCollisionY ( );
	t.entityelement[e].x = fX;
	t.entityelement[e].y = fGroundY;
	t.entityelement[e].z = fZ;
	t.entityelement[e].ry = fStandAngle;
	PositionObject ( iObj, fX, fGroundY, fZ );
	RotateObject ( iObj, t.entityelement[e].rx, fStandAngle, t.entityelement[e].rz );
	int iEntID = t.entityelement[e].bankindex;
	if ( t.entityprofile[iEntID].addhandlelimb == 0 ) entity_calculateentityLODdistances ( iEntID, iObj, t.entityelement[e].eleprof.lodmodifier );

	// a character again, facing the way he stands and settling on the surface
	t.entityelement[e].eleprof.disableascharacter = knockout.iDisableAsCharacter;
	int iCharIndex = ragdoll_charanimindexof ( e );
	if ( iCharIndex > 0 )
	{
		t.charanimstates[iCharIndex].currentangle_f = fStandAngle;
		t.charanimstates[iCharIndex].moveangle_f = fStandAngle;
		t.charanimstates[iCharIndex].currentangleslowlyspeed_f = 0;
		t.charanimstates[iCharIndex].warmupcharacteratstart_f = 1.0f;
	}
	if ( knockout.bHadPhysics )
	{
		int iStoreE = t.e;
		t.e = e; entity_lua_collisionon ( );
		t.e = iStoreE;
	}
	return 1;
}

// holds a body of a ragdoll (the pelvis, or the one moving pLimbName) at a point, turned as the character would be by
// the angles: 0,a,0 as standing facing a; 90,a,0 face down with the head toward a; -90,a,0 face up with the feet toward
// a. Called each frame while held; the object follows the pelvis. 1 when held
int ragdoll_hold ( int e, float fX, float fY, float fZ, float fAX, float fAY, float fAZ, const char* pLimbName )
{
	if ( e <= 0 || e > g.entityelementlist ) return 0;
	int iObj = t.entityelement[e].obj;
	if ( iObj <= 0 || ObjectExist ( iObj ) == 0 || BPhys_RagdollExist ( iObj ) == 0 ) return 0;
	int iPelvisBone = BPhys_RagDollFindBone ( iObj, ragdoll_skeletonlimb ( iObj, "_Pelvis" ) );
	int iBone = iPelvisBone;
	if ( pLimbName && pLimbName[0] ) iBone = BPhys_RagDollFindBone ( iObj, getlimbbyname ( iObj, (char*)pLimbName ) );
	if ( iBone < 0 ) return 0;
	BPhys_RagDollSetFollowBone ( iObj, iPelvisBone );
	bool bWasHeld = BPhys_RagDollIsHeld ( iObj ) == 1;
	bool bHeld = BPhys_RagDollHold ( iObj, iBone, fX, fY, fZ, fAX, fAY, fAZ, timeGetTime() );
	if ( bHeld && !bWasHeld )
	{
		// GG: each hold's knees into the log, their bend in the pose the drape starts from (as made, one knocked out
		// kneeling has his bent, or the reference) and in the drape
		float fMade[4], fHeld[4];
		int iKnees = BPhys_RagDollGetHoldKnees ( iObj, fMade, fHeld, 4 );
		char pMsg[256];
		int iLen = sprintf_s ( pMsg, "ragdoll_hold obj %d (%s): %d knees", iObj, BPhys_RagDollHasHoldReference ( iObj ) ? "reference" : "as made", iKnees );
		for ( int k = 0; k < iKnees && iLen > 0 && iLen < (int)sizeof(pMsg) - 40; k++ )
			iLen += sprintf_s ( pMsg + iLen, sizeof(pMsg) - iLen, ", made %.0f held %.0f deg", fMade[k], fHeld[k] );
		timestampactivity ( 0, pMsg );
	}
	return bHeld ? 1 : 0;
}

void ragdoll_release ( int e )
{
	if ( e <= 0 || e > g.entityelementlist ) return;
	int iObj = t.entityelement[e].obj;
	if ( iObj > 0 && BPhys_RagdollExist ( iObj ) == 1 ) BPhys_RagDollRelease ( iObj );
}

// GG: the pose a held ragdoll's drape starts from: e's own animation clip at a point through it (0 its first frame, 1
// its last), each body turned and moved as its limbs are from the pose the ragdoll was made in to that one; with no
// clip, the pose it was made in again. 1 when set
int ragdoll_setholdreference ( int e, const char* pClip, float fFraction )
{
	if ( e <= 0 || e > g.entityelementlist ) return 0;
	int iObj = t.entityelement[e].obj;
	if ( iObj <= 0 || ObjectExist ( iObj ) == 0 || BPhys_RagdollExist ( iObj ) == 0 ) return 0;
	if ( !pClip || !pClip[0] ) return BPhys_RagDollSetHoldReference ( iObj, NULL, NULL, 0 ) ? 1 : 0;
	float fStart = -1, fFinish = -1;
	extern int entity_lua_getanimationname ( int e, cstr FindThisName_s, float* fFoundStart, float* fFoundFinish );
	if ( entity_lua_getanimationname ( e, cstr((char*)pClip), &fStart, &fFinish ) <= 0 || fStart < 0 ) return 0;
	sObject* pObject = GetObjectData ( iObj );
	std::map<int, sRagdollMadeLimbs>::iterator it = g_RagdollMadeLimbs.find ( iObj );
	if ( !pObject || it == g_RagdollMadeLimbs.end() ) return 0;
	const sRagdollMadeLimbs& made = it->second;
	int iLimbs = pObject->iFrameCount;
	if ( (int)made.locals.size() != iLimbs * 10 || (int)made.worlds.size() != iLimbs * 16 ) return 0;
	if ( fFraction < 0.0f ) fFraction = 0.0f;
	if ( fFraction > 1.0f ) fFraction = 1.0f;
	float fFrame = fStart + ( ( fFinish - fStart ) * fFraction );
	std::vector<float> reference ( iLimbs * 16 );
	if ( !WickedCall_GetLimbWorldsAtFrame ( pObject, fFrame, made.locals.data(), made.worlds.data(), reference.data() ) ) return 0;
	bool bSet = BPhys_RagDollSetHoldReference ( iObj, made.worlds.data(), reference.data(), iLimbs );
	char pMsg[256];
	sprintf_s ( pMsg, "ragdoll_holdreference obj %d: %s frame %.1f (%.1f to %.1f) %s", iObj, pClip, fFrame, fStart, fFinish, bSet ? "set" : "refused" );
	timestampactivity ( 0, pMsg );
	return bSet ? 1 : 0;
}

// each frame after physics: a knocked out character's entity is where his body is, and he stays out of the character
// system; one that has died is left a ragdoll of the dead, and one whose ragdoll has gone is dropped
void ragdoll_knockoutloop ( void )
{
	for ( std::map<int, sRagdollKnockout>::iterator it = g_RagdollKnockouts.begin(); it != g_RagdollKnockouts.end(); )
	{
		int e = it->first;
		int iObj = it->second.iObj;
		bool bRagdoll = t.entityelement[e].obj == iObj && ObjectExist ( iObj ) == 1 && BPhys_RagdollExist ( iObj ) == 1;
		if ( !bRagdoll || t.entityelement[e].health <= 0 )
		{
			t.entityelement[e].eleprof.disableascharacter = it->second.iDisableAsCharacter;
			it = g_RagdollKnockouts.erase ( it );
			continue;
		}
		t.entityelement[e].x = ObjectPositionX ( iObj );
		t.entityelement[e].y = ObjectPositionY ( iObj );
		t.entityelement[e].z = ObjectPositionZ ( iObj );
		t.entityelement[e].eleprof.disableascharacter = 1;
		++it;
	}
}

// at a level's start and end; the level's own reset restores the objects
void ragdoll_knockoutclear ( void )
{
	for ( std::map<int, sRagdollKnockout>::iterator it = g_RagdollKnockouts.begin(); it != g_RagdollKnockouts.end(); ++it )
	{
		if ( it->first > 0 && it->first <= g.entityelementlist ) t.entityelement[it->first].eleprof.disableascharacter = it->second.iDisableAsCharacter;
	}
	g_RagdollKnockouts.clear();
}
