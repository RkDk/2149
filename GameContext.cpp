#include "GameContext.h"


#ifdef __APPLE__
#include "CoreFoundation/CoreFoundation.h"
#endif

CTOFNContext::CTOFNContext() : CLuaContext(), m_pPlayerEntity( NULL ), m_MaxEnemyCount( 3 ), m_CurEnemyCount( 0 ), m_NextEnemySpawn( 0 ), m_PlayerEXP( 0 )
{

}

void CTOFNContext::InitializePhysicsWorld()
{

    m_PhysicsWorld.Initialize( 0.0f, 0.0f );
    m_CollisionCallback.SetContext( this );
    m_PhysicsWorld.SetContactListener( &m_CollisionCallback );

}

void CTOFNContext::InitializeLua()
{

    m_Lua.Initialize();
    SetLuaContext( this );
    
    m_Lua.ReadFile( "scripts/main.lua" );

}


void CTOFNContext::InitializeGraphics()
{

    int id = m_pGraphicsContext->LoadShaderProgram( "star.v", "star.f" );
    int width, height;

     m_pGraphicsContext->GetWindowSize( &width, &height );
    
    m_StarEngine.Init( MAX_STARS, 1 );

    for( int j = 1; j >= 0; j-- )
    {

        int id = m_pGraphicsContext->GetShaderIDFromIndex( j );

        m_pDrawContext->UseShaderProgram( id );

        m_pDrawContext->Calculate2DProjectionMatrix( width, height );

        CMatrix< float > viewMat;
        viewMat.Identity();
        m_pDrawContext->UpdateViewMatrix( &viewMat );

    }

}

void CTOFNContext::InitializeData()
{

    LoadEnemyData();

}

void CTOFNContext::LoadEnemyData()
{
    
    Log::Debug( "Reading enemy data" );

    CJsonObject jsonData;
    jsonData.Open( "enemy.txt" );

    rapidjson::Document & doc = jsonData.GetDocument();
  
    int nEnemy = doc.Size();

    for( rapidjson::SizeType j = 0; j < nEnemy; j++ )
    {

		CEnemyData d;

        d.m_Health = doc[j]["health"].GetInt();
        d.m_SpeedMin = doc[j]["speedmin"].GetInt();
        d.m_SpeedMax = doc[j]["speedmax"].GetInt();
        d.m_Width = d.m_CollisionBoxWidth = doc[j]["width"].GetInt();
        d.m_Height = d.m_CollisionBoxHeight = doc[j]["height"].GetInt();

		if( doc[j].HasMember( "colwidth" ) )
			d.m_CollisionBoxWidth = doc[j]["colwidth"].GetInt();

		if( doc[j].HasMember( "colheight" ) )
			d.m_CollisionBoxHeight = doc[j]["colheight"].GetInt();

        d.m_Sprite = doc[j]["sprite"].GetString();

		if( doc[j].HasMember( "gun" ) )
		{

			rapidjson::Value & gunarray = doc[j]["gun"];

			if( gunarray.IsArray() )
			{

				rapidjson::SizeType nGun = doc[j]["gun"].Size();
				for( rapidjson::SizeType i = 0; i < nGun; i++ )
				{

					rapidjson::Value & v = doc[j]["gun"][i];
                    d.m_GunDmg.push_back( v["dmg"].GetDouble() );
					d.m_GunPos.push_back( Vector2< float >( v["x"].GetDouble(), v["y"].GetDouble() ) );

				}

			}

		}

		m_EnemyData.push_back( d );

    }

    Log::Debug( "Done reading enemy data" );

}

