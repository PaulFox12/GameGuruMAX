// BulletPhysicsWrapper for DarkBasic Proffessional
// Stab In The Dark Software 
// Copyright (c) 2002-2014
// http://stabinthedarksoftware.com

///#include "StdAfx.h"
#include "DBProRagDoll.h"
#include "DBProToBullet.h"
#include "DBProMotionState.h"
#include "DBProJoints.h"
#include "DBProJointManager.h"
#include "BT2DX.h"
#include "LinearMath/btTransformUtil.h"

#include "CObjectsC.h"
#include ".\..\..\Error\CError.h"

#ifdef WICKEDENGINE
#include ".\..\..\..\..\Guru-WickedMAX\wickedcalls.h"
#endif

// externs to globals elsewhere
extern btDiscreteDynamicsWorld* g_dynamicsWorld;
extern DBProJointManager* jointManager;
extern float gSc;

//Global External variables
DBProRagDoll* currentDBProRagDoll = NULL;

DBProRagDoll::DBProRagDoll(int ragdollID) : BaseItem(ragdollID)
{
	//070714 - missing init vars
	m_modelTotalWeight=0;
	m_modelTotalVolume=0;
	m_modelTotalDensity=0;

	m_bNeedsUpdateForCulling = false;
#ifndef WICKEDENGINE
	m_linearDamping = 0.05f;
	m_angularDamping = 0.85f;
#else
	m_linearDamping = 0.5f;
	m_angularDamping = 0.95f;
#endif
	m_linearSleepingThresholds = 1.6f;
	m_angularSleepingThresholds = 2.5f;
	m_deactivationTime = 0.8f;
	
	m_bIsStatic = false;
	m_fRadius = 0;
	m_fScaledRadius = 0;
	m_fScaledLargestRadius = 0;
	m_bNeedsUpdateForCulling = false;

	// GG: nothing held and no body followed until asked
	m_iFollowBone = -1;
	m_iHoldBone = -1;
	m_pHoldAnchor = NULL;
	m_pHoldAnchorShape = NULL;
	m_pHoldJoint = NULL;
	m_holdFrom.setIdentity();
	m_uHoldStartMs = 0;
	m_holdTarget.setIdentity();
	m_fHoldTimeLeft = 0;
	m_fLimitsRestoreLeft = 0;
	m_fRestYaw = 0;
	m_fHoldStiffness = 0.85f;
	m_fHoldDamping = 0.6f;
	m_fHoldWaistFold = 60.0f;
	m_fHoldHipFold = 60.0f;
	m_bHoldEased = false;
	m_bHoldSteppedEased = false;
	m_bHoldDrawnKnown = false;
	m_fHoldTargetYaw = 0;
	m_fHoldYawStepped = 0;
	m_fHoldYawAtUpdate = 0;
	m_vecObjectAtUpdate.setZero();
	m_vecObjectAnglesAtUpdate.setZero();
	m_vecHeldAtUpdate.setZero();

	sObject* pObject = GetObjectData(ragdollID);
	//SetCullingRadius(pObject->collision.fRadius, pObject->collision.fScaledRadius, pObject->collision.fScaledLargestRadius);
	pObject->collision.fRadius=0;
	pObject->collision.fScaledRadius=0;
	pObject->collision.fScaledLargestRadius=0;

#ifdef WICKEDENGINE
	// Find the frame just after the root i.e. the frame that animates the skeleton, but not the world orientation.
	m_iAnimateFromJoint = 2;
	for (int iF = 0; iF < pObject->iFrameCount; iF++)
	{
		sFrame* pFrame = pObject->ppFrameList[iF];
		if (strcmp(pFrame->szName, "Bip01") == NULL)
		{
			m_iAnimateFromJoint = iF;
			break;
		}
	}
#endif
}

DBProRagDoll::~DBProRagDoll()
{
	// GG: the hold's joint goes before the bodies it links
	Release();
	if ( jointManager )
	{
		for (int i = 0; i < m_joints.size(); i++)
		{
			jointManager->DeleteJoint(m_joints[i]);
		}
	}
	for ( int i = 0; i < m_ragDollBoneArray.size(); i++)
	{
		SAFE_DELETE(m_ragDollBoneArray[i]);
	}
	m_ragDollBoneArray.clear();
}

void DBProRagDoll::ResetObjectParametersForAnimation()
{
	sObject* pObject = GetObjectData(m_id);
	if(pObject)
	{
		pObject->position.bCustomBoneMatrix = false;
		pObject->bUpdateOverallBounds = true;
	}
}

void DBProRagDoll::ResetObjectParametersForCulling()
{
	if(!m_bNeedsUpdateForCulling)
		return;
	m_bNeedsUpdateForCulling = false;
	sObject* pObject = GetObjectData(m_id);

	if (!pObject)
		return;
	// all meshes in object
	for ( int iFrame = 0; iFrame < pObject->iFrameCount; iFrame++ )
	{
		// get a pointer to the frame
		sFrame* pFrame = pObject->ppFrameList [ iFrame ];

		sMesh* pMesh = pFrame->pMesh;
		if(pMesh!=NULL)
		{
			CalculateMeshBounds(pMesh);
			btTransform transform = DBProToBullet::GetTransform(m_id);
			transform.setOrigin(btVector3(0.0,0.0,0.0));
			btVector3 btVecCenter = transform * BT2DX::DX_VECTOR3_2BT(pMesh->Collision.vecCentre);
			GGVECTOR3 vecCenter = BT2DX::BT2DX_VECTOR3(btVecCenter);
			//
			btVector3 btVecMin =transform * BT2DX::DX_VECTOR3_2BT(pMesh->Collision.vecMin);
			GGVECTOR3 vecMin = BT2DX::BT2DX_VECTOR3(btVecMin);
			//
			btVector3 btVecMax= transform * BT2DX::DX_VECTOR3_2BT(pMesh->Collision.vecMax);
			GGVECTOR3 vecMax = BT2DX::BT2DX_VECTOR3(btVecMax);
			//
			pObject->collision.vecCentre = vecCenter;//pMesh->Collision.vecCentre;
			pObject->collision.vecColCenter = vecCenter;//pMesh->Collision.vecCentre;
			pObject->collision.vecMin = vecMin;//pMesh->Collision.vecMin;
			pObject->collision.vecMax = vecMax;//pMesh->Collision.vecMax;

			pObject->collision.fRadius = 0;//pMesh->Collision.fRadius;
			pObject->collision.fLargestRadius = 0;//pMesh->Collision.fRadius;
			pObject->collision.fScaledRadius = 0;//pMesh->Collision.fRadius;
			pObject->collision.fScaledLargestRadius = 0;//->Collision.fRadius;
			pObject->collision.bColCenterUpdated = true;
		}
	}
	//FHB:Leave for debugging
	//ShowObjectBounds(m_id, 0);
}

void DBProRagDoll::SetTotalWeight(btScalar totalWeight)
{
	m_modelTotalWeight = totalWeight;
}

void DBProRagDoll::SetFriction(btScalar friction)
{
	for ( int i = 0; i < m_ragDollBoneArray.size(); i++)
	{
		btRigidBody* body = m_ragDollBoneArray[i]->GetRigidBody();
		if(body)
		{
			body->setFriction(friction);
			//body->setAnisotropicFriction ( btVector3(friction,friction,friction), 1 );
		}
	}
}

bool DBProRagDoll::IsStatic()
{
	return m_bIsStatic;
}

void DBProRagDoll::SetStatic(bool bStatic)
{
	m_bIsStatic = bStatic;
	for ( int i = 0; i < m_ragDollBoneArray.size(); i++)
	{
		if(bStatic)
		{
			//g_dynamicsWorld->SetRigidBodyMass(m_ragDollBoneArray[i]->GetRigidBody(), 0.0);
			btRigidBody* body = m_ragDollBoneArray[i]->GetRigidBody();
			btScalar mass = 0.0f;
			if(body)
			{
				btVector3 localInertia(0.0,0.0,0.0);
				body->getCollisionShape()->calculateLocalInertia(mass,localInertia);
				body->setMassProps(mass, localInertia);
			}
		}
		else
		{
			btScalar mass = m_modelTotalDensity * m_ragDollBoneArray[i]->boneVolume;
			//DynamicsWorldArray[0]->SetRigidBodyMass(m_ragDollBoneArray[i]->GetRigidBody(),mass);
			btRigidBody* body = m_ragDollBoneArray[i]->GetRigidBody();
			if(body)
			{
				btVector3 localInertia(0.0,0.0,0.0);
				body->getCollisionShape()->calculateLocalInertia(mass,localInertia);
				body->setMassProps(mass, localInertia);
			}
		}
	}
}

