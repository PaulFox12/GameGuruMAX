// Force commands

void ODESetLinearVelocity ( int iObject, float fX, float fY, float fZ )
{
	// get reference object
	sObjectList* pPhyObject = ODEFindID ( iObject );
	if ( pPhyObject==NULL ) return;
	if ( pPhyObject->body==NULL ) return;

	// a frozen body (FreezeEntity) keeps still: its zeroed factors scale forces, not a velocity set directly
	if ( pPhyObject->iResponseMode > 0 ) return;

	// apply linear velocity
	physicslock.lock();
	pPhyObject->body->activate();
	pPhyObject->body->setLinearVelocity(btVector3(fX/gSc,fY/gSc,fZ/gSc));
	physicslock.unlock();
}

void ODESetLinearVelocityXZWithGravity ( int iObject, float fX, float fZ, float fGravityForce )
{
	// get reference object
	sObjectList* pPhyObject = ODEFindID ( iObject );
	if ( pPhyObject==NULL ) return;
	if ( pPhyObject->body==NULL ) return;
	if ( pPhyObject->iResponseMode > 0 ) return; // frozen (ODESetLinearVelocity)

	// apply linear velocity for XZ (kinetic but force for gravity)
	pPhyObject->body->activate();
	btVector3 currentVelocity = pPhyObject->body->getLinearVelocity();
	pPhyObject->body->setLinearVelocity(btVector3(fX/gSc,currentVelocity.getY(),fZ/gSc));
	pPhyObject->body->applyForce(btVector3(0,fGravityForce/gSc,0), btVector3(0,0,0));
}

void ODESetLinearVelocityUsingWorldPosTarget (int iObject, float fWorldX, float fWorldY, float fWorldZ)
{
	// NOTE: changed to direct position setting, velocity did not work at all!

	// get reference object
	sObjectList* pPhyObject = ODEFindID (iObject);
	if (pPhyObject == NULL) return;
	if (pPhyObject->body == NULL) return;

	// work out force from current physics position and world xz
	btTransform trans = pPhyObject->body->getWorldTransform();
	fWorldY += pPhyObject->fRaised;
	fWorldX /= gSc; fWorldY /= gSc; fWorldZ /= gSc;
	trans.setOrigin(btVector3(fWorldX, fWorldY, fWorldZ));
	pPhyObject->body->setWorldTransform(trans);
	pPhyObject->body->activate();
}

void ODEAddBodyForce ( int iObject, float fX, float fY, float fZ, float fPX, float fPY, float fPZ )
{
	// GG: SetPhysicsPushLog's object: its body before and after the push, and over the steps after
	bool bLogPush = ( iObject > 0 && iObject == g_iPushLogObject );
	if ( bLogPush )
	{
		char pWhen[128];
		snprintf ( pWhen, sizeof(pWhen), "push %.0f %.0f %.0f at %.0f %.0f %.0f, before", fX, fY, fZ, fPX, fPY, fPZ );
		PhysicsPushLogBody ( pWhen );
	}

	// get reference object
	sObjectList* pPhyObject = ODEFindID ( iObject );
	if ( pPhyObject==NULL ) return;
	if ( pPhyObject->body==NULL ) return;

	// apply force
	pPhyObject->body->activate();
	PhysicsApplyPush(pPhyObject->body, btVector3(fX/gSc,fY/gSc,fZ/gSc), btVector3(fPX/gSc,fPY/gSc,fPZ/gSc));
	if ( bLogPush )
	{
		PhysicsPushLogBody ( "after" );
		g_iPushLogStepsLeft = g_iPushLogSteps;
	}
}

void ODESetAngularVelocity (int iObject, float fX, float fY, float fZ)
{
	// get reference object
	sObjectList* pPhyObject = ODEFindID (iObject);
	if (pPhyObject == NULL) return;
	if (pPhyObject->body == NULL) return;

	// a frozen body keeps still (ODESetLinearVelocity): an explosion's random spin turned it about its centre, through
	// the ground, as its zeroed angular factor let no contact stop it
	if (pPhyObject->iResponseMode > 0) return;

	// apply linear velocity
	pPhyObject->body->activate();
	pPhyObject->body->setAngularVelocity(btVector3(fX / gSc, fY / gSc, fZ / gSc));
}

// GG: a body's spin in radians per second about the world's axes (ODESetAngularVelocity scales its rates down as it
// does lengths, by gSc, and its callers are tuned to that); a frozen body keeps still
void ODESetAngularVelocityRadians (int iObject, float fX, float fY, float fZ)
{
	sObjectList* pPhyObject = ODEFindID (iObject);
	if (pPhyObject == NULL) return;
	if (pPhyObject->body == NULL) return;
	if (pPhyObject->iResponseMode > 0) return;
	physicslock.lock();
	pPhyObject->body->activate();
	pPhyObject->body->setAngularVelocity(btVector3(fX, fY, fZ));
	physicslock.unlock();
}

