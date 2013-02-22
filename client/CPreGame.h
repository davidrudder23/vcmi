#pragma once

#include "../lib/Filesystem/CResourceLoader.h"
#include <SDL.h>
#include "../lib/StartInfo.h"
#include "GUIClasses.h"
#include "FunctionList.h"
#include "../lib/Mapping/CMapInfo.h"
#include "../lib/RMG/CMapGenOptions.h"

/*
 * CPreGame.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

class CMusicHandler;
class CMapHeader;
class CCampaignHeader;
class CTextInput;
class CCampaign;
class CGStatusBar;
class CTextBox;
class CCampaignState;
class CConnection;
class JsonNode;
class CMapGenOptions;
class RandomMapTab;
struct CPackForSelectionScreen;
struct PlayerInfo;

namespace boost{ class thread; class recursive_mutex;}

enum ESortBy{_playerAm, _size, _format, _name, _viccon, _loscon, _numOfMaps}; //_numOfMaps is for campaigns

/// Class which handles map sorting by different criteria
class mapSorter
{
public:
	ESortBy sortBy;
	bool operator()(const CMapInfo *aaa, const CMapInfo *bbb);
	mapSorter(ESortBy es):sortBy(es){};
};

/// The main menu screens listed in the EState enum
class CMenuScreen : public CIntObject
{
	const JsonNode& config;

	CTabbedInt *tabs;

	CPicture * background;
	std::vector<CPicture*> images;

	CIntObject *createTab(size_t index);
public:
	std::vector<std::string> menuNameToEntry;

	enum EState { //where are we?
		mainMenu, newGame, loadGame, campaignMain, saveGame, scenarioInfo, campaignList
	};

	enum EMultiMode {
		SINGLE_PLAYER = 0, MULTI_HOT_SEAT, MULTI_NETWORK_HOST, MULTI_NETWORK_GUEST
	};
	CMenuScreen(const JsonNode& configNode);

	void showAll(SDL_Surface * to);
	void show(SDL_Surface * to);
	void activate();
	void deactivate();

	void switchToTab(size_t index);
};

class CMenuEntry : public CIntObject
{
	std::vector<CPicture*> images;
	std::vector<CAdventureMapButton*> buttons;

	CAdventureMapButton* createButton(CMenuScreen* parent, const JsonNode& button);
public:
	CMenuEntry(CMenuScreen* parent, const JsonNode &config);
};

class CreditsScreen : public CIntObject
{
	CTextBox* credits;
public:
	CreditsScreen();

	void show(SDL_Surface * to);
	void showAll(SDL_Surface * to);

	void clickLeft(tribool down, bool previousState);
	void clickRight(tribool down, bool previousState);
};

/// Implementation of the chat box
class CChatBox : public CIntObject
{
public:
	CTextBox *chatHistory;
	CTextInput *inputBox;

	CChatBox(const Rect &rect);

	void keyPressed(const SDL_KeyboardEvent & key);

	void addNewMessage(const std::string &text);
};

class InfoCard : public CIntObject
{
public:
	CPicture *bg;
	CMenuScreen::EState type;

	bool network;
	bool chatOn;  //if chat is shown, then description is hidden
	CTextBox *mapDescription;
	CChatBox *chat;
	CPicture *playerListBg;

	CHighlightableButtonsGroup *difficulty;
	CDefHandler *sizes, *sFlags;

	void changeSelection(const CMapInfo *to);
	void showAll(SDL_Surface * to);
	void clickRight(tribool down, bool previousState);
	void showTeamsPopup();
	void toggleChat();
	void setChat(bool activateChat);
	InfoCard(bool Network = false);
	~InfoCard();
};

/// The selection tab which is shown at the map selection screen
class SelectionTab : public CIntObject
{
private:
	CDefHandler *format; //map size

    void parseMaps(const std::vector<ResourceID> &files);
	void parseGames(const std::vector<ResourceID> &files, bool multi);
	void parseCampaigns(const std::vector<ResourceID> & files );
	std::vector<ResourceID> getFiles(std::string dirURI, int resType);
	CMenuScreen::EState tabType;
public:
	int positions; //how many entries (games/maps) can be shown
	CPicture *bg; //general bg image
	CSlider *slider;
	std::vector<CMapInfo> allItems;
	std::vector<CMapInfo*> curItems;
	size_t selectionPos;
	boost::function<void(CMapInfo *)> onSelect;

	ESortBy sortingBy;
	bool ascending;

	CTextInput *txt;


	void filter(int size, bool selectFirst = false); //0 - all
	void select(int position); //position: <0 - positions>  position on the screen
	void selectAbs(int position); //position: absolute position in curItems vector
	int getPosition(int x, int y); //convert mouse coords to entry position; -1 means none
	void sliderMove(int slidPos);
	void sortBy(int criteria);
	void sort();
	void printMaps(SDL_Surface *to);
	int getLine();
	void selectFName(std::string fname);
	const CMapInfo * getSelectedMapInfo() const;

	void showAll(SDL_Surface * to);
	void clickLeft(tribool down, bool previousState);
	void keyPressed(const SDL_KeyboardEvent & key);
	void onDoubleClick();
	SelectionTab(CMenuScreen::EState Type, const boost::function<void(CMapInfo *)> &OnSelect, CMenuScreen::EMultiMode MultiPlayer = CMenuScreen::SINGLE_PLAYER);
    ~SelectionTab();
};

/// The options tab which is shown at the map selection phase.
class OptionsTab : public CIntObject
{
	CPicture *bg;
public:
	enum SelType {TOWN, HERO, BONUS};

	struct CPlayerSettingsHelper
	{
		const PlayerSettings & settings;
		const SelType type;

		CPlayerSettingsHelper(const PlayerSettings & settings, SelType type):
		    settings(settings),
		    type(type)
		{}

		/// visible image settings
		size_t getImageIndex();
		std::string getImageName();

		std::string getName();       /// name visible in options dialog
		std::string getTitle();      /// title in popup box
		std::string getSubtitle();   /// popup box subtitle
		std::string getDescription();/// popup box description, not always present
	};

	class CPregameTooltipBox : public CWindowObject, public CPlayerSettingsHelper
	{
		void genHeader();
		void genTownWindow();
		void genHeroWindow();
		void genBonusWindow();
	public:
		CPregameTooltipBox(CPlayerSettingsHelper & helper);
	};

	struct SelectedBox : public CIntObject, public CPlayerSettingsHelper //img with current town/hero/bonus
	{
		CAnimImage * image;
		CLabel *subtitle;

		SelectedBox(Point position, PlayerSettings & settings, SelType type);
		void clickRight(tribool down, bool previousState);

		void update();
	};

	struct PlayerOptionsEntry : public CIntObject
	{
		PlayerInfo &pi;
		PlayerSettings &s;
		CPicture *bg;
		CAdventureMapButton *btns[6]; //left and right for town, hero, bonus
		CAdventureMapButton *flag;
		SelectedBox *town;
		SelectedBox *hero;
		SelectedBox *bonus;
		enum {HUMAN_OR_CPU, HUMAN, CPU} whoCanPlay;

		PlayerOptionsEntry(OptionsTab *owner, PlayerSettings &S);
		void selectButtons(); //hides unavailable buttons
		void showAll(SDL_Surface * to);
		void update();
	};

	CSlider *turnDuration;

	std::set<int> usedHeroes;

	struct PlayerToRestore
	{
		int color, id;
		void reset() { color = id = -1; }
		PlayerToRestore(){ reset(); }
	} playerToRestore;


	std::map<int, PlayerOptionsEntry *> entries; //indexed by color

	void nextCastle(int player, int dir); //dir == -1 or +1
	void nextHero(int player, int dir); //dir == -1 or +1
	void nextBonus(int player, int dir); //dir == -1 or +1
	void setTurnLength(int npos);
	void flagPressed(int player);

	void recreate();
	OptionsTab();
	~OptionsTab();
	void showAll(SDL_Surface * to);

	int nextAllowedHero(int player, int min, int max, int incl, int dir );

	bool canUseThisHero(int player, int ID );
};

/**
 * The random map tab shows options for generating a random map.
 */