btRigidBody* DBProRagDoll::localCreateRigidBody (btScalar mass, const btTransform& startTransform, btCollisionShape* shape, int objID, int collisionGroup, int collisionMask)
{
	bool isDynamic = (mass != 0.f);
	btVector3 localInertia(0,0,0);
	if (isDynamic)
		shape->calculateLocalInertia(mass,localInertia);
	DBProMotionState* myMotionState = new DBProMotionState(startTransform, objID);
	btRigidBody::btRigidBodyConstructionInfo rbInfo(mass,myMotionState,shape,localInertia);
	btRigidBody* body = new btRigidBody(rbInfo);
	g_dynamicsWorld->addRigidBody(body, collisionGroup, collisionMask);
	return body;
}

int DBProRagDoll::AddBone(int objID, int limbID1, int limbID2, btScalar diameter, int collisionGroup, int collisionMask)
{
	m_ragDollBoneArray.push_back(new DBProRagDollBone(objID, limbID1, limbID2, diameter, 1.0f, collisionGroup, collisionMask));

	return m_ragDollBoneArray.size()-1;
}

int DBProRagDoll::AddBone(int objID, int limbID1, int limbID2, btScalar diameter, float lengthmod, int collisionGroup, int collisionMask)
{
	m_ragDollBoneArray.push_back(new DBProRagDollBone(objID, limbID1, limbID2, diameter, lengthmod, collisionGroup, collisionMask));

	return m_ragDollBoneArray.size()-1;
}

void DBProRagDoll::AddHingeJoint(int boneIndex1, int boneIndex2, int limbID, btVector3& jointRotation,
																					btScalar minRotation, btScalar maxRotation)
{
	btScalar scaleFactor = 40.0f;///DynamicsWorldArray[0]->m_scaleFactor;
	btHingeConstraint* hingeC;
	btTransform localATrans, localBTrans, modelTrans, jointTrans, limbRotationTrans;
	btMatrix3x3 modelMatrix, jointMatrix, limbMatrix;
	int objectID = m_id;
	btVector3 modelPosition(ObjectPositionX(objectID), ObjectPositionY(objectID), ObjectPositionZ(objectID));
	modelPosition = modelPosition/scaleFactor;

	//Create TransForm for DBPro Model do not include position.
	//We we use setEulerYPR for DBpro rotations passed in, the order is ZYX
	modelMatrix.setEulerYPR(btRadians(ObjectAngleZ(objectID)),
												btRadians(ObjectAngleY(objectID)),
													btRadians(ObjectAngleX(objectID)));
	modelTrans.setIdentity();
	modelTrans.setBasis(modelMatrix);

	//Create Transform for Joint with Position and Rotation
	btVector3 jointPosition(LimbPositionX(objectID,limbID), LimbPositionY(objectID,limbID), LimbPositionZ(objectID,limbID));
	jointPosition= jointPosition/scaleFactor;

	//When we use setEulerZYX here it needs to be YXZ
	jointMatrix.setEulerZYX(btRadians(jointRotation.getY()), btRadians(jointRotation.getX()), btRadians(jointRotation.getZ()));
	jointTrans.setIdentity();
	jointTrans.setBasis(jointMatrix);

	//FHB: Removed, not needed when we handel a frame of animation, might be needed if model has no animation.
	//If the model is rotated from its original orientation.
	//the joint will then have the same orientation as the models orientation.
	//jointTrans = modelTrans * jointTrans;

	//Adjustment based on the joints rotations at a frame of animation 

	//Get current limb rotation from animation and convert to radians
	btVector3 limbRotation(btRadians(LimbDirectionX(objectID,limbID)), btRadians(LimbDirectionY(objectID,limbID)), 
																		btRadians(LimbDirectionZ(objectID,limbID)));
	//Create Rotation only Transform for Models limb, No position
	//We use setEulerYPR for DBpro rotations passed in, the order is ZYX
	limbMatrix.setEulerYPR(limbRotation.getZ(), limbRotation.getY(), limbRotation.getX());//This rotation works for the models limbs rotations.
	limbRotationTrans.setIdentity();
	limbRotationTrans.setBasis(limbMatrix);

	//Rotate the joints transform with the limbs rotation
	jointTrans = limbRotationTrans * jointTrans ;

	jointTrans.setOrigin(jointPosition);
	
	//Creates local transforms for each side of joint using our adjusted joint transform
	DBProJoints::CreateLocalTransforms(m_ragDollBoneArray[boneIndex1]->GetRigidBody(),
																m_ragDollBoneArray[boneIndex2]->GetRigidBody(),
																	jointTrans, localATrans, localBTrans);

	btScalar minLimit = btRadians(minRotation);
	btScalar maxLimit = btRadians(maxRotation);

	//Get the Angle between the two bone capsules
	btVector3 bone1Norm, bone2Norm;
	bone1Norm = m_ragDollBoneArray[boneIndex1]->GetNormilizedVector();
	bone2Norm = m_ragDollBoneArray[boneIndex2]->GetNormilizedVector();
	btScalar ang = bone1Norm.angle(bone2Norm);

	//Subtract this angle from the limits.
	minLimit = minLimit - abs(ang);
	maxLimit = maxLimit - abs(ang);

	hingeC = DBProJoints::CreateHingeConstraint(m_ragDollBoneArray[boneIndex1]->GetRigidBody(), 
																				m_ragDollBoneArray[boneIndex2]->GetRigidBody(),
																					localATrans, localBTrans, minLimit, maxLimit,true);
	int jointID = jointManager->AddJoint(hingeC);
	m_joints.push_back(jointID);
	hingeC = NULL;
}