void ODESetAngularFactor (int iObject)
{
	sObjectList* pPhyObject = ODEFindID (iObject);
	if (pPhyObject == NULL) return;
	if (pPhyObject->body == NULL) return;
	pPhyObject->body->activate();
	pPhyObject->body->setAngularFactor(btVector3(0, 1, 0));
}

void ODEConstrainBodyMotion( int iObjectNumber, float fX, float fY, float fZ )
{
	// get reference object
	sObjectList* pPhyObject = ODEFindID( iObjectNumber );
	if (pPhyObject == NULL) return;
	if (pPhyObject->body == NULL) return;

	pPhyObject->body->activate();
	pPhyObject->body->setLinearFactor( btVector3( fX, fY, fZ ) );
}

void ODEConstrainBodyRotation( int iObjectNumber, float fX, float fY, float fZ )
{
	// get reference object
	sObjectList* pPhyObject = ODEFindID( iObjectNumber );
	if (pPhyObject == NULL) return;
	if (pPhyObject->body == NULL) return;

	pPhyObject->body->activate();
	pPhyObject->body->setAngularFactor( btVector3( fX, fY, fZ ) );
}

void ODERemoveBodyConstraints(int iObjectNumber)
{
	// get reference object
	sObjectList* pPhyObject = ODEFindID( iObjectNumber );
	if (pPhyObject == NULL) return;
	if (pPhyObject->body == NULL) return;

	for ( int i = 0; i < g_dynamicsWorld->getNumConstraints(); i++ )
	{ 
		btTypedConstraint *tC = g_dynamicsWorld->getConstraint( i );
		if ( pPhyObject->body == &( tC->getRigidBodyA() ) ||
			 pPhyObject->body == &( tC->getRigidBodyB() ) )
		{
			g_dynamicsWorld->removeConstraint( tC );
		}
	}
}

void ODERemoveConstraint(int iConstraintId)
{
	btTypedConstraint *tC = g_dynamicsWorld->getConstraint( iConstraintId );
	if ( tC != NULL ) g_dynamicsWorld->removeConstraint( tC );
}

int ODERayTerrainEx (float fX, float fY, float fZ, float fToX, float fToY, float fToZ, int iCollisionType, bool bUpdateDebugObj)
{
	// Cast a ray against the terrain object and see if hit
	int iResult = 0;
	if (g_dynamicsWorld)
	{
		/*
		// show debug if active
		if (g_debugraycastvisual > 0 && bUpdateDebugObj == true)
		{
			if (ObjectExist(g_debugraycastvisual) == 1)
			{
				float fSourceYTweak = fY - 5.0f;
				float fCameraYTweak = fToY - 5.0f;
				float fDX = fToX - fX;
				float fDY = fCameraYTweak - fSourceYTweak;
				float fDZ = fToZ - fZ;
				float fMiddleX = fX + ((fDX) / 2);
				float fMiddleY = fSourceYTweak + ((fDY) / 2);
				float fMiddleZ = fZ + ((fDZ) / 2);
				float fDistance = sqrt(fabs(fDX*fDX) + fabs(fDY*fDY) + fabs(fDZ*fDZ));
				PositionObject(g_debugraycastvisual, fMiddleX, fMiddleY, fMiddleZ);
				PointObject (g_debugraycastvisual, fToX, fCameraYTweak, fToZ);
				ScaleObject (g_debugraycastvisual, 10, 10, fDistance);
			}
		}
		*/

		// do the cast
		btVector3 Start, End, Normal;
		Start = btVector3(fX / gSc, fY / gSc, fZ / gSc);
		End = btVector3(fToX / gSc, fToY / gSc, fToZ / gSc);
		btCollisionWorld::ClosestRayResultCallback RayCallback(Start, End);
		RayCallback.m_collisionFilterGroup = 1 << 2;
		RayCallback.m_collisionFilterMask = iCollisionType;
		RayCallback.m_fEdgeDetectThreshold = 0; // normal
		g_dynamicsWorld->rayTest(Start, End, RayCallback);
		if (RayCallback.hasHit())
		{
			End = RayCallback.m_hitPointWorld;
			Normal = RayCallback.m_hitNormalWorld;
			g_hitObjectNumber = 0;
			g_hitPointWorld = End * gSc;
			g_hitNormalWorld = Normal;
			iResult = 1;
		}
		else
		{
			iResult = 0;
		}
	}
	else
		iResult = 0;

	// return result
	return iResult;
}

void ODESetBodyScaling(int iObjectNumber, float fX, float fY, float fZ)
{
	// get reference object
	sObjectList* pPhyObject = ODEFindID(iObjectNumber);
	if (pPhyObject == NULL) return;
	if (pPhyObject->body == NULL) return;
	btCollisionShape* shape = pPhyObject->body->getCollisionShape();
	shape->setLocalScaling( btVector3( fX, fY, fZ ) );
	//g_dynamicsWorld->updateSingleAabb((btCollisionObject*)pPhyObject->body); resizing live is UNSTABLE
}

