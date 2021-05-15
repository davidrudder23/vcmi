/*
* ExecuteHeroChain.cpp, part of VCMI engine
*
* Authors: listed in file AUTHORS in main folder
*
* License: GNU General Public License v2.0 or later
* Full text of license available in license.txt file, in main folder
*
*/
#include "StdInc.h"
#include "ExecuteHeroChain.h"
#include "VisitTile.h"
#include "../VCAI.h"
#include "../FuzzyHelper.h"
#include "../AIhelper.h"
#include "../../../lib/mapping/CMap.h" //for victory conditions
#include "../../../lib/CPathfinder.h"
#include "../Engine/Nullkiller.h"

extern boost::thread_specific_ptr<CCallback> cb;
extern boost::thread_specific_ptr<VCAI> ai;
extern FuzzyHelper * fh;

using namespace Goals;

ExecuteHeroChain::ExecuteHeroChain(const AIPath & path, const CGObjectInstance * obj)
	:CGoal(Goals::EXECUTE_HERO_CHAIN), chainPath(path)
{
	evaluationContext.danger = path.getTotalDanger(hero);
	evaluationContext.movementCost = path.movementCost();
	evaluationContext.armyLoss = path.armyLoss;
	evaluationContext.heroStrength = path.getHeroStrength();
	hero = path.targetHero;
	tile = path.targetTile();

	if(obj)
	{
		objid = obj->id.getNum();
		targetName = obj->getObjectName() + tile.toString();
	}
	else
	{
		targetName = "tile" + tile.toString();
	}
}

bool ExecuteHeroChain::operator==(const ExecuteHeroChain & other) const
{
	return false;
}

TSubgoal ExecuteHeroChain::whatToDoToAchieve()
{
	return iAmElementar();
}

void ExecuteHeroChain::accept(VCAI * ai)
{
	logAi->debug("Executing hero chain towards %s. Path %s", targetName, chainPath.toString());

	std::set<int> blockedIndexes;

	for(int i = chainPath.nodes.size() - 1; i >= 0; i--)
	{
		auto & node = chainPath.nodes[i];

		HeroPtr hero = node.targetHero;

		if(vstd::contains(blockedIndexes, i))
		{
			blockedIndexes.insert(node.parentIndex);
			ai->nullkiller->lockHero(hero);

			continue;
		}

		logAi->debug("Executing chain node %" PRId32 ". Moving hero %s to %s", i, hero.name, node.coord.toString());

		try
		{
			if(hero->movement)
			{
				ai->nullkiller->setActive(hero);

				Goals::VisitTile(node.coord).sethero(hero).accept(ai);
			}

			// no exception means we were not able to rich the tile
			ai->nullkiller->lockHero(hero);
			blockedIndexes.insert(node.parentIndex);
		}
		catch(goalFulfilledException)
		{
			if(!hero)
			{
				logAi->debug("Hero %s was killed while attempting to rich %s", hero.name, node.coord.toString());

				return;
			}
		}
	}
}

std::string ExecuteHeroChain::name() const
{
	return "ExecuteHeroChain " + targetName;
}

std::string ExecuteHeroChain::completeMessage() const
{
	return "Hero chain completed";
}

ExchangeSwapTownHeroes::ExchangeSwapTownHeroes(const CGTownInstance * town, const CGHeroInstance * garrisonHero)
	:CGoal(Goals::EXCHANGE_SWAP_TOWN_HEROES), town(town), garrisonHero(garrisonHero)
{
}

std::string ExchangeSwapTownHeroes::name() const
{
	return "Exchange and swap heroes of " + town->name;
}

std::string ExchangeSwapTownHeroes::completeMessage() const
{
	return "Exchange and swap heroes of " + town->name + " compelete";
}

bool ExchangeSwapTownHeroes::operator==(const ExchangeSwapTownHeroes & other) const
{
	return town == other.town;
}

TSubgoal ExchangeSwapTownHeroes::whatToDoToAchieve()
{
	return iAmElementar();
}

void ExchangeSwapTownHeroes::accept(VCAI * ai)
{
	if(!garrisonHero && town->garrisonHero && town->visitingHero)
		throw cannotFulfillGoalException("Invalid configuration. Garrison hero is null.");

	if(!garrisonHero)
	{
		if(!town->garrisonHero)
			throw cannotFulfillGoalException("Invalid configuration. There is no hero in town garrison.");
		
		cb->swapGarrisonHero(town);
		ai->nullkiller->unlockHero(town->visitingHero.get());
		logAi->debug("Extracted hero %s from garrison of %s", town->visitingHero->name, town->name);

		return;
	}

	if(town->visitingHero && town->visitingHero.get() != garrisonHero)
		cb->swapGarrisonHero(town);

	ai->moveHeroToTile(town->visitablePos(), garrisonHero);

	cb->swapGarrisonHero(town); // selected hero left in garrison with strongest army
	ai->nullkiller->lockHero(town->garrisonHero.get());

	if(town->visitingHero)
		ai->nullkiller->unlockHero(town->visitingHero.get());

	logAi->debug("Put hero %s to garrison of %s", town->garrisonHero->name, town->name);
}