#include <cstring>
#include <string>
#include <memory>
#include <vector>
#include <algorithm>
#include <regex>
#include <functional>
#include <sw/redis++/redis++.h>
#include <panel.h>
#include <iostream>
#include <unordered_set>
#include <utility>
#include <chrono>
#include <thread>
#include "seqentity.hxx"
#include <atomic>
#include <cstdlib>
#include <fstream>

using namespace std;
using namespace sw::redis;

/* Globals */
char *titleChat = "Chat Room";
char *titleCmd = "unknown";
static vector<PANEL*> _layout_; 
const string _users_{"users"};
const string _rooms_{"rooms"};
const string _sessions_{"sessions"};
const string noroom = "noroom";
string current_room = noroom;
const string unknown = "unknown";
static string username = unknown;

const std::string currentDateTime() {
    time_t     now = time(0);
    struct tm  tstruct;
    char       buf[80];
    tstruct = *localtime(&now);
    strftime(buf, sizeof(buf), "%Y-%m-%d %X", &tstruct);
    return buf;
}

enum { DATETIME, NICK, SEPARATOR, MESG, WINP, };

bool user_flag = true;

std::string ltrim(std::string s) {
   s.erase(s.begin(), std::find_if(s.begin(), s.end(),
            std::not1(std::ptr_fun<int, int>(std::isspace))));
   return s;
}

std::string rtrim(std::string s) {
   s.erase(std::find_if(s.rbegin(), s.rend(),
            std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
   return s;
}

std::string trim(std::string s) {
   return ltrim(rtrim(s));
}

struct Box {
    int height;
    int width;
    int starty;
    int startx;
    Box() : height(0), width(0), starty(0), startx(0) {}
    Box(int height, int width, int starty, int startx) :
        height(height), width(width), starty(starty), startx(startx) {} 
};

class Resolution {
    public:
    Box box;
    Resolution(WINDOW *pwin = stdscr) :
        box{} {
	    getbegyx(pwin, box.starty, box.startx);
	    getmaxyx(pwin, box.height, box.width);
    }
};

WINDOW* createWindow(const Box &box, bool scroll, bool hline = false) {
    WINDOW *pWnd = newwin(box.height, box.width, box.starty, box.startx);
    scrollok(pWnd, scroll);
    ::box(pWnd, 0, 0);
    refresh();
    return pWnd;
}

void destroyWindow(WINDOW *pWnd) {
    if (pWnd) {
        delwin(pWnd);
        pWnd = NULL;
    }
}

PANEL * createPanel(WINDOW *pWnd) {
    PANEL *pPanel = NULL;
    if (pWnd)
        pPanel = new_panel(pWnd);
    return pPanel;
}

void destroyPanel(PANEL *pPanel) {
    if (pPanel) {
        destroyWindow(pPanel->win);
        del_panel(pPanel);
        pPanel = NULL;
    }
}

void colorPairs() {
    if (has_colors() == TRUE) {
        start_color();
        init_pair(DATETIME,  COLOR_CYAN,  COLOR_BLACK);
        init_pair(NICK,      COLOR_GREEN, COLOR_BLACK);
        init_pair(SEPARATOR, COLOR_CYAN,  COLOR_BLACK);
        init_pair(MESG,      COLOR_WHITE, COLOR_BLACK);
        init_pair(WINP,      COLOR_GREEN, COLOR_BLACK);
    }
}

void initNcurses(bool kpadOn = false, bool echoOn = false) {
	initscr();
	colorPairs();
	cbreak();
	if (echoOn) noecho();
	keypad(stdscr, kpadOn);
}

void destroyNcurses(vector<PANEL*> &layout) {
    for_each(layout.begin(), layout.end(), [](PANEL *p){
        if (p && p->win) {
            delwin(p->win);
            del_panel(p);
        }
    });
    endwin();
}

vector<PANEL*> createLayout() {
    vector<PANEL*> layout;
    Resolution res;
    Box chatBox{res.box.height - 3, res.box.width, res.box.starty, res.box.startx};
    layout.push_back(createPanel(createWindow(chatBox, true, true)));
    Box cmdBox{3, res.box.width, res.box.height - 3, res.box.startx};
    layout.push_back(createPanel(createWindow(cmdBox, false, false)));
    Box errBox{res.box.height/2, res.box.width/2, res.box.height/4, res.box.width/4};
    layout.push_back(createPanel(createWindow(errBox, false, false)));
    top_panel(layout[0]);
    update_panels();
    doupdate();
    return layout;
}


int main()
{	
    initNcurses();

    _layout_ = createLayout();

   char command[128];


	while(true) {	
        memset(command, 0, 128);
        mvwaddstr(_layout_[1]->win, 1, 1, "$> ");
        wrefresh(_layout_[1]->win);
        int ch = wgetstr(_layout_[1]->win, command);
        wrefresh(stdscr);
        memset(command, ' ', strlen(command));
        mvwaddstr(_layout_[1]->win, 1, 4, command);
        wrefresh(_layout_[1]->win);
        refresh();
		update_panels();
		doupdate();
	}

    destroyNcurses(_layout_);

	return 0;
}