// 1 when the last ODERayTerrain hit a tree rather than the ground
int g_iODERayTerrainHitTree = 0;

int ODERayTerrain ( float fX, float fY, float fZ, float fToX, float fToY, float fToZ, bool bUpdateDebugObj)
{
	// the ground first, then the tree trunks up to where the ray met it, so a trunk it reports is always the nearer and one
	// behind a hill is never reported (the trunks are the whole map's, physics_rayintersecttree)
	extern int physics_rayintersecttree (float fX, float fY, float fZ, float fToX, float fToY, float fToZ);
	g_iODERayTerrainHitTree = 0;
	int iGround = ODERayTerrainEx ( fX, fY, fZ, fToX, fToY, fToZ, COL_TERRAIN, bUpdateDebugObj);
	float fEndX = fToX, fEndY = fToY, fEndZ = fToZ;
	if (iGround == 1)
	{
		fEndX = g_hitPointWorld.x(); fEndY = g_hitPointWorld.y(); fEndZ = g_hitPointWorld.z();
	}
	if (physics_rayintersecttree (fX, fY, fZ, fEndX, fEndY, fEndZ) == 1)
	{
		// report where the ray entered the tree, or ODEGetRayCollision* would return the last ray's point
		extern float g_fTreeRayHitX, g_fTreeRayHitY, g_fTreeRayHitZ, g_fTreeRayHitNX, g_fTreeRayHitNY, g_fTreeRayHitNZ;
		g_hitObjectNumber = 0;
		g_hitPointWorld = btVector3(g_fTreeRayHitX, g_fTreeRayHitY, g_fTreeRayHitZ);
		g_hitNormalWorld = btVector3(g_fTreeRayHitNX, g_fTreeRayHitNY, g_fTreeRayHitNZ);
		g_iODERayTerrainHitTree = 1;
		return 1;
	}
	return iGround;
}

int ODERayForce ( float fX, float fY, float fZ, float fToX, float fToY, float fToZ, float fForceValue )
{
	// Cast a ray, and apply force to what it hits first
	if ( g_dynamicsWorld )
	{
		btVector3 Start, End, Normal;
		Start = btVector3(fX/gSc,fY/gSc,fZ/gSc);
		End = btVector3(fToX/gSc,fToY/gSc,fToZ/gSc);
		btCollisionWorld::ClosestRayResultCallback RayCallback(Start, End);
		g_dynamicsWorld->rayTest(Start, End, RayCallback);
		if(RayCallback.hasHit())
		{
			End = RayCallback.m_hitPointWorld;
			Normal = RayCallback.m_hitNormalWorld;
			g_hitObjectNumber = 0;
			g_hitPointWorld = End * gSc;
			g_hitNormalWorld = Normal;
			if ( RayCallback.m_collisionObject->getInternalType()==btCollisionObject::CO_RIGID_BODY )
			{
				// apply force to rigid body
				RayCallback.m_collisionObject->activate();
				btRigidBody* pBody = (btRigidBody*)RayCallback.m_collisionObject;
				btVector3 thisPos = pBody->getWorldTransform().getOrigin();
				btVector3 relPos = End - thisPos;
				btVector3 force = (End-Start);
				force = force / force.length() / gSc;
				force = force * (fForceValue/gSc);
				PhysicsApplyPush(pBody, force, relPos);

				// find which object we hit
				sObject* pFound = NULL;
				for (int j=((int)g_PhyObjectList.size())-1; j>=0 ;j--)
				{
					btRigidBody* findbody = g_PhyObjectList[j].body;
					if ( findbody==pBody )
					{
						pFound = GetObjectData ( g_PhyObjectList[j].iID );
						break;
					}
				}
				if ( pFound )
				{
					g_hitObjectNumber = pFound->dwObjectNumber;
				}
			}
			return 1;
		}
		else
			return 0;
	}
	else
		return 0;
}