class RandomMapTab : public CIntObject
{
public:
    /**
     * C-tor.
     */
    RandomMapTab();

    /**
     * Shows the interface and the visual representation of this tab.
     *
     * @param to where the graphics should be inserted
     */
    void showAll(SDL_Surface * to);

	/**
	 * Updates the map info object and fires the associated callback method.
	 */
	void updateMapInfo();

	/**
	 * Gets the map info changed callback method list object. This event
	 * occurs when the updateMapInfo method has been called or the options
	 * of this tab have been changed.
	 *
	 * @return the map info changed callback method list object
	 */
	CFunctionList<void (const CMapInfo *)> & getMapInfoChanged();

	/**
	 * Gets the created map info object.
	 *
	 * @return the created map info object
	 */
	const CMapInfo & getMapInfo() const;

	/**
	 * Gets the map generation options.
	 *
	 * @return the map generation options
	 */
	const CMapGenOptions & getMapGenOptions() const;

private:
    /**
     * Adds buttons specified by the defs list to the given buttons group.
     *
     * @param group the button group where the buttons should be added to
     * @param defs the names of the button defs
     * @param startIndex start index of the defs vector
     * @param endIndex end index of the defs vector
     * @param btnWidth width of one button(fixed width)
     * @param helpStartIndex the index of the first help msg entry
     */
    void addButtonsToGroup(CHighlightableButtonsGroup * group, const std::vector<std::string> & defs, int startIndex, int endIndex, int btnWidth, int helpStartIndex) const;