void DBProRagDoll::AddTwistConstraint(int boneIndex1, int boneIndex2, int limbID, btVector3& jointRotation, btVector3& jointlimits)
{
	btScalar scaleFactor = 40.0f;///DynamicsWorldArray[0]->m_scaleFactor;
	btConeTwistConstraint* coneC; 
	btTransform localATrans, localBTrans, modelTrans, jointTrans, limbRotationTrans;
	btMatrix3x3 modelMatrix, jointMatrix, limbMatrix;
	int objectID = m_id;
	//Create TransForm for DBPro Model do not include position.
	//We we use setEulerYPR for DBpro rotations passed in, the order is ZYX
	btVector3 modelPosition(ObjectPositionX(objectID), ObjectPositionY(objectID), ObjectPositionZ(objectID));
	modelPosition = modelPosition/scaleFactor;
	modelMatrix.setEulerYPR(btRadians(ObjectAngleZ(objectID)),
											btRadians(ObjectAngleY(objectID)),
											btRadians(ObjectAngleX(objectID)));
	modelTrans.setIdentity();
	modelTrans.setBasis(modelMatrix);

	//Create Transform for Joint, When we use setEulerYPR here it needs to be YXZ
	btVector3 jointPosition(LimbPositionX(objectID,limbID), LimbPositionY(objectID,limbID), LimbPositionZ(objectID,limbID));
	jointPosition= jointPosition/scaleFactor;

	//When we use setEulerZYX here it needs to be YXZ
	jointMatrix.setEulerZYX(btRadians(jointRotation.getY()), btRadians(jointRotation.getX()), btRadians(jointRotation.getZ()));
	jointTrans.setIdentity();
	jointTrans.setBasis(jointMatrix);

	//FHB: Removed, not needed when we handel a frame of animation, might be needed if model has no animation.
	//If the model is rotated from its original orientation.
	//the joint will then have the same orientation as the models orientation.
	//jointTrans = jointTrans * modelTrans;
	
	//Adjustment based on the joints rotations at a frame of animation 

	//Get current limb rotation from animation and conver to radians
	btVector3 limbRotation(btRadians(LimbDirectionX(objectID,limbID)), btRadians(LimbDirectionY(objectID,limbID)), 
																		btRadians(LimbDirectionZ(objectID,limbID)));
	//Create Rotation only Transform for Models limb No position
	//We use setEulerYPR for DBpro rotations passed in, the order is ZYX
	limbMatrix.setEulerYPR(limbRotation.getZ(), limbRotation.getY(), limbRotation.getX());//This rotation works for the models limbs rotations.
	limbRotationTrans.setIdentity();
	limbRotationTrans.setBasis(limbMatrix);

	//Rotate the joints transform with the limbs rotation
	jointTrans = limbRotationTrans * jointTrans ;

	jointTrans.setOrigin(jointPosition);

	//Creates local transforms for each side of joint using our adjusted joint transform
	DBProJoints::CreateLocalTransforms( m_ragDollBoneArray[boneIndex1]->GetRigidBody(),
																m_ragDollBoneArray[boneIndex2]->GetRigidBody(),
																	jointTrans, localATrans, localBTrans);

	btScalar swingSpan1 = btRadians(jointlimits.getZ());
	btScalar swingSpan2 = btRadians(jointlimits.getY());
	btScalar twistSpan = btRadians(jointlimits.getX());

	coneC =  DBProJoints::CreateTwistConstraint(m_ragDollBoneArray[boneIndex1]->GetRigidBody(), 
																				m_ragDollBoneArray[boneIndex2]->GetRigidBody(),
																					localATrans, localBTrans, swingSpan1, swingSpan2, twistSpan, true);

	//TODO: Figure out how to handel limits of cone twist at a frame of animation.
	//btScalar twistAng = coneC->getTwistAngle();

	int jointID = jointManager->AddJoint(coneC);
	m_joints.push_back(jointID);
	coneC = NULL;
}


int DBProRagDoll::GetBoneObjID(int boneIndex) 
{
	return (boneIndex >= 0 && boneIndex < m_ragDollBoneArray.size()) ? m_ragDollBoneArray[boneIndex]->GetRagDollBoneID() : -1; 
} 

bool DBProRagDoll::IsBoneObject(int objectID)
{
	for(int i = 0; i < m_ragDollBoneArray.size(); i++)
	{
		if(m_ragDollBoneArray[i]->GetObjectID()==objectID)
		{
			return true;
		}
	}
	return false;
}

void DBProRagDoll::Finalize()
{
	int objectID = m_id;
	sObject* pObject = GetObjectData(objectID);

	for (int i = 0; i < m_ragDollBoneArray.size(); i++)
	{
		m_modelTotalVolume += m_ragDollBoneArray[i]->boneVolume;
	}
	//Calculate density of ragdoll
	m_modelTotalDensity = m_modelTotalWeight / m_modelTotalVolume;

	//Set the mass of all the Rag doll bones to the calculated mass.
	for (int i = 0; i < m_ragDollBoneArray.size(); i++)
	{
		btScalar mass = m_modelTotalDensity * m_ragDollBoneArray[i]->boneVolume;
		btRigidBody* body = m_ragDollBoneArray[i]->GetRigidBody();
		if(body)
		{
			btVector3 localInertia(0.0,0.0,0.0);
			body->getCollisionShape()->calculateLocalInertia(mass,localInertia);
			body->setMassProps(mass, localInertia);

			body->setFriction(0.75f);
			body->setDamping(m_linearDamping, m_angularDamping);
			body->setDeactivationTime(m_deactivationTime);
			body->setSleepingThresholds(m_linearSleepingThresholds, m_angularSleepingThresholds);

			body->setCcdMotionThreshold(0.1f);
			body->setCcdSweptSphereRadius(0.1f);
		}
		//m_ragDollBoneArray[i]->GetRigidBody()->setFriction(0.75f);
		//m_ragDollBoneArray[i]->GetRigidBody()->setDamping(m_linearDamping, m_angularDamping);
		//m_ragDollBoneArray[i]->GetRigidBody()->setDeactivationTime(m_deactivationTime);
		//m_ragDollBoneArray[i]->GetRigidBody()->setSleepingThresholds(m_linearSleepingThresholds, m_angularSleepingThresholds);
	}
	//Set the object model to use CustomBoneMatrix for Rag doll
	//We can now move the models limbs with matCombined
	pObject->position.bCustomBoneMatrix = true;
	// This prevents the object from being culled when we move the limbs of the object.
	//Copied from DBPro Source Code
	//SetCullingRadius(pObject->collision.fRadius, pObject->collision.fScaledRadius, pObject->collision.fScaledLargestRadius);
	pObject->collision.fRadius = 0;
	pObject->collision.fScaledRadius = 0;
	pObject->collision.fScaledLargestRadius = 0;
}

void DBProRagDoll::SetCullingRadius(float fRadius, float fScaledRadius, float fScaledLargestRadius)
{
	m_fRadius = fRadius;
	m_fScaledRadius = fScaledRadius;
	m_fScaledLargestRadius = fScaledLargestRadius;
}

void DBProRagDoll::GetCullingRadius(float &outfRadius, float &outfScaledRadius, float &outfScaledLargestRadius)
{
	outfRadius = m_fRadius;
	outfScaledRadius = m_fScaledRadius;
	outfScaledLargestRadius = m_fScaledLargestRadius;
}

bool  DBProRagDoll::IsSleeping()
{
	bool bIsSleeping = true;
	for (int i = 0; i < m_ragDollBoneArray.size(); i++)
	{
		bIsSleeping = bIsSleeping && !m_ragDollBoneArray[i]->GetRigidBody()->isActive();
	}
	return bIsSleeping;
}

void DBProRagDoll::Activate()
{
	for (int i = 0; i < m_ragDollBoneArray.size(); i++)
	{
		m_ragDollBoneArray[i]->GetRigidBody()->activate();
	}
}

// GG: a force, or at a world rate of 60 an impulse so a frame with no world step doesn't lose it (BulletPhysics.CPP)
extern void PhysicsApplyPush(btRigidBody* pBody, const btVector3& force, const btVector3& relPos);

void DBProRagDoll::ApplyForce(int iLimbNumber, btVector3& Start, btVector3& End, float fForceValue)
{
	for (int i = 0; i < m_ragDollBoneArray.size(); i++)
	{
		btRigidBody* pBody = m_ragDollBoneArray[i]->GetRigidBody();
		btVector3 thisPos = pBody->getWorldTransform().getOrigin();
		btVector3 relPos = btVector3(0,0,0);//End - thisPos;
		btVector3 force = (End-Start);
		force = force / force.length() / gSc;
		force = force * (fForceValue/gSc);
		if ( iLimbNumber==-1 )
		{
			PhysicsApplyPush(pBody, force, relPos);
		}
		else
		{
			for (int j = 0; j < m_ragDollBoneArray[i]->limbOffsets.size(); j++)
			{
				if ( iLimbNumber==m_ragDollBoneArray[i]->dbproLimbIDs[j] )
				{
					PhysicsApplyPush(pBody, force, relPos);
				}
			}
		}
	}
}