CShipEntity * CTOFNContext::CreatePlayerEntity()
{

    static const float defaultPlayerPosX = 350.0f;
    static const float defaultPlayerPosY = 500.0f;

	Log::Debug( "Creating player entity" );

    CShipEntity * ent = new CShipEntity;
	ent->SetContext( this );
    ent->SetClassTypeID( ENTTYPE_PLAYER );
	ent->CreatePhysicsBody( m_PhysicsWorld.GetPhysicsWorld(), 64, 64 );
    ent->SetMaterial( m_pTextureFactory->GetObjectContent( "player2.png" ) );
	ent->SetSize( 80, 80 );
    ent->DisablePhysicsMovement();
    ent->SetGravity( 0 );
    ent->SetPos( defaultPlayerPosX, defaultPlayerPosY );
    ent->SetDrawDepth( 1 );
    ent->SetHealth( 200.0f );
    ent->SetArmor( 0.0f );

    ent->AddGun( .3f, .23f, 5.0f );
	ent->AddGun( .7f, .23f, 5.0f );

    if( m_pPlayerEntity )
        m_pEntityManager->RemoveEntity( m_pPlayerEntity );

    m_pPlayerEntity = ent;

    m_pEntityManager->AddEntity( m_pPlayerEntity );

	Log::Debug( "Created player entity" );

    return m_pPlayerEntity;

}

void CTOFNContext::FireBulletFrom( int type, CShipEntity * pShip, float speed )
{

    const std::vector< Vector3< float > > & GunPos = pShip->GetGunPositions();
    float pX, pY;

    pShip->GetPos().Get( &pX, &pY );

    for( int j = 0; j < GunPos.size(); j++ )
    {

        float x, y;
        float dmg = pShip->GetGunDamage( j );

        GunPos[j].Get( &x, &y );
        
        FireBulletFrom( type, pX + x, pY + y, dmg, speed );

    }

}

CParticleExplosion * CTOFNContext::CreateExplosion( int type, float x, float y )
{
    
    float range = -1.0f;
    float size = 60.0f;
    bool mega = false;
    
    if( Util::RandomNumber( 1, 10 ) == 2 ) {
     
        range = 4000.0f;
        size = 120.0f;
        mega = true;
        
    }
    
    CParticleExplosion * p = new CParticleExplosion;
    p->Init( ( mega ) ? 2 : 1, range, x, y );
    m_pExplosions.push_back( p );
    
    CWorldEntity * ent = new CWorldEntity;

	ent->SetContext( this );
    ent->SetClassTypeID( ENTTYPE_EXPLOSION );
    
    if( Util::RandomNumber( 1, 10 ) < 2 )
        ent->SetClassType( "XPL-NODMG" );
    else
        ent->SetClassType( "XPL" );

    ent->SetPos( x - size * .5f, y - size * .5f );
    ent->CreatePhysicsBody( m_PhysicsWorld.GetPhysicsWorld(), size, size );
    
    p->SetExplosionEntity( ent );
    m_pEntityManager->QueueEntity( ent );
    
    return p;

}

CAIEntity * CTOFNContext::FireBulletFrom( int type, float x, float y, float dmg, float speed )
{

    CAIEntity * ent = new CAIEntity;
    CBulletAI * aic = new CBulletAI;
    
    float r, g, b;
    float sx = 1.0f, sy = 1.0f;
    
    std::string sprite = "bullet.png";
    
    if( type == ENTTYPE_ENBULLET ) {
        
        r = 255.0f;
        g = Util::RandomNumber( 50, 150 );
        b = Util::RandomNumber( 50, 150 );
        
        Vector2< float > p = m_pPlayerEntity->GetPos();
        
        float ang = 270.0f;
        
        if( Util::RandomNumber( 1, 10 ) < 8 )
            ang = -Util::AngleBetweenPoints( x, y, p.GetX() + 45 + Util::RandomNumber( -50, 50 ), p.GetY() + 45 + Util::RandomNumber( -50, 50 ) );
        
        if( ang < 0.0f )
            ang += 360.0f;
        
        ang = Util::MinF( ang, 0.0f, 200.0f );
        ang = Util::MaxF( ang, 0.0f, 340.0f );
        
        aic->SetAngle( ang );
        
        sprite = "ball.png";
        sx = .5f;
        sy = .5f;
        
    } else {
     
        r = 0.0f;
        g = 255.0f;
        b = 0.0f;
        
        aic->SetAngle( 90.0f );
        
    }

    aic->SetSpeed( speed );
    aic->SetDamage( dmg );
    aic->SetDirection( ( type & ENTTYPE_ENBULLET )? DIR_DOWN : DIR_UP );

	ent->SetContext( this );
    ent->SetClassTypeID( type );
    ent->SetClassType( "BLT" );
	ent->CreatePhysicsBody( m_PhysicsWorld.GetPhysicsWorld(), 5, 5 );
    ent->SetMaterial( m_pTextureFactory->GetObjectContent( sprite ) );
    ent->DisablePhysicsMovement();
    ent->SetGravity( 0 );
    ent->SetPos( x, y );
    ent->SetScale( sx, sy );
    ent->SetColor( r / 255.0f, g / 255.0f, b / 255.0f, 1.0f );

    aic->SetTargetEntity( ent );
    ent->SetAIController( aic );
    
    if( bIter )
        bulletvec.push_back( ent );
    else
    m_pEntityManager->AddEntity( ent );

    return ent;

}

