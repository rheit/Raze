#pragma once

#include "coreactor.h"

BEGIN_BLD_NS

class DBloodActor;

struct SPRITEHIT
{
	Collision hit, ceilhit, florhit;
};

class DBloodActor : public DCoreActor
{
	DBloodActor* base();

public:
	int dudeSlope;
	int xvel, yvel, zvel;
	bool hasx;
	XSPRITE xsprite;
	SPRITEHIT hit;
	DUDEEXTRA dudeExtra;
	SPRITEMASS spriteMass;
	GENDUDEEXTRA genDudeExtra;
	DBloodActor* prevmarker;	// needed by the nnext marker code. This originally hijacked targetX in XSPRITE
	DBloodActor* ownerActor;	// was previously stored in the sprite's owner field.
	POINT3D basePoint;
	EventObject condition[2];
	bool explosionhackflag; // this originally hijacked the target field which is not safe when working with pointers.

	// transient data (not written to savegame)
	int cumulDamage;
	bool interpolated;

	DBloodActor()
	{
		index = (int(this - base()));
	}

	DBloodActor& operator=(const DBloodActor& other) = default;

	void Clear()
	{
		dudeSlope = 0;
		hit = {};
		dudeExtra = {};
		spriteMass = {};
		genDudeExtra = {};
		prevmarker = nullptr;
		basePoint = {};
		xsprite = {};
		hasx = false;
		interpolated = false;
		xvel = yvel = zvel = 0;
		explosionhackflag = false;
		interpolated = false;
	}
	bool hasX() { return hasx; }
	void addX() { hasx = true; }

	XSPRITE& x() { return xsprite; }	// calling this does not validate the xsprite!

	void SetOwner(DBloodActor* own)
	{
		ownerActor = own;
	}

	DBloodActor* GetOwner()
	{
		return ownerActor;
	}

	void SetTarget(DBloodActor* own)
	{
		x().target = own;
	}

	DBloodActor* GetTarget()
	{
		return x().target;
	}

	bool ValidateTarget(const char* func)
	{
		if (GetTarget() == nullptr)
		{
			Printf(PRINT_HIGH | PRINT_NOTIFY, "%s: invalid target in calling actor\n", func);
			return false;
		}
		return true;
	}

	void SetBurnSource(DBloodActor* own)
	{
		x().burnSource = own ? own->GetSpriteIndex() : -1;
	}

	DBloodActor* GetBurnSource()
	{
		if (x().burnSource == -1 || x().burnSource == kMaxSprites - 1) return nullptr;
		return base() + x().burnSource;
	}

	void SetSpecialOwner() // nnext hackery
	{
		ownerActor = nullptr;
		s().owner = kMaxSprites - 1;
	}

	bool GetSpecialOwner()
	{
		return  ownerActor == nullptr && (s().owner == kMaxSprites - 1);
	}

	bool IsPlayerActor()
	{
		return s().type >= kDudePlayer1 && s().type <= kDudePlayer8;
	}

	bool IsDudeActor()
	{
		return s().type >= kDudeBase && s().type < kDudeMax;
	}

	bool IsItemActor()
	{
		return s().type >= kItemBase && s().type < kItemMax;
	}

	bool IsWeaponActor()
	{
		return s().type >= kItemWeaponBase && s().type < kItemWeaponMax;
	}

	bool IsAmmoActor()
	{
		return s().type >= kItemAmmoBase && s().type < kItemAmmoMax;
	}

	bool isActive() 
	{
		if (!hasX())
			return false;

		switch (x().aiState->stateType) 
		{
		case kAiStateIdle:
		case kAiStateGenIdle:
		case kAiStateSearch:
		case kAiStateMove:
		case kAiStateOther:
			return false;
		default:
			return true;
		}
	}
};

extern DBloodActor bloodActors[kMaxSprites];

inline DBloodActor* DBloodActor::base() { return bloodActors; }

// subclassed to add a game specific actor() method

extern HitInfo gHitInfo;


// Iterator wrappers that return an actor pointer, not an index.
class BloodStatIterator : public StatIterator
{
public:
	BloodStatIterator(int stat) : StatIterator(stat)
	{
	}

	DBloodActor* Next()
	{
		int n = NextIndex();
		return n >= 0 ? &bloodActors[n] : nullptr;
	}

	DBloodActor* Peek()
	{
		int n = PeekIndex();
		return n >= 0 ? &bloodActors[n] : nullptr;
	}
};

class BloodSectIterator : public SectIterator
{
public:
	BloodSectIterator(int stat) : SectIterator(stat)
	{
	}

	BloodSectIterator(sectortype* stat) : SectIterator(stat)
	{
	}

	DBloodActor* Next()
	{
		int n = NextIndex();
		return n >= 0 ? &bloodActors[n] : nullptr;
	}

	DBloodActor* Peek()
	{
		int n = PeekIndex();
		return n >= 0 ? &bloodActors[n] : nullptr;
	}
};

// An iterator to iterate over all sprites.
class BloodSpriteIterator
{
	BloodStatIterator it;
	int stat = kStatDecoration;

public:
	BloodSpriteIterator() : it(kStatDecoration) {}

	DBloodActor* Next()
	{
		while (stat < kStatFree)
		{
			auto ac = it.Next();
			if (ac) return ac;
			stat++;
			if (stat < kStatFree) it.Reset(stat);
		}
		return nullptr;
	}
};

// For iterating linearly over map spawned sprites.
class BloodLinearSpriteIterator
{
	int index = 0;
public:

	void Reset()
	{
		index = 0;
	}

	DBloodActor* Next()
	{
		while (index < MAXSPRITES)
		{
			auto p = &bloodActors[index++];
			if (p->s().statnum != kStatFree) return p;
		}
		return nullptr;
	}
};



inline int DeleteSprite(DBloodActor* nSprite)
{
	if (nSprite) return DeleteSprite(nSprite->GetSpriteIndex());
	return 0;
}

inline void GetActorExtents(DBloodActor* actor, int* top, int* bottom)
{
	GetSpriteExtents(&actor->s(), top, bottom);
}

inline DBloodActor* getUpperLink(int sect)
{
	auto pSect = &sector[sect];
	return pSect->upperLink;
}

inline DBloodActor* getLowerLink(int sect)
{
	auto pSect = &sector[sect];
	return pSect->lowerLink;
}

inline void sfxPlay3DSound(DBloodActor* pSprite, int soundId, int a3 = -1, int a4 = 0)
{
	sfxPlay3DSound(&pSprite->s(), soundId, a3, a4);
}
inline void sfxPlay3DSoundCP(DBloodActor* pSprite, int soundId, int a3 = -1, int a4 = 0, int pitch = 0, int volume = 0)
{
	sfxPlay3DSoundCP(&pSprite->s(), soundId, a3, a4, pitch, volume);
}
inline void sfxKill3DSound(DBloodActor* pSprite, int a2 = -1, int a3 = -1)
{
	sfxKill3DSound(&pSprite->s(), a2, a3);
}

inline void ChangeActorStat(DBloodActor* actor, int stat)
{
	ChangeSpriteStat(actor->GetSpriteIndex(), stat);
}

inline void ChangeActorSect(DBloodActor* actor, int stat)
{
	ChangeSpriteSect(actor->GetSpriteIndex(), stat);
}

inline void ChangeActorSect(DBloodActor* actor, sectortype* stat)
{
	ChangeSpriteSect(actor->GetSpriteIndex(), sectnum(stat));
}

inline void setActorPos(DBloodActor* actor, vec3_t* pos)
{
	setsprite(actor->GetSpriteIndex(), pos);
}

END_BLD_NS