    /**
     * Adds buttons specified by the defs list and the random button to the given buttons group. Auto-selects the
     * random button.
     *
     * @param group the button group where the buttons should be added to
     * @param defs the names of the button defs
     * @param startIndex start index of the defs vector
     * @param endIndex end index of the defs vector
     * @param btnWidth width of one button(fixed width)
     * @param helpStartIndex the index of the first help msg entry
     * @param helpRandIndex the index of the random help msg entry
     */
    void addButtonsWithRandToGroup(CHighlightableButtonsGroup * group, const std::vector<std::string> & defs, int startIndex, int endIndex, int btnWidth, int helpStartIndex, int helpRandIndex) const;

    /**
     * Deactives buttons of a highlightable button group beginning from startId. Buttons with a id
     * lower than startId will be activated/reseted.
     *
     * @param group the associated highlightable button group
     * @param startId the id of the first button to deactivate
     */
    void deactivateButtonsFrom(CHighlightableButtonsGroup * group, int startId);

    /**
     * Validates players count and updates teams count, comp only players/teams count if necessary.
     *
     * @param playersCnt the players count to validate
     */
    void validatePlayersCnt(int playersCnt);

    /**
     * Validates computer only players count and updates comp only teams count if necessary.
     *
     * @param compOnlyPlayersCnt the computer only players count to validate
     */
    void validateCompOnlyPlayersCnt(int compOnlyPlayersCnt);

    /** the background image of the rmg options tab */
    CPicture * bg;

    /** the map size buttons group */
    CHighlightableButtonsGroup * mapSizeBtnGroup;

    /** the two levels highlightable button */
    CHighlightableButton * twoLevelsBtn;

    /** the players count group */
    CHighlightableButtonsGroup * playersCntGroup;

    /** the teams count group */
    CHighlightableButtonsGroup * teamsCntGroup;

    /** the computer only players count group */
    CHighlightableButtonsGroup * compOnlyPlayersCntGroup;

    /** the computer only teams count group */
    CHighlightableButtonsGroup * compOnlyTeamsCntGroup;

    /** the water content group */
    CHighlightableButtonsGroup * waterContentGroup;

    /** the monster strength group */
    CHighlightableButtonsGroup * monsterStrengthGroup;

	/** show previously created random maps button */
    CAdventureMapButton * showRandMaps;

    /** the map options selected by the user */
	CMapGenOptions mapGenOptions;

	/** map info object describing a randomly created map */
	CMapInfo mapInfo;

	/** callback method which gets called when the random options have been changed */
	CFunctionList<void(const CMapInfo *)> mapInfoChanged;
};

/// Interface for selecting a map.
class ISelectionScreenInfo
{
public:
	CMenuScreen::EMultiMode multiPlayer;
	CMenuScreen::EState screenType; //new/save/load#Game
	const CMapInfo *current;
	StartInfo sInfo;
	std::map<TPlayerColor, std::string> playerNames; // id of player <-> player name; 0 is reserved as ID of AI "players"