int ODERayEx (float fX, float fY, float fZ, float fToX, float fToY, float fToZ, int iCollisionType, int iIgnoreObj1, int iIgnoreObj2)
{
	// cast a physics ray, get nearest hit
	if (g_dynamicsWorld)
	{
		btVector3 Start, End, Normal;
		Start = btVector3(fX / gSc, fY / gSc, fZ / gSc);
		End = btVector3(fToX / gSc, fToY / gSc, fToZ / gSc);
		btCollisionWorld::ClosestRayResultCallback RayCallback(Start, End);
		RayCallback.m_collisionFilterGroup = iCollisionType;
		RayCallback.m_collisionFilterMask = iCollisionType;
		g_dynamicsWorld->rayTest(Start, End, RayCallback); // nice and fast!
		if (RayCallback.hasHit())
		{
			int iHitRegistered = 1;
			g_hitObjectNumber = 0;
			const btRigidBody* hitbody = btRigidBody::upcast(RayCallback.m_collisionObject);
			if (hitbody)
			{
				for (int j = ((int)g_PhyObjectList.size()) - 1; j >= 0; j--)
				{
					btRigidBody* body = g_PhyObjectList[j].body;
					if (body == NULL) continue;
					if (hitbody == body)
					{
						if ((iIgnoreObj1 > 0 && iIgnoreObj1 == g_PhyObjectList[j].iID) || (iIgnoreObj2 > 0 && iIgnoreObj2 == g_PhyObjectList[j].iID))
						{
							// we can ignore this one (added iHitRegistered to ensure ignored hit results in NO detection)
							iHitRegistered = 0;
						}
						else
						{
							// we hit this object
							g_hitObjectNumber = g_PhyObjectList[j].iID;
						}
						break;
					}
				}
			}
			else
			{
				// if ray hits player capsule (ghost) this is also detected!
				g_hitObjectNumber = -1;
			}
			End = RayCallback.m_hitPointWorld;
			Normal = RayCallback.m_hitNormalWorld;
			if (iHitRegistered == 1)
			{
				g_hitPointWorld = End * gSc;
				g_hitNormalWorld = Normal;
			}
			return iHitRegistered;
		}
	}
	return 0;
}

int ODERay (float fX, float fY, float fZ, float fToX, float fToY, float fToZ, int iCollisionType)
{
	return ODERayEx (fX, fY, fZ, fToX, fToY, fToZ, iCollisionType, 0, 0);
}

void ODEGetPosAndNorm(float* pOutX, float* pOutY, float* pOutZ, float* pNormX, float* pNormY, float* pNormZ)
{
	// return last ray hit position and normal
	*pOutX = g_hitPointWorld.getX();
	*pOutY = g_hitPointWorld.getY();
	*pOutZ = g_hitPointWorld.getZ();
	*pNormX = g_hitNormalWorld.getX();
	*pNormY = g_hitNormalWorld.getY();
	*pNormZ = g_hitNormalWorld.getZ();
}

float ODEGetRayCollisionX()
{
	float value = g_hitPointWorld.getX();
	return value;
}

float ODEGetRayCollisionY ( void )
{
	float value = g_hitPointWorld.getY();
	return value;
}

float ODEGetRayCollisionZ ( void )
{
	float value = g_hitPointWorld.getZ();
	return value;
}

float ODEGetRayNormalX	( void )
{
	float value = g_hitNormalWorld.getX();
	return value;
}

float ODEGetRayNormalY	( void )
{
	float value = g_hitNormalWorld.getY();
	return value;
}

float ODEGetRayNormalZ	( void )
{
	float value = g_hitNormalWorld.getZ();
	return value;
}

int ODEGetRayObjectHit ( void )
{
	return g_hitObjectNumber;
}

//
// Set commands
//

void ODESetActive ( int iObjectNumber, int iMode )
{
	sObjectList* pPhyObject = ODEFindID ( iObjectNumber );
	if ( pPhyObject==NULL ) return;
}

void ODESetBodyPosition ( int iObjectNumber, float fX, float fY, float fZ )
{
	sObjectList* pPhyObject = ODEFindID ( iObjectNumber );
	if ( pPhyObject==NULL ) return;

	// change position of physics object
	if ( pPhyObject->body )
	{
		//LB: retain current worldtrans, just adjust origin!
		//btTransform worldTrans;
		//worldTrans.setOrigin(btVector3(fX/gSc,fY/gSc,fZ/gSc));
		//pPhyObject->body->setWorldTransform(worldTrans);
		btTransform trans = pPhyObject->body->getWorldTransform();
		trans.setOrigin(btVector3(fX / gSc, fY / gSc, fZ / gSc));
		pPhyObject->body->setWorldTransform(trans);
		pPhyObject->body->activate();
	}
	else
	{
		// or character if no body (need further qualification here)
		if ( m_character )
		{
			m_character->warp(btVector3(fX/gSc,fY/gSc,fZ/gSc));
		}
	}
}

// the body was made turned by the object's pivot, as physics_setupobject makes a moving body for an entity with a fixnewy,
// so the object is drawn by the body's transform alone (applying the pivot as well would turn the model twice)
void ODESetBodyIncludesPivot ( int iObjectNumber, int iIncludes )
{
	sObjectList* pPhyObject = ODEFindID ( iObjectNumber );
	if ( pPhyObject ) pPhyObject->bPivotInBody = ( iIncludes != 0 );
}

