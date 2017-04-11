#include "ansi_escape.h"
#include "model/log.h"

#include <iostream>
#include <sstream>
#include <fstream>
#include <thread>
using namespace std;

#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <signal.h>

#include "ansi.h"
#include "apis/unicode.h"

tuple<int, int> current_resolution()
{
    int cols = 80;
    int lines = 24;

#ifdef TIOCGSIZE
    struct ttysize ts;
    ioctl(STDOUT_FILENO, TIOCGSIZE, &ts);
    cols = ts.ts_cols;
    lines = ts.ts_lines;
#elif defined(TIOCGWINSZ)
    struct winsize ts;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &ts);
    cols = ts.ws_col;
    lines = ts.ws_row;
#endif /* TIOCGSIZE */

    return std::make_tuple(cols, lines);
}

static volatile bool g_stop_winching = false;
static thread* winch_thread = nullptr;
static bool _winched = false;
static sigset_t g_sigset;

static void winch_proc()
{
    while (!g_stop_winching)
    {
        int sig;
        int result = sigwait(&g_sigset, &sig);
        if (result == 0 && sig == SIGWINCH) // winched
        {
            _winched = true;
        }
    }

    // remove mask
    pthread_sigmask(SIG_UNBLOCK, &g_sigset, nullptr);
}

static void register_winch(bool doit)
{
    if (winch_thread)
    {
        if (doit)
            return;

        g_stop_winching = true;
        raise(SIGUSR1); // so the thread will release
        winch_thread = nullptr;
    }
    else if (doit)
    {
        // block winch signals
        sigemptyset(&g_sigset);
        sigaddset(&g_sigset, SIGWINCH);
        sigaddset(&g_sigset, SIGUSR1);
        if (pthread_sigmask(SIG_BLOCK, &g_sigset, nullptr))
        {
            lerr << "failed to mask threads for winch - resize will be ignored\n";
            return;
        }

        g_stop_winching = false;
        winch_thread = new thread(&winch_proc);
    }
}

AnsiEscapeTerm::AnsiEscapeTerm()
{
    register_winch(true);
}

AnsiEscapeTerm::~AnsiEscapeTerm()
{
    register_winch(false);
    close();
}

bool AnsiEscapeTerm::update()
{
    if (_winched)
    {
        cout << Ansi::clear_scroll << flush;
        _winched = false;
        auto rez = current_resolution();
        int width = std::get<0>(rez);
        int height = std::get<1>(rez);
        for (auto pair : _states)
        {
            Frame* pFrame = pair.first;
            pFrame->winched(width, height);
        }
    }
    cout << flush;

    return true;
}

bool AnsiEscapeTerm::onOpen()
{
    //switch to alternative buffer screen
    // cout << esc << "47h";

    cout << Ansi::cursor_off;
    clear();
    return true;
}

void AnsiEscapeTerm::clear()
{
    cout << Ansi::clear_term << Ansi::set_pos(1, 1) << flush;
}

void AnsiEscapeTerm::onClose()
{
    cout << Ansi::cursor_on;
    _states.clear();
    cout << Ansi::set_pos(1, 1);
    cout << flush;
}

bool AnsiEscapeTerm::onAdd(Frame* pf)
{
    _states[pf] = {};
    return true;
}

static string replace_all(const string& src, const string& match, const string& replacement)
{
    size_t index = 0;
    string result = "";
    while (index < src.size())
    {
        size_t next = src.find(match, index);
        result += src.substr(index, next - index);
        index = next;
        if (next != string::npos)
        {
            result += replacement;
            index += match.size() + 1;
        }
    }
    return result;
}

string AnsiEscapeTerm::scrub(const string& value) const
{
    // replace tabs with (U+2409 for HT symbol)
    // I could use the ht unicode symbol in the source file
    // but i prefer to keep the source files in ascii
    return replace_all(value, "\t", string{(char)226, (char)144, (char)137, ' '});
}

void AnsiEscapeTerm::write(Frame* pf, int x, int y, const Cell& cell)
{
    if (pf->scrolling())
    {
        ofstream flog("log", fstream::app);
        flog << cell.value;
        flog.close();
    }
    else if (!cell.locked)
    {
        string cmd = "";
        if (x != _x || y != _y)
        {
            if (x == 1 && y == _y + 1) // new line
                cmd += "\r\n";
            else
                cmd += Ansi::set_pos(x, y);
        }
        if (cell.fg.rgb != _fg_rgb || cell.bg.rgb != _bg_rgb)
            cmd += Ansi::set_color(cell.fg, cell.bg);

        string text = scrub(cell.value);
        cout << cmd << text;
        _x = x + cell.width;
        _y = y;
        _fg_rgb = cell.fg.rgb;
        _bg_rgb = cell.bg.rgb;
    }
}

tuple<int, int> AnsiEscapeTerm::maxResolution() const
{
    return current_resolution();
}