CShipEntity * CTOFNContext::CreateRandomEnemyEntity()
{

	int width, height;

	m_pGraphicsContext->GetWindowSize( &width, &height );
    
	int type = Util::RandomNumber( 0, m_EnemyData.size() - 1 );
    
    CEnemyData & d = m_EnemyData[type];
    
	float x = Util::RandomNumber( 10, width - 100 );
	float y = Util::RandomNumber( -500, -100 );
    float speed =  Util::RandomNumber( d.m_SpeedMin, d.m_SpeedMax );
        
	return CreateEnemyEntity( type, x, y, speed );

}

int CTOFNContext::CreateRandomEnemyFormation( bool countasenemy ) {
    
    int formationtype = Util::RandomNumber( 0, 3 );
    int enemytype = Util::RandomNumber( 0, m_EnemyData.size() - 1 );
    Vector2< float > offset;
    int spritewidth = 0, spriteheight = 0;
    int formationwidth = 0, formationheight = 0;
    
    std::vector< Vector2< float > > formationpos;
    std::vector< Vector2< float > > enemypos;
    
    CEnemyData & d = m_EnemyData[enemytype];
    
    float speed =  Util::RandomNumber( d.m_SpeedMin, d.m_SpeedMax );
    
    spritewidth = d.m_Width;
    spriteheight = d.m_Height;
    
    switch( formationtype ) {
     
        case 0:
            formationpos.push_back( Vector2< float >( 0.0f, 0.0f ) );
            formationpos.push_back( Vector2< float >( 1.0f, 1.0f ) );
            formationpos.push_back( Vector2< float >( 2.0f, 2.0f ) );
            formationpos.push_back( Vector2< float >( 3.0f, 1.0f ) );
            formationpos.push_back( Vector2< float >( 4.0f, 0.0f ) );
            break;
            
        case 1:
            formationpos.push_back( Vector2< float >( 1.0f, 1.0f ) );
            formationpos.push_back( Vector2< float >( 2.0f, 0.0f ) );
            formationpos.push_back( Vector2< float >( 3.0f, 1.0f ) );
            formationpos.push_back( Vector2< float >( 2.0f, 1.0f ) );
            formationpos.push_back( Vector2< float >( 2.0f, 2.0f ) );
            break;
            
        case 2:
            formationpos.push_back( Vector2< float >( 0.0f, 0.0f ) );
            formationpos.push_back( Vector2< float >( 1.5f, 0.0f ) );
            formationpos.push_back( Vector2< float >( 0.0f, 1.5f ) );
            formationpos.push_back( Vector2< float >( 1.5f, 1.5f ) );
            formationpos.push_back( Vector2< float >( .75f, 3.0f ) );
            break;
            
        case 3:
            formationpos.push_back( Vector2< float >( 0.0f, 0.0f ) );
            formationpos.push_back( Vector2< float >( 0.0f, 1.5f ) );
            formationpos.push_back( Vector2< float >( 0.0f, 3.0f ) );
            formationpos.push_back( Vector2< float >( 0.0f, 4.5f ) );
            formationpos.push_back( Vector2< float >( 0.0f, 6.0f ) );
            break;
            
    }

    for( int i = 0; i < formationpos.size(); i++ ) {
        
        float x = formationpos[i].GetX() * ( float )spritewidth;
        float y = formationpos[i].GetY() * ( float )spriteheight;
        
        enemypos.push_back( Vector2< float >( x, y ) );
        
        float x1 = x + spritewidth;
        float y1 = y + spriteheight;
        
        if( x1 > formationwidth )
            formationwidth = x1;
        
        if( y1 > formationheight )
            formationheight = y1;
        
    }
    
    float halfwidth = formationwidth * .5f;
    
    offset.Set( Util::RandomNumber( -halfwidth, SCREEN_WIDTH - halfwidth ),
                Util::RandomNumber( -formationheight * 2.5f, -formationheight * 3.0f ) );
    
    float flyingangle = 270.0f;
    float anglespeed = Util::RandomNumber( 175, 275 );
    
    for( int i = 0; i < enemypos.size(); i++ ) {
     
        CShipEntity * e = CreateEnemyEntity( enemytype, enemypos[i].GetX() + offset.GetX(), enemypos[i].GetY() + offset.GetY(), speed );
        e->SetCountAsEnemy( countasenemy );
        
        if( formationtype == 3 ) {
         
            CEnemyAI * ai = static_cast< CEnemyAI* >( e->GetAIController() );
            ai->SetFlyingAngle( flyingangle );
            ai->SetFlyingAngleSpeed( anglespeed );
            ai->SetCrazyFlying( true );
            
            float radius = ( 90.0f / anglespeed ) * Util::MinMaxF( speed, -300.0f, 300.0f );
            float flyoffset = cos( flyingangle * DEG2RAD ) * -radius;
            
            e->SetPos( e->GetX() + flyoffset, e->GetY() );
            
            flyingangle += 60.0f;
            
            
        }
        
    }
    
    return enemypos.size();
    
}