/* not needed
void CalculatePureRotationsFromCombed (sFrame* pFrame)
{
	if (pFrame)
	{
		// work out pure rotation from local matrix (matCombined)
		GGVECTOR3 vecPoint = GGVECTOR3(0, 0, 0);
		GGVec3TransformCoord(&vecPoint, &vecPoint, &pFrame->matCombined);
		GGMATRIX matPureRot;
		GGVECTOR3 eye = GGVECTOR3(0,0,0);
		GGVECTOR3 at = vecPoint;
		GGVECTOR3 up = GGVECTOR3(0,1,0);
		GGMatrixLookAtLH(&matPureRot, &eye, &at, &up);
		pFrame->matCombined = matPureRot;
		if (pFrame->pChild) CalculatePureRotationsFromCombed(pFrame->pChild);
		if (pFrame->pSibling) CalculatePureRotationsFromCombed(pFrame->pSibling);
	}
}

bool g_bInitialQuatCollection = false;
GGVECTOR4 g_ragdollWorldQuats[100];
GGVECTOR4 g_poseWorldQuats[100];
GGVECTOR4 g_ragdolltoPoseQuats[100];
GGVECTOR4 g_ragdollQuats[100];
GGVECTOR4 g_ragdollInitialQuats[100];
GGVECTOR4 g_ragdollDifferenceQuats[100];

void CalculateLocalQuatFromWorldQuat(sFrame* pFrame, GGVECTOR4 poseParentWorld, GGVECTOR4 ragdollParentWorld)
{
	if (pFrame)
	{
		// make pose world from local pose
		GGVECTOR4 localInitialPoseQuat = GGVECTOR4(pFrame->matOriginal._11, pFrame->matOriginal._12, pFrame->matOriginal._13, pFrame->matOriginal._14);
		WickedCall_CalculateWorldQuat(&poseParentWorld, &localInitialPoseQuat, &g_poseWorldQuats[pFrame->iID]);

		// need local rot of this frame, so take this world and deduct parents world
		GGVECTOR4 thisWorld = g_poseWorldQuats[pFrame->iID];
		GGVECTOR4 localPoseRot;
		WickedCall_CalculateQuatDiff(&poseParentWorld, &thisWorld, &localPoseRot);

		// need relative ragdoll rot, so take ragdoll world and deduct parent ragdoll world
		GGVECTOR4 localRagdollRot;
		WickedCall_CalculateQuatDiff(&ragdollParentWorld, &g_ragdollWorldQuats[pFrame->iID], &localRagdollRot);

		// some idea that we handle local ragdoll rot through the orientation of the local pose rot
		//WickedCall_CalculateQuatDiff(&localPoseRot, &localRagdollRot, &localRagdollRot);

		// work out final relative local rot for this frame
		if (g_bInitialQuatCollection == false)
		{
			// work out difference between local pose and local ragdoll
			GGVECTOR4 localDifferenceToBridgeGap;
			//WickedCall_CalculateQuatDiff(&localPoseRot, &localRagdollRot, &localDifferenceToBridgeGap);
			WickedCall_CalculateQuatDiff(&localRagdollRot, &localPoseRot, &localDifferenceToBridgeGap);

			// initial bridge gap rot to be used later
			g_ragdolltoPoseQuats[pFrame->iID] = localDifferenceToBridgeGap;
			g_ragdollQuats[pFrame->iID] = GGVECTOR4(0, 0, 0, 1);
		}
		else
		{
			// the local rotation that represents the change in ragdoll compared to pose 
			WickedCall_CalculateQuatDiff(&g_ragdolltoPoseQuats[pFrame->iID], &localRagdollRot, &g_ragdollQuats[pFrame->iID]);
		}

		// do all frames in object
		if (pFrame->pChild) CalculateLocalQuatFromWorldQuat(pFrame->pChild, g_poseWorldQuats[pFrame->iID], g_ragdollWorldQuats[pFrame->iID]);
		if (pFrame->pSibling) CalculateLocalQuatFromWorldQuat(pFrame->pSibling, poseParentWorld, ragdollParentWorld);
	}
}
*/

void CalculateRelativeMatricesFromWorld (sFrame* pParentFrame, sFrame* pFrame)
{
	if (pFrame)
	{
		if (pParentFrame)
		{
			float fDet = 0.0f;
			GGMATRIX matMatrixWorldAtThisPointInv;
			GGMatrixInverse (&matMatrixWorldAtThisPointInv, &fDet, &pParentFrame->matCombined);
			GGMatrixMultiply (&pFrame->matTransformed, &pFrame->matCombined, &matMatrixWorldAtThisPointInv);
		}
		else
		{
			GGMatrixIdentity (&pFrame->matTransformed);
		}
		if (pFrame->pChild) CalculateRelativeMatricesFromWorld(pFrame, pFrame->pChild);
		if (pFrame->pSibling) CalculateRelativeMatricesFromWorld(pParentFrame, pFrame->pSibling);
	}
}

void DBProRagDoll::Update() 
{
	bool bStillMoving = false;
	m_bNeedsUpdateForCulling = true;
	int objectID = m_id;
	sObject* pObject = GetObjectData(objectID);
	if (pObject)
	{
		// Turn Culling Off
		pObject->collision.fRadius = 0;
		pObject->collision.fScaledRadius = 0;
		pObject->collision.fScaledLargestRadius = 0;

		// GG: the object goes where the followed body is before the limbs are worked out from its position, so the
		// body is drawn where it is and the object stays with it (a held body carried away from where it fell)
		if (m_iFollowBone >= 0 && m_iFollowBone < m_ragDollBoneArray.size())
		{
			btVector3 followPosition = GetBonePosition(m_iFollowBone);
			PositionObject(objectID, followPosition.getX(), followPosition.getY(), followPosition.getZ());
		}

		btScalar scaleFactor = 40.0f;///DynamicsWorldArray[0]->m_scaleFactor;

		//Create Transform for DBpro Model,needed for when model is not positioned at origin
		btTransform DBProModelTrans, DBProModelTransRot, limbInitalRotationTrans;
		btMatrix3x3 DBProModelRotation;
		DBProModelTrans.setIdentity();
		DBProModelTransRot.setIdentity();
		limbInitalRotationTrans.setIdentity();

		DBProModelRotation.setEulerYPR(btRadians(ObjectAngleZ(objectID)),
			btRadians(ObjectAngleY(objectID)),
			btRadians(ObjectAngleX(objectID)));
		DBProModelTransRot.setBasis(DBProModelRotation);

		btVector3 modelPosition = -1 * BT2DX::DX_VECTOR3_2BT(pObject->position.vecPosition);
		modelPosition = modelPosition / scaleFactor;
		DBProModelTrans.setOrigin(modelPosition);

		// bool array to track which frames are part of ragdoll adjustments
		bool* pbFramePartOfBoneMovement = new bool[pObject->iFrameCount];
		for (int iF = 0; iF < pObject->iFrameCount; iF++) pbFramePartOfBoneMovement[iF] = false;

		// Loop through all bones and subtract all initial rotations of the capsules from there transforms
		for (int i = 0; i < m_ragDollBoneArray.size(); i++)
		{
			btTransform boneWorldTransform, boneRotatedWorldTransform, newLimbWorldTransform;
			boneWorldTransform.setIdentity();
			boneRotatedWorldTransform.setIdentity();
			newLimbWorldTransform.setIdentity();
			btVector3 finalWorldPosition;

			//Get the bones World Transform
			boneWorldTransform = btTransform(m_ragDollBoneArray[i]->GetRigidBody()->getWorldTransform());

			//Subtract the initial rotation of the rag doll bone
			boneRotatedWorldTransform.setBasis(boneWorldTransform.getBasis() * m_ragDollBoneArray[i]->initialRotation.inverse());

			for (int j = 0; j < m_ragDollBoneArray[i]->limbOffsets.size(); j++)
			{
				//Offsets the positions vector to ends of rag doll bones(capsules) for joint alignment.
				finalWorldPosition = boneWorldTransform * m_ragDollBoneArray[i]->limbOffsets[j];

				//Take the offset position vector and multiply by the DBpro models world transform
				finalWorldPosition = DBProModelTrans * finalWorldPosition;

				//Updated vector is new final world position for the limbs transform
				newLimbWorldTransform.setOrigin(finalWorldPosition * scaleFactor);

				//set the rotation with the bones rotated transform 
				newLimbWorldTransform.setBasis(boneRotatedWorldTransform.getBasis());

				//Handel the Models limbs rotations at a frame of animation.
				limbInitalRotationTrans.setBasis(m_ragDollBoneArray[i]->limbInitalRotation[j]);
				limbInitalRotationTrans = limbInitalRotationTrans * DBProModelTransRot.inverse();
				newLimbWorldTransform = newLimbWorldTransform * limbInitalRotationTrans;
				newLimbWorldTransform = DBProModelTransRot.inverse() * (newLimbWorldTransform * DBProModelTransRot);

				//This moves the joints but not the models bounding box that is the object
				if (pObject->ppFrameList)
				{
					sFrame* pFrame = pObject->ppFrameList[m_ragDollBoneArray[i]->dbproLimbIDs[j]];
					if (pFrame)
					{
						pFrame->matCombined = BT2DX::BT2DX_MATRIX(newLimbWorldTransform);
						pbFramePartOfBoneMovement[m_ragDollBoneArray[i]->dbproLimbIDs[j]] = true;
					}
				}
			}

			// is this bone moving?
			float fEpsilonToFreezeLin = 0.04f;
			float fEpsilonToFreezeAng = 0.30f;
			btVector3 LinVel = m_ragDollBoneArray[i]->GetRigidBody()->getLinearVelocity();
			btVector3 AngVel = m_ragDollBoneArray[i]->GetRigidBody()->getAngularVelocity();
			if (fabs(LinVel.getX()) + fabs(LinVel.getY()) + fabs(LinVel.getZ()) > fEpsilonToFreezeLin) bStillMoving = true;
			if (fabs(AngVel.getX()) + fabs(AngVel.getY()) + fabs(AngVel.getZ()) > fEpsilonToFreezeAng) bStillMoving = true;
		}

		// go through all frame hierarchy and calc relative matrices from world matrices (and store back in matCombined)
		CalculateRelativeMatricesFromWorld(NULL, pObject->pFrame);

		for (int iF = m_iAnimateFromJoint + 1; iF < pObject->iFrameCount; iF++)
		{
			sFrame* pFrame = pObject->ppFrameList[iF];
			pFrame->matCombined = pFrame->matTransformed;
		}
		// and then give the relative matrices to wicked to animate the object
		for (int iF = m_iAnimateFromJoint; iF < pObject->iFrameCount; iF++)
		{
			if (pbFramePartOfBoneMovement[iF] == true)
			{
				sFrame* pFrame = pObject->ppFrameList[iF];
				if (pFrame)
				{
					bool bIncludeTranslation = false;
					if (iF == m_iAnimateFromJoint) bIncludeTranslation = true;
					WickedCall_OverrideLimbWithCombined(pObject, pFrame, bIncludeTranslation);
				}
			}
		}
		// must keep object playing so modified bones can affect the wicked animation system
		LoopObject(objectID, 0, 1);

		// moved this out of wicked def as otherwise memory leak in non-MAX builds
		delete pbFramePartOfBoneMovement;
		pbFramePartOfBoneMovement = NULL;
	}

	// GG: where an eased hold's update left the object and the held body, and the yaw it was held at, for Hold to draw
	// it where the script holds it after this update
	if (m_pHoldJoint && m_bHoldSteppedEased && pObject && m_iHoldBone >= 0)
	{
		m_vecObjectAtUpdate = BT2DX::DX_VECTOR3_2BT(pObject->position.vecPosition);
		m_vecObjectAnglesAtUpdate = btVector3(ObjectAngleX(objectID), ObjectAngleY(objectID), ObjectAngleZ(objectID));
		m_vecHeldAtUpdate = GetBonePosition(m_iHoldBone);
		m_fHoldYawAtUpdate = m_fHoldYawStepped;
		m_bHoldDrawnKnown = true;
	}

	// if overall ragdoll has stopped moving, switch it to static
	// GG: not while held, or a held body that stops swinging could no longer be carried
	if (bStillMoving == false && m_pHoldJoint == NULL)
		SetStatic(true);
}

