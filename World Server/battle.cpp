/*
    Rose Online Server Emulator
    Copyright (C) 2006,2007 OSRose Team http://www.osrose.net
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2
    of the License, or (at your option) any later version.
    
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    
    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

    depeloped with Main erose/hrose source server + some change from the original eich source        
*/
#include "character.h"
#include "worldserver.h"

void CCharacter::DoAttack( )
{
    CMap* map = GServer->MapList.Index[Position->Map];
    switch(Battle->atktype)
    {                    
        case NORMAL_ATTACK://normal attack         
        {            
            CCharacter* Enemy = GetCharTarget( );
            if(Enemy==NULL) 
            {
                ClearBattle( Battle );
                return;
            }
            if(IsTargetReached( Enemy ) && CanAttack( )) 
                NormalAttack( Enemy );
        }
        break;
        case SKILL_ATTACK://skill attack
        {
            CCharacter* Enemy = GetCharTarget( );
            if(Enemy==NULL) 
            {
                ClearBattle( Battle );
                return;
            }
            CSkills* skill = GServer->GetSkillByID( Battle->skillid );
            if(skill==NULL)
            {
                ClearBattle( Battle );
                return;
            }
            if(IsTargetReached( Enemy, skill ) && CanAttack( )) 
                SkillAttack( Enemy, skill );        
        }
        break;
        case SKILL_BUFF://buffs
        {
            CCharacter* Enemy = GetCharTarget( );
            if(Enemy==NULL) 
            {
                ClearBattle( Battle );
                return;
            }
            CSkills* skill = GServer->GetSkillByID( Battle->skillid );
            if(skill==NULL)
            {
                ClearBattle( Battle );
                return;
            }
            if(IsTargetReached( Enemy, skill ) && CanAttack( )) 
                BuffSkill( Enemy, skill );
        }
        case SKILL_AOE:
        case SKILL_SELF:            
        case AOE_TARGET:
        {
            CCharacter* Enemy = NULL;
            CSkills* skill = GServer->GetSkillByID( Battle->skillid );
            if(skill==NULL)
            {
                ClearBattle( Battle );
                return;
            }                 
            if(Battle->atktype==AOE_TARGET)
            {
                Enemy = GetCharTarget( );
                if(Enemy==NULL) 
                {
                    ClearBattle( Battle );
                    return;
                }     
                if(IsTargetReached( Enemy, skill ) && CanAttack( )) 
                    AoeSkill( skill, Enemy );                           
            }   
            else
            {
                AoeSkill( skill, Enemy );  
            }
        }
        break;
        case BUFF_SELF:
        {
            CSkills* skill = GServer->GetSkillByID( Battle->skillid );
            if(skill==NULL)
            {
                ClearBattle( Battle );
                return;                
            }          
            BuffSkill( this, skill );              
        }
        break;
        case BUFF_AOE:
        {
            CSkills* skill = GServer->GetSkillByID( Battle->skillid );
            if(skill==NULL)
            {
                ClearBattle( Battle );
                return;
            }  
            AoeBuff( skill );                      
        }
        break;
    }    
}

