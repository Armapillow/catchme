#include <iostream>
#include <vector>
#include <fstream>
#include <chrono>
#include <random>
#include <algorithm> 

#include <unistd.h>
#include <termios.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>
#include <assert.h>

#include "color.hpp"

#define H   26
#define W   80
#define MAXSTARS    100

const char* LOGO[] = {
    R"(__   ____    _  _____ ____ _   _ __  __ _____ __)",
    R"(\ \ / ___|  / \|_   _/ ___| | | |  \/  | ____/ /)",
    R"( \ \ |     / _ \ | || |   | |_| | |\/| |  _|/ /)",
    R"( / / |___ / ___ \| || |___|  _  | |  | | |__\ \)",
    R"(/_/ \____/_/   \_\_| \____|_| |_|_|  |_|_____\_\)"
};

const int MAX_PER_ROW = 1;  // or 2
std::vector<int> rowCount(H, 0);

struct Star {
    int x, y;
    int speed;
    int tick;
};

struct Cell {
    char ch;
    Color color;
};

struct Word {
    std::string text;
    int x, y;
    int speed;
    int tick;
    bool active;

    Word() = default;
};

std::ostream &operator<< (std::ostream &os, const Word &w)
{
    os << "w: " << w.text << " (" << w.x << "," << w.y
       << ") " << "speed: " << w.speed << ", " << w.tick
       << w.active;
    return os;
}

struct Term {
    Term(bool raw = true)
    {
        if (raw)
            setRaw();

        // hide cursor, clear the screen
        // [?1049h - switch to alternate screen
        // [?25l   - turn off cursor
        std::cout << "\033[?1049h\033[H\033[?25l";


        ::srand(::time(NULL));

        // init stars
        for (int i = 0; i < MAXSTARS; i++) {
            stars[i].x = rand() % W;
            stars[i].y = (rand() % (H-2)) + 1; // skip top and bottom row
            stars[i].speed = 1+rand()%3;
            stars[i].tick = 0;
        }

        const std::string filePath = "english.txt";
        std::ifstream in(filePath);
        std::string line;

        if (in.good()) {
            while (getline(in, line))
                words.push_back({ std::move(line),
                        /*x=*/0,
                        /*y=*/rand()%(H-2)+1,
                        /*speed=*/10/*+(rand()%8)*/,
                        /*tick=*/0,
                        /*active=*/false/*true*/
                        });
            in.close();
        }

        std::random_device rd;
        std::mt19937 rng(rd());
        std::shuffle(words.begin(), words.end(), rng);

        /*
        // TODO: this case
        // aircraft -> 8
        // 01234567
        //    air   -> 3
        const int gap = 10;
        int wordsSize = words.size();
        for (int i = 0; i < wordsSize; i++) {
            int nextX = -static_cast<int>(words[i].text.length()) - gap;
            for (int j = 0; j < wordsSize; j++) {
                if (i == j) continue;
                if (words[i].y == words[j].y) {
                    words[j].x = nextX;
                    nextX -= words[j].text.length() + gap;
                }
            }
        }*/

    }