	ISelectionScreenInfo(const std::map<TPlayerColor, std::string> *Names = NULL);
	virtual ~ISelectionScreenInfo();
	virtual void update(){};
	virtual void propagateOptions() {};
	virtual void postRequest(ui8 what, ui8 dir) {};
	virtual void postChatMessage(const std::string &txt){};

	void setPlayer(PlayerSettings &pset, TPlayerColor player);
	void updateStartInfo( std::string filename, StartInfo & sInfo, const CMapHeader * mapHeader );

	int getIdOfFirstUnallocatedPlayer(); //returns 0 if none
	bool isGuest() const;
	bool isHost() const;

};

/// The actual map selection screen which consists of the options and selection tab
class CSelectionScreen : public CIntObject, public ISelectionScreenInfo
{
	bool bordered;
public:
	CPicture *bg; //general bg image
	InfoCard *card;
	OptionsTab *opt;
    RandomMapTab * randMapTab;
	CAdventureMapButton *start, *back;

	SelectionTab *sel;
	CIntObject *curTab;

	boost::thread *serverHandlingThread;
	boost::recursive_mutex *mx;
	std::list<CPackForSelectionScreen *> upcomingPacks; //protected by mx

	CConnection *serv; //connection to server, used in MP mode
	bool ongoingClosing;
	ui8 myNameID; //used when networking - otherwise all player are "mine"

	CSelectionScreen(CMenuScreen::EState Type, CMenuScreen::EMultiMode MultiPlayer = CMenuScreen::SINGLE_PLAYER, const std::map<TPlayerColor, std::string> *Names = NULL);
	~CSelectionScreen();
	void toggleTab(CIntObject *tab);
	void changeSelection(const CMapInfo *to);
	void startCampaign();
	void startScenario();
	void difficultyChange(int to);

	void handleConnection();

	void processPacks();
	void setSInfo(const StartInfo &si);
	void update() override;
	void propagateOptions() override;
	void postRequest(ui8 what, ui8 dir) override;
	void postChatMessage(const std::string &txt) override;
	void propagateNames();
	void showAll(SDL_Surface *to);
};

/// Save game screen
class CSavingScreen : public CSelectionScreen
{
public:
	const CMapInfo *ourGame;


	CSavingScreen(bool hotseat = false);
	~CSavingScreen();
};

/// Scenario information screen shown during the game (thus not really a "pre-game" but fits here anyway)
class CScenarioInfo : public CIntObject, public ISelectionScreenInfo
{
public:
	CAdventureMapButton *back;
	InfoCard *card;
	OptionsTab *opt;

	CScenarioInfo(const CMapHeader *mapHeader, const StartInfo *startInfo);
	~CScenarioInfo();
};

/// Multiplayer mode
class CMultiMode : public CIntObject
{
public:
	CPicture *bg;
	CTextInput *txt;
	CAdventureMapButton *btns[7]; //0 - hotseat, 6 - cancel
	CGStatusBar *bar;

	CMultiMode();
	void openHotseat();
	void hostTCP();
	void joinTCP();
};

/// Hot seat player window
class CHotSeatPlayers : public CIntObject
{
	CPicture *bg;
	CTextBox *title;
	CTextInput* txt[8];
	CAdventureMapButton *ok, *cancel;
	CGStatusBar *bar;

	void onChange(std::string newText);
	void enterSelectionScreen();

public:
	CHotSeatPlayers(const std::string &firstPlayer);
};


class CPrologEpilogVideo : public CIntObject
{
	CCampaignScenario::SScenarioPrologEpilog spe;
	SDL_Surface * txt;
	int curTxtH, decrementDelayCounter;
	std::function<void()> exitCb;
public:
	CPrologEpilogVideo(CCampaignScenario::SScenarioPrologEpilog _spe, std::function<void()> callback);

	void clickLeft(tribool down, bool previousState);
	void show(SDL_Surface * to);
};

/// Campaign screen where you can choose one out of three starting bonuses
class CBonusSelection : public CIntObject
{
	SDL_Surface * background;
	CAdventureMapButton * startB, * backB;

	//campaign & map descriptions:
	CTextBox * cmpgDesc, * mapDesc;