void DBProRagDoll::AssignLimbIDToBone(int boneIndex, int dbproLimbID)
{
	if (boneIndex == -1) return;
	int objectID = m_id;
	m_ragDollBoneArray[boneIndex]->AddDBProLimbID(dbproLimbID);
	//Store the initial rotation of the Dbpro objects limbs.
	btMatrix3x3 matInitalRot;
	matInitalRot.setEulerYPR(btRadians(LimbDirectionZ(objectID, dbproLimbID)),
		btRadians(LimbDirectionY(objectID, dbproLimbID)),
		btRadians(LimbDirectionX(objectID, dbproLimbID)));
	m_ragDollBoneArray[boneIndex]->limbInitalRotation.push_back(matInitalRot);
}

void DBProRagDoll::HideBones()
{
	for(int i = 0; i < m_ragDollBoneArray.size(); i++)
	{
		if ( m_ragDollBoneArray[i]->GetRagDollBoneID()>0 )
			HideObject(m_ragDollBoneArray[i]->GetRagDollBoneID());
	}
}

void DBProRagDoll::ShowBones()
{
	for(int i = 0; i < m_ragDollBoneArray.size(); i++)
	{
		if ( m_ragDollBoneArray[i]->GetRagDollBoneID()>0 )
			ShowObject(m_ragDollBoneArray[i]->GetRagDollBoneID());
	}
}

void DBProRagDoll::SetDamping(btScalar linear, btScalar angular)
{
	m_linearDamping = linear;
	m_angularDamping = angular;
}

void DBProRagDoll::SetSleepingThresholds(btScalar linear, btScalar angular)
{
	m_linearSleepingThresholds = linear;
	m_angularSleepingThresholds = angular;
}

void  DBProRagDoll::SetDeactivationTime(btScalar time)
{
	m_deactivationTime = time;
}

// GG: the body whose capsule moves a limb (-1 when none does)
int DBProRagDoll::FindBone(int limbID)
{
	for (int i = 0; i < m_ragDollBoneArray.size(); i++)
	{
		for (int j = 0; j < m_ragDollBoneArray[i]->dbproLimbIDs.size(); j++)
		{
			if (m_ragDollBoneArray[i]->dbproLimbIDs[j] == limbID) return i;
		}
	}
	return -1;
}

// GG: a body's centre in world units
btVector3 DBProRagDoll::GetBonePosition(int boneIndex)
{
	btScalar scaleFactor = 40.0f;
	if (boneIndex < 0 || boneIndex >= m_ragDollBoneArray.size()) return btVector3(0, 0, 0);
	return m_ragDollBoneArray[boneIndex]->GetRigidBody()->getWorldTransform().getOrigin() * scaleFactor;
}

// GG: each body's turn as it would be with the character facing angle 0, taken as the ragdoll is made (the pose it is
// made from) with the angle the character faces, so Hold can turn a body as the character would be turned
void DBProRagDoll::SetRestYaw(btScalar yawDegrees)
{
	btMatrix3x3 facing;
	facing.setEulerYPR(0, btRadians(yawDegrees), 0);
	m_restRotation.clear();
	for (int i = 0; i < m_ragDollBoneArray.size(); i++)
	{
		m_restRotation.push_back(facing.transpose() * m_ragDollBoneArray[i]->GetRigidBody()->getWorldTransform().getBasis());
	}

	// and the pose it is made from, with each joint's pivot, for the drape a held ragdoll is pulled to (BuildHoldPose)
	m_fRestYaw = yawDegrees;
	m_createTransform.clear();
	for (int i = 0; i < m_ragDollBoneArray.size(); i++)
	{
		m_createTransform.push_back(m_ragDollBoneArray[i]->GetRigidBody()->getWorldTransform());
	}
	m_jointLinks.clear();
	if (!jointManager) return;
	for (int i = 0; i < m_joints.size(); i++)
	{
		DBProJoint* pDBProJoint = jointManager->GetJoint(m_joints[i]);
		btTypedConstraint* pJoint = pDBProJoint ? pDBProJoint->GetConstraint() : NULL;
		if (!pJoint) continue;
		btTransform frameA;
		if (pJoint->getConstraintType() == HINGE_CONSTRAINT_TYPE) frameA = ((btHingeConstraint*)pJoint)->getAFrame();
		else if (pJoint->getConstraintType() == CONETWIST_CONSTRAINT_TYPE) frameA = ((btConeTwistConstraint*)pJoint)->getAFrame();
		else continue;
		sJointLink link;
		link.iBoneA = BoneOfBody(&pJoint->getRigidBodyA());
		link.iBoneB = BoneOfBody(&pJoint->getRigidBodyB());
		if (link.iBoneA < 0 || link.iBoneB < 0) continue;
		link.pivot = pJoint->getRigidBodyA().getWorldTransform() * frameA.getOrigin();
		m_jointLinks.push_back(link);
	}
}