    ~Term()
    {
        // restore the state
        // ?1049l - return to normal screen
        // ?25h   - show cursor
        // 0m     - reset attributes
        std::cout << "\033[?1049l\033[0m\033[?25h";

        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_term);
    }

    void clearBuffer(void)
    {
        for (int y = 0; y < H; y++) {
            for (int x = 0; x < W; x++) {
                buffer[y][x] = {' ', Color::Reset };
            }
        }
    }

    void drawText(int row, int col, const char *text, Color color)
    {
        assert(col < W && "Numbers of columns are exceeded");
        while (*text && col < W)
            buffer[row][col++] = {*text++, color};
    }

    inline
    int remainingSeconds() const
    {
        auto now = clock::now();
        int elapsed = static_cast<int>(
                std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count()
                );

        int remaining = TOTAL_TIME - elapsed;
        return remaining > 0 ? remaining : 0;
    }

    void update(void)
    {
        // update stars
        for (int i=0; i < MAXSTARS; i++) {
            if (++stars[i].tick >= stars[i].speed) {
                stars[i].tick = 0;
                stars[i].x++;

                if (stars[i].x >= W) {
                    stars[i].x = 0;
                    // respawn on the new row
                    stars[i].y = (rand() % (H-2)) + 1;
                }
            }
        }

        if (!welcomeScreen) {
            auto now = clock::now();
            float waveElapsed = std::chrono::duration<float>(now - lastWaveTime).count();

            if (waveElapsed >= waveIntervals) {
                lastWaveTime = now;
                allowedCount = std::min(
                        allowedCount + waveIncrement,
                        (int)words.size());

                // optional difficulty ramp
                spawnInterval = std::max(0.1f, spawnInterval * 0.95f);
                waveIntervals  = std::max(3.0f, waveIntervals  * 0.98f);
            }
            float spawnElapsed =
                std::chrono::duration<float>(now - lastSpawnTime).count();

            // steady spawn logic
            if (spawnElapsed >= spawnInterval &&
                    spawnedCount < allowedCount &&
                    spawnedCount < words.size())
            {
                lastSpawnTime = now;

                int row = rand() % (H-2) + 1;
                if (rowCount[row] >= MAX_PER_ROW)
                    return; // skip this spawn

                Word& w = words[spawnedCount];
                //w.x = -static_cast<int>(w.text.length()) - 5;
                w.x = rand() % 2 ? -1 : -static_cast<int>(w.text.length())/2;
                w.y = row;
                w.active = true;
                rowCount[row]++;

                spawnedCount++;
            }

            // update words
            for (auto &w : words) {
                if (!w.active) continue;
                if (++w.tick >= w.speed) {
                    w.tick = 0;
                    w.x++;
                    if (w.x >= W) {
                        //w.x = -static_cast<int>(w.text.size());
                        w.active = false;
                        rowCount[w.y]--;
                    }
                }
            }

        }

    }

    void drawBuffer(void)
    {
        // draw bars
        for (int x = 0; x < W; x++) {
            buffer[0][x] = {' ', Color::BgBlue };
            buffer[H-1][x] = {' ', Color::BgBlue };
        }

        // draw stars
        for (int i= 0; i < MAXSTARS; i++) {
            Star s = stars[i];
            int x = s.x;
            int y = s.y;
            buffer[y][x].ch = '.';
            if (s.speed == 1)
                buffer[y][x].color = Color::White; /* | Color::Bold; */
            else if (s.speed == 2)
                buffer[y][x].color = Color::White;
            else if (s.speed == 3)
                buffer[y][x].color = Color::White | Color::Dim;
            else
                assert(false && "UNREACHABLE");
        }


        // Welcome screen
        if (welcomeScreen) {
            int pos = 5,
                n   = sizeof(LOGO)/sizeof(LOGO[0]);
            for (int i = 0; i < n; i++)
                drawText(pos+i, 16, LOGO[i], Color::White | Color::Bold);
            drawText(pos+n, 29, "<Press SPACE to start>", Color::Bold/*Color::Underline*/);
            drawText(pos+n+1, 36, "Option 1", Color::White);
            drawText(pos+n+2, 36, "Option 2", Color::White);
            drawText(pos+n+3, 36, "Option 3", Color::White);
        } else if (finalScreen) {
            std::string resultText = "Your result is " + std::to_string(hitWords) + "w/m";
            drawText(H/2-2, W/2 - resultText.size()/2, resultText.c_str(), Color::White | Color::Bold);
        } else  { // draw words
            for (auto &w : words) {
                if (w.active) {
                    int col = w.x;
                    int row = w.y;
                    int n = w.text.size();
                    for (size_t i = 0; i < n; i++) {
                        double pos = static_cast<double>(col+i)/W;
                        if (col+i >= 0 && col+i < W && row >= 0 && row < H) {
                            buffer[row][col+i].ch = w.text[i];
                            if (pos >= 0.8) 
                                buffer[row][col+i].color = Color::Red | Color::Bold;
                            else if (pos >= 0.6)
                                buffer[row][col+i].color = Color::Yellow | Color::Bold;
                            else {
                                buffer[row][col+i].color = Color::Green | Color::Bold;
                            }
                        }
                    }
                }
            }

            // Draw typing field and input
            std::string typeBox = "[Type: " + inputWord;
            int pos = 0;
            for (int i = 0; i < typeBox.size(); i++) {
                if (pos > W)
                    break;
                buffer[H-1][pos++] = { typeBox[i], Color::White | Color::BgBlue };
            }
            buffer[H-1][25] = { ']', Color::White | Color::BgBlue };

            // Draw timer
            int sec = remainingSeconds();
            if (sec == 0) {
                finalScreen = true;
                return;
            }
            int minutes = sec/60;
            int seconds = sec%60;

            std::string timerText = std::to_string(minutes) + ":" + (seconds < 10 ? "0" : "") + std::to_string(seconds);
            int x = W - 8; // adjust for MM:SS width
            int y = H - 1; // bottom row

            Color timerColor = Color::White | Color::BgBlue;
            if (sec <= 10) {
                // blink effect
                if ((sec * 2) % 2 == 0) // toggle every 0.5s
                    timerColor = Color::Red | Color::BgBlue | Color::Bold; // | Color::Blink;
            }

            for (int i = 0; i < timerText.size(); i++) {
                if (x+i >= 0 && x+i < W)
                    buffer[y][x+i] = { timerText[i], timerColor };
            }
        }



        // Acutal draw
        Color last = Color::Reset;
        std::cout << "\033[H";
        for (int y = 0; y < H; y++) {
            for (int x = 0; x < W; x++) {
                const Color& cur = buffer[y][x].color;
                 if (cur.mask != last.mask) {
                     std::cout << "\033[0m";
                     std::cout << cur;
                     last = cur;
                 }
                 std::cout << buffer[y][x].ch;
            }
            std::cout.write("\n", 1);
        }
        std::cout.flush();
    }


    void setRaw()
    {
        struct termios raw;
        tcgetattr(STDIN_FILENO, &orig_term);

        raw = orig_term;
        raw.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

        fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
    }

    void processInput()
    {
        ch = read();
        if (ch == -1) return;
        if (welcomeScreen) {
            if (ch == 'q')
                shouldClose = true;
            else if (ch == ' ') {
                welcomeScreen = false;
                inputWord.clear();
                startTime = clock::now();
            }
            return;
        }
        if (ch == '\033')
            shouldClose = true;
        else if (ch == 127 && !inputWord.empty())
                inputWord.pop_back();
        else if (ch == ' ')
            checkWords();
        else if (::isprint(ch))
            inputWord += ch;
    }

    int read()
    {
        int n = ::read(STDIN_FILENO, &ch, 1);
        if (n == 1) return ch;
        return -1;
    }

    bool ShouldClose()
    {
        return !shouldClose;
    }

    void checkWords()
    {
        for (Word &w : words) {
            if (w.text == inputWord) {
                w.active = false;
                hitWords++;
                rowCount[w.y]--;
            }
        }
        inputWord.clear();
    }


    // Real elapsed time
    using clock = std::chrono::steady_clock;
    clock::time_point lastSpawnTime = clock::now();
    clock::time_point lastWaveTime  = clock::now();
    int spawnedCount = 0;
    int allowedCount = 10; // size of the first wave
    int waveIncrement = 5;
    float waveIntervals = 7.0f; // second between waves
    float spawnInterval = 0.5f; // second per word (steady rate)


    // Timer
    clock::time_point startTime;
    const int TOTAL_TIME = 60;

    char ch;
    int hitWords{0};
    std::string inputWord{};
    std::vector<Word> words{};
    Star stars[MAXSTARS];
    Cell buffer[H][W];

    bool finalScreen = false;
    bool welcomeScreen = true;
    bool shouldClose = false;

    struct termios orig_term;
};

int main(void)
{
    Term term{};

    while (term.ShouldClose()) {

        term.clearBuffer();
        term.processInput();
        term.update();
        term.drawBuffer();

        ::usleep(30'000);

    }

    return 0;
}
