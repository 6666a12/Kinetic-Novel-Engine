#pragma once
#include <functional>
#include <cmath>
#include <algorithm>

class Fader
{
public:
    using CurveFunc = std::function<double(double)>;

    Fader() = default;
    explicit Fader(double init) : current(init) {}

    void fadeTo(double tar, double durationSec, CurveFunc curv = nullptr)
    {
        from     = current;
        to       = tar;
        duration = durationSec;
        elapsed  = 0;
        active   = true;
        curve    = curv ? curv : EaseSin;
    }

    void fadeIn(double durationSec, CurveFunc curv = nullptr) { fadeTo(1.0, durationSec, curv); }
    void fadeOut(double durationSec, CurveFunc curv = nullptr){ fadeTo(0.0, durationSec, curv); }

    void Skip(){ current = to; active = false; }
    void Pause(){ active = false; }

    float Update(double dt)
    {
        if(!active || duration <= 0.0f)
        {
            if(duration <= 0) current = to;
            active = false;
            return static_cast<float>(current);
        }

        elapsed += dt;
        double t = std::clamp(elapsed / duration, 0.0, 1.0);
        current = from + (to - from) * curve(t);

        if(t >= 1.0f) active = false;
        return static_cast<float>(current);
    }

    double Value()  const { return current; }
    double Target() const { return to; }
    bool isActive() const { return active; }
    double Prog()   const { return duration <= 0 ? 1.0f : std::clamp(elapsed / duration, 0.0, 1.0); }

protected:
    double from = 0.0f;
    double to = 1.0f;
    double current = 0;
    double duration = 0;
    double elapsed = 0;
    bool active = false;
    CurveFunc curve;

    static double EaseSin(double t){ return 1.0 - std::cos(t * M_PI / 2.0); }
};