// do normal attack
void CCharacter::NormalAttack( CCharacter* Enemy )
{
    Position->destiny = Position->current;
    reduceItemsLifeSpan( false );
    Enemy->OnBeAttacked( this );
    float attack = (float)Stats->Attack_Power - ((Enemy->Stats->Magic_Defense+Enemy->Stats->Defense )/2);
    if(attack<0) attack = 5;
    attack *= 0.65;
    float d_attack = attack / 100;
    if(attack<0) attack = 5;
    attack += (float)GServer->RandNumber( 0, 7 ) - 2;
    attack += ((Stats->Level - Enemy->Stats->Level) * d_attack);
    if(attack<7) attack = 5;
    long int hitpower = (long int)floor(attack + GServer->RandNumber(0, 4));
    /*if(IsPlayer( ))
        hitpower = (long int)floor(attack * 1.2 );*/
    if(IsPlayer( )) //temp fix to find balance btw monster and player
        hitpower = (long int)floor(attack * (GServer->Config.PlayerDmg/100.00));
    if(IsMonster( )) //temp fix to find balance btw monster and player
        hitpower = (long int)floor(attack * (GServer->Config.MonsterDmg/100.00));
    bool critical = false;
    if(hitpower<=0)
    {
        hitpower = 0;
    }
    else
    {
        if(GServer->RandNumber(0,300)<Stats->Critical)
        {
            hitpower = (long int)floor(hitpower*1.5);
            critical = true;
        }
    }
    // dodge
    unsigned int hitvalue = (unsigned int)floor(Stats->Accury * 50 / Enemy->Stats->Dodge);
    if(hitvalue>100) hitvalue = 100;
    if(GServer->RandNumber( 0, 100 )>hitvalue)
        hitpower = 0; // dodged
    if(!Enemy->IsSummon( ) && Enemy->IsMonster( ))
    {
        Enemy->AddDamage( this, hitpower );
    }
    Enemy->Stats->HP -= hitpower;
    
    //if (IsPlayer())
    //printf("Target suffered %i physical damage, %i HP still remain \n", hitpower, Enemy->Stats->HP);
    //else printf("Mob did %i physical damage, %i HP still remain \n", hitpower, Enemy->Stats->HP);
    Enemy->reduceItemsLifeSpan(true);
    BEGINPACKET( pak, 0x799 );
    ADDWORD    ( pak, clientid );
    ADDWORD    ( pak, Battle->atktarget );
    ADDDWORD   ( pak, hitpower );
    if(Enemy->IsDead())
    {
        CDrop* thisdrop = NULL;
        ADDDWORD   ( pak, critical?28:16 );
        if(!Enemy->IsSummon( ) && !Enemy->IsPlayer( ))
        {
            thisdrop = Enemy->GetDrop( );
            if(thisdrop!=NULL)
            {
                ADDFLOAT   ( pak, thisdrop->pos.x*100 );
                ADDFLOAT   ( pak, thisdrop->pos.y*100 );
                if(thisdrop->type==1)
                {
                    ADDDWORD( pak, 0xccccccdf );
                    ADDDWORD( pak, thisdrop->amount );
                    ADDDWORD( pak, 0xcccccccc );
                    ADDWORD ( pak, 0xcccc );
                }
                else
                {
                    ADDDWORD   ( pak, GServer->BuildItemHead( thisdrop->item ) );
                    ADDDWORD   ( pak, GServer->BuildItemData( thisdrop->item ) );
                    ADDDWORD( pak, 0x00000000 );
                    ADDWORD ( pak, 0x0000 );
                }
                ADDWORD    ( pak, thisdrop->clientid );
                ADDWORD    ( pak, thisdrop->owner );
                CMap* map = GServer->MapList.Index[thisdrop->posMap];
                map->AddDrop( thisdrop );
            }
        }
        GServer->SendToVisible( &pak, Enemy, thisdrop );
        OnEnemyDie( Enemy );
    }
    else
    {
        ADDDWORD   ( pak, (hitpower>0?(critical?12:0):0) );
        GServer->SendToVisible( &pak, Enemy );
    }
    ReduceABC( );
    Battle->lastAtkTime = clock( );
}

// do skill attack
bool CCharacter::SkillAttack( CCharacter* Enemy, CSkills* skill )
{
    Position->destiny = Position->current;        
    if(Battle->castTime==0)
    {
        BEGINPACKET( pak, 0x7bb );
        ADDWORD    ( pak, clientid );
        GServer->SendToVisible( &pak, this );        
        Battle->castTime = clock();
        return true;
    }
    else
    {
        clock_t etime = clock() - Battle->castTime;
        if(etime<SKILL_DELAY)
            return true;      
    }
    Battle->castTime = 0;      
    UseAtkSkill( Enemy, skill );
    Stats->MP -= (skill->mp - (skill->mp * Stats->MPReduction / 100));   
    if(Stats->MP<0) Stats->MP=0;   
    if(Battle->contatk)
    {
        Battle->atktype = NORMAL_ATTACK;
        Battle->skilltarget = 0;
        Battle->atktarget = Battle->target;
        Battle->skillid = 0;  
    }
    else
    {
        ClearBattle( Battle );
    }          
    GServer->DoSkillScript( this, skill );       //So far only used for summons
    Battle->lastAtkTime = clock( );    
    return true;    
}

// do buff skill
bool CCharacter::BuffSkill( CCharacter* Target, CSkills* skill )
{
    Position->destiny = Position->current;        
    if(Battle->castTime==0)
    {
        BEGINPACKET( pak, 0x7bb );
        ADDWORD    ( pak, clientid );
        GServer->SendToVisible( &pak, (CCharacter*)this );          
        Battle->castTime = clock();
        return true;
    }
    else
    {
        clock_t etime = clock() - Battle->castTime;
        if(etime<SKILL_DELAY)
            return true;      
    }
    Battle->castTime = 0;   
    UseBuffSkill( Target, skill );    
    Stats->MP -= (skill->mp - (skill->mp * Stats->MPReduction / 100));      
    if(Stats->MP<0) Stats->MP=0;
    ClearBattle( Battle );
    GServer->DoSkillScript( this, skill );           
    Battle->lastAtkTime = clock( );    
    return true;    
}