CShipEntity * CTOFNContext::CreateEnemyEntity( int type, float x, float y, float speed )
{

    CShipEntity * ent = new CShipEntity;
    CEnemyAI * aic = new CEnemyAI;

	CEnemyData & d = m_EnemyData[type];

    //What a hack
    if( ( int )speed == 0 ) {
     
        speed =  Util::RandomNumber( d.m_SpeedMin, d.m_SpeedMax );
        
    }
    
	ent->SetContext( this );
    ent->SetClassTypeID( ENTTYPE_ENEMY );
    ent->SetClassType( "EN" );
    ent->CreatePhysicsBody( m_PhysicsWorld.GetPhysicsWorld(), d.m_CollisionBoxWidth, d.m_CollisionBoxHeight );
    ent->SetMaterial( m_pTextureFactory->GetObjectContent( d.m_Sprite ) );
	ent->SetSize( d.m_Width, d.m_Height );
    ent->DisablePhysicsMovement();
    ent->SetGravity( 0 );
	ent->SetHealth( d.m_Health );
    ent->SetArmor( 0.0f );
    ent->SetPos( x, y );
    ent->SetDrawDepth( 1 );

    aic->SetEntityContext( this );
    aic->SetSpeed( speed );

	std::vector< Vector2< float > > & v = d.m_GunPos;

	for( int j = 0; j < v.size(); j++ )
	{

		ent->AddGun( v[j].GetX(), v[j].GetY(), d.m_GunDmg[j] );

	}

    aic->SetTargetEntity( ent );
    ent->SetAIController( aic );

    m_pEntityManager->AddEntity( ent );
    m_pEntityManager->TrackEntity( "EN", ent );

    return ent;

}