int DBProRagDoll::BoneOfBody(const btRigidBody* pBody)
{
	for (int i = 0; i < m_ragDollBoneArray.size(); i++)
		if (m_ragDollBoneArray[i]->GetRigidBody() == pBody) return i;
	return -1;
}

// GG: the drape a held ragdoll is pulled to, each body's transform from the held one: the pose it was made from (a
// knockout's, standing), each part hanging from a joint of the held body turned about that joint's pivot on his right
// to his front, by the waist fold for a part above the joint and the hip fold below (a fireman's carry, face down)
void DBProRagDoll::BuildHoldPose(int boneIndex)
{
	const int iBones = m_ragDollBoneArray.size();
	m_holdPose.resize(iBones);
	if (boneIndex < 0 || boneIndex >= iBones) return;
	if (m_createTransform.size() != iBones)
	{
		// with no pose to start from, the one it has now
		btTransform heldInverse = m_ragDollBoneArray[boneIndex]->GetRigidBody()->getWorldTransform().inverse();
		for (int i = 0; i < iBones; i++) m_holdPose[i] = heldInverse * m_ragDollBoneArray[i]->GetRigidBody()->getWorldTransform();
		return;
	}
	btTransform heldInverse = m_createTransform[boneIndex].inverse();
	for (int i = 0; i < iBones; i++) m_holdPose[i] = heldInverse * m_createTransform[i];
	btMatrix3x3 facing;
	facing.setEulerYPR(0, btRadians(m_fRestYaw), 0);
	btVector3 upLocal = heldInverse.getBasis() * btVector3(0, 1, 0);
	btVector3 rightLocal = heldInverse.getBasis() * (facing * btVector3(1, 0, 0));
	if (rightLocal.length2() < 0.0001f) return;
	rightLocal.normalize();

	btAlignedObjectArray<int> branchOf;
	branchOf.resize(iBones);
	for (int i = 0; i < iBones; i++) branchOf[i] = -1;
	branchOf[boneIndex] = -2;
	for (int l = 0; l < m_jointLinks.size(); l++)
	{
		const sJointLink& link = m_jointLinks[l];
		int iStart = link.iBoneA == boneIndex ? link.iBoneB : (link.iBoneB == boneIndex ? link.iBoneA : -1);
		if (iStart < 0 || branchOf[iStart] != -1) continue;

		// the part hanging from this joint: every body reached from it without going back through the held one
		btAlignedObjectArray<int> branch;
		branch.push_back(iStart);
		branchOf[iStart] = l;
		for (int b = 0; b < branch.size(); b++)
		{
			for (int m = 0; m < m_jointLinks.size(); m++)
			{
				int iNext = m_jointLinks[m].iBoneA == branch[b] ? m_jointLinks[m].iBoneB : (m_jointLinks[m].iBoneB == branch[b] ? m_jointLinks[m].iBoneA : -1);
				if (iNext < 0 || branchOf[iNext] != -1) continue;
				branchOf[iNext] = l;
				branch.push_back(iNext);
			}
		}

		// above the joint (the torso) folds by the waist fold, below it (a leg) by the hip fold, both toward his front
		btVector3 pivotLocal = heldInverse * link.pivot;
		btVector3 centre(0, 0, 0);
		for (int b = 0; b < branch.size(); b++) centre += m_holdPose[branch[b]].getOrigin();
		centre /= btScalar(branch.size());
		bool bAbove = (centre - pivotLocal).dot(upLocal) > 0;
		btScalar fFold = bAbove ? btRadians(m_fHoldWaistFold) : -btRadians(m_fHoldHipFold);
		btTransform toPivot(btQuaternion::getIdentity(), pivotLocal);
		btTransform fold(btQuaternion(rightLocal, fFold), btVector3(0, 0, 0));
		btTransform fromPivot(btQuaternion::getIdentity(), -pivotLocal);
		btTransform turn = toPivot * fold * fromPivot;
		for (int b = 0; b < branch.size(); b++) m_holdPose[branch[b]] = turn * m_holdPose[branch[b]];
	}
}

// GG: held, the bodies slide where they touch (no friction) and their swing is damped; let go, as they were
void DBProRagDoll::SetHeldBodies(bool bHeld)
{
	if (bHeld && m_heldFriction.size() != m_ragDollBoneArray.size())
	{
		m_heldFriction.resize(m_ragDollBoneArray.size());
		for (int i = 0; i < m_ragDollBoneArray.size(); i++) m_heldFriction[i] = m_ragDollBoneArray[i]->GetRigidBody()->getFriction();
	}
	btScalar fDamping = btMin(btMax(m_fHoldDamping, btScalar(0.0)), btScalar(0.99));
	for (int i = 0; i < m_ragDollBoneArray.size(); i++)
	{
		btRigidBody* pBody = m_ragDollBoneArray[i]->GetRigidBody();
		if (!pBody) continue;
		if (bHeld)
		{
			pBody->setFriction(0);
			pBody->setDamping(btMax(m_linearDamping, fDamping), btMax(m_angularDamping, fDamping));
		}
		else
		{
			if (i < m_heldFriction.size()) pBody->setFriction(m_heldFriction[i]);
			pBody->setDamping(m_linearDamping, m_angularDamping);
		}
	}
	if (!bHeld) m_heldFriction.clear();
}

// GG: how hard a held ragdoll is pulled to its drape (0 hangs free, 1 rigid), its swing's damping (0 to 1), and the
// folds in degrees at the held body's joints; held now, at once
void DBProRagDoll::SetHoldStiffness(btScalar fStiffness, btScalar fDamping, btScalar fWaistFold, btScalar fHipFold)
{
	m_fHoldStiffness = btMin(btMax(fStiffness, btScalar(0.0)), btScalar(1.0));
	m_fHoldDamping = btMin(btMax(fDamping, btScalar(0.0)), btScalar(1.0));
	m_fHoldWaistFold = fWaistFold;
	m_fHoldHipFold = fHipFold;
	if (m_pHoldJoint)
	{
		BuildHoldPose(m_iHoldBone);
		SetHeldBodies(true);
	}
}

void DBProRagDoll::GetHoldStiffness(btScalar* pStiffness, btScalar* pDamping, btScalar* pWaistFold, btScalar* pHipFold)
{
	*pStiffness = m_fHoldStiffness;
	*pDamping = m_fHoldDamping;
	*pWaistFold = m_fHoldWaistFold;
	*pHipFold = m_fHoldHipFold;
}

// GG: the object's position follows this body from the next update (-1 stops it)
void DBProRagDoll::SetFollowBone(int boneIndex)
{
	m_iFollowBone = (boneIndex >= 0 && boneIndex < m_ragDollBoneArray.size()) ? boneIndex : -1;
}

