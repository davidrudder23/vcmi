/*
* AINodeStorage.h, part of VCMI engine
*
* Authors: listed in file AUTHORS in main folder
*
* License: GNU General Public License v2.0 or later
* Full text of license available in license.txt file, in main folder
*
*/

#pragma once

#undef VCMI_TRACE_PATHFINDER

#include "../../../lib/CPathfinder.h"
#include "../../../lib/mapObjects/CGHeroInstance.h"
#include "../AIUtility.h"
#include "../FuzzyHelper.h"
#include "../Goals/AbstractGoal.h"
#include "Actions/ISpecialAction.h"
#include "Actors.h"

struct AIPathNode : public CGPathNode
{
	uint64_t danger;
	uint64_t armyLoss;
	uint32_t manaCost;
	const AIPathNode * chainOther;
	std::shared_ptr<const ISpecialAction> specialAction;
	const ChainActor * actor;
};

struct AIPathNodeInfo
{
	float cost;
	uint8_t turns;
	int3 coord;
	uint64_t danger;
	const CGHeroInstance * targetHero;
	int parentIndex;
	std::shared_ptr<const ISpecialAction> specialAction;
};

struct AIPath
{
	std::vector<AIPathNodeInfo> nodes;
	std::shared_ptr<const ISpecialAction> specialAction;
	uint64_t targetObjectDanger;
	uint64_t armyLoss;
	const CGHeroInstance * targetHero;
	const CCreatureSet * heroArmy;
	uint64_t chainMask;

	AIPath();

	/// Gets danger of path excluding danger of visiting the target object like creature bank
	uint64_t getPathDanger() const;

	/// Gets danger of path including danger of visiting the target object like creature bank
	uint64_t getTotalDanger(HeroPtr hero) const;

	int3 firstTileToGet() const;
	int3 targetTile() const;

	const AIPathNodeInfo & firstNode() const;

	float movementCost() const;

	uint8_t turn() const;

	uint64_t getHeroStrength() const;

	std::string toString();
};

struct ExchangeCandidate : public AIPathNode
{
	AIPathNode * carrierParent;
	AIPathNode * otherParent;
};

class AINodeStorage : public INodeStorage
{
private:
	int3 sizes;

	/// 1-3 - position on map, 4 - layer (air, water, land), 5 - chain (normal, battle, spellcast and combinations)
	boost::multi_array<AIPathNode, 5> nodes;
	const CPlayerSpecificInfoCallback * cb;
	const VCAI * ai;
	std::unique_ptr<FuzzyHelper> dangerEvaluator;
	std::vector<std::shared_ptr<ChainActor>> actors;
	std::vector<CGPathNode *> heroChain;
	bool heroChainPass; // true if we need to calculate hero chain
	int heroChainTurn;
	PlayerColor playerID;

public:
	/// more than 1 chain layer for each hero allows us to have more than 1 path to each tile so we can chose more optimal one.
	static const int NUM_CHAINS = 5 * GameConstants::MAX_HEROES_PER_PLAYER;
	
	AINodeStorage(const int3 & sizes);
	~AINodeStorage();

	void initialize(const PathfinderOptions & options, const CGameState * gs) override;

	virtual std::vector<CGPathNode *> getInitialNodes() override;

	virtual std::vector<CGPathNode *> calculateNeighbours(
		const PathNodeInfo & source,
		const PathfinderConfig * pathfinderConfig,
		const CPathfinderHelper * pathfinderHelper) override;

	virtual std::vector<CGPathNode *> calculateTeleportations(
		const PathNodeInfo & source,
		const PathfinderConfig * pathfinderConfig,
		const CPathfinderHelper * pathfinderHelper) override;

	virtual void commit(CDestinationNodeInfo & destination, const PathNodeInfo & source) override;

	const AIPathNode * getAINode(const CGPathNode * node) const;
	void updateAINode(CGPathNode * node, std::function<void (AIPathNode *)> updater);

	bool hasBetterChain(const PathNodeInfo & source, CDestinationNodeInfo & destination) const;

	template<class NodeRange>
	bool hasBetterChain(
		const CGPathNode * source, 
		const AIPathNode * destinationNode,
		const NodeRange & chains) const;

	boost::optional<AIPathNode *> getOrCreateNode(const int3 & coord, const EPathfindingLayer layer, const ChainActor * actor);
	std::vector<AIPath> getChainInfo(const int3 & pos, bool isOnLand) const;
	bool isTileAccessible(const HeroPtr & hero, const int3 & pos, const EPathfindingLayer layer) const;
	void setHeroes(std::vector<HeroPtr> heroes, const VCAI * ai);
	void setTownsAndDwellings(
		const std::vector<const CGTownInstance *> & towns,
		const std::set<const CGObjectInstance *> & visitableObjs);
	const CGHeroInstance * getHero(const CGPathNode * node) const;
	const std::set<const CGHeroInstance *> getAllHeroes() const;
	void clear();
	bool calculateHeroChain();

	uint64_t evaluateDanger(const int3 &  tile, const CGHeroInstance * hero) const
	{
		return dangerEvaluator->evaluateDanger(tile, hero, ai);
	}

private:
	STRONG_INLINE
	void resetTile(const int3 & tile, EPathfindingLayer layer, CGPathNode::EAccessibility accessibility);

	void calculateHeroChain(
		AIPathNode * srcNode, 
		const std::vector<AIPathNode *> & variants, 
		std::vector<ExchangeCandidate> & result) const;

	void calculateHeroChain(
		AIPathNode * carrier, 
		AIPathNode * other, 
		std::vector<ExchangeCandidate> & result) const;
	
	void cleanupInefectiveChains(std::vector<ExchangeCandidate> & result) const;
	void addHeroChain(const std::vector<ExchangeCandidate> & result);

	void calculateTownPortalTeleportations(
		const PathNodeInfo & source,
		std::vector<CGPathNode *> & neighbours,
		const CPathfinderHelper * pathfinderHelper);
	void fillChainInfo(const AIPathNode * node, AIPath & path, int parentIndex) const;
	void commit(
		AIPathNode * destination, 
		const AIPathNode * source, 
		CGPathNode::ENodeAction action, 
		int turn, 
		int movementLeft, 
		float cost) const;

	ExchangeCandidate calculateExchange(
		ChainActor * exchangeActor, 
		AIPathNode * carrierParentNode, 
		AIPathNode * otherParentNode) const;
};
