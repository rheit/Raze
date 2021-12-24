//-------------------------------------------------------------------------
/*
Copyright (C) 1997, 2005 - 3D Realms Entertainment

This file is part of Shadow Warrior version 1.2

Shadow Warrior is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

Original Source: 1997 - Frank Maddin and Jim Norwood
Prepared for public release: 03/28/2005 - Charlie Wiederhold, 3D Realms
*/
//-------------------------------------------------------------------------
#include "ns.h"
#include "build.h"

#include "names2.h"
#include "game.h"
#include "tags.h"
#include "weapon.h"
#include "sprite.h"

//#include "ai.h"

BEGIN_SW_NS

SECTOR_OBJECTp DetectSectorObjectByWall(WALLp);

void SOwallmove(SECTOR_OBJECTp sop, DSWActor* actor, WALLp find_wallp, int dist, int *nx, int *ny)
{
    int j,k,wallcount;
    WALLp wp;
    short startwall,endwall;
    SECTORp *sectp;

    if (!actor->hasU() || TEST(sop->flags, SOBJ_SPRITE_OBJ))
        return;

    auto u = actor->u();
    wallcount = 0;
    for (sectp = sop->sectp, j = 0; *sectp; sectp++, j++)
    {

        // move all walls in sectors back to the original position
        for (auto& wal : wallsofsector(*sectp))
        {
            // find the one wall we want to adjust
            if (&wal == find_wallp)
            {
                short ang;
                // move orig x and y in saved angle
                ang = u->sang;

                *nx = MulScale(dist, bcos(ang), 14);
                *ny = MulScale(dist, bsin(ang), 14);

                sop->xorig[wallcount] -= *nx;
                sop->yorig[wallcount] -= *ny;

                SET(sop->flags, SOBJ_UPDATE_ONCE);
                return;
            }

            wallcount++;
        }
    }
}

int DoWallMove(DSWActor* actor)
{
    int dist,nx,ny;
    short shade1,shade2,ang,picnum1,picnum2;
    bool found = false;
    short dang;
    bool SOsprite = false;

    auto sp = &actor->s();

    dist = SP_TAG13(actor);
    ang = SP_TAG4(sp);
    picnum1 = SP_TAG5(sp);
    picnum2 = SP_TAG6(sp);
    shade1 = SP_TAG7(sp);
    shade2 = SP_TAG8(sp);
    dang = ((int)SP_TAG10(sp)) << 3;

    if (dang)
        ang = NORM_ANGLE(ang + (RandomRange(dang) - dang/2));

    nx = MulScale(dist, bcos(ang), 14);
    ny = MulScale(dist, bsin(ang), 14);

    for(auto& wal : wall)
    {
        if (wal.pos.X == sp->pos.X && wal.pos.Y == sp->pos.Y)
        {
            found = true;

            if (TEST(wal.extra, WALLFX_SECTOR_OBJECT))
            {
                SECTOR_OBJECTp sop;
                sop = DetectSectorObjectByWall(&wal);
                ASSERT(sop);
                SOwallmove(sop, actor, &wal, dist, &nx, &ny);

                SOsprite = true;
            }
            else
            {
                wal.move(sp->pos.X + nx, sp->pos.Y + ny);
            }

            if (shade1)
                wal.shade = int8_t(shade1);
            if (picnum1)
                wal.picnum = picnum1;

            // find the previous wall
            auto prev_wall = PrevWall(&wal);
            if (shade2)
                prev_wall->shade = int8_t(shade2);
            if (picnum2)
                prev_wall->picnum = picnum2;
        }
    }

    SP_TAG9(sp)--;
    if ((int8_t)SP_TAG9(sp) <= 0)
    {
        KillActor(actor);
    }
    else
    {
        if (SOsprite)
        {
            // move the sprite offset from center
            actor->user.sx -= nx;
            actor->user.sy -= ny;
        }
        else
        {
            sp->pos.X += nx;
            sp->pos.Y += ny;
        }
    }

    return found;
}

bool CanSeeWallMove(SPRITEp wp, short match)
{
    int i;
    bool found = false;
    SPRITEp sp;

    SWStatIterator it(STAT_WALL_MOVE_CANSEE);
    while (auto actor = it.Next())
    {
        sp = &actor->s();

        if (SP_TAG2(actor) == match)
        {
            found = true;

            if (cansee(wp->pos.X, wp->pos.Y, wp->pos.Z, wp->sector(), sp->pos.X, sp->pos.Y, sp->pos.Z, sp->sector()))
            {
                return true;
            }
        }
    }

    return !found;
}

int DoWallMoveMatch(short match)
{
    bool found = false;

    // just all with the same matching tags
    SWStatIterator it(STAT_WALL_MOVE);
    while (auto actor = it.Next())
    {
        if (SP_TAG2(actor) == match)
        {
            found = true;
            DoWallMove(actor);
        }
    }

    return found;
}


#include "saveable.h"

static saveable_code saveable_wallmove_code[] =
{
    SAVE_CODE(DoWallMove),
    SAVE_CODE(CanSeeWallMove),
    SAVE_CODE(DoWallMoveMatch),
};

saveable_module saveable_wallmove =
{
    // code
    saveable_wallmove_code,
    SIZ(saveable_wallmove_code),

    // data
    nullptr,0
};
END_SW_NS