void ODESetBodyResponse ( int iObjectNumber, int iResponseMode )
{
	sObjectList* pPhyObject = ODEFindID ( iObjectNumber );
	if ( pPhyObject )
	{
		// a freeze (any mode above 0) also stops the body and zeroes its linear and angular factors, so gravity, hits
		// and turning cannot move it between steps (zeroing the velocity after each step still let it creep, and mode 2
		// was never read); unfreezing puts back the factors it had, so a body with locked rotation keeps it
		if ( pPhyObject->body )
		{
			bool bWasFrozen = pPhyObject->iResponseMode > 0;
			bool bFreeze = iResponseMode > 0;
			if ( bFreeze && !bWasFrozen )
			{
				pPhyObject->vecUnfrozenLinearFactor = pPhyObject->body->getLinearFactor();
				pPhyObject->vecUnfrozenAngularFactor = pPhyObject->body->getAngularFactor();
				pPhyObject->body->setLinearFactor ( btVector3 ( 0, 0, 0 ) );
				pPhyObject->body->setAngularFactor ( btVector3 ( 0, 0, 0 ) );
				pPhyObject->body->setLinearVelocity ( btVector3 ( 0, 0, 0 ) );
				pPhyObject->body->setAngularVelocity ( btVector3 ( 0, 0, 0 ) );
			}
			if ( !bFreeze && bWasFrozen )
			{
				pPhyObject->body->setLinearFactor ( pPhyObject->vecUnfrozenLinearFactor );
				pPhyObject->body->setAngularFactor ( pPhyObject->vecUnfrozenAngularFactor );
				pPhyObject->body->activate();
			}
		}

		// simply calls setlinearvelocity in physics update each cycle
		pPhyObject->iResponseMode = iResponseMode;
	}
}

void ODESetBodyAngle ( int iObjectNumber, float fXRot, float fYRot, float fZRot )
{
	sObjectList* pPhyObject = ODEFindID ( iObjectNumber );
	if ( pPhyObject==NULL ) return;

	// change position of physics object
	if ( pPhyObject->body )
	{
		btTransform oldTrans = pPhyObject->body->getWorldTransform();
		btTransform worldTrans;
		worldTrans.setIdentity();
		GGQUATERNION QuatAroundX(1,0,0,fXRot);
		GGQUATERNION QuatAroundY(0,1,0,fYRot);
		GGQUATERNION QuatAroundZ(0,0,1,fZRot);
		// angles arrive in degrees (callers pass ObjectAngleX/Y/Z), the quaternion wants radians as in fillTransform
		GGQuaternionRotationAxis(&QuatAroundX,&GGVECTOR3(1,0,0),GGToRadian(fXRot));
		GGQuaternionRotationAxis(&QuatAroundY,&GGVECTOR3(0,1,0),GGToRadian(fYRot));
		GGQuaternionRotationAxis(&QuatAroundZ,&GGVECTOR3(0,0,1),GGToRadian(fZRot));
		GGQUATERNION finalOrientation = QuatAroundX * QuatAroundY * QuatAroundZ;
		worldTrans.setRotation(btQuaternion(finalOrientation.x,finalOrientation.y,finalOrientation.z,finalOrientation.w));

		// turn about the object's pivot, as the object itself turns, keeping the body's offset from the pivot
		// (the collision centre for boxes, none for meshes) as it was when the body was created
		btVector3 vecPos = oldTrans.getOrigin();
		sObject* pObject = GetObjectData(iObjectNumber);
		if ( pObject )
		{
			btVector3 vecPivot(pObject->position.vecPosition.x/gSc, pObject->position.vecPosition.y/gSc, pObject->position.vecPosition.z/gSc);
			btVector3 vecLocalOffset = oldTrans.getBasis().transpose() * (vecPos - vecPivot);
			vecPos = vecPivot + (worldTrans.getBasis() * vecLocalOffset);
		}
		worldTrans.setOrigin(vecPos);
		pPhyObject->body->setWorldTransform(worldTrans);

		// the world does not update the bounds of static bodies each step (setForceUpdateAllAabbs false)
		if ( pPhyObject->body->isStaticObject() && g_dynamicsWorld ) g_dynamicsWorld->updateSingleAabb(pPhyObject->body);
	}
	else
	{
		// or character if no body (need further qualification here)
		if ( m_character )
		{
			m_character->setWalkDirection(btVector3(0,fYRot,0));
		}
	}
}

void ODESetBodyMass ( int iObject, float fPercentage )
{
	sObjectList* pPhyObject = ODEFindID ( iObject );
	if ( pPhyObject==NULL ) return;
	if ( pPhyObject->body )
	{
		fPercentage=fPercentage/100.0f;
		float fOld = pPhyObject->body->getInvMass() * fPercentage;
		btScalar mass(fOld);
		btVector3 localInertia(0,0,0);
		pPhyObject->body->setMassProps(mass,localInertia);
	}
}

void ODESetBodyFriction ( int iObject, float fFriction )
{
	if ( fFriction == -1 ) fFriction = 90.0f;
	btScalar friction(fFriction);
	friction = friction / 100.0f;
	DBProRagDoll* ragdoll = ragdollManager->GetRagdoll(iObject);
	if ( ragdoll )
	{
		// ragdoll object
		ragdoll->SetFriction ( friction );
	}
	else
	{
		// regular object
		sObjectList* pPhyObject = ODEFindID ( iObject );
		if ( pPhyObject==NULL ) return;
		if ( pPhyObject->body )
		{
			pPhyObject->body->setFriction(friction);
		}
	}
}