// GG: holds a body at a point (world units) turned by the given turn of the character: a fixed joint to a kinematic body
// with no collision, which HoldStep moves toward the point a little at each simulation step. From where the body was,
// the point is eased there over a quarter of a second. The other bodies hang from it on the ragdoll's own joints, those
// at the held body opened up so they can hang, all solved harder. Held, the bodies never sleep or turn static
void DBProRagDoll::Hold(int boneIndex, const btVector3& position, const btMatrix3x3& rotation, btScalar fYawDegrees, unsigned int uNowMs)
{
	if (boneIndex < 0 || boneIndex >= m_ragDollBoneArray.size()) return;
	if (m_pHoldJoint && m_iHoldBone != boneIndex) Release();
	btRigidBody* pBody = m_ragDollBoneArray[boneIndex]->GetRigidBody();
	if (!pBody) return;

	btScalar scaleFactor = 40.0f;
	btTransform target;
	target.setOrigin(position / scaleFactor);
	if (boneIndex < m_restRotation.size())
		target.setBasis(rotation * m_restRotation[boneIndex]);
	else
		target.setBasis(rotation);

	if (m_pHoldJoint == NULL)
	{
		// every body given its mass (a ragdoll that settled has none), and held, nothing may sleep
		SetStatic(false);
		for (int i = 0; i < m_ragDollBoneArray.size(); i++)
		{
			btRigidBody* pBoneBody = m_ragDollBoneArray[i]->GetRigidBody();
			if (pBoneBody) pBoneBody->setActivationState(DISABLE_DEACTIVATION);
		}

		// held, the bodies meet nothing: a foot sunk into the ground at the lift was held there and the leg pulled out
		SetHeldCollision(true);
		m_bHoldEased = false;
		m_bHoldSteppedEased = false;
		m_bHoldDrawnKnown = false;

		// the anchor starts where the body is, so the joint holds it without a jolt
		m_holdFrom = pBody->getWorldTransform();
		m_uHoldStartMs = uNowMs;
		m_pHoldAnchorShape = new btSphereShape(btScalar(0.05));
		btDefaultMotionState* pMotionState = new btDefaultMotionState(m_holdFrom);
		btRigidBody::btRigidBodyConstructionInfo anchorInfo(0, pMotionState, m_pHoldAnchorShape, btVector3(0, 0, 0));
		m_pHoldAnchor = new btRigidBody(anchorInfo);
		m_pHoldAnchor->setCollisionFlags(m_pHoldAnchor->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT | btCollisionObject::CF_NO_CONTACT_RESPONSE);
		m_pHoldAnchor->setActivationState(DISABLE_DEACTIVATION);
		g_dynamicsWorld->addRigidBody(m_pHoldAnchor, 0, 0);

		btGeneric6DofConstraint* pJoint = new btGeneric6DofConstraint(*m_pHoldAnchor, *pBody, btTransform::getIdentity(), btTransform::getIdentity(), true);
		pJoint->setLinearLowerLimit(btVector3(0, 0, 0));
		pJoint->setLinearUpperLimit(btVector3(0, 0, 0));
		pJoint->setAngularLowerLimit(btVector3(0, 0, 0));
		pJoint->setAngularUpperLimit(btVector3(0, 0, 0));
		g_dynamicsWorld->addConstraint(pJoint, true);
		m_pHoldJoint = pJoint;
		m_iHoldBone = boneIndex;
		m_holdTarget = m_holdFrom;
		m_fHoldTimeLeft = 0;

		// a waist that bends 50 degrees and hips 15 held a body out straight from a held pelvis, and the joints gave
		// (stretched and folded limbs): those at the held body open up so what hangs from it hangs, and every joint
		// is solved four times as hard
		RelaxHeldJoints(pBody);
		SetJointIterations(40);

		// and pulled to a drape at each step (HoldStep), sliding where it touches, its swing damped: open joints alone
		// left a held body swinging, and a hand on the ground held by friction
		BuildHoldPose(boneIndex);
		SetHeldBodies(true);
	}

	// eased from where the body was to the target over 0.25 s, then on the target; the anchor follows at each step
	float fBlend = (float)(uNowMs - m_uHoldStartMs) / 250.0f;
	if (fBlend < 0.0f) fBlend = 0.0f;
	if (fBlend > 1.0f) fBlend = 1.0f;
	if (fBlend >= 1.0f) m_bHoldEased = true;
	fBlend = fBlend * fBlend * (3.0f - (2.0f * fBlend));
	m_holdTarget.setOrigin(m_holdFrom.getOrigin().lerp(target.getOrigin(), fBlend));
	m_holdTarget.setRotation(m_holdFrom.getRotation().slerp(target.getRotation(), fBlend));
	m_fHoldTargetYaw = fYawDegrees;

	// once eased, drawn where it is held: the script sets this after the frame's physics, so the last update left
	// the held body where the previous call put it; the object (whose limbs are drawn from it) is moved by the
	// difference and turned about the held body by the change of yaw, and the next update takes over from there
	if (m_bHoldEased && m_bHoldDrawnKnown)
	{
		btScalar fTurn = fYawDegrees - m_fHoldYawAtUpdate;
		btMatrix3x3 turnY;
		turnY.setEulerYPR(0, btRadians(fTurn), 0);
		btVector3 objectDrawn = position + turnY * (m_vecObjectAtUpdate - m_vecHeldAtUpdate);
		PositionObject(m_id, objectDrawn.getX(), objectDrawn.getY(), objectDrawn.getZ());
		RotateObject(m_id, m_vecObjectAnglesAtUpdate.getX(), m_vecObjectAnglesAtUpdate.getY() + fTurn, m_vecObjectAnglesAtUpdate.getZ());
	}
}

// GG: held, the bodies are taken out of the world's collision (mask none) and put back as they were when let go
void DBProRagDoll::SetHeldCollision(bool bHeld)
{
	if (!g_dynamicsWorld) return;
	if (bHeld)
	{
		m_heldGroup.resize(m_ragDollBoneArray.size());
		m_heldMask.resize(m_ragDollBoneArray.size());
	}
	else if (m_heldGroup.size() != m_ragDollBoneArray.size())
	{
		return;
	}
	for (int i = 0; i < m_ragDollBoneArray.size(); i++)
	{
		btRigidBody* pBody = m_ragDollBoneArray[i]->GetRigidBody();
		if (!pBody || !pBody->getBroadphaseHandle()) continue;
		if (bHeld)
		{
			m_heldGroup[i] = pBody->getBroadphaseHandle()->m_collisionFilterGroup;
			m_heldMask[i] = pBody->getBroadphaseHandle()->m_collisionFilterMask;
		}
		g_dynamicsWorld->removeRigidBody(pBody);
		g_dynamicsWorld->addRigidBody(pBody, m_heldGroup[i], bHeld ? 0 : m_heldMask[i]);
	}
	if (!bHeld)
	{
		m_heldGroup.clear();
		m_heldMask.clear();
	}
}

// GG: the time the coming physics update covers: while easing, the anchor reaches its target by the end of it
void DBProRagDoll::HoldFrame(btScalar frameTime)
{
	m_fHoldTimeLeft = frameTime;
	m_fHoldYawStepped = m_fHoldTargetYaw;
	m_bHoldSteppedEased = m_bHoldEased;
}

// GG: before each simulation step. Easing (the lift), the anchor moves its share of the way left to the target in
// this update, at most 25 m/s and 360 degrees a second, with the velocity of that move so the joint follows it
// smoothly. Eased, it goes onto the target in the first step, so an update always ends with the held body where the
// last call put it (Hold draws it from there). After a hold, the joints opened up go back to their limits over a second
void DBProRagDoll::HoldStep(btScalar timeStep)
{
	if (m_pHoldAnchor && timeStep > 0)
	{
		btTransform from = m_pHoldAnchor->getWorldTransform();
		btScalar fShare = (m_fHoldTimeLeft > timeStep && !m_bHoldSteppedEased) ? timeStep / m_fHoldTimeLeft : btScalar(1.0);
		m_fHoldTimeLeft = (m_fHoldTimeLeft > timeStep) ? m_fHoldTimeLeft - timeStep : btScalar(0.0);

		btVector3 move = (m_holdTarget.getOrigin() - from.getOrigin()) * fShare;
		btScalar fMaxMove = btScalar(25.0) * timeStep; // bullet units are world units / 40, a metre
		if (!m_bHoldSteppedEased && move.length() > fMaxMove) move *= fMaxMove / move.length();

		btQuaternion turnFrom = from.getRotation();
		btQuaternion turnTo = m_holdTarget.getRotation();
		btScalar fAngle = turnFrom.angleShortestPath(turnTo);
		btScalar fTurn = fAngle * fShare;
		btScalar fMaxTurn = btRadians(360.0f) * timeStep;
		if (!m_bHoldSteppedEased && fTurn > fMaxTurn) fTurn = fMaxTurn;
		btQuaternion turn = (fAngle > btScalar(0.00001)) ? turnFrom.slerp(turnTo, fTurn / fAngle) : turnTo;

		btTransform to(turn, from.getOrigin() + move);
		btVector3 linearVelocity, angularVelocity;
		btTransformUtil::calculateVelocity(from, to, timeStep, linearVelocity, angularVelocity);
		m_pHoldAnchor->setWorldTransform(to);
		m_pHoldAnchor->setInterpolationWorldTransform(to);
		m_pHoldAnchor->getMotionState()->setWorldTransform(to);
		m_pHoldAnchor->setLinearVelocity(linearVelocity);
		m_pHoldAnchor->setAngularVelocity(angularVelocity);
		m_pHoldAnchor->setInterpolationLinearVelocity(linearVelocity);
		m_pHoldAnchor->setInterpolationAngularVelocity(angularVelocity);

		// each other body's velocity moved toward the one that puts it on the drape by the end of this step: all the
		// way at stiffness 1, the share per sixtieth of a second at less
		if (m_fHoldStiffness > 0 && m_holdPose.size() == m_ragDollBoneArray.size())
		{
			btScalar fPull = btScalar(1.0) - btPow(btScalar(1.0) - m_fHoldStiffness, timeStep * btScalar(60.0));
			for (int i = 0; i < m_ragDollBoneArray.size(); i++)
			{
				if (i == m_iHoldBone) continue;
				btRigidBody* pBody = m_ragDollBoneArray[i]->GetRigidBody();
				if (!pBody) continue;
				btTransform drape = to * m_holdPose[i];
				btVector3 drapeLinear, drapeAngular;
				btTransformUtil::calculateVelocity(pBody->getWorldTransform(), drape, timeStep, drapeLinear, drapeAngular);
				pBody->setLinearVelocity(pBody->getLinearVelocity().lerp(drapeLinear, fPull));
				pBody->setAngularVelocity(pBody->getAngularVelocity().lerp(drapeAngular, fPull));
			}
		}
	}
	if (m_fLimitsRestoreLeft > 0)
	{
		m_fLimitsRestoreLeft -= timeStep;
		if (m_fLimitsRestoreLeft > 0)
		{
			SetHeldJointLimits(m_fLimitsRestoreLeft);
		}
		else
		{
			SetHeldJointLimits(0);
			m_heldJointLimits.clear();
			m_fLimitsRestoreLeft = 0;
		}
	}
}