void CTOFNContext::DoEnemyGenerator()
{
    
    if( SDL_GetTicks() < m_NextEnemySpawn )
        return;

    int n = ( m_MaxEnemyCount - m_CurEnemyCount );
    for( int j = 0; j < n; j++ )
    {

        bool createformation = false;

        
        if( m_CurEnemyCount < m_MaxEnemyCount - 3 )
            if( Util::RandomNumber( 1, 10 ) <= 3 )
                createformation = true;
        
        
        if( !createformation ) {
            
            CShipEntity * e = CreateRandomEnemyEntity();
            e->SetCountAsEnemy( true );
            
        } else
            j += CreateRandomEnemyFormation( true );

    }

    m_CurEnemyCount = m_MaxEnemyCount;

}

COrbEntity * CTOFNContext::CreateOrb( int type, float x, float y ) {
 
    COrbEntity * ent = new COrbEntity;
    COrbAI * aic = new COrbAI;
    
    ent->SetContext( this );
    aic->SetEntityContext( this );
    CTextureImage * m = m_pTextureFactory->GetObjectContent( "orb.png" );
    ent->SetClassTypeID( ENTTYPE_ORB );
    ent->SetDrawDepth( 2 );
    ent->CreatePhysicsBody( m_PhysicsWorld.GetPhysicsWorld(), 20.0f, 20.0f );
    ent->SetMaterial( m);
    ent->SetSize( 15, 15 );
    ent->DisablePhysicsMovement();
    ent->SetGravity( 0 );
    ent->SetPos( x, y );
    
    aic->SetTargetEntity( ent );
    ent->SetAIController( aic );
    
    m_pEntityManager->QueueEntity( ent );
    
    return ent;
    
}

void CTOFNContext::CreateOrbs( int type, int count, float x, float y ) {
 
    for( int i = 0; i < count; i++ ) {
        
        COrbEntity * ent = CreateOrb( type, x, y );
        
    }
    
}

void CTOFNContext::DestroyShip( CShipEntity * e, bool quiet )
{

    if( !quiet )
    {

        const Vector2< float > & pos = e->GetPos();
        Vector2< int > size = e->GetSize();
        
        CreateExplosion( 0, pos.GetX() + ( float )size.GetX() * .5f, pos.GetY() + ( float )size.GetY() * .5f );

    }

    m_pEntityManager->DeleteEntity( e );

    if( SDL_GetTicks() > m_NextEnemySpawn && Util::RandomNumber( 1, 5 ) < 3 )
        m_NextEnemySpawn = SDL_GetTicks() + Util::RandomNumber( 500, 1000 );
    
    if( e->CountAsEnemy() )
        m_CurEnemyCount--;

}

void CTOFNContext::HandleEntityContact( void * pEntityA, int entTypeA, void * pEntityB, int entTypeB )
{

    //Due to the way our collision callback functions, entA is always a player ship or enemy ship
    CShipEntity * entA = static_cast< CShipEntity * >( pEntityA );

	CAIEntity * bulletEnt = NULL;
	CBulletAI * bulletAI = NULL;
	CShipEntity * shipEnt = NULL;
    CWorldEntity * explosionEnt = NULL;
    COrbEntity * orbEnt = NULL;

	bool playerInvolved = ( entTypeA & ( ENTTYPE_PLAYER ) );
	bool bulletCollision = ( entTypeB & ( ENTTYPE_PLYBULLET | ENTTYPE_ENBULLET ) );
    bool explosionCollision = ( entTypeB & ( ENTTYPE_EXPLOSION ) );
	bool playerBullet = ( entTypeB & ( ENTTYPE_PLYBULLET ) );
    bool orbCollision = ( entTypeB & ( ENTTYPE_ORB ) );
    
	if( bulletCollision )
	{

		bulletEnt = static_cast< CAIEntity * >( pEntityB );
		bulletAI = static_cast< CBulletAI * >( bulletEnt->GetAIController() );

    } else if( explosionCollision ) {
      
        explosionEnt = static_cast< CWorldEntity * >( pEntityB );
        
    } else if( orbCollision ) {
    
        orbEnt = static_cast< COrbEntity * >( pEntityB );
        
    } else
	{

		shipEnt = static_cast< CShipEntity * >( pEntityB );

	}
    
    if( !entA->IsActive() || ( bulletCollision && !bulletEnt->IsActive() ) || ( !orbCollision && !explosionCollision && !bulletCollision && !shipEnt->IsActive() ) || ( explosionCollision && !explosionEnt->IsActive() ) || ( orbCollision && !orbEnt->IsActive() ) )
        return;

	if( playerInvolved )
	{

		if( bulletCollision )
		{

			if( !playerBullet )
			{

                entA->Damage( bulletAI->GetDamage() );
                m_pEntityManager->DeleteEntity( bulletEnt );


			}

        } else if( explosionCollision ) {
            
            entA->Damage( 10 );
            
        } else if( entTypeB & ENTTYPE_POWERUP )
		{




        } else if( orbCollision ) {
            
            entA->Heal( 2.0f );
            m_pEntityManager->DeleteEntity( orbEnt );
            
        } else
		{

            shipEnt->Damage( 30 );
		}

	} else if( bulletCollision )
    {
        
        if( playerBullet ) {
            
            float y = entA->GetY();
            int height = entA->GetSize().GetY();
            
            if( y > -height - 20 ) {

                entA->Damage( bulletAI->GetDamage() );
                m_pEntityManager->DeleteEntity( bulletEnt );

                return;
            
            }

        }
        
    } else if( explosionCollision ) {
     
        if( explosionEnt->GetClassType() == "XPL" )
            if( entA->GetPos().GetY() > -10.0f )
                entA->Damage( 30 );
        
    }

}

