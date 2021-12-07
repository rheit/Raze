//-------------------------------------------------------------------------
/*
Copyright (C) 2010-2019 EDuke32 developers and contributors
Copyright (C) 2019 sirlemonhead, Nuke.YKT
This file is part of PCExhumed.
PCExhumed is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License version 2
as published by the Free Software Foundation.
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.
You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/
//-------------------------------------------------------------------------
#include "ns.h"
#include "engine.h"
#include "aistuff.h"
#include "sequence.h"
#include "exhumed.h"
#include "sound.h"
#include "player.h"
#include "names.h"
#include <string.h>
#include <assert.h>

BEGIN_PS_NS

enum { kMaxBullets		= 500 };


// 32 bytes
struct Bullet
{
    TObjPtr<DExhumedActor*> pActor;
    TObjPtr<DExhumedActor*> pEnemy;

    int16_t nSeq;
    int16_t nFrame;
    int16_t nRunRec;
    int16_t nRunRec2;
    int16_t nType;
    int16_t nPitch;
    int16_t field_E;
    uint16_t field_10;
    uint8_t field_12;
    uint8_t nDoubleDamage;
    int x;
    int y;
    int z;
};

FreeListArray<Bullet, kMaxBullets> BulletList;
int lasthitz, lasthitx, lasthity;
sectortype* lasthitsect;

size_t MarkBullets()
{
    for (int i = 0; i < kMaxBullets; i++)
    {
        GC::Mark(BulletList[i].pActor);
        GC::Mark(BulletList[i].pEnemy);
    }
    return 2 * kMaxBullets;
}

int nRadialBullet = 0;

FSerializer& Serialize(FSerializer& arc, const char* keyname, Bullet& w, Bullet* def)
{
    static Bullet nul;
    if (!def)
    {
        def = &nul;
        if (arc.isReading()) w = {};
    }
    if (arc.BeginObject(keyname))
    {
        arc("seq", w.nSeq, def->nSeq)
            ("frame", w.nFrame, def->nFrame)
            ("sprite", w.pActor, def->pActor)
            ("type", w.nType, def->nType)
            ("x", w.x, def->x)
            ("y", w.y, def->y)
            ("z", w.z, def->z)
            ("at6", w.nRunRec, def->nRunRec)
            ("at8", w.nRunRec2, def->nRunRec2)
            ("atc", w.nPitch, def->nPitch)
            ("ate", w.field_E, def->field_E)
            ("at10", w.field_10, def->field_10)
            ("at12", w.field_12, def->field_12)
            ("at13", w.nDoubleDamage, def->nDoubleDamage)
            ("enemy", w.pEnemy, def->pEnemy)
            .EndObject();
    }
    return arc;
}

void SerializeBullet(FSerializer& arc)
{
    if (arc.BeginObject("bullets"))
    {
        arc ("list", BulletList)
            ("lasthitx", lasthitx)
            ("lasthity", lasthity)
            ("lasthitz", lasthitz)
            ("lasthitsect", lasthitsect)
            ("radialbullet", nRadialBullet)
            .EndObject();
    }
}

bulletInfo BulletInfo[] = {
    { 25,   1,    20, -1, -1, 13, 0,  0, -1 },
    { 25,  -1, 65000, -1, 31, 73, 0,  0, -1 },
    { 15,  -1, 60000, -1, 31, 73, 0,  0, -1 },
    { 5,   15,  2000, -1, 14, 38, 4,  5,  3 },
    { 250, 100, 2000, -1, 33, 34, 4, 20, -1 },
    { 200, -1,  2000, -1, 20, 23, 4, 10, -1 },
    { 200, -1, 60000, 68, 68, -1, -1, 0, -1 },
    { 300,  1,     0, -1, -1, -1, 0, 50, -1 },
    { 18,  -1,  2000, -1, 18, 29, 4,  0, -1 },
    { 20,  -1,  2000, 37, 11, 30, 4,  0, -1 },
    { 25,  -1,  3000, -1, 44, 36, 4, 15, 90 },
    { 30,  -1,  1000, -1, 52, 53, 4, 20, 48 },
    { 20,  -1,  3500, -1, 54, 55, 4, 30, -1 },
    { 10,  -1,  5000, -1, 57, 76, 4,  0, -1 },
    { 40,  -1,  1500, -1, 63, 38, 4, 10, 40 },
    { 20,  -1,  2000, -1, 60, 12, 0,  0, -1 },
    { 5,   -1, 60000, -1, 31, 76, 0,  0, -1 }
};


void InitBullets()
{
    BulletList.Clear();
}

int GrabBullet()
{
    int grabbed = BulletList.Get();
    if (grabbed < 0) return -1;
    BulletList[grabbed].pEnemy = nullptr;
    return grabbed;
}

void DestroyBullet(int nBullet)
{
    DExhumedActor* pActor = BulletList[nBullet].pActor;
	auto pSprite = &pActor->s();

    runlist_DoSubRunRec(BulletList[nBullet].nRunRec);
    runlist_DoSubRunRec(pSprite->lotag - 1);
    runlist_SubRunRec(BulletList[nBullet].nRunRec2);

    StopActorSound(pActor);

    DeleteActor(pActor);
    BulletList.Release(nBullet);
}

void IgniteSprite(DExhumedActor* pActor)
{
	auto pSprite = &pActor->s();

    pSprite->hitag += 2;

    auto pAnimActor = BuildAnim(nullptr, 38, 0, pSprite->x, pSprite->y, pSprite->z, pSprite->sector(), 40, 20);
    
    if (pAnimActor)
    {
        pAnimActor->pTarget = pActor;
        ChangeActorStat(pAnimActor, kStatIgnited);

        int yRepeat = (tileHeight(pAnimActor->s().picnum) * 32) / nFlameHeight;
        if (yRepeat < 1)
            yRepeat = 1;

        pAnimActor->s().yrepeat = (uint8_t)yRepeat;
    }
}

void BulletHitsSprite(Bullet *pBullet, DExhumedActor* pBulletActor, DExhumedActor* pHitActor, int x, int y, int z, sectortype* pSector)
{
    assert(pSector != nullptr);

    bulletInfo *pBulletInfo = &BulletInfo[pBullet->nType];

    auto pHitSprite = &pHitActor->s();
    int nStat = pHitSprite->statnum;

    switch (pBullet->nType)
    {
        case 3:
        {
            if (nStat > 107 || nStat == kStatAnubisDrum) {
                return;
            }

            pHitSprite->hitag++;

            if (pHitSprite->hitag == 15) {
                IgniteSprite(pHitActor);
            }

            if (!RandomSize(2)) {
                BuildAnim(nullptr, pBulletInfo->field_C, 0, x, y, z, pSector, 40, pBulletInfo->nFlags);
            }

            return;
        }
        case 14:
        {
            if (nStat > 107 || nStat == kStatAnubisDrum) {
                return;
            }
            // else - fall through to below cases
            [[fallthrough]];
        }
        case 1:
        case 2:
        case 8:
        case 9:
        case 12:
        case 13:
        case 15:
        case 16:
        {
            // loc_29E59
            if (!nStat || nStat > 98) {
                break;
            }

            DExhumedActor* pActor = pBullet->pActor;
            spritetype *pSprite = &pActor->s();

            if (nStat == kStatAnubisDrum)
            {
                int nAngle = (pSprite->ang + 256) - RandomSize(9);

                pHitSprite->xvel = bcos(nAngle, 1);
                pHitSprite->yvel = bsin(nAngle, 1);
                pHitSprite->zvel = (-(RandomSize(3) + 1)) << 8;
            }
            else
            {
                int xVel = pHitSprite->xvel;
                int yVel = pHitSprite->yvel;

                pHitSprite->xvel = bcos(pSprite->ang, -2);
                pHitSprite->yvel = bsin(pSprite->ang, -2);

                MoveCreature(pHitActor);

                pHitSprite->xvel = xVel;
                pHitSprite->yvel = yVel;
            }

            break;
        }

        default:
            break;
    }

    // BHS_switchBreak:
    int nDamage = pBulletInfo->nDamage;

    if (pBullet->nDoubleDamage > 1) {
        nDamage *= 2;
    }

    runlist_DamageEnemy(pHitActor, pBulletActor, nDamage);

    if (nStat <= 90 || nStat >= 199)
    {
        BuildAnim(nullptr, pBulletInfo->field_C, 0, x, y, z, pSector, 40, pBulletInfo->nFlags);
        return;
    }

    switch (nStat)
    {
        case kStatDestructibleSprite:
            break;
        case kStatAnubisDrum:
        case 102:
        case kStatExplodeTrigger:
        case kStatExplodeTarget:
            BuildAnim(nullptr, 12, 0, x, y, z, pSector, 40, 0);
            break;
        default:
            BuildAnim(nullptr, 39, 0, x, y, z, pSector, 40, 0);
            if (pBullet->nType > 2)
            {
                BuildAnim(nullptr, pBulletInfo->field_C, 0, x, y, z, pSector, 40, pBulletInfo->nFlags);
            }
            break;
    }
}


void BackUpBullet(int *x, int *y, int nAngle)
{
    *x -= bcos(nAngle, -11);
    *y -= bsin(nAngle, -11);
}

int MoveBullet(int nBullet)
{
    DExhumedActor* hitactor = nullptr;
    sectortype* pHitSect = nullptr;
    walltype* pHitWall = nullptr;

    Bullet *pBullet = &BulletList[nBullet];
    int nType = pBullet->nType;
    bulletInfo *pBulletInfo = &BulletInfo[nType];

    DExhumedActor* pActor = BulletList[nBullet].pActor;
    spritetype *pSprite = &pActor->s();

    int x = pSprite->x;
    int y = pSprite->y;
    int z = pSprite->z; // ebx
    int nSectFlag = pSprite->sector()->Flag;

    int x2, y2, z2;

    int nVal = 0;
    Collision coll;

    if (pBullet->field_10 < 30000)
    {
        DExhumedActor* pEnemyActor = BulletList[nBullet].pEnemy;
        if (pEnemyActor)
        {
            if (!(pEnemyActor->s().cstat & 0x101))
                BulletList[nBullet].pEnemy = nullptr;
            else
            {
                coll = AngleChase(pActor, pEnemyActor, pBullet->field_10, 0, 16);
                goto MOVEEND;
            }
        }
        if (nType == 3)
        {
            if (pBullet->field_E < 8)
            {
                pSprite->xrepeat -= 1;
                pSprite->yrepeat += 8;

                pBullet->z -= 200;

                if (pSprite->shade < 90) {
                    pSprite->shade += 35;
                }

                if (pBullet->field_E == 3)
                {
                    pBullet->nSeq = 45;
                    pBullet->nFrame = 0;
                    pSprite->xrepeat = 40;
                    pSprite->yrepeat = 40;
                    pSprite->shade = 0;
                    pSprite->z += 512;
                }
            }
            else
            {
                pSprite->xrepeat += 4;
                pSprite->yrepeat += 4;
            }
        }

        coll = movesprite(pActor, pBullet->x, pBullet->y, pBullet->z, pSprite->clipdist >> 1, pSprite->clipdist >> 1, CLIPMASK1);

MOVEEND:
        if (coll.type || coll.exbits)
        {
            nVal = 1;
            x2 = pSprite->x;
            y2 = pSprite->y;
            z2 = pSprite->z;
            pHitSect = pSprite->sector();

#if 0
            // Original code. This was producing some beautiful undefined behavior in the first case because the index can be anything, not just a wall.
            if (coll.exbits)
            {
                hitwall = coll & 0x3FFF;
                goto HITWALL;
            }
            else
            {
                switch (coll & 0xc000)
                {
                case 0x8000:
                    hitwall = coll & 0x3FFF;
                    goto HITWALL;
                case 0xc000:
                    hitsprite = coll & 0x3FFF;
                    goto HITSPRITE;
                }
            }
#else
            switch (coll.type)
            {
            case kHitWall:
                pHitWall = coll.hitWall;
                goto HITWALL;
            case 0xc000:
                if (!coll.exbits)
                {
                    hitactor = coll.actor();
                    goto HITSPRITE;
                }
            }
#endif
        }

        nVal = coll.type || coll.exbits? 1:0;

        // pSprite's sector may have changed since we set nSectFlag ?
        int nFlagVal = nSectFlag ^ pSprite->sector()->Flag;
        if (nFlagVal & kSectUnderwater)
        {
            DestroyBullet(nBullet);
            nVal = 1;
        }

        if (nVal == 0 && nType != 15 && nType != 3)
        {
            AddFlash(pSprite->sector(), pSprite->x, pSprite->y, pSprite->z, 0);

            if (pSprite->pal != 5) {
                pSprite->pal = 1;
            }
        }
    }
    else
    {
        nVal = 1;

        if (BulletList[nBullet].pEnemy)
        {
            hitactor = BulletList[nBullet].pEnemy;
            auto hitsprite = &hitactor->s();
            x2 = hitsprite->x;
            y2 = hitsprite->y;
            z2 = hitsprite->z - (GetActorHeight(hitactor) >> 1);
            pHitSect = hitsprite->sector();
        }
        else
        {
            vec3_t startPos = { x, y, z };
            HitInfo hit{};
            int dz;
            if (bVanilla)
                dz = -bsin(pBullet->nPitch, 3);
            else
                dz = -pBullet->nPitch * 512;
            hitscan(startPos, pSprite->sector(), { bcos(pSprite->ang), bsin(pSprite->ang), dz }, hit, CLIPMASK1);
            x2 = hit.hitpos.x;
            y2 = hit.hitpos.y;
            z2 = hit.hitpos.z;
            hitactor = hit.actor();
            pHitSect = hit.hitSector;
            pHitWall = hit.hitWall;
        }

        lasthitx = x2;
        lasthity = y2;
        lasthitz = z2;
        lasthitsect = pHitSect;

        if (hitactor)
        {
HITSPRITE:
            auto hitsprite = &hitactor->s();
            if (pSprite->pal == 5 && hitsprite->statnum == 100)
            {
                int nPlayer = GetPlayerFromActor(hitactor);
                if (!PlayerList[nPlayer].bIsMummified)
                {
                    PlayerList[nPlayer].bIsMummified = true;
                    SetNewWeapon(nPlayer, kWeaponMummified);
                }
            }
            else
            {
                BulletHitsSprite(pBullet, pActor->pTarget, hitactor, x2, y2, z2, pHitSect);
            }
        }
        else if (pHitWall != nullptr)
        {
        HITWALL:
            if (pHitWall->picnum == kEnergy1)
            {
                if (pHitWall->twoSided())
                {
                    int nDamage = BulletInfo[pBullet->nType].nDamage;
                    if (pBullet->nDoubleDamage > 1) {
                        nDamage *= 2;
                    }

                    DExhumedActor* eb = EnergyBlocks[pHitWall->nextSector()->extra];
                    if (eb) runlist_DamageEnemy(eb, pActor, nDamage);
                }
            }
        }

        if (pHitSect != nullptr) // NOTE: hitsect can be -1. this check wasn't in original code. TODO: demo compatiblity?
        {
            if (hitactor == nullptr && pHitWall == nullptr)
            {
                if ((pHitSect->pBelow != nullptr && (pHitSect->pBelow->Flag & kSectUnderwater)) || pHitSect->Depth)
                {
                    pSprite->x = x2;
                    pSprite->y = y2;
                    pSprite->z = z2;
                    BuildSplash(pActor, pHitSect);
                }
                else
                {
                    BuildAnim(nullptr, pBulletInfo->field_C, 0, x2, y2, z2, pHitSect, 40, pBulletInfo->nFlags);
                }
            }
            else
            {
                if (pHitWall != nullptr)
                {
                    BackUpBullet(&x2, &y2, pSprite->ang);

                    if (nType != 3 || RandomSize(2) == 0)
                    {
                        int zOffset = RandomSize(8) << 3;

                        if (!RandomBit()) {
                            zOffset = -zOffset;
                        }

                        // draws bullet puff on walls when they're shot
                        BuildAnim(nullptr, pBulletInfo->field_C, 0, x2, y2, z2 + zOffset + -4096, pHitSect, 40, pBulletInfo->nFlags);
                    }
                }
                else
                {
                    pSprite->x = x2;
                    pSprite->y = y2;
                    pSprite->z = z2;

                    ChangeActorSect(pActor, pHitSect);
                }

                if (BulletInfo[nType].nRadius)
                {
                    nRadialBullet = nType;

                    runlist_RadialDamageEnemy(pActor, pBulletInfo->nDamage, pBulletInfo->nRadius);

                    nRadialBullet = -1;

                    AddFlash(pSprite->sector(), pSprite->x, pSprite->y, pSprite->z, 128);
                }
            }
        }

        DestroyBullet(nBullet);
    }

    return nVal;
}

void SetBulletEnemy(int nBullet, DExhumedActor* pEnemy)
{
    if (nBullet >= 0) {
        BulletList[nBullet].pEnemy = pEnemy;
    }
}

DExhumedActor* BuildBullet(DExhumedActor* pActor, int nType, int nZOffset, int nAngle, DExhumedActor* pTarget, int nDoubleDamage)
{
	auto pSprite = &pActor->s();
    Bullet sBullet;
    bulletInfo *pBulletInfo = &BulletInfo[nType];
    int nPitch = 0;

    if (pBulletInfo->field_4 > 30000)
    {
        if (pTarget)
        {
            spritetype *pTargetSprite = &pTarget->s();

            if (pTargetSprite->cstat & 0x101)
            {
                sBullet.nType = nType;
                sBullet.nDoubleDamage = nDoubleDamage;

                sBullet.pActor = insertActor(pSprite->sector(), 200);
                sBullet.pActor->s().ang = nAngle;

                int nHeight = GetActorHeight(pTarget);

				assert(pTargetSprite->sector());

                BulletHitsSprite(&sBullet, pActor, pTarget, pTargetSprite->x, pTargetSprite->y, pTargetSprite->z - (nHeight >> 1), pTargetSprite->sector());
                DeleteActor(sBullet.pActor);
                return nullptr;
            }
            else
            {
                nPitch = 0;
            }
        }
    }

    int nBullet = GrabBullet();
    if (nBullet < 0) {
        return nullptr;
    }

    sectortype* pSector;

    if (pSprite->statnum == 100)
    {
        pSector = PlayerList[GetPlayerFromActor(pActor)].pPlayerViewSect;
    }
    else
    {
        pSector = pSprite->sector();
    }

    auto pBulletActor = insertActor(pSector, 200);
	auto pBulletSprite = &pBulletActor->s();
    int nHeight = GetActorHeight(pActor);
    nHeight = nHeight - (nHeight >> 2);

    if (nZOffset == -1) {
        nZOffset = -nHeight;
    }

    pBulletSprite->x = pSprite->x;
    pBulletSprite->y = pSprite->y;
    pBulletSprite->z = pSprite->z;

    Bullet *pBullet = &BulletList[nBullet];

    pBullet->pEnemy = nullptr;

    pBulletSprite->cstat = 0;
    pBulletSprite->shade = -64;

    if (pBulletInfo->nFlags & 4) {
        pBulletSprite->pal = 4;
    }
    else {
        pBulletSprite->pal = 0;
    }

    pBulletSprite->clipdist = 25;

    int nRepeat = pBulletInfo->xyRepeat;
    if (nRepeat < 0) {
        nRepeat = 30;
    }

    pBulletSprite->xrepeat = (uint8_t)nRepeat;
    pBulletSprite->yrepeat = (uint8_t)nRepeat;
    pBulletSprite->xoffset = 0;
    pBulletSprite->yoffset = 0;
    pBulletSprite->ang = nAngle;
    pBulletSprite->xvel = 0;
    pBulletSprite->yvel = 0;
    pBulletSprite->zvel = 0;
    pBulletSprite->lotag = runlist_HeadRun() + 1;
    pBulletSprite->extra = -1;
    pBulletSprite->hitag = 0;
    pBulletActor->pTarget = pActor;
    pBulletActor->nPhase = nBullet;

//	GrabTimeSlot(3);

    pBullet->field_10 = 0;
    pBullet->field_E = pBulletInfo->field_2;
    pBullet->nFrame  = 0;

    int nSeq;

    if (pBulletInfo->field_8 != -1)
    {
        pBullet->field_12 = 0;
        nSeq = pBulletInfo->field_8;
    }
    else
    {
        pBullet->field_12 = 1;
        nSeq = pBulletInfo->nSeq;
    }

    pBullet->nSeq = nSeq;

    pBulletSprite->picnum = seq_GetSeqPicnum(nSeq, 0, 0);

    if (nSeq == kSeqBullet) {
        pBulletSprite->cstat |= 0x8000;
    }

    pBullet->nPitch = nPitch;
    pBullet->nType = nType;
    pBullet->pActor = pBulletActor;
    pBullet->nRunRec = runlist_AddRunRec(pBulletSprite->lotag - 1, nBullet, 0xB0000);
    pBullet->nRunRec2 = runlist_AddRunRec(NewRun, nBullet, 0xB0000);
    pBullet->nDoubleDamage = nDoubleDamage;
    pBulletSprite->z += nZOffset;
    pBulletSprite->backuppos();

    int var_18 = 0;

    pSector = pBulletSprite->sector();

    while (pBulletSprite->z < pSector->ceilingz)
    {
        if (pSector->pAbove == nullptr)
        {
            pBulletSprite->z = pSector->ceilingz;
            break;
        }

        pSector = pSector->pAbove;
        ChangeActorSect(pBulletActor, pSector);
    }

    if (pTarget == nullptr)
    {
        var_18 = (-bsin(nPitch) * pBulletInfo->field_4) >> 11;
    }
    else
    {
		auto pTargetSprite = &pTarget->s();

        if ((unsigned int)pBulletInfo->field_4 > 30000)
        {
            BulletList[nBullet].pEnemy = pTarget;
        }
        else
        {
            nHeight = GetActorHeight(pTarget);

            if (pTargetSprite->statnum == 100)
            {
                nHeight -= nHeight >> 2;
            }
            else
            {
                nHeight -= nHeight >> 1;
            }

            int var_20 = pTargetSprite->z - nHeight;

            int x, y;

            if (pActor != nullptr && pSprite->statnum != 100)
            {
                x = pTargetSprite->x;
                y = pTargetSprite->y;

                if (pTargetSprite->statnum != 100)
                {
                    x += (pTargetSprite->xvel * 20) >> 6;
                    y += (pTargetSprite->yvel * 20) >> 6;
                }
                else
                {
                    int nPlayer = GetPlayerFromActor(pTarget);
                    if (nPlayer > -1)
                    {
                        x += PlayerList[nPlayer].nPlayerDX * 15;
                        y += PlayerList[nPlayer].nPlayerDY * 15;
                    }
                }

                x -= pBulletSprite->x;
                y -= pBulletSprite->y;

                nAngle = GetMyAngle(x, y);
                pSprite->ang = nAngle;
            }
            else
            {
                // loc_2ABA3:
                x = pTargetSprite->x - pBulletSprite->x;
                y = pTargetSprite->y - pBulletSprite->y;
            }

            int nSqrt = lsqrt(y*y + x*x);
            if ((unsigned int)nSqrt > 0)
            {
                var_18 = ((var_20 - pBulletSprite->z) * pBulletInfo->field_4) / nSqrt;
            }
            else
            {
                var_18 = 0;
            }
        }
    }

    pBullet->z = 0;
    pBullet->x = (pSprite->clipdist << 2) * bcos(nAngle);
    pBullet->y = (pSprite->clipdist << 2) * bsin(nAngle);
    BulletList[nBullet].pEnemy = nullptr;


    if (MoveBullet(nBullet))
    {
        pBulletActor = nullptr;
    }
    else
    {
        pBullet->field_10 = pBulletInfo->field_4;
        pBullet->x = bcos(nAngle, -3) * pBulletInfo->field_4;
        pBullet->y = bsin(nAngle, -3) * pBulletInfo->field_4;
        pBullet->z = var_18 >> 3;
    }

    return pBulletActor;
}

void AIBullet::Tick(RunListEvent* ev)
{
    int nBullet = RunData[ev->nRun].nObjIndex;
    assert(nBullet >= 0 && nBullet < kMaxBullets);

    int nSeq = SeqOffsets[BulletList[nBullet].nSeq];
    DExhumedActor* pActor = BulletList[nBullet].pActor;
    auto pSprite = &pActor->s();

    int nFlag = FrameFlag[SeqBase[nSeq] + BulletList[nBullet].nFrame];

    seq_MoveSequence(pActor, nSeq, BulletList[nBullet].nFrame);

    if (nFlag & 0x80)
    {
        BuildAnim(nullptr, 45, 0, pSprite->x, pSprite->y, pSprite->z, pSprite->sector(), pSprite->xrepeat, 0);
    }

    BulletList[nBullet].nFrame++;
    if (BulletList[nBullet].nFrame >= SeqSize[nSeq])
    {
        if (!BulletList[nBullet].field_12)
        {
            BulletList[nBullet].nSeq = BulletInfo[BulletList[nBullet].nType].nSeq;
            BulletList[nBullet].field_12++;
        }

        BulletList[nBullet].nFrame = 0;
    }

    if (BulletList[nBullet].field_E != -1 && --BulletList[nBullet].field_E == 0)
    {
        DestroyBullet(nBullet);
    }
    else
    {
        MoveBullet(nBullet);
    }
}

void AIBullet::Draw(RunListEvent* ev)
{
    int nBullet = RunData[ev->nRun].nObjIndex;
    assert(nBullet >= 0 && nBullet < kMaxBullets);

    int nSeq = SeqOffsets[BulletList[nBullet].nSeq];

    ev->pTSprite->statnum = 1000;

    if (BulletList[nBullet].nType == 15)
    {
        seq_PlotArrowSequence(ev->nParam, nSeq, BulletList[nBullet].nFrame);
    }
    else
    {
        seq_PlotSequence(ev->nParam, nSeq, BulletList[nBullet].nFrame, 0);
        ev->pTSprite->ownerActor = nullptr;
    }
}

END_PS_NS