void ODESetBodyDamping ( int iObject, float fDamp, float fAngleDamp )
{
	if ( fDamp == - 1 ) fDamp = 0.001f;
	if ( fAngleDamp == - 1 ) fAngleDamp = 0.005f;
	sObjectList* pPhyObject = ODEFindID ( iObject );
	if ( pPhyObject==NULL ) return;
	if ( pPhyObject->body )
	{
		pPhyObject->body->setDamping ( fDamp, fAngleDamp );
	}
}

// GG: a projectile body that ends on its first contact: no bounce, swept so a fast one can't step through a thin part
// between steps, and its first contact with another body kept for ODEGetProjectileContact (iIgnoreObject: the thrower's)
void ODEWatchProjectileContact ( int iObjectNumber, int iIgnoreObject )
{
	physicslock.lock();
	sObjectList* pPhyObject = ODEFindID ( iObjectNumber );
	if ( pPhyObject && pPhyObject->body )
	{
		btRigidBody* body = pPhyObject->body;
		body->setRestitution ( 0.0f );
		btVector3 vecMin, vecMax;
		body->getCollisionShape()->getAabb ( btTransform::getIdentity(), vecMin, vecMax );
		const btVector3 vecSize = vecMax - vecMin;
		const btScalar fSmallest = btMin ( vecSize.x(), btMin ( vecSize.y(), vecSize.z() ) );
		body->setCcdMotionThreshold ( fSmallest * 0.5f );
		body->setCcdSweptSphereRadius ( fSmallest * 0.4f );
		sProjectileContact contact;
		contact.iObject = iObjectNumber;
		contact.iIgnoreObject = iIgnoreObject;
		contact.body = body;
		contact.iTouched = -1;
		contact.vecPoint = btVector3 ( 0, 0, 0 );
		contact.vecNormal = btVector3 ( 0, 0, 0 );
		bool bFound = false;
		for ( sProjectileContact& existing : g_ProjectileContacts )
		{
			if ( existing.iObject == iObjectNumber ) { existing = contact; bFound = true; }
		}
		if ( !bFound ) g_ProjectileContacts.push_back ( contact );
	}
	physicslock.unlock();
}

// the object number a watched projectile first touched (0 the terrain or a body with no object), where, and the touched
// surface's normal facing the projectile, or -1
int ODEGetProjectileContact ( int iObjectNumber, float* pX, float* pY, float* pZ, float* pNX, float* pNY, float* pNZ )
{
	int iTouched = -1;
	physicslock.lock();
	for ( const sProjectileContact& contact : g_ProjectileContacts )
	{
		if ( contact.iObject != iObjectNumber || contact.iTouched < 0 ) continue;
		iTouched = contact.iTouched;
		*pX = contact.vecPoint.x();
		*pY = contact.vecPoint.y();
		*pZ = contact.vecPoint.z();
		if ( pNX ) *pNX = contact.vecNormal.x();
		if ( pNY ) *pNY = contact.vecNormal.y();
		if ( pNZ ) *pNZ = contact.vecNormal.z();
	}
	physicslock.unlock();
	return iTouched;
}

DWORD ODEGetBodyLinearVelocityX ( int iObjectNumber )
{
	sObjectList* pPhyObject = ODEFindID ( iObjectNumber );
	if ( pPhyObject==NULL ) return 0;
	const float fValue = pPhyObject->body->getLinearVelocity().getX() * gSc;
	return *(DWORD*)&fValue;
}

DWORD ODEGetBodyLinearVelocityY ( int iObjectNumber )
{
	sObjectList* pPhyObject = ODEFindID ( iObjectNumber );
	if ( pPhyObject==NULL ) return 0;
	const float fValue = pPhyObject->body->getLinearVelocity().getY() * gSc;
	return *(DWORD*)&fValue;
}

DWORD ODEGetBodyLinearVelocityZ ( int iObjectNumber )
{
	sObjectList* pPhyObject = ODEFindID ( iObjectNumber );
	if ( pPhyObject==NULL ) return 0;
	const float fValue = pPhyObject->body->getLinearVelocity().getZ() * gSc;
	return *(DWORD*)&fValue;
}

int ODEGetBodyAttribValue ( int iObjectNumber )
{
	sObjectList* pPhyObject = ODEFindID ( iObjectNumber );
	if ( pPhyObject==NULL ) return 0;
	int iValue = (int)pPhyObject->body->getUserPointer();
	return iValue;
}

bool ODEGetBodyIsDynamic(int iObjectNumber)
{
	for (int i = 0; i < (int)g_PhyObjectList.size(); i++)
	{
		if ( g_PhyObjectList[i].iID == iObjectNumber )
		{
			return g_PhyObjectList[i].bDynamicUpdate;
		}
	}
	return false;
}