void CTOFNContext::CreateStarBackground()
{

    int winWidth, winHeight;

    m_pGraphicsContext->GetWindowSize( &winWidth, &winHeight );

    for( int j = 0; j < MAX_STARS; j++ )
    {

        CStar * s = new CStar;

        s->m_X = Util::RandomNumber( 0, winWidth );
        s->m_Y = Util::RandomNumber( -500, winHeight );

		RandomizeStar( s );

        m_pStars.push_back( s );

    }

}

void CTOFNContext::RandomizeStar( CStar * s )
{

	s->m_Size = Util::RandomNumber( 1, 3 );

	int c = Util::RandomNumber( 0, 25 );

	s->m_R = 1.0f;
	s->m_G = 1.0f;
	s->m_B = 1.0f;

	if( c == 5 )
	{

		s->m_G = 0.7f;
		s->m_B = 0.7f;

	} else if( c == 12 )
	{

		s->m_R = 0.7f;
		s->m_G = 0.7f;

	}

	s->m_A = ( float )Util::RandomNumber( 20, 180 ) / 255.0f;

	s->m_Speed = Util::RandomNumber( 50, 200 );

}

void CTOFNContext::AddEnemyToGenQueue( int i, float p ) {
 
    m_EnemyGenQueue.push_back( CEnemyGenQueue( i, p ) );
    
}

void CTOFNContext::ClearGenQueue() {
 
    m_EnemyGenQueue.clear();
    
}

void CTOFNContext::UpdateAllEntities()
{

    boost::ptr_vector< CEntityObject > * entityObjs = &m_pEntityManager->GetEntityObjects();
    bIter = true;
	for( boost::ptr_vector< CEntityObject >::iterator i = entityObjs->begin();
		 i != entityObjs->end(); i++ )
    {

        if( ( *i ).GetContent()->IsActive() )
        {

            ( *i ).GetContent()->Update();

            int type = ( *i ).GetContent()->GetClassTypeID();

            if( type & ( ENTTYPE_PLAYER | ENTTYPE_ENEMY ) )
            {

                CShipEntity * pShip = static_cast< CShipEntity * >( ( *i ).GetContent() );

                if( pShip->GetHealth() <= 0 )
                {

                    CreateOrbs( 0, Util::RandomNumber( 3, 6 ), pShip->GetX(), pShip->GetY() );
                    DestroyShip( pShip, false );

                }

                if( pShip->GetPos().GetY() > m_pGraphicsContext->GetWindowHeight() )
                {

                    DestroyShip( pShip, true );

                }

            } else if( type & ( ENTTYPE_ENBULLET | ENTTYPE_PLYBULLET ) )
			{

				CAIEntity * pBullet = static_cast< CAIEntity * >( ( *i ).GetContent() );

				if( ( pBullet->GetPos().GetY() < 0 && type & ENTTYPE_PLYBULLET ) ||
                    ( pBullet->GetPos().GetY() < m_pGraphicsContext->GetWindowHeight() * -.5f && type & ENTTYPE_ENBULLET ) ||
                    pBullet->GetPos().GetY() > m_pGraphicsContext->GetWindowHeight() )
					m_pEntityManager->DeleteEntity( pBullet );

			}

        }

    }
    bIter = false;
    for( int j = 0; j < bulletvec.size(); j++ ) {
     
        m_pEntityManager->AddEntity( bulletvec[j] );
        
    }
    
    bulletvec.clear();

}

