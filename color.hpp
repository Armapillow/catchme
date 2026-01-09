#ifndef __COLOR_H_
#define __COLOR_H_

struct Color {
    enum Code : uint32_t {
        Reset      = 0,

        Bold       = 1 << 0,
        Dim        = 1 << 1,
        Underline  = 1 << 2,

        Black      = 1 << 8,
        Red        = 1 << 9,
        Green      = 1 << 10,
        Yellow     = 1 << 11,
        Blue       = 1 << 12,
        Magenta    = 1 << 13,
        Cyan       = 1 << 14,
        White      = 1 << 15,

        BgBlack    = 1 << 16,
        BgRed      = 1 << 17,
        BgGreen    = 1 << 18,
        BgYellow   = 1 << 19,
        BgBlue     = 1 << 20,
        BgMagenta  = 1 << 21,
        BgCyan     = 1 << 22,
        BgWhite    = 1 << 23
    };

    uint32_t mask;
    constexpr Color(uint32_t m)
        : mask(m) {}

    Color() = default;
};

constexpr Color operator| (const Color a, const Color b)
{
    return Color(a.mask | b.mask);
}

std::ostream &operator<< (std::ostream &os, const Color c)
{
    os << "\033[";

    if (c.mask & Color::Reset) {
        os << "0m";
        return os;
    }

    bool first = true;
    auto emit = [&](const int code){
        if (!first) os << ';';
        os << code;
        first = false;
    };

    if (c.mask & Color::Bold)      emit(1);
    if (c.mask & Color::Dim)       emit(2);
    if (c.mask & Color::Underline) emit(4);

    if (c.mask & Color::Black)   emit(30);
    if (c.mask & Color::Red)     emit(31);
    if (c.mask & Color::Green)   emit(32);
    if (c.mask & Color::Yellow)  emit(33);
    if (c.mask & Color::Blue)    emit(34);
    if (c.mask & Color::Magenta) emit(35);
    if (c.mask & Color::Cyan)    emit(36);
    if (c.mask & Color::White)   emit(37);

    if (c.mask & Color::BgBlack)   emit(40);
    if (c.mask & Color::BgRed)     emit(41);
    if (c.mask & Color::BgGreen)   emit(42);
    if (c.mask & Color::BgYellow)  emit(43);
    if (c.mask & Color::BgBlue)    emit(44);
    if (c.mask & Color::BgMagenta) emit(45);
    if (c.mask & Color::BgCyan)    emit(46);
    if (c.mask & Color::BgWhite)   emit(47);


    os << "m";
    return os;
}


#endif // __COLOR_H_
