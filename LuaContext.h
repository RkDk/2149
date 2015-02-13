#ifndef LUACONTEXT_H_
#define LUACONTEXT_H_

#include "SoraIncludes.h"
#include "EntityManager.h"
#include "ShipEntity.h"

class CLuaContext : public CGameContext< CTOFNEntityManager >
{

public:

    virtual void SetMaxEnemyCount( int n ) = 0;
    virtual CShipEntity * GetPlayerEntity() = 0;
    virtual CShipEntity * CreatePlayerEntity() = 0;
    virtual CShipEntity * CreateRandomEnemyEntity() = 0;
    virtual CAIEntity * FireBulletFrom( int type, float x, float y, float dmg, float speed ) = 0;
    virtual void FireBulletFrom( int type, CShipEntity * pShip, float speed ) = 0;
    virtual void AddEnemyToGenQueue( int, float ) = 0;
    virtual void ClearGenQueue() = 0;
    
	CLuaContext() : CGameContext() { }
	virtual ~CLuaContext() { }

};

#endif
