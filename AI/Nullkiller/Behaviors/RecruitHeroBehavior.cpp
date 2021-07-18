/*
* RecruitHeroBehavior.cpp, part of VCMI engine
*
* Authors: listed in file AUTHORS in main folder
*
* License: GNU General Public License v2.0 or later
* Full text of license available in license.txt file, in main folder
*
*/
#include "StdInc.h"
#include "RecruitHeroBehavior.h"
#include "../VCAI.h"
#include "../AIhelper.h"
#include "../AIUtility.h"
#include "../Goals/RecruitHero.h"
#include "../Goals/ExecuteHeroChain.h"
#include "lib/mapping/CMap.h" //for victory conditions
#include "lib/CPathfinder.h"

extern boost::thread_specific_ptr<CCallback> cb;
extern boost::thread_specific_ptr<VCAI> ai;
extern FuzzyHelper * fh;

using namespace Goals;

std::string RecruitHeroBehavior::toString() const
{
	return "Recruit hero";
}

Goals::TGoalVec RecruitHeroBehavior::getTasks()
{
	Goals::TGoalVec tasks;
	auto towns = cb->getTownsInfo();

	for(auto town : towns)
	{
		if(!town->garrisonHero && ai->canRecruitAnyHero(town))
		{
			if(cb->getHeroesInfo().size() < cb->getTownsInfo().size() + 1
				|| cb->getResourceAmount(Res::GOLD) > 10000)
			{
				tasks.push_back(Goals::sptr(Goals::RecruitHero().settown(town)));
			}
		}
	}

	return tasks;
}