void CTOFNContext::DrawStarBackground()
{

    m_pTextureFactory->GetObjectContent( "star.png" )->Bind();

	int width, height;

	m_pGraphicsContext->GetWindowSize( &width, &height );

    int n = 0;
    for( ; n < MAX_STARS; n++ )
    {
        
        CStar * s = &m_pStars[n];
        
        m_StarEngine.SetParticleSize( n, s->m_Size, s->m_Size );
        m_StarEngine.SetParticleColor( n, s->m_R, s->m_G, s->m_B, s->m_A );
        m_StarEngine.SetParticlePos( n, s->m_X, s->m_Y );

        //We'll update in the same loop to save some cpu cycles
        s->m_Y += s->m_Speed * m_FrameDelta;

        if( s->m_Y > 800.0f )
        {

            s->m_X = Util::RandomNumber( 0, width );
            s->m_Y = Util::RandomNumber( -height, -5 );

			RandomizeStar( s );

        }


    }
    
    m_StarEngine.BindVertexBuffers();

    m_pDrawContext->UseShaderProgram( m_pGraphicsContext->GetShaderIDFromIndex( 1 ) );
    
    m_StarEngine.Draw();
    
	m_pDrawContext->UseShaderProgram( m_pGraphicsContext->GetShaderIDFromIndex( 0 ) );
    
    
    m_pDrawContext->Bind2DVertexBuffer();

}

void CTOFNContext::DrawExplosions() {
    
    m_pTextureFactory->GetObjectContent( "pixel.png" )->Bind();
    
    m_pDrawContext->UseShaderProgram( m_pGraphicsContext->GetShaderIDFromIndex( 1 ) );
    
    
    boost::ptr_vector< CParticleExplosion >::iterator iter = m_pExplosions.begin();
    for( ; iter != m_pExplosions.end(); iter++ ) {
        
        ( *iter ).Draw();
    }
    
    m_pDrawContext->UseShaderProgram( m_pGraphicsContext->GetShaderIDFromIndex( 0 ) );
    
    
    m_pDrawContext->Bind2DVertexBuffer();

    
}

void CTOFNContext::UpdateExplosions() {
 
    boost::ptr_vector< CParticleExplosion >::iterator iter = m_pExplosions.begin();
    
    for( ;iter != m_pExplosions.end(); ) {
     
        if( ( *iter ).KillMe() ) {
            m_pEntityManager->DeleteEntity( ( *iter ).GetExplosionEntity() );
            iter = m_pExplosions.erase( iter );
        } else {
         
            ( *iter ).Think( GetFrameDelta() );
            iter++;
            
        }
    }
    
}

void CTOFNContext::GameLogic()
{

    m_PhysicsWorld.Update();

    UpdateAllEntities();

    DoEnemyGenerator();
    
    UpdateExplosions();
    
    m_Lua.CallEngineFunction( "GameLogic" );
    
    //In normal operations, entities should be "deleted", when this happens they aren't actually removed from the entity manager until after all update logic is finished.
    m_pEntityManager->RemoveAllDeletedEntities();
    
    m_pEntityManager->AddAllQueuedEntities();

}
