
// BulletPhysicsWrapper for DarkBasic Proffessional
//Stab In The Dark Software 
//Copyright (c) 2002-2014
//http://stabinthedarksoftware.com


#pragma once

#include "BaseItem.h"
#include "DBProRagDollBone.h"
#include "btBulletDynamicsCommon.h"
#include "preprocessor-flags.h"

class DBProRagDoll : public BaseItem
{
public:
	DBProRagDoll(int ragdollID);
	virtual ~DBProRagDoll();


	btRigidBody* localCreateRigidBody (btScalar mass, const btTransform& startTransform, btCollisionShape* shape, int objID, int collisionGroup, int collisionMask);
	int AddBone(int objID, int limbID1, int LimbID2, btScalar diameter, float lengthmod, int collisionGroup, int collisionMask);
	int AddBone(int objID, int limbID1, int LimbID2, btScalar diameter, int collisionGroup, int collisionMask);
	void AddJoint( int boneIndex1, int boneIndex2, int jointType, int limbID);
	void AddHingeJoint(int boneIndex1, int boneIndex2, int limbID, btVector3& jointRotation, btScalar minRotation, btScalar maxRotation);
	void AddTwistConstraint(int boneIndex1, int boneIndex2, int limbID, btVector3& jointRotation, btVector3& jointlimits);
	void Finalize();
	void Activate();
	void ApplyForce(int iLimbNumber, btVector3& Start, btVector3& End, float fForceValue);
	void Update();
	void HideBones();
	void ShowBones();
	int GetBoneObjID(int boneIndex);
	void AssignLimbIDToBone(int boneIndex, int dbproLimbID);
	void SetDamping(btScalar linear, btScalar angular);
	void SetSleepingThresholds(btScalar linear, btScalar angular);
	void SetDeactivationTime(btScalar time); 
	void ResetObjectParametersForAnimation();
	void ResetObjectParametersForCulling(); 
	bool IsSleeping();
	void SetStatic(bool bStatic);
	void SetTotalWeight(btScalar totalWeight); 
	void SetFriction(btScalar friction);
	bool IsStatic(); 
	bool IsBoneObject(int objectID);
	btAlignedObjectArray<DBProRagDollBone*> GetRagdollBones();

	// GG: a body held at a point (BPhys_RagDollHold), the bodies' turn as if standing facing angle 0, and the body the object follows
	int FindBone(int limbID);
	btVector3 GetBonePosition(int boneIndex);
	void SetRestYaw(btScalar yawDegrees);
	void SetFollowBone(int boneIndex);
	void Hold(int boneIndex, const btVector3& position, const btMatrix3x3& rotation, btScalar fYawDegrees, unsigned int uNowMs);
	void Release();
	bool IsHeld();
	bool IsResting();
	void HoldFrame(btScalar frameTime);
	void HoldStep(btScalar timeStep);
	void SetHoldStiffness(btScalar fStiffness, btScalar fDamping, btScalar fWaistFold, btScalar fHipFold, btScalar fKneeFold);
	void GetHoldStiffness(btScalar* pStiffness, btScalar* pDamping, btScalar* pWaistFold, btScalar* pHipFold, btScalar* pKneeFold);
	int GetHoldKnees(btScalar* pMade, btScalar* pHeld, int iMax);

private:
	void RelaxHeldJoints(btRigidBody* pBody);
	void SetHeldJointLimits(btScalar fRelaxed);
	void SetJointIterations(int iIterations);
	int BoneOfBody(const btRigidBody* pBody);
	void BuildHoldPose(int boneIndex);
	bool HoldPoseJointTurn(int iLink, int iChild, btTransform& turn);
	bool HoldPoseKneeBend(int iHipLink, int iKneeLink, int iThigh, int iShin, const btVector3& right, btScalar& fBend, btVector3& axis, btVector3& pivot);
	void SetHeldBodies(bool bHeld);
	void SetHeldCollision(bool bHeld);

