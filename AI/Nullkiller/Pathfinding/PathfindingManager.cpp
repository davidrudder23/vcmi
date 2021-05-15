/*
* PathfindingManager.cpp, part of VCMI engine
*
* Authors: listed in file AUTHORS in main folder
*
* License: GNU General Public License v2.0 or later
* Full text of license available in license.txt file, in main folder
*
*/
#include "StdInc.h"
#include "PathfindingManager.h"
#include "AIPathfinder.h"
#include "AIPathfinderConfig.h"
#include "../Goals/Goals.h"
#include "../../../lib/CGameInfoCallback.h"
#include "../../../lib/mapping/CMap.h"

PathfindingManager::PathfindingManager(CPlayerSpecificInfoCallback * CB, VCAI * AI)
	: ai(AI), cb(CB)
{
}

void PathfindingManager::init(CPlayerSpecificInfoCallback * CB)
{
	cb = CB;
	pathfinder.reset(new AIPathfinder(cb, ai));
	pathfinder->init();
}

void PathfindingManager::setAI(VCAI * AI)
{
	ai = AI;
}

Goals::TGoalVec PathfindingManager::howToVisitTile(const int3 & tile, bool allowGatherArmy) const
{
	Goals::TGoalVec result;

	auto heroes = cb->getHeroesInfo();
	result.reserve(heroes.size());

	for(auto hero : heroes)
	{
		vstd::concatenate(result, howToVisitTile(hero, tile, allowGatherArmy));
	}

	return result;
}

Goals::TGoalVec PathfindingManager::howToVisitObj(ObjectIdRef obj, bool allowGatherArmy) const
{
	Goals::TGoalVec result;

	auto heroes = cb->getHeroesInfo();
	result.reserve(heroes.size());

	for(auto hero : heroes)
	{
		vstd::concatenate(result, howToVisitObj(hero, obj, allowGatherArmy));
	}

	return result;
}

Goals::TGoalVec PathfindingManager::howToVisitTile(const HeroPtr & hero, const int3 & tile, bool allowGatherArmy) const
{
	auto result = findPaths(tile, allowGatherArmy, hero, [&](int3 firstTileToGet) -> Goals::TSubgoal
	{
		return sptr(Goals::VisitTile(firstTileToGet).sethero(hero).setisAbstract(true));
	});

	for(Goals::TSubgoal solution : result)
	{
		solution->setparent(sptr(Goals::VisitTile(tile).sethero(hero).setevaluationContext(solution->evaluationContext)));
	}

	return result;
}

Goals::TGoalVec PathfindingManager::howToVisitObj(const HeroPtr & hero, ObjectIdRef obj, bool allowGatherArmy) const
{
	if(!obj)
	{
		return Goals::TGoalVec();
	}

	int3 dest = obj->visitablePos();

	auto result = findPaths(dest, allowGatherArmy, hero, [&](int3 firstTileToGet) -> Goals::TSubgoal
	{
		if(obj->ID.num == Obj::HERO && obj->getOwner() == hero->getOwner())
			return sptr(Goals::VisitHero(obj->id.getNum()).sethero(hero).setisAbstract(true));
		else
			return sptr(Goals::VisitObj(obj->id.getNum()).sethero(hero).setisAbstract(true));
	});

	for(Goals::TSubgoal solution : result)
	{
		solution->setparent(sptr(Goals::VisitObj(obj->id.getNum()).sethero(hero).setevaluationContext(solution->evaluationContext)));
	}

	return result;
}

std::vector<AIPath> PathfindingManager::getPathsToTile(const HeroPtr & hero, const int3 & tile) const
{
	auto paths = pathfinder->getPathInfo(tile);

	vstd::erase_if(paths, [&](AIPath & path) -> bool{
		return path.targetHero != hero.h;
	});

	return paths;
}

std::vector<AIPath> PathfindingManager::getPathsToTile(const int3 & tile) const
{
	return pathfinder->getPathInfo(tile);
}