// do AoE skill
bool CCharacter::AoeSkill( CSkills* skill, CCharacter* Enemy )
{
    Position->destiny = Position->current;      
    if(Battle->castTime==0)
    {
        BEGINPACKET( pak, 0x7bb );
        ADDWORD    ( pak, clientid );
        GServer->SendToVisible( &pak, (CCharacter*)this );          
        Battle->castTime = clock();
        return true;
    }
    else
    {
        clock_t etime = clock() - Battle->castTime;
        if(etime<SKILL_DELAY)
            return true;      
    }
    Battle->castTime = 0; 
    CMap* map = GServer->MapList.Index[Position->Map];
    for(UINT i=0;i<map->MonsterList.size();i++)
    {
        CMonster* monster = map->MonsterList.at(i);
        if(monster->clientid==clientid) continue;
        if(IsSummon( ) || IsPlayer( ))
        {
            if(monster->IsSummon( ) && (map->allowpvp==0 || monster->owner==clientid)) continue;
        }
        else
        {
            if(!monster->IsSummon( )) continue;
        }
        if(GServer->IsMonInCircle( Position->current,monster->Position->current,(float)skill->aoeradius+1))
            UseAtkSkill( (CCharacter*) monster, skill );                                        
    } 
    if(map->allowpvp!=0 || (IsMonster( ) && !IsSummon( )))
    {
        for(UINT i=0;i<map->PlayerList.size();i++)
        {
            CPlayer* player = map->PlayerList.at(i);
            if(player->clientid==clientid) continue;
            if(GServer->IsMonInCircle( Position->current,player->Position->current,(float)skill->aoeradius+1))
                UseAtkSkill( (CCharacter*) player, skill );                    
        }
    }
    if(Enemy!=NULL)
    {
        if(!Enemy->IsDead( ))
        {
            Battle->atktarget = Battle->target;
            Battle->atktype = NORMAL_ATTACK;
            Battle->skilltarget = 0;
            Battle->skillid = 0;            
        }
        else ClearBattle( Battle );        
    }
    else ClearBattle( Battle );
    Stats->MP -= (skill->mp - (skill->mp * Stats->MPReduction / 100));   
    if(Stats->MP<0) Stats->MP=0;     
    Battle->lastAtkTime = clock( );    
    return true;    
}

bool CCharacter::AoeBuff( CSkills* skill )
{
    Position->destiny = Position->current;        
    if(Battle->castTime==0)    
    {
        BEGINPACKET( pak, 0x7bb );
        ADDWORD    ( pak, clientid );
        GServer->SendToVisible( &pak, (CCharacter*)this );          
        Battle->castTime = clock();
        return true;
    }
    else
    {
        clock_t etime = clock() - Battle->castTime;
        if(etime<SKILL_DELAY)
            return true;      
    }
    Battle->castTime = 0;  
    CMap* map = GServer->MapList.Index[Position->Map];   
    if(skill->target==1 && GetParty( )==NULL)
    {
        UseBuffSkill( this, skill );
        ClearBattle( Battle );
        Battle->lastAtkTime = clock( ); 
        return true;
    } 
    for(UINT i=0;i<map->PlayerList.size();i++)
    {
        CPlayer* player = map->PlayerList.at(i);
        switch(skill->target)
        {
            case 1: // party
            {
                if(player->Party->party==GetParty( ))
                {
                    if(GServer->IsMonInCircle( Position->current,player->Position->current,(float)skill->aoeradius+1))
                        UseBuffSkill( (CCharacter*)player, skill ); 
                }
            }
        }
    }
    Stats->MP -= (skill->mp - (skill->mp * Stats->MPReduction / 100));   
    if(Stats->MP<0) Stats->MP=0;     
    ClearBattle( Battle );
    Battle->lastAtkTime = clock( );      
    return true;
}


