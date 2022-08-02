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

// list of currently active users
static kuna::chat::SeqEntity<string> users(_users_);
// list of currently active rooms
static kuna::chat::SeqEntity<string> rooms(_rooms_);


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

static void printline(const char *date, const char *time, const char *nick, const char *mesg) {
    //wprintw(pWnd, "\n");
    int y, x;
    WINDOW* pWnd = _layout_[0]->win;
    getyx(pWnd, y, x);
    wmove(pWnd, y + 1, 1);
    if (has_colors() == TRUE) wattron(pWnd, COLOR_PAIR(SEPARATOR));
    //wprintw(pWnd, "%s %s ", date, time);
    wprintw(pWnd, "%s ", time);
    if (has_colors() == TRUE) wattroff(pWnd, COLOR_PAIR(SEPARATOR));

    if (has_colors() == TRUE) wattron(pWnd, COLOR_PAIR(NICK));
    wprintw(pWnd, "%*.*s ", 10, 10, nick);
    if (has_colors() == TRUE) wattroff(pWnd, COLOR_PAIR(NICK));

    if (has_colors() == TRUE) wattron(pWnd, COLOR_PAIR(SEPARATOR));
    wprintw(pWnd, "|");
    if (has_colors() == TRUE) wattroff(pWnd, COLOR_PAIR(SEPARATOR));

    if (has_colors() == TRUE) wattron(pWnd, COLOR_PAIR(MESG));
    wprintw(pWnd, "%s", mesg);
    if (has_colors() == TRUE) wattroff(pWnd, COLOR_PAIR(MESG));
    wrefresh(stdscr);
    wrefresh(pWnd);
    wrefresh(_layout_[1]->win);
    top_panel(_layout_[0]);
    top_panel(_layout_[1]);
    update_panels();
    doupdate();
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

void addWinTitle(WINDOW *pWnd, const char *title) {  
    wattron(pWnd, A_REVERSE);
	mvwprintw(pWnd, 0, 2, title);
	wattroff(pWnd, A_REVERSE);
    wmove(pWnd, 1, 1);
    wrefresh(pWnd);
}

void sendMsg(string send_to, string message) {
    char *date, *time, *nick, *mesg;
    size_t len = 0;

    auto timestamp = currentDateTime();
    char msg[128];

    sprintf(msg, "%s %s %s %s", timestamp.c_str(), username.c_str(), send_to.c_str(), message.c_str());
    // publish message to the current channel
    if (current_room.compare(noroom))
        kuna::chat::Connector::instance()->redis().publish(current_room, msg);
}

void sendFile(string send_to, string message) {
    char *date, *time, *nick, *mesg;
    size_t len = 0;

    auto timestamp = currentDateTime();
    char msg[128];
    sprintf(msg, "%s %s %s SHARED:%s", timestamp.c_str(), username.c_str(), send_to.c_str(), message.c_str());
    if (current_room.compare(noroom))
        kuna::chat::Connector::instance()->redis().publish(current_room, msg);
}

void playFile(string send_to, string message) {
    char *date, *time, *nick, *mesg;
    size_t len = 0;

    auto timestamp = currentDateTime();
    char msg[256];
    sprintf(msg, "%s %s %s AUTOPLAY:%s", timestamp.c_str(), username.c_str(), send_to.c_str(), message.c_str());
    if (current_room.compare(noroom))
        kuna::chat::Connector::instance()->redis().publish(current_room, msg);
}

void doListRooms() {
    wclear(_layout_[2]->win);
    Resolution res;
    Box errBox{res.box.height/2, res.box.width/2, res.box.height/4, res.box.width/4};
    box(_layout_[2]->win, 0, 0);
    top_panel(_layout_[2]);
    addWinTitle(_layout_[2]->win, "List of Chat Rooms");
    // list rooms
    int y = 2, x = 2;
    int idx = 1;
    x = getmaxx(_layout_[2]->win);

    auto list_of_rooms = rooms.getValues();

    for_each(list_of_rooms.begin(), list_of_rooms.end(), [&](const string &str){
        wmove(_layout_[2]->win, y++, x/2 - 5);
        wprintw(_layout_[2]->win, "%d - %s", idx++, str.c_str());
    });
    update_panels();
    doupdate();
    int ch;
    do  {
        ch = wgetch(_layout_[2]->win);
        getyx(_layout_[2]->win, y, x);
        mvwprintw(_layout_[2]->win, y, x - 2, "  ");
        wrefresh(_layout_[2]->win);
    } while (ch != 27);
    top_panel(_layout_[0]);
    update_panels();
    doupdate();
}

void sound_thread(string path) {
    stringstream cmd_buf;
    cmd_buf << "mpg123" << " -q " << path << " 2>&1 > /dev/null";
    system(cmd_buf.str().c_str());
}

std::thread tx;
std::atomic_bool sub_run{false};
const string wake_up_sub_thread = "WAKE_UP_SUB_THREAD";

string pubsubMsg(string x1, string x2, string x3, string x4) {
    ostringstream msgbuf;
    msgbuf << currentDateTime() << " " << x1 << " " << x2 << " " << string(x3.length()?x3:string("")) << x4;
    return msgbuf.str();
}

void sub_thread(const string &room) {
    auto sub = kuna::chat::Connector::instance()->redis().subscriber();
    auto channel1 = room;
    sub.on_message([&](std::string channel, std::string msg) {
        if (msg.size() > 0 && msg.compare(wake_up_sub_thread) == 0) {
            //just let sub.consume return so that we can terminate this thread
            return;
        };
        istringstream itmsg(msg);
        vector<string> results(istream_iterator<string>{itmsg},
                                 istream_iterator<string>());
        string date = results[0];
        string time = results[1];
        string nick_from = trim(results[2]);
        string nick_to = trim(results[3]);
        if (
            nick_from.compare(username) == 0 || 
            nick_to.compare(username) == 0 ||
            nick_to.compare("all") == 0
        ) {
            regex rx(R"((AUTOPLAY:)(.*\.mp3))");
            smatch m;
            if (regex_search(msg, m, rx)) {
                if (username.compare(nick_to) == 0) {
                    if (m.size() >= 2) {
                        string fpath = trim(m[2]);
                        if (fpath.ends_with(".mp3")) {
                            ifstream f(fpath);
                            if (f.good()) {
                                thread stx(sound_thread, fpath);
                                stx.detach();
                            }
                        }
                    }
                }
            }
            string mesg = "";
            auto it = results.begin();
            advance(it, 4);
            mesg.clear();
            for_each(it, results.end(),[&](string &txt){
                mesg += txt + " ";
            });
            string nick = nick_from + "->" + nick_to;
            printline(date.c_str(), time.c_str(), nick.c_str(), mesg.c_str());
        }
    });

    sub.on_meta([](Subscriber::MsgType type, OptionalString channel, long long num) {
        // Process message of META type.
        auto v = channel.has_value() ? channel.value().c_str() : " ";
        std::cout << "META:";
        string META;
        switch(type) {
            case Subscriber::MsgType::SUBSCRIBE :
                META = "Subscriber::MsgType::SUBSCRIBE";
                if (channel.has_value()) {
                    string submsg = pubsubMsg(username, "all", username, " joined " + channel.value());
                    kuna::chat::Connector::instance()->redis().publish(channel.value(), submsg);
                }
                break;    
            case Subscriber::MsgType::MESSAGE :
                META = "Subscriber::MsgType::MESSAGE";
                break;    
            case Subscriber::MsgType::PMESSAGE :
                META = "Subscriber::MsgType::PMESSAGE";
                break;    
            case Subscriber::MsgType::PSUBSCRIBE :
                META = "Subscriber::MsgType::PSUBSCRIBE";
                break;    
            case Subscriber::MsgType::PUNSUBSCRIBE :
                META = "Subscriber::MsgType::PUNSUBSCRIBE";
                break;    
            case Subscriber::MsgType::UNSUBSCRIBE :
                META = "Subscriber::MsgType::UNSUBSCRIBE";
                if (channel.has_value()) {
                    string submsg = pubsubMsg(username, "all", username, " left " + channel.value());
                    kuna::chat::Connector::instance()->redis().publish(channel.value(), submsg);
                }
                break;    
        }
    });
    sub.subscribe(channel1);

    // Consume the SUBSCRIBE message.
    sub.consume();
    while (sub_run) {
        try {
            sub.consume();
            this_thread::yield();
        } catch (const Error &err) {
            cerr << err.what() << endl;
        }
    }
    sub.unsubscribe(channel1);
    sub.consume();
}

void doJoinRoom(string room) {
    // check if the room name exists, if not add
    if (!rooms.isValue(room)) {
        rooms.addValue(room);
    }
    if (tx.joinable()) {
        sub_run.store(false);
        if (current_room.compare(noroom))
            kuna::chat::Connector::instance()->redis().publish(current_room, wake_up_sub_thread);
        std::this_thread::sleep_for(500ms);
        if (tx.joinable())
            tx.join();
    }
    current_room = room;
    addWinTitle(_layout_[0]->win, current_room.c_str());
    sub_run.store(true);
    tx = std::thread(sub_thread, current_room);
}

vector<pair<string, string>> list_of_commands {
    {"USER <name>", "[U] Use (create) user <name>"},
    {"LIST", "[L] List existing rooms"},
    {"JOIN", "[J] Join an existing chat or create a new one"},
    {"DELETE", "[D] Delete room"},
    {"QUIT", "[Q]uit"},
    {"M send_to > message", "send to recepient a message"},
    {"F send_to > [file_send|file_play]:<path>", "send to or play on a remoate system the file at <path>"}
};

atomic_bool mon_run{true};

// Run this thread every minute and check if there any rooms that have not been used 
// for more than a minute and delete them
void monitoring_thread() {
    int count = 0;
    while(mon_run) {
        string resp;
        try {
            this_thread::sleep_for(100ms);
            count += 100;
            if ((count %= 60000) == 0) {
                auto r = kuna::chat::Connector::instance()->redis().command("pubsub", "channels");
                auto result = reply::parse<vector<OptionalString>>(*r);
                auto old_rooms = rooms.getValues();
                for_each(old_rooms.begin(), old_rooms.end(), [&result](const string &v){
                    auto it = find(result.begin(), result.end(), v);
                    if (it >= result.end()) {
                        // room not found in the list of active rooms - delete it
                        rooms.delValue(v);
                    }
                });
            }
    } catch (Error &r) {
            string err = r.what();
            bool z = false;
        }
    }
}

void doListCommands(string args) {
    top_panel(_layout_[2]);

    // list rooms
    int y = 2, x = 2;
    int idx = 1;
    x = getmaxx(_layout_[2]->win);
    if (args.length() == 0) {
        addWinTitle(_layout_[2]->win, "List of Commands");
        for_each(list_of_commands.begin(), list_of_commands.end(), [&](const pair<string,string> &p){
            wmove(_layout_[2]->win, y++, 1);
            wprintw(_layout_[2]->win, "%d - %s", idx++, p.first.c_str());
        });
    }
    else {
        auto vit = find_if(
            list_of_commands.begin(), 
            list_of_commands.end(), 
            [&](const pair<string, string> &p){
                return p.first.compare(args) == 0 || p.first.starts_with(args);
            });
        if (vit != end(list_of_commands)) {
            addWinTitle(_layout_[2]->win, (*vit).first.c_str());
            wprintw(_layout_[2]->win, (*vit).second.c_str());
        }
    }
    update_panels();
    doupdate();
    int ch;
    do  {
        ch = wgetch(_layout_[2]->win);
        getyx(_layout_[2]->win, y, x);
        mvwprintw(_layout_[2]->win, y, x - 2, "  ");
        wrefresh(_layout_[2]->win);
    } while (ch != 27);
    top_panel(_layout_[0]);
    update_panels();
    doupdate();   
}

void doAddUser(string nick) {
    if (username.compare(unknown) == 0) {
        if (!users.isValue(nick)) {
            users.addValue(nick);
        }
        username = nick;
    }
    char buf[16];
    memset(buf, ' ', 15);
    buf[15] = '\0';
    addWinTitle(_layout_[1]->win, buf);
    wrefresh(_layout_[1]->win);
    addWinTitle(_layout_[1]->win, username.c_str());
    wrefresh(_layout_[1]->win);
}

void doDelRoom(string args) {
    istringstream itmsg(args);
    vector<string> results(istream_iterator<string>{itmsg}, istream_iterator<string>());
    for_each(begin(results), end(results), [](const string &room){
        rooms.delValue(room);
    });
}

bool processCommand(string oper, string args) {
    bool flag = false;
    if (oper.starts_with("Q")) {
        if (tx.joinable()) {
            sub_run.store(false);
            if (current_room.compare(noroom))
                kuna::chat::Connector::instance()->redis().publish(current_room, wake_up_sub_thread);
            this_thread::sleep_for(500ms);
            tx.join();
        }
        return true;
    }

    if (oper.starts_with("L")) {
        doListRooms();
    } else if (oper.starts_with("J")) {
        doJoinRoom(args);
    } else if (oper.starts_with("?")) {
        doListCommands(args);
    } else if (oper.starts_with("U")) {
        doAddUser(args);
    } else if (oper.starts_with("D")) {
        doDelRoom(args);
    }
    return flag;
}

bool processInput(string input) {
    bool flag = false;
    string cmd = trim(input);
    auto const re = std::regex{R"((\w+)(\s*>)(.*))"};
    smatch m;
    if (regex_search(cmd, m, re)) {
        string send_to = trim(m[1]);
        string sep = m[2];
        sep = trim(sep);
        if (sep.compare(">") == 0) {
            string msg = trim(m[3]);
            if (size_t pos = msg.find("file_send:") != msg.npos) {
                sendFile(send_to, trim(msg.substr(pos+9)));
            } else if (size_t pos = msg.find("file_play:") != msg.npos) {
                playFile(send_to, trim(msg.substr(pos+9)));
            } else {
                sendMsg(send_to, msg);
            }
        }
    } else {
        size_t tok1 = cmd.find_first_of(" \t");
        if (tok1 < cmd.length()) {
            string oper = cmd.substr(0, tok1);
            string rest = cmd.substr(tok1+1);
            processCommand(oper, rest);
        } else {
            flag = processCommand(cmd, "");
        }
    }
    return flag;
}

void redraw_cmd_pannel() {
    char command[128];
    memset(command, ' ', 127);
    command[127] = '\0';
    wclear(_layout_[1]->win);
    Resolution res;
    Box cmdBox{3, res.box.width, res.box.height - 3, res.box.startx};
    box(_layout_[1]->win, 0, 0);
    mvwaddstr(_layout_[1]->win, 1, 1, "$> ");
    mvwaddstr(_layout_[1]->win, 1, 4, command);
    wmove(_layout_[1]->win, 1, 4);
    wrefresh(_layout_[1]->win);
    refresh();
}

int main()
{	
    initNcurses();

    _layout_ = createLayout();

   char command[128];
    addWinTitle(_layout_[0]->win, titleChat);
    addWinTitle(_layout_[1]->win, username.c_str());

    thread mtx(monitoring_thread);

	while(true) {	
        memset(command, 0, 128);
        mvwaddstr(_layout_[1]->win, 1, 1, "$> ");
        wrefresh(_layout_[1]->win);
        int ch = wgetstr(_layout_[1]->win, command);
        if (processInput(command)) break;
        wrefresh(stdscr);
        memset(command, ' ', strlen(command));
        mvwaddstr(_layout_[1]->win, 1, 4, command);
        wrefresh(_layout_[1]->win);
        refresh();
		update_panels();
		doupdate();
	}
    // terminate the monitoring thread
    if (mtx.joinable()) {
        mon_run.store(false);
        mtx.join();
    }

    destroyNcurses(_layout_);

	return 0;
}

void test(std::string arg){
	std::vector<std::string> test{};
}