Goals::TGoalVec PathfindingManager::findPaths(
	crint3 dest,
	bool allowGatherArmy,
	HeroPtr hero,
	const std::function<Goals::TSubgoal(int3)> doVisitTile) const
{
	Goals::TGoalVec result;
	boost::optional<uint64_t> armyValueRequired;
	uint64_t danger;

	std::vector<AIPath> chainInfo = pathfinder->getPathInfo(dest);

#ifdef VCMI_TRACE_PATHFINDER
	logAi->trace("Trying to find a way for %s to visit tile %s", hero->name, dest.toString());
#endif

	for(auto path : chainInfo)
	{
		if((hero && hero.get() != path.targetHero) || path.nodes.empty())
			continue;

		const AIPathNodeInfo & firstNode = path.firstNode();
		int3 firstTileToGet = firstNode.coord;

#ifdef VCMI_TRACE_PATHFINDER
		std::stringstream str;

		str << "Path found ";

		for(auto node : path.nodes)
			str << node.targetHero->name << "->" << node.coord.toString() << "; ";

		logAi->trace(str.str());
#endif
		if(ai->isTileNotReserved(hero.get(), firstTileToGet))
		{
			danger = path.getTotalDanger(hero);

			if(isSafeToVisit(hero, path.heroArmy, danger))
			{
				Goals::TSubgoal solution;

				if(path.specialAction)
				{
					solution = path.specialAction->whatToDo(hero);
				}
				else
				{
					solution = dest == firstTileToGet
						? doVisitTile(firstTileToGet)
						: clearWayTo(firstNode.targetHero, firstTileToGet);
				}

				if(solution->invalid())
					continue;

				if(solution->evaluationContext.danger < danger)
					solution->evaluationContext.danger = danger;

				solution->evaluationContext.movementCost += path.movementCost();
				solution->evaluationContext.armyLoss += path.armyLoss;
				solution->evaluationContext.heroStrength = path.getHeroStrength();
#ifdef VCMI_TRACE_PATHFINDER
				logAi->trace("It's safe for %s to visit tile %s with danger %s, loss %s, army strength %s, goal %s", 
					hero->name, 
					dest.toString(), 
					std::to_string(danger),
					std::to_string(path.armyLoss),
					std::to_string(path.heroArmy->getArmyStrength()),
					solution->name());
#endif
				result.push_back(solution);

				continue;
			}

			if(!armyValueRequired || armyValueRequired > danger)
			{
				armyValueRequired = boost::make_optional(danger);
			}
		}
	}

	danger = armyValueRequired.get_value_or(0);

	if(allowGatherArmy && danger > 0)
	{
		//we need to get army in order to conquer that place
#ifdef VCMI_TRACE_PATHFINDER
		logAi->trace("Gather army for %s, value=%s", hero->name, std::to_string(danger));
#endif
		result.push_back(sptr(Goals::GatherArmy(danger * SAFE_ATTACK_CONSTANT).sethero(hero).setisAbstract(true)));
	}

	return result;
}

Goals::TSubgoal PathfindingManager::clearWayTo(HeroPtr hero, int3 firstTileToGet) const
{
	if(isBlockedBorderGate(firstTileToGet))
	{
		//FIXME: this way we'll not visit gate and activate quest :?
		return sptr(Goals::FindObj(Obj::KEYMASTER, cb->getTile(firstTileToGet)->visitableObjects.back()->subID));
	}

	auto topObj = cb->getTopObj(firstTileToGet);
	if(topObj)
	{

		if(vstd::contains(ai->reservedObjs, topObj) && !vstd::contains(ai->reservedHeroesMap[hero], topObj))
		{
			return sptr(Goals::Invalid());
		}

		if(topObj->ID == Obj::HERO && cb->getPlayerRelations(hero->tempOwner, topObj->tempOwner) != PlayerRelations::ENEMIES)
		{
			if(topObj != hero.get(true)) //the hero we want to free
			{
				logAi->warn("%s stands in the way of %s", topObj->getObjectName(), hero->getObjectName());
			}
		}

		if(topObj->ID == Obj::QUEST_GUARD || topObj->ID == Obj::BORDERGUARD)
		{
			if(shouldVisit(hero, topObj))
			{
				//do NOT use VISIT_TILE, as tile with quets guard can't be visited
				return sptr(Goals::VisitObj(topObj->id.getNum()).sethero(hero));
			}

			auto questObj = dynamic_cast<const IQuestObject*>(topObj);

			if(questObj)
			{
				auto questInfo = QuestInfo(questObj->quest, topObj, topObj->visitablePos());

				return sptr(Goals::CompleteQuest(questInfo));
			}

			return sptr(Goals::Invalid());
		}
	}

	return sptr(Goals::VisitTile(firstTileToGet).sethero(hero).setisAbstract(true));
}

void PathfindingManager::updatePaths(std::vector<HeroPtr> heroes, bool useHeroChain)
{
	logAi->debug("AIPathfinder has been reseted.");
	pathfinder->updatePaths(heroes, useHeroChain);
}