// GG: the joints at a held body: a hinge widened 80 degrees each way, a cone's swing to 80 degrees (both the same either
// way, so gravity alone decides which way what hangs from it goes); their own limits kept to go back to
void DBProRagDoll::RelaxHeldJoints(btRigidBody* pBody)
{
	if (m_heldJointLimits.size() > 0)
	{
		SetHeldJointLimits(0);
		m_heldJointLimits.clear();
	}
	m_fLimitsRestoreLeft = 0;
	if (!jointManager) return;
	const btScalar fOpen = btRadians(80.0f);
	for (int i = 0; i < m_joints.size(); i++)
	{
		DBProJoint* pDBProJoint = jointManager->GetJoint(m_joints[i]);
		btTypedConstraint* pJoint = pDBProJoint ? pDBProJoint->GetConstraint() : NULL;
		if (!pJoint || (&pJoint->getRigidBodyA() != pBody && &pJoint->getRigidBodyB() != pBody)) continue;
		sHeldJointLimits limits;
		limits.pJoint = pJoint;
		if (pJoint->getConstraintType() == HINGE_CONSTRAINT_TYPE)
		{
			btHingeConstraint* pHinge = (btHingeConstraint*)pJoint;
			limits.fOriginal[0] = pHinge->getLowerLimit();
			limits.fOriginal[1] = pHinge->getUpperLimit();
			limits.fOriginal[2] = 0;
			limits.fRelaxed[0] = btMax(btScalar(-2.8), limits.fOriginal[0] - fOpen);
			limits.fRelaxed[1] = btMin(btScalar(2.8), limits.fOriginal[1] + fOpen);
			limits.fRelaxed[2] = 0;
		}
		else if (pJoint->getConstraintType() == CONETWIST_CONSTRAINT_TYPE)
		{
			btConeTwistConstraint* pCone = (btConeTwistConstraint*)pJoint;
			limits.fOriginal[0] = pCone->getSwingSpan1();
			limits.fOriginal[1] = pCone->getSwingSpan2();
			limits.fOriginal[2] = pCone->getTwistSpan();
			limits.fRelaxed[0] = btMax(limits.fOriginal[0], fOpen);
			limits.fRelaxed[1] = btMax(limits.fOriginal[1], fOpen);
			limits.fRelaxed[2] = limits.fOriginal[2];
		}
		else continue;
		m_heldJointLimits.push_back(limits);
	}
	SetHeldJointLimits(1);
}

// GG: the held body's joints between their own limits (0) and opened up (1)
void DBProRagDoll::SetHeldJointLimits(btScalar fRelaxed)
{
	for (int i = 0; i < m_heldJointLimits.size(); i++)
	{
		const sHeldJointLimits& limits = m_heldJointLimits[i];
		btScalar fLimit[3];
		for (int a = 0; a < 3; a++) fLimit[a] = limits.fOriginal[a] + ((limits.fRelaxed[a] - limits.fOriginal[a]) * fRelaxed);
		if (limits.pJoint->getConstraintType() == HINGE_CONSTRAINT_TYPE)
			((btHingeConstraint*)limits.pJoint)->setLimit(fLimit[0], fLimit[1]);
		else
			((btConeTwistConstraint*)limits.pJoint)->setLimit(fLimit[0], fLimit[1], fLimit[2]);
	}
}

// GG: solver iterations for every joint of this ragdoll and its hold (-1 the world's own)
void DBProRagDoll::SetJointIterations(int iIterations)
{
	if (jointManager)
	{
		for (int i = 0; i < m_joints.size(); i++)
		{
			DBProJoint* pDBProJoint = jointManager->GetJoint(m_joints[i]);
			btTypedConstraint* pJoint = pDBProJoint ? pDBProJoint->GetConstraint() : NULL;
			if (pJoint) pJoint->setOverrideNumSolverIterations(iIterations);
		}
	}
	if (m_pHoldJoint) m_pHoldJoint->setOverrideNumSolverIterations(iIterations);
}

// GG: lets a held body go; the ragdoll falls from where it is and can settle again, its opened joints going back to
// their limits over a second (at once they would fling what hangs from them back)
void DBProRagDoll::Release()
{
	if (m_pHoldJoint)
	{
		SetJointIterations(-1);
		SetHeldBodies(false);
		SetHeldCollision(false);
		m_bHoldEased = false;
		m_bHoldSteppedEased = false;
		m_bHoldDrawnKnown = false;
		if (m_heldJointLimits.size() > 0) m_fLimitsRestoreLeft = 1.0f;
		if (g_dynamicsWorld) g_dynamicsWorld->removeConstraint(m_pHoldJoint);
		delete m_pHoldJoint;
		m_pHoldJoint = NULL;
	}
	if (m_pHoldAnchor)
	{
		if (g_dynamicsWorld) g_dynamicsWorld->removeRigidBody(m_pHoldAnchor);
		delete m_pHoldAnchor->getMotionState();
		delete m_pHoldAnchor;
		m_pHoldAnchor = NULL;
	}
	if (m_pHoldAnchorShape)
	{
		delete m_pHoldAnchorShape;
		m_pHoldAnchorShape = NULL;
	}
	if (m_iHoldBone >= 0)
	{
		for (int i = 0; i < m_ragDollBoneArray.size(); i++)
		{
			btRigidBody* pBoneBody = m_ragDollBoneArray[i]->GetRigidBody();
			if (pBoneBody)
			{
				pBoneBody->forceActivationState(ACTIVE_TAG);
				pBoneBody->activate();
			}
		}
	}
	m_iHoldBone = -1;
}

bool DBProRagDoll::IsHeld()
{
	return m_pHoldJoint != NULL;
}

// GG: no body moving, as static or all asleep
bool DBProRagDoll::IsResting()
{
	return m_bIsStatic || IsSleeping();
}

#ifdef WICKEDENGINE
btAlignedObjectArray<DBProRagDollBone*> DBProRagDoll::GetRagdollBones()
{
	if(m_ragDollBoneArray[0] != nullptr)
	return m_ragDollBoneArray;

	btAlignedObjectArray<DBProRagDollBone*> empty;
	return empty;
}
#endif //WICKEDENGINE