// use skill attack
void CCharacter::UseAtkSkill( CCharacter* Enemy, CSkills* skill, bool deBuff )
{        
    reduceItemsLifeSpan( false );
    Enemy->reduceItemsLifeSpan(true);
    //Skill power calculations
    long int skillpower = skill->atkpower + (long int)floor(GetInt( )/2);    
    long int level_diff = Stats->Level - Enemy->Stats->Level;
    skillpower -= Enemy->Stats->Magic_Defense;
    if(Enemy->IsMonster()){
        if(level_diff >= 1){
            skillpower += Stats->Attack_Power * (level_diff / 5) + (level_diff*2);
        }else{
            skillpower += Stats->Attack_Power - (level_diff / 2);
        }
    }
    //Tell enemy he's attacked & add damage & send the dmg packet
    bool bflag = false;
    Enemy->OnBeAttacked( this );
    if(skillpower<=0) skillpower = 0;
    if(!Enemy->IsSummon( ) && Enemy->IsMonster( ))
    {
        Enemy->AddDamage( this, skillpower );
    }
    Enemy->Stats->HP -= skillpower;
    
    BEGINPACKET( pak, 0x7b6 );
    ADDWORD    ( pak, Enemy->clientid );
    ADDWORD    ( pak, clientid );
    ADDDWORD   ( pak, 0x000007f8 );
    ADDBYTE    ( pak, 0x00 );
    ADDDWORD   ( pak, skillpower ); 
    
    //If Enemy is killed
    if(Enemy->IsDead())
    {
        CDrop* thisdrop = NULL;
        ADDDWORD   ( pak, 16 );
        if(!Enemy->IsSummon( ) && !Enemy->IsPlayer( ))
        {
            thisdrop = Enemy->GetDrop( );
            if(thisdrop!=NULL)
            {
                ADDFLOAT   ( pak, thisdrop->pos.x*100 );
                ADDFLOAT   ( pak, thisdrop->pos.y*100 );
                if(thisdrop->type==1)
                {
                    ADDDWORD( pak, 0xccccccdf );
                    ADDDWORD( pak, thisdrop->amount );
                    ADDDWORD( pak, 0xcccccccc );
                    ADDWORD ( pak, 0xcccc );
                }
                else
                {
                    ADDDWORD   ( pak, GServer->BuildItemHead( thisdrop->item ) );
                    ADDDWORD   ( pak, GServer->BuildItemData( thisdrop->item ) );
                    ADDDWORD( pak, 0x00000000 );
                    ADDWORD ( pak, 0x0000 );
                }
                ADDWORD    ( pak, thisdrop->clientid );
                ADDWORD    ( pak, thisdrop->owner );
                CMap* map = GServer->MapList.Index[thisdrop->posMap];
                map->AddDrop( thisdrop );
            }
        }
        GServer->SendToVisible( &pak, Enemy, thisdrop );
        OnEnemyDie( Enemy );
    }
    
    //If enemy is still alive
    else
    {
        ADDDWORD   ( pak, 4 );
        GServer->SendToVisible( &pak, Enemy );
        
        if (deBuff) return;
        
        //GOTO debuffing section
        bflag = GServer->AddDeBuffs( skill, Enemy, GetInt( ) );
        
        //Send (de)buff information to the whole world
        if(skill->nbuffs>0 && bflag)
        {
            BEGINPACKET( pak, 0x7b5 );
            ADDWORD    ( pak, Battle->skilltarget );
            ADDWORD    ( pak, clientid );
            ADDWORD    ( pak, Battle->skillid );
            ADDWORD    ( pak, GetInt( ) );
            ADDBYTE    ( pak, skill->nbuffs );
            GServer->SendToVisible( &pak, Enemy );
        }
    }
    if (deBuff) return;
    //Send skill animation to the world
    RESETPACKET( pak, 0x7b9);
    ADDWORD    ( pak, clientid);
    ADDWORD    ( pak, Battle->skillid);
    ADDWORD    ( pak, 1);
    GServer->SendToVisible( &pak, this );
}

// use buff skill
void CCharacter::UseBuffSkill( CCharacter* Target, CSkills* skill )
{
    bool bflag = false;       
    bflag = GServer->AddBuffs( skill, Target, GetInt( ) ); 
    if(skill->nbuffs>0 && bflag == true)
    {  
        BEGINPACKET( pak, 0x7b5 );
        ADDWORD    ( pak, Target->clientid );
        ADDWORD    ( pak, clientid );    
        ADDWORD    ( pak, Battle->skillid );
        ADDWORD    ( pak, GetInt( ) );
        ADDBYTE    ( pak, skill->nbuffs );  
        GServer->SendToVisible( &pak, Target ); 
    }                                   
    BEGINPACKET( pak, 0x7b9);
    ADDWORD    ( pak, clientid);
    ADDWORD    ( pak, Battle->skillid);
    ADDWORD    ( pak, 1);
	GServer->SendToVisible( &pak, (CCharacter*)this );                          
}
