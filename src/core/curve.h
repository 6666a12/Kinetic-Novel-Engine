/*
    curve.h

    Animation curve function header.
*/
#pragma once
#include <cmath>

// @param b TRUE for Fadein FALSE for Fadeout
namespace curvefunc
{
    inline float EaseSin(float t, bool b){
        return !b ? 1.0f - std::cos(t * float(M_PI) / 2.0f) : std::sin(t * float(M_PI) / 2.0f);
    }

    /*
        pow = 
            2 for Quad
            3 for Cubic
            4 for Quart
            5 for Quint
            and Customized val
    */
    inline float EasePow(float t, float pow, bool b){
        return !b ? std::pow(t, pow) : 1.0f - std::pow(1.0f - t, pow);
    }

    inline float EaseCir(float t, bool b){
        return !b ? 1.0f - std::sqrt(1.0f - std::pow(t, 2)) : std::sqrt(1.0f - std::pow(1.0f - t, 2));
    }

    inline float EaseExp(float t, bool b){
        return !b ? std::pow(2.0f, 10 * (t - 1)) : 1.0f - std::pow(2.0f, -10 * t);
    }

    inline float Easeback(float t, bool b, const double& c1 = 1.70158f){
        double c3 = c1 + 1.0f;
        return !b ? c3 * std::pow(t, 3) - c1 * std::pow(t, 2) : 1.0f + c3 * std::pow(t - 1.0f, 3) + c1 * std::pow(t - 1.0f, 2);
    }

    // Back Curve InOut
    inline float EaseInOutback(float t, const float& c1 = 1.70158f) {
        float c2 = c1 * 1.525f;
        if (t < 0.5f) {
            return (std::pow(2.0f * t, 2) * ((c2 + 1.0f) * 2.0f * t - c2)) / 2.0f;
        } else {
            return (std::pow(2.0f * t - 2.0f, 2) * ((c2 + 1.0f) * (t * 2.0f - 2.0f) + c2) + 2.0f) / 2.0f;
        }
    }

};