// where an object's physics body is, as the pose it would give the object (the world position and angles in degrees,
// as Update sets them for a moving body; a static body never moves its object, so this shows the collider against the
// drawn object), and how many bodies the physics list holds for the object; the pose is the first body's, the one
// ODEDestroyObject removes. A character capsule gives its centre and no angles
int ODEGetBodyPose ( int iObjectNumber, float* pPosition, float* pAngles )
{
	int iBodies = 0;
	physicslock.lock();
	sObject* pObject = GetObjectData ( iObjectNumber );
	for ( int j = 0; j < (int)g_PhyObjectList.size(); j++ )
	{
		if ( g_PhyObjectList[j].iID != iObjectNumber || g_PhyObjectList[j].body == NULL ) continue;
		if ( iBodies == 0 && pObject )
		{
			const btTransform& trans = g_PhyObjectList[j].body->getWorldTransform();
			if ( g_PhyObjectList[j].bCapsuleControl )
			{
				pPosition[0] = trans.getOrigin().getX() * gSc;
				pPosition[1] = trans.getOrigin().getY() * gSc;
				pPosition[2] = trans.getOrigin().getZ() * gSc;
				pAngles[0] = pAngles[1] = pAngles[2] = 0.0f;
			}
			else
			{
				GGMATRIX matWorld;
				GGVECTOR3 vecRotate;
				ODEObjectPoseFromBody ( &g_PhyObjectList[j], pObject, trans, &matWorld, &vecRotate );
				pPosition[0] = matWorld._41;
				pPosition[1] = matWorld._42;
				pPosition[2] = matWorld._43;
				pAngles[0] = vecRotate.x;
				pAngles[1] = vecRotate.y;
				pAngles[2] = vecRotate.z;
			}
		}
		iBodies++;
	}
	physicslock.unlock();
	return iBodies;
}

int	ODEGetBodyNumCollisions( int iObjectNumber )
{
	return GetObjectNumCollisions( iObjectNumber );
}

void ODEGetBodyCollisionDetails( int iObjectNumber, int iColNumber, int &iColObj,
	                             float &fX, float &fY, float &fZ, float &fImpulse )
{
	GetObjectCollisionDetails( iObjectNumber, iColNumber, iColObj, fX, fY, fZ, fImpulse );
}

int	ODEGetTerrainNumCollisions( int iObjectNumber )
{
	return GetTerrainNumCollisions( iObjectNumber );
}

void ODEGetTerrainCollisionDetails( int iObjectNumber, int iColNumber, int &iLatest,
	                               float &fX, float &fY, float &fZ )
{
	GetTerrainCollisionDetails( iObjectNumber, iColNumber, iLatest, fX, fY, fZ );
}

void ODEAddBodyCollisionCheck(int iObjectNumber)
{
	AddObjectCollisionCheck( iObjectNumber );
}

void ODERemoveBodyCollisionCheck(int iObjectNumber)
{
	RemoveObjectCollisionCheck(iObjectNumber);
}

void ODESetWorldGravity ( float fX, float fY, float fZ, float fallspeed)
{
	// set player gravity (assuming 20 is default)
	if ( m_character )
	{
		float fRelative = fabs(fY/20.0f);
		m_character->setGravity((fCharacterGravity*fRelative)/gSc);
		if(fallspeed != 0)
			m_character->setFallSpeed((fallspeed*fRelative) / gSc);
		else
			m_character->setFallSpeed((fCharacterFallSpeed*fRelative)/gSc);
	}
}

void BULLETAnglesFromMatrix ( GGMATRIX* pmatMatrix, GGVECTOR3* pVecAngles )
{
 // from http://www.martinb.com/maths/geometry/rotations/conversions/matrixToEuler/index.htm
 float m00 = pmatMatrix->_11;
 float m01 = pmatMatrix->_12;
 float m02 = pmatMatrix->_13;
 float m12 = pmatMatrix->_23;
 float m22 = pmatMatrix->_33;
 float heading = (float)atan2(m01,m00);
 float attitude = (float)atan2(m12,m22);
 float bank = (float)asin(-m02);

 // check for gimbal lock
 if ( fabs ( m02 ) > 1.0f )
 {
  // looking straight up or down
  float PI = GG_PI / 2.0f;
  pVecAngles->x = 0.0f;
  pVecAngles->y = GGToDegree ( PI * m02 );
  pVecAngles->z = 0.0f;
 }
 else
 {
  pVecAngles->x = GGToDegree ( attitude );
  pVecAngles->y = GGToDegree ( bank );
  pVecAngles->z = GGToDegree ( heading );
 }
}

void ODECreateTerrain ( unsigned int iVertexCount, unsigned int iIndexCount, float* pVertices, int* piIndices )
{
	// MAX does not use this
}

int g_iDebugDrawStatics = 0;

