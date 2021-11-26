#pragma once

#include "game.h"

// This consolidates the contents of several smaller includes
BEGIN_SW_NS

enum
{
	CACHE_SOUND_PRECACHE = 0,
	CACHE_SOUND_PLAY =1
};

void DoTheCache(void);

void InitCheats();

void MapColors(short num,COLOR_MAP cm,short create);
int32_t CONFIG_ReadSetup(void);

DSWActor* WarpPlane(int32_t* x, int32_t* y, int32_t* z, int* sectnum);
bool WarpSectorInfo(sectortype* sect, DSWActor** sp_warp);
DSWActor* Warp(int32_t* x, int32_t* y, int32_t* z, int* sectnum);
inline DSWActor* Warp(int32_t* x, int32_t* y, int32_t* z, sectortype** sect)
{
	int sn = sectnum(*sect);
	auto res = Warp(x, y, z, &sn);
	*sect = sn == -1? nullptr : &sector[sn];
	return res;
}

inline DSWActor* WarpPlane(int32_t* x, int32_t* y, int32_t* z, sectortype** sect)
{
	int sn = sectnum(*sect);
	auto res = WarpPlane(x, y, z, &sn);
	*sect = sn == -1 ? nullptr : &sector[sn];
	return res;
}


void ProcessVisOn(void);
void VisViewChange(PLAYERp pp, int* vis);
void SpawnVis(DSWActor* Parent, sectortype* sect, int x, int y, int z, int amt);

enum TriggerType { TRIGGER_TYPE_REMOTE_SO };

int ActorFollowTrack(DSWActor*, short locktics);
void ActorLeaveTrack(DSWActor*);
void RefreshPoints(SECTOR_OBJECTp sop, int nx, int ny, bool dynamic);
void TrackSetup(void);
void PlaceSectorObject(SECTOR_OBJECTp sop, int newx, int newy);
void PlaceSectorObjectsOnTracks(void);
void PlaceActorsOnTracks(void);
void SetupSectorObject(sectortype* sectnum, short tag);
void PostSetupSectorObject(void);
void VehicleSetSmoke(SECTOR_OBJECTp sop, ANIMATORp animator);
void CollapseSectorObject(SECTOR_OBJECTp sop, int nx, int ny);
void KillSectorObjectSprites(SECTOR_OBJECTp sop);
void MoveSectorObjects(SECTOR_OBJECTp sop, short locktics);

#define TEXT_INFO_TIME (3)
#define TEXT_INFO_Y (40)
#define TEXT_INFO_YOFF (10)
inline constexpr int TEXT_INFO_LINE(int line) { return (TEXT_INFO_Y + ((line)*TEXT_INFO_YOFF)); }

void PutStringInfo(PLAYERp pp, const char* string);


void DoSlidorMatch(PLAYERp pp, short match, bool);
bool TestSlidorMatchActive(short match);
void InterpSectorSprites(sectortype* sect, bool state);

using INTERP_FUNC = void(*)(walltype*, int);

void SetSlidorActive(DSWActor*);
void DoSlidorInterp(DSWActor*, INTERP_FUNC);

int DoBeginJump(DSWActor* actor);
int DoJump(DSWActor* actor);
int DoBeginFall(DSWActor* actor);
int DoFall(DSWActor* actor);
void KeepActorOnFloor(DSWActor* actor);
int DoActorSlide(DSWActor* actor);
int DoActorSectorDamage(DSWActor* actor);
int DoScaleSprite(DSWActor* actor);
int DoActorStopFall(DSWActor* actor);

void InitPlayerSprite(PLAYERp pp);
void InitAllPlayerSprites(void);
void PlayerPanelSetup(void);
void PlayerDeathReset(PLAYERp pp);
void SpawnPlayerUnderSprite(PLAYERp pp);

void DoQuakeMatch(short match);
void ProcessQuakeOn(void);
void ProcessQuakeSpot(void);
void QuakeViewChange(PLAYERp pp, int* z_diff, int* x_diff, int* y_diff, short* ang_diff);
void DoQuake(PLAYERp pp);
bool SetQuake(PLAYERp pp, short tics, short amt);
int SetExpQuake(DSWActor*);
int SetGunQuake(DSWActor*);
int SetPlayerQuake(PLAYERp mpp);
int SetNuclearQuake(DSWActor*);
int SetSumoQuake(DSWActor*);
int SetSumoFartQuake(DSWActor*);

END_SW_NS
