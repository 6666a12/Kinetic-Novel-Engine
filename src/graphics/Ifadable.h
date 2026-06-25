/* DEPRECATED: Use Fader from fader.h instead. This file kept for reference only. */

/*
    Ifadable.h

    Interface of Fadeout and Fadein
    DEPRECATED!
*/
#pragma once
#include <cmath>

class Ifadable
{
public:
    virtual ~Ifadable() = default;

    virtual void StartFadeOut(bool in2out, double duration, double* curve(double)) = 0;
    virtual void CancelFadeOut() = 0;
    virtual bool IsFadingOut() const = 0;
    virtual float GetOp() const = 0;
};