	struct SCampPositions
	{
		std::string campPrefix;
		int colorSuffixLength;

		struct SRegionDesc
		{
			std::string infix;
			int xpos, ypos;
		};

		std::vector<SRegionDesc> regions;

	};

	std::vector<SCampPositions> campDescriptions;

	class CRegion : public CIntObject
	{
		CBonusSelection * owner;
		SDL_Surface* graphics[3]; //[0] - not selected, [1] - selected, [2] - striped
		bool accessible; //false if region should be striped
		bool selectable; //true if region should be selectable
		int myNumber; //number of region
	public:
		std::string rclickText;
		CRegion(CBonusSelection * _owner, bool _accessible, bool _selectable, int _myNumber);
		~CRegion();

		void clickLeft(tribool down, bool previousState);
		void clickRight(tribool down, bool previousState);
		void show(SDL_Surface * to);
	};

	std::vector<CRegion *> regions;
	CRegion * highlightedRegion;

	void loadPositionsOfGraphics();
	shared_ptr<CCampaignState> ourCampaign;
	CMapHeader *ourHeader;
	CDefHandler *sizes; //icons of map sizes
	SDL_Surface* diffPics[5]; //pictures of difficulties, user-selectable (or not if campaign locks this)
	CAdventureMapButton * diffLb, * diffRb; //buttons for changing difficulty
	void changeDiff(bool increase); //if false, then decrease


	void updateStartButtonState(int selected = -1); //-1 -- no bonus is selected
	//bonus selection
	void updateBonusSelection();
	CHighlightableButtonsGroup * bonuses;

public:
	StartInfo sInfo;
	CDefHandler *sFlags;

	void selectMap(int whichOne, bool initialSelect);
	void selectBonus(int id);
	void init();

	CBonusSelection(std::string campaignFName);
	CBonusSelection(shared_ptr<CCampaignState> _ourCampaign);
	~CBonusSelection();

	void showAll(SDL_Surface * to);
	void show(SDL_Surface * to);

	void goBack();
	void startMap();
};

/// Campaign selection screen
class CCampaignScreen : public CIntObject
{
public:
	enum CampaignStatus {DEFAULT = 0, ENABLED, DISABLED, COMPLETED}; // the status of the campaign

private:
	/// A button which plays a video when you move the mouse cursor over it
	class CCampaignButton : public CIntObject
	{
	private:
		CPicture *image;
		CPicture *checkMark;

		CLabel *hoverLabel;
		CampaignStatus status;

		std::string campFile; // the filename/resourcename of the campaign
		std::string video; // the resource name of the video
		std::string hoverText;

		void clickLeft(tribool down, bool previousState);
		void hover(bool on);

	public:
		CCampaignButton(const JsonNode &config );
		void show(SDL_Surface * to);
	};

	CAdventureMapButton *back;
	std::vector<CCampaignButton*> campButtons;
	std::vector<CPicture*> images;

	CAdventureMapButton* createExitButton(const JsonNode& button);

public:
	enum CampaignSet {ROE, AB, SOD, WOG};

	CCampaignScreen(const JsonNode &config);
	void showAll(SDL_Surface *to);
};

/// Handles background screen, loads graphics for victory/loss condition and random town or hero selection
class CGPreGame : public CIntObject, public IUpdateable
{
	void loadGraphics();
	void disposeGraphics();

	CGPreGame(); //Use createIfNotPresent

public:
	const JsonNode * const pregameConfig;

	CMenuScreen* menu;

	CDefHandler *victory, *loss;

	~CGPreGame();
	void update();
	void openSel(CMenuScreen::EState type, CMenuScreen::EMultiMode multi = CMenuScreen::SINGLE_PLAYER);

	void openCampaignScreen(std::string name);

	static CGPreGame * create();
	void removeFromGui();
	static void showLoadingScreen(boost::function<void()> loader);
};

class CLoadingScreen : public CWindowObject
{
	boost::thread loadingThread;

	std::string getBackground();
public:
	CLoadingScreen(boost::function<void()> loader);
	~CLoadingScreen();

	void showAll(SDL_Surface *to);
};

extern ISelectionScreenInfo *SEL;
extern CGPreGame *CGP;