	// a held body drawn where it is held now: the hold's target set after this frame's physics (the script runs after
	// it) is reached by the next, so the object is moved and turned by the difference from where the last update left it
	bool m_bHoldEased;
	bool m_bHoldSteppedEased;
	bool m_bHoldDrawnKnown;
	btScalar m_fHoldTargetYaw;
	btScalar m_fHoldYawStepped;
	btScalar m_fHoldYawAtUpdate;
	btVector3 m_vecObjectAtUpdate;
	btVector3 m_vecObjectAnglesAtUpdate;
	btVector3 m_vecHeldAtUpdate;
	btAlignedObjectArray<int> m_heldGroup;
	btAlignedObjectArray<int> m_heldMask;

	// a joint as made: its bodies, its pivot in the world, and the joint
	struct sJointLink
	{
		int iBoneA;
		int iBoneB;
		btVector3 pivot;
		btTypedConstraint* pJoint;
	};

	// a knee of a held ragdoll: the joint beyond a hip of the held body, its bodies, and its bend in degrees as the
	// ragdoll was made and in the drape
	struct sHoldKnee
	{
		int iHipLink;
		int iKneeLink;
		int iThigh;
		int iShin;
		btScalar fMade;
		btScalar fHeld;
	};

	// the drape a held ragdoll is pulled to: each body's transform from the held body, how hard, its swing damped, and
	// the folds at the joints of the held body (above it, toward the chest by fWaistFold; below, by fHipFold) and at
	// the knees beyond (bent to fKneeFold, none when negative)
	btAlignedObjectArray<btTransform> m_createTransform;
	btScalar m_fRestYaw;
	btAlignedObjectArray<sJointLink> m_jointLinks;
	btAlignedObjectArray<btTransform> m_holdPose;
	btAlignedObjectArray<sHoldKnee> m_holdKnees;
	btAlignedObjectArray<btScalar> m_heldFriction;
	btScalar m_fHoldStiffness;
	btScalar m_fHoldDamping;
	btScalar m_fHoldWaistFold;
	btScalar m_fHoldHipFold;
	btScalar m_fHoldKneeFold;

	struct sHeldJointLimits
	{
		btTypedConstraint* pJoint;
		btScalar fOriginal[3];
		btScalar fRelaxed[3];
	};

	btAlignedObjectArray<btMatrix3x3> m_restRotation;
	int m_iFollowBone;
	int m_iHoldBone;
	btRigidBody* m_pHoldAnchor;
	btCollisionShape* m_pHoldAnchorShape;
	btTypedConstraint* m_pHoldJoint;
	btTransform m_holdFrom;
	unsigned int m_uHoldStartMs;
	btTransform m_holdTarget;
	btScalar m_fHoldTimeLeft;
	btAlignedObjectArray<sHeldJointLimits> m_heldJointLimits;
	btScalar m_fLimitsRestoreLeft;

private:
	void SetCullingRadius(float fRadius, float fScaledRadius, float fScaledLargestRadius);
	void GetCullingRadius(float &outfRadius, float &outfScaledRadius, float &outfScaledLargestRadius);

	btAlignedObjectArray<int> m_joints;
	btAlignedObjectArray<DBProRagDollBone*> m_ragDollBoneArray;

	btScalar m_modelTotalWeight;
	btScalar m_modelTotalVolume;
	btScalar m_modelTotalDensity;
	btScalar m_linearDamping;
	btScalar m_angularDamping;
	btScalar m_linearSleepingThresholds;
	btScalar m_angularSleepingThresholds;
	btScalar m_deactivationTime; 
	bool m_bIsStatic;
	float m_fRadius;
	float m_fScaledRadius;
	float m_fScaledLargestRadius;
	bool m_bNeedsUpdateForCulling;
#ifdef WICKEDENGINE
	int m_iAnimateFromJoint;
#endif
};

extern DBProRagDoll* currentDBProRagDoll;