void BPhys_SetDebugDrawerMode(int iMode, int iStatics, int iConstraints)
{
	if (g_iDebugDrawStatics != iStatics)
	{
		g_iDebugDrawStatics = iStatics;
		g_debugDrawer.setDebugMode(0);
	}
	g_debugDrawer.SetDrawTransforms(0);
	if (iMode > 0)
	{
		int mode = btIDebugDraw::DBG_DrawWireframe;
		if (iConstraints)
		{
			mode = btIDebugDraw::DBG_DrawWireframe | btIDebugDraw::DBG_DrawConstraints | btIDebugDraw::DBG_DrawConstraintLimits;
		}
		g_debugDrawer.setDebugMode(mode);
	}
	else
	{
		g_debugDrawer.setDebugMode(iMode);
	}
}

int	BPhys_GetDebugDrawerMode()
{
	return g_debugDrawer.getDebugMode();
}

// Called every frame.
float* BPhys_GetDynamicDebugDrawData(int& size)
{
	// Reset index tracker now that the data has been passed on.
	g_debugDrawer.elementCount = 0;
	size = (int) g_debugDrawer.lineData.size();
	if (size != 0)
	{
		return &g_debugDrawer.lineData[0];
	}
	return nullptr;
}

// Should only be called once.
float* BPhys_GetStaticDebugDrawData(int& size)
{
	btRigidBody* rBody = nullptr;
	btTransform transform;

	// Draw the static rigid bodies in the dynamic world.
	if (g_iDebugDrawStatics & 1)
	{
		for (int i = 0; i < g_PhyObjectList.size(); i++)
		{
			//PE: Dont include terrain, will run out of memory
			bool bDrawThisPhysicsObject = false;
			if (g_PhyObjectList[i].iID > 70000)
			{
				if (g_PhyObjectList[i].bDynamicUpdate == false)
				{
					bDrawThisPhysicsObject = true;
				}
			}
			if (g_PhyObjectList[i].iID <= 70000)
			{
				// if in PROXIMITY to an object with physics (terrain), show that too
				int iObj = g_PhyObjectList[i].iID;
				float fDX = ObjectPositionX(iObj) - CameraPositionX(0);
				float fDZ = ObjectPositionZ(iObj) - CameraPositionZ(0);
				float fDiff = sqrt(fabs(fDX * fDX) + fabs(fDZ * fDZ));
				if (g_iDebugDrawStatics & 2)
				{
					if (fDiff < 10000.0f) bDrawThisPhysicsObject = true;
				}
			}
			if (bDrawThisPhysicsObject == true)
			{
				rBody = g_PhyObjectList[i].body;
				if (rBody && rBody->getInvMass() == 0.0f)
				{
					rBody->getMotionState()->getWorldTransform(transform);
					g_dynamicsWorld->debugDrawObject(transform, rBody->getCollisionShape(), btVector3(0, 1, 0));
				}
				rBody = nullptr;
			}
		}

		// Reset index tracker now that the data has been passed on.
		size = (int)g_debugDrawer.lineData.size();
		if (size > 0) return &g_debugDrawer.lineData[0];
	}
	else
	{
		// no static to  create
		size = 0;
		return nullptr;
	}
}

void BPhys_ClearDebugDrawData()
{
	#ifdef WICKEDENGINE
	g_debugDrawer.lineData.clear();
	g_debugDrawer.elementCount = 0;
	g_debugDrawer.iRagdollID.clear();
	g_debugDrawer.iDrawObjects.clear();
	#endif
}

void BPhys_AddDebugSingleObject(int objectID)
{
	#ifdef WICKEDENGINE
	g_debugDrawer.iDrawObjects.push_back(objectID);
	#endif
}

void BPhys_RemoveDebugSingleObject(int objectID)
{
	#ifdef WICKEDENGINE
	for (int i = 0; i < g_debugDrawer.iDrawObjects.size(); i++)
	{
		if (g_debugDrawer.iDrawObjects[i] == objectID) 
		{
			g_debugDrawer.iDrawObjects.erase(g_debugDrawer.iDrawObjects.begin() + i);
			break;
		}
	}
	#endif
}

void BPhys_DrawDebugObjects()
{
	btRigidBody* rBody;
	btTransform transform;
	for (int i = 0; i < g_PhyObjectList.size(); i++)
	{
		for (int j = 0; j < g_debugDrawer.iDrawObjects.size(); j++)
		{
			if (g_debugDrawer.iDrawObjects[j] == g_PhyObjectList[i].iID)
			{
				rBody = nullptr;
				rBody = g_PhyObjectList[i].body;
				if (rBody == nullptr) continue;

				rBody->getMotionState()->getWorldTransform(transform);
				g_dynamicsWorld->debugDrawObject(transform, rBody->getCollisionShape(), btVector3(0, 1, 0));
				
				rBody = nullptr;
			}
		}
	}
}

int BPhys_GetDebugObjectCount()
{
	#ifdef WICKEDENGINE
	return (int) g_debugDrawer.iDrawObjects.size();
	#else
	return 0;
	#endif
}
